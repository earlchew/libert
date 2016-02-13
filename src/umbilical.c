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

#include "umbilical.h"
#include "macros_.h"
#include "type_.h"
#include "error_.h"
#include "fd_.h"

#include <unistd.h>
#include <errno.h>

/* -------------------------------------------------------------------------- */
/* Umbilical Process
 *
 * The purpose of the umbilical process is to sense if the watchdog itself
 * is performing properly. The umbilical will break if either the watchdog
 * process terminates, or if the umbilical process terminates. Additionally
 * the umbilical process monitors the umbilical for periodic messages
 * sent by the watchdog, and echoes the messages back to the watchdog.
 */

static const struct Type * const sUmbilicalMonitorType =
    TYPE("UmbilicalMonitor");

static const char *sPollFdMonitorNames[POLL_FD_MONITOR_KINDS] =
{
    [POLL_FD_MONITOR_UMBILICAL] = "umbilical",
};

static const char *sPollFdMonitorTimerNames[POLL_FD_MONITOR_TIMER_KINDS] =
{
    [POLL_FD_MONITOR_TIMER_UMBILICAL] = "umbilical",
};

/* -------------------------------------------------------------------------- */
static void
pollFdMonitorUmbilical(void                        *self_,
                       const struct EventClockTime *aPollTime)
{
    struct UmbilicalMonitorPoll *self = self_;

    ensure(sUmbilicalMonitorType == self->mType);

    char buf[1];

    ssize_t rdlen = read(
        self->mPollFds[POLL_FD_MONITOR_UMBILICAL].fd, buf, sizeof(buf));

    if (-1 == rdlen)
    {
        if (EINTR != errno)
            terminate(
                errno,
                "Unable to read umbilical connection");
    }
    else if ( ! rdlen)
    {
        warn(0, "Broken umbilical connection");
        self->mPollFds[POLL_FD_MONITOR_UMBILICAL].events = 0;
    }
    else
    {
        /* Once activity is detected on the umbilical, reset the
         * umbilical timer, but configure the timer so that it is
         * out-of-phase with the expected activity on the umbilical
         * to avoid having to deal with races when there is a tight
         * finish. */

        struct PollFdTimerAction *umbilicalTimer =
            &self->mPollFdTimerActions[POLL_FD_MONITOR_TIMER_UMBILICAL];

        lapTimeTrigger(&umbilicalTimer->mSince,
                       umbilicalTimer->mPeriod, aPollTime);

        lapTimeDelay(
            &umbilicalTimer->mSince,
            Duration(NanoSeconds(umbilicalTimer->mPeriod.duration.ns / 2)));

        self->mCycleCount = 0;
    }
}

static void
pollFdMonitorTimerUmbilical(
    void                        *self_,
    const struct EventClockTime *aPollTime)
{
    struct UmbilicalMonitorPoll *self = self_;

    ensure(sUmbilicalMonitorType == self->mType);

    /* If nothing is available from the umbilical connection after the
     * timeout period expires, then assume that the watchdog itself
     * is stuck. */

    enum ProcessStatus parentstatus = fetchProcessState(self->mParentPid);

    if (ProcessStateStopped == parentstatus)
    {
        debug(
            0,
            "umbilical timeout deferred due to parent status %c",
            parentstatus);
        self->mCycleCount = 0;
    }
    else if (++self->mCycleCount >= self->mCycleLimit)
    {
        warn(0, "Umbilical connection timed out");
        self->mPollFds[POLL_FD_MONITOR_UMBILICAL].events = 0;
    }
}

static bool
pollFdMonitorCompletion(void *self_)
{
    struct UmbilicalMonitorPoll *self = self_;

    ensure(sUmbilicalMonitorType == self->mType);

    return ! self->mPollFds[POLL_FD_MONITOR_UMBILICAL].events;
}

/* -------------------------------------------------------------------------- */
int
createUmbilicalMonitor(
    struct UmbilicalMonitorPoll *self,
    int   aStdinFd,
    pid_t aParentPid)
{
    int rc = -1;

    unsigned cycleLimit = 2;

    *self = (struct UmbilicalMonitorPoll)
    {
        .mType       = sUmbilicalMonitorType,
        .mCycleLimit = cycleLimit,
        .mParentPid  = aParentPid,

        .mPollFds =
        {
            [POLL_FD_MONITOR_UMBILICAL] = { .fd     = aStdinFd,
                                            .events = POLL_INPUTEVENTS },
        },

        .mPollFdActions =
        {
            [POLL_FD_MONITOR_UMBILICAL] = { pollFdMonitorUmbilical },
        },

        .mPollFdTimerActions =
        {
            [POLL_FD_MONITOR_TIMER_UMBILICAL] =
            {
                pollFdMonitorTimerUmbilical,
                Duration(
                    NanoSeconds(
                        NSECS(Seconds(gOptions.mTimeout.mUmbilical_s)).ns
                        / cycleLimit)),
            },
        },
    };

    rc = 0;

    goto Finally;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
synchroniseUmbilicalMonitor(struct UmbilicalMonitorPoll *self)
{
    int rc = -1;

    /* Use a blocking read to wait for the watchdog to signal that the
     * umbilical monitor should proceed. */

    FINALLY_IF(
        -1 == waitFdReadReady(STDIN_FILENO, 0));

    pollFdMonitorUmbilical(self, 0);

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
runUmbilicalMonitor(struct UmbilicalMonitorPoll *self)
{
    int rc = -1;

    struct PollFd pollfd;
    FINALLY_IF(
        createPollFd(
            &pollfd,
            self->mPollFds,
            self->mPollFdActions,
            sPollFdMonitorNames, POLL_FD_MONITOR_KINDS,
            self->mPollFdTimerActions,
            sPollFdMonitorTimerNames, POLL_FD_MONITOR_TIMER_KINDS,
            pollFdMonitorCompletion, self));

    FINALLY_IF(
        runPollFdLoop(&pollfd));

    FINALLY_IF(
        closePollFd(&pollfd));

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}


/* -------------------------------------------------------------------------- */