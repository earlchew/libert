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

#include "thread_.h"

#include <unistd.h>

#include "gtest/gtest.h"

static int sigFpeCount_;

static void
sigFpeAction_(int)
{
    ++sigFpeCount_;
}

TEST(ThreadTest, ThreadSigMutex)
{
    struct ThreadSigMutex sigMutex;

    createThreadSigMutex(&sigMutex);

    struct sigaction prevAction;
    struct sigaction nextAction;

    nextAction.sa_handler = sigFpeAction_;
    nextAction.sa_flags   = 0;
    EXPECT_FALSE(sigfillset(&nextAction.sa_mask));

    EXPECT_FALSE(sigaction(SIGFPE, &nextAction, &prevAction));

    EXPECT_FALSE(raise(SIGFPE));
    EXPECT_EQ(1, sigFpeCount_);

    EXPECT_FALSE(raise(SIGFPE));
    EXPECT_EQ(2, sigFpeCount_);

    lockThreadSigMutex(&sigMutex);
    {
        // Verify that the lock also excludes the delivery of signals
        // while the lock is taken.

        EXPECT_FALSE(raise(SIGFPE));
        EXPECT_EQ(2, sigFpeCount_);

        EXPECT_FALSE(raise(SIGFPE));
        EXPECT_EQ(2, sigFpeCount_);
    }
    unlockThreadSigMutex(&sigMutex);

    EXPECT_EQ(3, sigFpeCount_);

    EXPECT_FALSE(raise(SIGFPE));
    EXPECT_EQ(4, sigFpeCount_);

    EXPECT_FALSE(raise(SIGFPE));
    EXPECT_EQ(5, sigFpeCount_);

    EXPECT_FALSE(sigaction(SIGFPE, &prevAction, 0));

    destroyThreadSigMutex(&sigMutex);
}

#include "../googletest/src/gtest_main.cc"