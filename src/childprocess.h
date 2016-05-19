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
#ifndef CHILD_H
#define CHILD_H

#include "int_.h"
#include "pid_.h"
#include "pipe_.h"
#include "thread_.h"
#include "eventlatch_.h"

#include <stdio.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct StdFdFiller;
struct SocketPair;
struct BellSocketPair;
struct ChildMonitor;
struct UmbilicalProcess;

/* -------------------------------------------------------------------------- */
struct ChildProcess
{
    struct Pid  mPid;
    struct Pgid mPgid;

    struct EventLatch  mChildLatch_;
    struct EventLatch *mChildLatch;
    struct EventLatch  mUmbilicalLatch_;
    struct EventLatch *mUmbilicalLatch;

    struct Pipe  mTetherPipe_;
    struct Pipe *mTetherPipe;

    struct
    {
        struct ThreadSigMutex  mMutex_;
        struct ThreadSigMutex *mMutex;
        struct ChildMonitor   *mMonitor;
    } mChildMonitor;
};

/* -------------------------------------------------------------------------- */
INT
createChildProcess(struct ChildProcess *self);

INT
superviseChildProcess(struct ChildProcess *self, struct Pid aUmbilicalPid);

INT
forkChildProcess(
    struct ChildProcess   *self,
    char                 **aCmd,
    struct StdFdFiller    *aStdFdFiller,
    struct BellSocketPair *aSyncSocket,
    struct SocketPair     *aUmbilicalSocket);

INT
killChildProcess(struct ChildProcess *self, int aSigNum);

INT
closeChildProcessTether(struct ChildProcess *self);

INT
monitorChildProcess(struct ChildProcess     *self,
                    struct UmbilicalProcess *aUmbilicalProcess,
                    struct File             *aUmbilicalFile,
                    struct Pid               aParentPid,
                    struct Pipe             *aParentPipe);

INT
raiseChildProcessSigCont(struct ChildProcess *self);

INT
reapChildProcess(struct ChildProcess *self, int *aStatus);

struct ChildProcess *
closeChildProcess(struct ChildProcess *self);

int
printChildProcess(const struct ChildProcess *self, FILE *aFile);

/* -------------------------------------------------------------------------- */
INT
killChildProcessGroup(struct ChildProcess *self);

INT
pauseChildProcessGroup(struct ChildProcess *self);

INT
resumeChildProcessGroup(struct ChildProcess *self);

/* -------------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* CHILDPROCESS_H */