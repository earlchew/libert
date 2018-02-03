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

    if ( ! ert_ownWatchProcessMethodNil(self->mReap.mMethod))
        ERT_ERROR_IF(
            ert_callWatchProcessMethod(self->mReap.mMethod));

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
ert_raiseJobControlSignal_(
    struct Ert_JobControl *self,
    int aSigNum, struct Ert_Pid aPid, struct Ert_Uid aUid)
{
    int rc = -1;

    if ( ! ert_ownWatchProcessSignalMethodNil(self->mRaise.mMethod))
        ERT_ERROR_IF(
            ert_callWatchProcessSignalMethod(
                self->mRaise.mMethod, aSigNum, aPid, aUid));

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
ert_raiseJobControlSigStop_(struct Ert_JobControl *self)
{
    int rc = -1;

    if ( ! ert_ownWatchProcessMethodNil(self->mStop.mPauseMethod))
        ERT_ERROR_IF(
            ert_callWatchProcessMethod(self->mStop.mPauseMethod));

    ERT_ERROR_IF(
        raise(SIGSTOP),
        {
            ert_warn(
                errno,
                "Unable to stop process pid %" PRId_Ert_Pid,
                FMTd_Ert_Pid(ert_ownProcessId()));
        });

    if ( ! ert_ownWatchProcessMethodNil(self->mStop.mResumeMethod))
        ERT_ERROR_IF(
            ert_callWatchProcessMethod(self->mStop.mResumeMethod));

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
ert_raiseJobControlSigCont_(struct Ert_JobControl *self)
{
    int rc = -1;

    if ( ! ert_ownWatchProcessMethodNil(self->mContinue.mMethod))
        ERT_ERROR_IF(
            ert_callWatchProcessMethod(self->mContinue.mMethod));

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_createJobControl(struct Ert_JobControl *self)
{
    int rc = -1;

    self->mRaise.mMethod      = Ert_WatchProcessSignalMethodNil();
    self->mReap.mMethod       = Ert_WatchProcessMethodNil();
    self->mStop.mPauseMethod  = Ert_WatchProcessMethodNil();
    self->mStop.mResumeMethod = Ert_WatchProcessMethodNil();
    self->mContinue.mMethod   = Ert_WatchProcessMethodNil();

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
struct Ert_JobControl *
ert_closeJobControl(struct Ert_JobControl *self)
{
    if (self)
    {
        ERT_ABORT_IF(ert_unwatchProcessSigCont());
        ERT_ABORT_IF(ert_unwatchProcessSigStop());
        ERT_ABORT_IF(ert_unwatchProcessSignals());
        ERT_ABORT_IF(ert_unwatchProcessChildren());
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
int
ert_watchJobControlSignals(struct Ert_JobControl              *self,
                       struct Ert_WatchProcessSignalMethod aRaiseMethod)
{
    int rc = -1;

    ERT_ERROR_IF(
        ert_ownWatchProcessSignalMethodNil(aRaiseMethod),
        {
            errno = EINVAL;
        });

    ERT_ERROR_UNLESS(
        ert_ownWatchProcessSignalMethodNil(self->mRaise.mMethod),
        {
            errno = EPERM;
        });

    self->mRaise.mMethod = aRaiseMethod;

    ERT_ERROR_IF(
        ert_watchProcessSignals(
            Ert_WatchProcessSignalMethod(self, ert_raiseJobControlSignal_)),
        {
            self->mRaise.mMethod = Ert_WatchProcessSignalMethodNil();
        });

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_unwatchJobControlSignals(struct Ert_JobControl *self)
{
    int rc = -1;

    ERT_ERROR_IF(
        ert_ownWatchProcessSignalMethodNil(self->mRaise.mMethod),
        {
            errno = EPERM;
        });

    ERT_ERROR_IF(
        ert_unwatchProcessSignals());

    self->mRaise.mMethod = Ert_WatchProcessSignalMethodNil();

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_watchJobControlDone(struct Ert_JobControl        *self,
                    struct Ert_WatchProcessMethod aReapMethod)
{
    int rc = -1;

    ERT_ERROR_IF(
        ert_ownWatchProcessMethodNil(aReapMethod),
        {
            errno = EINVAL;
        });

    ERT_ERROR_UNLESS(
        ert_ownWatchProcessMethodNil(self->mReap.mMethod),
        {
            errno = EPERM;
        });

    self->mReap.mMethod = aReapMethod;

    ERT_ERROR_IF(
        ert_watchProcessChildren(
            Ert_WatchProcessMethod(self, ert_reapJobControl_)),
        {
            self->mReap.mMethod = Ert_WatchProcessMethodNil();
        });

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_unwatchJobControlDone(struct Ert_JobControl *self)
{
    int rc = -1;

    ERT_ERROR_IF(
        ert_ownWatchProcessMethodNil(self->mReap.mMethod),
        {
            errno = EPERM;
        });

    ERT_ERROR_IF(
        ert_unwatchProcessChildren());

    self->mReap.mMethod = Ert_WatchProcessMethodNil();

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_watchJobControlStop(struct Ert_JobControl        *self,
                    struct Ert_WatchProcessMethod aPauseMethod,
                    struct Ert_WatchProcessMethod aResumeMethod)
{
    int rc = -1;

    ERT_ERROR_IF(
        ert_ownWatchProcessMethodNil(aPauseMethod) &&
        ert_ownWatchProcessMethodNil(aResumeMethod),
        {
            errno = EINVAL;
        });

    ERT_ERROR_UNLESS(
        ert_ownWatchProcessMethodNil(self->mStop.mPauseMethod) &&
        ert_ownWatchProcessMethodNil(self->mStop.mResumeMethod),
        {
            errno = EPERM;
        });

    self->mStop.mPauseMethod  = aPauseMethod;
    self->mStop.mResumeMethod = aResumeMethod;

    ERT_ERROR_IF(
        ert_watchProcessSigStop(
            Ert_WatchProcessMethod(self, ert_raiseJobControlSigStop_)),
        {
            self->mStop.mPauseMethod  = Ert_WatchProcessMethodNil();
            self->mStop.mResumeMethod = Ert_WatchProcessMethodNil();
        });

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_unwatchJobControlStop(struct Ert_JobControl *self)
{
    int rc = -1;

    ERT_ERROR_IF(
        ert_ownWatchProcessMethodNil(self->mStop.mPauseMethod) &&
        ert_ownWatchProcessMethodNil(self->mStop.mResumeMethod),
        {
            errno = EPERM;
        });

    ERT_ERROR_IF(
        ert_unwatchProcessSigStop());

    self->mStop.mPauseMethod  = Ert_WatchProcessMethodNil();
    self->mStop.mResumeMethod = Ert_WatchProcessMethodNil();

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_watchJobControlContinue(struct Ert_JobControl        *self,
                        struct Ert_WatchProcessMethod aContinueMethod)
{
    int rc = -1;

    ERT_ERROR_IF(
        ert_ownWatchProcessMethodNil(aContinueMethod),
        {
            errno = EINVAL;
        });

    ERT_ERROR_UNLESS(
        ert_ownWatchProcessMethodNil(self->mContinue.mMethod),
        {
            errno = EPERM;
        });

    self->mContinue.mMethod  = aContinueMethod;

    ERT_ERROR_IF(
        ert_watchProcessSigCont(
            Ert_WatchProcessMethod(self, ert_raiseJobControlSigCont_)),
        {
            self->mContinue.mMethod = Ert_WatchProcessMethodNil();
        });

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_unwatchJobControlContinue(struct Ert_JobControl *self)
{
    int rc = -1;

    ERT_ERROR_IF(
        ert_ownWatchProcessMethodNil(self->mContinue.mMethod),
        {
            errno = EPERM;
        });

    ERT_ERROR_IF(
        ert_unwatchProcessSigCont());

    self->mContinue.mMethod = Ert_WatchProcessMethodNil();

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
