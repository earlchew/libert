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

#include "process_.h"
#include "timekeeping_.h"

#include <unistd.h>

#include "gtest/gtest.h"

TEST(ProcessTest, ProcessState)
{
    EXPECT_EQ(ProcessStateError, fetchProcessState(-1));

    EXPECT_EQ(ProcessStateRunning, fetchProcessState(getpid()));
}

TEST(ProcessTest, ProcessStatus)
{
    EXPECT_EQ(ProcessStatusError, monitorProcess(getpid()));

    pid_t childpid = fork();

    EXPECT_NE(-1, childpid);

    if ( ! childpid)
        _exit(EXIT_SUCCESS);

    while (ProcessStatusRunning == monitorProcess(childpid))
        continue;

    EXPECT_EQ(ProcessStatusExited, monitorProcess(childpid));
}

TEST(ProcessTest, ProcessStartTime)
{
    struct BootClockTime starttime;
    EXPECT_EQ(0, fetchProcessStartTime_(getpid(), &starttime));

    struct BootClockTime before = bootclockTime();

    monotonicSleep(Duration(NSECS(Seconds(1))));

    pid_t pid = fork();

    EXPECT_NE(-1, pid);

    if ( ! pid)
        _exit(0);

    monotonicSleep(Duration(NSECS(Seconds(1))));

    struct BootClockTime after = bootclockTime();

    struct BootClockTime childstarttime;
    EXPECT_EQ(0, fetchProcessStartTime_(pid, &childstarttime));

    int status;
    EXPECT_EQ(0, reapProcess(pid, &status));

    struct ExitCode exitcode = extractProcessExitStatus(status);
    EXPECT_EQ(0, exitcode.mStatus);

    EXPECT_GE(before.bootclock.ns, starttime.bootclock.ns);
    EXPECT_LE(before.bootclock.ns, childstarttime.bootclock.ns);
    EXPECT_GE(after.bootclock.ns,  childstarttime.bootclock.ns);

    struct BootClockTime nostarttime;
    EXPECT_EQ(-1, fetchProcessStartTime_(pid, &nostarttime));
    EXPECT_EQ(ENOENT, errno);
}

#include "../googletest/src/gtest_main.cc"
