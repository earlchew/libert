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

#include "ert/fd.h"
#include "ert/deadline.h"
#include "ert/fdset.h"
#include "ert/system.h"

#include "eintr_.h"

#include <stdlib.h>


#include <valgrind/valgrind.h>

/* -------------------------------------------------------------------------- */
#define DEVNULLPATH "/dev/null"

static const char devNullPath_[] = DEVNULLPATH;

/* -------------------------------------------------------------------------- */
int
ert_openFd(const char *aPath, int aFlags, mode_t aMode)
{
    int rc = -1;

    int fd;

    do
    {
        ERT_ERROR_IF(
            (fd = open(aPath, aFlags, aMode),
             -1 == fd && EINTR != errno));
    }
    while (-1 == fd);

    rc = fd;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
static int
openBlockedFd_(int aSelector, int aFd)
{
    int rc = -1;
    int fd = -1;

    int pipefd[2];

    ERT_ERROR_IF(
        pipe2(pipefd, O_CLOEXEC));

    int selected   = 0 + aSelector;
    int unselected = 1 - aSelector;

    pipefd[unselected] = ert_closeFd(pipefd[unselected]);

    if (aFd != pipefd[selected])
    {
        fd = pipefd[selected];

        ERT_ERROR_IF(
            aFd != ert_duplicateFd(fd, aFd));

        fd = ert_closeFd(fd);
    }

    rc = 0;

Ert_Finally:

    ERT_FINALLY
    ({
        fd = ert_closeFd(fd);
    });

    return rc;
}

static int
openBlockedInput_(int aFd)
{
    return openBlockedFd_(0, aFd);
}

static int
openBlockedOutput_(int aFd)
{
    return openBlockedFd_(1, aFd);
}

int
ert_openStdFds(void)
{
    int rc = -1;

    /* Create a file descriptor to take the place of stdin, stdout or stderr
     * as required. Any created file descriptor will automatically be
     * closed on exec(). If an input file descriptor is created, that
     * descriptor will return eof on any attempted read. Conversely
     * if an output file descriptor is created, that descriptor will
     * return eof or SIGPIPE on any attempted write. */

    int validStdin;
    ERT_ERROR_IF(
        (validStdin = ert_ownFdValid(STDIN_FILENO),
         -1 == validStdin));

    int validStdout;
    ERT_ERROR_IF(
        (validStdout = ert_ownFdValid(STDOUT_FILENO),
         -1 == validStdout));

    int validStderr;
    ERT_ERROR_IF(
        (validStderr = ert_ownFdValid(STDERR_FILENO),
         -1 == validStderr));

    if ( ! validStdin)
        ERT_ERROR_IF(
            openBlockedInput_(STDIN_FILENO));

    if ( ! validStdout && ! validStderr)
    {
        ERT_ERROR_IF(
            openBlockedOutput_(STDOUT_FILENO));
        ERT_ERROR_IF(
            STDERR_FILENO != ert_duplicateFd(STDOUT_FILENO, STDERR_FILENO));
    }
    else if ( ! validStdout)
        ERT_ERROR_IF(
            openBlockedOutput_(STDOUT_FILENO));
    else if ( ! validStderr)
        ERT_ERROR_IF(
            openBlockedOutput_(STDERR_FILENO));

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_closeFd(int aFd)
{
    if (-1 != aFd)
    {
        /* Assume Posix semantics as described in:
         *    http://austingroupbugs.net/view.php?id=529
         *
         * Posix semantics are normally available if POSIX_CLOSE_RESTART
         * is defined, but this code will simply assume that these semantics are
         * available.
         *
         * For Posix, if POSIX_CLOSE_RESTART is zero, close() will never return
         * EINTR. Otherwise, close() can return EINTR but it is unspecified
         * if any function other than close() can be used on the file
         * descriptor. Fundamentally this means that if EINTR is returned,
         * the only useful thing that can be done is to close the file
         * descriptor. */

        int err;
        do
        {
            ERT_ABORT_IF(
                (err = close(aFd),
                 err && EINTR != errno && EINPROGRESS != errno));
        } while (err && EINTR == errno);
    }

    return -1;
}

/* -------------------------------------------------------------------------- */
struct FdWhiteListVisitor_
{
    int mFd;

    struct rlimit mFdLimit;
};

static int
closeFdWhiteListVisitor_(
    struct FdWhiteListVisitor_ *self, struct Ert_FdRange aRange)
{
    int rc = -1;

    /* Be careful not to underflow or overflow the arithmetic representation
     * if rlim_cur happens to be zero, or much wider than aFdLast. */

    int fdEnd = 0;

    if (self->mFdLimit.rlim_cur)
    {
        fdEnd =
            aRange.mRhs < self->mFdLimit.rlim_cur
            ? aRange.mRhs + 1
            : self->mFdLimit.rlim_cur;
    }

    int fdBegin = aRange.mLhs;

    if (fdBegin > fdEnd)
        fdBegin = fdEnd;

    for (int fd = self->mFd; fd < fdBegin; ++fd)
    {
        int valid;

        ERT_ERROR_IF(
            (valid = ert_ownFdValid(fd),
             -1 == valid));

        while (valid && ert_closeFd(fd))
            break;
    }

    self->mFd = fdEnd;

    rc = (self->mFdLimit.rlim_cur <= fdEnd);

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

int
ert_closeFdExceptWhiteList(const struct Ert_FdSet *aFdSet)
{
    int rc = -1;

    struct FdWhiteListVisitor_ whiteListVisitor;

    whiteListVisitor.mFd = 0;

    ERT_ERROR_IF(
        getrlimit(RLIMIT_NOFILE, &whiteListVisitor.mFdLimit));

    ERT_ERROR_IF(
        -1 == ert_visitFdSet(
            aFdSet,
            Ert_FdSetVisitor(&whiteListVisitor, closeFdWhiteListVisitor_)));

    /* Cover off all the file descriptors from the end of the last
     * whitelisted range, to the end of the process file descriptor
     * range. */

    ERT_ERROR_UNLESS(
        1 == closeFdWhiteListVisitor_(
            &whiteListVisitor,
            Ert_FdRange(
                whiteListVisitor.mFdLimit.rlim_cur,
                whiteListVisitor.mFdLimit.rlim_cur)));

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
struct FdBlackListVisitor_
{
    struct rlimit mFdLimit;
};

static int
closeFdBlackListVisitor_(
    struct FdBlackListVisitor_ *self, struct Ert_FdRange aRange)
{
    int rc = -1;

    int done = 1;

    int fd = aRange.mLhs;

    while (fd != self->mFdLimit.rlim_cur)
    {
        int valid;

        ERT_ERROR_IF(
            (valid = ert_ownFdValid(fd),
             -1 == valid));

        while (valid && ert_closeFd(fd))
            break;

        if (fd == aRange.mRhs)
        {
            done = 0;
            break;
        }

        ++fd;
    }

    rc = done;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

int
ert_closeFdOnlyBlackList(const struct Ert_FdSet *aFdSet)
{
    int rc = -1;

    struct FdBlackListVisitor_ blackListVisitor;

    ERT_ERROR_IF(
        getrlimit(RLIMIT_NOFILE, &blackListVisitor.mFdLimit));

    ERT_ERROR_IF(
        -1 == ert_visitFdSet(
            aFdSet,
            Ert_FdSetVisitor(&blackListVisitor, closeFdBlackListVisitor_)));

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
rankFd_(const void *aLhs, const void *aRhs)
{
    int lhs = * (const int *) aLhs;
    int rhs = * (const int *) aRhs;

    if (lhs < rhs) return -1;
    if (lhs > rhs) return +1;
    return 0;
}

int
ert_closeFdDescriptors(const int *aWhiteList, size_t aWhiteListLen)
{
    int rc = -1;

    if (aWhiteListLen)
    {
        struct rlimit noFile;

        ERT_ERROR_IF(
            getrlimit(RLIMIT_NOFILE, &noFile));

        int whiteList[aWhiteListLen + 1];

        whiteList[aWhiteListLen] = noFile.rlim_cur;

        for (size_t ix = 0; aWhiteListLen > ix; ++ix)
        {
            whiteList[ix] = aWhiteList[ix];
            ert_ensure(whiteList[aWhiteListLen] > whiteList[ix]);
        }

        qsort(
            whiteList,
            ERT_NUMBEROF(whiteList), sizeof(whiteList[0]), rankFd_);

        unsigned purgedFds = 0;

        for (int fd = 0, wx = 0; ERT_NUMBEROF(whiteList) > wx; )
        {
            if (0 > whiteList[wx])
            {
                ++wx;
                continue;
            }

            do
            {
                if (fd != whiteList[wx])
                {
                    ++purgedFds;

                    int valid;
                    ERT_ERROR_IF(
                        (valid = ert_ownFdValid(fd),
                         -1 == valid));

                    while (valid && ert_closeFd(fd))
                        break;
                }
                else
                {
                    ert_debug(0, "not closing fd %d", fd);

                    while (ERT_NUMBEROF(whiteList) > ++wx)
                    {
                        if (whiteList[wx] != fd)
                            break;
                    }
                }

            } while (0);

            ++fd;
        }

        ert_debug(0, "purged %u fds", purgedFds);
    }

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
bool
ert_stdFd(int aFd)
{
    return STDIN_FILENO == aFd || STDOUT_FILENO == aFd || STDERR_FILENO == aFd;
}

/* -------------------------------------------------------------------------- */
int
ert_closeFdOnExec(int aFd, unsigned aCloseOnExec)
{
    int rc = -1;

    unsigned closeOnExec = 0;

    if (aCloseOnExec)
    {
        ERT_ERROR_IF(
            aCloseOnExec != O_CLOEXEC,
            {
                errno = EINVAL;
            });

        /* Take care. O_CLOEXEC is the flag for obtaining close-on-exec
         * semantics when using open(), but fcntl() requires FD_CLOEXEC. */

        closeOnExec = FD_CLOEXEC;
    }

    int prevFlags;
    ERT_ERROR_IF(
        (prevFlags = fcntl(aFd, F_GETFD),
         -1 == prevFlags));

    int nextFlags = (prevFlags & ~FD_CLOEXEC) | closeOnExec;

    if (prevFlags != nextFlags)
        ERT_ERROR_IF(
            fcntl(aFd, F_SETFD, nextFlags));

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_duplicateFd(int aFd, int aTargetFd)
{
    /* Strangely dup() and dup2() do not preserve FD_CLOEXEC when duplicating
     * the file descriptor. This function interrogates the source file
     * descriptor, and transfers the FD_CLOEXEC flag when duplicating
     * the source to the target file descriptor. */

    int rc = -1;
    int fd = -1;

    int cloexec;
    ERT_ERROR_IF(
        (cloexec = ert_ownFdCloseOnExec(aFd),
         -1 == cloexec));

    if (-1 == aTargetFd)
    {
        long otherFd = 0;

        ERT_ERROR_IF(
            (fd = fcntl(aFd, cloexec ? F_DUPFD_CLOEXEC : F_DUPFD, otherFd),
             -1 == fd));
    }
    else if (aFd == aTargetFd)
    {
        fd = aTargetFd;
    }
    else
    {
        ERT_ERROR_IF(
            (fd = dup3(aFd, aTargetFd, cloexec ? O_CLOEXEC : 0),
             -1 == fd));
    }

    rc = fd;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_nullifyFd(int aFd)
{
    int rc = -1;

    int fd = -1;

    int closeExec;
    ERT_ERROR_IF(
        (closeExec = ert_ownFdCloseOnExec(aFd),
         -1 == closeExec));

    if (closeExec)
        closeExec = O_CLOEXEC;

    ERT_ERROR_IF(
        (fd = ert_openFd(devNullPath_, O_WRONLY | closeExec, 0),
         -1 == fd));

    if (fd == aFd)
        fd = -1;
    else
        ERT_ERROR_IF(
            aFd != ert_duplicateFd(fd, aFd));

    rc = 0;

Ert_Finally:

    ERT_FINALLY
    ({
        fd = ert_closeFd(fd);
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_nonBlockingFd(int aFd, unsigned aNonBlocking)
{
    int rc = -1;

    unsigned nonBlocking = 0;

    if (aNonBlocking)
    {
        ERT_ERROR_IF(
            aNonBlocking != O_NONBLOCK,
            {
                errno = EINVAL;
            });

        nonBlocking = O_NONBLOCK;
    }

    int prevStatusFlags;
    ERT_ERROR_IF(
        (prevStatusFlags = fcntl(aFd, F_GETFL),
         -1 == prevStatusFlags));

    int nextStatusFlags = (prevStatusFlags & ~O_NONBLOCK) | nonBlocking;

    if (prevStatusFlags != nextStatusFlags)
    {
        if (nonBlocking)
        {
            int descriptorFlags;
            ERT_ERROR_IF(
                (descriptorFlags = fcntl(aFd, F_GETFD),
                 -1 == descriptorFlags));

            /* Because O_NONBLOCK affects the underlying open file, to get some
             * peace of mind, only allow non-blocking mode on file descriptors
             * that are not going to be shared. This is not a water-tight
             * defense, but seeks to prevent some careless mistakes. */

            ERT_ERROR_UNLESS(
                descriptorFlags & FD_CLOEXEC,
                {
                    errno = EBADF;
                });
        }

        ERT_ERROR_IF(
            fcntl(aFd, F_SETFL, nextStatusFlags));
    }

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_ownFdNonBlocking(int aFd)
{
    int rc = -1;

    int flags;
    ERT_ERROR_IF(
        (flags = fcntl(aFd, F_GETFL),
         -1 == flags));

    rc = flags & O_NONBLOCK ? 1 : 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_ownFdCloseOnExec(int aFd)
{
    int rc = -1;

    int flags;
    ERT_ERROR_IF(
        (flags = fcntl(aFd, F_GETFD),
         -1 == flags));

    rc = flags & FD_CLOEXEC ? 1 : 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_ownFdFlags(int aFd)
{
    int rc = -1;

    int flags = -1;

    ERT_ERROR_IF(
        (flags = fcntl(aFd, F_GETFL),
         -1 == flags));

    rc = flags;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_ownFdValid(int aFd)
{
    int rc = -1;

    int valid = 1;

    ERT_ERROR_IF(
        (-1 == ert_ownFdFlags(aFd) && (valid = 0, EBADF != errno)));

    rc = valid;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_ioctlFd(int aFd, int aReq, void *aArg)
{
    int rc;

    do
    {
        rc = ioctl(aFd, aReq, aArg);
    } while (-1 == rc && EINTR == errno);

    return rc;
}

/* -------------------------------------------------------------------------- */
ssize_t
ert_spliceFd(int aSrcFd, int aDstFd, size_t aLen, unsigned aFlags)
{
    int rc = -1;

    if ( ! RUNNING_ON_VALGRIND)
        rc = splice(aSrcFd, 0, aDstFd, 0, aLen, aFlags);
    else
    {
        /* Early versions of valgrind do not support splice(), so
         * provide a workaround here. This implementation of splice()
         * does not mimic the kernel implementation exactly, but is
         * enough for testing. */

        char buffer[8192];

        size_t len = sizeof(buffer);

        if (len > aLen)
            len = aLen;

        ssize_t bytes;

        do
            ERT_ERROR_IF(
                (bytes = read(aSrcFd, buffer, len),
                 -1 == bytes && EINTR != errno));
        while (-1 == bytes);

        if (bytes)
        {
            char *bufptr = buffer;
            char *bufend = bufptr + bytes;

            while (bufptr != bufend)
            {
                ssize_t wrote;

                do
                    ERT_ERROR_IF(
                        (wrote = write(aDstFd, bufptr, bufend - bufptr),
                         -1 == wrote && EINTR != errno));
                while (-1 == wrote);

                bufptr += wrote;
            }
        }

        rc = bytes;
    }

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
waitFdReady_(int aFd, unsigned aPollMask, const struct Ert_Duration *aTimeout)
{
    int rc = -1;

    struct pollfd pollfd[1] =
    {
        {
            .fd      = aFd,
            .events  = aPollMask,
            .revents = 0,
        },
    };

    struct Ert_EventClockTime since = ERT_EVENTCLOCKTIME_INIT;
    struct Ert_Duration       remaining;

    const struct Ert_Duration timeout = aTimeout ? *aTimeout : Ert_ZeroDuration;

    struct Ert_ProcessSigContTracker sigContTracker =
        Ert_ProcessSigContTracker();

    while (1)
    {
        struct Ert_EventClockTime tm = ert_eventclockTime();

        ERT_TEST_RACE
        ({
            /* In case the process is stopped after the time is
             * latched, check once more if the fds are ready
             * before checking the deadline. */

            int events;

            ERT_ERROR_IF(
                (events = poll(pollfd, ERT_NUMBEROF(pollfd), 0),
                 -1 == events && EINTR != errno));

            if (0 > events)
                break;
        });

        int timeout_ms;

        if ( ! aTimeout)
            timeout_ms = -1;
        else
        {
            if (ert_deadlineTimeExpired(&since, timeout, &remaining, &tm))
            {
                if (ert_checkProcessSigContTracker(&sigContTracker))
                {
                    since = ERT_EVENTCLOCKTIME_INIT;
                    continue;
                }

                break;
            }

            uint64_t remaining_ms = ERT_MSECS(remaining.duration).ms;

            timeout_ms = remaining_ms;

            if (0 > timeout_ms || timeout_ms != remaining_ms)
                timeout_ms = INT_MAX;
        }

        int events;
        ERT_ERROR_IF(
            (events = poll(pollfd, ERT_NUMBEROF(pollfd), timeout_ms),
             -1 == events && EINTR != errno));

        switch (events)
        {
        default:
            break;

        case -1:
            continue;

        case 0:
            pollfd[0].revents = 0;
            continue;
        }

        break;
    }

    rc = !! (pollfd[0].revents & aPollMask);

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

int
ert_waitFdWriteReady(int aFd, const struct Ert_Duration *aTimeout)
{
    return waitFdReady_(aFd, POLLOUT, aTimeout);
}

int
ert_waitFdReadReady(int aFd, const struct Ert_Duration *aTimeout)
{
    return waitFdReady_(aFd, POLLPRI | POLLIN, aTimeout);
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
waitFdReadyDeadline_(
    int aFd, unsigned aPollMask, struct Ert_Deadline *aDeadline)
{
    int rc = -1;

    int ready = -1;

    if ( ! aDeadline)
        ready = waitFdReady_(aFd, aPollMask, 0);
    else
    {
        struct FdReadyDeadline
        {
            int      mFd;
            unsigned mPollMask;

        } readyDeadline = {
            .mFd       = aFd,
            .mPollMask = aPollMask,
        };

        ERT_ERROR_IF(
            (ready = ert_checkDeadlineExpired(
                aDeadline,
                Ert_DeadlinePollMethod(
                    &readyDeadline,
                    ERT_LAMBDA(
                        int, (struct FdReadyDeadline *self_),
                        {
                            return waitFdReady_(
                                self_->mFd,
                                self_->mPollMask,
                                &Ert_ZeroDuration);
                        })),
                Ert_DeadlineWaitMethod(
                    &readyDeadline,
                    ERT_LAMBDA(
                        int, (struct FdReadyDeadline *self_,
                              const struct Ert_Duration  *aTimeout),
                        {
                            return waitFdReady_(
                                self_->mFd,
                                self_->mPollMask,
                                aTimeout);
                        }))),
             -1 == ready));
    }

    rc = ready;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

int
ert_waitFdWriteReadyDeadline(int aFd, struct Ert_Deadline *aDeadline)
{
    return waitFdReadyDeadline_(aFd, POLLOUT, aDeadline);
}

int
ert_waitFdReadReadyDeadline(int aFd, struct Ert_Deadline *aDeadline)
{
    return waitFdReadyDeadline_(aFd, POLLPRI | POLLIN, aDeadline);
}

/* -------------------------------------------------------------------------- */
static ssize_t
readFdDeadline_(int aFd,
                char *aBuf, size_t aLen, struct Ert_Deadline *aDeadline,
                ssize_t aReader(int, void *, size_t))
{
    ssize_t rc = -1;

    char *bufPtr = aBuf;
    char *bufEnd = bufPtr + aLen;

    while (bufPtr != bufEnd)
    {
        if (aDeadline)
        {
            int ready = -1;

            ERT_ERROR_IF(
                (ready = ert_checkDeadlineExpired(
                    aDeadline,
                    Ert_DeadlinePollMethod(
                        &aFd,
                        ERT_LAMBDA(
                            int, (int *fd),
                            {
                                return ert_waitFdReadReady(
                                    *fd, &Ert_ZeroDuration);
                            })),
                    Ert_DeadlineWaitMethod(
                        &aFd,
                        ERT_LAMBDA(
                            int, (int *fd,
                                  const struct Ert_Duration *aTimeout),
                            {
                                return ert_waitFdReadReady(*fd, aTimeout);
                            }))),
                 -1 == ready && bufPtr == aBuf));

            if (-1 == ready)
                break;

            if ( ! ready)
                continue;
        }

        ssize_t len;

        ERT_ERROR_IF(
            (len = read(aFd, bufPtr, bufEnd - bufPtr),
             -1 == len && (EINTR       != errno &&
                           EWOULDBLOCK != errno &&
                           EAGAIN      != errno) && bufPtr == aBuf));

        if ( ! len)
            break;

        if (-1 == len)
        {
            if (EINTR == errno)
                continue;

            if (EWOULDBLOCK == errno || EAGAIN == errno)
            {
                int rdReady;
                ERT_ERROR_IF(
                    (rdReady = ert_waitFdReadReadyDeadline(aFd, aDeadline),
                     -1 == rdReady && bufPtr == aBuf));

                if (0 <= rdReady)
                    continue;
            }

            break;
        }

        bufPtr += len;
    }

    rc = bufPtr - aBuf;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

ssize_t
ert_readFdDeadline(int aFd,
                   char *aBuf, size_t aLen, struct Ert_Deadline *aDeadline)
{
    return readFdDeadline_(aFd, aBuf, aLen, aDeadline, read);
}

/* -------------------------------------------------------------------------- */
static ssize_t
writeFdDeadline_(int aFd,
                 const char *aBuf, size_t aLen, struct Ert_Deadline *aDeadline,
                 ssize_t aWriter(int, const void *, size_t))
{
    ssize_t rc = -1;

    const char *bufPtr = aBuf;
    const char *bufEnd = bufPtr + aLen;

    while (bufPtr != bufEnd)
    {
        if (aDeadline)
        {
            int ready = -1;

            ERT_ERROR_IF(
                (ready = ert_checkDeadlineExpired(
                    aDeadline,
                    Ert_DeadlinePollMethod(
                        &aFd,
                        ERT_LAMBDA(
                            int, (int *fd),
                            {
                                return ert_waitFdWriteReady(
                                    *fd, &Ert_ZeroDuration);
                            })),
                    Ert_DeadlineWaitMethod(
                        &aFd,
                        ERT_LAMBDA(
                            int, (int *fd,
                                  const struct Ert_Duration *aTimeout),
                            {
                                return ert_waitFdWriteReady(*fd, aTimeout);
                            }))),
                 -1 == ready && bufPtr == aBuf));

            if (-1 == ready)
                break;

            if ( ! ready)
                continue;
        }

        ssize_t len;

        ERT_ERROR_IF(
            (len = aWriter(aFd, bufPtr, bufEnd - bufPtr),
             -1 == len && (EINTR       != errno &&
                           EWOULDBLOCK != errno &&
                           EAGAIN      != errno) && bufPtr == aBuf));

        if ( ! len)
            break;

        if (-1 == len)
        {
            if (EINTR == errno)
                continue;

            if (EWOULDBLOCK == errno || EAGAIN == errno)
            {
                int wrReady;
                ERT_ERROR_IF(
                    (wrReady = ert_waitFdWriteReadyDeadline(aFd, aDeadline),
                     -1 == wrReady && bufPtr == aBuf));

                if (0 <= wrReady)
                    continue;
            }

            break;
        }

        bufPtr += len;
    }

    rc = bufPtr - aBuf;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

ssize_t
ert_writeFdDeadline(
    int aFd,
    const char *aBuf, size_t aLen, struct Ert_Deadline *aDeadline)
{
    return writeFdDeadline_(aFd, aBuf, aLen, aDeadline, &write);
}

/* -------------------------------------------------------------------------- */
static ssize_t
readFd_(int aFd,
        char *aBuf, size_t aLen, const struct Ert_Duration *aTimeout,
        ssize_t aReader(int, void *, size_t))
{
    ssize_t rc = -1;

    struct Ert_Deadline  deadline_;
    struct Ert_Deadline *deadline = 0;

    if (aTimeout)
    {
        ERT_ERROR_IF(
            ert_createDeadline(&deadline_, aTimeout));
        deadline = &deadline_;
    }

    rc = readFdDeadline_(aFd, aBuf, aLen, deadline, aReader);

Ert_Finally:

    ERT_FINALLY
    ({
        deadline = ert_closeDeadline(deadline);
    });

    return rc;
}

ssize_t
ert_readFd(int aFd,
           char *aBuf, size_t aLen, const struct Ert_Duration *aTimeout)
{
    return readFd_(aFd, aBuf, aLen, aTimeout, read);
}

ssize_t
ert_readFdRaw(int aFd,
              char *aBuf, size_t aLen, const struct Ert_Duration *aTimeout)
{
    return readFd_(aFd, aBuf, aLen, aTimeout, read_raw);
}

/* -------------------------------------------------------------------------- */
static ssize_t
writeFd_(int aFd,
         const char *aBuf, size_t aLen, const struct Ert_Duration *aTimeout,
         ssize_t aWriter(int, const void *, size_t))
{
    ssize_t rc = -1;

    struct Ert_Deadline  deadline_;
    struct Ert_Deadline *deadline = 0;

    if (aTimeout)
    {
        ERT_ERROR_IF(
            ert_createDeadline(&deadline_, aTimeout));
        deadline = &deadline_;
    }

    rc = writeFdDeadline_(aFd, aBuf, aLen, deadline, aWriter);

Ert_Finally:

    ERT_FINALLY
    ({
        deadline = ert_closeDeadline(deadline);
    });

    return rc;
}

ssize_t
ert_writeFd(int aFd,
            const char *aBuf, size_t aLen, const struct Ert_Duration *aTimeout)
{
    return writeFd_(aFd, aBuf, aLen, aTimeout, write);
}

ssize_t
ert_writeFdRaw(int aFd,
               const char *aBuf, size_t aLen, const struct Ert_Duration *aTimeout)
{
    return writeFd_(aFd, aBuf, aLen, aTimeout, write_raw);
}

/* -------------------------------------------------------------------------- */
ssize_t
ert_readFdFully(int aFd, char **aBuf, size_t aBufSize)
{
    ssize_t rc = -1;

    char   *buf = 0;
    char   *end = buf;
    size_t  len = end - buf;

    while (1)
    {
        size_t avail = len - (end - buf);

        if ( ! avail)
        {
            len =
                len ? 2 * len :
                ert_testMode(Ert_TestLevelRace) ? 1 :
                aBufSize ? aBufSize : ert_fetchSystemPageSize();

            char *ptr;
            ERT_ERROR_UNLESS(
                (ptr = realloc(buf, len)));

            end = ptr + (end - buf);
            buf = ptr;
            continue;
        }

        ssize_t rdlen;
        ERT_ERROR_IF(
            (rdlen = ert_readFd(aFd, end, avail, 0),
             -1 == rdlen));
        if ( ! rdlen)
            break;
        end += rdlen;
    }

    rc = end - buf;

    if ( ! rc)
        *aBuf = 0;
    else
    {
        *aBuf = buf;
        buf   = 0;
    }

Ert_Finally:

    ERT_FINALLY
    ({
        free(buf);
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
off_t
ert_lseekFd(int aFd, off_t aOffset, struct Ert_WhenceType aWhenceType)
{
    off_t rc = -1;

    int ert_whenceType;
    switch (aWhenceType.mType)
    {
    default:
        ert_ensure(0);

    case Ert_WhenceTypeStart_:
        ert_whenceType = SEEK_SET;
        break;

    case Ert_WhenceTypeHere_:
        ert_whenceType = SEEK_CUR;
        break;

    case Ert_WhenceTypeEnd_:
        ert_whenceType = SEEK_END;
        break;
    }

    rc = lseek(aFd, aOffset, ert_whenceType);

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_lockFd(int aFd, struct Ert_LockType aLockType)
{
    int rc = -1;

    int ert_lockType;
    switch (aLockType.mType)
    {
    default:
        ert_ensure(0);

    case Ert_LockTypeWrite_:
        ert_lockType = LOCK_EX;
        break;

    case Ert_LockTypeRead_:
        ert_lockType = LOCK_SH;
        break;
    }

    int err;
    do
        ERT_ERROR_IF(
            (err = flock(aFd, ert_lockType),
             err && EINTR != errno));
    while (err);

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_unlockFd(int aFd)
{
    int rc = -1;

    ERT_ERROR_IF(
        flock(aFd, LOCK_UN));

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_lockFdRegion(
    int aFd, struct Ert_LockType aLockType, off_t aPos, off_t aLen)
{
    int rc = -1;

    int ert_lockType;
    switch (aLockType.mType)
    {
    default:
        ert_ensure(0);

    case Ert_LockTypeWrite_:
        ert_lockType = F_WRLCK;
        break;

    case Ert_LockTypeRead_:
        ert_lockType = F_RDLCK;
        break;
    }

    struct flock lockRegion =
    {
        .l_type   = ert_lockType,
        .l_whence = SEEK_SET,
        .l_start  = aPos,
        .l_len    = aLen,
        .l_pid    = getpid(),
    };

    int err;
    do
        ERT_ERROR_IF(
            (err = fcntl(aFd, F_SETLKW, &lockRegion),
             -1 == err && EINTR != errno));
    while (err);

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_unlockFdRegion(int aFd, off_t aPos, off_t aLen)
{
    int rc = -1;

    struct flock lockRegion =
    {
        .l_type   = F_UNLCK,
        .l_whence = SEEK_SET,
        .l_start  = aPos,
        .l_len    = aLen,
    };

    ERT_ERROR_IF(
        fcntl(aFd, F_SETLK, &lockRegion));

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
struct Ert_LockType
ert_ownFdRegionLocked(int aFd, off_t aPos, off_t aLen)
{
    int rc = -1;

    struct Ert_LockType ert_lockType = Ert_LockTypeUnlocked;

    struct flock lockRegion =
    {
        .l_type   = F_WRLCK,
        .l_whence = SEEK_SET,
        .l_start  = aPos,
        .l_len    = aLen,
    };

    ERT_ERROR_IF(
        fcntl(aFd, F_GETLK, &lockRegion));

    switch (lockRegion.l_type)
    {
    default:
        ERT_ERROR_IF(
            true,
            {
                errno = EIO;
            });

    case F_UNLCK:
        break;

    case F_RDLCK:
        if (lockRegion.l_pid != ert_ownProcessId().mPid)
            ert_lockType = Ert_LockTypeRead;
        break;

    case F_WRLCK:
        if (lockRegion.l_pid != ert_ownProcessId().mPid)
            ert_lockType = Ert_LockTypeWrite;
        break;
    }

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc ? Ert_LockTypeError : ert_lockType;
}

/* -------------------------------------------------------------------------- */
