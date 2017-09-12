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
#ifndef ERT_TIMEKEEPING_H
#define ERT_TIMEKEEPING_H

#include "ert/compiler.h"
#include "ert/timescale.h"

#include <stdbool.h>
#include <signal.h>

#include <sys/time.h>

ERT_BEGIN_C_SCOPE;

struct Ert_TimeKeepingModule
{
    struct Ert_TimeKeepingModule *mModule;
};

struct Ert_MonotonicTime
{
    struct NanoSeconds monotonic;
};

struct Ert_WallClockTime
{
    struct NanoSeconds wallclock;
};

struct Ert_EventClockTime
{
    struct NanoSeconds eventclock;
};

struct Ert_BootClockTime
{
    struct NanoSeconds bootclock;
};

struct Ert_PushedIntervalTimer
{
    int                  mType;
    int                  mSignal;
    struct Ert_MonotonicTime mMark;
    struct sigaction     mAction;
    struct itimerval     mTimer;
};

struct Ert_MonotonicDeadline
{
    struct Ert_MonotonicTime *mSince;
    struct Ert_MonotonicTime  mSince_;
};

/* -------------------------------------------------------------------------- */
#define ERT_EVENTCLOCKTIME_INIT ((struct Ert_EventClockTime) { { { ns : 0 } } })

#define ERT_MONOTONICDEADLINE_INIT \
    ((struct Ert_MonotonicDeadline) { mSince : 0, { { { ns : 0 } } } })

/* -------------------------------------------------------------------------- */
void
ert_monotonicSleep(struct Duration aPeriod);

/* -------------------------------------------------------------------------- */
struct Ert_EventClockTime
ert_eventclockTime(void);

struct Ert_MonotonicTime
ert_monotonicTime(void);

struct Ert_WallClockTime
ert_wallclockTime(void);

struct Ert_BootClockTime
ert_bootclockTime(void);

/* -------------------------------------------------------------------------- */
#ifdef __linux__
ERT_CHECKED int
ert_procUptime(struct Duration *aUptime, const char *aFileName);
#endif

/* -------------------------------------------------------------------------- */
struct Duration
ert_lapTimeSince(struct Ert_EventClockTime       *self,
                 struct Duration              aPeriod,
                 const struct Ert_EventClockTime *aTime);

void
ert_lapTimeRestart(struct Ert_EventClockTime       *self,
                   const struct Ert_EventClockTime *aTime);

void
ert_lapTimeTrigger(struct Ert_EventClockTime       *self,
                   struct Duration              aPeriod,
                   const struct Ert_EventClockTime *aTime);

void
ert_lapTimeDelay(struct Ert_EventClockTime *self,
                 struct Duration        aDelay);

/* -------------------------------------------------------------------------- */
bool
ert_deadlineTimeExpired(
    struct Ert_EventClockTime       *self,
    struct Duration              aPeriod,
    struct Duration             *aRemaining,
    const struct Ert_EventClockTime *aTime);

bool
ert_monotonicDeadlineTimeExpired(
    struct Ert_MonotonicDeadline   *self,
    struct Duration             aPeriod,
    struct Duration            *aRemaining,
    const struct Ert_MonotonicTime *aTime);

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
Ert_Timekeeping_init(struct Ert_TimeKeepingModule *self);

ERT_CHECKED struct Ert_TimeKeepingModule *
Ert_Timekeeping_exit(struct Ert_TimeKeepingModule *self);

/* -------------------------------------------------------------------------- */

ERT_END_C_SCOPE;

#endif /* ERT_TIMEKEEPING_H */
