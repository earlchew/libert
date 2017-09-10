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
#ifndef ERT_THREAD_H
#define ERT_THREAD_H

#include "ert/compiler.h"
#include "ert/method.h"

#include <stdbool.h>
#include <pthread.h>
#include <signal.h>

/* -------------------------------------------------------------------------- */
#define ERT_METHOD_DEFINITION
#define ERT_METHOD_RETURN_ThreadMethod    int
#define ERT_METHOD_CONST_ThreadMethod
#define ERT_METHOD_ARG_LIST_ThreadMethod  ()
#define ERT_METHOD_CALL_LIST_ThreadMethod ()

#define ERT_METHOD_NAME      ThreadMethod
#define ERT_METHOD_RETURN    ERT_METHOD_RETURN_ThreadMethod
#define ERT_METHOD_CONST     ERT_METHOD_CONST_ThreadMethod
#define ERT_METHOD_ARG_LIST  ERT_METHOD_ARG_LIST_ThreadMethod
#define ERT_METHOD_CALL_LIST ERT_METHOD_CALL_LIST_ThreadMethod
#include "ert/method.h"

#define ThreadMethod(Object_, Method_)          \
    ERT_METHOD_TRAMPOLINE(                      \
        Object_, Method_,                       \
        ThreadMethod_,                          \
        ERT_METHOD_RETURN_ThreadMethod,         \
        ERT_METHOD_CONST_ThreadMethod,          \
        ERT_METHOD_ARG_LIST_ThreadMethod,       \
        ERT_METHOD_CALL_LIST_ThreadMethod)

/* -------------------------------------------------------------------------- */
#define ERT_METHOD_DEFINITION
#define ERT_METHOD_RETURN_MutexRepairMethod    int
#define ERT_METHOD_CONST_MutexRepairMethod
#define ERT_METHOD_ARG_LIST_MutexRepairMethod  ()
#define ERT_METHOD_CALL_LIST_MutexRepairMethod ()

#define ERT_METHOD_NAME      MutexRepairMethod
#define ERT_METHOD_RETURN    ERT_METHOD_RETURN_MutexRepairMethod
#define ERT_METHOD_CONST     ERT_METHOD_CONST_MutexRepairMethod
#define ERT_METHOD_ARG_LIST  ERT_METHOD_ARG_LIST_MutexRepairMethod
#define ERT_METHOD_CALL_LIST ERT_METHOD_CALL_LIST_MutexRepairMethod
#include "ert/method.h"

#define MutexRepairMethod(Object_, Method_)          \
    ERT_METHOD_TRAMPOLINE(                           \
        Object_, Method_,                            \
        MutexRepairMethod_,                          \
        ERT_METHOD_RETURN_MutexRepairMethod,         \
        ERT_METHOD_CONST_MutexRepairMethod,          \
        ERT_METHOD_ARG_LIST_MutexRepairMethod,       \
        ERT_METHOD_CALL_LIST_MutexRepairMethod)

/* -------------------------------------------------------------------------- */
/* Reference Counting Shared Objects
 *
 * It is unsafe to rely on reference counting for objects that reside in
 * memory that is shared across processes because there is no way to
 * guarantee that a user space reference count will be decremented when
 * a process is killed. */

#define THREAD_NOT_IMPLEMENTED ERT_NOTIMPLEMENTED

/* -------------------------------------------------------------------------- */
/* Fork Sentry
 *
 * Register a fork handler that will lock a resource (typcially some kind
 * of mutex) to prepare for the fork, and then unlock the resource
 * as the fork completes.
 */

#define THREAD_FORK_SENTRY(Lock_, Unlock_)                      \
    THREAD_FORK_SENTRY_1_(ERT_COUNTER, Lock_, Unlock_)

#define THREAD_FORK_SENTRY_1_(Suffix_, Lock_, Unlock_)          \
    THREAD_FORK_SENTRY_2_(Suffix_, Lock_, Unlock_)

#define THREAD_FORK_SENTRY_2_(Suffix_, Lock_, Unlock_)          \
    ERT_EARLY_INITIALISER(                                          \
        threadForkLockSentry_ ## Suffix_,                       \
        ({                                                      \
            void (*lock_)(void) =                               \
                ERT_LAMBDA(                                         \
                    void, (void),                               \
                    {                                           \
                        while ((Lock_))                         \
                            break;                              \
                    });                                         \
                                                                \
            void (*unlock_)(void) =                             \
                ERT_LAMBDA(                                         \
                    void, (void),                               \
                    {                                           \
                        while ((Unlock_))                       \
                            break;                              \
                    });                                         \
                                                                \
            ABORT_IF(                                           \
                pthread_atfork(lock_, unlock_, unlock_));       \
        }),                                                     \
        ({ }))


/* -------------------------------------------------------------------------- */
ERT_BEGIN_C_SCOPE;

struct Ert_Tid;

struct ThreadSigMask
{
    sigset_t mSigSet;
};

enum ThreadSigMaskAction
{
    ThreadSigMaskUnblock = -1,
    ThreadSigMaskSet     =  0,
    ThreadSigMaskBlock   = +1,
};

struct ThreadSigMutex
{
    pthread_mutex_t      mMutex_;
    pthread_mutex_t     *mMutex;
    pthread_cond_t       mCond_;
    pthread_cond_t      *mCond;
    struct ThreadSigMask mMask;
    unsigned             mLocked;
    pthread_t            mOwner;
};

#define THREAD_SIG_MUTEX_INITIALIZER(Mutex_) \
{                                            \
    .mMutex_ = PTHREAD_MUTEX_INITIALIZER,    \
    .mMutex  = &(Mutex_).mMutex_,            \
    .mCond_  = PTHREAD_COND_INITIALIZER,     \
    .mCond   = &(Mutex_).mCond_,             \
}

struct RWMutexReader
{
    pthread_rwlock_t *mMutex;
};

struct RWMutexWriter
{
    pthread_rwlock_t *mMutex;
};

struct SharedMutex
{
    pthread_mutex_t mMutex;
};

struct SharedCond
{
    pthread_cond_t mCond;
};

struct Thread
{
    pthread_t mThread;
    char     *mName;
    bool      mJoined;
};

struct ThreadAttr
{
    pthread_attr_t mAttr;
};

/* -------------------------------------------------------------------------- */
struct Ert_Tid
ownThreadId(void);

/* -------------------------------------------------------------------------- */
ERT_CHECKED struct Thread *
createThread(
    struct Thread           *self,
    const char              *aName,
    const struct ThreadAttr *aAttr,
    struct ThreadMethod      aMethod);

ERT_CHECKED int
joinThread(struct Thread *self);

void
cancelThread(struct Thread *self);

ERT_CHECKED struct Thread *
closeThread(struct Thread *self);

ERT_CHECKED int
killThread(struct Thread *self, int aSignal);

/* -------------------------------------------------------------------------- */
ERT_CHECKED struct ThreadAttr *
createThreadAttr(struct ThreadAttr *self);

ERT_CHECKED struct ThreadAttr *
destroyThreadAttr(struct ThreadAttr *self);

/* -------------------------------------------------------------------------- */
ERT_CHECKED struct ThreadSigMutex *
createThreadSigMutex(struct ThreadSigMutex *self);

ERT_CHECKED struct ThreadSigMutex *
destroyThreadSigMutex(struct ThreadSigMutex *self);

ERT_CHECKED struct ThreadSigMutex *
lockThreadSigMutex(struct ThreadSigMutex *self);

unsigned
ownThreadSigMutexLocked(struct ThreadSigMutex *self);

ERT_CHECKED struct ThreadSigMutex *
unlockThreadSigMutex(struct ThreadSigMutex *self);

/* -------------------------------------------------------------------------- */
ERT_CHECKED pthread_mutex_t *
createMutex(pthread_mutex_t *self);

ERT_CHECKED pthread_mutex_t *
destroyMutex(pthread_mutex_t *self);

ERT_CHECKED pthread_mutex_t *
lockMutex(pthread_mutex_t *self);

ERT_CHECKED pthread_mutex_t *
unlockMutex(pthread_mutex_t *self);

ERT_CHECKED pthread_mutex_t *
unlockMutexSignal(pthread_mutex_t *self, pthread_cond_t *aCond);

ERT_CHECKED pthread_mutex_t *
unlockMutexBroadcast(pthread_mutex_t *self, pthread_cond_t *aCond);

/* -------------------------------------------------------------------------- */
ERT_CHECKED struct SharedMutex *
createSharedMutex(struct SharedMutex *self);

ERT_CHECKED struct SharedMutex *
destroySharedMutex(struct SharedMutex *self);

ERT_CHECKED struct SharedMutex *
refSharedMutex(struct SharedMutex *self) THREAD_NOT_IMPLEMENTED;

ERT_CHECKED struct SharedMutex *
unrefSharedMutex(struct SharedMutex *self) THREAD_NOT_IMPLEMENTED;

ERT_CHECKED struct SharedMutex *
lockSharedMutex(struct SharedMutex *self, struct MutexRepairMethod aRepair);

ERT_CHECKED struct SharedMutex *
unlockSharedMutex(struct SharedMutex *self);

ERT_CHECKED struct SharedMutex *
unlockSharedMutexSignal(struct SharedMutex *self, struct SharedCond *aCond);

ERT_CHECKED struct SharedMutex *
unlockSharedMutexBroadcast(struct SharedMutex *self, struct SharedCond *aCond);

/* -------------------------------------------------------------------------- */
ERT_CHECKED pthread_rwlock_t *
createRWMutex(pthread_rwlock_t *self);

ERT_CHECKED pthread_rwlock_t *
destroyRWMutex(pthread_rwlock_t *self);

/* -------------------------------------------------------------------------- */
ERT_CHECKED struct RWMutexReader *
createRWMutexReader(struct RWMutexReader *self,
                    pthread_rwlock_t     *aMutex);

ERT_CHECKED struct RWMutexReader *
destroyRWMutexReader(struct RWMutexReader *self);

/* -------------------------------------------------------------------------- */
ERT_CHECKED struct RWMutexWriter *
createRWMutexWriter(struct RWMutexWriter *self,
                    pthread_rwlock_t     *aMutex);

ERT_CHECKED struct RWMutexWriter *
destroyRWMutexWriter(struct RWMutexWriter *self);

/* -------------------------------------------------------------------------- */
ERT_CHECKED pthread_cond_t *
createCond(pthread_cond_t *self);

ERT_CHECKED pthread_cond_t *
destroyCond(pthread_cond_t *self);

ERT_CHECKED struct SharedCond *
refSharedCond(struct SharedCond *self) THREAD_NOT_IMPLEMENTED;

ERT_CHECKED struct SharedCond *
unrefSharedCond(struct SharedCond *self) THREAD_NOT_IMPLEMENTED;

void
waitCond(pthread_cond_t *self, pthread_mutex_t *aMutex);

/* -------------------------------------------------------------------------- */
ERT_CHECKED struct SharedCond *
createSharedCond(struct SharedCond *self);

ERT_CHECKED struct SharedCond *
destroySharedCond(struct SharedCond *self);

ERT_CHECKED int
waitSharedCond(struct SharedCond *self, struct SharedMutex *aMutex);

/* -------------------------------------------------------------------------- */
ERT_CHECKED struct ThreadSigMask *
pushThreadSigMask(
    struct ThreadSigMask    *self,
    enum ThreadSigMaskAction aAction,
    const int               *aSigList);

ERT_CHECKED struct ThreadSigMask *
popThreadSigMask(struct ThreadSigMask *self);

ERT_CHECKED int
waitThreadSigMask(const int *aSigList);

/* -------------------------------------------------------------------------- */

ERT_END_C_SCOPE;

#endif /* ERT_THREAD_H */
