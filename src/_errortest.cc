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

#include "ert/error.h"
#include "ert/printf.h"
#include "ert/file.h"
#include "ert/void.h"
#include "ert/process.h"

#include <unistd.h>

#include "gtest/gtest.h"

class ErrorTest : public ::testing::Test
{
    void SetUp()
    {
        ASSERT_EQ(0, Ert_Error_init(&mErrorModule_));
        mErrorModule = &mErrorModule_;

        ASSERT_EQ(0, Ert_Process_init(&mProcessModule_, __FILE__));
        mProcessModule = &mProcessModule_;
    }

    void TearDown()
    {
        mProcessModule = Ert_Process_exit(mProcessModule);
        mErrorModule   = Ert_Error_exit(mErrorModule);
    }

private:

    struct Ert_ErrorModule  mErrorModule_;
    struct Ert_ErrorModule *mErrorModule;

    struct Ert_ProcessModule  mProcessModule_;
    struct Ert_ProcessModule *mProcessModule;
};

TEST_F(ErrorTest, ErrnoText)
{
    Ert_Error_warn_(EPERM, "Test EPERM");
    Ert_Error_warn_(0,     "Test errno 0");
    Ert_Error_warn_(-1,    "Test errno -1");
}

static int
ok()
{
    return 0;
}

static int
fail()
{
    return -1;
}

struct Class;

static struct Class *nilClass;

static int
print(const struct Class *self, FILE *aFile)
{
    return fprintf(aFile, "<Test Print Context>");
}

static int
testFinallyIfOk()
{
    int rc = -1;

    ERT_ERROR_IF(
        ok(),
        {
            errno = 0;
        });

    rc = 0;

Ert_Finally:

    ERT_FINALLY
    ({
        ert_finally_warn_if(rc, nilClass, print);
    });

    return rc;
}

static int
testFinallyIfFail_0()
{
    errno = 0;
    return -1;
}

static int
testFinallyIfFail_1()
{
    int rc = -1;

    ERT_ERROR_IF(
        fail(),
        {
            errno = -1;
        });

    rc = 0;

Ert_Finally:

    ERT_FINALLY
    ({
        ert_finally_warn_if(rc,
                            nilClass, print,
                            "Error context at testFinallyIfFail_1");

        ERT_ABORT_UNLESS(
            testFinallyIfFail_0());
    });

    return rc;
}

static int
testFinallyIfFail_2()
{
    int rc = -1;

    ERT_ERROR_IF(
        testFinallyIfFail_1(),
        {
            errno = -2;
        });

    rc = 0;

Ert_Finally:

    ERT_FINALLY
    ({
        ert_finally_warn_if(rc,
                            nilClass, print,
                            "Error context at testFinallyIfFail_2");

        ERT_ABORT_UNLESS(
            testFinallyIfFail_1());
    });

    return rc;
}

TEST_F(ErrorTest, FinallyIf)
{
    int errCode;
    int sigErrCode;

    EXPECT_EQ(0, testFinallyIfOk());
    EXPECT_EQ(0u, ert_ownErrorFrameOffset_());
    ert_restartErrorFrameSequence_();

    EXPECT_EQ(-1, testFinallyIfFail_1());
    errCode = errno;
    EXPECT_EQ(1u, ert_ownErrorFrameOffset_());
    EXPECT_EQ(-1, ert_ownErrorFrame_(Ert_ErrorFrameStackThread, 0)->mErrno);
    EXPECT_EQ(0,  ert_ownErrorFrame_(Ert_ErrorFrameStackThread, 1));
    ert_logErrorFrameSequence();
    Ert_Error_warn_(errCode, "One level error frame test");
    ert_restartErrorFrameSequence_();

    EXPECT_EQ(-1, testFinallyIfFail_2());
    errCode = errno;
    EXPECT_EQ(2u, ert_ownErrorFrameOffset_());
    EXPECT_EQ(-1, ert_ownErrorFrame_(Ert_ErrorFrameStackThread, 0)->mErrno);
    EXPECT_EQ(-2, ert_ownErrorFrame_(Ert_ErrorFrameStackThread, 1)->mErrno);
    EXPECT_EQ(0,  ert_ownErrorFrame_(Ert_ErrorFrameStackThread, 2));
    ert_logErrorFrameSequence();
    Ert_Error_warn_(errCode, "Two level error frame test");
    ert_restartErrorFrameSequence_();

    EXPECT_EQ(-1, testFinallyIfFail_2());
    errCode = errno;

    enum Ert_ErrorFrameStackKind stackKind =
        ert_switchErrorFrameStack(Ert_ErrorFrameStackSignal);
    EXPECT_EQ(Ert_ErrorFrameStackThread, stackKind);

    EXPECT_EQ(-1, testFinallyIfFail_1());
    sigErrCode = errno;
    EXPECT_EQ(1u, ert_ownErrorFrameOffset_());
    EXPECT_EQ(-1, ert_ownErrorFrame_(Ert_ErrorFrameStackThread, 0)->mErrno);
    EXPECT_EQ(0,  ert_ownErrorFrame_(Ert_ErrorFrameStackThread, 1));
    ert_logErrorFrameSequence();
    Ert_Error_warn_(sigErrCode, "Signal stack one level error frame test");
    ert_restartErrorFrameSequence_();

    stackKind = ert_switchErrorFrameStack(stackKind);
    EXPECT_EQ(Ert_ErrorFrameStackSignal, stackKind);

    EXPECT_EQ(2u, ert_ownErrorFrameOffset_());
    EXPECT_EQ(-1, ert_ownErrorFrame_(Ert_ErrorFrameStackThread, 0)->mErrno);
    EXPECT_EQ(-2, ert_ownErrorFrame_(Ert_ErrorFrameStackThread, 1)->mErrno);
    EXPECT_EQ(0,  ert_ownErrorFrame_(Ert_ErrorFrameStackThread, 2));
    ert_logErrorFrameSequence();
    Ert_Error_warn_(errCode, "Two level error frame test");
    ert_restartErrorFrameSequence_();
}

static int
testDeeplyNested(int aLevel)
{
    int rc = -1;

    if (aLevel)
        ERT_ERROR_IF(testDeeplyNested(aLevel-1));
    else
    {
        ERT_ERROR_IF(
            -1,
            {
                errno = EACCES;
            });
    }

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

TEST_F(ErrorTest, OffsetCounting)
{
    EXPECT_EQ(-1, testFinallyIfFail_1());
    EXPECT_EQ(1u, ert_ownErrorFrameOffset_());

    struct Ert_ErrorFrameSequence frameSequence = ert_pushErrorFrameSequence();

    EXPECT_EQ(0u, ert_ownErrorFrameOffset_());

    EXPECT_EQ(-1, testFinallyIfFail_2());
    EXPECT_EQ(2u, ert_ownErrorFrameOffset_());

    ert_popErrorFrameSequence(frameSequence);

    EXPECT_EQ(1u, ert_ownErrorFrameOffset_());
}

TEST_F(ErrorTest, DeeplyNested)
{
    long pageSize = sysconf(_SC_PAGESIZE);

    ert_restartErrorFrameSequence_();
    testDeeplyNested(pageSize);
    ert_logErrorFrameSequence();
}

TEST_F(ErrorTest, FreezeThaw)
{
    int errCode;

    struct Ert_File  tempFile_;
    struct Ert_File *tempFile = 0;

    ASSERT_EQ(
        0, ert_temporaryFile(&tempFile_, 0));
    tempFile = &tempFile_;

    EXPECT_EQ(-1, testFinallyIfFail_2());
    errCode = errno;
    EXPECT_EQ(-2, errCode);
    EXPECT_EQ(2u, ert_ownErrorFrameOffset_());
    EXPECT_EQ(-1, ert_ownErrorFrame_(Ert_ErrorFrameStackThread, 0)->mErrno);
    EXPECT_EQ(-2, ert_ownErrorFrame_(Ert_ErrorFrameStackThread, 1)->mErrno);
    EXPECT_EQ(0,  ert_ownErrorFrame_(Ert_ErrorFrameStackThread, 2));
    ert_logErrorFrameSequence();

    EXPECT_EQ(2u, ert_ownErrorFrameOffset_());

    struct Ert_ErrorFrameSequence frameSequence = ert_pushErrorFrameSequence();

    EXPECT_EQ(
        2, ert_freezeErrorFrameSequence(tempFile->mFd, &frameSequence));

    ert_popErrorFrameSequence(frameSequence);

    EXPECT_EQ(
        0, ert_lseekFile(tempFile, 0, Ert_WhenceTypeStart));

    EXPECT_EQ(
        -1, ert_thawErrorFrameSequence(tempFile->mFd));

    errCode = errno;
    EXPECT_EQ(-2, errCode);
    EXPECT_EQ(2u, ert_ownErrorFrameOffset_());
    EXPECT_EQ(-1, ert_ownErrorFrame_(Ert_ErrorFrameStackThread, 0)->mErrno);
    EXPECT_EQ(-2, ert_ownErrorFrame_(Ert_ErrorFrameStackThread, 1)->mErrno);
    EXPECT_EQ(0,  ert_ownErrorFrame_(Ert_ErrorFrameStackThread, 2));

    tempFile = ert_closeFile(tempFile);
}

TEST_F(ErrorTest, ChildError)
{
    /* Failure in child postfork */

    errno = 0;

    struct Ert_Pid childPid =
        ert_forkProcessChild(
            Ert_ForkProcessInheritProcessGroup,
            Ert_Pgid(0),
            Ert_PreForkProcessMethod(
                ert_Void(),
                ERT_LAMBDA(
                    int, (
                        struct Ert_Void                 *self_,
                        const struct Ert_PreForkProcess *aFork),
                    {
                        return 0;
                    })),
            Ert_PostForkChildProcessMethod(
                ert_Void(),
                ERT_LAMBDA(
                    int, (struct Ert_Void *self_),
                    {
                        return testFinallyIfFail_2();
                    })),
            Ert_PostForkParentProcessMethod(
                ert_Void(),
                ERT_LAMBDA(
                    int, (struct Ert_Void *self_,
                          struct Ert_Pid   aChildPid),
                    {
                        abort();

                        errno = EINVAL;
                        return -1;
                    })),
            Ert_ForkProcessMethod(
                ert_Void(),
                ERT_LAMBDA(
                    int, (struct Ert_Void *self_),
                    {
                        abort();

                        errno = EINVAL;
                        return -1;
                    })));

    int errCode = errno;
    EXPECT_EQ(-1, childPid.mPid);
    EXPECT_EQ(-2, errCode);
    EXPECT_LT(2u, ert_ownErrorFrameOffset_());
    EXPECT_EQ(-1, ert_ownErrorFrame_(Ert_ErrorFrameStackThread, 0)->mErrno);
    EXPECT_EQ(-2, ert_ownErrorFrame_(Ert_ErrorFrameStackThread, 1)->mErrno);
    EXPECT_TRUE(ert_ownErrorFrame_(Ert_ErrorFrameStackThread, 2));
    ert_logErrorFrameSequence();
}

#include "_test_.h"
