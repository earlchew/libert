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
#include "ert/error.h"
#include "malloc_.h"
#include "ert/timescale.h"

#include <limits.h>
#include <sys/epoll.h>

/* -------------------------------------------------------------------------- */
static uint32_t pollTriggers_[Ert_FileEventQueuePollTriggers] =
{
    [Ert_FileEventQueuePollDisconnect] = (EPOLLHUP | EPOLLERR),
    [Ert_FileEventQueuePollWrite]      = (EPOLLHUP | EPOLLERR | EPOLLOUT),
    [Ert_FileEventQueuePollRead]       = (EPOLLHUP | EPOLLERR | EPOLLPRI
                                                              | EPOLLIN),
};

/* -------------------------------------------------------------------------- */
static void
ert_markFileEventQueueActivityPending_(struct Ert_FileEventQueueActivity *self,
                                   struct epoll_event            *aPollEvent)
{
    ensure(self->mArmed);
    ensure( ! self->mPending);

    self->mPending = aPollEvent;
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
ert_disarmFileEventQueueActivity_(struct Ert_FileEventQueueActivity *self)
{
    struct Ert_FileEventQueueActivityMethod method = self->mMethod;

    ensure(self->mArmed);
    ensure(self->mPending);
    ensure( ! ert_ownFileEventQueueActivityMethodNil(method));

    self->mArmed   = 0;
    self->mPending = 0;
    self->mMethod  = Ert_FileEventQueueActivityMethodNil();

    return ert_callFileEventQueueActivityMethod(method);
}

/* -------------------------------------------------------------------------- */
int
ert_createFileEventQueue(struct Ert_FileEventQueue *self, int aQueueSize)
{
    int rc = -1;

    ensure(0 < aQueueSize);

    self->mFile         = 0;
    self->mQueue        = 0;
    self->mQueueSize    = 0;
    self->mQueuePending = 0;
    self->mNumArmed     = 0;
    self->mNumPending   = 0;

    ERROR_UNLESS(
        (self->mQueue = malloc(sizeof(*self->mQueue) * aQueueSize)));

    self->mQueueSize = aQueueSize;

    ERROR_IF(
        ert_createFile(
            &self->mFile_,
            epoll_create1(EPOLL_CLOEXEC)));
    self->mFile = &self->mFile_;

    rc = 0;

Finally:

    FINALLY
    ({
        if (rc)
        {
            while (ert_closeFileEventQueue(self))
                break;
        }
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
ert_controlFileEventQueueActivity_(struct Ert_FileEventQueue         *self,
                               struct Ert_FileEventQueueActivity *aEvent,
                               uint32_t                       aEvents,
                               int                            aCtlOp)
{
    int rc = -1;

    struct epoll_event pollEvent =
    {
        .events = aEvents,
        .data   = { .ptr = aEvent },
    };

    ERROR_IF(
        epoll_ctl(self->mFile->mFd, aCtlOp, aEvent->mFile->mFd, &pollEvent));

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
ert_attachFileEventQueueActivity_(struct Ert_FileEventQueue         *self,
                              struct Ert_FileEventQueueActivity *aEvent)
{
    return ert_controlFileEventQueueActivity_(self, aEvent, 0, EPOLL_CTL_ADD);
}

static ERT_CHECKED int
ert_detachFileEventQueueActivity_(struct Ert_FileEventQueue         *self,
                              struct Ert_FileEventQueueActivity *aEvent)
{
    return ert_controlFileEventQueueActivity_(self, aEvent, 0, EPOLL_CTL_DEL);
}

/* -------------------------------------------------------------------------- */
struct Ert_FileEventQueue *
ert_closeFileEventQueue(struct Ert_FileEventQueue *self)
{
    if (self)
    {
        ensure( ! self->mNumArmed);
        ensure( ! self->mNumPending);

        self->mFile = ert_closeFile(self->mFile);

        free(self->mQueue);
        self->mQueue        = 0;
        self->mQueueSize    = 0;
        self->mQueuePending = 0;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
ert_lodgeFileEventQueueActivity_(struct Ert_FileEventQueue         *self,
                             struct Ert_FileEventQueueActivity *aEvent)
{
    int rc = -1;

    ensure(aEvent->mArmed);

    ERROR_IF(
        ert_controlFileEventQueueActivity_(
            self, aEvent, aEvent->mArmed | EPOLLONESHOT, EPOLL_CTL_MOD));

    ++self->mNumArmed;

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
static void
ert_purgeFileEventQueueActivity_(struct Ert_FileEventQueue         *self,
                             struct Ert_FileEventQueueActivity *aEvent,
                             struct epoll_event            *aPollEvent)
{
    ensure(self->mNumArmed);

    ABORT_IF(
        ert_controlFileEventQueueActivity_(self, aEvent, 0, EPOLL_CTL_MOD));

    --self->mNumArmed;

    if (aPollEvent)
    {
        ensure(&self->mQueue[0]               <= aPollEvent &&
               &self->mQueue[self->mQueueSize] > aPollEvent);

        *aPollEvent = (struct epoll_event) { };

        --self->mNumPending;
    }
}

/* -------------------------------------------------------------------------- */
int
ert_pollFileEventQueueActivity(struct Ert_FileEventQueue *self,
                           const struct Duration *aTimeout)
{
    int rc = -1;

    if ( ! self->mQueuePending)
    {
        int timeout_ms = -1;

        if (aTimeout)
        {
            struct MilliSeconds timeoutDuration = MSECS(aTimeout->duration);

            timeout_ms = timeoutDuration.ms;

            if (0 > timeout_ms || timeoutDuration.ms != timeout_ms)
                timeout_ms = INT_MAX;
        }

        int polledEvents;
        ERROR_IF(
            (polledEvents = epoll_wait(
               self->mFile->mFd, self->mQueue, self->mQueueSize, timeout_ms),
             -1 == polledEvents && EINTR != errno));

        if (0 > polledEvents)
            polledEvents = 0;

        for (int ix = 0; ix < polledEvents; ++ix)
        {
            struct Ert_FileEventQueueActivity *event = self->mQueue[ix].data.ptr;

            ert_markFileEventQueueActivityPending_(event, &self->mQueue[ix]);
        }

        self->mQueuePending = polledEvents;

        ensure(self->mNumArmed >= polledEvents);

        self->mNumArmed   -= polledEvents;
        self->mNumPending += polledEvents;
    }

    while (self->mQueuePending)
    {
        int eventIndex = --self->mQueuePending;

        struct Ert_FileEventQueueActivity *event =
            self->mQueue[eventIndex].data.ptr;

        if (event)
        {
            --self->mNumPending;

            ERROR_IF(
                ert_disarmFileEventQueueActivity_(event));
        }
    }

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_createFileEventQueueActivity(struct Ert_FileEventQueueActivity *self,
                                 struct Ert_FileEventQueue         *aQueue,
                                 struct Ert_File                   *aFile)
{
    int rc = -1;

    self->mQueue   = aQueue;
    self->mFile    = aFile;
    self->mArmed   = 0;
    self->mPending = 0;
    self->mMethod  = Ert_FileEventQueueActivityMethodNil();

    ERROR_IF(
        ert_attachFileEventQueueActivity_(self->mQueue, self));

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_armFileEventQueueActivity(struct Ert_FileEventQueueActivity      *self,
                              enum Ert_FileEventQueuePollTrigger      aTrigger,
                              struct Ert_FileEventQueueActivityMethod aMethod)
{
    int rc = -1;

    ensure( ! self->mArmed);
    ensure( ! self->mPending);
    ensure(ert_ownFileEventQueueActivityMethodNil(self->mMethod));

    self->mArmed  = pollTriggers_[aTrigger];
    self->mMethod = aMethod;

    ERROR_IF(
        ert_lodgeFileEventQueueActivity_(self->mQueue, self));

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
struct Ert_FileEventQueueActivity *
ert_closeFileEventQueueActivity(struct Ert_FileEventQueueActivity *self)
{
    if (self)
    {
        if (self->mArmed)
            ert_purgeFileEventQueueActivity_(
                self->mQueue, self, self->mPending);

        self->mArmed   = 0;
        self->mPending = 0;
        self->mMethod  = Ert_FileEventQueueActivityMethodNil();

        ABORT_IF(
            ert_detachFileEventQueueActivity_(self->mQueue, self));
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
