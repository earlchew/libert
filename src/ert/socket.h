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
#ifndef ERT_SOCKET_H
#define ERT_SOCKET_H

#include "ert/compiler.h"
#include "ert/file.h"

#include <sys/socket.h>

ERT_BEGIN_C_SCOPE;

struct Ert_Socket
{
    struct Ert_File  mFile_;
    struct Ert_File *mFile;
};

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
ert_createSocket(struct Ert_Socket *self, int aFd);

ERT_CHECKED struct Ert_Socket *
ert_closeSocket(struct Ert_Socket *self);

bool
ert_ownSocketValid(const struct Ert_Socket *self);

/* -------------------------------------------------------------------------- */
ERT_CHECKED ssize_t
ert_writeSocket(struct Ert_Socket *self,
            const char *aBuf, size_t aLen, const struct Ert_Duration *aTimeout);

ERT_CHECKED ssize_t
ert_readSocket(struct Ert_Socket *self,
           char *aBuf, size_t aLen, const struct Ert_Duration *aTimeout);

/* -------------------------------------------------------------------------- */
ERT_CHECKED ssize_t
ert_writeSocketDeadline(struct Ert_Socket *self,
                    const char *aBuf, size_t aLen,
                    struct Ert_Deadline *aDeadline);

ERT_CHECKED ssize_t
ert_readSocketDeadline(struct Ert_Socket *self,
                   char *aBuf, size_t aLen,
                   struct Ert_Deadline *aDeadline);

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
ert_waitSocketWriteReady(const struct Ert_Socket   *self,
                     const struct Ert_Duration *aTimeout);

ERT_CHECKED int
ert_waitSocketReadReady(const struct Ert_Socket   *self,
                    const struct Ert_Duration *aTimeout);

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
ert_bindSocket(struct Ert_Socket *self,
               struct sockaddr *aAddr, size_t aAddrLen);

ERT_CHECKED int
ert_connectSocket(struct Ert_Socket *self,
                  struct sockaddr *aAddr, size_t aAddrLen);

ERT_CHECKED int
ert_acceptSocket(struct Ert_Socket *self, unsigned aFlags);

ERT_CHECKED int
ert_listenSocket(struct Ert_Socket *self, unsigned aQueueLen);

ERT_CHECKED ssize_t
ert_sendSocket(struct Ert_Socket *self, const char *aBuf, size_t aLen);

ERT_CHECKED ssize_t
ert_recvSocket(struct Ert_Socket *self, char *aBuf, size_t aLen);

ERT_CHECKED ssize_t
ert_sendSocketMsg(struct Ert_Socket *self,
                  const struct msghdr *aMsg, int aFlags);

ERT_CHECKED ssize_t
ert_recvSocketMsg(struct Ert_Socket *self, struct msghdr *aMsg, int aFlags);

ERT_CHECKED int
ert_shutdownSocketReader(struct Ert_Socket *self);

ERT_CHECKED int
ert_shutdownSocketWriter(struct Ert_Socket *self);

ERT_CHECKED int
ert_ownSocketName(const struct Ert_Socket *self,
              struct sockaddr *aAddr, socklen_t *aAddrLen);

ERT_CHECKED int
ert_ownSocketPeerName(const struct Ert_Socket *self,
                  struct sockaddr *aAddr, socklen_t *aAddrLen);

ERT_CHECKED int
ert_ownSocketError(const struct Ert_Socket *self, int *aError);

ERT_CHECKED int
ert_ownSocketPeerCred(const struct Ert_Socket *self, struct ucred *aCred);

/* -------------------------------------------------------------------------- */

ERT_END_C_SCOPE;

#endif /* ERT_SOCKET_H */
