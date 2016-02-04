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

#include "fd_.h"
#include "macros_.h"
#include "error_.h"
#include "test_.h"

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/file.h>
#include <sys/resource.h>

#include <valgrind/valgrind.h>

/* -------------------------------------------------------------------------- */
#define DEVNULLPATH "/dev/null"

static const char sDevNullPath[] = DEVNULLPATH;

/* -------------------------------------------------------------------------- */
int
closeFd(int *aFd)
{
    int rc = -1;

    if (-1 != *aFd)
    {
        if (close(*aFd))
            goto Finally;
        *aFd = -1;
    }

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
static int
rankFd_(const void *aLhs, const void *aRhs)
{
    int lhs = * (const int *) aLhs;
    int rhs = * (const int *) aRhs;

    if (lhs < rhs) return -1;
    if (lhs > rhs) return +1;
    return 0;
}

int
closeFdDescriptors(const int *aWhiteList, size_t aWhiteListLen)
{
    int rc = -1;

    if (aWhiteListLen)
    {
        struct rlimit noFile;

        if (getrlimit(RLIMIT_NOFILE, &noFile))
            goto Finally;

        int whiteList[aWhiteListLen + 1];

        whiteList[aWhiteListLen] = noFile.rlim_cur;

        for (size_t ix = 0; aWhiteListLen > ix; ++ix)
        {
            whiteList[ix] = aWhiteList[ix];
            ensure(whiteList[aWhiteListLen] > whiteList[ix]);
        }

        debug(0, "purging %zd fds", aWhiteListLen);

        qsort(
            whiteList,
            NUMBEROF(whiteList), sizeof(whiteList[0]), rankFd_);

        for (int fd = 0, wx = 0; NUMBEROF(whiteList) > wx; ++fd)
        {
            if (0 > whiteList[wx])
            {
                ++wx;
                continue;
            }

            if (fd != whiteList[wx])
            {
                int closedFd = fd;

                if (closeFd(&closedFd) && EBADF != errno)
                    goto Finally;
            }
            else
            {
                debug(0, "not closing fd %d", fd);

                while (NUMBEROF(whiteList) > ++wx)
                    if (whiteList[wx] != fd)
                        break;
            }
        }
    }

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
bool
stdFd(int aFd)
{
    return STDIN_FILENO == aFd || STDOUT_FILENO == aFd || STDERR_FILENO == aFd;
}

/* -------------------------------------------------------------------------- */
int
closeFdOnExec(int aFd, unsigned aCloseOnExec)
{
    int rc = -1;

    unsigned closeOnExec = 0;

    if (aCloseOnExec)
    {
        if (aCloseOnExec != O_CLOEXEC)
        {
            errno = EINVAL;
            goto Finally;
        }

        /* Take care. O_CLOEXEC is the flag for obtaining close-on-exec
         * semantics when using open(), but fcntl() requires FD_CLOEXEC. */

        closeOnExec = FD_CLOEXEC;
    }

    long flags = fcntl(aFd, F_GETFD);

    if (-1 == flags)
        goto Finally;

    rc = fcntl(aFd, F_SETFD, (flags & ~FD_CLOEXEC) | closeOnExec);

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
nullifyFd(int aFd)
{
    int rc = -1;
    int fd = open(sDevNullPath, O_WRONLY);

    if (-1 == fd)
        goto Finally;

    if (fd == aFd)
        fd = -1;
    else
    {
        if (aFd != dup2(fd, aFd))
            goto Finally;
    }

    rc = 0;

Finally:

    FINALLY
    ({
        closeFd(&fd);
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
int
nonblockingFd(int aFd)
{
    int rc = -1;

    long statusFlags = fcntl(aFd, F_GETFL);
    int  descriptorFlags = fcntl(aFd, F_GETFD);

    if (-1 == statusFlags || -1 == descriptorFlags)
        goto Finally;

    /* Because O_NONBLOCK affects the underlying open file, to get some
     * peace of mind, only allow non-blocking mode on file descriptors
     * that are not going to be shared. This is not a water-tight
     * defense, but seeks to prevent some careless mistakes. */

    if ( ! (descriptorFlags & FD_CLOEXEC))
    {
        errno = EBADF;
        goto Finally;
    }

    rc = (statusFlags & O_NONBLOCK) ? 0 : fcntl(aFd, F_SETFL,
                                                statusFlags | O_NONBLOCK);

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ownFdNonBlocking(int aFd)
{
    int rc = -1;

    int flags = fcntl(aFd, F_GETFL);

    if (-1 == flags)
        goto Finally;

    rc = flags & O_NONBLOCK ? 1 : 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ownFdValid(int aFd)
{
    int rc = -1;

    int valid = 1;

    if (-1 == fcntl(aFd, F_GETFL))
    {
        if (EBADF != errno)
            goto Finally;
        valid = 0;
    }

    rc = valid;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
ssize_t
spliceFd(int aSrcFd, int aDstFd, size_t aLen, unsigned aFlags)
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
            bytes = read(aSrcFd, buffer, len);
        while (-1 == bytes && EINTR == errno);

        if (-1 == bytes)
            goto Finally;

        if (bytes)
        {
            char *bufptr = buffer;
            char *bufend = bufptr + bytes;

            while (bufptr != bufend)
            {
                ssize_t wrote;

                do
                    wrote = write(aDstFd, bufptr, bufend - bufptr);
                while (-1 == wrote  && EINTR == errno);

                if (-1 == wrote)
                    goto Finally;

                bufptr += wrote;
            }
        }

        rc = bytes;
    }

Finally:

    return rc;
}

/* -------------------------------------------------------------------------- */
ssize_t
readFd(int aFd, char *aBuf, size_t aLen)
{
    ssize_t rc = -1;

    char *bufPtr = aBuf;
    char *bufEnd = bufPtr + aLen;

    while (bufPtr != bufEnd)
    {
        ssize_t len;

        len = read(aFd, bufPtr, bufEnd - bufPtr);

        if ( ! len)
            break;

        if (-1 == len)
        {
            if (EINTR == errno)
                continue;

            if (bufPtr != aBuf)
                break;

            goto Finally;
        }

        bufPtr += len;
    }

    rc = bufPtr - aBuf;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
ssize_t
writeFd(int aFd, const char *aBuf, size_t aLen)
{
    ssize_t rc = -1;

    const char *bufPtr = aBuf;
    const char *bufEnd = bufPtr + aLen;

    while (bufPtr != bufEnd)
    {
        ssize_t len;

        len = write(aFd, bufPtr, bufEnd - bufPtr);

        if ( ! len)
            break;

        if (-1 == len)
        {
            if (EINTR == errno)
                continue;

            if (bufPtr != aBuf)
                break;

            goto Finally;
        }

        bufPtr += len;
    }

    rc = bufPtr - aBuf;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
ssize_t
readFdFully(int aFd, char **aBuf, size_t aBufSize)
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
                testMode() ? 1 :
                aBufSize ? aBufSize : getpagesize();

            char *ptr = realloc(buf, len);
            if ( ! ptr)
                goto Finally;

            end = ptr + (end - buf);
            buf = ptr;
            continue;
        }

        ssize_t rdlen = readFd(aFd, end, avail);
        if (-1 == rdlen)
            goto Finally;
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

Finally:

    FINALLY
    ({
        free(buf);
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
int
lockFd(int aFd, int aType)
{
    int rc = -1;

    if (LOCK_EX != aType && LOCK_SH != aType)
    {
        errno = EINVAL;
        goto Finally;
    }

    if (flock(aFd, aType))
        goto Finally;

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
unlockFd(int aFd)
{
    int rc = -1;

    if (flock(aFd, LOCK_UN))
        goto Finally;

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
