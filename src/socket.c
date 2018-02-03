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

#include "ert/socket.h"
#include "ert/process.h"

#include <fcntl.h>


/* -------------------------------------------------------------------------- */
struct Ert_Socket *
ert_closeSocket(
    struct Ert_Socket *self)
{
    if (self)
        self->mFile = ert_closeFile(self->mFile);

    return 0;
}

/* -------------------------------------------------------------------------- */
int
ert_createSocket(
    struct Ert_Socket *self,
    int                aFd)
{
    int rc = -1;

    self->mFile = 0;

    ERT_ERROR_IF(
        ert_createFile(&self->mFile_, aFd));
    self->mFile = &self->mFile_;

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
bool
ert_ownSocketValid(
    const struct Ert_Socket *self)
{
    return ert_ownFileValid(self->mFile);
}

/* -------------------------------------------------------------------------- */
ssize_t
ert_writeSocket(
    struct Ert_Socket *self,
    const char *aBuf, size_t aLen, const struct Ert_Duration *aTimeout)
{
    return ert_writeFile(self->mFile, aBuf, aLen, aTimeout);
}

/* -------------------------------------------------------------------------- */
ssize_t
ert_readSocket(
    struct Ert_Socket *self,
    char *aBuf, size_t aLen, const struct Ert_Duration *aTimeout)
{
    return ert_readFile(self->mFile, aBuf, aLen, aTimeout);
}

/* -------------------------------------------------------------------------- */
ssize_t
ert_writeSocketDeadline(
    struct Ert_Socket *self,
    const char *aBuf, size_t aLen, struct Ert_Deadline *aDeadline)
{
    return ert_writeFileDeadline(self->mFile, aBuf, aLen, aDeadline);
}

/* -------------------------------------------------------------------------- */
ssize_t
ert_readSocketDeadline(
    struct Ert_Socket *self,
    char *aBuf, size_t aLen, struct Ert_Deadline *aDeadline)
{
    return ert_readFileDeadline(self->mFile, aBuf, aLen, aDeadline);
}

/* -------------------------------------------------------------------------- */
int
ert_bindSocket(
    struct Ert_Socket *self,
    struct sockaddr *aAddr, size_t aAddrLen)
{
    return bind(self->mFile->mFd, aAddr, aAddrLen);
}

/* -------------------------------------------------------------------------- */
int
ert_connectSocket(
    struct Ert_Socket *self,
    struct sockaddr *aAddr, size_t aAddrLen)
{
    int rc;

    do
        rc = connect(self->mFile->mFd, aAddr, aAddrLen);
    while (rc && EINTR == errno);

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_acceptSocket(
    struct Ert_Socket *self,
    unsigned           aFlags)
{
    int rc    = -1;
    int fd    = -1;
    int flags = 0;

    switch (aFlags)
    {
    default:
        ERT_ERROR_IF(
            true,
            {
                errno = EINVAL;
            });

    case 0:                                                            break;
    case O_NONBLOCK | O_CLOEXEC: flags = SOCK_NONBLOCK | SOCK_CLOEXEC; break;
    case O_NONBLOCK:             flags = SOCK_NONBLOCK;                break;
    case O_CLOEXEC:              flags = SOCK_CLOEXEC;                 break;
    }

    while (1)
    {
        ERT_ERROR_IF(
            (fd = accept4(self->mFile->mFd, 0, 0, flags),
             -1 == fd && EINTR != errno));
        if (-1 != fd)
            break;
    }

    rc = fd;

Ert_Finally:

    ERT_FINALLY
    ({
        if (-1 == rc)
            fd = ert_closeFd(fd);
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_listenSocket(
    struct Ert_Socket *self,
    unsigned           aQueueLen)
{
    return listen(self->mFile->mFd, aQueueLen ? aQueueLen : 1);
}

/* -------------------------------------------------------------------------- */
int
ert_waitSocketWriteReady(
    const struct Ert_Socket   *self,
    const struct Ert_Duration *aTimeout)
{
    return ert_waitFileWriteReady(self->mFile, aTimeout);
}

/* -------------------------------------------------------------------------- */
int
ert_waitSocketReadReady(
    const struct Ert_Socket   *self,
    const struct Ert_Duration *aTimeout)
{
    return ert_waitFileReadReady(self->mFile, aTimeout);
}

/* -------------------------------------------------------------------------- */
int
ert_ownSocketError(
    const struct Ert_Socket *self,
    int                     *aError)
{
    int rc = -1;

    socklen_t len = sizeof(*aError);

    ERT_ERROR_IF(
        getsockopt(self->mFile->mFd, SOL_SOCKET, SO_ERROR, aError, &len));

    ERT_ERROR_IF(
        sizeof(*aError) != len,
        {
            errno = EINVAL;
        });

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_ownSocketPeerCred(
    const struct Ert_Socket *self,
    struct ucred            *aCred)
{
    int rc = -1;

    socklen_t len = sizeof(*aCred);

    ERT_ERROR_IF(
        getsockopt(self->mFile->mFd, SOL_SOCKET, SO_PEERCRED, aCred, &len));

    ERT_ERROR_IF(
        sizeof(*aCred) != len,
        {
            errno = EINVAL;
        });

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_ownSocketName(
    const struct Ert_Socket *self,
    struct sockaddr *aAddr, socklen_t *aAddrLen)
{
    return getsockname(self->mFile->mFd, aAddr, aAddrLen);
}

/* -------------------------------------------------------------------------- */
int
ert_ownSocketPeerName(
    const struct Ert_Socket *self,
    struct sockaddr *aAddr, socklen_t *aAddrLen)
{
    return getpeername(self->mFile->mFd, aAddr, aAddrLen);
}

/* -------------------------------------------------------------------------- */
ssize_t
ert_sendSocket(
    struct Ert_Socket *self,
    const char *aBuf, size_t aLen)
{
    ssize_t rc;

    do
        rc = send(self->mFile->mFd, aBuf, aLen, 0);
    while (-1 == rc && EINTR == errno);

    return rc;
}

/* -------------------------------------------------------------------------- */
ssize_t
ert_recvSocket(
    struct Ert_Socket *self,
    char *aBuf, size_t aLen)
{
    ssize_t rc;

    do
        rc = recv(self->mFile->mFd, aBuf, aLen, 0);
    while (-1 == rc && EINTR == errno);

    return rc;
}

/* -------------------------------------------------------------------------- */
ssize_t
ert_sendSocketMsg(
    struct Ert_Socket *self,
    const struct msghdr *aMsg, int aFlags)
{
    ssize_t rc;

    do
        rc = sendmsg(self->mFile->mFd, aMsg, aFlags);
    while (-1 == rc && EINTR == errno);

    return rc;
}

/* -------------------------------------------------------------------------- */
ssize_t
ert_recvSocketMsg(
    struct Ert_Socket *self,
    struct msghdr *aMsg, int aFlags)
{
    ssize_t rc;

    do
        rc = recvmsg(self->mFile->mFd, aMsg, aFlags);
    while (-1 == rc && EINTR == errno);

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_shutdownSocketReader(
    struct Ert_Socket *self)
{
    return shutdown(self->mFile->mFd, SHUT_RD);
}

/* -------------------------------------------------------------------------- */
int
ert_shutdownSocketWriter(
    struct Ert_Socket *self)
{
    return shutdown(self->mFile->mFd, SHUT_WR);
}

/* -------------------------------------------------------------------------- */
