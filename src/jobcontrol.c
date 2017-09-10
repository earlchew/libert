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

#include "ert/jobcontrol.h"



/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
ert_reapJobControl_(struct Ert_JobControl *self)
{
    int rc = -1;

    if ( ! ownWatchProcessMethodNil(self->mReap.mMethod))
        ERROR_IF(
            callWatchProcessMethod(self->mReap.mMethod));

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
ert_raiseJobControlSignal_(
    struct Ert_JobControl *self, int aSigNum, struct Ert_Pid ert_aPid, struct Uid aUid)
{
    int rc = -1;

    if ( ! ownWatchProcessSignalMethodNil(self->mRaise.mMethod))
        ERROR_IF(
            callWatchProcessSignalMethod(
                self->mRaise.mMethod, aSigNum, ert_aPid, aUid));

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
ert_raiseJobControlSigStop_(struct Ert_JobControl *self)
{
    int rc = -1;

    if ( ! ownWatchProcessMethodNil(self->mStop.mPauseMethod))
        ERROR_IF(
            callWatchProcessMethod(self->mStop.mPauseMethod));

    ERROR_IF(
        raise(SIGSTOP),
        {
            warn(
                errno,
                "Unable to stop process pid %" PRId_Ert_Pid,
                FMTd_Ert_Pid(ownProcessId()));
        });

    if ( ! ownWatchProcessMethodNil(self->mStop.mResumeMethod))
        ERROR_IF(
            callWatchProcessMethod(self->mStop.mResumeMethod));

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
ert_raiseJobControlSigCont_(struct Ert_JobControl *self)
{
    int rc = -1;

    if ( ! ownWatchProcessMethodNil(self->mContinue.mMethod))
        ERROR_IF(
            callWatchProcessMethod(self->mContinue.mMethod));

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_createJobControl(struct Ert_JobControl *self)
{
    int rc = -1;

    self->mRaise.mMethod      = WatchProcessSignalMethodNil();
    self->mReap.mMethod       = WatchProcessMethodNil();
    self->mStop.mPauseMethod  = WatchProcessMethodNil();
    self->mStop.mResumeMethod = WatchProcessMethodNil();
    self->mContinue.mMethod   = WatchProcessMethodNil();

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
struct Ert_JobControl *
ert_closeJobControl(struct Ert_JobControl *self)
{
    if (self)
    {
        ABORT_IF(unwatchProcessSigCont());
        ABORT_IF(unwatchProcessSigStop());
        ABORT_IF(unwatchProcessSignals());
        ABORT_IF(unwatchProcessChildren());
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
int
ert_watchJobControlSignals(struct Ert_JobControl              *self,
                       struct WatchProcessSignalMethod aRaiseMethod)
{
    int rc = -1;

    ERROR_IF(
        ownWatchProcessSignalMethodNil(aRaiseMethod),
        {
            errno = EINVAL;
        });

    ERROR_UNLESS(
        ownWatchProcessSignalMethodNil(self->mRaise.mMethod),
        {
            errno = EPERM;
        });

    self->mRaise.mMethod = aRaiseMethod;

    ERROR_IF(
        watchProcessSignals(
            WatchProcessSignalMethod(self, ert_raiseJobControlSignal_)),
        {
            self->mRaise.mMethod = WatchProcessSignalMethodNil();
        });

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_unwatchJobControlSignals(struct Ert_JobControl *self)
{
    int rc = -1;

    ERROR_IF(
        ownWatchProcessSignalMethodNil(self->mRaise.mMethod),
        {
            errno = EPERM;
        });

    ERROR_IF(
        unwatchProcessSignals());

    self->mRaise.mMethod = WatchProcessSignalMethodNil();

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_watchJobControlDone(struct Ert_JobControl        *self,
                    struct WatchProcessMethod aReapMethod)
{
    int rc = -1;

    ERROR_IF(
        ownWatchProcessMethodNil(aReapMethod),
        {
            errno = EINVAL;
        });

    ERROR_UNLESS(
        ownWatchProcessMethodNil(self->mReap.mMethod),
        {
            errno = EPERM;
        });

    self->mReap.mMethod = aReapMethod;

    ERROR_IF(
        watchProcessChildren(WatchProcessMethod(self, ert_reapJobControl_)),
        {
            self->mReap.mMethod = WatchProcessMethodNil();
        });

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_unwatchJobControlDone(struct Ert_JobControl *self)
{
    int rc = -1;

    ERROR_IF(
        ownWatchProcessMethodNil(self->mReap.mMethod),
        {
            errno = EPERM;
        });

    ERROR_IF(
        unwatchProcessChildren());

    self->mReap.mMethod = WatchProcessMethodNil();

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_watchJobControlStop(struct Ert_JobControl        *self,
                    struct WatchProcessMethod aPauseMethod,
                    struct WatchProcessMethod aResumeMethod)
{
    int rc = -1;

    ERROR_IF(
        ownWatchProcessMethodNil(aPauseMethod) &&
        ownWatchProcessMethodNil(aResumeMethod),
        {
            errno = EINVAL;
        });

    ERROR_UNLESS(
        ownWatchProcessMethodNil(self->mStop.mPauseMethod) &&
        ownWatchProcessMethodNil(self->mStop.mResumeMethod),
        {
            errno = EPERM;
        });

    self->mStop.mPauseMethod  = aPauseMethod;
    self->mStop.mResumeMethod = aResumeMethod;

    ERROR_IF(
        watchProcessSigStop(
            WatchProcessMethod(self, ert_raiseJobControlSigStop_)),
        {
            self->mStop.mPauseMethod  = WatchProcessMethodNil();
            self->mStop.mResumeMethod = WatchProcessMethodNil();
        });

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_unwatchJobControlStop(struct Ert_JobControl *self)
{
    int rc = -1;

    ERROR_IF(
        ownWatchProcessMethodNil(self->mStop.mPauseMethod) &&
        ownWatchProcessMethodNil(self->mStop.mResumeMethod),
        {
            errno = EPERM;
        });

    ERROR_IF(
        unwatchProcessSigStop());

    self->mStop.mPauseMethod  = WatchProcessMethodNil();
    self->mStop.mResumeMethod = WatchProcessMethodNil();

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_watchJobControlContinue(struct Ert_JobControl        *self,
                        struct WatchProcessMethod aContinueMethod)
{
    int rc = -1;

    ERROR_IF(
        ownWatchProcessMethodNil(aContinueMethod),
        {
            errno = EINVAL;
        });

    ERROR_UNLESS(
        ownWatchProcessMethodNil(self->mContinue.mMethod),
        {
            errno = EPERM;
        });

    self->mContinue.mMethod  = aContinueMethod;

    ERROR_IF(
        watchProcessSigCont(
            WatchProcessMethod(self, ert_raiseJobControlSigCont_)),
        {
            self->mContinue.mMethod = WatchProcessMethodNil();
        });

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_unwatchJobControlContinue(struct Ert_JobControl *self)
{
    int rc = -1;

    ERROR_IF(
        ownWatchProcessMethodNil(self->mContinue.mMethod),
        {
            errno = EPERM;
        });

    ERROR_IF(
        unwatchProcessSigCont());

    self->mContinue.mMethod  = WatchProcessMethodNil();

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
