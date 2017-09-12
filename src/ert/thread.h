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

#define ERT_METHOD_TYPE_PREFIX     Ert_
#define ERT_METHOD_FUNCTION_PREFIX ert_

#define ERT_METHOD_NAME      ThreadMethod
#define ERT_METHOD_RETURN    ERT_METHOD_RETURN_ThreadMethod
#define ERT_METHOD_CONST     ERT_METHOD_CONST_ThreadMethod
#define ERT_METHOD_ARG_LIST  ERT_METHOD_ARG_LIST_ThreadMethod
#define ERT_METHOD_CALL_LIST ERT_METHOD_CALL_LIST_ThreadMethod
#include "ert/method.h"

#define Ert_ThreadMethod(Object_, Method_)      \
    ERT_METHOD_TRAMPOLINE(                      \
        Object_, Method_,                       \
        Ert_ThreadMethod_,                      \
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

#define ERT_METHOD_TYPE_PREFIX     Ert_
#define ERT_METHOD_FUNCTION_PREFIX ert_

#define ERT_METHOD_NAME      MutexRepairMethod
#define ERT_METHOD_RETURN    ERT_METHOD_RETURN_MutexRepairMethod
#define ERT_METHOD_CONST     ERT_METHOD_CONST_MutexRepairMethod
#define ERT_METHOD_ARG_LIST  ERT_METHOD_ARG_LIST_MutexRepairMethod
#define ERT_METHOD_CALL_LIST ERT_METHOD_CALL_LIST_MutexRepairMethod
#include "ert/method.h"

#define Ert_MutexRepairMethod(Object_, Method_)      \
    ERT_METHOD_TRAMPOLINE(                           \
        Object_, Method_,                            \
        Ert_MutexRepairMethod_,                      \
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

#define ERT_THREAD_NOT_IMPLEMENTED ERT_NOTIMPLEMENTED

/* -------------------------------------------------------------------------- */
/* Fork Sentry
 *
 * Register a fork handler that will lock a resource (typcially some kind
 * of mutex) to prepare for the fork, and then unlock the resource
 * as the fork completes.
 */

#define ERT_THREAD_FORK_SENTRY(Lock_, Unlock_)                  \
    ERT_THREAD_FORK_SENTRY_1_(ERT_COUNTER, Lock_, Unlock_)

#define ERT_THREAD_FORK_SENTRY_1_(Suffix_, Lock_, Unlock_)      \
    ERT_THREAD_FORK_SENTRY_2_(Suffix_, Lock_, Unlock_)

#define ERT_THREAD_FORK_SENTRY_2_(Suffix_, Lock_, Unlock_)      \
    ERT_EARLY_INITIALISER(                                      \
        threadForkLockSentry_ ## Suffix_,                       \
        ({                                                      \
            void (*lock_)(void) =                               \
                ERT_LAMBDA(                                     \
                    void, (void),                               \
                    {                                           \
                        while ((Lock_))                         \
                            break;                              \
                    });                                         \
                                                                \
            void (*unlock_)(void) =                             \
                ERT_LAMBDA(                                     \
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

struct Ert_ThreadSigMask
{
    sigset_t mSigSet;
};

enum Ert_ThreadSigMaskAction
{
    Ert_ThreadSigMaskUnblock = -1,
    Ert_ThreadSigMaskSet     =  0,
    Ert_ThreadSigMaskBlock   = +1,
};

struct Ert_ThreadSigMutex
{
    pthread_mutex_t           mMutex_;
    pthread_mutex_t          *mMutex;
    pthread_cond_t            mCond_;
    pthread_cond_t           *mCond;
    struct Ert_ThreadSigMask  mMask;
    unsigned                  mLocked;
    pthread_t                 mOwner;
};

#define ERT_THREAD_SIG_MUTEX_INITIALIZER(Mutex_)        \
{                                                       \
    .mMutex_ = PTHREAD_MUTEX_INITIALIZER,               \
    .mMutex  = &(Mutex_).mMutex_,                       \
    .mCond_  = PTHREAD_COND_INITIALIZER,                \
    .mCond   = &(Mutex_).mCond_,                        \
}

struct Ert_RWMutexReader
{
    pthread_rwlock_t *mMutex;
};

struct Ert_RWMutexWriter
{
    pthread_rwlock_t *mMutex;
};

struct Ert_SharedMutex
{
    pthread_mutex_t mMutex;
};

struct Ert_SharedCond
{
    pthread_cond_t mCond;
};

struct Ert_Thread
{
    pthread_t mThread;
    char     *mName;
    bool      mJoined;
};

struct Ert_ThreadAttr
{
    pthread_attr_t mAttr;
};

/* -------------------------------------------------------------------------- */
struct Ert_Tid
ert_ownThreadId(void);

/* -------------------------------------------------------------------------- */
ERT_CHECKED struct Ert_Thread *
ert_createThread(
    struct Ert_Thread           *self,
    const char                  *aName,
    const struct Ert_ThreadAttr *aAttr,
    struct Ert_ThreadMethod      aMethod);

ERT_CHECKED int
ert_joinThread(struct Ert_Thread *self);

void
ert_cancelThread(struct Ert_Thread *self);

ERT_CHECKED struct Ert_Thread *
ert_closeThread(struct Ert_Thread *self);

ERT_CHECKED int
ert_killThread(struct Ert_Thread *self, int aSignal);

/* -------------------------------------------------------------------------- */
ERT_CHECKED struct Ert_ThreadAttr *
ert_createThreadAttr(struct Ert_ThreadAttr *self);

ERT_CHECKED struct Ert_ThreadAttr *
ert_destroyThreadAttr(struct Ert_ThreadAttr *self);

/* -------------------------------------------------------------------------- */
ERT_CHECKED struct Ert_ThreadSigMutex *
ert_createThreadSigMutex(struct Ert_ThreadSigMutex *self);

ERT_CHECKED struct Ert_ThreadSigMutex *
ert_destroyThreadSigMutex(struct Ert_ThreadSigMutex *self);

ERT_CHECKED struct Ert_ThreadSigMutex *
ert_lockThreadSigMutex(struct Ert_ThreadSigMutex *self);

unsigned
ert_ownThreadSigMutexLocked(struct Ert_ThreadSigMutex *self);

ERT_CHECKED struct Ert_ThreadSigMutex *
ert_unlockThreadSigMutex(struct Ert_ThreadSigMutex *self);

/* -------------------------------------------------------------------------- */
ERT_CHECKED pthread_mutex_t *
ert_createMutex(pthread_mutex_t *self);

ERT_CHECKED pthread_mutex_t *
ert_destroyMutex(pthread_mutex_t *self);

ERT_CHECKED pthread_mutex_t *
ert_lockMutex(pthread_mutex_t *self);

ERT_CHECKED pthread_mutex_t *
ert_unlockMutex(pthread_mutex_t *self);

ERT_CHECKED pthread_mutex_t *
ert_unlockMutexSignal(pthread_mutex_t *self, pthread_cond_t *aCond);

ERT_CHECKED pthread_mutex_t *
ert_unlockMutexBroadcast(pthread_mutex_t *self, pthread_cond_t *aCond);

/* -------------------------------------------------------------------------- */
ERT_CHECKED struct Ert_SharedMutex *
ert_createSharedMutex(struct Ert_SharedMutex *self);

ERT_CHECKED struct Ert_SharedMutex *
ert_destroySharedMutex(struct Ert_SharedMutex *self);

ERT_CHECKED struct Ert_SharedMutex *
ert_refSharedMutex(struct Ert_SharedMutex *self) ERT_THREAD_NOT_IMPLEMENTED;

ERT_CHECKED struct Ert_SharedMutex *
ert_unrefSharedMutex(struct Ert_SharedMutex *self) ERT_THREAD_NOT_IMPLEMENTED;

ERT_CHECKED struct Ert_SharedMutex *
ert_lockSharedMutex(
    struct Ert_SharedMutex *self, struct Ert_MutexRepairMethod aRepair);

ERT_CHECKED struct Ert_SharedMutex *
ert_unlockSharedMutex(struct Ert_SharedMutex *self);

ERT_CHECKED struct Ert_SharedMutex *
ert_unlockSharedMutexSignal(
    struct Ert_SharedMutex *self, struct Ert_SharedCond *aCond);

ERT_CHECKED struct Ert_SharedMutex *
ert_unlockSharedMutexBroadcast(
    struct Ert_SharedMutex *self, struct Ert_SharedCond *aCond);

/* -------------------------------------------------------------------------- */
ERT_CHECKED pthread_rwlock_t *
ert_createRWMutex(pthread_rwlock_t *self);

ERT_CHECKED pthread_rwlock_t *
ert_destroyRWMutex(pthread_rwlock_t *self);

/* -------------------------------------------------------------------------- */
ERT_CHECKED struct Ert_RWMutexReader *
ert_createRWMutexReader(struct Ert_RWMutexReader *self,
                    pthread_rwlock_t     *aMutex);

ERT_CHECKED struct Ert_RWMutexReader *
ert_destroyRWMutexReader(struct Ert_RWMutexReader *self);

/* -------------------------------------------------------------------------- */
ERT_CHECKED struct Ert_RWMutexWriter *
ert_createRWMutexWriter(struct Ert_RWMutexWriter *self,
                    pthread_rwlock_t     *aMutex);

ERT_CHECKED struct Ert_RWMutexWriter *
ert_destroyRWMutexWriter(struct Ert_RWMutexWriter *self);

/* -------------------------------------------------------------------------- */
ERT_CHECKED pthread_cond_t *
ert_createCond(pthread_cond_t *self);

ERT_CHECKED pthread_cond_t *
ert_destroyCond(pthread_cond_t *self);

ERT_CHECKED struct Ert_SharedCond *
ert_refSharedCond(struct Ert_SharedCond *self) ERT_THREAD_NOT_IMPLEMENTED;

ERT_CHECKED struct Ert_SharedCond *
ert_unrefSharedCond(struct Ert_SharedCond *self) ERT_THREAD_NOT_IMPLEMENTED;

void
ert_waitCond(pthread_cond_t *self, pthread_mutex_t *aMutex);

/* -------------------------------------------------------------------------- */
ERT_CHECKED struct Ert_SharedCond *
ert_createSharedCond(struct Ert_SharedCond *self);

ERT_CHECKED struct Ert_SharedCond *
ert_destroySharedCond(struct Ert_SharedCond *self);

ERT_CHECKED int
ert_waitSharedCond(struct Ert_SharedCond *self, struct Ert_SharedMutex *aMutex);

/* -------------------------------------------------------------------------- */
ERT_CHECKED struct Ert_ThreadSigMask *
ert_pushThreadSigMask(
    struct Ert_ThreadSigMask    *self,
    enum Ert_ThreadSigMaskAction aAction,
    const int               *aSigList);

ERT_CHECKED struct Ert_ThreadSigMask *
ert_popThreadSigMask(struct Ert_ThreadSigMask *self);

ERT_CHECKED int
ert_waitThreadSigMask(const int *aSigList);

/* -------------------------------------------------------------------------- */

ERT_END_C_SCOPE;

#endif /* ERT_THREAD_H */
