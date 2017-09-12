/* -*- c-basic-offset:4; indent-tabs-mode:nil -*- vi: set sw=4 et: */
/*
// Copyright (c) 2016, Earl Chew
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the names of the authors of source code nor the names
//       of the contributors to the source code may be used to endorse or
//       promote products derived from this software without specific
//       prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL EARL CHEW BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "ert/error.h"
#include "ert/fd.h"
#include "ert/thread.h"
#include "ert/process.h"
#include "ert/queue.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <execinfo.h>

#include <sys/mman.h>

#include <valgrind/valgrind.h>

/* -------------------------------------------------------------------------- */
static unsigned moduleInit_;

static struct Ert_ErrorUnwindFrame_ __thread errorUnwind_;

typedef TAILQ_HEAD(
   Ert_ErrorFrameChunkList, Ert_ErrorFrameChunk) Ert_ErrorFrameChunkListT;

struct Ert_ErrorFrameChunk
{
    TAILQ_ENTRY(Ert_ErrorFrameChunk) mStackList;

    struct Ert_ErrorFrameChunk *mChunkList;
    size_t                  mChunkSize;

    struct Ert_ErrorFrame *mBegin;
    struct Ert_ErrorFrame *mEnd;

    struct Ert_ErrorFrame  mFrame_[];
};

static struct
{
    /* Carefully crafted so that new threads will see an initialised
     * instance that can be used immediately. In particular, note that
     * threads cannot be created from signal context, so ErrorFrameStackThread
     * must be zero. */

    struct
    {
        struct Ert_ErrorFrameIter mLevel;
        struct Ert_ErrorFrameIter mSequence;

        Ert_ErrorFrameChunkListT mHead;

    } mStack_[Ert_ErrorFrameStackKinds], *mStack;

} __thread errorStack_;

/* -------------------------------------------------------------------------- */
/* Thread specific error frame cleanup
 *
 * Error frames are collected for each thread, and their number is allowed
 * to grow as required. When the thread terminates, the collected frames
 * must be released.
 *
 * To do this, attach a callback to be invoked when the thread is destroyed
 * by using pthread_key_create(). Use pthread_once() and ERT_EARLY_INITIALISER()
 * to ensure that pthread_key_create() is invoked at some time during
 * program initialisation. Most likely there will only be one thread
 * running at this time, but this cannot be guaranteed necessitating
 * the use of pthread_once(). */

struct Ert_ErrorDtor
{
    pthread_key_t  mKey;
    pthread_once_t mOnce;
};

static struct Ert_ErrorDtor errorDtor_ =
{
    .mOnce = PTHREAD_ONCE_INIT,
};

static void
destroyErrorKey_(void *aChunk_)
{
    struct Ert_ErrorFrameChunk *chunk = aChunk_;

    while (chunk)
    {
        struct Ert_ErrorFrameChunk *next = chunk->mChunkList;

        if (munmap(chunk, chunk->mChunkSize))
            abortProcess();

        VALGRIND_FREELIKE_BLOCK(chunk, 0);

        chunk = next;
    }
}

static void
initErrorKey_(void)
{
    if (pthread_once(
            &errorDtor_.mOnce,
            LAMBDA(
                void, (void),
                {
                    if (pthread_key_create(
                            &errorDtor_.mKey, destroyErrorKey_))
                        abortProcess();
                })))
        abortProcess();
}

static void
setErrorKey_(struct Ert_ErrorFrameChunk *aChunk)
{
    initErrorKey_();

    if (pthread_setspecific(errorDtor_.mKey, aChunk))
        abortProcess();
}

ERT_EARLY_INITIALISER(
    error_,
    ({
        initErrorKey_();
    }),
    ({ }));

/* -------------------------------------------------------------------------- */
static struct Ert_ErrorFrameChunk *
createErrorFrameChunk_(struct Ert_ErrorFrameChunk *aParent)
{
    struct Ert_ErrorFrameChunk *self = 0;

    /* Do not use fetchSystemPageSize() because that might cause a recursive
     * reference to createErrorFrameChunk_(). */

    long pageSize = sysconf(_SC_PAGESIZE);

    if (-1 == pageSize || ! pageSize)
        abortProcess();

    /* Do not use malloc() because the the implementation in malloc_.c
     * will cause a recursive reference to createErrorFrameChunk_(). */

    size_t chunkSize =
        ROUNDUP(sizeof(*self) + sizeof(self->mFrame_[0]), pageSize);

    self = mmap(0, chunkSize,
                PROT_READ | PROT_WRITE,
                MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    if (MAP_FAILED == self)
        abortProcess();

    VALGRIND_MALLOCLIKE_BLOCK(self, chunkSize, 0, 0);

    self->mChunkList = 0;
    self->mChunkSize = chunkSize;

    /* Find out the number of frames that will fit in the allocated
     * space, but only use a small number during test in order to
     * exercise the frame allocator. */

    size_t numFrames =
        (chunkSize - sizeof(*self)) / sizeof(self->mFrame_[0]);

    unsigned testFrames = 2;

    if (testMode(TestLevelRace) && numFrames > testFrames)
        numFrames = testFrames;

    self->mBegin = &self->mFrame_[0];
    self->mEnd   = &self->mFrame_[numFrames];

    if (aParent)
    {
        self->mChunkList    = aParent->mChunkList;
        aParent->mChunkList = self;
    }

    return self;
}

static void
initErrorFrame_(void)
{
    ensure(0 == Ert_ErrorFrameStackThread);

    if ( ! errorStack_.mStack)
    {
        struct Ert_ErrorFrameChunk *parentChunk = 0;

        for (unsigned ix = NUMBEROF(errorStack_.mStack_); ix--; )
        {
            errorStack_.mStack = &errorStack_.mStack_[ix];

            TAILQ_INIT(&errorStack_.mStack->mHead);

            struct Ert_ErrorFrameChunk *chunk =
                createErrorFrameChunk_(parentChunk);

            if ( ! parentChunk)
                parentChunk = chunk;

            errorStack_.mStack->mLevel = (struct Ert_ErrorFrameIter)
            {
                .mIndex = 0,
                .mFrame = 0,
                .mChunk = 0,
            };

            struct Ert_ErrorFrameIter *iter = &errorStack_.mStack->mLevel;

            TAILQ_INSERT_TAIL(&errorStack_.mStack->mHead, chunk, mStackList);

            iter->mChunk = chunk;
            iter->mFrame = chunk->mBegin;

            errorStack_.mStack->mSequence = *iter;
        }

        ensure(parentChunk);

        setErrorKey_(parentChunk);
    }
}

/* -------------------------------------------------------------------------- */
void
ert_addErrorFrame(const struct Ert_ErrorFrame *aFrame, int aErrno)
{
    initErrorFrame_();

    struct Ert_ErrorFrameIter *iter = &errorStack_.mStack->mLevel;

    if (iter->mFrame == iter->mChunk->mEnd)
    {
        struct Ert_ErrorFrameChunk *chunk = createErrorFrameChunk_(iter->mChunk);

        TAILQ_INSERT_TAIL(&errorStack_.mStack->mHead, chunk, mStackList);

        iter->mChunk = chunk;
        iter->mFrame = chunk->mBegin;
    }

    iter->mFrame[0]        = *aFrame;
    iter->mFrame[0].mErrno = aErrno;

    ++iter->mIndex;
    ++iter->mFrame;
}

/* -------------------------------------------------------------------------- */
void
ert_restartErrorFrameSequence(void)
{
    initErrorFrame_();

    errorStack_.mStack->mLevel = errorStack_.mStack->mSequence;
}

/* -------------------------------------------------------------------------- */
struct Ert_ErrorFrameSequence
ert_pushErrorFrameSequence(void)
{
    initErrorFrame_();

    struct Ert_ErrorFrameIter iter = errorStack_.mStack->mSequence;

    errorStack_.mStack->mSequence = errorStack_.mStack->mLevel;

    return (struct Ert_ErrorFrameSequence)
    {
        .mIter = iter,
    };
}

/* -------------------------------------------------------------------------- */
void
ert_popErrorFrameSequence(struct Ert_ErrorFrameSequence aSequence)
{
    ert_restartErrorFrameSequence();

    errorStack_.mStack->mSequence = aSequence.mIter;
}

/* -------------------------------------------------------------------------- */
enum Ert_ErrorFrameStackKind
ert_switchErrorFrameStack(enum Ert_ErrorFrameStackKind aStack)
{
    initErrorFrame_();

    enum Ert_ErrorFrameStackKind stackKind =
        errorStack_.mStack - &errorStack_.mStack_[0];

    errorStack_.mStack = &errorStack_.mStack_[aStack];

    return stackKind;
}

/* -------------------------------------------------------------------------- */
unsigned
ert_ownErrorFrameLevel(void)
{
    initErrorFrame_();

    return errorStack_.mStack->mLevel.mIndex;
}

/* -------------------------------------------------------------------------- */
const struct Ert_ErrorFrame *
ert_ownErrorFrame(enum Ert_ErrorFrameStackKind aStack, unsigned aLevel)
{
    initErrorFrame_();

    struct Ert_ErrorFrame *frame = 0;

    if (aLevel < errorStack_.mStack->mLevel.mIndex)
    {
        struct Ert_ErrorFrame *begin =
            errorStack_.mStack->mLevel.mChunk->mBegin;

        struct Ert_ErrorFrame *end =
            errorStack_.mStack->mLevel.mFrame;

        unsigned top = errorStack_.mStack->mLevel.mIndex;

        while (1)
        {
            unsigned size   = end - begin;
            unsigned bottom = top - size;

            if (aLevel >= bottom)
            {
                frame = begin + (aLevel - bottom);
                break;
            }

            struct Ert_ErrorFrameChunk *prevChunk =
                TAILQ_PREV(errorStack_.mStack->mLevel.mChunk,
                           Ert_ErrorFrameChunkList,
                           mStackList);

            ensure(prevChunk);

            begin = prevChunk->mBegin;
            end   = prevChunk->mEnd;
            top   = bottom;
        }
    }

    return frame;
}

/* -------------------------------------------------------------------------- */
void
ert_logErrorFrameSequence(void)
{
    initErrorFrame_();

    unsigned seqLen =
        errorStack_.mStack->mLevel.mIndex -
        errorStack_.mStack->mSequence.mIndex;

    for (unsigned ix = 0; ix < seqLen; ++ix)
    {
        struct Ert_ErrorFrame *frame =
            errorStack_.mStack->mSequence.mFrame + ix;

        ert_errorWarn(
            frame->mErrno,
            frame->mName,
            frame->mFile,
            frame->mLine,
            "Error frame %u - %s",
            seqLen - ix - 1,
            frame->mText);
    }
}

/* -------------------------------------------------------------------------- */
struct Ert_ErrorUnwindFrame_ *
ert_pushErrorUnwindFrame_(void)
{
    struct Ert_ErrorUnwindFrame_ *self = &errorUnwind_;

    ++self->mActive;

    return self;
}

/* -------------------------------------------------------------------------- */
void
ert_popErrorUnwindFrame_(struct Ert_ErrorUnwindFrame_ *self)
{
    ensure(self->mActive);

    --self->mActive;
}

/* -------------------------------------------------------------------------- */
struct Ert_ErrorUnwindFrame_ *
ert_ownErrorUnwindActiveFrame_(void)
{
    struct Ert_ErrorUnwindFrame_ *self = &errorUnwind_;

    return self->mActive ? self : 0;
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
tryErrTextLength_(int aErrCode, size_t *aSize)
{
    int rc = -1;

    char errCodeText[*aSize];

    const char *errText;
    ERROR_IF(
        (errno = 0,
         errText = strerror_r(aErrCode, errCodeText, sizeof(errCodeText)),
         errno));

    *aSize = errCodeText != errText ? 1 : strlen(errCodeText);

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

static ERT_CHECKED size_t
findErrTextLength_(int aErrCode)
{
    size_t rc = 0;

    size_t textCapacity = testMode(TestLevelRace) ? 2 : 128;

    while (1)
    {
        size_t textSize = textCapacity;

        ERROR_IF(
            tryErrTextLength_(aErrCode, &textSize));

        if (textCapacity > textSize)
        {
            rc = textSize;
            break;
        }

        textSize = 2 * textCapacity;
        ensure(textCapacity < textSize);

        textCapacity = textSize;
    }

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
static void
dprint_(
    int                    aLockErr,
    int                    aErrCode,
    const char            *aErrText,
    struct Pid             aPid,
    struct Tid             aTid,
    const struct Duration *aElapsed,
    uint64_t               aElapsed_h,
    uint64_t               aElapsed_m,
    uint64_t               aElapsed_s,
    uint64_t               aElapsed_ms,
    const char            *aFunction,
    const char            *aFile,
    unsigned               aLine,
    const char            *aFmt, va_list aArgs)
{

    if ( ! aFile)
        dprintf(STDERR_FILENO, "%s: ", ownProcessName());
    else
    {
        if (aPid.mPid == aTid.mTid)
        {
            if (aElapsed->duration.ns)
                dprintf(
                    STDERR_FILENO,
                    "%s: [%04" PRIu64 ":%02" PRIu64
                    ":%02" PRIu64
                    ".%03" PRIu64 " %" PRId_Pid " %s %s:%u] ",
                    ownProcessName(),
                    aElapsed_h, aElapsed_m, aElapsed_s, aElapsed_ms,
                    FMTd_Pid(aPid), aFunction, aFile, aLine);
            else
                dprintf(
                    STDERR_FILENO,
                    "%s: [%" PRId_Pid " %s %s:%u] ",
                    ownProcessName(),
                    FMTd_Pid(aPid), aFunction, aFile, aLine);
        }
        else
        {
            if (aElapsed->duration.ns)
                dprintf(
                    STDERR_FILENO,
                    "%s: [%04" PRIu64 ":%02" PRIu64
                    ":%02" PRIu64
                    ".%03" PRIu64 " %" PRId_Pid ":%" PRId_Tid " %s %s:%u] ",
                    ownProcessName(),
                    aElapsed_h, aElapsed_m, aElapsed_s, aElapsed_ms,
                    FMTd_Pid(aPid), FMTd_Tid(aTid), aFunction, aFile, aLine);
            else
                dprintf(
                    STDERR_FILENO,
                    "%s: [%" PRId_Pid ":%" PRId_Tid " %s %s:%u] ",
                    ownProcessName(),
                    FMTd_Pid(aPid), FMTd_Tid(aTid), aFunction, aFile, aLine);
        }

        if (EWOULDBLOCK != aLockErr)
            dprintf(STDERR_FILENO, "- lock error %d - ", aLockErr);
    }

    xvdprintf(STDERR_FILENO, aFmt, aArgs);
    if ( ! aErrCode)
        dprintf(STDERR_FILENO, "\n");
    else if (aErrText)
        dprintf(STDERR_FILENO, " - errno %d [%s]\n", aErrCode, aErrText);
    else
        dprintf(STDERR_FILENO, " - errno %d\n", aErrCode);
}

static void
dprintf_(
    int                    aErrCode,
    const char            *aErrText,
    struct Pid             aPid,
    struct Tid             aTid,
    const struct Duration *aElapsed,
    uint64_t               aElapsed_h,
    uint64_t               aElapsed_m,
    uint64_t               aElapsed_s,
    uint64_t               aElapsed_ms,
    const char            *aFunction,
    const char            *aFile,
    unsigned               aLine,
    const char            *aFmt, ...)
{
    va_list args;

    va_start(args, aFmt);
    dprint_(EWOULDBLOCK,
            aErrCode, aErrText,
            aPid, aTid,
            aElapsed, aElapsed_h, aElapsed_m, aElapsed_s, aElapsed_ms,
            aFunction, aFile, aLine, aFmt, args);
    va_end(args);
}

/* -------------------------------------------------------------------------- */
static struct {
    char  *mBuf;
    size_t mSize;
    FILE  *mFile;
} printBuf_;

static void
print_(
    int aErrCode,
    const char *aFunction, const char *aFile, unsigned aLine,
    const char *aFmt, va_list aArgs)
{
    /* The glibc implementation does not re-issue IO from libio:
     *
     * https://lists.debian.org/debian-glibc/2007/06/msg00165.html
     * https://gcc.gnu.org/ml/libstdc++/2000-q2/msg00184.html
     *
     *   No.  The write functions must be able to time out.  Setting alarm()
     *   and try writing, returning after a while because the device does
     *   not respond.  Leave libio as it is.
     *
     * Additionally http://www.openwall.com/lists/musl/2015/06/05/11
     *
     *   My unpopular but firm opinion, which I already have expressed here,
     *   is that stdio is a toy interface that should not be used for any
     *   output that requires the simplest hint of reliability.
     *
     * In the normal case, output will be generated using open_memstream() and
     * writeFd(). Use of dprintf() is restricted to error cases, and in
     * these cases output can be truncated do to EINTR. */

    FINALLY
    ({
        struct Pid pid = ownProcessId();
        struct Tid tid = ownThreadId();

        /* The availability of buffered IO might be lost while a message
         * is being processed since this code might run in a thread
         * that continues to execute while the process is being shut down. */

        int  lockerr;
        bool locked;
        bool buffered;

        if (acquireProcessAppLock())
        {
            lockerr  = errno ? errno : EPERM;
            locked   = false;
            buffered = false;
        }
        else
        {
            lockerr  = EWOULDBLOCK;
            locked   = true;
            buffered = !! printBuf_.mFile;
        }

        struct Duration elapsed = ownProcessElapsedTime();

        uint64_t elapsed_ms = MSECS(elapsed.duration).ms;
        uint64_t elapsed_s;
        uint64_t elapsed_m;
        uint64_t elapsed_h;

        elapsed_h  = elapsed_ms / (1000 * 60 * 60);
        elapsed_m  = elapsed_ms % (1000 * 60 * 60) / (1000 * 60);
        elapsed_s  = elapsed_ms % (1000 * 60 * 60) % (1000 * 60) / 1000;
        elapsed_ms = elapsed_ms % (1000 * 60 * 60) % (1000 * 60) % 1000;

        const char *errText    = "";
        size_t      errTextLen = 0;

        if (aErrCode)
            errTextLen = findErrTextLength_(aErrCode);

        char errTextBuffer[1+errTextLen];

        if (errTextLen)
        {
            /* Annoyingly strerror() is not thread safe, so is pretty
             * much unusable in any contemporary context since there
             * is no way to be absolutely sure that there is no other
             * thread attempting to use it. */

            errno = 0;
            const char *errTextMsg =
                strerror_r(aErrCode, errTextBuffer, sizeof(errTextBuffer));
            if ( ! errno)
                errText = errTextMsg;
        }

        if ( ! buffered)
        {
            /* Note that there is an old defect which causes dprintf()
             * to race with fork():
             *
             *    https://sourceware.org/bugzilla/show_bug.cgi?id=12847
             *
             * The symptom is that the child process will terminate with
             * SIGSEGV in fresetlockfiles(). */

            dprint_(lockerr,
                    aErrCode, errText,
                    pid, tid,
                    &elapsed, elapsed_h, elapsed_m, elapsed_s, elapsed_ms,
                    aFunction, aFile, aLine,
                    aFmt, aArgs);
        }
        else
        {
            rewind(printBuf_.mFile);

            if ( ! aFile)
                fprintf(printBuf_.mFile, "%s: ", ownProcessName());
            else
            {
                if (pid.mPid == tid.mTid)
                {
                    if (elapsed.duration.ns)
                        fprintf(
                            printBuf_.mFile,
                            "%s: [%04" PRIu64 ":%02" PRIu64
                            ":%02" PRIu64
                            ".%03" PRIu64 " %" PRId_Pid " %s %s:%u] ",
                            ownProcessName(),
                            elapsed_h, elapsed_m, elapsed_s, elapsed_ms,
                            FMTd_Pid(pid), aFunction, aFile, aLine);
                    else
                        fprintf(
                            printBuf_.mFile,
                            "%s: [%" PRId_Pid " %s %s:%u] ",
                            ownProcessName(),
                            FMTd_Pid(pid), aFunction, aFile, aLine);
                }
                else
                {
                    if (elapsed.duration.ns)
                        fprintf(
                            printBuf_.mFile,
                            "%s: [%04" PRIu64 ":%02" PRIu64
                            ":%02" PRIu64
                            ".%03" PRIu64 " %" PRId_Pid ":%" PRId_Tid " "
                            "%s %s:%u] ",
                            ownProcessName(),
                            elapsed_h, elapsed_m, elapsed_s, elapsed_ms,
                            FMTd_Pid(pid), FMTd_Tid(tid),
                            aFunction, aFile, aLine);
                    else
                        fprintf(
                            printBuf_.mFile,
                            "%s: [%" PRId_Pid ":%" PRId_Tid " %s %s:%u] ",
                            ownProcessName(),
                            FMTd_Pid(pid), FMTd_Tid(tid),
                            aFunction, aFile, aLine);
                }
            }

            xvfprintf(printBuf_.mFile, aFmt, aArgs);
            if ( ! aErrCode)
                fprintf(printBuf_.mFile, "\n");
            else if (errText)
                fprintf(
                    printBuf_.mFile, " - errno %d [%s]\n", aErrCode, errText);
            else
                fprintf(printBuf_.mFile, " - errno %d\n", aErrCode);
            fflush(printBuf_.mFile);

            /* Use writeFdRaw() rather than writeFd() to avoid having
             * EINTR error injection cause diagnostic messages to
             * be issued recursively. This avoids the message log being
             * unsightly with EINTR messages grafted into the middle of
             * message lines, and also EINTR messages with later timestamps
             * printed before messages with earlier timestamps. */

            if (printBuf_.mSize != writeFdRaw(STDERR_FILENO,
                                              printBuf_.mBuf,
                                              printBuf_.mSize, 0))
                abortProcess();
        }

        if (locked)
        {
            if (releaseProcessAppLock())
            {
                dprintf_(
                    errno, 0,
                    pid, tid,
                    &elapsed, elapsed_h, elapsed_m, elapsed_s, elapsed_ms,
                    __func__, __FILE__, __LINE__,
                    "Unable to release process lock");
                abortProcess();
            }
        }
    });
}

/* -------------------------------------------------------------------------- */
static void
printf_(
    int aErrCode,
    const char *aFunction, const char *aFile, unsigned aLine,
    const char *aFmt, ...)
{
    FINALLY
    ({
        va_list args;

        va_start(args, aFmt);
        print_(0, aFunction, aFile, aLine, aFmt, args);
        va_end(args);
    });
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
errorEnsureBacktrace_(int aFd, int aDepth)
{
    int rc = -1;

    char **frames = 0;

    ERROR_IF(
        0 >= aDepth,
        {
            errno = EINVAL;
        });

    {
        void *symbols[aDepth];

        int depth = backtrace(symbols, aDepth);

        ERROR_IF(
            (0 > depth || aDepth <= depth),
            {
                errno = EAGAIN;
            });

        frames = backtrace_symbols(symbols, depth);

        if ( ! frames || testAction(TestLevelRace))
            backtrace_symbols_fd(symbols, depth, aFd);
        else
        {
            for (int ix = 0; ix < depth; ++ix)
                printf_(0, 0, 0, 0, "%s", frames[ix]);
        }
    }

    rc = 0;

Finally:

    FINALLY
    ({
        free(frames);
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
void
ert_errorEnsure(
    const char *aFunction,
    const char *aFile,
    unsigned    aLine,
    const char *aPredicate)
{
    FINALLY
    ({
        static const char msgFmt[] = "Assertion failure - %s";

        const char *predicate = aPredicate ? aPredicate : "TEST";

        printf_(0, aFunction, aFile, aLine, msgFmt, predicate);

        if (RUNNING_ON_VALGRIND)
            VALGRIND_PRINTF_BACKTRACE(msgFmt, predicate);
        else
        {
            int err;

            int depth = 1;

            do
            {
                depth = depth * 2;
                err = errorEnsureBacktrace_(STDERR_FILENO, depth);

            } while (-1 == err && EAGAIN == errno);

            if (err)
                printf_(errno, 0, 0, 0, "Unable to print backtrace");
        }

        /* This conditional exit is used by the unit test to exercise
         * the backtrace functionality. */

        if (aPredicate)
            abortProcess();
        else
            exitProcess(EXIT_SUCCESS);
    });
}

/* -------------------------------------------------------------------------- */
void
ert_errorDebug(
    const char *aFunction, const char *aFile, unsigned aLine,
    const char *aFmt, ...)
{
    FINALLY
    ({
        va_list args;

        struct Ert_ErrorFrameSequence frameSequence =
            ert_pushErrorFrameSequence();

        va_start(args, aFmt);
        print_(0, aFunction, aFile, aLine, aFmt, args);
        va_end(args);

        ert_popErrorFrameSequence(frameSequence);
    });
}

/* -------------------------------------------------------------------------- */
void
ert_errorWarn(
    int aErrCode,
    const char *aFunction, const char *aFile, unsigned aLine,
    const char *aFmt, ...)
{
    FINALLY
    ({
        struct Ert_ErrorFrameSequence frameSequence =
            ert_pushErrorFrameSequence();

        va_list args;

        va_start(args, aFmt);
        print_(aErrCode, aFunction, aFile, aLine, aFmt, args);
        va_end(args);

        ert_popErrorFrameSequence(frameSequence);
    });

    struct Ert_ErrorUnwindFrame_ *unwindFrame =
        ert_ownErrorUnwindActiveFrame_();

    if (unwindFrame)
        longjmp(unwindFrame->mJmpBuf, 1);
}

/* -------------------------------------------------------------------------- */
void
ert_errorMessage(
    int aErrCode,
    const char *aFunction, const char *aFile, unsigned aLine,
    const char *aFmt, ...)
{
    FINALLY
    ({
        va_list args;

        struct Ert_ErrorFrameSequence frameSequence =
            ert_pushErrorFrameSequence();

        va_start(args, aFmt);
        print_(aErrCode, 0, 0, 0, aFmt, args);
        va_end(args);

        ert_popErrorFrameSequence(frameSequence);
    });
}

/* -------------------------------------------------------------------------- */
void
ert_errorTerminate(
    int aErrCode,
    const char *aFunction, const char *aFile, unsigned aLine,
    const char *aFmt, ...)
{
    FINALLY
    ({
        struct Ert_ErrorFrameSequence frameSequence =
            ert_pushErrorFrameSequence();

        va_list args;

        if (gErtOptions_.mDebug)
        {
            va_start(args, aFmt);
            print_(aErrCode, aFunction, aFile, aLine, aFmt, args);
            va_end(args);
        }

        ert_popErrorFrameSequence(frameSequence);

        va_start(args, aFmt);
        print_(aErrCode, 0, 0, 0, aFmt, args);
        va_end(args);

        abortProcess();
    });
}

/* -------------------------------------------------------------------------- */
static struct Ert_ErrorModule *
Error_exit_(struct Ert_ErrorModule *self)
{
    if (self)
    {
        struct ProcessAppLock *appLock = createProcessAppLock();

        FILE *file = printBuf_.mFile;

        if (file)
        {
            ABORT_IF(
                fclose(file));

            free(printBuf_.mBuf);

            printBuf_.mFile = 0;
        }

        appLock = destroyProcessAppLock(appLock);

        self->mPrintfModule = Printf_exit(self->mPrintfModule);
    }

    return 0;
}

struct Ert_ErrorModule *
Ert_Error_exit(struct Ert_ErrorModule *self)
{
    if (self)
    {
        if (0 == --moduleInit_)
            self = Error_exit_(self);
    }

    return 0;
}


/* -------------------------------------------------------------------------- */
int
Ert_Error_init(struct Ert_ErrorModule *self)
{
    int rc = -1;

    struct ProcessAppLock *appLock = 0;

    self->mModule       = self;
    self->mPrintfModule = 0;

    if ( ! moduleInit_)
    {
        ERROR_IF(
            Printf_init(&self->mPrintfModule_));
        self->mPrintfModule = &self->mPrintfModule_;

        printBuf_.mBuf  = 0;
        printBuf_.mSize = 0;

        FILE *file;
        ERROR_UNLESS(
            (file = open_memstream(&printBuf_.mBuf, &printBuf_.mSize)));

        appLock = createProcessAppLock();

        printBuf_.mFile = file;
    }

    ++moduleInit_;

    rc = 0;

Finally:

    FINALLY
    ({
        appLock = destroyProcessAppLock(appLock);

        if (rc)
            self = Error_exit_(self);
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
