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
#include "ert/timekeeping.h"
#include "ert/bellsocketpair.h"
#include "ert/fdset.h"
#include "ert/thread.h"
#include "ert/macros.h"

#include <string>

#include <unistd.h>

#include <sys/mman.h>
#include <sys/resource.h>

#include <valgrind/valgrind.h>

#include "gtest/gtest.h"

class ProcessTest : public ::testing::Test
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

TEST_F(ProcessTest, ProcessSignalName)
{
    struct Ert_ProcessSignalName sigName;

    static const char nsigFormat[] = "signal %zu";

    char nsigName[sizeof(nsigFormat) + CHAR_BIT * sizeof(size_t)];
    EXPECT_FALSE(0 > sprintf(nsigName, nsigFormat, (size_t) NSIG));

    EXPECT_EQ(std::string("SIGHUP"),
              ert_formatProcessSignalName(&sigName, SIGHUP));
    EXPECT_EQ(std::string("signal 0"),
              ert_formatProcessSignalName(&sigName, 0));
    EXPECT_EQ(std::string("signal -1"),
              ert_formatProcessSignalName(&sigName, -1));
    EXPECT_EQ(std::string(nsigName),
              ert_formatProcessSignalName(&sigName, NSIG));
}

TEST_F(ProcessTest, ProcessState)
{
    EXPECT_EQ(Ert_ProcessState::Ert_ProcessStateError,
              ert_fetchProcessState(Ert_Pid(-1)).mState);

    EXPECT_EQ(Ert_ProcessState::Ert_ProcessStateRunning,
              ert_fetchProcessState(ert_ownProcessId()).mState);
}

TEST_F(ProcessTest, ProcessStatus)
{
    EXPECT_EQ(Ert_ChildProcessState::Ert_ChildProcessStateError,
              ert_monitorProcessChild(ert_ownProcessId()).mChildState);

    struct Ert_Pid childpid = Ert_Pid(fork());

    EXPECT_NE(-1, childpid.mPid);

    if ( ! childpid.mPid)
    {
        execl("/bin/true", "true", (char *) 0);
        _exit(EXIT_FAILURE);
    }

    while (Ert_ChildProcessState::Ert_ChildProcessStateRunning ==
           ert_monitorProcessChild(childpid).mChildState)
        continue;

    EXPECT_EQ(Ert_ChildProcessState::Ert_ChildProcessStateExited,
              ert_monitorProcessChild(childpid).mChildState);

    EXPECT_EQ(childpid.mPid, ert_waitProcessChildren().mPid);

    int status;
    EXPECT_EQ(0, ert_reapProcessChild(childpid, &status));
    EXPECT_EQ(0, status);

    EXPECT_EQ(0, ert_waitProcessChildren().mPid);
}

static int sigTermCount_;

static void
sigTermAction_(int)
{
    ++sigTermCount_;
}

TEST_F(ProcessTest, ProcessAppLock)
{
    struct sigaction nextAction;
    struct sigaction prevAction;

    nextAction.sa_handler = sigTermAction_;
    nextAction.sa_flags   = 0;
    EXPECT_FALSE(sigfillset(&nextAction.sa_mask));

    EXPECT_FALSE(sigaction(SIGTERM, &nextAction, &prevAction));

    EXPECT_FALSE(raise(SIGTERM));
    EXPECT_EQ(1, sigTermCount_);

    EXPECT_FALSE(raise(SIGTERM));
    EXPECT_EQ(2, sigTermCount_);

    struct Ert_ProcessAppLock *appLock = ert_createProcessAppLock();
    {
        // Verify that the application lock also excludes the delivery
        // of signals while the lock is taken.

        EXPECT_FALSE(raise(SIGTERM));
        EXPECT_EQ(2, sigTermCount_);

        EXPECT_FALSE(raise(SIGTERM));
        EXPECT_EQ(2, sigTermCount_);
    }
    appLock = ert_destroyProcessAppLock(appLock);

    EXPECT_EQ(3, sigTermCount_);

    EXPECT_FALSE(raise(SIGTERM));
    EXPECT_EQ(4, sigTermCount_);

    EXPECT_FALSE(raise(SIGTERM));
    EXPECT_EQ(5, sigTermCount_);

    EXPECT_FALSE(sigaction(SIGTERM, &prevAction, 0));
}

struct DaemonState
{
    int mErrno;
    int mSigMask[NSIG];

    struct Ert_BellSocketPair *mBellSocket;
};

static int
daemonProcess_(struct DaemonState *self)
{
    sigset_t sigMask;

    self->mErrno = 0;

    if (pthread_sigmask(SIG_BLOCK, 0, &sigMask))
        self->mErrno = errno;
    else
    {
        for (unsigned sx = 0; ERT_NUMBEROF(self->mSigMask) > sx; ++sx)
            self->mSigMask[sx] = sigismember(&sigMask, sx);
    }

    // Use a sanctioned Posix memory barrier to ensure that the updates
    // to the shared memory region are visible before signalling to the
    // parent process that the results are ready.

    pthread_mutex_t  mutex_;
    pthread_mutex_t *mutex = ert_createMutex(&mutex_);

    while (ert_unlockMutex(ert_lockMutex(mutex)))
        break;

    ert_closeBellSocketPairParent(self->mBellSocket);
    if (ert_ringBellSocketPairChild(self->mBellSocket) ||
        ert_waitBellSocketPairChild(self->mBellSocket, 0))
    {
        execl("/bin/false", "false", (char *) 0);
    }
    else
    {
        execl("/bin/true", "true", (char *) 0);
    }

    _exit(EXIT_FAILURE);
}

TEST_F(ProcessTest, ProcessDaemon)
{
    struct Ert_BellSocketPair  bellSocket_;
    struct Ert_BellSocketPair *bellSocket = 0;

    EXPECT_EQ(0, ert_createBellSocketPair(&bellSocket_, 0));
    bellSocket = &bellSocket_;

    struct DaemonState *daemonState =
        reinterpret_cast<struct DaemonState *>(
            mmap(0,
                 sizeof(*daemonState),
                 PROT_READ | PROT_WRITE,
                 MAP_ANONYMOUS | MAP_SHARED, -1, 0));

    EXPECT_NE(MAP_FAILED, (void *) daemonState);

    daemonState->mErrno      = ENOSYS;
    daemonState->mBellSocket = bellSocket;

    struct Ert_Pid daemonPid = ert_forkProcessDaemon(
        Ert_PreForkProcessMethod(
            bellSocket,
            ERT_LAMBDA(
                int, (struct Ert_BellSocketPair       *aBellSocket,
                      const struct Ert_PreForkProcess *aPreFork),
                {
                    int rc_ = -1;

                    do
                    {
                        struct Ert_SocketPair *socketPair =
                            aBellSocket->mSocketPair;

                        if (ert_insertFdSet(
                                aPreFork->mWhitelistFds,
                                socketPair->mParentSocket->mSocket->mFile->mFd))
                            break;

                        if (ert_insertFdSet(
                                aPreFork->mWhitelistFds,
                                socketPair->mChildSocket->mSocket->mFile->mFd))
                            break;

                        rc_ = 0;

                    } while (0);

                    return rc_;
                })),
        Ert_PostForkChildProcessMethodNil(),
        Ert_PostForkParentProcessMethodNil(),
        Ert_ForkProcessMethod(daemonState, daemonProcess_));

    ert_closeBellSocketPairChild(bellSocket);

    EXPECT_NE(-1, daemonPid.mPid);
    EXPECT_EQ(daemonPid.mPid, getpgid(daemonPid.mPid));
    EXPECT_EQ(getsid(0), getsid(daemonPid.mPid));

    EXPECT_EQ(0, ert_waitBellSocketPairParent(bellSocket, 0));

    EXPECT_EQ(0, daemonState->mErrno);

    {
        sigset_t sigMask;

        EXPECT_EQ(0, pthread_sigmask(SIG_BLOCK, 0, &sigMask));

        for (unsigned sx = 0; ERT_NUMBEROF(daemonState->mSigMask) > sx; ++sx)
            EXPECT_EQ(
                daemonState->mSigMask[sx], sigismember(&sigMask, sx))
                << " failed at index " << sx;
    }

    EXPECT_EQ(0, munmap(daemonState, sizeof(*daemonState)));

    bellSocket = ert_closeBellSocketPair(bellSocket);
}

struct ProcessForkArg
{
    bool              mStart;
    pthread_cond_t    mCond_;
    pthread_cond_t   *mCond;
    pthread_mutex_t   mMutex_;
    pthread_mutex_t  *mMutex;
    struct Ert_FdSet  mFdSet_;
    struct Ert_FdSet *mFdSet;

    unsigned mNumFds;
    unsigned mNumForks;
};

struct ProcessForkTest
{
    int            mPipeFds[2];
    struct Ert_Pid mChildPid;
};

static unsigned
countFds(struct Ert_FdSet *aFdSet)
{
    struct rlimit fdLimit;

    if (getrlimit(RLIMIT_NOFILE, &fdLimit))
        abort();

    unsigned numFds = 0;

    for (unsigned fd = 0; fd < fdLimit.rlim_cur; ++fd)
    {
        if (ert_ownFdValid(fd))
        {
            if (aFdSet)
            {
                if (ert_insertFdSet(aFdSet, fd))
                    abort();
            }
            ++numFds;
        }
    }

    return numFds;
}

static int
filterFds(struct ProcessForkArg *aArg,
          struct Ert_FdSet *aBlacklist)
{
    return ert_visitFdSet(
        aArg->mFdSet,
        Ert_FdSetVisitor(
            aBlacklist,
            ERT_LAMBDA(
                int, (struct Ert_FdSet  *aBlacklist_,
                      struct Ert_FdRange aRange),
                {
                    for (int fd = aRange.mLhs; ; ++fd)
                    {
                        if (ert_removeFdSet(aBlacklist_, fd) &&
                            ENOENT != errno)
                        {
                            abort();
                        }
                        if (fd == aRange.mRhs)
                            break;
                    }

                    return 0;
                })));
}

static void
processForkTest_Trivial_()
{
    /* Simple with no prefork or postfork methods */

    struct Ert_Pid childPid =
        ert_forkProcessChild(
            Ert_ForkProcessInheritProcessGroup,
            Ert_Pgid(0),
            Ert_PreForkProcessMethodNil(),
            Ert_PostForkChildProcessMethodNil(),
            Ert_PostForkParentProcessMethodNil(),
            Ert_ForkProcessMethodNil());

    EXPECT_NE(-1, childPid.mPid);

    if ( ! childPid.mPid)
        abort();

    int status;
    EXPECT_EQ(0, ert_reapProcessChild(childPid, &status));
    EXPECT_EQ(0, (ert_extractProcessExitStatus(status, childPid).mStatus));
}

static void
processForkTest_CloseFds_(struct ProcessForkArg *aArg)
{
    /* Simple with no prefork or postfork methods */

    struct Ert_Pid childPid =
        ert_forkProcessChild(
            Ert_ForkProcessInheritProcessGroup,
            Ert_Pgid(0),
            Ert_PreForkProcessMethod(
                aArg,
                ERT_LAMBDA(
                    int, (struct ProcessForkArg           *aArg_,
                          const struct Ert_PreForkProcess *aPreFork),
                    {
                        int rc = -1;

                        do
                        {
                            if (ert_insertFdSetRange(
                                    aPreFork->mWhitelistFds,
                                    Ert_FdRange(0, INT_MAX)))
                            {
                                fprintf(stderr, "%u 1\n", __LINE__);
                                break;
                            }

                            if (ert_insertFdSetRange(
                                    aPreFork->mBlacklistFds,
                                    Ert_FdRange(0, INT_MAX)))
                            {
                                fprintf(stderr, "%u 2\n", __LINE__);
                                break;
                            }

                            if (-1 == filterFds(
                                    aArg_,
                                    aPreFork->mBlacklistFds))
                            {
                                fprintf(stderr, "%u 3\n", __LINE__);
                                break;
                            }

                            rc = 0;

                        } while (0);

                        return rc;
                    })),
            Ert_PostForkChildProcessMethodNil(),
            Ert_PostForkParentProcessMethodNil(),
            Ert_ForkProcessMethodNil());

    EXPECT_NE(-1, childPid.mPid);

    if ( ! childPid.mPid)
        abort();

    int status;
    EXPECT_EQ(0, ert_reapProcessChild(childPid, &status));
    EXPECT_EQ(0, (ert_extractProcessExitStatus(status, childPid).mStatus));
}

static void
processForkTest_Usual_(struct ProcessForkArg *aArg)
{
    /* Standard use case */

    struct ProcessForkTest forkTest;

    struct Ert_Pid childPid =
        ert_forkProcessChild(
            Ert_ForkProcessInheritProcessGroup,
            Ert_Pgid(0),
            Ert_PreForkProcessMethod(
                &forkTest,
                ERT_LAMBDA(
                    int, (
                        struct ProcessForkTest      *self,
                        const struct Ert_PreForkProcess *aFork),
                    {
                        /* Provide time for competing threads
                         * to also run this code. */

                        sleep((ert_ownThreadId().mTid / 2) % 3);
                        int err = pipe(self->mPipeFds);

                        err = err
                            ? err
                            : ert_insertFdSet(
                                aFork->mBlacklistFds,
                                self->mPipeFds[1]);

                        err = err
                            ? err
                            : ert_insertFdSet(
                                aFork->mWhitelistFds,
                                self->mPipeFds[1]);

                        return err;
                    })),
            Ert_PostForkChildProcessMethod(
                &forkTest,
                ERT_LAMBDA(
                    int, (struct ProcessForkTest *self),
                    {
                        return 0;
                    })),
            Ert_PostForkParentProcessMethod(
                &forkTest,
                ERT_LAMBDA(
                    int, (struct ProcessForkTest *self,
                          struct Ert_Pid              aChildPid),
                    {
                        self->mChildPid = aChildPid;

                        self->mPipeFds[1] = -1;

                        return 0;
                    })),
            Ert_ForkProcessMethod(
                &forkTest,
                ERT_LAMBDA(
                    int, (struct ProcessForkTest *self),
                    {
                        int rc = -1;

                        do
                        {
                            if ( ! ert_ownFdValid(STDIN_FILENO))
                            {
                                fprintf(stderr, "%u\n", __LINE__);
                                break;
                            }

                            if ( ! ert_ownFdValid(STDOUT_FILENO))
                            {
                                fprintf(stderr, "%u\n", __LINE__);
                                break;
                            }

                            if ( ! ert_ownFdValid(STDERR_FILENO))
                            {
                                fprintf(stderr, "%u\n", __LINE__);
                                break;
                            }

                            /* Two additional fds were opened,
                             * but one is closed, so the
                             * net difference should be one extra.
                             *
                             * There will be an additional extra
                             * fd for the file used to
                             * coordinate the process lock. */

                            unsigned openFds = countFds(0);

                            if (5 != openFds)
                            {
                                fprintf(
                                    stderr,
                                    "%u %u\n", __LINE__, openFds);
                                break;
                            }

                            if (ert_ownFdValid(self->mPipeFds[0]))
                            {
                                fprintf(stderr, "%u\n", __LINE__);
                                break;
                            }

                            if (1 != ert_writeFd(
                                    self->mPipeFds[1],
                                    "X", 1, 0))
                            {
                                fprintf(stderr, "%u\n", __LINE__);
                                break;
                            }

                            self->mPipeFds[1] =
                                ert_closeFd(self->mPipeFds[1]);

                            rc = 0;

                        } while (0);

                        return rc;
                    })));

    EXPECT_NE(-1, childPid.mPid);

    if ( ! childPid.mPid)
        abort();

    EXPECT_EQ(forkTest.mChildPid.mPid, childPid.mPid);

    EXPECT_EQ(-1, forkTest.mPipeFds[1]);

    char buf[1] = { '@' };

    EXPECT_EQ(
        1, ert_readFd(forkTest.mPipeFds[0], buf, sizeof(buf), 0));

    EXPECT_EQ('X', buf[0]);

    forkTest.mPipeFds[0] = ert_closeFd(forkTest.mPipeFds[0]);

    int status;
    EXPECT_EQ(0, ert_reapProcessChild(childPid, &status));
    EXPECT_EQ(0, (ert_extractProcessExitStatus(status, childPid).mStatus));
}

static void
processForkTest_FailedPreFork_()
{
    /* Failure in prefork */

    struct ProcessForkTest forkTest;

    errno = 0;

    struct Ert_Pid childPid =
        ert_forkProcessChild(
            Ert_ForkProcessInheritProcessGroup,
            Ert_Pgid(0),
            Ert_PreForkProcessMethod(
                &forkTest,
                ERT_LAMBDA(
                    int, (
                        struct ProcessForkTest      *self,
                        const struct Ert_PreForkProcess *aFork),
                    {
                        errno = EINVAL;
                        return -1;
                    })),
            Ert_PostForkChildProcessMethod(
                &forkTest,
                ERT_LAMBDA(
                    int, (struct ProcessForkTest *self),
                    {
                        abort();

                        errno = EINVAL;
                        return -1;
                    })),
            Ert_PostForkParentProcessMethod(
                &forkTest,
                ERT_LAMBDA(
                    int, (struct ProcessForkTest *self,
                          struct Ert_Pid              aChildPid),
                    {
                        abort();

                        errno = EINVAL;
                        return -1;
                    })),
            Ert_ForkProcessMethod(
                &forkTest,
                ERT_LAMBDA(
                    int, (struct ProcessForkTest *self),
                    {
                        abort();

                        errno = EINVAL;
                        return -1;
                    })));

    EXPECT_EQ(-1, childPid.mPid);
    EXPECT_EQ(EINVAL, errno);
}

static int
processForkTest_Error_()
{
    int rc = -1;

    ERROR_IF(
        true,
        {
            errno = EINVAL;
        });

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

static void
processForkTest_FailedChildPostFork_()
{
    /* Failure in child postfork */

    struct ProcessForkTest forkTest;

    errno = 0;

    struct Ert_Pid childPid =
        ert_forkProcessChild(
            Ert_ForkProcessInheritProcessGroup,
            Ert_Pgid(0),
            Ert_PreForkProcessMethod(
                &forkTest,
                ERT_LAMBDA(
                    int, (
                        struct ProcessForkTest      *self,
                        const struct Ert_PreForkProcess *aFork),
                    {
                        return 0;
                    })),
            Ert_PostForkChildProcessMethod(
                &forkTest,
                ERT_LAMBDA(
                    int, (struct ProcessForkTest *self),
                    {
                        return processForkTest_Error_();
                    })),
            Ert_PostForkParentProcessMethod(
                &forkTest,
                ERT_LAMBDA(
                    int, (struct ProcessForkTest *self,
                          struct Ert_Pid              aChildPid),
                    {
                        abort();

                        errno = EINVAL;
                        return -1;
                    })),
            Ert_ForkProcessMethod(
                &forkTest,
                ERT_LAMBDA(
                    int, (struct ProcessForkTest *self),
                    {
                        abort();

                        errno = EINVAL;
                        return -1;
                    })));

    EXPECT_EQ(-1, childPid.mPid);
    EXPECT_EQ(EINVAL, errno);
}

static void
processForkTest_FailedParentPostFork_()
{
    /* Failure in parent postfork */

    struct ProcessForkTest forkTest;

    errno = 0;

    struct Ert_Pid childPid =
        ert_forkProcessChild(
            Ert_ForkProcessInheritProcessGroup,
            Ert_Pgid(0),
            Ert_PreForkProcessMethod(
                &forkTest,
                ERT_LAMBDA(
                    int, (
                        struct ProcessForkTest      *self,
                        const struct Ert_PreForkProcess *aFork),
                    {
                        return 0;
                    })),
            Ert_PostForkChildProcessMethod(
                &forkTest,
                ERT_LAMBDA(
                    int, (struct ProcessForkTest *self),
                    {
                        return 0;
                    })),
            Ert_PostForkParentProcessMethod(
                &forkTest,
                ERT_LAMBDA(
                    int, (struct ProcessForkTest *self,
                          struct Ert_Pid              aChildPid),
                    {
                        return processForkTest_Error_();
                    })),
            Ert_ForkProcessMethod(
                &forkTest,
                ERT_LAMBDA(
                    int, (struct ProcessForkTest *self),
                    {
                        abort();

                        errno = EINVAL;
                        return -1;
                    })));

    EXPECT_EQ(-1, childPid.mPid);
    EXPECT_EQ(EINVAL, errno);
}

static void
processForkTest_(struct ProcessForkArg *aArg)
{
    pthread_mutex_t *lock = ert_lockMutex(aArg->mMutex);

    while ( ! aArg->mStart)
        ert_waitCond(aArg->mCond, lock);

    lock = ert_unlockMutex(lock);

    processForkTest_Trivial_();
    processForkTest_CloseFds_(aArg);
    processForkTest_Usual_(aArg);
    processForkTest_FailedPreFork_();
    processForkTest_FailedChildPostFork_();
    processForkTest_FailedParentPostFork_();
}

static void
processForkTest_Raw_(struct ProcessForkArg *aArg)
{
    pthread_mutex_t *lock = ert_lockMutex(aArg->mMutex);

    while ( ! aArg->mStart)
        ert_waitCond(aArg->mCond, lock);

    lock = ert_unlockMutex(lock);

    sleep((ert_ownThreadId().mTid / 2) % 3);

    pid_t childpid = fork();

    EXPECT_NE(-1, childpid);

    if ( ! childpid)
    {
        int rc = -1;

        do
        {
            unsigned openFds = countFds(0);

            if (openFds < aArg->mNumFds)
            {
                fprintf(stderr, "%u 1 %u %u\n",
                        __LINE__, openFds, aArg->mNumFds);
                break;
            }

            if (openFds > aArg->mNumFds + aArg->mNumForks)
            {
                fprintf(stderr, "%u 2 %u %u\n",
                        __LINE__, openFds, aArg->mNumFds);
                break;
            }

            rc = 0;

        } while (0);

        if (rc)
            execl("/bin/false", "false", (char *) 0);
        else
            execl("/bin/true", "true", (char *) 0);
        _exit(EXIT_FAILURE);
    }

    struct Ert_Pid childPid = Ert_Pid(childpid);

    int status;
    EXPECT_EQ(0, ert_reapProcessChild(childPid, &status));
    EXPECT_EQ(0, (ert_extractProcessExitStatus(status, childPid).mStatus));
}

TEST_F(ProcessTest, ProcessFork)
{
    static const char *threadName[2] =
    {
        "thread 1",
        "thread 2",
    };

    struct Ert_Thread  thread_[2];
    struct Ert_Thread *thread[2] = { 0, 0 };

    struct ProcessForkArg forkArg;

    forkArg.mStart    = false;
    forkArg.mMutex    = ert_createMutex(&forkArg.mMutex_);
    forkArg.mCond     = ert_createCond(&forkArg.mCond_);
    forkArg.mNumForks = ERT_NUMBEROF(thread);

    EXPECT_EQ(
        0, ert_createFdSet(&forkArg.mFdSet_));
    forkArg.mFdSet  = &forkArg.mFdSet_;
    forkArg.mNumFds = countFds(forkArg.mFdSet);

    pthread_mutex_t *lock = ert_lockMutex(forkArg.mMutex);
    forkArg.mStart = false;

    for (unsigned ix = 0; ix < ERT_NUMBEROF(thread); ++ix)
    {
        thread[ix] = ert_createThread(
            &thread_[ix],
            threadName[ix],
            0,
            Ert_ThreadMethod(
                &forkArg,
                ERT_LAMBDA(
                    int, (struct ProcessForkArg *self),
                    {
                        processForkTest_(self);
                        return 0;
                    })));
        EXPECT_TRUE(thread[ix]);
    }

    struct Ert_Thread  rawForkThread_;
    struct Ert_Thread *rawForkThread;

    rawForkThread = ert_createThread(
        &rawForkThread_,
        "rawfork",
        0,
        Ert_ThreadMethod(
            &forkArg,
            ERT_LAMBDA(
                int, (struct ProcessForkArg *self),
                {
                    processForkTest_Raw_(self);

                    return 0;
                })));
    EXPECT_TRUE(rawForkThread);

    forkArg.mStart = true;
    lock = ert_unlockMutexBroadcast(lock, forkArg.mCond);

    for (unsigned ix = 0; ix < ERT_NUMBEROF(thread); ++ix)
        thread[ix] = ert_closeThread(thread[ix]);

    rawForkThread = ert_closeThread(rawForkThread);

    int status;
    EXPECT_EQ(-1, wait(&status));
    EXPECT_EQ(ECHILD, errno);

    forkArg.mFdSet = ert_closeFdSet(forkArg.mFdSet);
    forkArg.mCond  = ert_destroyCond(forkArg.mCond);
    forkArg.mMutex = ert_destroyMutex(forkArg.mMutex);
}

static int
runSlave(void)
{
    int rc = -1;

    do
    {
        struct Ert_Pid childPid =
            ert_forkProcessChild(
                Ert_ForkProcessInheritProcessGroup,
                Ert_Pgid(0),
                Ert_PreForkProcessMethod(
                    (char *) "",
                    ERT_LAMBDA(
                        int, (char                        *self_,
                              const struct Ert_PreForkProcess *aPreFork),
                        {
                            return ert_fillFdSet(
                                aPreFork->mBlacklistFds);
                        })),
                Ert_PostForkChildProcessMethodNil(),
                Ert_PostForkParentProcessMethodNil(),
                Ert_ForkProcessMethodNil());

        if (-1 == childPid.mPid)
            break;

        int status;
        if (ert_reapProcessChild(childPid, &status))
            break;

        if (ert_extractProcessExitStatus(status, childPid).mStatus)
            break;

        rc = 0;

    } while (0);

    return rc;
}

TEST_F(ProcessTest, ProcessForkRecursiveParent)
{
    struct Ert_Pid childPid = ert_forkProcessChild(
        Ert_ForkProcessInheritProcessGroup,
        Ert_Pgid(0),
        Ert_PreForkProcessMethod(
            (char *) "",
            ERT_LAMBDA(
                int, (char                        *self_,
                      const struct Ert_PreForkProcess *aPreFork),
                {
                    return ert_fillFdSet(
                        aPreFork->mBlacklistFds);
                })),
        Ert_PostForkChildProcessMethodNil(),
        Ert_PostForkParentProcessMethod(
            (char *) "",
            ERT_LAMBDA(
                int, (char      *self_,
                      struct Ert_Pid aChildPid),
                {
                    return runSlave();
                })),
        Ert_ForkProcessMethodNil());

    EXPECT_NE(-1, childPid.mPid);

    int status;
    EXPECT_EQ(0, ert_reapProcessChild(childPid, &status));
    EXPECT_EQ(0, (ert_extractProcessExitStatus(status, childPid).mStatus));
}

TEST_F(ProcessTest, ProcessForkRecursiveChild)
{
    struct Ert_Pid childPid = ert_forkProcessChild(
        Ert_ForkProcessInheritProcessGroup,
        Ert_Pgid(0),
        Ert_PreForkProcessMethod(
            (char *) "",
            ERT_LAMBDA(
                int, (char                        *self_,
                      const struct Ert_PreForkProcess *aPreFork),
                {
                    return ert_fillFdSet(
                        aPreFork->mBlacklistFds);
                })),
        Ert_PostForkChildProcessMethod(
            (char *) "",
            ERT_LAMBDA(
                int, (char *self_),
                {
                    return runSlave();
                })),
        Ert_PostForkParentProcessMethodNil(),
        Ert_ForkProcessMethodNil());

    EXPECT_NE(-1, childPid.mPid);

    int status;
    EXPECT_EQ(0, ert_reapProcessChild(childPid, &status));
    EXPECT_EQ(0, (ert_extractProcessExitStatus(status, childPid).mStatus));
}

#include "../googletest/src/gtest_main.cc"
