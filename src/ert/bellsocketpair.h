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
#ifndef ERT_BELLSOCKETPAIR_H
#define ERT_BELLSOCKETPAIR_H

#include "ert/compiler.h"
#include "ert/socketpair.h"

ERT_BEGIN_C_SCOPE;

struct Ert_Duration;

struct Ert_BellSocketPair
{
    struct Ert_SocketPair  mSocketPair_;
    struct Ert_SocketPair *mSocketPair;
};

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
ert_createBellSocketPair(struct Ert_BellSocketPair *self, unsigned aFlags);

ERT_CHECKED struct Ert_BellSocketPair *
ert_closeBellSocketPair(struct Ert_BellSocketPair *self);

void
ert_closeBellSocketPairParent(struct Ert_BellSocketPair *self);

void
ert_closeBellSocketPairChild(struct Ert_BellSocketPair *self);

ERT_CHECKED int
ert_ringBellSocketPairChild(struct Ert_BellSocketPair *self);

ERT_CHECKED int
ert_ringBellSocketPairParent(struct Ert_BellSocketPair *self);

ERT_CHECKED int
ert_waitBellSocketPairChild(struct Ert_BellSocketPair *self,
                            const struct Ert_Duration *aTimeout);

ERT_CHECKED int
ert_waitBellSocketPairParent(struct Ert_BellSocketPair *self,
                             const struct Ert_Duration *aTimeout);

/* -------------------------------------------------------------------------- */

ERT_END_C_SCOPE;

#endif /* ERT_BELLSOCKETPAIR_H */
