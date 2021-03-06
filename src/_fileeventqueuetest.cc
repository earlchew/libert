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

#include "ert/fileeventqueue.h"
#include "ert/bellsocketpair.h"
#include "ert/timescale.h"
#include "ert/macros.h"

#include "gtest/gtest.h"

static int eventCount_;

static int
armTestFileQueueActivity(struct Ert_FileEventQueueActivity *aActivity)
{
    return ert_armFileEventQueueActivity(
        aActivity,
        Ert_FileEventQueuePollRead,
        Ert_FileEventQueueActivityMethod(
            (char *) 0,
            ERT_LAMBDA(
                int, (char *self_),
                {
                    ++eventCount_;
                    return 0;
                })));
}

class FileEventQueueTest : public ::testing::Test
{
    void SetUp()
    {
        ASSERT_EQ(0, ert_createFileEventQueue(&mEventQueue_, 2));
        mEventQueue = &mEventQueue_;

        ASSERT_EQ(0, ert_createBellSocketPair(&mTestSocket_, 0));
        mTestSocket = &mTestSocket_;

        mEventActivity = 0;
    }

    void TearDown()
    {
        ASSERT_EQ(0, mEventActivity);

        mTestSocket = ert_closeBellSocketPair(mTestSocket);
        mEventQueue = ert_closeFileEventQueue(mEventQueue);
    }

protected:

    struct Ert_FileEventQueue  mEventQueue_;
    struct Ert_FileEventQueue *mEventQueue;

    struct Ert_BellSocketPair  mTestSocket_;
    struct Ert_BellSocketPair *mTestSocket;

    struct Ert_FileEventQueueActivity  mEventActivity_;
    struct Ert_FileEventQueueActivity *mEventActivity;
};

TEST_F(FileEventQueueTest, ArmReadyPollClose)
{
    /* Create the event queue file, arm it, make it ready, and poll the
     * event queue. This is the expected life cycle of the event file. */

    EXPECT_EQ(0, ert_createFileEventQueueActivity(
                  &mEventActivity_,
                  mEventQueue,
                  mTestSocket->mSocketPair->mParentSocket->mSocket->mFile));
    mEventActivity = &mEventActivity_;

    eventCount_ = 0;
    EXPECT_EQ(
        0, ert_pollFileEventQueueActivity(mEventQueue, &Ert_ZeroDuration));
    EXPECT_EQ(0, eventCount_);

    EXPECT_EQ(0, armTestFileQueueActivity(mEventActivity));

    eventCount_ = 0;
    EXPECT_EQ(
        0, ert_pollFileEventQueueActivity(mEventQueue, &Ert_ZeroDuration));
    EXPECT_EQ(0, eventCount_);

    EXPECT_EQ(0, ert_ringBellSocketPairChild(mTestSocket));
    EXPECT_EQ(1, ert_waitUnixSocketReadReady(
                  mTestSocket->mSocketPair->mParentSocket, 0));

    eventCount_ = 0;
    EXPECT_EQ(
        0, ert_pollFileEventQueueActivity(mEventQueue, &Ert_ZeroDuration));
    EXPECT_EQ(1, eventCount_);

    /* Ensure that simply polling again will not result in additional
     * activity since event activity has not yet been re-armed. */

    eventCount_ = 0;
    EXPECT_EQ(
        0, ert_pollFileEventQueueActivity(mEventQueue, &Ert_ZeroDuration));
    EXPECT_EQ(0, eventCount_);

    /* Now re-arm the event activity, and verify that polling results
     * in that activity being detected. */

    EXPECT_EQ(0, armTestFileQueueActivity(mEventActivity));
    eventCount_ = 0;
    EXPECT_EQ(
        0, ert_pollFileEventQueueActivity(mEventQueue, &Ert_ZeroDuration));
    EXPECT_EQ(1, eventCount_);

    eventCount_ = 0;
    EXPECT_EQ(
        0, ert_pollFileEventQueueActivity(mEventQueue, &Ert_ZeroDuration));
    EXPECT_EQ(0, eventCount_);

    mEventActivity = ert_closeFileEventQueueActivity(mEventActivity);

    eventCount_ = 0;
    EXPECT_EQ(
        0, ert_pollFileEventQueueActivity(mEventQueue, &Ert_ZeroDuration));
    EXPECT_EQ(0, eventCount_);
}

TEST_F(FileEventQueueTest, ReadyArmPollClose)
{
    /* Create the event queue file, make it ready, arm it and poll the
     * event queue. This is the alternate expected life cycle of the
     * event file. */

    EXPECT_EQ(0, ert_createFileEventQueueActivity(
                  &mEventActivity_,
                  mEventQueue,
                  mTestSocket->mSocketPair->mParentSocket->mSocket->mFile));
    mEventActivity = &mEventActivity_;

    eventCount_ = 0;
    EXPECT_EQ(
        0, ert_pollFileEventQueueActivity(mEventQueue, &Ert_ZeroDuration));
    EXPECT_EQ(0, eventCount_);

    EXPECT_EQ(0, ert_ringBellSocketPairChild(mTestSocket));
    EXPECT_EQ(1, ert_waitUnixSocketReadReady(
                  mTestSocket->mSocketPair->mParentSocket, 0));

    EXPECT_EQ(0, armTestFileQueueActivity(mEventActivity));

    eventCount_ = 0;
    EXPECT_EQ(
        0, ert_pollFileEventQueueActivity(mEventQueue, &Ert_ZeroDuration));
    EXPECT_EQ(1, eventCount_);

    mEventActivity = ert_closeFileEventQueueActivity(mEventActivity);

    eventCount_ = 0;
    EXPECT_EQ(
        0, ert_pollFileEventQueueActivity(mEventQueue, &Ert_ZeroDuration));
    EXPECT_EQ(0, eventCount_);
}

TEST_F(FileEventQueueTest, ArmPollReadyClose)
{
    /* Create the event queue file, arm it, poll the event queue, then
     * make it ready. This is the alternate expected life cycle of the
     * event file. */

    EXPECT_EQ(0, ert_createFileEventQueueActivity(
                  &mEventActivity_,
                  mEventQueue,
                  mTestSocket->mSocketPair->mParentSocket->mSocket->mFile));
    mEventActivity = &mEventActivity_;

    eventCount_ = 0;
    EXPECT_EQ(
        0, ert_pollFileEventQueueActivity(mEventQueue, &Ert_ZeroDuration));
    EXPECT_EQ(0, eventCount_);

    EXPECT_EQ(0, armTestFileQueueActivity(mEventActivity));

    eventCount_ = 0;
    EXPECT_EQ(
        0, ert_pollFileEventQueueActivity(mEventQueue, &Ert_ZeroDuration));
    EXPECT_EQ(0, eventCount_);

    EXPECT_EQ(0, ert_ringBellSocketPairChild(mTestSocket));
    EXPECT_EQ(1, ert_waitUnixSocketReadReady(
                  mTestSocket->mSocketPair->mParentSocket, 0));

    mEventActivity = ert_closeFileEventQueueActivity(mEventActivity);

    eventCount_ = 0;
    EXPECT_EQ(
        0, ert_pollFileEventQueueActivity(mEventQueue, &Ert_ZeroDuration));
    EXPECT_EQ(0, eventCount_);
}

TEST_F(FileEventQueueTest, ArmClose)
{
    /* Create the event queue file, arm it but do not poll the event queue.
     * Simply close the event queue file, and then verify that it has
     * taken itself off the event queue. */

    EXPECT_EQ(0, ert_createFileEventQueueActivity(
                  &mEventActivity_,
                  mEventQueue,
                  mTestSocket->mSocketPair->mParentSocket->mSocket->mFile));
    mEventActivity = &mEventActivity_;

    EXPECT_EQ(0, armTestFileQueueActivity(mEventActivity));

    mEventActivity = ert_closeFileEventQueueActivity(mEventActivity);

    eventCount_ = 0;
    EXPECT_EQ(
        0, ert_pollFileEventQueueActivity(mEventQueue, &Ert_ZeroDuration));
    EXPECT_EQ(0, eventCount_);
}

TEST_F(FileEventQueueTest, ArmReadyClose)
{
    /* Create the event queue file, arm it, make it ready, but do not poll
     * the event queue. Simply close the event queue file, and then verify
     * that it has taken itself off the event queue. */

    EXPECT_EQ(0, ert_createFileEventQueueActivity(
                  &mEventActivity_,
                  mEventQueue,
                  mTestSocket->mSocketPair->mParentSocket->mSocket->mFile));
    mEventActivity = &mEventActivity_;

    EXPECT_EQ(0, armTestFileQueueActivity(mEventActivity));

    EXPECT_EQ(0, ert_ringBellSocketPairChild(mTestSocket));
    EXPECT_EQ(1, ert_waitUnixSocketReadReady(
                  mTestSocket->mSocketPair->mParentSocket, 0));

    mEventActivity = ert_closeFileEventQueueActivity(mEventActivity);

    eventCount_ = 0;
    EXPECT_EQ(
        0, ert_pollFileEventQueueActivity(mEventQueue, &Ert_ZeroDuration));
    EXPECT_EQ(0, eventCount_);
}

TEST_F(FileEventQueueTest, ReadyArmClose)
{
    /* Create the event queue file, make it ready, arm it, but do not poll
     * the event queue. Simply close the event queue file, and then verify
     * that it has taken itself off the event queue. */

    EXPECT_EQ(0, ert_createFileEventQueueActivity(
                  &mEventActivity_,
                  mEventQueue,
                  mTestSocket->mSocketPair->mParentSocket->mSocket->mFile));
    mEventActivity = &mEventActivity_;

    EXPECT_EQ(0, ert_ringBellSocketPairChild(mTestSocket));
    EXPECT_EQ(1, ert_waitUnixSocketReadReady(
                  mTestSocket->mSocketPair->mParentSocket, 0));

    EXPECT_EQ(0, armTestFileQueueActivity(mEventActivity));
    eventCount_ = 0;
    EXPECT_EQ(0, ert_pollFileEventQueueActivity(mEventQueue, &Ert_ZeroDuration));
    EXPECT_EQ(1, eventCount_);

    EXPECT_EQ(0, armTestFileQueueActivity(mEventActivity));

    mEventActivity = ert_closeFileEventQueueActivity(mEventActivity);

    eventCount_ = 0;
    EXPECT_EQ(
        0, ert_pollFileEventQueueActivity(mEventQueue, &Ert_ZeroDuration));
    EXPECT_EQ(0, eventCount_);
}

TEST_F(FileEventQueueTest, Close)
{
    /* Create the event queue file, then immediately close it to
     * verify that it can be cleaned up. */

    EXPECT_EQ(0, ert_createFileEventQueueActivity(
                  &mEventActivity_,
                  mEventQueue,
                  mTestSocket->mSocketPair->mParentSocket->mSocket->mFile));
    mEventActivity = &mEventActivity_;

    mEventActivity = ert_closeFileEventQueueActivity(mEventActivity);

    eventCount_ = 0;
    EXPECT_EQ(
        0, ert_pollFileEventQueueActivity(mEventQueue, &Ert_ZeroDuration));
    EXPECT_EQ(0, eventCount_);
}

TEST_F(FileEventQueueTest, ReadyClose)
{
    /* Create the event queue file, make it ready, then close it to
     * verify that it can be cleaned up. */

    EXPECT_EQ(0, ert_createFileEventQueueActivity(
                  &mEventActivity_,
                  mEventQueue,
                  mTestSocket->mSocketPair->mParentSocket->mSocket->mFile));
    mEventActivity = &mEventActivity_;

    EXPECT_EQ(0, ert_ringBellSocketPairChild(mTestSocket));
    EXPECT_EQ(1, ert_waitUnixSocketReadReady(
                  mTestSocket->mSocketPair->mParentSocket, 0));

    mEventActivity = ert_closeFileEventQueueActivity(mEventActivity);

    eventCount_ = 0;
    EXPECT_EQ(
        0, ert_pollFileEventQueueActivity(mEventQueue, &Ert_ZeroDuration));
    EXPECT_EQ(0, eventCount_);
}

#include "_test_.h"
