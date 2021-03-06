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
#include "ert/error.h"

#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
#define EVENTLATCH_DISABLE_BIT_ 0
#define EVENTLATCH_DATA_BIT_    1

#define EVENTLATCH_DISABLE_MASK_ (1u << EVENTLATCH_DISABLE_BIT_)
#define EVENTLATCH_DATA_MASK_    (1u << EVENTLATCH_DATA_BIT_)

/* -------------------------------------------------------------------------- */
int
ert_createEventLatch(
    struct Ert_EventLatch *self,
    const char            *aName)
{
    int rc = -1;

    self->mMutex = ert_createThreadSigMutex(&self->mMutex_);
    self->mEvent = 0;
    self->mPipe  = 0;
    self->mName  = 0;
    self->mList  = (struct Ert_EventLatchListEntry)
    {
        .mMethod = Ert_EventLatchMethodNil(),
        .mLatch = self,
    };

    ERT_ERROR_UNLESS(
        self->mName = strdup(aName));

    rc = 0;

Ert_Finally:

    ERT_FINALLY
    ({
        if (rc)
            self = ert_closeEventLatch(self);
    });

    return 0;
}

/* -------------------------------------------------------------------------- */
struct Ert_EventLatch *
ert_closeEventLatch(
    struct Ert_EventLatch *self)
{
    if (self)
    {
        if (self->mPipe)
            ERT_ABORT_IF(
                Ert_EventLatchSettingError == ert_unbindEventLatchPipe(self));

        self->mMutex = ert_destroyThreadSigMutex(self->mMutex);

        free(self->mName);
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
int
ert_printEventLatch(
    const struct Ert_EventLatch *self,
    FILE                        *aFile)
{
    return fprintf(aFile, "<%p %s>", self, self->mName);
}

/* -------------------------------------------------------------------------- */
static int
signalEventLatch_(struct Ert_EventLatch *self)
{
    int rc = -1;

    if (self->mPipe)
    {
        int signalled;

        do
        {
            signalled = -1;

            ERT_ERROR_IF(
                (signalled = ert_setEventPipe(self->mPipe),
                 -1 == signalled && EINTR != errno));

        } while (-1 == signalled);
    }

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
static enum Ert_EventLatchSetting
bindEventLatchPipe_(
    struct Ert_EventLatch       *self,
    struct Ert_EventPipe        *aPipe,
    struct Ert_EventLatchMethod  aMethod)
{
    enum Ert_EventLatchSetting rc = Ert_EventLatchSettingError;

    struct Ert_ThreadSigMutex *lock = ert_lockThreadSigMutex(self->mMutex);

    enum Ert_EventLatchSetting setting;

    unsigned event = self->mEvent;

    if (event & EVENTLATCH_DISABLE_MASK_)
        setting = Ert_EventLatchSettingDisabled;
    else
        setting = (event & EVENTLATCH_DATA_MASK_)
            ? Ert_EventLatchSettingOn
            : Ert_EventLatchSettingOff;

    if (self->mPipe != aPipe)
    {
        if (self->mPipe)
        {
            ert_detachEventPipeLatch_(self->mPipe, &self->mList);
            self->mList.mMethod = Ert_EventLatchMethodNil();
        }

        self->mPipe = aPipe;

        if (self->mPipe)
        {
            self->mList.mMethod = aMethod;
            ert_attachEventPipeLatch_(self->mPipe, &self->mList);

            if (Ert_EventLatchSettingOff != setting)
                ERT_ERROR_IF(
                    signalEventLatch_(self));
        }
    }

    rc = setting;

Ert_Finally:

    ERT_FINALLY
    ({
        lock = ert_unlockThreadSigMutex(lock);
    });

    return rc;
}

enum Ert_EventLatchSetting
ert_bindEventLatchPipe(
    struct Ert_EventLatch       *self,
    struct Ert_EventPipe        *aPipe,
    struct Ert_EventLatchMethod  aMethod)
{
    ert_ensure(aPipe);
    ert_ensure( ! self->mPipe);

    return bindEventLatchPipe_(self, aPipe, aMethod);
}

enum Ert_EventLatchSetting
ert_unbindEventLatchPipe(
    struct Ert_EventLatch *self)
{
    return bindEventLatchPipe_(self, 0, Ert_EventLatchMethodNil());
}

/* -------------------------------------------------------------------------- */
enum Ert_EventLatchSetting
ert_disableEventLatch(
    struct Ert_EventLatch *self)
{
    enum Ert_EventLatchSetting rc = Ert_EventLatchSettingError;

    struct Ert_ThreadSigMutex *lock = ert_lockThreadSigMutex(self->mMutex);

    enum Ert_EventLatchSetting setting;

    unsigned event = self->mEvent;

    if (event & EVENTLATCH_DISABLE_MASK_)
        setting = Ert_EventLatchSettingDisabled;
    else
    {
        setting = (event & EVENTLATCH_DATA_MASK_)
            ? Ert_EventLatchSettingOn
            : Ert_EventLatchSettingOff;

        ERT_ERROR_IF(
            signalEventLatch_(self));

        self->mEvent = event ^ EVENTLATCH_DISABLE_MASK_;
    }

    rc = setting;

Ert_Finally:

    ERT_FINALLY
    ({
        lock = ert_unlockThreadSigMutex(lock);
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
enum Ert_EventLatchSetting
ert_setEventLatch(
    struct Ert_EventLatch *self)
{
    enum Ert_EventLatchSetting rc = Ert_EventLatchSettingError;

    struct Ert_ThreadSigMutex *lock = ert_lockThreadSigMutex(self->mMutex);

    enum Ert_EventLatchSetting setting;

    unsigned event = self->mEvent;

    if (event & EVENTLATCH_DISABLE_MASK_)
        setting = Ert_EventLatchSettingDisabled;
    else if (event & EVENTLATCH_DATA_MASK_)
        setting = Ert_EventLatchSettingOn;
    else
    {
        setting = Ert_EventLatchSettingOff;

        ERT_ERROR_IF(
            signalEventLatch_(self));

        self->mEvent = event ^ EVENTLATCH_DATA_MASK_;
    }

    rc = setting;

Ert_Finally:

    ERT_FINALLY
    ({
        lock = ert_unlockThreadSigMutex(lock);
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
enum Ert_EventLatchSetting
ert_resetEventLatch(
    struct Ert_EventLatch *self)
{
    enum Ert_EventLatchSetting rc = Ert_EventLatchSettingError;

    struct Ert_ThreadSigMutex *lock = ert_lockThreadSigMutex(self->mMutex);

    enum Ert_EventLatchSetting setting;

    unsigned event = self->mEvent;

    if (event & EVENTLATCH_DISABLE_MASK_)
        setting = Ert_EventLatchSettingDisabled;
    else if ( ! (event & EVENTLATCH_DATA_MASK_))
        setting = Ert_EventLatchSettingOff;
    else
    {
        setting = Ert_EventLatchSettingOn;

        self->mEvent = event ^ EVENTLATCH_DATA_MASK_;
    }

    rc = setting;

Ert_Finally:

    ERT_FINALLY
    ({
        lock = ert_unlockThreadSigMutex(lock);
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
enum Ert_EventLatchSetting
ert_ownEventLatchSetting(
    const struct Ert_EventLatch *self_)
{
    enum Ert_EventLatchSetting rc = Ert_EventLatchSettingError;

    struct Ert_EventLatch *self = (struct Ert_EventLatch *) self_;

    struct Ert_ThreadSigMutex *lock = ert_lockThreadSigMutex(self->mMutex);

    enum Ert_EventLatchSetting setting;

    unsigned event = self->mEvent;

    if (event & EVENTLATCH_DISABLE_MASK_)
        setting = Ert_EventLatchSettingDisabled;
    else
        setting = (event & EVENTLATCH_DATA_MASK_)
            ? Ert_EventLatchSettingOn
            : Ert_EventLatchSettingOff;

    rc = setting;

Ert_Finally:

    ERT_FINALLY
    ({
        lock = ert_unlockThreadSigMutex(lock);
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_pollEventLatchListEntry(
    struct Ert_EventLatchListEntry  *self,
    const struct Ert_EventClockTime *aPollTime)
{
    int rc = -1;

    int called = 0;

    if (self->mLatch)
    {
        enum Ert_EventLatchSetting setting;
        ERT_ERROR_IF(
            (setting = ert_resetEventLatch(self->mLatch),
             Ert_EventLatchSettingError == setting),
            {
                ert_warn(errno,
                     "Unable to reset event latch %" PRIs_Ert_Method,
                     FMTs_Ert_Method(self->mLatch, ert_printEventLatch));
            });

        if (Ert_EventLatchSettingOff != setting)
        {
            bool enabled;

            if (Ert_EventLatchSettingOn == setting)
                enabled = true;
            else
            {
                ert_ensure(Ert_EventLatchSettingDisabled == setting);

                self->mLatch = 0;

                enabled = false;
            }

            called = 1;

            ERT_ERROR_IF(
                ert_callEventLatchMethod(self->mMethod, enabled, aPollTime));
        }
    }

    rc = called;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
