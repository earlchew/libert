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

struct Ert_FdSet;

struct Ert_PreForkProcess
{
    struct Ert_FdSet *mBlacklistFds;
    struct Ert_FdSet *mWhitelistFds;
};

ERT_END_C_SCOPE;

/* -------------------------------------------------------------------------- */
#define ERT_METHOD_DEFINITION
#define ERT_METHOD_RETURN_PreForkProcessMethod    int
#define ERT_METHOD_CONST_PreForkProcessMethod
#define ERT_METHOD_ARG_LIST_PreForkProcessMethod  \
    (const struct Ert_PreForkProcess *aPreFork_)
#define ERT_METHOD_CALL_LIST_PreForkProcessMethod (aPreFork_)

#define ERT_METHOD_TYPE_PREFIX     Ert_
#define ERT_METHOD_FUNCTION_PREFIX ert_

#define ERT_METHOD_NAME      PreForkProcessMethod
#define ERT_METHOD_RETURN    ERT_METHOD_RETURN_PreForkProcessMethod
#define ERT_METHOD_CONST     ERT_METHOD_CONST_PreForkProcessMethod
#define ERT_METHOD_ARG_LIST  ERT_METHOD_ARG_LIST_PreForkProcessMethod
#define ERT_METHOD_CALL_LIST ERT_METHOD_CALL_LIST_PreForkProcessMethod
#include "ert/method.h"

#define Ert_PreForkProcessMethod(Object_, Method_) \
    ERT_METHOD_TRAMPOLINE(                         \
        Object_, Method_,                          \
        Ert_PreForkProcessMethod_,                 \
        ERT_METHOD_RETURN_PreForkProcessMethod,    \
        ERT_METHOD_CONST_PreForkProcessMethod,     \
        ERT_METHOD_ARG_LIST_PreForkProcessMethod,  \
        ERT_METHOD_CALL_LIST_PreForkProcessMethod)

/* -------------------------------------------------------------------------- */
#define ERT_METHOD_DEFINITION
#define ERT_METHOD_RETURN_PostForkParentProcessMethod    int
#define ERT_METHOD_CONST_PostForkParentProcessMethod
#define ERT_METHOD_ARG_LIST_PostForkParentProcessMethod  \
    (struct Ert_Pid aChildPid_)
#define ERT_METHOD_CALL_LIST_PostForkParentProcessMethod (aChildPid_)

#define ERT_METHOD_TYPE_PREFIX     Ert_
#define ERT_METHOD_FUNCTION_PREFIX ert_

#define ERT_METHOD_NAME      PostForkParentProcessMethod
#define ERT_METHOD_RETURN    ERT_METHOD_RETURN_PostForkParentProcessMethod
#define ERT_METHOD_CONST     ERT_METHOD_CONST_PostForkParentProcessMethod
#define ERT_METHOD_ARG_LIST  ERT_METHOD_ARG_LIST_PostForkParentProcessMethod
#define ERT_METHOD_CALL_LIST ERT_METHOD_CALL_LIST_PostForkParentProcessMethod
#include "ert/method.h"

#define Ert_PostForkParentProcessMethod(Object_, Method_) \
    ERT_METHOD_TRAMPOLINE(                                \
        Object_, Method_,                                 \
        Ert_PostForkParentProcessMethod_,                 \
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

#define ERT_METHOD_TYPE_PREFIX     Ert_
#define ERT_METHOD_FUNCTION_PREFIX ert_

#define ERT_METHOD_NAME      PostForkChildProcessMethod
#define ERT_METHOD_RETURN    ERT_METHOD_RETURN_PostForkChildProcessMethod
#define ERT_METHOD_CONST     ERT_METHOD_CONST_PostForkChildProcessMethod
#define ERT_METHOD_ARG_LIST  ERT_METHOD_ARG_LIST_PostForkChildProcessMethod
#define ERT_METHOD_CALL_LIST ERT_METHOD_CALL_LIST_PostForkChildProcessMethod
#include "ert/method.h"

#define Ert_PostForkChildProcessMethod(Object_, Method_) \
    ERT_METHOD_TRAMPOLINE(                               \
        Object_, Method_,                                \
        Ert_PostForkChildProcessMethod_,                 \
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

#define ERT_METHOD_TYPE_PREFIX     Ert_
#define ERT_METHOD_FUNCTION_PREFIX ert_

#define ERT_METHOD_NAME      ForkProcessMethod
#define ERT_METHOD_RETURN    ERT_METHOD_RETURN_ForkProcessMethod
#define ERT_METHOD_CONST     ERT_METHOD_CONST_ForkProcessMethod
#define ERT_METHOD_ARG_LIST  ERT_METHOD_ARG_LIST_ForkProcessMethod
#define ERT_METHOD_CALL_LIST ERT_METHOD_CALL_LIST_ForkProcessMethod
#include "ert/method.h"

#define Ert_ForkProcessMethod(Object_, Method_) \
    ERT_METHOD_TRAMPOLINE(                      \
        Object_, Method_,                       \
        Ert_ForkProcessMethod_,                 \
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

#define ERT_METHOD_TYPE_PREFIX     Ert_
#define ERT_METHOD_FUNCTION_PREFIX ert_

#define ERT_METHOD_NAME      WatchProcessMethod
#define ERT_METHOD_RETURN    ERT_METHOD_RETURN_WatchProcessMethod
#define ERT_METHOD_CONST     ERT_METHOD_CONST_WatchProcessMethod
#define ERT_METHOD_ARG_LIST  ERT_METHOD_ARG_LIST_WatchProcessMethod
#define ERT_METHOD_CALL_LIST ERT_METHOD_CALL_LIST_WatchProcessMethod
#include "ert/method.h"

#define Ert_WatchProcessMethod(Object_, Method_)        \
    ERT_METHOD_TRAMPOLINE(                      \
        Object_, Method_,                       \
        Ert_WatchProcessMethod_,                \
        ERT_METHOD_RETURN_WatchProcessMethod,   \
        ERT_METHOD_CONST_WatchProcessMethod,    \
        ERT_METHOD_ARG_LIST_WatchProcessMethod, \
        ERT_METHOD_CALL_LIST_WatchProcessMethod)

/* -------------------------------------------------------------------------- */
#define ERT_METHOD_DEFINITION
#define ERT_METHOD_RETURN_WatchProcessSignalMethod    int
#define ERT_METHOD_CONST_WatchProcessSignalMethod
#define ERT_METHOD_ARG_LIST_WatchProcessSignalMethod  (int aSigNum_,         \
                                                       struct Ert_Pid aPid_, \
                                                       struct Ert_Uid aUid_)
#define ERT_METHOD_CALL_LIST_WatchProcessSignalMethod (aSigNum_, aPid_, aUid_)

#define ERT_METHOD_TYPE_PREFIX     Ert_
#define ERT_METHOD_FUNCTION_PREFIX ert_

#define ERT_METHOD_NAME      WatchProcessSignalMethod
#define ERT_METHOD_RETURN    ERT_METHOD_RETURN_WatchProcessSignalMethod
#define ERT_METHOD_CONST     ERT_METHOD_CONST_WatchProcessSignalMethod
#define ERT_METHOD_ARG_LIST  ERT_METHOD_ARG_LIST_WatchProcessSignalMethod
#define ERT_METHOD_CALL_LIST ERT_METHOD_CALL_LIST_WatchProcessSignalMethod
#include "ert/method.h"

#define Ert_WatchProcessSignalMethod(Object_, Method_)  \
    ERT_METHOD_TRAMPOLINE(                              \
        Object_, Method_,                               \
        Ert_WatchProcessSignalMethod_,                  \
        ERT_METHOD_RETURN_WatchProcessSignalMethod,     \
        ERT_METHOD_CONST_WatchProcessSignalMethod,      \
        ERT_METHOD_ARG_LIST_WatchProcessSignalMethod,   \
        ERT_METHOD_CALL_LIST_WatchProcessSignalMethod)

/* -------------------------------------------------------------------------- */
ERT_BEGIN_C_SCOPE;

struct timespec;

struct Ert_BootClockTime;
struct Ert_Pipe;
struct File;
struct ProcessAppLock;

struct Ert_ProcessModule
{
    struct Ert_ProcessModule *mModule;

    struct Ert_ErrorModule  mErrorModule_;
    struct Ert_ErrorModule *mErrorModule;
};

#define PRId_Ert_ExitCode "d"
#define FMTd_Ert_ExitCode(ExitCode) ((ExitCode).mStatus)
struct Ert_ExitCode
{
    int mStatus;
};

enum Ert_ForkProcessOption
{
    Ert_ForkProcessInheritProcessGroup,
    Ert_ForkProcessSetProcessGroup,
    Ert_ForkProcessSetSessionLeader,
};

#define PRIs_Ert_ProcessState "c"
#define FMTs_Ert_ProcessState(State) ((State).mState)
struct Ert_ProcessState
{
    enum
    {
        Ert_ProcessStateError    = '#',
        Ert_ProcessStateRunning  = 'R',
        Ert_ProcessStateSleeping = 'S',
        Ert_ProcessStateWaiting  = 'W',
        Ert_ProcessStateZombie   = 'Z',
        Ert_ProcessStateStopped  = 'T',
        Ert_ProcessStateTraced   = 'D',
        Ert_ProcessStateDead     = 'X'
    } mState;
};

#define PRIs_Ert_ChildProcessState "c.%d"
#define FMTs_Ert_ChildProcessState(State) \
    ((State).mChildState), ((State).mChildState)
struct Ert_ChildProcessState
{
    enum
    {
        Ert_ChildProcessStateError     = '#',
        Ert_ChildProcessStateRunning   = 'r',
        Ert_ChildProcessStateExited    = 'x',
        Ert_ChildProcessStateKilled    = 'k',
        Ert_ChildProcessStateDumped    = 'd',
        Ert_ChildProcessStateStopped   = 's',
        Ert_ChildProcessStateTrapped   = 't',
    } mChildState;

    int mChildStatus;
};

struct Ert_Program
{
    struct Ert_ExitCode mExitCode;
};

/* -------------------------------------------------------------------------- */
struct Ert_ProcessSigContTracker
Ert_ProcessSigContTracker_(void);

struct Ert_ProcessSigContTracker
{
#ifdef __cplusplus
    Ert_ProcessSigContTracker()
    { *this = Ert_ProcessSigContTracker_(); }
#endif

    unsigned mCount;
};

/* -------------------------------------------------------------------------- */
#define ERT_PROCESS_SIGNALNAME_FMT_ "signal %d"

struct Ert_ProcessSignalName
{
    char mSignalText_[sizeof(ERT_PROCESS_SIGNALNAME_FMT_) +
                      sizeof("-") +
                      sizeof(int) * CHAR_BIT];

    const char *mSignalName;
};

const char *
ert_formatProcessSignalName(struct Ert_ProcessSignalName *self, int aSigNum);

/* -------------------------------------------------------------------------- */
#define ERT_PROCESS_DIRNAME_FMT_  "/proc/%" PRId_Ert_Pid

struct Ert_ProcessDirName
{
    char mDirName[sizeof(ERT_PROCESS_DIRNAME_FMT_) + sizeof(pid_t) * CHAR_BIT];
};

ERT_CHECKED int
ert_initProcessDirName(struct Ert_ProcessDirName *self, struct Ert_Pid aPid);

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
ert_purgeProcessOrphanedFds(void);

/* -------------------------------------------------------------------------- */
unsigned
ert_ownProcessSignalContext(void);

ERT_CHECKED int
ert_watchProcessChildren(struct Ert_WatchProcessMethod aMethod);

ERT_CHECKED int
ert_unwatchProcessChildren(void);

ERT_CHECKED int
ert_watchProcessSignals(struct Ert_WatchProcessSignalMethod aMethod);

ERT_CHECKED int
ert_unwatchProcessSignals(void);

ERT_CHECKED int
ert_ignoreProcessSigPipe(void);

ERT_CHECKED int
ert_resetProcessSigPipe(void);

ERT_CHECKED int
ert_watchProcessSigCont(struct Ert_WatchProcessMethod aMethod);

ERT_CHECKED int
ert_unwatchProcessSigCont(void);

ERT_CHECKED int
ert_watchProcessSigStop(struct Ert_WatchProcessMethod aMethod);

ERT_CHECKED int
ert_unwatchProcessSigStop(void);

ERT_CHECKED int
ert_watchProcessClock(
    struct Ert_WatchProcessMethod aMethod, struct Ert_Duration aPeriod);

ERT_CHECKED int
ert_unwatchProcessClock(void);

/* -------------------------------------------------------------------------- */
#ifndef __cplusplus
static inline struct Ert_ProcessSigContTracker
Ert_ProcessSigContTracker(void)
{
    return Ert_ProcessSigContTracker_();
}
#endif

bool
ert_checkProcessSigContTracker(struct Ert_ProcessSigContTracker *self);

/* -------------------------------------------------------------------------- */
ERT_CHECKED struct Ert_Pid
ert_forkProcessChild(
    enum Ert_ForkProcessOption             aOption,
    struct Ert_Pgid                        aPgid,
    struct Ert_PreForkProcessMethod        aPreForkMethod,
    struct Ert_PostForkChildProcessMethod  aPostForkChildMethod,
    struct Ert_PostForkParentProcessMethod aPostForkParentMethod,
    struct Ert_ForkProcessMethod           aMethod);

ERT_CHECKED struct Ert_Pid
ert_forkProcessDaemon(
    struct Ert_PreForkProcessMethod        aPreForkMethod,
    struct Ert_PostForkChildProcessMethod  aPostForkChildMethod,
    struct Ert_PostForkParentProcessMethod aPostForkParentMethod,
    struct Ert_ForkProcessMethod           aMethod);

ERT_CHECKED int
ert_reapProcessChild(struct Ert_Pid aPid, int *aStatus);

ERT_CHECKED struct Ert_ChildProcessState
ert_waitProcessChild(struct Ert_Pid aPid);

ERT_CHECKED struct Ert_ChildProcessState
ert_monitorProcessChild(struct Ert_Pid aPid);

ERT_CHECKED struct Ert_Pid
ert_waitProcessChildren(void);

struct Ert_ExitCode
ert_extractProcessExitStatus(int aStatus, struct Ert_Pid aPid);

void
ert_execProcess(const char *aCmd, const char * const *aArgv);

void
ert_execShell(const char *aCmd);

ERT_CHECKED int
ert_signalProcessGroup(struct Ert_Pgid aPgid, int aSignal);

void
ert_exitProcess(int aStatus) ERT_NORETURN;

void
ert_abortProcess(void) ERT_NORETURN;

void
ert_quitProcess(void) ERT_NORETURN;

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
ert_acquireProcessAppLock(void);

ERT_CHECKED int
ert_releaseProcessAppLock(void);

ERT_CHECKED struct Ert_ProcessAppLock *
ert_createProcessAppLock(void);

ERT_CHECKED struct Ert_ProcessAppLock *
ert_destroyProcessAppLock(struct Ert_ProcessAppLock *self);

unsigned
ert_ownProcessAppLockCount(void);

const struct Ert_File *
ert_ownProcessAppLockFile(const struct Ert_ProcessAppLock *self);

/* -------------------------------------------------------------------------- */
struct Ert_Duration
ert_ownProcessElapsedTime(void);

struct Ert_MonotonicTime
ert_ownProcessBaseTime(void);

const char*
ert_ownProcessName(void);

struct Ert_Pid
ert_ownProcessParentId(void);

struct Ert_Pid
ert_ownProcessId(void);

struct Ert_Pgid
ert_ownProcessGroupId(void);

/* -------------------------------------------------------------------------- */
struct Ert_ProcessState
ert_fetchProcessState(struct Ert_Pid aPid);

struct Ert_Pgid
ert_fetchProcessGroupId(struct Ert_Pid aPid);

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
Ert_Process_init(struct Ert_ProcessModule *self, const char *aArg0);

ERT_CHECKED struct Ert_ProcessModule *
Ert_Process_exit(struct Ert_ProcessModule *self);

/* -------------------------------------------------------------------------- */

ERT_END_C_SCOPE;

#endif /* ERT_PROCESS_H */
