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

#include "ert/thread.h"
#include "ert/timekeeping.h"
#include "ert/process.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <sys/syscall.h>

/* -------------------------------------------------------------------------- */
static void *
timedLock_(
    void *aLock,
    int   aTryLock(void *),
    int   aTimedLock(void *, const struct timespec *))
{
    struct Ert_ProcessSigContTracker sigContTracker =
        Ert_ProcessSigContTracker();

    while (1)
    {
        ERT_ABORT_IF(
            (errno = aTryLock(aLock),
             errno && EBUSY != errno && EOWNERDEAD != errno),
            {
                ert_terminate(errno, "Unable to acquire lock");
            });

        if (EBUSY != errno)
            break;

        /* There is no way to configure the mutex to use a monotonic
         * clock to compute the deadline. Since the timeout is only
         * important on the error path, this is not a critical problem
         * in this use case. */

        const unsigned timeout_s = 600;

        struct Ert_WallClockTime tm = ert_wallclockTime();

        struct timespec deadline =
            ert_timeSpecFromNanoSeconds(
                Ert_NanoSeconds(
                    tm.wallclock.ns
                        + ERT_NSECS(Ert_Seconds(timeout_s * 60)).ns));

        ERT_ABORT_IF(
            (errno = aTimedLock(aLock, &deadline),
             errno && ETIMEDOUT != errno && EOWNERDEAD != errno),
            {
                ert_terminate(errno,
                          "Unable to acquire lock after %us", timeout_s);
            });

        if (ETIMEDOUT == errno)
        {
            /* Try again if the attempt to lock the mutex timed out
             * but the process was stopped for some part of that time. */

            if (ert_checkProcessSigContTracker(&sigContTracker))
                continue;
        }

        break;
    }

    return errno ? 0 : aLock;
}

/* -------------------------------------------------------------------------- */
struct Ert_Tid
ert_ownThreadId(void)
{
    return Ert_Tid(syscall(SYS_gettid));
}

/* -------------------------------------------------------------------------- */
struct Ert_Thread *
ert_closeThread(
    struct Ert_Thread *self)
{
    if (self)
    {
        /* If the thread has not yet been joined, join it now and include
         * cancellation as a failure condition. The caller will join
         * explicitly if cancellation is to be benign. */

        if ( ! self->mJoined)
        {
            ERT_ABORT_IF(
                ert_joinThread(self),
                {
                    ert_terminate(errno,
                              "Unable to join thread%s%s",
                              ! self->mName ? "" : " ",
                              ! self->mName ? "" : self->mName);
                });
        }

        free(self->mName);
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
struct Thread_
{
    const char *mName;

    pthread_mutex_t mMutex;
    pthread_cond_t  mCond;

    struct Ert_ThreadMethod mMethod;
};

static void *
createThread_(
    void *self_)
{
    struct Thread_ *self = self_;

    struct Ert_ThreadMethod method = self->mMethod;

    pthread_mutex_t *lock = ert_lockMutex(&self->mMutex);

    if (self->mName)
        ERT_ABORT_IF(
            errno = pthread_setname_np(pthread_self(), self->mName),
            {
                ert_terminate(errno,
                          "Unable to set thread name %s", self->mName);
            });

    lock = ert_unlockMutexSignal(lock, &self->mCond);

    /* Do not reference self beyond this point because the parent
     * will have deallocated the struct Thread_ instance. Also do
     * not rely on a struct Ert_Thread instance because a detached thread
     * will not have one. */

    ERT_ABORT_IF(
        ert_callThreadMethod(method));

    return 0;
}

struct Ert_Thread *
ert_createThread(
    struct Ert_Thread           *self,
    const char                  *aName,
    const struct Ert_ThreadAttr *aAttr,
    struct Ert_ThreadMethod      aMethod)
{
    /* The caller can specify a null self to create a detached thread.
     * Detached threads are not owned by the parent, and the parent will
     * not join or close the detached thread. */

    struct Thread_ thread =
    {
        .mName = aName,

        .mMutex = PTHREAD_MUTEX_INITIALIZER,
        .mCond  = PTHREAD_COND_INITIALIZER,

        .mMethod = aMethod,
    };

    struct Ert_Thread self_;
    if ( ! self)
        self = &self_;
    else
    {
        self->mName = 0;

        if (aName)
        {
            ERT_ABORT_UNLESS(
                self->mName = strdup(aName),
                {
                    ert_terminate(errno, "Unable to copy thread name %s", aName);
                });
        }

        if (aAttr)
        {
            /* If the caller has specified a non-null self, ensure that there
             * the thread is detached, since the parent is expected to join
             * and close the thread. */

            int detached;
            ERT_ABORT_IF(
                (errno = pthread_attr_getdetachstate(&aAttr->mAttr, &detached)),
                {
                    ert_terminate(
                        errno,
                        "Unable to query detached state attribute "
                        "for thread %s",
                        aName);
                });

            ERT_ABORT_IF(
                detached,
                {
                    errno = EINVAL;
                });
        }
    }

    pthread_mutex_t *lock = ert_lockMutex(&thread.mMutex);

    self->mJoined = false;

    ERT_ABORT_IF(
        (errno = pthread_create(
            &self->mThread, aAttr ? &aAttr->mAttr : 0, createThread_, &thread)),
        {
            ert_terminate(
                errno,
                "Unable to create thread %s", aName);
        });

    ert_waitCond(&thread.mCond, lock);
    lock = ert_unlockMutex(lock);

    return self != &self_ ? self : 0;
}

/* -------------------------------------------------------------------------- */
int
ert_joinThread(
    struct Ert_Thread *self)
{
    int rc = -1;

    ERT_ERROR_IF(
        self->mJoined,
        {
            errno = EINVAL;
        });

    void *future;
    ERT_ERROR_IF(
        (errno = pthread_join(self->mThread, &future)));

    self->mJoined = true;

    ERT_ERROR_IF(
        PTHREAD_CANCELED == future,
        {
            errno = ECANCELED;
        });

    ert_ensure( ! future);

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
void
ert_cancelThread(
    struct Ert_Thread *self)
{
    ERT_ABORT_IF(
        errno = pthread_cancel(self->mThread),
        {
            ert_terminate(errno,
                      "Unable to cancel thread%s%s",
                      ! self->mName ? "" : " ",
                      ! self->mName ? "" : self->mName);
        });
}

/* -------------------------------------------------------------------------- */
int
ert_killThread(
    struct Ert_Thread *self,
    int                aSignal)
{
    int rc = -1;

    ERT_ERROR_IF(
        errno = pthread_kill(self->mThread, aSignal));

    rc = 0;

Ert_Finally:
    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
struct Ert_ThreadAttr *
ert_createThreadAttr(
    struct Ert_ThreadAttr *self)
{
    ERT_ABORT_IF(
        (errno = pthread_attr_init(&self->mAttr)),
        {
            ert_terminate(
                errno,
                "Unable to create thread attribute");
        });

    return self;
}

/* -------------------------------------------------------------------------- */
struct Ert_ThreadAttr *
ert_destroyThreadAttr(
    struct Ert_ThreadAttr *self)
{
    ERT_ABORT_IF(
        (errno = pthread_attr_destroy(&self->mAttr)),
        {
            ert_terminate(
                errno,
                "Unable to destroy thread attribute");
        });

    return 0;
}

/* -------------------------------------------------------------------------- */
pthread_mutex_t *
ert_createMutex(
    pthread_mutex_t *self)
{
    ERT_ABORT_IF(
        (errno = pthread_mutex_init(self, 0)),
        {
            ert_terminate(
                errno,
                "Unable to create mutex");
        });

    return self;
}

/* -------------------------------------------------------------------------- */
pthread_mutex_t *
ert_destroyMutex(
    pthread_mutex_t *self)
{
    if (self)
    {
        ERT_ABORT_IF(
            (errno = pthread_mutex_destroy(self)),
            {
                ert_terminate(
                    errno,
                    "Unable to destroy mutex");
            });
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
tryMutexLock_(
    void *self)
{
    return pthread_mutex_trylock(self);
}

static ERT_CHECKED int
tryMutexTimedLock_(
    void                  *self,
    const struct timespec *aDeadline)
{
    return pthread_mutex_timedlock(self, aDeadline);
}

static pthread_mutex_t *
lockMutex_(
    pthread_mutex_t *self)
{
    return timedLock_(self, tryMutexLock_, tryMutexTimedLock_);
}

pthread_mutex_t *
ert_lockMutex(
    pthread_mutex_t *self)
{
    ert_ensure( ! ert_ownProcessSignalContext());

    ERT_ABORT_UNLESS(
        lockMutex_(self));

    return self;
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED pthread_mutex_t *
unlockMutex_(
    pthread_mutex_t *self)
{
    if (self)
    {
        ERT_ABORT_IF(
            (errno = pthread_mutex_unlock(self)),
            {
                ert_terminate(errno, "Unable to unlock mutex");
            });
    }

    return 0;
}

pthread_mutex_t *
ert_unlockMutex(
    pthread_mutex_t *self)
{
    ert_ensure( ! ert_ownProcessSignalContext());

    return unlockMutex_(self);
}

/* -------------------------------------------------------------------------- */
static pthread_mutex_t *
ert_unlockMutexSignal_(
    pthread_mutex_t *self,
    pthread_cond_t  *aCond)
{
    if (self)
    {
        ERT_ABORT_IF(
            (errno = pthread_cond_signal(aCond)),
            {
                ert_terminate(
                    errno,
                    "Unable to signal to condition variable");
            });

        ERT_ABORT_IF(
            (errno = pthread_mutex_unlock(self)),
            {
                ert_terminate(
                    errno,
                    "Unable to lock mutex");
            });
    }

    return 0;
}

pthread_mutex_t *
ert_unlockMutexSignal(
    pthread_mutex_t *self,
    pthread_cond_t  *aCond)
{
    ert_ensure( ! ert_ownProcessSignalContext());

    return ert_unlockMutexSignal_(self, aCond);
}

/* -------------------------------------------------------------------------- */
static pthread_mutex_t *
ert_unlockMutexBroadcast_(
    pthread_mutex_t *self,
    pthread_cond_t  *aCond)
{
    if (self)
    {
        ERT_ABORT_IF(
            (errno = pthread_cond_broadcast(aCond)),
            {
                ert_terminate(
                    errno,
                    "Unable to broadcast to condition variable");
            });

        ERT_ABORT_IF(
            (errno = pthread_mutex_unlock(self)),
            {
                ert_terminate(
                    errno,
                    "Unable to lock mutex");
            });
    }

    return 0;
}

pthread_mutex_t *
ert_unlockMutexBroadcast(
    pthread_mutex_t *self,
    pthread_cond_t  *aCond)
{
    ert_ensure( ! ert_ownProcessSignalContext());

    return ert_unlockMutexBroadcast_(self, aCond);
}

/* -------------------------------------------------------------------------- */
struct Ert_SharedMutex *
ert_createSharedMutex(
    struct Ert_SharedMutex *self)
{
    pthread_mutexattr_t  mutexattr_;
    pthread_mutexattr_t *mutexattr = 0;

    ERT_ABORT_IF(
        (errno = pthread_mutexattr_init(&mutexattr_)),
        {
            ert_terminate(
                errno,
                "Unable to allocate mutex attribute");
        });
    mutexattr = &mutexattr_;

    ERT_ABORT_IF(
        (errno = pthread_mutexattr_setpshared(mutexattr,
                                              PTHREAD_PROCESS_SHARED)),
        {
            ert_terminate(
                errno,
                "Unable to set mutex attribute PTHREAD_PROCESS_SHARED");
        });

    ERT_ABORT_IF(
        (errno = pthread_mutexattr_setrobust(mutexattr,
                                             PTHREAD_MUTEX_ROBUST)),
        {
            ert_terminate(
                errno,
                "Unable to set mutex attribute PTHREAD_MUTEX_ROBUST");
        });

    ERT_ABORT_IF(
        (errno = pthread_mutex_init(&self->mMutex, mutexattr)),
        {
            ert_terminate(
                errno,
                "Unable to create shared mutex");
        });

    ERT_ABORT_IF(
        (errno = pthread_mutexattr_destroy(mutexattr)),
        {
            ert_terminate(
                errno,
                "Unable to destroy mutex attribute");
        });

    return self;
}

/* -------------------------------------------------------------------------- */
struct Ert_SharedMutex *
ert_destroySharedMutex(
    struct Ert_SharedMutex *self)
{
    if (self)
        ERT_ABORT_IF(
            ert_destroyMutex(&self->mMutex));

    return 0;
}

/* -------------------------------------------------------------------------- */
struct Ert_SharedMutex *
ert_lockSharedMutex(
    struct Ert_SharedMutex      *self,
    struct Ert_MutexRepairMethod aRepair)
{
    ert_ensure( ! ert_ownProcessSignalContext());

    if ( ! lockMutex_(&self->mMutex))
    {
        ERT_ABORT_IF(
            ert_callMutexRepairMethod(aRepair),
            {
                ert_terminate(
                    errno,
                    "Unable to repair mutex consistency");
            });

        ERT_ABORT_IF(
            (errno = pthread_mutex_consistent(&self->mMutex)),
            {
                ert_terminate(
                    errno,
                    "Unable to restore mutex consistency");
            });
    }

    return self;
}

/* -------------------------------------------------------------------------- */
struct Ert_SharedMutex *
ert_unlockSharedMutex(
    struct Ert_SharedMutex *self)
{
    ert_ensure( ! ert_ownProcessSignalContext());

    if (self)
        ERT_ABORT_IF(
            unlockMutex_(&self->mMutex));

    return 0;
}

/* -------------------------------------------------------------------------- */
struct Ert_SharedMutex *
ert_unlockSharedMutexSignal(
    struct Ert_SharedMutex *self,
    struct Ert_SharedCond  *aCond)
{
    ert_ensure( ! ert_ownProcessSignalContext());

    if (self)
        ERT_ABORT_IF(
            ert_unlockMutexSignal_(&self->mMutex, &aCond->mCond));

    return 0;
}

/* -------------------------------------------------------------------------- */
struct Ert_SharedMutex *
ert_unlockSharedMutexBroadcast(
    struct Ert_SharedMutex *self,
    struct Ert_SharedCond  *aCond)
{
    ert_ensure( ! ert_ownProcessSignalContext());

    if (self)
        ERT_ABORT_IF(
            ert_unlockMutexBroadcast_(&self->mMutex, &aCond->mCond));

    return 0;
}

/* -------------------------------------------------------------------------- */
pthread_cond_t *
ert_createCond(
    pthread_cond_t *self)
{
    pthread_condattr_t  condattr_;
    pthread_condattr_t *condattr = 0;

    ERT_ABORT_IF(
        (errno = pthread_condattr_init(&condattr_)),
        {
            ert_terminate(
                errno,
                "Unable to allocate condition variable attribute");
        });
    condattr = &condattr_;

    ERT_ABORT_IF(
        (errno = pthread_condattr_setclock(condattr, CLOCK_MONOTONIC)),
        {
            ert_terminate(
                errno,
                "Unable to set condition attribute CLOCK_MONOTONIC");
        });

    ERT_ABORT_IF(
        (errno = pthread_cond_init(self, condattr)),
        {
            ert_terminate(
                errno,
                "Unable to create condition variable");
        });

    ERT_ABORT_IF(
        (errno = pthread_condattr_destroy(condattr)),
        {
            ert_terminate(
                errno,
                "Unable to destroy condition attribute");
        });

    return self;
}

/* -------------------------------------------------------------------------- */
pthread_cond_t *
ert_destroyCond(
    pthread_cond_t *self)
{
    if (self)
    {
        ERT_ABORT_IF(
            (errno = pthread_cond_destroy(self)),
            {
                ert_terminate(
                    errno,
                    "Unable to destroy condition variable");
            });
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
static int
waitCond_(
    pthread_cond_t  *self,
    pthread_mutex_t *aMutex)
{
    ERT_ABORT_IF(
        (errno = pthread_cond_wait(self, aMutex),
         errno && EOWNERDEAD != errno),
        {
            ert_terminate(
                errno,
                "Unable to wait for condition variable");
        });

    return errno;
}

void
ert_waitCond(
    pthread_cond_t  *self,
    pthread_mutex_t *aMutex)
{
    ert_ensure( ! ert_ownProcessSignalContext());

    ERT_ABORT_IF(
        waitCond_(self, aMutex),
        {
            ert_terminate(
                errno,
                "Condition variable mutex owner has terminated");
        });
}

/* -------------------------------------------------------------------------- */
struct Ert_ThreadSigMask *
ert_pushThreadSigMask(
    struct Ert_ThreadSigMask     *self,
    enum Ert_ThreadSigMaskAction  aAction,
    const int                    *aSigList)
{
    int maskAction;
    switch (aAction)
    {
    default:
        ERT_ABORT_IF(
            true,
            {
                errno = EINVAL;
            });
    case Ert_ThreadSigMaskUnblock: maskAction = SIG_UNBLOCK; break;
    case Ert_ThreadSigMaskSet:     maskAction = SIG_SETMASK; break;
    case Ert_ThreadSigMaskBlock:   maskAction = SIG_BLOCK;   break;
    }

    sigset_t sigSet;

    if ( ! aSigList)
        ERT_ABORT_IF(sigfillset(&sigSet));
    else
    {
        ERT_ABORT_IF(sigemptyset(&sigSet));
        for (size_t ix = 0; aSigList[ix]; ++ix)
            ERT_ABORT_IF(sigaddset(&sigSet, aSigList[ix]));
    }

    ERT_ABORT_IF(pthread_sigmask(maskAction, &sigSet, &self->mSigSet));

    return self;
}

/* -------------------------------------------------------------------------- */
struct Ert_SharedCond *
ert_createSharedCond(
    struct Ert_SharedCond *self)
{
    pthread_condattr_t  condattr_;
    pthread_condattr_t *condattr = 0;

    ERT_ABORT_IF(
        (errno = pthread_condattr_init(&condattr_)),
        {
            ert_terminate(
                errno,
                "Unable to allocate condition variable attribute");
        });
    condattr = &condattr_;

    ERT_ABORT_IF(
        (errno = pthread_condattr_setclock(condattr, CLOCK_MONOTONIC)),
        {
            ert_terminate(
                errno,
                "Unable to set condition attribute CLOCK_MONOTONIC");
        });

    ERT_ABORT_IF(
        (errno = pthread_condattr_setpshared(condattr,
                                             PTHREAD_PROCESS_SHARED)),
        {
            ert_terminate(
                errno,
                "Unable to set condition attribute PTHREAD_PROCESS_SHARED");
        });

    ERT_ABORT_IF(
        (errno = pthread_cond_init(&self->mCond, condattr)),
        {
            ert_terminate(
                errno,
                "Unable to create shared condition variable");
        });

    ERT_ABORT_IF(
        (errno = pthread_condattr_destroy(condattr)),
        {
            ert_terminate(
                errno,
                "Unable to destroy condition attribute");
        });

    return self;
}

/* -------------------------------------------------------------------------- */
struct Ert_SharedCond *
ert_destroySharedCond(
    struct Ert_SharedCond *self)
{
    if (self)
        ERT_ABORT_IF(
            ert_destroyCond(&self->mCond));

    return 0;
}

/* -------------------------------------------------------------------------- */
int
ert_waitSharedCond(
    struct Ert_SharedCond  *self,
    struct Ert_SharedMutex *aMutex)
{
    ert_ensure( ! ert_ownProcessSignalContext());

    return waitCond_(&self->mCond, &aMutex->mMutex) ? -1 : 0;
}

/* -------------------------------------------------------------------------- */
struct Ert_ThreadSigMask *
ert_popThreadSigMask(
    struct Ert_ThreadSigMask *self)
{
    if (self)
        ERT_ABORT_IF(pthread_sigmask(SIG_SETMASK, &self->mSigSet, 0));

    return 0;
}

/* -------------------------------------------------------------------------- */
int
ert_waitThreadSigMask(
    const int *aSigList)
{
    int rc = -1;

    sigset_t sigSet;

    if ( ! aSigList)
        ERT_ERROR_IF(
            sigemptyset(&sigSet));
    else
    {
        ERT_ERROR_IF(
            sigfillset(&sigSet));
        for (size_t ix = 0; aSigList[ix]; ++ix)
            ERT_ERROR_IF(
                sigdelset(&sigSet, aSigList[ix]));
    }

    int err = 0;
    ERT_ERROR_IF(
        (err = sigsuspend(&sigSet),
         -1 != err || EINTR != errno),
        {
            if (-1 != err)
                errno = 0;
        });

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
struct Ert_ThreadSigMutex *
ert_createThreadSigMutex(
    struct Ert_ThreadSigMutex *self)
{
    self->mMutex = ert_createMutex(&self->mMutex_);
    self->mCond  = ert_createCond(&self->mCond_);

    self->mLocked = 0;

    return self;
}

/* -------------------------------------------------------------------------- */
struct Ert_ThreadSigMutex *
ert_destroyThreadSigMutex(
    struct Ert_ThreadSigMutex *self)
{
    if (self)
    {
        ert_ensure( ! self->mLocked);

        self->mCond  = ert_destroyCond(self->mCond);
        self->mMutex = ert_destroyMutex(self->mMutex);
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
struct Ert_ThreadSigMutex *
ert_lockThreadSigMutex(
    struct Ert_ThreadSigMutex *self)
{
    /* When acquiring the lock, first ensure that no signal is delivered
     * within the context of this thread, and only then lock the mutex
     * to prevent other threads accessing the resource. */

    struct Ert_ThreadSigMask  threadSigMask_;
    struct Ert_ThreadSigMask *threadSigMask =
        ert_pushThreadSigMask(&threadSigMask_, Ert_ThreadSigMaskBlock, 0);

    pthread_mutex_t *lock;
    ERT_ABORT_UNLESS(
        lock = lockMutex_(self->mMutex));

    if (self->mLocked && ! pthread_equal(self->mOwner, pthread_self()))
    {
        do
            waitCond_(self->mCond, lock);
        while (self->mLocked);
    }

    if (1 == ++self->mLocked)
    {
        self->mMask  = *threadSigMask;
        self->mOwner = pthread_self();
    }

    lock = unlockMutex_(lock);

    ert_ensure(self->mLocked);
    ert_ensure(pthread_equal(self->mOwner, pthread_self()));

    if (1 != self->mLocked)
        threadSigMask = ert_popThreadSigMask(threadSigMask);

    return self;
}

/* -------------------------------------------------------------------------- */
struct Ert_ThreadSigMutex *
ert_unlockThreadSigMutex(
    struct Ert_ThreadSigMutex *self)
{
    if (self)
    {
        ert_ensure(self->mLocked);
        ert_ensure(pthread_equal(self->mOwner, pthread_self()));

        unsigned locked = self->mLocked - 1;

        if (locked)
            self->mLocked = locked;
        else
        {
            struct Ert_ThreadSigMask  threadSigMask_ = self->mMask;
            struct Ert_ThreadSigMask *threadSigMask  = &threadSigMask_;

            pthread_mutex_t *lock;
            ERT_ABORT_UNLESS(
                lock = lockMutex_(self->mMutex));

            self->mLocked = 0;

            ert_unlockMutexSignal_(lock, self->mCond);

            /* Do not reference the mutex instance once it is released
             * because it might at this point be owned by another
             * thread. */

            threadSigMask = ert_popThreadSigMask(threadSigMask);
        }
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
unsigned
ert_ownThreadSigMutexLocked(
    struct Ert_ThreadSigMutex *self)
{
    unsigned locked;

    pthread_mutex_t *lock;
    ERT_ABORT_UNLESS(
        lock = lockMutex_(self->mMutex));

    locked = self->mLocked;

    if (locked && ! pthread_equal(self->mOwner, pthread_self()))
        locked = 0;

    lock = unlockMutex_(lock);

    return locked;
}

/* -------------------------------------------------------------------------- */
pthread_rwlock_t *
ert_createRWMutex(
    pthread_rwlock_t *self)
{
    ERT_ABORT_IF(
        (errno = pthread_rwlock_init(self, 0)),
        {
            ert_terminate(
                errno,
                "Unable to create rwlock");
        });

    return self;
}

/* -------------------------------------------------------------------------- */
pthread_rwlock_t *
ert_destroyRWMutex(
    pthread_rwlock_t *self)
{
    if (self)
    {
        ERT_ABORT_IF(
            (errno = pthread_rwlock_destroy(self)),
            {
                ert_terminate(
                    errno,
                    "Unable to destroy rwlock");
            });
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
ert_tryRWMutexRdLock_(
    void *self)
{
    return pthread_rwlock_tryrdlock(self);
}

static ERT_CHECKED int
ert_tryRWMutexTimedRdLock_(
    void                  *self,
    const struct timespec *aDeadline)
{
    return pthread_rwlock_timedrdlock(self, aDeadline);
}

struct Ert_RWMutexReader *
ert_createRWMutexReader(
    struct Ert_RWMutexReader *self,
    pthread_rwlock_t         *aMutex)
{
    ERT_ABORT_UNLESS(
        self->mMutex = timedLock_(
            aMutex, ert_tryRWMutexRdLock_, ert_tryRWMutexTimedRdLock_));

    return self;
}

/* -------------------------------------------------------------------------- */
struct Ert_RWMutexReader *
ert_destroyRWMutexReader(
    struct Ert_RWMutexReader *self)
{
    if (self)
    {
        ERT_ABORT_IF(
            (errno = pthread_rwlock_unlock(self->mMutex)),
            {
                ert_terminate(
                    errno,
                    "Unable to release rwlock reader lock");
            });
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
ert_tryRWMutexWrLock_(
    void *self)
{
    return pthread_rwlock_trywrlock(self);
}

static ERT_CHECKED int
ert_tryRWMutexTimedWrLock_(
    void                  *self,
    const struct timespec *aDeadline)
{
    return pthread_rwlock_timedwrlock(self, aDeadline);
}

struct Ert_RWMutexWriter *
ert_createRWMutexWriter(
    struct Ert_RWMutexWriter *self,
    pthread_rwlock_t         *aMutex)
{
    ERT_ABORT_UNLESS(
        self->mMutex = timedLock_(
            aMutex, ert_tryRWMutexWrLock_, ert_tryRWMutexTimedWrLock_));

    return self;
}

/* -------------------------------------------------------------------------- */
struct Ert_RWMutexWriter *
ert_destroyRWMutexWriter(
    struct Ert_RWMutexWriter *self)
{
    if (self)
    {
        ERT_ABORT_IF(
            (errno = pthread_rwlock_unlock(self->mMutex)),
            {
                ert_terminate(
                    errno,
                    "Unable to release rwlock writer lock");
            });
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
