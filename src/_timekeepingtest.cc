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

#include "ert/timekeeping.h"
#include "ert/pipe.h"
#include "ert/process.h"

#include "gtest/gtest.h"

bool
operator==(const struct timespec &aLhs, const struct timespec &aRhs)
{
    return aLhs.tv_sec == aRhs.tv_sec && aLhs.tv_nsec == aRhs.tv_nsec;
}

bool
operator==(const struct timeval &aLhs, const struct timeval &aRhs)
{
    return aLhs.tv_sec == aRhs.tv_sec && aLhs.tv_usec == aRhs.tv_usec;
}

bool
operator==(const struct itimerval &aLhs, const struct itimerval &aRhs)
{
    return
        aLhs.it_value    == aRhs.it_value &&
        aLhs.it_interval == aRhs.it_interval;
}

TEST(TimeKeepingTest, NanoSecondConversion)
{
    {
        struct Ert_NanoSeconds tm = Ert_NanoSeconds(1);

        EXPECT_FALSE(ERT_MSECS(tm).ms - 1);
        EXPECT_FALSE(ERT_SECS(tm).s - 1);
    }

    {
        struct Ert_NanoSeconds tm = Ert_NanoSeconds(0 + 1000 * 1000);

        EXPECT_FALSE(ERT_MSECS(tm).ms - 1);
        EXPECT_FALSE(ERT_SECS(tm).s - 1);
    }

    {
        struct Ert_NanoSeconds tm = Ert_NanoSeconds(1 + 1000 * 1000);

        EXPECT_FALSE(ERT_MSECS(tm).ms - 2);
        EXPECT_FALSE(ERT_SECS(tm).s - 1);
    }

    {
        struct Ert_NanoSeconds tm = Ert_NanoSeconds(1000 * 1000 + 1000 * 1000 * 1000);

        EXPECT_FALSE(ERT_MSECS(tm).ms - 1001);
        EXPECT_FALSE(ERT_SECS(tm).s - 2);
    }
}

TEST(TimeKeepingTest, MicroSecondConversion)
{
    {
        struct Ert_MicroSeconds tm = Ert_MicroSeconds(1);

        EXPECT_EQ(tm.us, ERT_USECS(ERT_NSECS(tm)).us);

        EXPECT_FALSE(ERT_SECS(tm).s - 1);
    }

    {
        struct Ert_MicroSeconds tm = Ert_MicroSeconds(999999);

        EXPECT_EQ(tm.us, ERT_USECS(ERT_NSECS(tm)).us);

        EXPECT_FALSE(ERT_SECS(tm).s - 1);
    }
}

TEST(TimeKeepingTest, MilliSecondConversion)
{
    {
        struct Ert_MilliSeconds tm = Ert_MilliSeconds(1);

        EXPECT_EQ(tm.ms, ERT_MSECS(ERT_NSECS(tm)).ms);

        EXPECT_FALSE(ERT_SECS(tm).s - 1);
    }

    {
        struct Ert_MilliSeconds tm = Ert_MilliSeconds(999);

        EXPECT_EQ(tm.ms, ERT_MSECS(ERT_NSECS(tm)).ms);

        EXPECT_FALSE(ERT_SECS(tm).s - 1);
    }
}

TEST(TimeKeepingTest, SecondConversion)
{
    {
        struct Ert_Seconds tm = Ert_Seconds(0);

        EXPECT_FALSE(ERT_MSECS(tm).ms);
        EXPECT_FALSE(ERT_NSECS(tm).ns);
    }

    {
        struct Ert_Seconds tm = Ert_Seconds(1);

        EXPECT_FALSE(ERT_MSECS(tm).ms - 1000);
        EXPECT_FALSE(ERT_NSECS(tm).ns - 1000 * 1000 * 1000);
    }
}

TEST(TimeKeepingTest, DeadlineRunsOnce)
{
    struct Ert_EventClockTime since = ERT_EVENTCLOCKTIME_INIT;

    EXPECT_FALSE(ert_deadlineTimeExpired(&since, ZeroDuration, 0, 0));
}

TEST(TimeKeepingTest, DeadlineExpires)
{
    auto period = Ert_Duration(ERT_NSECS(Ert_MilliSeconds(1000)));

    struct Ert_EventClockTime since     = ERT_EVENTCLOCKTIME_INIT;
    struct Ert_Duration       remaining = ZeroDuration;

    auto startTimeOuter = ert_monotonicTime();
    EXPECT_FALSE(ert_deadlineTimeExpired(&since, period, &remaining, 0));
    EXPECT_EQ(period.duration.ns, remaining.duration.ns);
    auto startTimeInner = ert_monotonicTime();

    bool firstiteration = true;
    while ( ! ert_deadlineTimeExpired(&since, period, &remaining, 0))
    {
        EXPECT_TRUE(firstiteration || remaining.duration.ns);
        firstiteration = false;
    }
    EXPECT_TRUE( ! remaining.duration.ns);

    auto stopTime = ert_monotonicTime();

    auto elapsedInner = ERT_MSECS(
        Ert_NanoSeconds(stopTime.monotonic.ns -
                    startTimeInner.monotonic.ns)).ms / 100;
    auto elapsedOuter = ERT_MSECS(
        Ert_NanoSeconds(stopTime.monotonic.ns -
                    startTimeOuter.monotonic.ns)).ms / 100;

    auto interval = ERT_MSECS(period.duration).ms / 100;

    EXPECT_LE(elapsedInner, interval);
    EXPECT_GE(elapsedOuter, interval);
}

TEST(TimeKeepingTest, MonotonicDeadlineRunsOnce)
{
    struct Ert_MonotonicDeadline deadline = ERT_MONOTONICDEADLINE_INIT;

    EXPECT_FALSE(
        ert_monotonicDeadlineTimeExpired(&deadline, ZeroDuration, 0, 0));
}

TEST(TimeKeepingTest, MonotonicDeadlineExpires)
{
    auto period = Ert_Duration(ERT_NSECS(Ert_MilliSeconds(1000)));

    struct Ert_MonotonicDeadline deadline  = ERT_MONOTONICDEADLINE_INIT;
    struct Ert_Duration          remaining = ZeroDuration;

    auto startTimeOuter = ert_monotonicTime();
    EXPECT_FALSE(
        ert_monotonicDeadlineTimeExpired(&deadline, period, &remaining, 0));
    EXPECT_EQ(period.duration.ns, remaining.duration.ns);
    auto startTimeInner = ert_monotonicTime();

    bool firstiteration = true;
    while ( ! ert_monotonicDeadlineTimeExpired(
                &deadline, period, &remaining, 0))
    {
        EXPECT_TRUE(firstiteration || remaining.duration.ns);
        firstiteration = false;
    }
    EXPECT_TRUE( ! remaining.duration.ns);

    auto stopTime = ert_monotonicTime();

    auto elapsedInner = ERT_MSECS(
        Ert_NanoSeconds(stopTime.monotonic.ns -
                    startTimeInner.monotonic.ns)).ms / 100;
    auto elapsedOuter = ERT_MSECS(
        Ert_NanoSeconds(stopTime.monotonic.ns -
                    startTimeOuter.monotonic.ns)).ms / 100;

    auto interval = ERT_MSECS(period.duration).ms / 100;

    EXPECT_LE(elapsedInner, interval);
    EXPECT_GE(elapsedOuter, interval);
}

TEST(TimeKeepingTest, MonotonicSleep)
{
    auto period = Ert_Duration(ERT_NSECS(Ert_MilliSeconds(1000)));

    auto startTime = ert_monotonicTime();
    ert_monotonicSleep(period);
    auto stopTime = ert_monotonicTime();

    auto elapsedTime = ERT_MSECS(
        Ert_NanoSeconds(stopTime.monotonic.ns - startTime.monotonic.ns)).ms / 100;

    auto interval = ERT_MSECS(period.duration).ms / 100;

    EXPECT_EQ(interval, elapsedTime);
}

TEST(TimeKeepingTest, LapTimeSinceNoPeriod)
{
    auto period = Ert_Duration(ERT_NSECS(Ert_MilliSeconds(1000)));

    struct Ert_EventClockTime since = ERT_EVENTCLOCKTIME_INIT;

    EXPECT_FALSE(ert_lapTimeSince(&since, ZeroDuration, 0).duration.ns);

    {
        ert_monotonicSleep(period);

        auto lapTime = ERT_MSECS(
            ert_lapTimeSince(&since, ZeroDuration, 0).duration).ms / 100;

        auto interval = ERT_MSECS(period.duration).ms / 100;

        EXPECT_EQ(1 * interval, lapTime);

        lapTime = ERT_MSECS(
            ert_lapTimeSince(&since, ZeroDuration, 0).duration).ms / 100;

        EXPECT_EQ(1 * interval, lapTime);
    }

    {
        ert_monotonicSleep(period);

        auto lapTime = ERT_MSECS(
            ert_lapTimeSince(&since, ZeroDuration, 0).duration).ms / 100;

        auto interval = ERT_MSECS(period.duration).ms / 100;

        EXPECT_EQ(2 * interval, lapTime);

        lapTime = ERT_MSECS(
            ert_lapTimeSince(&since, ZeroDuration, 0).duration).ms / 100;

        EXPECT_EQ(2 * interval, lapTime);
    }
}

TEST(TimeKeepingTest, LapTimeSinceWithPeriod)
{
    auto sleepPeriod = Ert_Duration(ERT_NSECS(Ert_MilliSeconds(1000)));
    auto period      = Ert_Duration(ERT_NSECS(Ert_MilliSeconds(5000)));

    struct Ert_EventClockTime since = ERT_EVENTCLOCKTIME_INIT;

    EXPECT_FALSE(ert_lapTimeSince(&since, ZeroDuration, 0).duration.ns);

    {
        ert_monotonicSleep(sleepPeriod);

        uint64_t lapTime = ERT_MSECS(
            ert_lapTimeSince(&since, period, 0).duration).ms / 100;

        auto interval = ERT_MSECS(sleepPeriod.duration).ms / 100;

        EXPECT_EQ(1 * interval, lapTime);

        lapTime = ERT_MSECS(
            ert_lapTimeSince(&since, period, 0).duration).ms / 100;

        EXPECT_EQ(1 * interval, lapTime);
    }

    {
        ert_monotonicSleep(sleepPeriod);

        uint64_t lapTime = ERT_MSECS(
            ert_lapTimeSince(&since, period, 0).duration).ms / 100;

        auto interval = ERT_MSECS(sleepPeriod.duration).ms / 100;

        EXPECT_EQ(2 * interval, lapTime);

        lapTime = ERT_MSECS(
            ert_lapTimeSince(&since, period, 0).duration).ms / 100;

        EXPECT_EQ(2 * interval, lapTime);
    }
}

TEST(TimeKeepingTest, EarliestTime)
{
    struct timespec small;
    small.tv_sec  = 1;
    small.tv_nsec = 1000;

    struct timespec medium;
    medium.tv_sec  = 1;
    medium.tv_nsec = 1001;

    struct timespec large;
    large.tv_sec  = 2;
    large.tv_nsec = 1000;

    EXPECT_EQ(small,  ert_earliestTime(&small,  &small));
    EXPECT_EQ(large,  ert_earliestTime(&large,  &large));
    EXPECT_EQ(medium, ert_earliestTime(&medium, &medium));

    EXPECT_EQ(small, ert_earliestTime(&small,  &medium));
    EXPECT_EQ(small, ert_earliestTime(&medium, &small));

    EXPECT_EQ(small, ert_earliestTime(&small, &large));
    EXPECT_EQ(small, ert_earliestTime(&large, &small));

    EXPECT_EQ(medium, ert_earliestTime(&large,  &medium));
    EXPECT_EQ(medium, ert_earliestTime(&medium, &large));
}

TEST(TimeKeepingTest, TimeVal)
{
    struct timeval timeVal;
    timeVal.tv_sec  = 1;
    timeVal.tv_usec = 2;

    uint64_t nsTime = (1000 * 1000 + 2) * 1000;

    EXPECT_EQ(Ert_NanoSeconds(nsTime).ns,
              ert_timeValToNanoSeconds(&timeVal).ns);

    EXPECT_EQ(timeVal,
              ert_timeValFromNanoSeconds(Ert_NanoSeconds(nsTime +    1)));
    EXPECT_EQ(timeVal,
              ert_timeValFromNanoSeconds(Ert_NanoSeconds(nsTime + 1000 - 1)));
}

TEST(TimeKeepingTest, ShortenTimeInterval)
{
    struct timeval zero;
    zero.tv_sec  = 0;
    zero.tv_usec = 0;

    struct timeval one;
    one.tv_sec  = 1;
    one.tv_usec = 0;

    struct timeval two;
    two.tv_sec  = 2;
    two.tv_usec = 0;

    struct timeval three;
    three.tv_sec  = 3;
    three.tv_usec = 0;

    uint64_t nsTime_1_0 = 1000 * 1000 * 1000;

    struct itimerval alarmVal, shortenedVal;

    // Timer is disabled.
    alarmVal.it_value    = zero;
    alarmVal.it_interval = one;

    shortenedVal.it_value    = zero;
    shortenedVal.it_interval = one;

    EXPECT_EQ(shortenedVal,
              ert_shortenIntervalTime(
                  &alarmVal,
                  Ert_Duration(Ert_NanoSeconds(nsTime_1_0 * 1))));

    // Elapsed time is less that the outstanding alarm time.
    alarmVal.it_value    = two;
    alarmVal.it_interval = three;

    shortenedVal.it_value    = one;
    shortenedVal.it_interval = three;

    EXPECT_EQ(shortenedVal,
              ert_shortenIntervalTime(
                  &alarmVal,
                  Ert_Duration(Ert_NanoSeconds(nsTime_1_0 * 1))));

    // Elapsed time equals the outstanding alarm time.
    alarmVal.it_value    = two;
    alarmVal.it_interval = three;

    shortenedVal.it_value    = three;
    shortenedVal.it_interval = three;

    EXPECT_EQ(shortenedVal,
              ert_shortenIntervalTime(
                  &alarmVal,
                  Ert_Duration(Ert_NanoSeconds(nsTime_1_0 * 2))));

    // Elapsed time exceeds the outstanding alarm time.
    alarmVal.it_value    = two;
    alarmVal.it_interval = three;

    shortenedVal.it_value    = two;
    shortenedVal.it_interval = three;

    EXPECT_EQ(shortenedVal,
              ert_shortenIntervalTime(
                  &alarmVal,
                  Ert_Duration(Ert_NanoSeconds(nsTime_1_0 * 3))));

    // Elapsed time exceeds the outstanding alarm time and the next interval.
    alarmVal.it_value    = two;
    alarmVal.it_interval = three;

    shortenedVal.it_value    = three;
    shortenedVal.it_interval = three;

    EXPECT_EQ(shortenedVal,
              ert_shortenIntervalTime(
                  &alarmVal,
                  Ert_Duration(Ert_NanoSeconds(nsTime_1_0 * 8))));

    alarmVal.it_value    = two;
    alarmVal.it_interval = three;

    shortenedVal.it_value    = two;
    shortenedVal.it_interval = three;

    EXPECT_EQ(shortenedVal,
              ert_shortenIntervalTime(
                  &alarmVal,
                  Ert_Duration(Ert_NanoSeconds(nsTime_1_0 * 9))));

    alarmVal.it_value    = two;
    alarmVal.it_interval = three;

    shortenedVal.it_value    = one;
    shortenedVal.it_interval = three;

    EXPECT_EQ(shortenedVal,
              ert_shortenIntervalTime(
                  &alarmVal,
                  Ert_Duration(Ert_NanoSeconds(nsTime_1_0 * 10))));

    alarmVal.it_value    = two;
    alarmVal.it_interval = three;

    shortenedVal.it_value    = three;
    shortenedVal.it_interval = three;

    EXPECT_EQ(shortenedVal,
              ert_shortenIntervalTime(
                  &alarmVal,
                  Ert_Duration(Ert_NanoSeconds(nsTime_1_0 * 11))));
}

static struct Ert_Duration
uptime()
{
    struct Ert_Pipe  pipe_;
    struct Ert_Pipe *pipe = 0;

    EXPECT_EQ(0, ert_createPipe(&pipe_, 0));
    pipe = &pipe_;

    struct Ert_Pid pid = Ert_Pid(fork());

    EXPECT_NE(-1, pid.mPid);

    if ( ! pid.mPid)
    {
        int rc = 0;

        rc = rc ||
            STDOUT_FILENO == dup2(pipe->mWrFile->mFd, STDOUT_FILENO)
            ? 0
            : -1;

        pipe = ert_closePipe(pipe);

        rc = rc ||
            execlp(
                "sh",
                "sh",
                "-c",
                "set -xe ; read U I < /proc/uptime && echo ${U%%.*}",
                (char *) 0);

        _exit(EXIT_FAILURE);
    }

    ert_closePipeWriter(pipe);

    FILE *fp = fdopen(pipe->mRdFile->mFd, "r");

    EXPECT_TRUE(fp);
    EXPECT_EQ(0, ert_detachPipeReader(pipe));

    unsigned seconds;
    EXPECT_EQ(1, fscanf(fp, "%u", &seconds));

    EXPECT_EQ(0, fclose(fp));
    pipe = ert_closePipe(pipe);

    int status;
    EXPECT_EQ(0, ert_reapProcessChild(pid, &status));

    struct Ert_ExitCode exitcode = ert_extractProcessExitStatus(status, pid);

    EXPECT_EQ(0, exitcode.mStatus);

    return Ert_Duration(ERT_NSECS(Ert_Seconds(seconds)));
}

TEST(TimeKeepingTest, BootClockTime)
{
    struct Ert_Duration before = uptime();

    struct Ert_BootClockTime bootclocktime = ert_bootclockTime();

    ert_monotonicSleep(Ert_Duration(ERT_NSECS(Ert_Seconds(1))));

    struct Ert_Duration after = uptime();

    EXPECT_LE(before.duration.ns, bootclocktime.bootclock.ns);
    EXPECT_GE(after.duration.ns,  bootclocktime.bootclock.ns);
}

#include "../googletest/src/gtest_main.cc"
