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

#include "ert/eventpipe.h"
#include "ert/error.h"

#include <unistd.h>

/* -------------------------------------------------------------------------- */
int
ert_createEventPipe(
    struct Ert_EventPipe *self,
    unsigned              aFlags)
{
    int rc = -1;

    self->mPipe      = 0;
    self->mSignalled = false;
    self->mMutex     = ert_createThreadSigMutex(&self->mMutex_);

    LIST_INIT(&self->mLatchList_.mList);
    self->mLatchList = &self->mLatchList_;

    ERT_ERROR_IF(
        ert_createPipe(&self->mPipe_, aFlags));
    self->mPipe = &self->mPipe_;

    rc = 0;

Ert_Finally:

    ERT_FINALLY
    ({
        if (rc)
            self = ert_closeEventPipe(self);
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
struct Ert_EventPipe *
ert_closeEventPipe(
    struct Ert_EventPipe *self)
{
    if (self)
    {
        if (self->mLatchList)
            ert_ensure(LIST_EMPTY(&self->mLatchList->mList));
        self->mLatchList = 0;

        self->mPipe  = ert_closePipe(self->mPipe);
        self->mMutex = ert_destroyThreadSigMutex(self->mMutex);
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
int
ert_setEventPipe(
    struct Ert_EventPipe *self)
{
    int rc = -1;

    struct Ert_ThreadSigMutex *lock = ert_lockThreadSigMutex(self->mMutex);

    int signalled = 0;

    if ( ! self->mSignalled)
    {
        /* Use write() so that the caller can optionally restart the
         * the operation on EINTR. */

        char buf[1] = { 0 };

        ssize_t rv = 0;
        ERT_ERROR_IF(
            (rv = write(self->mPipe->mWrFile->mFd, buf, sizeof(buf)),
             1 != rv),
            {
                errno = -1 == rv ? errno : EIO;
            });

        self->mSignalled = true;
        signalled        = 1;
    }

    rc = signalled;

Ert_Finally:

    ERT_FINALLY
    ({
        lock = ert_unlockThreadSigMutex(lock);
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
static int
resetEventPipe_(
    struct Ert_EventPipe *self)
{
    int rc = -1;

    int signalled = 0;

    if (self->mSignalled)
    {
        /* Use read() so that the caller can optionally restart the
         * the operation on EINTR. */

        char buf[1];

        ssize_t rv = 0;
        ERT_ERROR_IF(
            (rv = read(self->mPipe->mRdFile->mFd, buf, sizeof(buf)),
             1 != rv),
            {
                errno = -1 == rv ? errno : EIO;
            });

        ert_ensure( ! buf[0]);

        self->mSignalled = false;
        signalled        = 1;
    }

    rc = signalled;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

int
ert_resetEventPipe(
    struct Ert_EventPipe *self)
{
    int rc = -1;

    struct Ert_ThreadSigMutex *lock = ert_lockThreadSigMutex(self->mMutex);

    int signalled;
    ERT_ERROR_IF(
        (signalled = resetEventPipe_(self),
         -1 == signalled));

    rc = signalled;

Ert_Finally:

    ERT_FINALLY
     ({
         lock = ert_unlockThreadSigMutex(lock);
     });

    return rc;
}

/* -------------------------------------------------------------------------- */
void
ert_attachEventPipeLatch_(
    struct Ert_EventPipe           *self,
    struct Ert_EventLatchListEntry *aEntry)
{
    struct Ert_ThreadSigMutex *lock = ert_lockThreadSigMutex(self->mMutex);

    LIST_INSERT_HEAD(&self->mLatchList->mList, aEntry, mEntry);

    lock = ert_unlockThreadSigMutex(lock);
}

/* -------------------------------------------------------------------------- */
void
ert_detachEventPipeLatch_(
    struct Ert_EventPipe           *self,
    struct Ert_EventLatchListEntry *aEntry)
{
    struct Ert_ThreadSigMutex *lock = ert_lockThreadSigMutex(self->mMutex);

    LIST_REMOVE(aEntry, mEntry);

    lock = ert_unlockThreadSigMutex(lock);
}

int
ert_pollEventPipe(
    struct Ert_EventPipe            *self,
    const struct Ert_EventClockTime *aPollTime)
{
    int rc = -1;

    struct Ert_ThreadSigMutex *lock = ert_lockThreadSigMutex(self->mMutex);

    int pollCount = 0;

    if (self->mSignalled)
    {
        struct Ert_EventLatchListEntry *iter;

        int called = 0;

        LIST_FOREACH(iter, &self->mLatchList->mList, mEntry)
        {
            called = -1;

            ERT_ERROR_IF(
                (called = ert_pollEventLatchListEntry(iter, aPollTime),
                 -1 == called && ! pollCount));

            if (-1 == called)
                break;

            if (0 < called)
                ++pollCount;
        }

        /* Leave the event pipe set if any one latch could not be polled.
         * In particular, if the latch poll returns EINTR, the event
         * pipe will remain set and cause the pipe the be polled again. */

        if (-1 != called)
        {
            int signalled = -1;

            ERT_ERROR_IF(
                (signalled = resetEventPipe_(self),
                 -1 == signalled));

            ert_ensure(0 < signalled);
        }
    }

    rc = pollCount;

Ert_Finally:

    ERT_FINALLY
    ({
        lock = ert_unlockThreadSigMutex(lock);
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
