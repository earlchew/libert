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

struct Socket
{
    struct File  mFile_;
    struct File *mFile;
};

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
createSocket(struct Socket *self, int aFd);

ERT_CHECKED struct Socket *
closeSocket(struct Socket *self);

bool
ownSocketValid(const struct Socket *self);

/* -------------------------------------------------------------------------- */
ERT_CHECKED ssize_t
writeSocket(struct Socket *self,
            const char *aBuf, size_t aLen, const struct Duration *aTimeout);

ERT_CHECKED ssize_t
readSocket(struct Socket *self,
           char *aBuf, size_t aLen, const struct Duration *aTimeout);

/* -------------------------------------------------------------------------- */
ERT_CHECKED ssize_t
writeSocketDeadline(struct Socket *self,
                    const char *aBuf, size_t aLen, struct Deadline *aDeadline);

ERT_CHECKED ssize_t
readSocketDeadline(struct Socket *self,
                   char *aBuf, size_t aLen, struct Deadline *aDeadline);

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
waitSocketWriteReady(const struct Socket   *self,
                     const struct Duration *aTimeout);

ERT_CHECKED int
waitSocketReadReady(const struct Socket   *self,
                    const struct Duration *aTimeout);

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
bindSocket(struct Socket *self, struct sockaddr *aAddr, size_t aAddrLen);

ERT_CHECKED int
connectSocket(struct Socket *self, struct sockaddr *aAddr, size_t aAddrLen);

ERT_CHECKED int
acceptSocket(struct Socket *self, unsigned aFlags);

ERT_CHECKED int
listenSocket(struct Socket *self, unsigned aQueueLen);

ERT_CHECKED ssize_t
sendSocket(struct Socket *self, const char *aBuf, size_t aLen);

ERT_CHECKED ssize_t
recvSocket(struct Socket *self, char *aBuf, size_t aLen);

ERT_CHECKED ssize_t
sendSocketMsg(struct Socket *self, const struct msghdr *aMsg, int aFlags);

ERT_CHECKED ssize_t
recvSocketMsg(struct Socket *self, struct msghdr *aMsg, int aFlags);

ERT_CHECKED int
shutdownSocketReader(struct Socket *self);

ERT_CHECKED int
shutdownSocketWriter(struct Socket *self);

ERT_CHECKED int
ownSocketName(const struct Socket *self,
              struct sockaddr *aAddr, socklen_t *aAddrLen);

ERT_CHECKED int
ownSocketPeerName(const struct Socket *self,
                  struct sockaddr *aAddr, socklen_t *aAddrLen);

ERT_CHECKED int
ownSocketError(const struct Socket *self, int *aError);

ERT_CHECKED int
ownSocketPeerCred(const struct Socket *self, struct ucred *aCred);

/* -------------------------------------------------------------------------- */

ERT_END_C_SCOPE;

#endif /* ERT_SOCKET_H */
