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
#include "ert/fd.h"
#include "ert/error.h"

#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

#ifdef __linux__
#ifndef CLOCK_BOOTTIME
#define CLOCK_BOOTTIME 7 /* Since 2.6.39 */
#endif
#endif

/* -------------------------------------------------------------------------- */
static unsigned             moduleInit_;
static struct Ert_MonotonicTime eventClockTimeBase_;

/* -------------------------------------------------------------------------- */
struct Ert_MonotonicTime
ert_monotonicTime(void)
{
    struct timespec ts;

    ABORT_IF(
        clock_gettime(CLOCK_MONOTONIC, &ts),
        {
            terminate(
                errno,
                "Unable to fetch monotonic time");
        });

    return (struct Ert_MonotonicTime)
    { .monotonic = ert_timeSpecToNanoSeconds(&ts) };
}

/* -------------------------------------------------------------------------- */
#if __linux__
int
ert_procUptime(struct Ert_Duration *aUptime, const char *aFileName)
{
    int   rc  = -1;
    char *buf = 0;
    int   fd  = -1;

    ERROR_IF(
        (fd = ert_openFd(aFileName, O_RDONLY, 0),
         -1 == fd));

    ssize_t buflen;

    ERROR_IF(
        (buflen = ert_readFdFully(fd, &buf, 64),
         -1 == buflen));

    ERROR_UNLESS(
        buflen,
        {
            errno = ERANGE;
        });

    char *end;
    ERROR_UNLESS(
        (end = strchr(buf, ' ')),
        {
            errno = ERANGE;
        });

    uint64_t uptime_ns  = 0;
    unsigned fracdigits = 0;

    for (char *ptr = buf; ptr != end; ++ptr)
    {
        unsigned digit;

        switch (*ptr)
        {
        default:
            ERROR_IF(
                true,
                {
                    errno = ERANGE;
                });

        case '.':
            ERROR_IF(
                fracdigits,
                {
                    errno = ERANGE;
                });
            fracdigits = 1;
            continue;

        case '0': digit = 0; break;
        case '1': digit = 1; break;
        case '2': digit = 2; break;
        case '3': digit = 3; break;
        case '4': digit = 4; break;
        case '5': digit = 5; break;
        case '6': digit = 6; break;
        case '7': digit = 7; break;
        case '8': digit = 8; break;
        case '9': digit = 9; break;
        }

        uint64_t value = uptime_ns * 10;
        ERROR_IF(
            value / 10 != uptime_ns || value + digit < uptime_ns,
            {
                errno = ERANGE;
            });

        uptime_ns = value + digit;

        if (fracdigits)
            fracdigits += 2;
    }

    switch (fracdigits / 2)
    {
    default:
        ERROR_IF(
            true,
            {
                errno = ERANGE;
            });

    case 0: uptime_ns *= 1000000000; break;
    case 1: uptime_ns *=  100000000; break;
    case 2: uptime_ns *=   10000000; break;
    case 3: uptime_ns *=    1000000; break;
    case 4: uptime_ns *=     100000; break;
    case 5: uptime_ns *=      10000; break;
    case 6: uptime_ns *=       1000; break;
    case 7: uptime_ns *=        100; break;
    case 8: uptime_ns *=         10; break;
    case 9: uptime_ns *=          1; break;
    }

    *aUptime = Ert_Duration(Ert_NanoSeconds(uptime_ns));

    rc = 0;

Finally:

    FINALLY
    ({
        fd = ert_closeFd(fd);

        free(buf);
    });

    return rc;
}
#endif

/* -------------------------------------------------------------------------- */
struct Ert_BootClockTime
ert_bootclockTime(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_BOOTTIME, &ts))
    {
        do
        {
#ifdef __linux__
            if (EINVAL == errno)
            {
                static const char procUptimeFileName[] = "/proc/uptime";

                struct Ert_Duration uptime;

                ABORT_IF(
                    ert_procUptime(&uptime, procUptimeFileName),
                    {
                        terminate(
                            errno,
                            "Unable to read %s", procUptimeFileName);
                    });

                ts.tv_nsec = uptime.duration.ns % Ert_TimeScale_ns;
                ts.tv_sec  = uptime.duration.ns / Ert_TimeScale_ns;
                break;
            }
#endif

            ABORT_IF(
                true,
                {
                    terminate(
                        errno,
                        "Unable to fetch boot time");
                });

        } while (0);
    }

    return (struct Ert_BootClockTime)
    { .bootclock = ert_timeSpecToNanoSeconds(&ts) };
}

/* -------------------------------------------------------------------------- */
static void
eventclockTime_init_(void)
{
    /* Initialise the time base for the event clock, and ensure that
     * the event clock will subsequently always return a non-zero result. */

    eventClockTimeBase_ = (struct Ert_MonotonicTime) {
        .monotonic = Ert_NanoSeconds(ert_monotonicTime().monotonic.ns - 1) };
}

struct Ert_EventClockTime
ert_eventclockTime(void)
{
    struct Ert_EventClockTime tm = {
        .eventclock = Ert_NanoSeconds(
            ert_monotonicTime().monotonic.ns
                - eventClockTimeBase_.monotonic.ns) };

    ensure(tm.eventclock.ns);

    return tm;
}

/* -------------------------------------------------------------------------- */
struct Ert_WallClockTime
ert_wallclockTime(void)
{
    struct timespec ts;

    ABORT_IF(
        clock_gettime(CLOCK_REALTIME, &ts),
        {
            terminate(
                errno,
                "Unable to fetch monotonic time");
        });

    return (struct Ert_WallClockTime)
    { .wallclock = ert_timeSpecToNanoSeconds(&ts) };
}

/* -------------------------------------------------------------------------- */
bool
ert_deadlineTimeExpired(
    struct Ert_EventClockTime       *self,
    struct Ert_Duration              aPeriod,
    struct Ert_Duration             *aRemaining,
    const struct Ert_EventClockTime *aTime)
{
    bool                  expired;
    uint64_t              remaining_ns;
    struct Ert_EventClockTime tm;

    if ( ! aTime)
    {
        tm   = ert_eventclockTime();
        aTime = &tm;
    }

    if (self->eventclock.ns)
    {
        uint64_t elapsed_ns = aTime->eventclock.ns - self->eventclock.ns;

        if (elapsed_ns >= aPeriod.duration.ns)
        {
            remaining_ns = 0;
            expired      = true;
        }
        else
        {
            remaining_ns = aPeriod.duration.ns - elapsed_ns;
            expired      = false;
        }
    }
    else
    {
        /* Initialise the mark time from which the duration will be
         * measured until the deadline, and then ensure that the
         * caller gets to execute at least once before the deadline
         * expires. */

        *self = *aTime;

        ensure(self->eventclock.ns);

        remaining_ns = aPeriod.duration.ns;
        expired      = false;
    }

    if (aRemaining)
        aRemaining->duration.ns = remaining_ns;

    return expired;
}

/* -------------------------------------------------------------------------- */
bool
ert_monotonicDeadlineTimeExpired(
    struct Ert_MonotonicDeadline   *self,
    struct Ert_Duration             aPeriod,
    struct Ert_Duration            *aRemaining,
    const struct Ert_MonotonicTime *aTime)
{
    bool                 expired;
    uint64_t             remaining_ns;
    struct Ert_MonotonicTime tm;

    if ( ! aTime)
    {
        tm    = ert_monotonicTime();
        aTime = &tm;
    }

    if (self->mSince)
    {
        uint64_t elapsed_ns = aTime->monotonic.ns - self->mSince->monotonic.ns;

        if (elapsed_ns >= aPeriod.duration.ns)
        {
            remaining_ns = 0;
            expired      = true;
        }
        else
        {
            remaining_ns = aPeriod.duration.ns - elapsed_ns;
            expired      = false;
        }
    }
    else
    {
        /* Initialise the mark time from which the duration will be
         * measured until the deadline, and then ensure that the
         * caller gets to execute at least once before the deadline
         * expires. */

        self->mSince = &self->mSince_;

        *self->mSince = *aTime;

        remaining_ns = aPeriod.duration.ns;
        expired      = false;
    }

    if (aRemaining)
        aRemaining->duration.ns = remaining_ns;

    return expired;
}

/* -------------------------------------------------------------------------- */
void
ert_lapTimeTrigger(struct Ert_EventClockTime       *self,
                   struct Ert_Duration              aPeriod,
                   const struct Ert_EventClockTime *aTime)
{
    self->eventclock = Ert_NanoSeconds(
        (aTime ? *aTime
               : ert_eventclockTime()).eventclock.ns - aPeriod.duration.ns);
}

/* -------------------------------------------------------------------------- */
void
ert_lapTimeRestart(struct Ert_EventClockTime       *self,
                   const struct Ert_EventClockTime *aTime)
{
    ensure(self->eventclock.ns);

    *self = aTime ? *aTime : ert_eventclockTime();
}

/* -------------------------------------------------------------------------- */
void
ert_lapTimeDelay(struct Ert_EventClockTime *self,
                 struct Ert_Duration        aDelay)
{
    ensure(self->eventclock.ns);

    self->eventclock.ns += aDelay.duration.ns;
}

/* -------------------------------------------------------------------------- */
struct Ert_Duration
ert_lapTimeSince(struct Ert_EventClockTime       *self,
                 struct Ert_Duration              aPeriod,
                 const struct Ert_EventClockTime *aTime)
{
    struct Ert_EventClockTime tm;

    if ( ! aTime)
    {
        tm    = ert_eventclockTime();
        aTime = &tm;
    }

    uint64_t lapTime_ns;

    if (self->eventclock.ns)
    {
        lapTime_ns = aTime->eventclock.ns - self->eventclock.ns;

        if (aPeriod.duration.ns && lapTime_ns >= aPeriod.duration.ns)
            self->eventclock.ns =
                aTime->eventclock.ns - lapTime_ns % aPeriod.duration.ns;
    }
    else
    {
        lapTime_ns = 0;

        *self = *aTime;

        ensure(self->eventclock.ns);
    }

    return Ert_Duration(Ert_NanoSeconds(lapTime_ns));
}

/* -------------------------------------------------------------------------- */
void
ert_monotonicSleep(struct Ert_Duration aPeriod)
{
    int rc;

    struct timespec sleepTime =
        ert_timeSpecFromNanoSeconds(
            Ert_NanoSeconds(
                ert_monotonicTime().monotonic.ns + aPeriod.duration.ns));

    do
        rc = clock_nanosleep(
            CLOCK_MONOTONIC, TIMER_ABSTIME, &sleepTime, 0);
    while (EINTR == rc);

    ensure( ! rc);
}

/* -------------------------------------------------------------------------- */
int
Ert_Timekeeping_init(struct Ert_TimeKeepingModule *self)
{
    int rc = -1;

    self->mModule = self;

    if ( ! moduleInit_)
        eventclockTime_init_();

    ++moduleInit_;

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
struct Ert_TimeKeepingModule *
Ert_Timekeeping_exit(struct Ert_TimeKeepingModule *self)
{
    if (self)
        --moduleInit_;

    return 0;
}

/* -------------------------------------------------------------------------- */
