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
#include "ert/process.h"

#include <unistd.h>

#include <sys/mman.h>

#include <valgrind/valgrind.h>

#include "gtest/gtest.h"

class ThreadTest : public ::testing::Test
{
    void SetUp()
    {
        ASSERT_EQ(0, Ert_Process_init(&mModule_, __FILE__));
        mModule= &mModule_;
    }

    void TearDown()
    {
        mModule = Ert_Process_exit(mModule);
    }

private:

    struct Ert_ProcessModule  mModule_;
    struct Ert_ProcessModule *mModule;
};

TEST_F(ThreadTest, MutexDestroy)
{
    pthread_mutex_t  mutex_ = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t *mutex  = &mutex_;

    mutex = ert_destroyMutex(mutex);
}

TEST_F(ThreadTest, CondDestroy)
{
    pthread_cond_t  cond_ = PTHREAD_COND_INITIALIZER;
    pthread_cond_t *cond  = &cond_;

    cond = ert_destroyCond(cond);
}

TEST_F(ThreadTest, RWMutexDestroy)
{
    pthread_rwlock_t  rwlock_ = PTHREAD_RWLOCK_INITIALIZER;
    pthread_rwlock_t *rwlock  = &rwlock_;

    rwlock = ert_destroyRWMutex(rwlock);
}

static int sigTermCount_;

static void
sigTermAction_(int)
{
    ++sigTermCount_;
}

TEST_F(ThreadTest, ThreadSigMutex)
{
    struct Ert_ThreadSigMutex  sigMutex_;
    struct Ert_ThreadSigMutex *sigMutex = 0;

    sigMutex = ert_createThreadSigMutex(&sigMutex_);

    struct sigaction prevAction;
    struct sigaction nextAction;

    nextAction.sa_handler = sigTermAction_;
    nextAction.sa_flags   = 0;
    EXPECT_FALSE(sigfillset(&nextAction.sa_mask));

    EXPECT_FALSE(sigaction(SIGTERM, &nextAction, &prevAction));

    EXPECT_FALSE(raise(SIGTERM));
    EXPECT_EQ(1, sigTermCount_);

    EXPECT_FALSE(raise(SIGTERM));
    EXPECT_EQ(2, sigTermCount_);

    struct Ert_ThreadSigMutex *lock = ert_lockThreadSigMutex(sigMutex);
    {
        // Verify that the lock also excludes the delivery of signals
        // while the lock is taken.

        EXPECT_FALSE(raise(SIGTERM));
        EXPECT_EQ(2, sigTermCount_);

        EXPECT_FALSE(raise(SIGTERM));
        EXPECT_EQ(2, sigTermCount_);
    }
    lock = ert_unlockThreadSigMutex(lock);

    EXPECT_EQ(3, sigTermCount_);

    EXPECT_FALSE(raise(SIGTERM));
    EXPECT_EQ(4, sigTermCount_);

    EXPECT_FALSE(raise(SIGTERM));
    EXPECT_EQ(5, sigTermCount_);

    EXPECT_FALSE(sigaction(SIGTERM, &prevAction, 0));

    sigMutex = ert_destroyThreadSigMutex(sigMutex);
}

struct Ert_SharedMutexTestState
{
    struct Ert_SharedMutex  mMutex_;
    struct Ert_SharedMutex *mMutex;
    bool                mRepaired;
};

TEST_F(ThreadTest, ThreadSharedMutex)
{
    void *state_ = mmap(0,
                        sizeof(struct Ert_SharedMutexTestState),
                        PROT_READ | PROT_WRITE,
                        MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    EXPECT_FALSE(MAP_FAILED == state_);

    auto *state = reinterpret_cast<struct Ert_SharedMutexTestState *>(state_);

    {
        state->mMutex = ert_createSharedMutex(&state->mMutex_);
        EXPECT_TRUE(state->mMutex);

        state->mMutex = ert_destroySharedMutex(state->mMutex);
        EXPECT_FALSE(state->mMutex);
    }

    {
        state->mMutex = ert_createSharedMutex(&state->mMutex_);
        EXPECT_TRUE(state->mMutex);
        EXPECT_EQ(&state->mMutex_, state->mMutex);

        pid_t childpid = fork();

        EXPECT_NE(-1, childpid);

        if ( ! childpid)
        {
            auto mutex =
                ert_lockSharedMutex(
                    state->mMutex,
                    Ert_MutexRepairMethod(
                        state,
                        ERT_LAMBDA(
                            int, (struct Ert_SharedMutexTestState *),
                            {
                                return -1;
                            })));

            if (mutex != state->mMutex)
            {
                fprintf(stderr, "%s %u\n", __FILE__, __LINE__);
                execl("/bin/false", "false", (char *) 0);
                _exit(EXIT_FAILURE);
            }

            execl("/bin/true", "true", (char *) 0);
            _exit(EXIT_FAILURE);
        }

        int status;
        EXPECT_EQ(0, ert_reapProcessChild(Ert_Pid(childpid), &status));
        EXPECT_TRUE(WIFEXITED(status));
        EXPECT_EQ(EXIT_SUCCESS, WEXITSTATUS(status));

        {
            state->mRepaired = false;

            auto mutex =
                ert_lockSharedMutex(
                    state->mMutex,
                    Ert_MutexRepairMethod(
                        state,
                        ERT_LAMBDA(
                            int, (struct Ert_SharedMutexTestState *self),
                            {
                                self->mRepaired = true;
                                return 0;
                            })));

            EXPECT_EQ(mutex, state->mMutex);
            EXPECT_TRUE(state->mRepaired);
        }
    }

    EXPECT_EQ(0, munmap(state, sizeof(*state)));
}

#include "_test_.h"
