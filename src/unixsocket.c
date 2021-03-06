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

#include "ert/unixsocket.h"
#include "ert/timekeeping.h"
#include "ert/process.h"


#include <sys/file.h>
#include <sys/un.h>

/* -------------------------------------------------------------------------- */
static void
createRandomName(
    struct sockaddr_un *aSockAddr,
    uint32_t           *aRandom)
{
    static const char hexDigits[16] = "0123456789abcdef";

    char *bp = aSockAddr->sun_path;
    char *ep = bp + sizeof(aSockAddr->sun_path);

    *bp++ = 0;

    static const char check[sizeof(aSockAddr->sun_path) > 40 ? 1 : 0]
        __attribute__((__unused__));

    for (unsigned ix = 0; 10 > ix; ++ix)
    {
        /* LCG(2^32, 69069, 0, 1)
         * http://mathforum.org/kb/message.jspa?messageID=1608043 */

        *aRandom = *aRandom * 69069 + 1;

        uint32_t rnd = *aRandom;

        bp[0] = hexDigits[rnd % sizeof(hexDigits)]; rnd >>= 8;
        bp[1] = hexDigits[rnd % sizeof(hexDigits)]; rnd >>= 8;
        bp[2] = hexDigits[rnd % sizeof(hexDigits)]; rnd >>= 8;
        bp[3] = hexDigits[rnd % sizeof(hexDigits)]; rnd >>= 8;

        bp += 4;
    }

    memset(bp, 0, ep-bp);
}

int
ert_createUnixSocket(
    struct Ert_UnixSocket *self,
    const char            *aName,
    size_t                 aNameLen,
    unsigned               aQueueLen)
{
    int rc = -1;

    self->mSocket = 0;

    ERT_ERROR_IF(
        ert_createSocket(&self->mSocket_,
                     socket(AF_UNIX,
                            SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0)));
    self->mSocket = &self->mSocket_;

    /* Do not use random() from stdlib to avoid perturbing the behaviour of
     * programs that themselves use the PRNG from the library. */

    uint32_t rnd =
        aNameLen
        ? aNameLen
        : ert_ownProcessId().mPid ^ ERT_MSECS(ert_monotonicTime().monotonic).ms;

    while (1)
    {
        struct sockaddr_un sockAddr;

        if ( ! aName)
        {
            createRandomName(&sockAddr, &rnd);
            sockAddr.sun_family = AF_UNIX;
            sockAddr.sun_path[0] = 0;
        }
        else
        {
            sockAddr.sun_family = AF_UNIX;
            memset(sockAddr.sun_path, 0, sizeof(sockAddr.sun_path));

            if ( ! aNameLen)
                strncpy(sockAddr.sun_path, aName, sizeof(sockAddr.sun_path));
            else
            {
                ERT_ERROR_IF(
                    sizeof(sockAddr.sun_path) < aNameLen,
                    {
                        errno = EINVAL;
                    });
                memcpy(sockAddr.sun_path, aName, aNameLen);
            }
        }

        /* Only perform an automatic retry if requested. This is
         * primarily to allow the unit test to verify correct
         * operation of the retry and name generation code. */

        int err;
        ERT_ERROR_IF(
            (err = ert_bindSocket(
                self->mSocket,
                (struct sockaddr *) &sockAddr, sizeof(sockAddr)),
             err && (EADDRINUSE != errno || aName || aNameLen)));

        if (err)
            continue;

        ERT_ERROR_IF(
            ert_listenSocket(self->mSocket, aQueueLen));

        break;
    }

    rc = 0;

Ert_Finally:

    ERT_FINALLY(
    {
        if (rc)
            self->mSocket = ert_closeSocket(self->mSocket);
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_acceptUnixSocket(
    struct Ert_UnixSocket       *self,
    const struct Ert_UnixSocket *aServer)
{
    int rc = -1;

    self->mSocket = 0;

    while (1)
    {
        int err;
        ERT_ERROR_IF(
            (err = ert_createSocket(
                &self->mSocket_,
                ert_acceptSocket(aServer->mSocket, O_NONBLOCK | O_CLOEXEC)),
             err && EINTR != errno));

        if ( ! err)
            break;
    }
    self->mSocket = &self->mSocket_;

    rc = 0;

Ert_Finally:

    ERT_FINALLY(
    {
        if (rc)
            self->mSocket = ert_closeSocket(self->mSocket);
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_connectUnixSocket(
    struct Ert_UnixSocket *self,
    const char *aName, size_t aNameLen)
{
    int rc = -1;
    int err = 0;

    self->mSocket = 0;

    struct sockaddr_un sockAddr;

    if ( ! aNameLen)
        aNameLen = strlen(aName);
    ERT_ERROR_UNLESS(
        aNameLen,
        {
            errno = EINVAL;
        });
    ERT_ERROR_IF(
        sizeof(sockAddr.sun_path) < aNameLen);

    sockAddr.sun_family = AF_UNIX;
    memcpy(sockAddr.sun_path, aName, aNameLen);
    memset(
        sockAddr.sun_path + aNameLen, 0, sizeof(sockAddr.sun_path) - aNameLen);

    ERT_ERROR_IF(
        ert_createSocket(
            &self->mSocket_,
            socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0)));
    self->mSocket = &self->mSocket_;

    while (1)
    {
        ERT_ERROR_IF(
            (err = ert_connectSocket(
                self->mSocket,
                (struct sockaddr *) &sockAddr, sizeof(sockAddr)),
             err && EINTR != errno && EINPROGRESS != errno));

        if (err)
        {
            if (EINTR == errno)
                continue;

            err = EINPROGRESS;
        }

        break;
    }

    rc = 0;

Ert_Finally:

    ERT_FINALLY(
    {
        if (rc)
            self->mSocket = ert_closeSocket(self->mSocket);
    });

    return rc ? rc : err ? err : 0;
}

/* -------------------------------------------------------------------------- */
struct Ert_UnixSocket *
ert_closeUnixSocket(
    struct Ert_UnixSocket *self)
{
    if (self)
    {
        self->mSocket = ert_closeSocket(self->mSocket);
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
int
ert_createUnixSocketPair(
    struct Ert_UnixSocket *aParent,
    struct Ert_UnixSocket *aChild,
    unsigned               aFlags)
{
    int rc = -1;

    int fd[2] = { -1, -1 };

    aParent->mSocket = 0;
    aChild->mSocket  = 0;

    ERT_ERROR_IF(
        aFlags & ~ (O_CLOEXEC | O_NONBLOCK),
        {
            errno = EINVAL;
        });

    int sockFlags = 0;

    if (aFlags & O_NONBLOCK)
        sockFlags |= SOCK_NONBLOCK;

    if (aFlags & O_CLOEXEC)
        sockFlags |= SOCK_CLOEXEC;

    ERT_ERROR_IF(
        socketpair(AF_UNIX, SOCK_STREAM | sockFlags, 0, fd),
        {
            fd[0] = -1;
            fd[1] = -1;
        });

    ERT_ERROR_IF(
        -1 == fd[0] || -1 == fd[1]);

    ERT_ERROR_IF(
        ert_createSocket(&aParent->mSocket_, fd[0]));
    aParent->mSocket = &aParent->mSocket_;

    fd[0] = -1;

    ERT_ERROR_IF(
        ert_createSocket(&aChild->mSocket_, fd[1]));
    aChild->mSocket = &aChild->mSocket_;

    fd[1] = -1;

    rc = 0;

Ert_Finally:

    ERT_FINALLY
    ({
        fd[0] = ert_closeFd(fd[0]);
        fd[1] = ert_closeFd(fd[1]);

        if (rc)
        {
            aParent->mSocket = ert_closeSocket(aParent->mSocket);
            aChild->mSocket  = ert_closeSocket(aChild->mSocket);
        }
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_sendUnixSocketFd(
    struct Ert_UnixSocket *self,
    int                    aFd)
{
    int rc = -1;

    struct msghdr msg;

    char cmsgbuf[CMSG_SPACE(sizeof aFd)];

    char buf[1] = { 0 };

    struct iovec iov[1];

    iov[0].iov_base = buf;
    iov[0].iov_len  = sizeof(buf);

    msg.msg_name       = 0;
    msg.msg_namelen    = 0;
    msg.msg_iov        = iov;
    msg.msg_iovlen     = ERT_NUMBEROF(iov);
    msg.msg_flags      = 0;
    msg.msg_control    = cmsgbuf;
    msg.msg_controllen = CMSG_LEN(sizeof(aFd));

    memset(cmsgbuf, 0, sizeof(cmsgbuf));
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);

    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type  = SCM_RIGHTS;
    cmsg->cmsg_len   = CMSG_LEN(sizeof(aFd));

    ert_ensure(msg.msg_controllen >= cmsg->cmsg_len);

    int *bufPtr = (void *) CMSG_DATA(cmsg);

    bufPtr[0] = aFd;

    ERT_ERROR_IF(
        sizeof(buf) != ert_sendSocketMsg(self->mSocket, &msg, 0));

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
ert_recvUnixSocketFd_(
    struct Ert_UnixSocket *self,
    struct msghdr         *aMsg)
{
    int rc = -1;

    int    *fdPtr = 0;
    size_t  fdLen = 0;

    int fd = -1;

    for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(aMsg);
         cmsg;
         cmsg = CMSG_NXTHDR(aMsg, cmsg))
    {
        if (SOL_SOCKET == cmsg->cmsg_level &&
            SCM_RIGHTS == cmsg->cmsg_type &&
            CMSG_LEN(sizeof(fd)) == cmsg->cmsg_len)
        {
            ++fdLen;
        }
    }

    int fdBuf_[fdLen ? fdLen : 1];

    if (fdLen)
    {
        size_t ix;

        for (ix = 0; fdLen > ix; ++ix)
            fdBuf_[ix] = -1;
        fdPtr = fdBuf_;

        ix = 0;
        for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(aMsg);
             cmsg;
             cmsg = CMSG_NXTHDR(aMsg, cmsg))
        {
            if (SOL_SOCKET == cmsg->cmsg_level &&
                SCM_RIGHTS == cmsg->cmsg_type &&
                CMSG_LEN(sizeof(fd)) == cmsg->cmsg_len)
            {
                const int *bufPtr = (const void *) CMSG_DATA(cmsg);

                fdPtr[ix++] = *bufPtr;
            }
        }

        ert_ensure(fdLen == ix);
    }

    for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(aMsg);
         cmsg;
         cmsg = CMSG_NXTHDR(aMsg, cmsg))
    {
        ERT_ERROR_IF(
            cmsg->cmsg_level != SOL_SOCKET ||
            cmsg->cmsg_type  != SCM_RIGHTS ||
            CMSG_LEN(sizeof(fd)) != cmsg->cmsg_len);
    }

    ERT_ERROR_UNLESS(
        1 == fdLen && 0 <= fdPtr[0]);

    fd       = fdPtr[0];
    fdPtr[0] = -1;

    rc = fd;

Ert_Finally:

    ERT_FINALLY
    ({
        if (-1 != rc)
        {
            if (fdPtr)
            {
                for (size_t ix = 0; fdLen > ix; ++ix)
                    fdPtr[ix] = ert_closeFd(fdPtr[ix]);
            }
        }
    });

    return rc;
}

int
ert_recvUnixSocketFd(
    struct Ert_UnixSocket *self,
    unsigned               aFlags)
{
    int rc = -1;
    int fd = -1;

    char cmsgbuf[CMSG_SPACE(sizeof(fd))];

    ERT_ERROR_IF(
        aFlags & ~ O_CLOEXEC);

    unsigned flags = 0;

    if (aFlags & O_CLOEXEC)
        flags |= MSG_CMSG_CLOEXEC;

    char buf[1];

    struct iovec iov[1];

    iov[0].iov_base = buf;
    iov[0].iov_len  = sizeof(buf);

    struct msghdr msg;

    msg.msg_name    = 0;
    msg.msg_namelen = 0;
    msg.msg_iov     = iov;
    msg.msg_iovlen  = ERT_NUMBEROF(iov);
    msg.msg_flags   = 0;

    msg.msg_control    = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);

    ssize_t rdlen;
    ERT_ERROR_IF(
        (rdlen = ert_recvSocketMsg(self->mSocket, &msg, flags),
         -1 == rdlen || (errno = EIO, sizeof(buf) != rdlen) || buf[0]));

    ERT_ERROR_UNLESS(
        msg.msg_controllen);

    ERT_ERROR_IF(
        (fd = ert_recvUnixSocketFd_(self, &msg),
         -1 == fd));

    ERT_ERROR_IF(
        MSG_CTRUNC & msg.msg_flags,
        {
            errno = EIO;
        });

    rc = fd;
    fd = -1;

Ert_Finally:

    ERT_FINALLY
    ({
        fd = ert_closeFd(fd);
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
bool
ert_ownUnixSocketValid(
    const struct Ert_UnixSocket *self)
{
    return ert_ownSocketValid(self->mSocket);
}

/* -------------------------------------------------------------------------- */
int
ert_shutdownUnixSocketReader(
    struct Ert_UnixSocket *self)
{
    return ert_shutdownSocketReader(self->mSocket);
}

/* -------------------------------------------------------------------------- */
int
ert_shutdownUnixSocketWriter(
    struct Ert_UnixSocket *self)
{
    return ert_shutdownSocketWriter(self->mSocket);
}

/* -------------------------------------------------------------------------- */
int
ert_waitUnixSocketWriteReady(
    const struct Ert_UnixSocket *self,
    const struct Ert_Duration   *aTimeout)
{
    return ert_waitSocketWriteReady(self->mSocket, aTimeout);
}

/* -------------------------------------------------------------------------- */
int
ert_waitUnixSocketReadReady(
    const struct Ert_UnixSocket *self,
    const struct Ert_Duration   *aTimeout)
{
    return ert_waitSocketReadReady(self->mSocket, aTimeout);
}

/* -------------------------------------------------------------------------- */
ssize_t
ert_sendUnixSocket(
    struct Ert_UnixSocket *self,
    const char *aBuf, size_t aLen)
{
    return ert_sendSocket(self->mSocket, aBuf, aLen);
}

/* -------------------------------------------------------------------------- */
ssize_t
ert_recvUnixSocket(struct Ert_UnixSocket *self, char *aBuf, size_t aLen)
{
    return ert_recvSocket(self->mSocket, aBuf, aLen);
}

/* -------------------------------------------------------------------------- */
ssize_t
ert_writeUnixSocket(
    struct Ert_UnixSocket *self,
    const char *aBuf, size_t aLen, const struct Ert_Duration *aTimeout)
{
    return ert_writeSocket(self->mSocket, aBuf, aLen, aTimeout);
}

/* -------------------------------------------------------------------------- */
ssize_t
ert_readUnixSocket(
    struct Ert_UnixSocket *self,
    char *aBuf, size_t aLen, const struct Ert_Duration *aTimeout)
{
    return ert_readSocket(self->mSocket, aBuf, aLen, aTimeout);
}

/* -------------------------------------------------------------------------- */
int
ert_ownUnixSocketName(const struct Ert_UnixSocket *self,
                  struct sockaddr_un *aAddr)
{
    socklen_t addrLen = sizeof(*aAddr);

    return ert_ownSocketName(
        self->mSocket, (struct sockaddr *) aAddr, &addrLen);
}

/* -------------------------------------------------------------------------- */
int
ert_ownUnixSocketPeerName(
    const struct Ert_UnixSocket *self,
    struct sockaddr_un          *aAddr)
{
    socklen_t addrLen = sizeof(*aAddr);

    return ert_ownSocketPeerName(
        self->mSocket, (struct sockaddr *) aAddr, &addrLen);
}

/* -------------------------------------------------------------------------- */
int
ert_ownUnixSocketError(
    const struct Ert_UnixSocket *self,
    int                         *aError)
{
    return ert_ownSocketError(self->mSocket, aError);
}

/* -------------------------------------------------------------------------- */
int
ert_ownUnixSocketPeerCred(
    const struct Ert_UnixSocket *self,
    struct ucred                *aCred)
{
    return ert_ownSocketPeerCred(self->mSocket, aCred);
}

/* -------------------------------------------------------------------------- */
