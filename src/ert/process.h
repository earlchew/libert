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
#ifndef ERT_PROCESS_H
#define ERT_PROCESS_H

#include "ert/compiler.h"
#include "ert/timescale.h"
#include "ert/method.h"
#include "ert/pid.h"
#include "ert/uid.h"
#include "ert/error.h"
#include "ert/method.h"

#include <limits.h>

#include <sys/types.h>

/* -------------------------------------------------------------------------- */
ERT_BEGIN_C_SCOPE;

struct FdSet;

struct PreForkProcess
{
    struct FdSet *mBlacklistFds;
    struct FdSet *mWhitelistFds;
};

ERT_END_C_SCOPE;

/* -------------------------------------------------------------------------- */
#define ERT_METHOD_DEFINITION
#define ERT_METHOD_RETURN_PreForkProcessMethod    int
#define ERT_METHOD_CONST_PreForkProcessMethod
#define ERT_METHOD_ARG_LIST_PreForkProcessMethod  \
    (const struct PreForkProcess *aPreFork_)
#define ERT_METHOD_CALL_LIST_PreForkProcessMethod (aPreFork_)

#define ERT_METHOD_NAME      PreForkProcessMethod
#define ERT_METHOD_RETURN    ERT_METHOD_RETURN_PreForkProcessMethod
#define ERT_METHOD_CONST     ERT_METHOD_CONST_PreForkProcessMethod
#define ERT_METHOD_ARG_LIST  ERT_METHOD_ARG_LIST_PreForkProcessMethod
#define ERT_METHOD_CALL_LIST ERT_METHOD_CALL_LIST_PreForkProcessMethod
#include "ert/method.h"

#define PreForkProcessMethod(Object_, Method_)     \
    ERT_METHOD_TRAMPOLINE(                         \
        Object_, Method_,                          \
        PreForkProcessMethod_,                     \
        ERT_METHOD_RETURN_PreForkProcessMethod,    \
        ERT_METHOD_CONST_PreForkProcessMethod,     \
        ERT_METHOD_ARG_LIST_PreForkProcessMethod,  \
        ERT_METHOD_CALL_LIST_PreForkProcessMethod)

/* -------------------------------------------------------------------------- */
#define ERT_METHOD_DEFINITION
#define ERT_METHOD_RETURN_PostForkParentProcessMethod    int
#define ERT_METHOD_CONST_PostForkParentProcessMethod
#define ERT_METHOD_ARG_LIST_PostForkParentProcessMethod  (struct Pid aChildPid_)
#define ERT_METHOD_CALL_LIST_PostForkParentProcessMethod (aChildPid_)

#define ERT_METHOD_NAME      PostForkParentProcessMethod
#define ERT_METHOD_RETURN    ERT_METHOD_RETURN_PostForkParentProcessMethod
#define ERT_METHOD_CONST     ERT_METHOD_CONST_PostForkParentProcessMethod
#define ERT_METHOD_ARG_LIST  ERT_METHOD_ARG_LIST_PostForkParentProcessMethod
#define ERT_METHOD_CALL_LIST ERT_METHOD_CALL_LIST_PostForkParentProcessMethod
#include "ert/method.h"

#define PostForkParentProcessMethod(Object_, Method_)     \
    ERT_METHOD_TRAMPOLINE(                                \
        Object_, Method_,                                 \
        PostForkParentProcessMethod_,                     \
        ERT_METHOD_RETURN_PostForkParentProcessMethod,    \
        ERT_METHOD_CONST_PostForkParentProcessMethod,     \
        ERT_METHOD_ARG_LIST_PostForkParentProcessMethod,  \
        ERT_METHOD_CALL_LIST_PostForkParentProcessMethod)

/* -------------------------------------------------------------------------- */
#define ERT_METHOD_DEFINITION
#define ERT_METHOD_RETURN_PostForkChildProcessMethod    int
#define ERT_METHOD_CONST_PostForkChildProcessMethod
#define ERT_METHOD_ARG_LIST_PostForkChildProcessMethod  ()
#define ERT_METHOD_CALL_LIST_PostForkChildProcessMethod ()

#define ERT_METHOD_NAME      PostForkChildProcessMethod
#define ERT_METHOD_RETURN    ERT_METHOD_RETURN_PostForkChildProcessMethod
#define ERT_METHOD_CONST     ERT_METHOD_CONST_PostForkChildProcessMethod
#define ERT_METHOD_ARG_LIST  ERT_METHOD_ARG_LIST_PostForkChildProcessMethod
#define ERT_METHOD_CALL_LIST ERT_METHOD_CALL_LIST_PostForkChildProcessMethod
#include "ert/method.h"

#define PostForkChildProcessMethod(Object_, Method_)     \
    ERT_METHOD_TRAMPOLINE(                               \
        Object_, Method_,                                \
        PostForkChildProcessMethod_,                     \
        ERT_METHOD_RETURN_PostForkChildProcessMethod,    \
        ERT_METHOD_CONST_PostForkChildProcessMethod,     \
        ERT_METHOD_ARG_LIST_PostForkChildProcessMethod,  \
        ERT_METHOD_CALL_LIST_PostForkChildProcessMethod)

/* -------------------------------------------------------------------------- */
#define ERT_METHOD_DEFINITION
#define ERT_METHOD_RETURN_ForkProcessMethod    int
#define ERT_METHOD_CONST_ForkProcessMethod
#define ERT_METHOD_ARG_LIST_ForkProcessMethod  ()
#define ERT_METHOD_CALL_LIST_ForkProcessMethod ()

#define ERT_METHOD_NAME      ForkProcessMethod
#define ERT_METHOD_RETURN    ERT_METHOD_RETURN_ForkProcessMethod
#define ERT_METHOD_CONST     ERT_METHOD_CONST_ForkProcessMethod
#define ERT_METHOD_ARG_LIST  ERT_METHOD_ARG_LIST_ForkProcessMethod
#define ERT_METHOD_CALL_LIST ERT_METHOD_CALL_LIST_ForkProcessMethod
#include "ert/method.h"

#define ForkProcessMethod(Object_, Method_)     \
    ERT_METHOD_TRAMPOLINE(                      \
        Object_, Method_,                       \
        ForkProcessMethod_,                     \
        ERT_METHOD_RETURN_ForkProcessMethod,    \
        ERT_METHOD_CONST_ForkProcessMethod,     \
        ERT_METHOD_ARG_LIST_ForkProcessMethod,  \
        ERT_METHOD_CALL_LIST_ForkProcessMethod)

/* -------------------------------------------------------------------------- */
#define ERT_METHOD_DEFINITION
#define ERT_METHOD_RETURN_WatchProcessMethod    int
#define ERT_METHOD_CONST_WatchProcessMethod
#define ERT_METHOD_ARG_LIST_WatchProcessMethod  ()
#define ERT_METHOD_CALL_LIST_WatchProcessMethod ()

#define ERT_METHOD_NAME      WatchProcessMethod
#define ERT_METHOD_RETURN    ERT_METHOD_RETURN_WatchProcessMethod
#define ERT_METHOD_CONST     ERT_METHOD_CONST_WatchProcessMethod
#define ERT_METHOD_ARG_LIST  ERT_METHOD_ARG_LIST_WatchProcessMethod
#define ERT_METHOD_CALL_LIST ERT_METHOD_CALL_LIST_WatchProcessMethod
#include "ert/method.h"

#define WatchProcessMethod(Object_, Method_)    \
    ERT_METHOD_TRAMPOLINE(                      \
        Object_, Method_,                       \
        WatchProcessMethod_,                    \
        ERT_METHOD_RETURN_WatchProcessMethod,   \
        ERT_METHOD_CONST_WatchProcessMethod,    \
        ERT_METHOD_ARG_LIST_WatchProcessMethod, \
        ERT_METHOD_CALL_LIST_WatchProcessMethod)

/* -------------------------------------------------------------------------- */
#define ERT_METHOD_DEFINITION
#define ERT_METHOD_RETURN_WatchProcessSignalMethod    int
#define ERT_METHOD_CONST_WatchProcessSignalMethod
#define ERT_METHOD_ARG_LIST_WatchProcessSignalMethod  (int aSigNum_,     \
                                                       struct Pid aPid_, \
                                                       struct Uid aUid_)
#define ERT_METHOD_CALL_LIST_WatchProcessSignalMethod (aSigNum_, aPid_, aUid_)

#define ERT_METHOD_NAME      WatchProcessSignalMethod
#define ERT_METHOD_RETURN    ERT_METHOD_RETURN_WatchProcessSignalMethod
#define ERT_METHOD_CONST     ERT_METHOD_CONST_WatchProcessSignalMethod
#define ERT_METHOD_ARG_LIST  ERT_METHOD_ARG_LIST_WatchProcessSignalMethod
#define ERT_METHOD_CALL_LIST ERT_METHOD_CALL_LIST_WatchProcessSignalMethod
#include "ert/method.h"

#define WatchProcessSignalMethod(Object_, Method_)      \
    ERT_METHOD_TRAMPOLINE(                              \
        Object_, Method_,                               \
        WatchProcessSignalMethod_,                      \
        ERT_METHOD_RETURN_WatchProcessSignalMethod,     \
        ERT_METHOD_CONST_WatchProcessSignalMethod,      \
        ERT_METHOD_ARG_LIST_WatchProcessSignalMethod,   \
        ERT_METHOD_CALL_LIST_WatchProcessSignalMethod)

/* -------------------------------------------------------------------------- */
ERT_BEGIN_C_SCOPE;

struct timespec;

struct BootClockTime;
struct Pipe;
struct File;
struct ProcessAppLock;

struct ProcessModule
{
    struct ProcessModule *mModule;

    struct ErrorModule  mErrorModule_;
    struct ErrorModule *mErrorModule;
};

#define PRId_ExitCode "d"
#define FMTd_ExitCode(ExitCode) ((ExitCode).mStatus)
struct ExitCode
{
    int mStatus;
};

enum ForkProcessOption
{
    ForkProcessInheritProcessGroup,
    ForkProcessSetProcessGroup,
    ForkProcessSetSessionLeader,
};

#define PRIs_ProcessState "c"
#define FMTs_ProcessState(State) ((State).mState)
struct ProcessState
{
    enum
    {
        ProcessStateError    = '#',
        ProcessStateRunning  = 'R',
        ProcessStateSleeping = 'S',
        ProcessStateWaiting  = 'W',
        ProcessStateZombie   = 'Z',
        ProcessStateStopped  = 'T',
        ProcessStateTraced   = 'D',
        ProcessStateDead     = 'X'
    } mState;
};

#define PRIs_ChildProcessState "c.%d"
#define FMTs_ChildProcessState(State) \
    ((State).mChildState), ((State).mChildState)
struct ChildProcessState
{
    enum
    {
        ChildProcessStateError     = '#',
        ChildProcessStateRunning   = 'r',
        ChildProcessStateExited    = 'x',
        ChildProcessStateKilled    = 'k',
        ChildProcessStateDumped    = 'd',
        ChildProcessStateStopped   = 's',
        ChildProcessStateTrapped   = 't',
    } mChildState;

    int mChildStatus;
};

struct Program
{
    struct ExitCode mExitCode;
};

/* -------------------------------------------------------------------------- */
struct ProcessSigContTracker
ProcessSigContTracker_(void);

struct ProcessSigContTracker
{
#ifdef __cplusplus
    ProcessSigContTracker()
    { *this = ProcessSigContTracker_(); }
#endif

    unsigned mCount;
};

/* -------------------------------------------------------------------------- */
#define PROCESS_SIGNALNAME_FMT_ "signal %d"

struct ProcessSignalName
{
    char mSignalText_[sizeof(PROCESS_SIGNALNAME_FMT_) +
                      sizeof("-") +
                      sizeof(int) * CHAR_BIT];

    const char *mSignalName;
};

const char *
formatProcessSignalName(struct ProcessSignalName *self, int aSigNum);

/* -------------------------------------------------------------------------- */
#define PROCESS_DIRNAME_FMT_  "/proc/%" PRId_Pid

struct ProcessDirName
{
    char mDirName[sizeof(PROCESS_DIRNAME_FMT_) + sizeof(pid_t) * CHAR_BIT];
};

ERT_CHECKED int
initProcessDirName(struct ProcessDirName *self, struct Pid aPid);

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
purgeProcessOrphanedFds(void);

/* -------------------------------------------------------------------------- */
unsigned
ownProcessSignalContext(void);

ERT_CHECKED int
watchProcessChildren(struct WatchProcessMethod aMethod);

ERT_CHECKED int
unwatchProcessChildren(void);

ERT_CHECKED int
watchProcessSignals(struct WatchProcessSignalMethod aMethod);

ERT_CHECKED int
unwatchProcessSignals(void);

ERT_CHECKED int
ignoreProcessSigPipe(void);

ERT_CHECKED int
resetProcessSigPipe(void);

ERT_CHECKED int
watchProcessSigCont(struct WatchProcessMethod aMethod);

ERT_CHECKED int
unwatchProcessSigCont(void);

ERT_CHECKED int
watchProcessSigStop(struct WatchProcessMethod aMethod);

ERT_CHECKED int
unwatchProcessSigStop(void);

ERT_CHECKED int
watchProcessClock(struct WatchProcessMethod aMethod, struct Duration aPeriod);

ERT_CHECKED int
unwatchProcessClock(void);

/* -------------------------------------------------------------------------- */
#ifndef __cplusplus
static inline struct ProcessSigContTracker
ProcessSigContTracker(void)
{
    return ProcessSigContTracker_();
}
#endif

bool
checkProcessSigContTracker(struct ProcessSigContTracker *self);

/* -------------------------------------------------------------------------- */
ERT_CHECKED struct Pid
forkProcessChild(enum ForkProcessOption             aOption,
                 struct Pgid                        aPgid,
                 struct PreForkProcessMethod        aPreForkMethod,
                 struct PostForkChildProcessMethod  aPostForkChildMethod,
                 struct PostForkParentProcessMethod aPostForkParentMethod,
                 struct ForkProcessMethod           aMethod);

ERT_CHECKED struct Pid
forkProcessDaemon(struct PreForkProcessMethod        aPreForkMethod,
                  struct PostForkChildProcessMethod  aPostForkChildMethod,
                  struct PostForkParentProcessMethod aPostForkParentMethod,
                  struct ForkProcessMethod           aMethod);

ERT_CHECKED int
reapProcessChild(struct Pid aPid, int *aStatus);

ERT_CHECKED struct ChildProcessState
waitProcessChild(struct Pid aPid);

ERT_CHECKED struct ChildProcessState
monitorProcessChild(struct Pid aPid);

ERT_CHECKED struct Pid
waitProcessChildren(void);

struct ExitCode
extractProcessExitStatus(int aStatus, struct Pid aPid);

void
execProcess(const char *aCmd, const char * const *aArgv);

void
execShell(const char *aCmd);

ERT_CHECKED int
signalProcessGroup(struct Pgid aPgid, int aSignal);

void
exitProcess(int aStatus) ERT_NORETURN;

void
abortProcess(void) ERT_NORETURN;

void
quitProcess(void) ERT_NORETURN;

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
acquireProcessAppLock(void);

ERT_CHECKED int
releaseProcessAppLock(void);

ERT_CHECKED struct ProcessAppLock *
createProcessAppLock(void);

ERT_CHECKED struct ProcessAppLock *
destroyProcessAppLock(struct ProcessAppLock *self);

unsigned
ownProcessAppLockCount(void);

const struct File *
ownProcessAppLockFile(const struct ProcessAppLock *self);

/* -------------------------------------------------------------------------- */
struct Duration
ownProcessElapsedTime(void);

struct MonotonicTime
ownProcessBaseTime(void);

const char*
ownProcessName(void);

struct Pid
ownProcessParentId(void);

struct Pid
ownProcessId(void);

struct Pgid
ownProcessGroupId(void);

/* -------------------------------------------------------------------------- */
struct ProcessState
fetchProcessState(struct Pid aPid);

struct Pgid
fetchProcessGroupId(struct Pid aPid);

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
Process_init(struct ProcessModule *self, const char *aArg0);

ERT_CHECKED struct ProcessModule *
Process_exit(struct ProcessModule *self);

/* -------------------------------------------------------------------------- */

ERT_END_C_SCOPE;

#endif /* ERT_PROCESS_H */
