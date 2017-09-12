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
#ifndef ERT_JOBCONTROL_H
#define ERT_JOBCONTROL_H

#include "ert/compiler.h"
#include "ert/process.h"
#include "ert/thread.h"

ERT_BEGIN_C_SCOPE;

/* -------------------------------------------------------------------------- */
struct Ert_JobControl
{
    struct
    {
        struct Ert_WatchProcessSignalMethod mMethod;
    } mRaise;

    struct
    {
        struct Ert_WatchProcessMethod mMethod;
    } mReap;

    struct
    {
        struct Ert_WatchProcessMethod mPauseMethod;
        struct Ert_WatchProcessMethod mResumeMethod;
    } mStop;

    struct
    {
        struct Ert_WatchProcessMethod mMethod;
    } mContinue;
};

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
ert_createJobControl(
    struct Ert_JobControl *self);

ERT_CHECKED struct Ert_JobControl *
ert_closeJobControl(
    struct Ert_JobControl *self);

ERT_CHECKED int
ert_watchJobControlSignals(
    struct Ert_JobControl              *self,
    struct Ert_WatchProcessSignalMethod aRaiseMethod);

ERT_CHECKED int
ert_unwatchJobControlSignals(
    struct Ert_JobControl *self);

ERT_CHECKED int
ert_watchJobControlDone(
    struct Ert_JobControl        *self,
    struct Ert_WatchProcessMethod aReapMethod);

ERT_CHECKED int
ert_unwatchJobControlDone(
    struct Ert_JobControl *self);

ERT_CHECKED int
ert_watchJobControlStop(
    struct Ert_JobControl        *self,
    struct Ert_WatchProcessMethod aPauseMethod,
    struct Ert_WatchProcessMethod aResumeMethod);

ERT_CHECKED int
ert_unwatchJobControlStop(
    struct Ert_JobControl *self);

ERT_CHECKED int
ert_watchJobControlContinue(
    struct Ert_JobControl        *self,
    struct Ert_WatchProcessMethod aContinueMethod);

ERT_CHECKED int
ert_unwatchJobControlContinue(
    struct Ert_JobControl *self);

/* -------------------------------------------------------------------------- */

ERT_END_C_SCOPE;

#endif /* ERT_JOBCONTROL_H */
