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

#include "ert/eventlatch.h"
#include "ert/eventpipe.h"
#include "ert/timekeeping.h"

#include "gtest/gtest.h"

TEST(EventLatchTest, SetReset)
{
    struct Ert_EventLatch  eventLatch_;
    struct Ert_EventLatch *eventLatch = 0;

    EXPECT_EQ(0, ert_createEventLatch(&eventLatch_, "test"));
    eventLatch = &eventLatch_;

    EXPECT_EQ(Ert_EventLatchSettingOff,
              ert_ownEventLatchSetting(eventLatch));

    EXPECT_EQ(Ert_EventLatchSettingOff,
              ert_setEventLatch(eventLatch));
    EXPECT_EQ(Ert_EventLatchSettingOn,
              ert_ownEventLatchSetting(eventLatch));

    EXPECT_EQ(Ert_EventLatchSettingOn,
              ert_setEventLatch(eventLatch));
    EXPECT_EQ(Ert_EventLatchSettingOn,
              ert_ownEventLatchSetting(eventLatch));

    EXPECT_EQ(Ert_EventLatchSettingOn,
              ert_setEventLatch(eventLatch));
    EXPECT_EQ(Ert_EventLatchSettingOn,
              ert_ownEventLatchSetting(eventLatch));

    EXPECT_EQ(Ert_EventLatchSettingOn,
              ert_resetEventLatch(eventLatch));
    EXPECT_EQ(Ert_EventLatchSettingOff,
              ert_ownEventLatchSetting(eventLatch));

    EXPECT_EQ(Ert_EventLatchSettingOff,
              ert_resetEventLatch(eventLatch));
    EXPECT_EQ(Ert_EventLatchSettingOff,
              ert_ownEventLatchSetting(eventLatch));

    EXPECT_EQ(Ert_EventLatchSettingOff,
              ert_resetEventLatch(eventLatch));
    EXPECT_EQ(Ert_EventLatchSettingOff,
              ert_ownEventLatchSetting(eventLatch));
    eventLatch = ert_closeEventLatch(eventLatch);
}

TEST(EventLatchTest, DisableSetReset)
{
    struct Ert_EventLatch  eventLatch_;
    struct Ert_EventLatch *eventLatch = 0;

    EXPECT_EQ(0, ert_createEventLatch(&eventLatch_, "test"));
    eventLatch = &eventLatch_;

    EXPECT_EQ(Ert_EventLatchSettingOff,
              ert_ownEventLatchSetting(eventLatch));

    EXPECT_EQ(Ert_EventLatchSettingOff,
              ert_disableEventLatch(eventLatch));
    EXPECT_EQ(Ert_EventLatchSettingDisabled,
              ert_disableEventLatch(eventLatch));
    EXPECT_EQ(Ert_EventLatchSettingDisabled,
              ert_ownEventLatchSetting(eventLatch));

    EXPECT_EQ(Ert_EventLatchSettingDisabled,
              ert_setEventLatch(eventLatch));
    EXPECT_EQ(Ert_EventLatchSettingDisabled,
              ert_resetEventLatch(eventLatch));

    eventLatch = ert_closeEventLatch(eventLatch);
}

TEST(EventLatchTest, SetDisableSetReset)
{
    struct Ert_EventLatch  eventLatch_;
    struct Ert_EventLatch *eventLatch = 0;

    EXPECT_EQ(0, ert_createEventLatch(&eventLatch_, "test"));
    eventLatch = &eventLatch_;

    EXPECT_EQ(Ert_EventLatchSettingOff,
              ert_ownEventLatchSetting(eventLatch));

    EXPECT_EQ(Ert_EventLatchSettingOff,
              ert_setEventLatch(eventLatch));
    EXPECT_EQ(Ert_EventLatchSettingOn,
              ert_ownEventLatchSetting(eventLatch));

    EXPECT_EQ(Ert_EventLatchSettingOn,
              ert_disableEventLatch(eventLatch));
    EXPECT_EQ(Ert_EventLatchSettingDisabled,
              ert_disableEventLatch(eventLatch));
    EXPECT_EQ(Ert_EventLatchSettingDisabled,
              ert_ownEventLatchSetting(eventLatch));

    EXPECT_EQ(Ert_EventLatchSettingDisabled,
              ert_setEventLatch(eventLatch));
    EXPECT_EQ(Ert_EventLatchSettingDisabled,
              ert_resetEventLatch(eventLatch));

    eventLatch = ert_closeEventLatch(eventLatch);
}

TEST(EventLatchTest, PipeBindUnbind)
{
    struct Ert_EventLatch  eventLatch_;
    struct Ert_EventLatch *eventLatch = 0;

    struct EventPipe  eventPipe_;
    struct EventPipe *eventPipe = 0;

    EXPECT_EQ(0, ert_createEventLatch(&eventLatch_, "test"));
    eventLatch = &eventLatch_;

    EXPECT_EQ(0, createEventPipe(&eventPipe_, 0));
    eventPipe = &eventPipe_;

    EXPECT_EQ(Ert_EventLatchSettingOff,
              ert_bindEventLatchPipe(eventLatch, eventPipe,
                                 Ert_EventLatchMethodNil()));
    EXPECT_EQ(0, resetEventPipe(eventPipe));

    EXPECT_EQ(Ert_EventLatchSettingOff,
              ert_unbindEventLatchPipe(eventLatch));
    EXPECT_EQ(0, resetEventPipe(eventPipe));
    EXPECT_EQ(0, resetEventPipe(eventPipe));

    EXPECT_EQ(Ert_EventLatchSettingOff,
              ert_setEventLatch(eventLatch));
    EXPECT_EQ(Ert_EventLatchSettingOn,
              ert_bindEventLatchPipe(eventLatch, eventPipe,
                                 Ert_EventLatchMethodNil()));
    EXPECT_EQ(1, resetEventPipe(eventPipe));
    EXPECT_EQ(0, resetEventPipe(eventPipe));
    EXPECT_EQ(Ert_EventLatchSettingOn,
              ert_unbindEventLatchPipe(eventLatch));
    EXPECT_EQ(0, resetEventPipe(eventPipe));

    EXPECT_EQ(Ert_EventLatchSettingOn,
              ert_disableEventLatch(eventLatch));
    EXPECT_EQ(Ert_EventLatchSettingDisabled,
              ert_bindEventLatchPipe(eventLatch, eventPipe,
                                 Ert_EventLatchMethodNil()));
    EXPECT_EQ(1, resetEventPipe(eventPipe));
    EXPECT_EQ(0, resetEventPipe(eventPipe));
    EXPECT_EQ(Ert_EventLatchSettingDisabled,
              ert_unbindEventLatchPipe(eventLatch));
    EXPECT_EQ(0, resetEventPipe(eventPipe));

    eventPipe = closeEventPipe(eventPipe);
    eventLatch = ert_closeEventLatch(eventLatch);
}

TEST(EventLatchTest, PipeBinding)
{
    struct Ert_EventLatch  eventLatch_;
    struct Ert_EventLatch *eventLatch = 0;

    struct EventPipe  eventPipe_;
    struct EventPipe *eventPipe = 0;

    EXPECT_EQ(0, createEventPipe(&eventPipe_, 0));
    eventPipe = &eventPipe_;

    EXPECT_EQ(0, ert_createEventLatch(&eventLatch_, "test"));
    eventLatch = &eventLatch_;

    EXPECT_EQ(Ert_EventLatchSettingOff,
              ert_bindEventLatchPipe(eventLatch, eventPipe,
                                 Ert_EventLatchMethodNil()));

    EXPECT_EQ(Ert_EventLatchSettingOff,
              ert_ownEventLatchSetting(eventLatch));

    EXPECT_EQ(0, resetEventPipe(eventPipe));
    EXPECT_EQ(Ert_EventLatchSettingOff,
              ert_setEventLatch(eventLatch));
    EXPECT_EQ(Ert_EventLatchSettingOn,
              ert_ownEventLatchSetting(eventLatch));
    EXPECT_EQ(1, resetEventPipe(eventPipe));
    EXPECT_EQ(0, resetEventPipe(eventPipe));
    EXPECT_EQ(Ert_EventLatchSettingOn,
              ert_resetEventLatch(eventLatch));
    EXPECT_EQ(Ert_EventLatchSettingOff,
              ert_resetEventLatch(eventLatch));
    EXPECT_EQ(0, resetEventPipe(eventPipe));

    EXPECT_EQ(0, resetEventPipe(eventPipe));
    EXPECT_EQ(Ert_EventLatchSettingOff,
              ert_setEventLatch(eventLatch));
    EXPECT_EQ(Ert_EventLatchSettingOn,
              ert_ownEventLatchSetting(eventLatch));
    EXPECT_EQ(Ert_EventLatchSettingOn,
              ert_resetEventLatch(eventLatch));
    EXPECT_EQ(Ert_EventLatchSettingOff,
              ert_resetEventLatch(eventLatch));
    EXPECT_EQ(1, resetEventPipe(eventPipe));
    EXPECT_EQ(0, resetEventPipe(eventPipe));
    EXPECT_EQ(Ert_EventLatchSettingOff,
              ert_resetEventLatch(eventLatch));
    EXPECT_EQ(0, resetEventPipe(eventPipe));

    eventLatch = ert_closeEventLatch(eventLatch);
    eventPipe = closeEventPipe(eventPipe);
}

struct PipePolling
{
    static int Count;

    static int countPoll(struct PipePolling          *self,
                         bool                         aEnabled,
                         const struct EventClockTime *aPollTime_)
    {
        ++Count;
        return 0;
    }
};

int PipePolling::Count;

TEST(EventLatchTest, PipePolling)
{
    struct Ert_EventLatch  eventLatch_;
    struct Ert_EventLatch *eventLatch = 0;

    struct EventPipe  eventPipe_;
    struct EventPipe *eventPipe = 0;

    struct PipePolling pipePolling;

    EXPECT_EQ(0, createEventPipe(&eventPipe_, 0));
    eventPipe = &eventPipe_;

    EXPECT_EQ(0, ert_createEventLatch(&eventLatch_, "test"));
    eventLatch = &eventLatch_;

    EXPECT_EQ(Ert_EventLatchSettingOff,
              ert_bindEventLatchPipe(eventLatch, eventPipe,
                                 Ert_EventLatchMethod(
                                     &pipePolling,
                                     PipePolling::countPoll)));

    struct EventClockTime  pollTime_ = eventclockTime();
    struct EventClockTime *pollTime = &pollTime_;

    EXPECT_EQ(0, pollEventPipe(eventPipe, pollTime));
    EXPECT_EQ(0, PipePolling::Count);

    EXPECT_EQ(Ert_EventLatchSettingOff,
              ert_setEventLatch(eventLatch));
    EXPECT_EQ(1, pollEventPipe(eventPipe, pollTime));
    EXPECT_EQ(1, PipePolling::Count);

    EXPECT_EQ(0, pollEventPipe(eventPipe, pollTime));
    EXPECT_EQ(1, PipePolling::Count);

    /* Set the latch, then reset before polling the pipe */

    EXPECT_EQ(Ert_EventLatchSettingOff,
              ert_setEventLatch(eventLatch));
    EXPECT_EQ(Ert_EventLatchSettingOn,
              ert_resetEventLatch(eventLatch));
    EXPECT_EQ(0, pollEventPipe(eventPipe, pollTime));
    EXPECT_EQ(1, PipePolling::Count);

    /* Ensure the latch is not set, then disable before polling the pipe */

    EXPECT_EQ(0, pollEventPipe(eventPipe, pollTime));
    EXPECT_EQ(1, PipePolling::Count);
    EXPECT_EQ(Ert_EventLatchSettingOff,
              ert_disableEventLatch(eventLatch));
    EXPECT_EQ(1, pollEventPipe(eventPipe, pollTime));
    EXPECT_EQ(2, PipePolling::Count);

    /* With the latch still disabled, try another poll */

    EXPECT_EQ(0, pollEventPipe(eventPipe, pollTime));
    EXPECT_EQ(2, PipePolling::Count);

    eventLatch = ert_closeEventLatch(eventLatch);
    eventPipe = closeEventPipe(eventPipe);
}

#include "../googletest/src/gtest_main.cc"
