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
#include "ert/void.h"

#include "eintr_.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <execinfo.h>

#include <sys/mman.h>

#include <valgrind/valgrind.h>

/* -------------------------------------------------------------------------- */
#define ERT_METHOD_DEFINITION
#define ERT_METHOD_RETURN_ErrorFrameVisitorMethod    int
#define ERT_METHOD_CONST_ErrorFrameVisitorMethod
#define ERT_METHOD_ARG_LIST_ErrorFrameVisitorMethod  \
        (unsigned aOffset, const struct Ert_ErrorFrame *aFrame)
#define ERT_METHOD_CALL_LIST_ErrorFrameVisitorMethod (aOffset, aFrame)

#define ERT_METHOD_TYPE_PREFIX
#define ERT_METHOD_FUNCTION_PREFIX

#define ERT_METHOD_NAME      ErrorFrameVisitorMethod
#define ERT_METHOD_RETURN    ERT_METHOD_RETURN_ErrorFrameVisitorMethod
#define ERT_METHOD_CONST     ERT_METHOD_CONST_ErrorFrameVisitorMethod
#define ERT_METHOD_ARG_LIST  ERT_METHOD_ARG_LIST_ErrorFrameVisitorMethod
#define ERT_METHOD_CALL_LIST ERT_METHOD_CALL_LIST_ErrorFrameVisitorMethod
#include "ert/method.h"

#define ErrorFrameVisitorMethod(Object_, Method_)     \
    ERT_METHOD_TRAMPOLINE(                            \
        Object_, Method_,                             \
        ErrorFrameVisitorMethod_,                     \
        ERT_METHOD_RETURN_ErrorFrameVisitorMethod,    \
        ERT_METHOD_CONST_ErrorFrameVisitorMethod,     \
        ERT_METHOD_ARG_LIST_ErrorFrameVisitorMethod,  \
        ERT_METHOD_CALL_LIST_ErrorFrameVisitorMethod)

/* -------------------------------------------------------------------------- */
/* Unwinding Error Frame
 *
 * Error frames are used to provide context when a program terminates.
 * The approach approximates exception stack unwinding without actual
 * support for exception handling. Results depend on the implementation
 * using ERROR_IF and ABORT_IF idioms.
 *
 * Rather than tracking stack frames in the program call stack, error
 * frames are activated on the error frame stack only as errors are
 * encountered in the program. Generally this means that error frames
 * are pushed onto the error frame stack as errors propagate back up
 * the program call stack.
 *
 * Each ERROR_IF is modelled as an Error-Block. The life cycle of
 * an Error-Block comprises: activation (E), successful completion (S),
 * and failed completion (F).
 *
 * Each ABORT_IF is modelled as an Abort-Block. The life ccle of
 * an Abort-Block comprises: activation (A), successful completion (C),
 * and program termination (T). Only the latter is important for unwinding
 * the stack and logging program context.
 *
 * When program termination occurs, the error frame stack comprises
 * the following important elements:
 *
 * Error Frame Stack:    ... T F F F
 * Error Frame Index:    ...   2 3 4
 * Error Frame Sequence: ... 2
 * Program Stack Depth:  ... 4 7 6 5
 *
 * Activation of the Abort-Block is shown at program stack depth 4, and
 * the most important thing to note is that program termination T is
 * triggered by Error-Block failure F at program stack depth 5 (ie 4+1).
 * Once program termination is triggered, the sequence of error frames F
 * at successive program stack depths, and error frame indices, provides
 * context for the program termination.
 *
 * Notice that the base error frame index matches the error frame sequence
 * of the Abort-Block termination T. The error frames F are pushed on to
 * the error frame stack (ie 2, 3, 4) in reverse order of invocation as
 * shown by the program stack depth (ie 7, 6, 5).
 *
 * Any particular Error-Block failure does not necessarily lead to
 * program termination:
 *
 * Error Frame Stack:    A F F T ...
 * Error Frame Index:      0 1   ...
 * Error Frame Sequence: 0     2 ...
 * Program Stack Depth:  0 8 2 4 ...
 *
 * The above also illustrates how error frames F are pushed onto the
 * error stack as they are created. This can lead to gaps because
 * not all Error-Block activations E end in failure. A sequence of
 * Error-Block activations can end in any mixture of success and failure.
 *
 * Activation of each new Abort-Block A advances the error frame sequence
 * by using the current error frame index. The error frame sequence retreats
 * to its previous value if the Abort-Block completes without termination.
 *
 * Advancing the error frame sequence in this way is important because
 * the activation of the Abort-Block might be preceded by a candidate
 * error frame sequence:
 *
 * Error Frame Stack:    A F F F A ...
 * Error Frame Index:      0 1 2   ...
 * Error Frame Sequence: 0       3 ...
 * Program Stack Depth:  0 3 2 1 1 ...
 *
 * This example shows activation A at program stack depth 1. It is quite
 * possible that this Abort-Block completes successfully, but execution
 * continues and delivers the error frame sequence to the enclosing
 * Abort-Block at program stack depth 0.
 *
 * This scenario can occur because:
 *
 *  o Abort-Block activations can occur any, in particular in Finally-Blocks
 *  o Error-Block activations are not permitted in Finally-Blocks
 *
 * It is because Abort-Block activations are allowed in Finally-Blocks
 * that they cannot trigger restarts of the candidate sequence of error
 * frames. The Finally-Block is activated even after an Error-Block failure
 * and restarting the candidate sequence would lose that record.
 *
 * Restarting of the candidate sequence of error frames occurs at
 * the following times:
 *
 *  o Immediately before activation of a new Error-Block E
 *  o Immediately before the end of a Finally-Block
 *
 * The former is to accommodate this scenario:
 *
 *    int e()
 *    {
 *        ERROR_IF(1, { errno = EPERM; });
 *
 *    Ert_Finally:
 *        ERT_FINALLY({});
 *        return 0;
 *    }
 *
 *    int f()
 *    {
 *        ERROR_IF(e() && EACCES == errno);
 *        ERROR_IF(1);  // Error triggered here
 *        // Unwinding should not include e()
 *
 *    Ert_Finally:
 *        ERT_FINALLY({});
 *        return 0;
 *    }
 *
 * The latter is to accommodate this scenario:
 *
 *    int e()
 *    {
 *        ERROR_IF(1, { errno = EPERM; }); // Error frame pushed here
 *
 *    Ert_Finally:
 *        ERT_FINALLY({});
 *        return 0;
 *    }
 *
 *    int f()
 *    {
 *        ERROR_IF(e() && EACCES == errno);
 *        // Error not triggered due because EPERM == errno
 *
 *    Ert_Finally:
 *        ERT_FINALLY({});
 *        return 0;
 *    }
 *
 *    int g()
 *    {
 *        return -1;
 *    }
 *
 *    int h()
 *    {
 *        ERROR_IF(f() || g()); // Error triggered here by g()
 *        // Unwinding should not include f()
 *
 *    Ert_Finally:
 *        ERT_FINALLY({});
 *        return 0;
 *    }
 */

/* -------------------------------------------------------------------------- */
static unsigned moduleInit_;

typedef TAILQ_HEAD(
   Ert_ErrorFrameChunkList, Ert_ErrorFrameChunk) Ert_ErrorFrameChunkListT;

struct Ert_ErrorFrameChunk
{
    TAILQ_ENTRY(Ert_ErrorFrameChunk) mStackList;

    struct Ert_ErrorFrameChunk *mChunkList;
    size_t                      mChunkSize;

    struct Ert_ErrorFrame *mBegin;
    struct Ert_ErrorFrame *mEnd;

    struct Ert_ErrorFrame  mFrame_[];
};

static struct Ert_ErrorFrameStackPool
{
    /* Carefully crafted so that new threads will see an initialised
     * instance that can be used immediately. In particular, note that
     * threads cannot be created from signal context, so
     * Ert_ErrorFrameStackThread must be zero. */

    struct Ert_ErrorFrameChunk *mAlloc[0 == Ert_ErrorFrameStackThread];

    struct
    {
        struct Ert_ErrorFrameSequenceTail mTail;
        struct Ert_ErrorFrameSequenceHead mHead;

        Ert_ErrorFrameChunkListT mChunkList;

    } mStack_[Ert_ErrorFrameStackKinds], *mStack;

    unsigned mSeqIndex;

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
            ert_abortProcess();

        VALGRIND_FREELIKE_BLOCK(chunk, 0);

        chunk = next;
    }
}

static void
initErrorKey_(void)
{
    if (pthread_once(
            &errorDtor_.mOnce,
            ERT_LAMBDA(
                void, (void),
                {
                    if (pthread_key_create(
                            &errorDtor_.mKey, destroyErrorKey_))
                        ert_abortProcess();
                })))
        ert_abortProcess();
}

static void
setErrorKey_(struct Ert_ErrorFrameChunk *aChunk)
{
    initErrorKey_();

    if (pthread_setspecific(errorDtor_.mKey, aChunk))
        ert_abortProcess();
}

ERT_EARLY_INITIALISER(
    error_,
    ({
        initErrorKey_();
    }),
    ({ }));

/* -------------------------------------------------------------------------- */
static struct Ert_ErrorFrameChunk *
createErrorFrameChunk_(void)
{
    struct Ert_ErrorFrameChunk *self = 0;

    /* Do not use fetchSystemPageSize() because that might cause a recursive
     * reference to createErrorFrameChunk_(). */

    long pageSize = sysconf(_SC_PAGESIZE);

    if (-1 == pageSize || ! pageSize)
        ert_abortProcess();

    /* Do not use malloc() because the the implementation in malloc_.c
     * will cause a recursive reference to createErrorFrameChunk_(). */

    size_t chunkSize =
        ERT_ROUNDUP(sizeof(*self) + sizeof(self->mFrame_[0]), pageSize);

    self = mmap(0, chunkSize,
                PROT_READ | PROT_WRITE,
                MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    if (MAP_FAILED == self)
        ert_abortProcess();

    VALGRIND_MALLOCLIKE_BLOCK(self, chunkSize, 0, 0);

    self->mChunkList = 0;
    self->mChunkSize = chunkSize;

    /* Find out the number of frames that will fit in the allocated
     * space, but only use a small number during test in order to
     * exercise the frame allocator. */

    size_t numFrames =
        (chunkSize - sizeof(*self)) / sizeof(self->mFrame_[0]);

    unsigned testFrames = 2;

    if (ert_testMode(Ert_TestLevelRace) && numFrames > testFrames)
        numFrames = testFrames;

    self->mBegin = &self->mFrame_[0];
    self->mEnd   = &self->mFrame_[numFrames];

    if (errorStack_.mAlloc[0])
        errorStack_.mAlloc[0]->mChunkList = self;
    else
        setErrorKey_(self);

    errorStack_.mAlloc[0] = self;

    return self;
}

static void
initErrorFrame_(void)
{
    ert_Error_assert_(0 == Ert_ErrorFrameStackThread);

    if ( ! errorStack_.mStack)
    {
        errorStack_.mSeqIndex = 0;

        for (unsigned ix = ERT_NUMBEROF(errorStack_.mStack_); ix--; )
        {
            errorStack_.mStack = &errorStack_.mStack_[ix];

            TAILQ_INIT(&errorStack_.mStack->mChunkList);

            struct Ert_ErrorFrameChunk *chunk = createErrorFrameChunk_();

            TAILQ_INSERT_TAIL(
                &errorStack_.mStack->mChunkList, chunk, mStackList);

            errorStack_.mStack->mHead = (struct Ert_ErrorFrameSequenceHead)
            {
                .mIter     = { .mFrame = chunk->mBegin, .mChunk = chunk, },
                .mSeqIndex = errorStack_.mSeqIndex++,
            };

            errorStack_.mStack->mTail = (struct Ert_ErrorFrameSequenceTail)
            {
                .mIter   = errorStack_.mStack->mHead.mIter,
                .mOffset = 0,
            };
        }
    }
}

/* -------------------------------------------------------------------------- */
static int
ert_freezeErrorFrame_(int aFd, const struct Ert_ErrorFrame *aFrame)
{
    int rc = -1;

    ssize_t wroteLen = -1;
    ERT_ERROR_IF(
        (wroteLen = ert_writeFdRaw(
            aFd, (const void *) aFrame, sizeof(*aFrame), 0),
         -1 == wroteLen || sizeof(*aFrame) != wroteLen),
        {
            if (-1 != wroteLen)
                errno = EIO;
        });

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
static int
ert_thawErrorFrame_(int aFd, struct Ert_ErrorFrame *aFrame)
{
    int rc = -1;

    ssize_t readLen = -1;
    ERT_ERROR_IF(
        (readLen = ert_readFdRaw(
            aFd, (void *) aFrame, sizeof(*aFrame), 0),
         -1 == readLen || sizeof(*aFrame) != readLen),
        {
            if (-1 != readLen)
                errno = EIO;
        });

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
void
ert_addErrorFrame_(
    const struct Ert_ErrorFrame           *self,
    int                                    aErrno,
    const struct Ert_ErrorFrameSequenceId *aSeqId )
{
    initErrorFrame_();

    struct Ert_ErrorFrameSequenceTail *tail = &errorStack_.mStack->mTail;
    struct Ert_ErrorFrameIter         *iter = &tail->mIter;

    if (iter->mFrame == iter->mChunk->mEnd)
    {
        struct Ert_ErrorFrameChunk *chunk = createErrorFrameChunk_();

        TAILQ_INSERT_TAIL(&errorStack_.mStack->mChunkList, chunk, mStackList);

        iter->mChunk = chunk;
        iter->mFrame = chunk->mBegin;
    }

    iter->mFrame[0]        = *self;
    iter->mFrame[0].mErrno = aErrno;
    iter->mFrame[0].mSeqId = (
        aSeqId ? *aSeqId : ert_ownErrorFrameSequenceId());

    ++tail->mOffset;
    ++iter->mFrame;
}

/* -------------------------------------------------------------------------- */
void
ert_restartErrorFrameSequence_(void)
{
    initErrorFrame_();

    if (errorStack_.mStack->mTail.mOffset)
    {
        errorStack_.mStack->mTail.mOffset   = 0;
        errorStack_.mStack->mHead.mSeqIndex = errorStack_.mSeqIndex++;
        errorStack_.mStack->mTail.mIter     = errorStack_.mStack->mHead.mIter;
    }
}

/* -------------------------------------------------------------------------- */
struct Ert_ErrorFrameSequence
ert_pushErrorFrameSequence(void)
{
    initErrorFrame_();

    struct Ert_ErrorFrameSequence frameSequence =
    {
        .mRange =
        {
            .mBegin = errorStack_.mStack->mHead.mIter,
            .mEnd   = errorStack_.mStack->mTail.mIter,
        },
        .mSeqIndex = errorStack_.mStack->mHead.mSeqIndex,
        .mOffset   = errorStack_.mStack->mTail.mOffset,
    };

    errorStack_.mStack->mHead.mIter = errorStack_.mStack->mTail.mIter;

    errorStack_.mStack->mHead.mSeqIndex = errorStack_.mSeqIndex++;
    errorStack_.mStack->mTail.mOffset   = 0;

    return frameSequence;
}

/* -------------------------------------------------------------------------- */
void
ert_popErrorFrameSequence(struct Ert_ErrorFrameSequence aSequence)
{
    ert_restartErrorFrameSequence_();

    if (errorStack_.mSeqIndex == errorStack_.mStack->mHead.mSeqIndex + 1)
        errorStack_.mSeqIndex = errorStack_.mStack->mHead.mSeqIndex;

    errorStack_.mStack->mHead.mSeqIndex = aSequence.mSeqIndex;
    errorStack_.mStack->mHead.mIter     = aSequence.mRange.mBegin;

    errorStack_.mStack->mTail.mOffset   = aSequence.mOffset;
    errorStack_.mStack->mTail.mIter     = aSequence.mRange.mEnd;
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
struct Ert_ErrorFrameSequenceId
ert_ownErrorFrameSequenceId(void)
{
    initErrorFrame_();

    return (struct Ert_ErrorFrameSequenceId)
    {
        .mTid      = ert_ownThreadId(),
        .mSeqIndex = errorStack_.mStack->mHead.mSeqIndex,
    };
}

/* -------------------------------------------------------------------------- */
static unsigned
ert_ownErrorFrameSequenceLength_(const struct Ert_ErrorFrameSequence *self)
{
    return self->mOffset;
}

/* -------------------------------------------------------------------------- */
unsigned
ert_ownErrorFrameOffset_(void)
{
    initErrorFrame_();

    return errorStack_.mStack->mTail.mOffset;
}

/* -------------------------------------------------------------------------- */
const struct Ert_ErrorFrame *
ert_ownErrorFrame_(enum Ert_ErrorFrameStackKind aStack, unsigned aOffset)
{
    initErrorFrame_();

    struct Ert_ErrorFrame *frame = 0;

    if (aOffset < errorStack_.mStack->mTail.mOffset)
    {
        struct Ert_ErrorFrame *begin =
            errorStack_.mStack->mTail.mIter.mChunk->mBegin;

        struct Ert_ErrorFrame *end =
            errorStack_.mStack->mTail.mIter.mFrame;

        unsigned top = errorStack_.mStack->mTail.mOffset;

        while (1)
        {
            unsigned size = end - begin;
            unsigned tail = top - aOffset;

            if (size >= tail)
            {
                frame = end - tail;
                break;
            }

            struct Ert_ErrorFrameChunk *prevChunk =
                TAILQ_PREV(errorStack_.mStack->mTail.mIter.mChunk,
                           Ert_ErrorFrameChunkList,
                           mStackList);

            ert_Error_assert_(prevChunk);

            begin = prevChunk->mBegin;
            end   = prevChunk->mEnd;
            top   = top - size;
        }
    }

    return frame;
}

/* -------------------------------------------------------------------------- */
static int
visitErrorFrameSequence_(
    const struct Ert_ErrorFrameSequence *self,
    struct ErrorFrameVisitorMethod       aVisitor)
{
    int rc = -1;

    struct Ert_ErrorFrameIter iter = self->mRange.mBegin;

    /* Avoid perturbing the current error frame sequence while visiting
     * all the frames in the sequence. */

    for (unsigned offset = 0; offset < self->mOffset; ++offset)
    {
        if (iter.mFrame == iter.mChunk->mEnd)
        {
            iter.mChunk = TAILQ_NEXT(iter.mChunk, mStackList);
            iter.mFrame = iter.mChunk->mBegin;
        }

        ERT_ERROR_IF(
            callErrorFrameVisitorMethod(aVisitor, offset, iter.mFrame));

        ++iter.mFrame;
    }

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_freezeErrorFrameSequence(
    int aFd, const struct Ert_ErrorFrameSequence *aSequence)
{
    int rc = -1;

    initErrorFrame_();

    struct SequenceIterator
    {
        int      mFd;
        unsigned mLength;
    } seqIter = {
        .mFd     = aFd,
        .mLength = 0,
    };

    unsigned seqLength = ert_ownErrorFrameSequenceLength_(aSequence);

    ssize_t wroteLen = -1;
    ERT_ERROR_IF(
        (wroteLen = ert_writeFd(
            aFd, (const void *) &seqLength, sizeof(seqLength), 0),
         -1 == wroteLen || sizeof(seqLength) != wroteLen),
        {
            if (-1 != wroteLen)
                errno = EIO;
        });

    struct ErrorFrameVisitorMethod visitor = ErrorFrameVisitorMethod(
        &seqIter,
        ERT_LAMBDA(
            int, (struct SequenceIterator             *self_,
                  unsigned                             aOffset_,
                  const struct Ert_ErrorFrame         *aFramePtr),
            {
                ++self_->mLength;
                return ert_freezeErrorFrame_(self_->mFd, aFramePtr);
            }));

    ERT_ERROR_IF(
        visitErrorFrameSequence_(aSequence, visitor));

    ert_ensure(seqLength == seqIter.mLength);

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc ? rc : seqLength;
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
ert_thawErrorFrameSequence_(int aFd, unsigned aSeqLength)
{
    int rc = -1;

    ert_ensure(aSeqLength);

    struct Ert_ErrorFrame errorFrames[aSeqLength];

    for (unsigned sx = 0; sx < aSeqLength; ++sx)
        ERT_ERROR_IF(
            ert_thawErrorFrame_(aFd, &errorFrames[sx]));

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    /* If this method returns successfully, the thawed error will
     * be placed on the error frame stack. Otherwise the method
     * result code will indicate an error, and the error frame stack
     * will yield the error that caused the thawing to fail. */

    if ( ! rc)
    {
        ert_restartErrorFrameSequence_();

        for (unsigned sx = 0; sx < aSeqLength; ++sx)
            ert_addErrorFrame_(
                &errorFrames[sx],
                errorFrames[sx].mErrno,
                &errorFrames[sx].mSeqId);

        errno = errorFrames[aSeqLength-1].mErrno;
    }

    return rc;
}

int
ert_thawErrorFrameSequence(int aFd)
{
    int rc = -1;

    unsigned seqLength = 0;

    ssize_t readLen = -1;
    ERT_ERROR_IF(
        (readLen = ert_readFd(
            aFd, (void *) &seqLength, sizeof(seqLength), 0),
         -1 == readLen || sizeof(seqLength) != readLen),
        {
            if (-1 != readLen)
                errno = EIO;
        });

    if (seqLength)
    {
        ERT_ERROR_IF(
            ert_thawErrorFrameSequence_(aFd, seqLength));

        /* If thawing succeeds, return an error code to the caller to
         * represent the thawed error frame sequence. */

        goto Ert_Error_;
    }

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
void
ert_logErrorFrameSequence(void)
{
    initErrorFrame_();

    struct ErrorFrameVisitorMethod visitor = ErrorFrameVisitorMethod(
        ert_Void(),
        ERT_LAMBDA(
            int, (struct Ert_Void             *self_,
                  unsigned                     aFrameOffset,
                  const struct Ert_ErrorFrame *aFramePtr),
            {
                ert_errorWarn(
                    aFramePtr->mErrno,
                    aFramePtr->mName,
                    aFramePtr->mFile,
                    aFramePtr->mLine,
                    "%" PRIs_Ert_ErrorFrameSequenceId " Error frame %u - %s",
                    FMTs_Ert_ErrorFrameSequenceId(aFramePtr->mSeqId),
                    aFrameOffset,
                    aFramePtr->mText);

                return 0;
            }));

    struct Ert_ErrorFrameSequence frameSequence = ert_pushErrorFrameSequence();

    while (visitErrorFrameSequence_(&frameSequence, visitor))
        break;

    ert_popErrorFrameSequence(frameSequence);
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
tryErrTextLength_(int aErrCode, size_t *aSize)
{
    int rc = -1;

    char errCodeText[*aSize];

    const char *errText;
    ERT_ERROR_IF(
        (errno = 0,
         errText = strerror_r(aErrCode, errCodeText, sizeof(errCodeText)),
         errno));

    *aSize = errCodeText != errText ? 1 : strlen(errCodeText);

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

static ERT_CHECKED size_t
findErrTextLength_(int aErrCode)
{
    size_t rc = 0;

    size_t textCapacity = ert_testMode(Ert_TestLevelRace) ? 2 : 128;

    while (1)
    {
        size_t textSize = textCapacity;

        ERT_ERROR_IF(
            tryErrTextLength_(aErrCode, &textSize));

        if (textCapacity > textSize)
        {
            rc = textSize;
            break;
        }

        textSize = 2 * textCapacity;
        ert_Error_assert_(textCapacity < textSize);

        textCapacity = textSize;
    }

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
static void
dprint_(
    int                        aLockErr,
    int                        aErrCode,
    const char                *aErrText,
    struct Ert_Pid             aPid,
    struct Ert_Tid             aTid,
    const struct Ert_Duration *aElapsed,
    uint64_t                   aElapsed_h,
    uint64_t                   aElapsed_m,
    uint64_t                   aElapsed_s,
    uint64_t                   aElapsed_ms,
    const char                *aFunction,
    const char                *aFile,
    unsigned                   aLine,
    const char                *aFmt, va_list aArgs)
{

    if ( ! aFile)
        dprintf(STDERR_FILENO, "%s: ", ert_ownProcessName());
    else
    {
        if (aPid.mPid == aTid.mTid)
        {
            if (aElapsed->duration.ns)
                dprintf(
                    STDERR_FILENO,
                    "%s: [%04" PRIu64 ":%02" PRIu64
                    ":%02" PRIu64
                    ".%03" PRIu64 " %" PRId_Ert_Pid " %s %s:%u] ",
                    ert_ownProcessName(),
                    aElapsed_h, aElapsed_m, aElapsed_s, aElapsed_ms,
                    FMTd_Ert_Pid(aPid), aFunction, aFile, aLine);
            else
                dprintf(
                    STDERR_FILENO,
                    "%s: [%" PRId_Ert_Pid " %s %s:%u] ",
                    ert_ownProcessName(),
                    FMTd_Ert_Pid(aPid), aFunction, aFile, aLine);
        }
        else
        {
            if (aElapsed->duration.ns)
                dprintf(
                    STDERR_FILENO,
                    "%s: [%04" PRIu64 ":%02" PRIu64
                    ":%02" PRIu64
                    ".%03" PRIu64
                    " %" PRId_Ert_Pid
                    ":%" PRId_Ert_Tid " %s %s:%u] ",
                    ert_ownProcessName(),
                    aElapsed_h, aElapsed_m, aElapsed_s, aElapsed_ms,
                    FMTd_Ert_Pid(aPid), FMTd_Ert_Tid(aTid),
                    aFunction, aFile, aLine);
            else
                dprintf(
                    STDERR_FILENO,
                    "%s: [%" PRId_Ert_Pid ":%" PRId_Ert_Tid " %s %s:%u] ",
                    ert_ownProcessName(),
                    FMTd_Ert_Pid(aPid), FMTd_Ert_Tid(aTid),
                    aFunction, aFile, aLine);
        }

        if (EWOULDBLOCK != aLockErr)
            dprintf(STDERR_FILENO, "- lock error %d - ", aLockErr);
    }

    ert_vdprintf(STDERR_FILENO, aFmt, aArgs);
    if ( ! aErrCode)
        dprintf(STDERR_FILENO, "\n");
    else if (aErrText)
        dprintf(STDERR_FILENO, " - errno %d [%s]\n", aErrCode, aErrText);
    else
        dprintf(STDERR_FILENO, " - errno %d\n", aErrCode);
}

static void
dprintf_(
    int                        aErrCode,
    const char                *aErrText,
    struct Ert_Pid             aPid,
    struct Ert_Tid             aTid,
    const struct Ert_Duration *aElapsed,
    uint64_t                   aElapsed_h,
    uint64_t                   aElapsed_m,
    uint64_t                   aElapsed_s,
    uint64_t                   aElapsed_ms,
    const char                *aFunction,
    const char                *aFile,
    unsigned                   aLine,
    const char                *aFmt, ...)
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

    ERT_SCOPED_ERRNO
    ({
        struct Ert_Pid pid = ert_ownProcessId();
        struct Ert_Tid tid = ert_ownThreadId();

        /* The availability of buffered IO might be lost while a message
         * is being processed since this code might run in a thread
         * that continues to execute while the process is being shut down. */

        int  lockerr;
        bool locked;
        bool buffered;

        if (ert_acquireProcessAppLock())
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

        struct Ert_Duration elapsed = ert_ownProcessElapsedTime();

        uint64_t elapsed_ms = ERT_MSECS(elapsed.duration).ms;
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
                fprintf(printBuf_.mFile, "%s: ", ert_ownProcessName());
            else
            {
                if (pid.mPid == tid.mTid)
                {
                    if (elapsed.duration.ns)
                        fprintf(
                            printBuf_.mFile,
                            "%s: [%04" PRIu64 ":%02" PRIu64
                            ":%02" PRIu64
                            ".%03" PRIu64 " %" PRId_Ert_Pid " %s %s:%u] ",
                            ert_ownProcessName(),
                            elapsed_h, elapsed_m, elapsed_s, elapsed_ms,
                            FMTd_Ert_Pid(pid), aFunction, aFile, aLine);
                    else
                        fprintf(
                            printBuf_.mFile,
                            "%s: [%" PRId_Ert_Pid " %s %s:%u] ",
                            ert_ownProcessName(),
                            FMTd_Ert_Pid(pid), aFunction, aFile, aLine);
                }
                else
                {
                    if (elapsed.duration.ns)
                        fprintf(
                            printBuf_.mFile,
                            "%s: [%04" PRIu64 ":%02" PRIu64
                            ":%02" PRIu64
                            ".%03" PRIu64 " %" PRId_Ert_Pid
                            ":%" PRId_Ert_Tid " "
                            "%s %s:%u] ",
                            ert_ownProcessName(),
                            elapsed_h, elapsed_m, elapsed_s, elapsed_ms,
                            FMTd_Ert_Pid(pid), FMTd_Ert_Tid(tid),
                            aFunction, aFile, aLine);
                    else
                        fprintf(
                            printBuf_.mFile,
                            "%s: [%" PRId_Ert_Pid
                            ":%" PRId_Ert_Tid " %s %s:%u] ",
                            ert_ownProcessName(),
                            FMTd_Ert_Pid(pid), FMTd_Ert_Tid(tid),
                            aFunction, aFile, aLine);
                }
            }

            ert_vfprintf(printBuf_.mFile, aFmt, aArgs);
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

            if (printBuf_.mSize != ert_writeFdRaw(STDERR_FILENO,
                                                  printBuf_.mBuf,
                                                  printBuf_.mSize, 0))
                ert_abortProcess();
        }

        if (locked)
        {
            if (ert_releaseProcessAppLock())
            {
                dprintf_(
                    errno, 0,
                    pid, tid,
                    &elapsed, elapsed_h, elapsed_m, elapsed_s, elapsed_ms,
                    __func__, __FILE__, __LINE__,
                    "Unable to release process lock");
                ert_abortProcess();
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
    ERT_SCOPED_ERRNO
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

    ERT_ERROR_IF(
        0 >= aDepth,
        {
            errno = EINVAL;
        });

    {
        void *symbols[aDepth];

        int depth = backtrace(symbols, aDepth);

        ERT_ERROR_IF(
            (0 > depth || aDepth <= depth),
            {
                errno = EAGAIN;
            });

        frames = backtrace_symbols(symbols, depth);

        if ( ! frames || ert_testAction(Ert_TestLevelRace))
            backtrace_symbols_fd(symbols, depth, aFd);
        else
        {
            for (int ix = 0; ix < depth; ++ix)
                printf_(0, 0, 0, 0, "%s", frames[ix]);
        }
    }

    rc = 0;

Ert_Finally:

    ERT_FINALLY
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
    ERT_SCOPED_ERRNO
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
            ert_abortProcess();
        else
            ert_exitProcess(EXIT_SUCCESS);
    });
}

/* -------------------------------------------------------------------------- */
void
ert_errorDebug(
    const char *aFunction, const char *aFile, unsigned aLine,
    const char *aFmt, ...)
{
    ERT_SCOPED_ERRNO
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
    ERT_SCOPED_ERRNO
    ({
        struct Ert_ErrorFrameSequence frameSequence =
            ert_pushErrorFrameSequence();

        va_list args;

        va_start(args, aFmt);
        print_(aErrCode, aFunction, aFile, aLine, aFmt, args);
        va_end(args);

        ert_popErrorFrameSequence(frameSequence);
    });
}

/* -------------------------------------------------------------------------- */
void
ert_errorMessage(
    int aErrCode,
    const char *aFunction, const char *aFile, unsigned aLine,
    const char *aFmt, ...)
{
    ERT_SCOPED_ERRNO
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
    ERT_SCOPED_ERRNO
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

        ert_abortProcess();
    });
}

/* -------------------------------------------------------------------------- */
static void
write_raw_(int aFd, const char *aBuf, size_t aBufLen)
{
    size_t      bufLen = aBufLen;
    const char *bufPtr = aBuf;

    while (bufLen)
    {
        ssize_t wroteLen = write_raw(aFd, bufPtr, bufLen);

        if (-1 == wroteLen)
        {
            if (EINTR == errno)
                continue;
            break;
        }

        bufLen -= wroteLen;
        bufPtr += wroteLen;
    }
}

void
ert_errorAssert_(const char *aPredicate, const char *aFile, unsigned aLine)
{
    char  line[sizeof(aLine) * CHAR_BIT + 1];
    char *linePtr = &line[ERT_NUMBEROF(line)];

    *--linePtr = 0;
    do
    {
        *--linePtr = "0123456789"[aLine % 10];
    } while (aLine /= 10);

    const char *msg[] =
    {
        aFile,
        ":",
        linePtr,
        " - ",
        aPredicate,
        "\n",
    };

    /* Emit the diagnostic message, but do so without using any
     * functions that might call ert/error.h recursively to
     * avoid the possibility of infinite recusion. */

    for (size_t ix = 0; ix < ERT_NUMBEROF(msg); ++ix)
    {
        if (msg[ix])
            write_raw_(STDERR_FILENO, msg[ix], strlen(msg[ix]));
    }

    ert_abortProcess();
}

/* -------------------------------------------------------------------------- */
static struct Ert_ErrorModule *
Error_exit_(struct Ert_ErrorModule *self)
{
    if (self)
    {
        struct Ert_ProcessAppLock *appLock = ert_createProcessAppLock();

        FILE *file = printBuf_.mFile;

        if (file)
        {
            ERT_ABORT_IF(
                fclose(file));

            free(printBuf_.mBuf);

            printBuf_.mFile = 0;
        }

        appLock = ert_destroyProcessAppLock(appLock);

        self->mPrintfModule = Ert_Printf_exit(self->mPrintfModule);
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

    struct Ert_ProcessAppLock *appLock = 0;

    self->mModule       = self;
    self->mPrintfModule = 0;

    if ( ! moduleInit_)
    {
        ERT_ERROR_IF(
            Ert_Printf_init(&self->mPrintfModule_));
        self->mPrintfModule = &self->mPrintfModule_;

        printBuf_.mBuf  = 0;
        printBuf_.mSize = 0;

        FILE *file;
        ERT_ERROR_UNLESS(
            (file = open_memstream(&printBuf_.mBuf, &printBuf_.mSize)));

        appLock = ert_createProcessAppLock();

        printBuf_.mFile = file;
    }

    ++moduleInit_;

    rc = 0;

Ert_Finally:

    ERT_FINALLY
    ({
        appLock = ert_destroyProcessAppLock(appLock);

        if (rc)
            self = Error_exit_(self);
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
