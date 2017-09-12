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
#ifndef ERT_UNIXSOCKET_H
#define ERT_UNIXSOCKET_H

#include "ert/compiler.h"
#include "ert/socket.h"

ERT_BEGIN_C_SCOPE;

struct Ert_Duration;

struct sockaddr_un;
struct ucred;

struct Ert_UnixSocket
{
    struct Ert_Socket  mSocket_;
    struct Ert_Socket *mSocket;
};

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
ert_createUnixSocketPair(struct Ert_UnixSocket *aParent,
                     struct Ert_UnixSocket *aChild,
                     unsigned           aFlags);

ERT_CHECKED int
ert_createUnixSocket(struct Ert_UnixSocket *self,
                 const char        *aName,
                 size_t             aNameLen,
                 unsigned           aQueueLen);

ERT_CHECKED int
ert_acceptUnixSocket(struct Ert_UnixSocket       *self,
                 const struct Ert_UnixSocket *aServer);

ERT_CHECKED int
ert_connectUnixSocket(struct Ert_UnixSocket *self,
                 const char         *aName,
                 size_t              aNameLen);

ERT_CHECKED struct Ert_UnixSocket *
ert_closeUnixSocket(struct Ert_UnixSocket *self);

ERT_CHECKED int
ert_sendUnixSocketFd(struct Ert_UnixSocket *self, int aFd);

ERT_CHECKED int
ert_recvUnixSocketFd(struct Ert_UnixSocket *self, unsigned aFlags);

bool
ert_ownUnixSocketValid(const struct Ert_UnixSocket *self);

ERT_CHECKED int
ert_shutdownUnixSocketReader(struct Ert_UnixSocket *self);

ERT_CHECKED int
ert_shutdownUnixSocketWriter(struct Ert_UnixSocket *self);

ERT_CHECKED int
ert_waitUnixSocketWriteReady(const struct Ert_UnixSocket *self,
                         const struct Ert_Duration   *aTimeout);

ERT_CHECKED int
ert_waitUnixSocketReadReady(const struct Ert_UnixSocket *self,
                        const struct Ert_Duration   *aTimeout);

ERT_CHECKED int
ert_ownUnixSocketPeerName(const struct Ert_UnixSocket *self,
                      struct sockaddr_un      *aAddr);

ERT_CHECKED int
ert_ownUnixSocketName(const struct Ert_UnixSocket *self,
                  struct sockaddr_un      *aAddr);

ERT_CHECKED int
ert_ownUnixSocketError(const struct Ert_UnixSocket *self, int *aError);

ERT_CHECKED int
ert_ownUnixSocketPeerCred(
    const struct Ert_UnixSocket *self, struct ucred *aCred);

ERT_CHECKED ssize_t
ert_sendUnixSocket(struct Ert_UnixSocket *self, const char *aBuf, size_t aLen);

ERT_CHECKED ssize_t
ert_recvUnixSocket(struct Ert_UnixSocket *self, char *aBuf, size_t aLen);

ERT_CHECKED ssize_t
ert_writeUnixSocket(
    struct Ert_UnixSocket *self,
    const char *aBuf, size_t aLen, const struct Ert_Duration *aTimeout);

ERT_CHECKED ssize_t
ert_readUnixSocket(
    struct Ert_UnixSocket *self,
    char *aBuf, size_t aLen, const struct Ert_Duration *aTimeout);

/* -------------------------------------------------------------------------- */

ERT_END_C_SCOPE;

#endif /* ERT_UNIXSOCKET_H */
