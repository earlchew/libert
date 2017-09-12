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

#include "ert/process.h"
#include "ert/pipe.h"
#include "ert/stdfdfiller.h"
#include "ert/bellsocketpair.h"
#include "ert/timekeeping.h"
#include "ert/thread.h"
#include "ert/fdset.h"
#include "ert/random.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/file.h>
#include <sys/wait.h>

#include <valgrind/valgrind.h>

struct ProcessLock
{
    struct Ert_File            mFile_;
    struct Ert_File           *mFile;
    const struct Ert_LockType *mLock;
};

struct Ert_ProcessAppLock
{
    void *mNull;
};

static struct Ert_ProcessAppLock processAppLock_;

static unsigned processAbort_;
static unsigned processQuit_;

static struct
{
    struct Ert_ThreadSigMutex  mMutex_;
    struct Ert_ThreadSigMutex *mMutex;
    struct ProcessLock     mLock_;
    struct ProcessLock    *mLock;
} processLock_ =
{
    .mMutex_ = ERT_THREAD_SIG_MUTEX_INITIALIZER(processLock_.mMutex_),
    .mMutex  = &processLock_.mMutex_,
};

struct ForkProcessChannel_;

static struct Ert_ProcessForkChildLock_
{
    pthread_mutex_t  mMutex_;
    pthread_mutex_t *mMutex;
    struct Ert_Pid       mProcess;
    struct Ert_Tid       mThread;
    unsigned         mCount;

    struct ForkProcessChannel_ *mChannelList;

} processForkChildLock_ =
{
    .mMutex_ = PTHREAD_MUTEX_INITIALIZER,
};

static struct
{
    pthread_mutex_t        mMutex_;
    pthread_mutex_t       *mMutex;
    struct Ert_Pid             mParentPid;
    struct Ert_RWMutexWriter   mSigVecLock_;
    struct Ert_RWMutexWriter  *mSigVecLock;
    struct Ert_ThreadSigMutex *mSigLock;
    struct Ert_ThreadSigMutex *mLock;
} processFork_ =
{
    .mMutex_ = PTHREAD_MUTEX_INITIALIZER,
};

static unsigned              moduleInit_;
static bool                  moduleInitOnce_;
static bool                  moduleInitAtFork_;
static sigset_t              processSigMask_;
static const char           *processArg0_;
static const char           *programName_;
static struct Ert_MonotonicTime  processTimeBase_;

static const char *signalNames_[NSIG] =
{
    [SIGHUP]    = "SIGHUP",
    [SIGABRT]   = "SIGABRT",
    [SIGALRM]   = "SIGALRM",
    [SIGBUS]    = "SIGBUS",
    [SIGCHLD]   = "SIGCHLD",
    [SIGCONT]   = "SIGCONT",
    [SIGFPE]    = "SIGFPE",
    [SIGHUP]    = "SIGHUP",
    [SIGILL]    = "SIGILL",
    [SIGINT]    = "SIGINT",
    [SIGKILL]   = "SIGKILL",
    [SIGPIPE]   = "SIGPIPE",
    [SIGQUIT]   = "SIGQUIT",
    [SIGSEGV]   = "SIGSEGV",
    [SIGSTOP]   = "SIGSTOP",
    [SIGTERM]   = "SIGTERM",
    [SIGTSTP]   = "SIGTSTP",
    [SIGTTIN]   = "SIGTTIN",
    [SIGTTOU]   = "SIGTTOU",
    [SIGUSR1]   = "SIGUSR1",
    [SIGUSR2]   = "SIGUSR2",
    [SIGPOLL]   = "SIGPOLL",
    [SIGPROF]   = "SIGPROF",
    [SIGSYS]    = "SIGSYS",
    [SIGTRAP]   = "SIGTRAP",
    [SIGURG]    = "SIGURG",
    [SIGVTALRM] = "SIGVTALRM",
    [SIGXCPU]   = "SIGXCPU",
    [SIGXFSZ]   = "SIGXFSZ",
};

/* -------------------------------------------------------------------------- */
static unsigned __thread processSignalContext_;

struct ProcessSignalVector
{
    struct sigaction mAction;
    pthread_mutex_t  mActionMutex_;
    pthread_mutex_t *mActionMutex;
};

static struct
{
    struct ProcessSignalVector mVector[NSIG];
    pthread_rwlock_t           mVectorLock;
    struct Ert_ThreadSigMutex      mSignalMutex;

} processSignals_ =
{
    .mVectorLock  = PTHREAD_RWLOCK_INITIALIZER,
    .mSignalMutex = ERT_THREAD_SIG_MUTEX_INITIALIZER(
        processSignals_.mSignalMutex),
};

static void
dispatchSigExit_(int aSigNum)
{
    /* Check for handlers for termination signals that might compete
     * with programmatically requested behaviour. */

    if (SIGABRT == aSigNum && processAbort_)
        ert_abortProcess();

    if (SIGQUIT == aSigNum && processQuit_)
        ert_quitProcess();
}

static void
runSigAction_(int aSigNum, siginfo_t *aSigInfo, void *aSigContext)
{
    struct ProcessSignalVector *sv = &processSignals_.mVector[aSigNum];

    struct Ert_RWMutexReader  sigVecLock_;
    struct Ert_RWMutexReader *sigVecLock;

    sigVecLock = ert_createRWMutexReader(
        &sigVecLock_, &processSignals_.mVectorLock);

    enum Ert_ErrorFrameStackKind stackKind =
        ert_switchErrorFrameStack(Ert_ErrorFrameStackSignal);

    struct Ert_ProcessSignalName sigName;
    debug(1,
          "dispatch signal %s",
          ert_formatProcessSignalName(&sigName, aSigNum));

    pthread_mutex_t *actionLock = ert_lockMutex(sv->mActionMutex);
    {
        dispatchSigExit_(aSigNum);

        if (SIG_DFL != sv->mAction.sa_handler &&
            SIG_IGN != sv->mAction.sa_handler)
        {
            ++processSignalContext_;

            struct Ert_ErrorFrameSequence frameSequence =
                ert_pushErrorFrameSequence();

            sv->mAction.sa_sigaction(aSigNum, aSigInfo, aSigContext);

            ert_popErrorFrameSequence(frameSequence);

            --processSignalContext_;
        }
    }
    actionLock = ert_unlockMutex(actionLock);

    ert_switchErrorFrameStack(stackKind);
    sigVecLock = ert_destroyRWMutexReader(sigVecLock);
}

static void
dispatchSigAction_(int aSigNum, siginfo_t *aSigInfo, void *aSigContext)
{
    FINALLY
    ({
        runSigAction_(aSigNum, aSigInfo, aSigContext);
    });
}

static void
runSigHandler_(int aSigNum)
{
    struct ProcessSignalVector *sv = &processSignals_.mVector[aSigNum];

    struct Ert_RWMutexReader  sigVecLock_;
    struct Ert_RWMutexReader *sigVecLock;

    sigVecLock = ert_createRWMutexReader(
        &sigVecLock_, &processSignals_.mVectorLock);

    enum Ert_ErrorFrameStackKind stackKind =
        ert_switchErrorFrameStack(Ert_ErrorFrameStackSignal);

    struct Ert_ProcessSignalName sigName;
    debug(1,
          "dispatch signal %s",
          ert_formatProcessSignalName(&sigName, aSigNum));

    pthread_mutex_t *actionLock = ert_lockMutex(sv->mActionMutex);
    {
        dispatchSigExit_(aSigNum);

        if (SIG_DFL != sv->mAction.sa_handler &&
            SIG_IGN != sv->mAction.sa_handler)
        {
            ++processSignalContext_;

            struct Ert_ErrorFrameSequence frameSequence =
                ert_pushErrorFrameSequence();

            sv->mAction.sa_handler(aSigNum);

            ert_popErrorFrameSequence(frameSequence);

            --processSignalContext_;
        }
    }
    actionLock = ert_unlockMutex(actionLock);

    ert_switchErrorFrameStack(stackKind);
    sigVecLock = ert_destroyRWMutexReader(sigVecLock);
}

static void
dispatchSigHandler_(int aSigNum)
{
    FINALLY
    ({
        runSigHandler_(aSigNum);
    });
}

static ERT_CHECKED int
changeSigAction_(unsigned          aSigNum,
                 struct sigaction  aNewAction,
                 struct sigaction *aOldAction)
{
    int rc = -1;

    ensure(ERT_NUMBEROF(processSignals_.mVector) > aSigNum);

    struct Ert_RWMutexReader  sigVecLock_;
    struct Ert_RWMutexReader *sigVecLock = 0;

    struct Ert_ThreadSigMask  threadSigMask_;
    struct Ert_ThreadSigMask *threadSigMask = 0;

    pthread_mutex_t *actionLock = 0;

    struct sigaction nextAction = aNewAction;

    if (SIG_DFL != nextAction.sa_handler && SIG_IGN != nextAction.sa_handler)
    {
        if (nextAction.sa_flags & SA_SIGINFO)
            nextAction.sa_sigaction = dispatchSigAction_;
        else
            nextAction.sa_handler = dispatchSigHandler_;

        /* Require that signal delivery not restart system calls.
         * This is important so that event loops have a chance
         * to re-compute their deadlines. See restart_syscalls()
         * for related information pertaining to SIGCONT.
         *
         * See the wrappers in eintr_.c that wrap system calls
         * to ensure that restart semantics are available. */

        ERROR_IF(
            (nextAction.sa_flags & SA_RESTART));

        /* Require that signal delivery is not recursive to avoid
         * having to deal with too many levels of re-entrancy, but
         * allow programmatic failures to be delivered in order to
         * terminate the program. */

        sigset_t filledSigSet;
        ERROR_IF(
            sigfillset(&filledSigSet));

        ERROR_IF(
            sigdelset(&filledSigSet, SIGABRT));

        nextAction.sa_mask   = filledSigSet;
        nextAction.sa_flags &= ~ SA_NODEFER;
    }

    {
        struct Ert_ThreadSigMutex *sigLock =
            ert_lockThreadSigMutex(&processSignals_.mSignalMutex);

        if ( ! processSignals_.mVector[aSigNum].mActionMutex)
            processSignals_.mVector[aSigNum].mActionMutex =
                ert_createMutex(
                    &processSignals_.mVector[aSigNum].mActionMutex_);

        sigLock = ert_unlockThreadSigMutex(sigLock);
    }

    /* Block signal delivery into this thread to avoid the signal
     * dispatch attempting to acquire the dispatch mutex recursively
     * in the same thread context. */

    sigVecLock = ert_createRWMutexReader(
        &sigVecLock_, &processSignals_.mVectorLock);

    threadSigMask = ert_pushThreadSigMask(
        &threadSigMask_, Ert_ThreadSigMaskBlock, (const int []) { aSigNum, 0 });

    actionLock = ert_lockMutex(processSignals_.mVector[aSigNum].mActionMutex);

    struct sigaction prevAction;
    ERROR_IF(
        sigaction(aSigNum, &nextAction, &prevAction));

    /* Do not overwrite the output result unless the underlying
     * sigaction() call succeeds. */

    if (aOldAction)
        *aOldAction = prevAction;

    processSignals_.mVector[aSigNum].mAction = aNewAction;

    rc = 0;

Finally:

    FINALLY
    ({
        actionLock = ert_unlockMutex(actionLock);

        threadSigMask = ert_popThreadSigMask(threadSigMask);

        sigVecLock = ert_destroyRWMutexReader(sigVecLock);
    });

    return rc;
}

unsigned
ert_ownProcessSignalContext(void)
{
    return processSignalContext_;
}

/* -------------------------------------------------------------------------- */
static struct sigaction processSigPipeAction_ =
{
    .sa_handler = SIG_ERR,
};

int
ert_ignoreProcessSigPipe(void)
{
    int rc = -1;

    ERROR_IF(
        changeSigAction_(
            SIGPIPE,
            (struct sigaction) { .sa_handler = SIG_IGN },
            &processSigPipeAction_));

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

static ERT_CHECKED int
resetProcessSigPipe_(void)
{
    int rc = -1;

    if (SIG_ERR != processSigPipeAction_.sa_handler ||
        (processSigPipeAction_.sa_flags & SA_SIGINFO))
    {
        ERROR_IF(
            changeSigAction_(SIGPIPE, processSigPipeAction_, 0));

        processSigPipeAction_.sa_handler = SIG_ERR;
        processSigPipeAction_.sa_flags   = 0;
    }

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

int
ert_resetProcessSigPipe(void)
{
    return resetProcessSigPipe_();
}

/* -------------------------------------------------------------------------- */
static struct
{
    struct Ert_ThreadSigMutex     mSigMutex;
    unsigned                  mCount;
    struct Ert_WatchProcessMethod mMethod;
} processSigCont_ =
{
    .mSigMutex = ERT_THREAD_SIG_MUTEX_INITIALIZER(processSigCont_.mSigMutex),
};

static void
sigCont_(int aSigNum)
{
    /* See the commentary in ert_ownProcessSigContCount() to understand
     * the motivation for using a lock free update here. Other solutions
     * are possible, but a lock free approach is the most straightforward. */

    __sync_add_and_fetch(&processSigCont_.mCount, 2);

    struct Ert_ThreadSigMutex *lock = ert_lockThreadSigMutex(
        &processSigCont_.mSigMutex);

    if (ert_ownWatchProcessMethodNil(processSigCont_.mMethod))
        debug(1, "detected SIGCONT");
    else
    {
        debug(1, "observed SIGCONT");
        ABORT_IF(
            ert_callWatchProcessMethod(processSigCont_.mMethod));
    }

    lock = ert_unlockThreadSigMutex(lock);
}

static ERT_CHECKED int
hookProcessSigCont_(void)
{
    int rc = -1;

    ERROR_IF(
        changeSigAction_(
            SIGCONT,
            (struct sigaction) { .sa_handler = sigCont_ }, 0));

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

static ERT_CHECKED int
unhookProcessSigCont_(void)
{
    int rc = -1;

    ERROR_IF(
        changeSigAction_(
            SIGCONT,
            (struct sigaction) { .sa_handler = SIG_DFL },
            0));

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

static ERT_CHECKED int
updateProcessSigContMethod_(struct Ert_WatchProcessMethod aMethod)
{
    struct Ert_ThreadSigMutex *lock =
        ert_lockThreadSigMutex(&processSigCont_.mSigMutex);
    processSigCont_.mMethod = aMethod;
    lock = ert_unlockThreadSigMutex(lock);

    return 0;
}

static ERT_CHECKED int
resetProcessSigCont_(void)
{
    int rc = -1;

    ERROR_IF(
        updateProcessSigContMethod_(Ert_WatchProcessMethodNil()));

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

int
ert_watchProcessSigCont(struct Ert_WatchProcessMethod aMethod)
{
    int rc = -1;

    ERROR_IF(
        updateProcessSigContMethod_(aMethod));

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

int
ert_unwatchProcessSigCont(void)
{
    return resetProcessSigCont_();
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED unsigned
fetchProcessSigContTracker_(void)
{
    /* Because this function is called from lockMutex(), amongst other places,
     * do not use or cause lockMutex() to be used here to avoid introducing
     * the chance of infinite recursion.
     *
     * Use the least sigificant bit to ensure that only non-zero counts
     * are valid, allowing detection of zero initialised values that
     * have not been constructed properly. */

    return 1 | processSigCont_.mCount;
}

struct Ert_ProcessSigContTracker
Ert_ProcessSigContTracker_(void)
{
    return (struct Ert_ProcessSigContTracker)
    {
        .mCount = fetchProcessSigContTracker_(),
    };
}

/* -------------------------------------------------------------------------- */
bool
ert_checkProcessSigContTracker(struct Ert_ProcessSigContTracker *self)
{
    unsigned sigContCount = self->mCount;

    ensure(1 && sigContCount);

    self->mCount = fetchProcessSigContTracker_();

    return sigContCount != self->mCount;
}

/* -------------------------------------------------------------------------- */
static struct
{
    struct Ert_ThreadSigMutex     mSigMutex;
    struct Ert_WatchProcessMethod mMethod;
} processSigStop_ =
{
    .mSigMutex = ERT_THREAD_SIG_MUTEX_INITIALIZER(processSigStop_.mSigMutex),
};

static void
sigStop_(int aSigNum)
{
    struct Ert_ThreadSigMutex *lock =
        ert_lockThreadSigMutex(&processSigStop_.mSigMutex);

    if (ert_ownWatchProcessMethodNil(processSigStop_.mMethod))
    {
        debug(1, "detected SIGTSTP");

        ABORT_IF(
            raise(SIGSTOP),
            {
                terminate(errno, "Unable to stop process");
            });
    }
    else
    {
        debug(1, "observed SIGTSTP");
        ABORT_IF(
            ert_callWatchProcessMethod(processSigStop_.mMethod));
    }

    lock = ert_unlockThreadSigMutex(lock);
}

static ERT_CHECKED int
hookProcessSigStop_(void)
{
    int rc = -1;

    ERROR_IF(
        changeSigAction_(
            SIGTSTP,
            (struct sigaction) { .sa_handler = sigStop_ },
            0));

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

static ERT_CHECKED int
unhookProcessSigStop_(void)
{
    int rc = -1;

    ERROR_IF(
        changeSigAction_(
            SIGTSTP,
            (struct sigaction) { .sa_handler = SIG_DFL },
            0));

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

static ERT_CHECKED int
updateProcessSigStopMethod_(struct Ert_WatchProcessMethod aMethod)
{
    struct Ert_ThreadSigMutex *lock =
        ert_lockThreadSigMutex(&processSigStop_.mSigMutex);
    processSigStop_.mMethod = aMethod;
    lock = ert_unlockThreadSigMutex(lock);

    return 0;
}

static ERT_CHECKED int
resetProcessSigStop_(void)
{
    int rc = -1;

    ERROR_IF(
        updateProcessSigStopMethod_(Ert_WatchProcessMethodNil()));

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

int
ert_watchProcessSigStop(struct Ert_WatchProcessMethod aMethod)
{
    int rc = -1;

    ERROR_IF(
        updateProcessSigStopMethod_(aMethod));

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

int
ert_unwatchProcessSigStop(void)
{
    return resetProcessSigStop_();
}

/* -------------------------------------------------------------------------- */
static struct Ert_WatchProcessMethod processSigChldMethod_;

static void
sigChld_(int aSigNum)
{
    if ( ! ert_ownWatchProcessMethodNil(processSigChldMethod_))
    {
        debug(1, "observed SIGCHLD");
        ABORT_IF(
            ert_callWatchProcessMethod(processSigChldMethod_));
    }
}

static ERT_CHECKED int
resetProcessChildrenWatch_(void)
{
    int rc = -1;

    ERROR_IF(
        changeSigAction_(
            SIGCHLD,
            (struct sigaction) { .sa_handler = SIG_DFL },
            0));

    processSigChldMethod_ = Ert_WatchProcessMethodNil();

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

int
ert_watchProcessChildren(struct Ert_WatchProcessMethod aMethod)
{
    int rc = -1;

    struct Ert_WatchProcessMethod sigChldMethod = processSigChldMethod_;

    processSigChldMethod_ = aMethod;

    ERROR_IF(
        changeSigAction_(
            SIGCHLD,
            (struct sigaction) { .sa_handler = sigChld_ },
            0));

    rc = 0;

Finally:

    FINALLY
    ({
        if (rc)
            processSigChldMethod_ = sigChldMethod;
    });

    return rc;
}

int
ert_unwatchProcessChildren(void)
{
    return resetProcessChildrenWatch_();
}

/* -------------------------------------------------------------------------- */
static struct Ert_Duration           processClockTickPeriod_;
static struct Ert_WatchProcessMethod processClockMethod_;
static struct sigaction          processClockTickSigAction_ =
{
    .sa_handler = SIG_ERR,
};

static void
clockTick_(int aSigNum)
{
    if (ert_ownWatchProcessMethodNil(processClockMethod_))
        debug(1, "received clock tick");
    else
    {
        debug(1, "observed clock tick");
        ABORT_IF(
            ert_callWatchProcessMethod(processClockMethod_));
    }
}

static ERT_CHECKED int
resetProcessClockWatch_(void)
{
    int rc = -1;

    if (SIG_ERR != processClockTickSigAction_.sa_handler ||
        (processClockTickSigAction_.sa_flags & SA_SIGINFO))
    {
        struct itimerval disableClock =
        {
            .it_value    = { .tv_sec = 0 },
            .it_interval = { .tv_sec = 0 },
        };

        ERROR_IF(
            setitimer(ITIMER_REAL, &disableClock, 0));

        ERROR_IF(
            changeSigAction_(SIGALRM, processClockTickSigAction_, 0));

        processClockMethod_ = Ert_WatchProcessMethodNil();

        processClockTickSigAction_.sa_handler = SIG_ERR;
        processClockTickSigAction_.sa_flags = 0;

        processClockTickPeriod_ = ZeroDuration;
    }

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

int
ert_watchProcessClock(struct Ert_WatchProcessMethod aMethod,
                      struct Ert_Duration           aClockPeriod)
{
    int rc = -1;

    struct Ert_WatchProcessMethod clockMethod = processClockMethod_;

    processClockMethod_ = aMethod;

    struct sigaction  prevAction_;
    struct sigaction *prevAction = 0;

    ERROR_IF(
        changeSigAction_(
            SIGALRM,
            (struct sigaction) { .sa_handler = clockTick_ },
            &prevAction_));
    prevAction = &prevAction_;

    /* Make sure that there are no timers already running. The
     * interface only supports one clock instance. */

    struct itimerval clockTimer;

    ERROR_IF(
        getitimer(ITIMER_REAL, &clockTimer));

    ERROR_IF(
        clockTimer.it_value.tv_sec || clockTimer.it_value.tv_usec,
        {
            errno = EPERM;
        });

    clockTimer.it_interval = clockTimer.it_value;
    clockTimer.it_value    = ert_timeValFromNanoSeconds(
        processClockTickPeriod_.duration);

    ERROR_IF(
        setitimer(ITIMER_REAL, &clockTimer, 0));

    processClockTickSigAction_ = *prevAction;
    processClockTickPeriod_    = aClockPeriod;

    rc = 0;

Finally:

    FINALLY
    ({
        if (rc)
        {
            if (prevAction)
                ABORT_IF(
                    changeSigAction_(SIGALRM, *prevAction, 0),
                    {
                        terminate(errno, "Unable to revert SIGALRM handler");
                    });

            processClockMethod_ = clockMethod;
        }
    });

    return rc;
}

int
ert_unwatchProcessClock(void)
{
    return resetProcessClockWatch_();
}

/* -------------------------------------------------------------------------- */
static struct Ert_WatchProcessSignalMethod processWatchedSignalMethod_;

static struct SignalWatch {
    int              mSigNum;
    struct sigaction mSigAction;
    bool             mWatched;
} processWatchedSignals_[] =
{
    { SIGHUP },
    { SIGINT },
    { SIGQUIT },
    { SIGTERM },
};

static void
caughtSignal_(int aSigNum, siginfo_t *aSigInfo, void *aUContext_)
{
    if ( ! ert_ownWatchProcessSignalMethodNil(processWatchedSignalMethod_))
    {
        struct Ert_ProcessSignalName sigName;

        struct Ert_Pid pid = Ert_Pid(aSigInfo->si_pid);
        struct Uid uid = Uid(aSigInfo->si_uid);

        debug(1, "observed %s pid %" PRId_Ert_Pid " uid %" PRId_Uid,
              ert_formatProcessSignalName(&sigName, aSigNum),
              FMTd_Ert_Pid(pid),
              FMTd_Uid(uid));

        ert_callWatchProcessSignalMethod(
            processWatchedSignalMethod_, aSigNum, pid, uid);
    }
}

int
ert_watchProcessSignals(struct Ert_WatchProcessSignalMethod aMethod)
{
    int rc = -1;

    processWatchedSignalMethod_ = aMethod;

    for (unsigned ix = 0; ERT_NUMBEROF(processWatchedSignals_) > ix; ++ix)
    {
        struct SignalWatch *watchedSig = processWatchedSignals_ + ix;

        ERROR_IF(
            changeSigAction_(
                watchedSig->mSigNum,
                (struct sigaction) { .sa_sigaction = caughtSignal_,
                                     .sa_flags     = SA_SIGINFO },
                &watchedSig->mSigAction));

        watchedSig->mWatched = true;
    }

    rc = 0;

Finally:

    FINALLY
    ({
        if (rc)
        {
            for (unsigned ix = 0;
                 ERT_NUMBEROF(processWatchedSignals_) > ix; ++ix)
            {
                struct SignalWatch *watchedSig = processWatchedSignals_ + ix;

                if (watchedSig->mWatched)
                {
                    struct Ert_ProcessSignalName sigName;

                    ABORT_IF(
                        changeSigAction_(
                            watchedSig->mSigNum, watchedSig->mSigAction, 0),
                        {
                            terminate(
                                errno,
                                "Unable to revert action for %s",
                                ert_formatProcessSignalName(
                                    &sigName, watchedSig->mSigNum));
                        });

                    watchedSig->mWatched = false;
                }
            }

            processWatchedSignalMethod_ = Ert_WatchProcessSignalMethodNil();
        }
    });

    return rc;
}

static ERT_CHECKED int
resetProcessSignalsWatch_(void)
{
    int rc  = 0;
    int err = 0;

    for (unsigned ix = 0; ERT_NUMBEROF(processWatchedSignals_) > ix; ++ix)
    {
        struct SignalWatch *watchedSig = processWatchedSignals_ + ix;

        if (watchedSig->mWatched)
        {
            if (changeSigAction_(
                    watchedSig->mSigNum, watchedSig->mSigAction, 0))
            {
                if ( ! rc)
                {
                    rc  = -1;
                    err = errno;
                }

                watchedSig->mWatched = false;
            }
        }
    }

    processWatchedSignalMethod_ = Ert_WatchProcessSignalMethodNil();

    if (rc)
        errno = err;

    return rc;
}

int
ert_unwatchProcessSignals(void)
{
    return resetProcessSignalsWatch_();
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
resetSignals_(void)
{
    int rc = -1;

    ERROR_IF(
        resetProcessSigStop_());
    ERROR_IF(
        resetProcessSigCont_());
    ERROR_IF(
        resetProcessClockWatch_());
    ERROR_IF(
        resetProcessSignalsWatch_());
    ERROR_IF(
        resetProcessChildrenWatch_());

    /* Do not call resetProcessSigPipe_() here since this function is
     * called from forkProcess() and that would mean that the child
     * process would not receive EPIPE on writes to broken pipes. Instead
     * defer the call to execProcess() so that new programs will have
     * SIGPIPE delivered.
     *
     *    if (resetProcessSigPipe_())
     *       goto Finally_;
     */

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_initProcessDirName(struct Ert_ProcessDirName *self, struct Ert_Pid aPid)
{
    int rc = -1;

    ERROR_IF(
        0 > sprintf(self->mDirName,
                    ERT_PROCESS_DIRNAME_FMT_, FMTd_Ert_Pid(aPid)));

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
const char *
ert_formatProcessSignalName(struct Ert_ProcessSignalName *self, int aSigNum)
{
    self->mSignalName = 0;

    if (0 <= aSigNum && ERT_NUMBEROF(signalNames_) > aSigNum)
        self->mSignalName = signalNames_[aSigNum];

    if ( ! self->mSignalName)
    {
        static const char signalErr[] = "signal ?";

        typedef char SignalErrCheckT[
            2 * (sizeof(signalErr) < sizeof(ERT_PROCESS_SIGNALNAME_FMT_)) - 1];

        if (0 > sprintf(self->mSignalText_,
                        ERT_PROCESS_SIGNALNAME_FMT_, aSigNum))
            strcpy(self->mSignalText_, signalErr);
        self->mSignalName = self->mSignalText_;
    }

    return self->mSignalName;
}

/* -------------------------------------------------------------------------- */
struct Ert_ProcessState
ert_fetchProcessState(struct Ert_Pid aPid)
{
    struct Ert_ProcessState rc = { .mState = Ert_ProcessStateError };

    int   statFd  = -1;
    char *statBuf = 0;

    struct Ert_ProcessDirName processDirName;

    static const char processStatFileNameFmt_[] = "%s/stat";

    ERROR_IF(
        ert_initProcessDirName(&processDirName, aPid));

    {
        char processStatFileName[strlen(processDirName.mDirName) +
                                 sizeof(processStatFileNameFmt_)];

        ERROR_IF(
            0 > sprintf(processStatFileName,
                        processStatFileNameFmt_, processDirName.mDirName));

        ERROR_IF(
            (statFd = ert_openFd(processStatFileName, O_RDONLY, 0),
             -1 == statFd));
    }

    ssize_t statlen;
    ERROR_IF(
        (statlen = ert_readFdFully(statFd, &statBuf, 0),
         -1 == statlen));

    char *statend = statBuf + statlen;

    for (char *bufptr = statend; bufptr != statBuf; --bufptr)
    {
        if (')' == bufptr[-1])
        {
            if (1 < statend - bufptr && ' ' == *bufptr)
            {
                switch (bufptr[1])
                {
                default:
                    ERROR_IF(
                        true,
                        {
                            errno = ENOSYS;
                        });

                case 'R': rc.mState = Ert_ProcessStateRunning;  break;
                case 'S': rc.mState = Ert_ProcessStateSleeping; break;
                case 'D': rc.mState = Ert_ProcessStateWaiting;  break;
                case 'Z': rc.mState = Ert_ProcessStateZombie;   break;
                case 'T': rc.mState = Ert_ProcessStateStopped;  break;
                case 't': rc.mState = Ert_ProcessStateTraced;   break;
                case 'X': rc.mState = Ert_ProcessStateDead;     break;
                }
            }
            break;
        }
    }

Finally:

    FINALLY
    ({
        statFd = ert_closeFd(statFd);

        free(statBuf);
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
createProcessLock_(struct ProcessLock *self)
{
    int rc = -1;

    self->mFile = 0;
    self->mLock = 0;

    ERROR_IF(
        ert_temporaryFile(&self->mFile_));
    self->mFile = &self->mFile_;

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
static void
closeProcessLock_(struct ProcessLock *self)
{
    if (self)
        self->mFile = ert_closeFile(self->mFile);
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
lockProcessLock_(struct ProcessLock *self)
{
    int rc = -1;

    ensure( ! self->mLock);

    ERROR_IF(
        ert_lockFileRegion(self->mFile, Ert_LockTypeWrite, 0, 0));

    self->mLock = &Ert_LockTypeWrite;

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
static void
unlockProcessLock_(struct ProcessLock *self)
{
    ensure(self->mLock);

    ABORT_IF(
        ert_unlockFileRegion(self->mFile, 0, 0));

    self->mLock = 0;
}

/* -------------------------------------------------------------------------- */
static void
forkProcessLock_(struct ProcessLock *self)
{
    if (self->mLock)
    {
        ABORT_IF(
            ert_unlockFileRegion(self->mFile, 0, 0));
        ABORT_IF(
            ert_lockFileRegion(self->mFile, Ert_LockTypeWrite, 0, 0));
    }
}

/* -------------------------------------------------------------------------- */
int
ert_acquireProcessAppLock(void)
{
    int rc = -1;

    struct Ert_ThreadSigMutex *lock =
        ert_lockThreadSigMutex(processLock_.mMutex);

    if (1 == ert_ownThreadSigMutexLocked(processLock_.mMutex))
    {
        if (processLock_.mLock)
            ERROR_IF(
                lockProcessLock_(processLock_.mLock));
    }

    rc = 0;

Finally:

    FINALLY
    ({
        if (rc)
            lock = ert_unlockThreadSigMutex(lock);
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_releaseProcessAppLock(void)
{
    int rc = -1;

    struct Ert_ThreadSigMutex *lock = processLock_.mMutex;

    if (1 == ert_ownThreadSigMutexLocked(lock))
    {
        if (processLock_.mLock)
            unlockProcessLock_(processLock_.mLock);
    }

    rc = 0;

Finally:

    FINALLY
    ({
        lock = ert_unlockThreadSigMutex(lock);
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
struct Ert_ProcessAppLock *
ert_createProcessAppLock(void)
{
    struct Ert_ProcessAppLock *self = &processAppLock_;

    ABORT_IF(
        ert_acquireProcessAppLock(),
        {
            terminate(errno, "Unable to acquire application lock");
        });

    return self;
}

/* -------------------------------------------------------------------------- */
struct Ert_ProcessAppLock *
ert_destroyProcessAppLock(struct Ert_ProcessAppLock *self)
{
    if (self)
    {
        ensure(&processAppLock_ == self);

        ABORT_IF(
            ert_releaseProcessAppLock(),
            {
                terminate(errno, "Unable to release application lock");
            });
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
unsigned
ert_ownProcessAppLockCount(void)
{
    return ert_ownThreadSigMutexLocked(processLock_.mMutex);
}

/* -------------------------------------------------------------------------- */
const struct Ert_File *
ert_ownProcessAppLockFile(const struct Ert_ProcessAppLock *self)
{
    ensure(&processAppLock_ == self);

    return processLock_.mLock ? processLock_.mLock->mFile : 0;
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED struct Ert_ProcessForkChildLock_ *
ert_acquireProcessForkChildLock_(struct Ert_ProcessForkChildLock_ *self)
{
    struct Ert_Tid tid = ert_ownThreadId();
    struct Ert_Pid pid = ert_ownProcessId();

    ensure( ! self->mProcess.mPid || self->mProcess.mPid == pid.mPid);

    if (self->mThread.mTid == tid.mTid)
        ++self->mCount;
    else
    {
        pthread_mutex_t *lock = ert_lockMutex(&self->mMutex_);

        self->mMutex   = lock;
        self->mThread  = tid;
        self->mProcess = pid;

        ensure( ! self->mCount);

        ++self->mCount;
    }

    return self;
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED struct Ert_ProcessForkChildLock_ *
ert_relinquishProcessForkChildLock_(struct Ert_ProcessForkChildLock_ *self,
                                unsigned                      aRelease)
{
    if (self)
    {
        struct Ert_Tid tid = ert_ownThreadId();
        struct Ert_Pid pid = ert_ownProcessId();

        ensure(self->mCount);
        ensure(self->mMutex == &self->mMutex_);

        /* Note that the owning tid will not match in the main thread
         * of the child process (which will have a different tid) after
         * the lock is acquired and the parent process is forked. */

        ensure(self->mProcess.mPid);

        if (self->mProcess.mPid == pid.mPid)
            ensure(self->mThread.mTid == tid.mTid);
        else
        {
            self->mProcess = pid;
            self->mThread  = tid;
        }

        ensure(aRelease <= self->mCount);

        self->mCount -= aRelease;

        if ( ! self->mCount)
        {
            pthread_mutex_t *lock = self->mMutex;

            self->mThread = Ert_Tid(0);
            self->mMutex  = 0;

            lock = ert_unlockMutex(lock);
        }
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED struct Ert_ProcessForkChildLock_ *
ert_releaseProcessForkChildLock_(struct Ert_ProcessForkChildLock_ *self)
{
    return ert_relinquishProcessForkChildLock_(self, 1);
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED struct Ert_ProcessForkChildLock_ *
ert_resetProcessForkChildLock_(struct Ert_ProcessForkChildLock_ *self)
{
    return ert_relinquishProcessForkChildLock_(self, self->mCount);
}

/* -------------------------------------------------------------------------- */
int
ert_reapProcessChild(struct Ert_Pid aPid, int *aStatus)
{
    int rc = -1;

    ERROR_IF(
        -1 == aPid.mPid || ! aPid.mPid,
        {
            errno = EINVAL;
        });

    pid_t pid;

    do
    {
        ERROR_IF(
            (pid = waitpid(aPid.mPid, aStatus, __WALL),
             -1 == pid && EINTR != errno));
    } while (pid != aPid.mPid);

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
static int
waitProcessChild_(const siginfo_t *aSigInfo)
{
    int rc = Ert_ChildProcessStateError;

    switch (aSigInfo->si_code)
    {
    default:
        break;

    case CLD_EXITED: rc = Ert_ChildProcessStateExited;  break;
    case CLD_KILLED: rc = Ert_ChildProcessStateKilled;  break;
    case CLD_DUMPED: rc = Ert_ChildProcessStateDumped;  break;
    }

Finally:

    FINALLY({});

    return rc;
}

struct Ert_ChildProcessState
ert_waitProcessChild(struct Ert_Pid aPid)
{
    struct Ert_ChildProcessState rc =
        { .mChildState = Ert_ChildProcessStateError };

    ERROR_IF(
        -1 == aPid.mPid || ! aPid.mPid,
        {
            errno = EINVAL;
        });

    siginfo_t siginfo;

    do
    {
        siginfo.si_pid = 0;
        ERROR_IF(
            waitid(P_PID, aPid.mPid, &siginfo, WEXITED | WNOWAIT) &&
            EINTR != errno);

    } while (siginfo.si_pid != aPid.mPid);

    rc.mChildStatus = siginfo.si_status;

    ERROR_IF(
        (rc.mChildState = waitProcessChild_(&siginfo),
         Ert_ChildProcessStateError == rc.mChildState),
        {
            errno = EINVAL;
        });

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
struct Ert_ChildProcessState
ert_monitorProcessChild(struct Ert_Pid aPid)
{
    struct Ert_ChildProcessState rc =
        { .mChildState = Ert_ChildProcessStateError };

    siginfo_t siginfo;

    while (1)
    {
        siginfo.si_pid = 0;

        int waitErr;
        ERROR_IF(
            (waitErr = waitid(P_PID, aPid.mPid, &siginfo,
                   WEXITED | WSTOPPED | WCONTINUED | WNOHANG | WNOWAIT),
            waitErr && EINTR != errno));
        if (waitErr)
            continue;

        break;
    }

    if (siginfo.si_pid != aPid.mPid)
        rc.mChildState = Ert_ChildProcessStateRunning;
    else
    {
        rc.mChildStatus = siginfo.si_status;

        switch (siginfo.si_code)
        {
        default:
            ERROR_IF(
                true,
                {
                    errno = EINVAL;
                });

        case CLD_EXITED:
            rc.mChildState = Ert_ChildProcessStateExited;  break;
        case CLD_KILLED:
            rc.mChildState = Ert_ChildProcessStateKilled;  break;
        case CLD_DUMPED:
            rc.mChildState = Ert_ChildProcessStateDumped;  break;
        case CLD_STOPPED:
            rc.mChildState = Ert_ChildProcessStateStopped; break;
        case CLD_TRAPPED:
            rc.mChildState = Ert_ChildProcessStateTrapped; break;
        case CLD_CONTINUED:
            rc.mChildState = Ert_ChildProcessStateRunning; break;
        }
    }

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
struct Ert_Pid
ert_waitProcessChildren()
{
    int rc = -1;

    struct Ert_Pid pid;

    while (1)
    {
        siginfo_t siginfo;

        siginfo.si_pid = 0;

        pid_t waitErr;
        ERROR_IF(
            (waitErr = waitid(P_ALL, 0, &siginfo, WEXITED | WNOWAIT),
             waitErr && EINTR != errno && ECHILD != errno));
        if (waitErr && EINTR == errno)
            continue;

        if (waitErr)
            pid = Ert_Pid(0);
        else
        {
            ensure(siginfo.si_pid);
            pid = Ert_Pid(siginfo.si_pid);
        }

        break;
    }

    rc = 0;

Finally:

    FINALLY({});

    return rc ? Ert_Pid(-1) : pid;
}

/* -------------------------------------------------------------------------- */
struct ForkProcessResult_
{
    int mReturnCode;
    int mErrCode;
};

struct ForkProcessChannel_
{
    struct ForkProcessChannel_  *mPrev;
    struct ForkProcessChannel_ **mList;

    struct Ert_Pipe  mResultPipe_;
    struct Ert_Pipe *mResultPipe;

    struct Ert_BellSocketPair  mResultSocket_;
    struct Ert_BellSocketPair *mResultSocket;
};

static ERT_CHECKED struct ForkProcessChannel_ *
closeForkProcessChannel_(
    struct ForkProcessChannel_ *self)
{
    if (self)
    {
        *self->mList = self->mPrev;
        self->mPrev  = 0;

        self->mResultSocket = ert_closeBellSocketPair(self->mResultSocket);
        self->mResultPipe   = ert_closePipe(self->mResultPipe);
    }

    return 0;
}

static void
closeForkProcessChannelResultChild_(
    struct ForkProcessChannel_ *self)
{
    ert_closePipeWriter(self->mResultPipe);
    ert_closeBellSocketPairChild(self->mResultSocket);
}

static void
closeForkProcessChannelResultParent_(
    struct ForkProcessChannel_ *self)
{
    ert_closePipeReader(self->mResultPipe);
    ert_closeBellSocketPairParent(self->mResultSocket);
}

static ERT_CHECKED int
createForkProcessChannel_(
    struct ForkProcessChannel_ *self, struct ForkProcessChannel_ **aList)
{
    int rc = -1;

    struct Ert_StdFdFiller  stdFdFiller_;
    struct Ert_StdFdFiller *stdFdFiller = 0;

    self->mResultPipe   = 0;
    self->mResultSocket = 0;
    self->mList         = aList;

    self->mPrev  = *aList;
    *self->mList = self;

    /* Prevent the fork communication channels from inadvertently becoming
     * stdin, stdout or stderr. */

    ERROR_IF(
        ert_createStdFdFiller(&stdFdFiller_));
    stdFdFiller = &stdFdFiller_;

    ERROR_IF(
        ert_createPipe(&self->mResultPipe_, O_CLOEXEC));
    self->mResultPipe = &self->mResultPipe_;

    ERROR_IF(
        ert_createBellSocketPair(&self->mResultSocket_, O_CLOEXEC));
    self->mResultSocket = &self->mResultSocket_;

    rc = 0;

Finally:

    FINALLY
    ({
        if (rc)
            self = closeForkProcessChannel_(self);

        stdFdFiller = ert_closeStdFdFiller(stdFdFiller);
    });

    return rc;
}

static ERT_CHECKED int
includeForkProcessChannelFdSet_(
    const struct ForkProcessChannel_ *self,
    struct Ert_FdSet                     *aFdSet)
{
    int rc = -1;

    struct Ert_File *filelist[] =
    {
        self->mResultPipe->mRdFile,
        self->mResultPipe->mWrFile,

        self->mResultSocket->mSocketPair->mChildSocket ?
        self->mResultSocket->mSocketPair->mChildSocket->mSocket->mFile : 0,

        self->mResultSocket->mSocketPair->mParentSocket ?
        self->mResultSocket->mSocketPair->mParentSocket->mSocket->mFile : 0,
    };

    for (unsigned ix = 0; ERT_NUMBEROF(filelist) > ix; ++ix)
    {
        if (filelist[ix])
            ERROR_IF(
                ert_insertFdSetFile(aFdSet, filelist[ix]) &&
                EEXIST != errno);
    }

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

static ERT_CHECKED int
excludeForkProcessChannelFdSet_(
    const struct ForkProcessChannel_ *self,
    struct Ert_FdSet                     *aFdSet)
{
    int rc = -1;

    struct Ert_File *filelist[] =
    {
        self->mResultPipe->mRdFile,
        self->mResultPipe->mWrFile,

        self->mResultSocket->mSocketPair->mChildSocket ?
        self->mResultSocket->mSocketPair->mChildSocket->mSocket->mFile : 0,

        self->mResultSocket->mSocketPair->mParentSocket ?
        self->mResultSocket->mSocketPair->mParentSocket->mSocket->mFile : 0,
    };

    for (unsigned ix = 0; ERT_NUMBEROF(filelist) > ix; ++ix)
    {
        if (filelist[ix])
            ERROR_IF(
                ert_removeFdSetFile(aFdSet, filelist[ix]) &&
                ENOENT != errno);
    }

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

static ERT_CHECKED int
sendForkProcessChannelResult_(
    struct ForkProcessChannel_      *self,
    const struct ForkProcessResult_ *aResult)
{
    int rc = -1;

    ERROR_IF(
        sizeof(*aResult) != ert_writeFile(
            self->mResultPipe->mWrFile, (char *) aResult, sizeof(*aResult), 0));

    ERROR_IF(
        ert_ringBellSocketPairChild(self->mResultSocket));

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

static ERT_CHECKED int
recvForkProcessChannelResult_(
    struct ForkProcessChannel_ *self,
    struct ForkProcessResult_  *aResult)
{
    int rc = -1;

    ERROR_IF(
        ert_waitBellSocketPairParent(self->mResultSocket, 0));

    ERROR_IF(
        sizeof(*aResult) != ert_readFile(
            self->mResultPipe->mRdFile, (char *) aResult, sizeof(*aResult), 0));

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

static ERT_CHECKED int
recvForkProcessChannelAcknowledgement_(
    struct ForkProcessChannel_ *self)
{
    int rc = -1;

    ERROR_IF(
        ert_waitBellSocketPairChild(self->mResultSocket, 0));

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

static ERT_CHECKED int
sendForkProcessChannelAcknowledgement_(
    struct ForkProcessChannel_ *self)
{
    return ert_ringBellSocketPairParent(self->mResultSocket);
}

/* -------------------------------------------------------------------------- */
static void
callForkMethod_(struct Ert_ForkProcessMethod aMethod) ERT_NORETURN;

static void
callForkMethod_(struct Ert_ForkProcessMethod aMethod)
{
    int status = EXIT_SUCCESS;

    if ( ! ert_ownForkProcessMethodNil(aMethod))
    {
        ABORT_IF(
            (status = ert_callForkProcessMethod(aMethod),
             -1 == status || (errno = 0, 0 > status || 255 < status)),
            {
                if (-1 != status)
                    terminate(
                        0,
                        "Out of range exit status %d", status);
            });
    }

    /* When running with valgrind, use execl() to prevent
     * valgrind performing a leak check on the temporary
     * process. */

    if (RUNNING_ON_VALGRIND)
    {
        static const char exitCmdFmt[] = "exit %d";

        char exitCmd[sizeof(exitCmdFmt) + sizeof(status) * CHAR_BIT];

        ABORT_IF(
            0 > sprintf(exitCmd, exitCmdFmt, status),
            {
                terminate(
                    errno,
                    "Unable to format exit status command: %d", status);
            });

        ABORT_IF(
            (execl("/bin/sh", "sh", "-c", exitCmd, (char *) 0), true),
            {
                terminate(
                    errno,
                    "Unable to exit process: %s", exitCmd);
            });
    }

    ert_exitProcess(status);
}

static ERT_CHECKED int
forkProcessChild_PostParent_(
    struct ForkProcessChannel_            *self,
    enum Ert_ForkProcessOption             aOption,
    struct Ert_Pid                         aChildPid,
    struct Ert_Pgid                        aChildPgid,
    struct Ert_PostForkParentProcessMethod aPostForkParentMethod,
    struct Ert_FdSet                      *aBlacklistFds)
{
    int rc = -1;

    struct Ert_StdFdFiller  stdFdFiller_;
    struct Ert_StdFdFiller *stdFdFiller = 0;

    /* Forcibly set the process group of the child to avoid
     * the race that would occur if only the child attempts
     * to set its own process group */

    if (Ert_ForkProcessSetProcessGroup == aOption)
        ERROR_IF(
            setpgid(aChildPid.mPid,
                    aChildPgid.mPgid ? aChildPgid.mPgid : aChildPid.mPid));

    /* On Linux, struct Ert_PidSignature uses the process start
     * time from /proc/pid/stat, but that start time is measured
     * in _SC_CLK_TCK periods which limits the rate at which
     * processes can be forked without causing ambiguity. Although
     * this ambiguity is largely theoretical, it is a simple matter
     * to overcome by constraining the rate at which processes can
     * fork. */

#ifdef __linux__
    {
        long clocktick;
        ERROR_IF(
            (clocktick = sysconf(_SC_CLK_TCK),
             -1 == clocktick));

        ert_monotonicSleep(
            Ert_Duration(
                Ert_NanoSeconds(Ert_TimeScale_ns / clocktick * 5 / 4)));
    }
#endif

    /* Sequence fork operations with the child so that the actions
     * are completely synchronised. The actions are performed in
     * the following order:
     *
     *      o Close all but whitelisted fds in child
     *      o Run child post fork method
     *      o Run parent post fork method
     *      o Close only blacklisted fds in parent
     *      o Continue both parent and child process
     *
     * The blacklisted fds in the parent will only be closed if there
     * are no errors detected in either the child or the parent
     * post fork method. This provides the parent with the potential
     * to retry the operation. */

    closeForkProcessChannelResultChild_(self);

    for (struct ForkProcessChannel_ *processChannel = self;
         processChannel;
         processChannel = processChannel->mPrev)
    {
        ERROR_IF(
            excludeForkProcessChannelFdSet_(processChannel, aBlacklistFds));
    }

    {
        struct ForkProcessResult_ forkResult;

        ERROR_IF(
            recvForkProcessChannelResult_(self, &forkResult));

        ERROR_IF(
            forkResult.mReturnCode,
            {
                errno = forkResult.mErrCode;
            });

        ERROR_IF(
            ! ert_ownPostForkParentProcessMethodNil(aPostForkParentMethod) &&
            ert_callPostForkParentProcessMethod(
                aPostForkParentMethod, aChildPid));

        /* Always remove the stdin, stdout and stderr from the blacklisted
         * file descriptors. If they were indeed blacklisted, then for stdin
         * and stdout, close and replace each so that the descriptors themselves
         * remain valid, but leave stderr intact. */

        {
            ERROR_IF(
                ert_createStdFdFiller(&stdFdFiller_));
            stdFdFiller = &stdFdFiller_;

            const struct
            {
                int  mFd;
                int  mAltFd;

            } stdfdlist[] = {
                { .mFd = STDIN_FILENO,
                  .mAltFd = stdFdFiller->mFile[0]->mFd },

                { .mFd = STDOUT_FILENO,
                  .mAltFd = stdFdFiller->mFile[1]->mFd },

                { .mFd = STDERR_FILENO,
                  .mAltFd = -1 },
            };

            for (unsigned ix = 0; ERT_NUMBEROF(stdfdlist) > ix; ++ix)
            {
                int fd    = stdfdlist[ix].mFd;
                int altFd = stdfdlist[ix].mAltFd;

                int err;
                ERROR_IF(
                    (err = ert_removeFdSet(aBlacklistFds, fd),
                     err && ENOENT != errno));

                if ( ! err && -1 != altFd)
                    ERROR_IF(
                        fd != ert_duplicateFd(altFd, fd));
            }

            stdFdFiller = ert_closeStdFdFiller(stdFdFiller);
        }

        ERROR_IF(
            ert_closeFdOnlyBlackList(aBlacklistFds));

        ERROR_IF(
            sendForkProcessChannelAcknowledgement_(self));
    }

    rc = 0;

Finally:

    FINALLY
    ({
        stdFdFiller = ert_closeStdFdFiller(stdFdFiller);
    });

    return rc;
}

static void
forkProcessChild_PostChild_(
    struct ForkProcessChannel_            *self,
    enum Ert_ForkProcessOption             aOption,
    struct Ert_Pgid                        aChildPgid,
    struct Ert_PostForkChildProcessMethod  aPostForkChildMethod,
    struct Ert_FdSet                      *aWhitelistFds)
{
    int rc = -1;

    /* Detach this instance from its recursive ancestors if any because
     * the child process will only interact with its direct parent. */

    self->mPrev = 0;

    /* Ensure that the behaviour of each child diverges from the
     * behaviour of the parent. This is primarily useful for
     * testing. */

    ert_scrambleRandomSeed(ert_ownProcessId().mPid);

    if (Ert_ForkProcessSetSessionLeader == aOption)
    {
        ERROR_IF(
            -1 == setsid());
    }
    else if (Ert_ForkProcessSetProcessGroup == aOption)
    {
        ERROR_IF(
            setpgid(0, aChildPgid.mPgid));
    }

    /* Reset all the signals so that the child will not attempt
     * to catch signals. The parent should have set the signal
     * mask appropriately. */

    ERROR_IF(
        resetSignals_());

    closeForkProcessChannelResultParent_(self);

    ERROR_IF(
        includeForkProcessChannelFdSet_(self, aWhitelistFds));

    ERROR_IF(
        ert_closeFdExceptWhiteList(aWhitelistFds));

    ERROR_IF(
        ! ert_ownPostForkChildProcessMethodNil(aPostForkChildMethod) &&
        ert_callPostForkChildProcessMethod(aPostForkChildMethod));

    {
        /* Send the child fork process method result to the parent so
         * that it can return an error code to the caller, then wait
         * for the parent to acknowledge. */

        struct ForkProcessResult_ forkResult =
        {
            .mReturnCode = 0,
            .mErrCode    = 0,
        };

        if (sendForkProcessChannelResult_(self, &forkResult) ||
            recvForkProcessChannelAcknowledgement_(self))
        {
            /* Terminate the child if the result could not be sent. The
             * parent will detect the child has terminated because the
             * result socket will close.
             *
             * Also terminate the child if the acknowledgement could not
             * be received from the parent. This will typically be because
             * the parent has terminated without sending the
             * acknowledgement, so it is reasonable to simply terminate
             * the child. */

            ert_exitProcess(EXIT_FAILURE);
        }
    }

    rc = 0;

Finally:

    if (rc)
    {
        /* This is the error path running in the child. After attempting
         * to send the error indication, simply terminate the child.
         * The parent should either receive the failure indication, or
         * detect that the child has terminated before sending the
         * error indication. */

        struct ForkProcessResult_ forkResult =
        {
            .mReturnCode = rc,
            .mErrCode    = errno,
        };

        while (
            sendForkProcessChannelResult_(self, &forkResult) ||
            recvForkProcessChannelAcknowledgement_(self))
        {
            break;
        }

        ert_exitProcess(EXIT_FAILURE);
    }
}

struct Ert_Pid
ert_forkProcessChild(
    enum Ert_ForkProcessOption             aOption,
    struct Ert_Pgid                        aPgid,
    struct Ert_PreForkProcessMethod        aPreForkMethod,
    struct Ert_PostForkChildProcessMethod  aPostForkChildMethod,
    struct Ert_PostForkParentProcessMethod aPostForkParentMethod,
    struct Ert_ForkProcessMethod           aForkMethod)
{
    pid_t rc = -1;

    struct Ert_Pid childPid = Ert_Pid(-1);

    struct Ert_FdSet  blacklistFds_;
    struct Ert_FdSet *blacklistFds = 0;

    struct Ert_FdSet  whitelistFds_;
    struct Ert_FdSet *whitelistFds = 0;

    struct ForkProcessChannel_  forkChannel_;
    struct ForkProcessChannel_ *forkChannel = 0;

    /* Acquire the processForkLock_ so that other threads that issue
     * a raw fork() will synchronise with this code via the pthread_atfork()
     * handler. */

    struct Ert_ProcessForkChildLock_ *forkLock =
        ert_acquireProcessForkChildLock_(&processForkChildLock_);

    ensure(Ert_ForkProcessSetProcessGroup == aOption || ! aPgid.mPgid);

    ERROR_IF(
        ert_createFdSet(&blacklistFds_));
    blacklistFds = &blacklistFds_;

    ERROR_IF(
        ert_createFdSet(&whitelistFds_));
    whitelistFds = &whitelistFds_;

    ERROR_IF(
        ! ert_ownPreForkProcessMethodNil(aPreForkMethod) &&
        ert_callPreForkProcessMethod(
            aPreForkMethod,
            & (struct Ert_PreForkProcess)
            {
                .mBlacklistFds = blacklistFds,
                .mWhitelistFds = whitelistFds,
            }));

    /* Do not open the fork channel until after the pre fork method
     * has been run so that these additional file descriptors are
     * not visible to that method. */

    ERROR_IF(
        createForkProcessChannel_(&forkChannel_, &forkLock->mChannelList));
    forkChannel = &forkChannel_;

    /* Always include stdin, stdout and stderr in the whitelisted
     * fds for the child, and never include these in the blacklisted
     * fds for the parent. If required, the child and the parent can
     * close these in their post fork methods. */

    const int stdfdlist[] =
    {
        STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO,
    };

    for (unsigned ix = 0; ERT_NUMBEROF(stdfdlist) > ix; ++ix)
    {
        ERROR_IF(
            ert_insertFdSet(whitelistFds, stdfdlist[ix]) && EEXIST != errno);

        ERROR_IF(
            ert_removeFdSet(blacklistFds, stdfdlist[ix]) && ENOENT != errno);
    }

    if (processLock_.mLock)
    {
        ERROR_IF(
            ert_insertFdSetFile(whitelistFds,
                            processLock_.mLock->mFile) && EEXIST != errno);
        ERROR_IF(
            ert_removeFdSetFile(blacklistFds,
                            processLock_.mLock->mFile) && ENOENT != errno);
    }

    /* Note that the fork() will complete and launch the child process
     * before the child pid is recorded in the local variable. This
     * is an important consideration for propagating signals to
     * the child process. */

    ERT_TEST_RACE
    ({
        childPid = Ert_Pid(fork());
    });

    switch (childPid.mPid)
    {
    default:
        ERROR_IF(
            forkProcessChild_PostParent_(
                forkChannel,
                aOption,
                childPid,
                aPgid,
                aPostForkParentMethod,
                blacklistFds));
        break;

    case -1:
        break;

    case 0:
        {
            /* At the first chance, reset the fork lock so that
             * its internal state can be synchronised to the new
             * child process. This allows the code in the
             * PostForkChildProcessMethod to invoke
             * ert_acquireProcessForkChildLock_() without triggering
             * assertions. */

            struct Ert_ProcessForkChildLock_ *childForkLock = forkLock;

            forkLock = ert_resetProcessForkChildLock_(forkLock);
            forkLock = ert_acquireProcessForkChildLock_(childForkLock);
        }

        forkProcessChild_PostChild_(
                forkChannel,
                aOption,
                aPgid,
                aPostForkChildMethod,
                whitelistFds);

        forkChannel = closeForkProcessChannel_(forkChannel);
        forkLock    = ert_releaseProcessForkChildLock_(forkLock);

        callForkMethod_(aForkMethod);

        ensure(0);
        break;
    }

    rc = childPid.mPid;

Finally:

    FINALLY
    ({
        forkChannel  = closeForkProcessChannel_(forkChannel);
        blacklistFds = ert_closeFdSet(blacklistFds);
        whitelistFds = ert_closeFdSet(whitelistFds);

        forkLock = ert_releaseProcessForkChildLock_(forkLock);

        if (-1 == rc)
        {
            /* If the parent has successfully created a child process, but
             * there is a problem, then the child needs to be reaped. */

            if (-1 != childPid.mPid)
            {
                int status;
                ABORT_IF(
                    ert_reapProcessChild(childPid, &status));
            }
        }
    });

    return Ert_Pid(rc);
}

/* -------------------------------------------------------------------------- */
struct ForkProcessDaemon
{
    struct Ert_PreForkProcessMethod       mPreForkMethod;
    struct Ert_PostForkChildProcessMethod mChildMethod;
    struct Ert_ForkProcessMethod          mForkMethod;
    struct Ert_SocketPair                *mSyncSocket;
    struct Ert_ThreadSigMask             *mSigMask;
};

struct ForkProcessDaemonSigHandler
{
    unsigned mHangUp;
};

static ERT_CHECKED int
forkProcessDaemonSignalHandler_(
    struct ForkProcessDaemonSigHandler *self,
    int                                 aSigNum,
    struct Ert_Pid                          aPid,
    struct Uid                          aUid)
{
    ++self->mHangUp;

    struct Ert_ProcessSignalName sigName;
    debug(1,
          "daemon received %s pid %" PRId_Ert_Pid " uid %" PRId_Uid,
          ert_formatProcessSignalName(&sigName, aSigNum),
          FMTd_Ert_Pid(aPid),
          FMTd_Uid(aUid));

    return 0;
}

static int
forkProcessDaemonChild_(struct ForkProcessDaemon *self)
{
    int rc = -1;

    struct Ert_Pid daemonPid = ert_ownProcessId();

    struct ForkProcessDaemonSigHandler sigHandler = { .mHangUp = 0 };

    ERROR_IF(
        ert_watchProcessSignals(
            Ert_WatchProcessSignalMethod(
                &sigHandler, &forkProcessDaemonSignalHandler_)));

    /* Once the signal handler is established to catch SIGHUP, allow
     * the parent to stop and then make the daemon process an orphan. */

    ERROR_IF(
        ert_waitThreadSigMask((const int []) { SIGHUP, 0 }));

    self->mSigMask = ert_popThreadSigMask(self->mSigMask);

    debug(0, "daemon orphaned");

    ERROR_UNLESS(
        sizeof(daemonPid.mPid) == sendUnixSocket(
            self->mSyncSocket->mChildSocket,
            (void *) &daemonPid.mPid,
            sizeof(daemonPid.mPid)));

    char buf[1];
    ERROR_UNLESS(
        sizeof(buf) == recvUnixSocket(
            self->mSyncSocket->mChildSocket, buf, sizeof(buf)));

    self->mSyncSocket = ert_closeSocketPair(self->mSyncSocket);

    if ( ! ert_ownForkProcessMethodNil(self->mForkMethod))
        ERROR_IF(
            ert_callForkProcessMethod(self->mForkMethod));

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

static int
forkProcessDaemonGuardian_(struct ForkProcessDaemon *self)
{
    int rc = -1;

    ert_closeSocketPairParent(self->mSyncSocket);

    struct Ert_Pid daemonPid;
    ERROR_IF(
        (daemonPid = ert_forkProcessChild(
            Ert_ForkProcessSetProcessGroup,
            Ert_Pgid(0),
            Ert_PreForkProcessMethod(
                self,
                ERT_LAMBDA(
                    int, (struct ForkProcessDaemon    *self_,
                          const struct Ert_PreForkProcess *aPreFork),
                    {
                        return ert_fillFdSet(aPreFork->mWhitelistFds);
                    })),
            self->mChildMethod,
            Ert_PostForkParentProcessMethodNil(),
            Ert_ForkProcessMethod(
                self, forkProcessDaemonChild_)),
         -1 == daemonPid.mPid));

    /* Terminate the server to make the child an orphan. The child
     * will become the daemon, and when it is adopted by init(8).
     *
     * When a parent process terminates, Posix says:
     *
     *    o If the process is a controlling process, the SIGHUP signal
     *      shall be sent to each process in the foreground process group
     *      of the controlling terminal belonging to the calling process.
     *
     *    o If the exit of the process causes a process group to become
     *      orphaned, and if any member of the newly-orphaned process
     *      group is stopped, then a SIGHUP signal followed by a
     *      SIGCONT signal shall be sent to each process in the
     *      newly-orphaned process group.
     *
     * The server created here is not a controlling process since it is
     * not a session leader (although it might have a controlling terminal).
     * So no SIGHUP is sent for the first reason.
     *
     * To avoid ambiguity, the child is always placed into its own
     * process group and stopped, so that when it is orphaned it is
     * guaranteed to receive a SIGHUP signal. */

    while (1)
    {
        ERROR_IF(
            kill(daemonPid.mPid, SIGSTOP));

        ert_monotonicSleep(Ert_Duration(ERT_NSECS(Ert_MilliSeconds(100))));

        struct Ert_ChildProcessState daemonStatus =
            ert_monitorProcessChild(daemonPid);
        if (Ert_ChildProcessStateStopped == daemonStatus.mChildState)
            break;

        ert_monotonicSleep(Ert_Duration(ERT_NSECS(Ert_Seconds(1))));
    }

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

static int
forkProcessDaemonPreparation_(struct ForkProcessDaemon    *self,
                              const struct Ert_PreForkProcess *aPreFork)
{
    int rc = -1;

    if ( ! ert_ownPreForkProcessMethodNil(self->mPreForkMethod))
        ERROR_IF(
            ert_callPreForkProcessMethod(self->mPreForkMethod, aPreFork));

    ERROR_IF(
        ert_removeFdSet(
            aPreFork->mBlacklistFds,
            self->mSyncSocket->mParentSocket->mSocket->mFile->mFd) &&
        ENOENT != errno);

    ERROR_IF(
        ert_insertFdSet(
            aPreFork->mWhitelistFds,
            self->mSyncSocket->mParentSocket->mSocket->mFile->mFd) &&
        EEXIST != errno);

    ERROR_IF(
        ert_removeFdSet(
            aPreFork->mBlacklistFds,
            self->mSyncSocket->mChildSocket->mSocket->mFile->mFd) &&
        ENOENT != errno);

    ERROR_IF(
        ert_insertFdSet(
            aPreFork->mWhitelistFds,
            self->mSyncSocket->mChildSocket->mSocket->mFile->mFd) &&
        EEXIST != errno);

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

struct Ert_Pid
ert_forkProcessDaemon(
    struct Ert_PreForkProcessMethod        aPreForkMethod,
    struct Ert_PostForkChildProcessMethod  aPostForkChildMethod,
    struct Ert_PostForkParentProcessMethod aPostForkParentMethod,
    struct Ert_ForkProcessMethod           aForkMethod)
{
    pid_t rc = -1;

    struct Ert_ThreadSigMask  sigMask_;
    struct Ert_ThreadSigMask *sigMask = 0;

    struct Ert_SocketPair  syncSocket_;
    struct Ert_SocketPair *syncSocket = 0;

    sigMask = ert_pushThreadSigMask(
        &sigMask_, Ert_ThreadSigMaskBlock, (const int []) { SIGHUP, 0 });

    ERROR_IF(
        ert_createSocketPair(&syncSocket_, O_CLOEXEC));
    syncSocket = &syncSocket_;

    struct ForkProcessDaemon daemonProcess =
    {
        .mSigMask       = sigMask,
        .mPreForkMethod = aPreForkMethod,
        .mChildMethod   = aPostForkChildMethod,
        .mForkMethod    = aForkMethod,
        .mSyncSocket    = syncSocket,
    };

    struct Ert_Pid serverPid;
    ERROR_IF(
        (serverPid = ert_forkProcessChild(
            Ert_ForkProcessInheritProcessGroup,
            Ert_Pgid(0),
            Ert_PreForkProcessMethod(
                &daemonProcess, forkProcessDaemonPreparation_),
            Ert_PostForkChildProcessMethod(
                &daemonProcess, forkProcessDaemonGuardian_),
            Ert_PostForkParentProcessMethod(
                &daemonProcess,
                ERT_LAMBDA(
                    int, (struct ForkProcessDaemon *self_,
                          struct Ert_Pid                aGuardianPid),
                    {
                        ert_closeSocketPairChild(self_->mSyncSocket);
                        return 0;
                    })),
            Ert_ForkProcessMethodNil()),
         -1 == serverPid.mPid));

    int status;
    ERROR_IF(
        ert_reapProcessChild(serverPid, &status));

    ERROR_UNLESS(
        WIFEXITED(status) && ! WEXITSTATUS(status));

    struct Ert_Pid daemonPid;
    ERROR_UNLESS(
        sizeof(daemonPid.mPid) == recvUnixSocket(syncSocket->mParentSocket,
                                                 (void *) &daemonPid.mPid,
                                                 sizeof(daemonPid.mPid)));

    char buf[1] = { 0 };
    ERROR_UNLESS(
        sizeof(buf) == sendUnixSocket(
            syncSocket->mParentSocket, buf, sizeof(buf)));

    rc = daemonPid.mPid;

Finally:

    FINALLY
    ({
        syncSocket = ert_closeSocketPair(syncSocket);

        sigMask = ert_popThreadSigMask(sigMask);
    });

    return Ert_Pid(rc);
}

/* -------------------------------------------------------------------------- */
void
ert_execProcess(const char *aCmd, const char * const *aArgv)
{
    /* Call resetProcessSigPipe_() here to ensure that SIGPIPE will be
     * delivered to the new program. Note that it is possible that there
     * was no previous call to forkProcess(), though this is normally
     * the case. */

    ERROR_IF(
        resetProcessSigPipe_());

    ERROR_IF(
        errno = pthread_sigmask(SIG_SETMASK, &processSigMask_, 0));

    execvp(aCmd, (char * const *) aArgv);

Finally:

    FINALLY({});
}

/* -------------------------------------------------------------------------- */
void
ert_execShell(const char *aCmd)
{
    const char *shell = getenv("SHELL");

    if ( ! shell)
        shell = "/bin/sh";

    const char *cmd[] = { shell, "-c", aCmd, 0 };

    ert_execProcess(shell, cmd);
}

/* -------------------------------------------------------------------------- */
void
ert_exitProcess(int aStatus)
{
    _exit(aStatus);

    while (1)
        raise(SIGKILL);
}

/* -------------------------------------------------------------------------- */
int
ert_signalProcessGroup(struct Ert_Pgid aPgid, int aSignal)
{
    int rc = -1;

    struct Ert_ProcessSignalName sigName;

    ensure(aPgid.mPgid);

    debug(0,
          "sending %s to process group pgid %" PRId_Ert_Pgid,
          ert_formatProcessSignalName(&sigName, aSignal),
          FMTd_Ert_Pgid(aPgid));

    ERROR_IF(
        killpg(aPgid.mPgid, aSignal));

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
static void
killProcess_(int aSigNum, unsigned *aSigTrigger) __attribute__((__noreturn__));

static void
killProcess_(int aSigNum, unsigned *aSigTrigger)
{
    /* When running under valgrind, do not abort() because it causes the
     * program to behave as if it received SIGKILL. Instead, exit the
     * program immediately and allow valgrind to survey the program for
     * for leaks. */

    if (RUNNING_ON_VALGRIND)
        _exit(128 + aSigNum);

    /* Other threads might be attaching or have attached a signal handler,
     * and the signal might be blocked.
     *
     * Also, multiple threads might call this function at the same time.
     *
     * Try to raise the signal in this thread, but also mark the signal
     * trigger which will be noticed if a handler is already attached.
     *
     * Do not call back into any application libraries to avoid the risk
     * of infinite recursion, however be aware that dispatchSigHandler_()
     * might end up calling into this function recursively.
     *
     * Do not call library functions such as abort(3) because they will
     * try to flush IO streams and perform other activity that might fail. */

    __sync_or_and_fetch(aSigTrigger, 1);

    do
    {
        sigset_t sigSet;

        if (pthread_sigmask(SIG_SETMASK, 0, &sigSet))
            break;

        if (1 == sigismember(&sigSet, aSigNum))
        {
            if (sigdelset(&sigSet, aSigNum))
                break;

            if (pthread_sigmask(SIG_SETMASK, &sigSet, 0))
                break;
        }

        for (unsigned ix = 0; 10 > ix; ++ix)
        {
            struct sigaction sigAction = { .sa_handler = SIG_DFL };

            if (sigaction(aSigNum, &sigAction, 0))
                break;

            /* There is a window here for another thread to configure the
             * signal to be ignored, or handled. So when the signal is raised,
             * it might not actually cause the process to abort. */

            if ( ! ert_testAction(Ert_TestLevelRace))
            {
                if (raise(aSigNum))
                    break;
            }

            sigset_t pendingSet;

            if (sigpending(&pendingSet))
                break;

            int pendingSignal = sigismember(&pendingSet, aSigNum);
            if (-1 == pendingSignal)
                break;

            if (pendingSignal)
                ert_monotonicSleep(
                    Ert_Duration(ERT_NSECS(Ert_MilliSeconds(100))));
        }
    }
    while (0);

    /* There was an error trying to deliver the signal to the process, so
     * try one last time, then resort to killing the process. */

    if ( ! ert_testAction(Ert_TestLevelRace))
        raise(aSigNum);

    while (1)
    {
        ert_monotonicSleep(
            Ert_Duration(ERT_NSECS(Ert_Seconds(1))));

        if ( ! ert_testAction(Ert_TestLevelRace))
            raise(SIGKILL);
    }
}

/* -------------------------------------------------------------------------- */
void
ert_abortProcess(void)
{
    killProcess_(SIGABRT, &processAbort_);
}

void
ert_quitProcess(void)
{
    killProcess_(SIGQUIT, &processQuit_);
}

/* -------------------------------------------------------------------------- */
const char *
ert_ownProcessName(void)
{
    extern const char *__progname;

    return programName_ ? programName_ : __progname;
}

/* -------------------------------------------------------------------------- */
struct Ert_Pid
ert_ownProcessParentId(void)
{
    return Ert_Pid(getppid());
}

/* -------------------------------------------------------------------------- */
struct Ert_Pid
ert_ownProcessId(void)
{
    return Ert_Pid(getpid());
}

/* -------------------------------------------------------------------------- */
struct Ert_Pgid
ert_ownProcessGroupId(void)
{
    return Ert_Pgid(getpgid(0));
}

/* -------------------------------------------------------------------------- */
struct Ert_Pgid
ert_fetchProcessGroupId(struct Ert_Pid aPid)
{
    ensure(aPid.mPid);

    return Ert_Pgid(getpgid(aPid.mPid));
}

/* -------------------------------------------------------------------------- */
struct Ert_ExitCode
ert_extractProcessExitStatus(int aStatus, struct Ert_Pid aPid)
{
    /* Taking guidance from OpenGroup:
     *
     * http://pubs.opengroup.org/onlinepubs/009695399/
     *      utilities/xcu_chap02.html#tag_02_08_02
     *
     * Use exit codes above 128 to indicate signals, and codes below
     * 128 to indicate exit status. */

    struct Ert_ExitCode exitCode = { 255 };

    if (WIFEXITED(aStatus))
    {
        exitCode.mStatus = WEXITSTATUS(aStatus);
    }
    else if (WIFSIGNALED(aStatus))
    {
        struct Ert_ProcessSignalName sigName;

        debug(
            0,
            "process pid %" PRId_Ert_Pid " terminated by %s",
            FMTd_Ert_Pid(aPid),
            ert_formatProcessSignalName(&sigName, WTERMSIG(aStatus)));

        exitCode.mStatus = 128 + WTERMSIG(aStatus);
        if (255 < exitCode.mStatus)
            exitCode.mStatus = 255;
    }

    debug(0,
          "process pid %" PRId_Ert_Pid " exit code %" PRId_Ert_ExitCode,
          FMTd_Ert_Pid(aPid),
          FMTd_Ert_ExitCode(exitCode));

    return exitCode;
}

/* -------------------------------------------------------------------------- */
struct Ert_Duration
ert_ownProcessElapsedTime(void)
{
    return
        Ert_Duration(
            Ert_NanoSeconds(
                processTimeBase_.monotonic.ns
                ? (ert_monotonicTime().monotonic.ns
                       - processTimeBase_.monotonic.ns)
                : 0));
}

/* -------------------------------------------------------------------------- */
struct Ert_MonotonicTime
ert_ownProcessBaseTime(void)
{
    return processTimeBase_;
}

/* -------------------------------------------------------------------------- */
int
ert_purgeProcessOrphanedFds(void)
{
    /* Remove all the file descriptors that were not created explicitly by the
     * process itself, with the exclusion of stdin, stdout and stderr. */

    int rc = -1;

    /* Count the number of file descriptors explicitly created by the
     * process itself in order to correctly size the array to whitelist
     * the explicitly created file dsscriptors. Include stdin, stdout
     * and stderr in the whitelist by default.
     *
     * Note that stdin, stdout and stderr might in fact already be
     * represented in the file descriptor list, so the resulting
     * algorithms must be capable of handing duplicates. Force that
     * scenario to be covered by explicitly repeating each of them here. */

    int stdfds[] =
    {
        STDIN_FILENO,  STDIN_FILENO,
        STDOUT_FILENO, STDOUT_FILENO,
        STDERR_FILENO, STDERR_FILENO,
    };

    unsigned numFds = ERT_NUMBEROF(stdfds);

    ert_walkFileList(
        Ert_FileVisitor(
            &numFds,
            ERT_LAMBDA(
                int, (unsigned *aNumFds, const struct Ert_File *aFile),
                {
                    ++(*aNumFds);

                    return 0;
                })));

    /* Create the whitelist of file descriptors by copying the fds
     * from each of the explicitly created file descriptors. */

    int whiteList[numFds];

    {
        struct ProcessFdWhiteList
        {
            int     *mList;
            unsigned mLen;

        } fdWhiteList =
        {
            .mList = whiteList,
            .mLen  = 0,
        };

        for (unsigned jx = 0; ERT_NUMBEROF(stdfds) > jx; ++jx)
            fdWhiteList.mList[fdWhiteList.mLen++] = stdfds[jx];

        ert_walkFileList(
            Ert_FileVisitor(
                &fdWhiteList,
                ERT_LAMBDA(
                    int, (struct ProcessFdWhiteList *aWhiteList,
                          const struct Ert_File         *aFile),
                    {
                        aWhiteList->mList[aWhiteList->mLen++] = aFile->mFd;

                        return 0;
                    })));

        ensure(fdWhiteList.mLen == numFds);
    }

    ERROR_IF(
        ert_closeFdDescriptors(whiteList, ERT_NUMBEROF(whiteList)));

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
static void
prepareFork_(void)
{
    /* Acquire processFork_.mMutex to allow only one thread to
     * use the shared process fork structure instance at a time. */

    processFork_.mMutex = ert_lockMutex(&processFork_.mMutex_);

    /* Note that processLock_.mMutex is recursive, meaning that
     * it might already be held by this thread on entry to this function. */

    processFork_.mLock = ert_lockThreadSigMutex(processLock_.mMutex);

    ensure(
        0 < ert_ownThreadSigMutexLocked(processLock_.mMutex));

    /* Acquire the processSigVecLock_ for writing to ensure that there
     * are no other signal vector activity in progress. The purpose here
     * is to prevent the signal mutexes from being held while a fork is
     * in progress, since those locked mutexes will then be transferred
     * into the child process. */

    processFork_.mSigVecLock = ert_createRWMutexWriter(
        &processFork_.mSigVecLock_, &processSignals_.mVectorLock);

    /* Acquire the processSigMutex_ to ensure that there is no other
     * signal handler activity while the fork is in progress. */

    processFork_.mSigLock = ert_lockThreadSigMutex(&processSignals_.mSignalMutex);

    processFork_.mParentPid = ert_ownProcessId();

    debug(1, "prepare fork");
}

static void
completeFork_(void)
{
    ERT_TEST_RACE
    ({
        /* This function is called in the context of both parent and child
         * process immediately after the fork completes. Both processes
         * release the resources acquired when preparations were made
         * immediately preceding the fork. */

        processFork_.mSigLock =
            ert_unlockThreadSigMutex(processFork_.mSigLock);

        processFork_.mSigVecLock =
            ert_destroyRWMutexWriter(processFork_.mSigVecLock);

        processFork_.mLock = ert_unlockThreadSigMutex(processLock_.mMutex);

        pthread_mutex_t *lock = processFork_.mMutex;

        processFork_.mMutex = 0;

        lock = ert_unlockMutex(lock);
    });
}

static void
postForkParent_(void)
{
    /* This function is called in the context of the parent process
     * immediately after the fork completes. */

    debug(1, "groom forked parent");

    ensure(ert_ownProcessId().mPid == processFork_.mParentPid.mPid);

    completeFork_();
}

static void
postForkChild_(void)
{
    /* This function is called in the context of the child process
     * immediately after the fork completes, at which time it will
     * be the only thread running in the new process. The application
     * lock is recursive in the parent, and hence also in the child.
     * The parent holds the application lock, so the child must acquire
     * the lock to ensure that the recursive semantics in the child
     * are preserved. */

    if (processLock_.mLock)
        forkProcessLock_(processLock_.mLock);

    debug(1, "groom forked child");

    /* Do not check the parent pid here because it is theoretically possible
     * that the parent will have terminated and the pid reused by the time
     * the child gets around to checking. */

    completeFork_();
}

static struct Ert_ProcessForkChildLock_ *processAtForkLock_;

static void
prepareProcessFork_(void)
{
    processAtForkLock_ = ert_acquireProcessForkChildLock_(&processForkChildLock_);

    if (moduleInit_)
        prepareFork_();
}

static void
ert_postProcessForkParent_(void)
{
    if (moduleInit_)
        postForkParent_();

    struct Ert_ProcessForkChildLock_ *lock = processAtForkLock_;

    processAtForkLock_ = 0;

    lock = ert_releaseProcessForkChildLock_(lock);
}

static void
ert_postProcessForkChild_(void)
{
    if (moduleInit_)
        postForkChild_();

    struct Ert_ProcessForkChildLock_ *lock = processAtForkLock_;

    processAtForkLock_ = 0;

    lock = ert_releaseProcessForkChildLock_(lock);
}

/* -------------------------------------------------------------------------- */
int
Ert_Process_init(struct Ert_ProcessModule *self, const char *aArg0)
{
    int rc = -1;

    bool hookedSignals = false;

    self->mModule      = self;
    self->mErrorModule = 0;

    ensure( ! moduleInit_);

    processArg0_ = aArg0;

    programName_ = strrchr(processArg0_, '/');
    programName_ = programName_ ? programName_ + 1 : processArg0_;

    /* Open file descriptors to overlay stdin, stdout and stderr
     * to prevent other files inadvertently taking on these personas. */

    ERROR_IF(
        ert_openStdFds());

    /* Ensure that the recorded time base is non-zero to allow it
     * to be distinguished from the case that it was not recorded
     * at all, and also ensure that the measured elapsed process
     * time is always non-zero. */

    if ( ! moduleInitOnce_)
    {
        processTimeBase_ = ert_monotonicTime();
        do
            --processTimeBase_.monotonic.ns;
        while ( ! processTimeBase_.monotonic.ns);

        moduleInitOnce_ = true;
    }

    if ( ! moduleInitAtFork_)
    {
        /* Ensure that the synchronisation and signal functions are
         * prepared when a fork occurs so that they will be available
         * for use in the child process. Be aware that once functions
         * are registered, there is no way to deregister them. */

        ERROR_IF(
            errno = pthread_atfork(
                prepareProcessFork_,
                ert_postProcessForkParent_,
                ert_postProcessForkChild_));

        moduleInitAtFork_ = true;
    }

    ERROR_IF(
        Ert_Error_init(&self->mErrorModule_));
    self->mErrorModule = &self->mErrorModule_;

    ERROR_IF(
        errno = pthread_sigmask(SIG_BLOCK, 0, &processSigMask_));

    ensure( ! processLock_.mLock);

    ERROR_IF(
        createProcessLock_(&processLock_.mLock_));
    processLock_.mLock = &processLock_.mLock_;

    ERROR_IF(
        hookProcessSigCont_());
    ERROR_IF(
        hookProcessSigStop_());
    hookedSignals = true;

    ++moduleInit_;

    rc = 0;

Finally:

    FINALLY
    ({
        if (rc)
        {
            if (hookedSignals)
            {
                ABORT_IF(
                    unhookProcessSigStop_());
                ABORT_IF(
                    unhookProcessSigCont_());
            }

            struct ProcessLock *processLock = processLock_.mLock;
            processLock_.mLock = 0;

            closeProcessLock_(processLock);

            self->mErrorModule = Ert_Error_exit(self->mErrorModule);
        }
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
struct Ert_ProcessModule *
Ert_Process_exit(struct Ert_ProcessModule *self)
{
    if (self)
    {
        ensure( ! --moduleInit_);

        ABORT_IF(
            unhookProcessSigStop_());
        ABORT_IF(
            unhookProcessSigCont_());

        struct ProcessLock *processLock = processLock_.mLock;
        processLock_.mLock = 0;

        ensure(processLock);
        closeProcessLock_(processLock);

        ABORT_IF(
            errno = pthread_sigmask(SIG_SETMASK, &processSigMask_, 0));

        self->mErrorModule = Ert_Error_exit(self->mErrorModule);
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
