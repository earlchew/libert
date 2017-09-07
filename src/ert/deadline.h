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
#ifndef ERT_DEADLINE_H
#define ERT_DEADLINE_H

#include "ert/compiler.h"
#include "ert/timekeeping.h"
#include "ert/process.h"
#include "ert/method.h"

#include <stdbool.h>

ERT_BEGIN_C_SCOPE;
struct Duration;
ERT_END_C_SCOPE;

/* -------------------------------------------------------------------------- */
#define ERT_METHOD_DEFINITION
#define ERT_METHOD_RETURN_DeadlinePollMethod    int
#define ERT_METHOD_CONST_DeadlinePollMethod
#define ERT_METHOD_ARG_LIST_DeadlinePollMethod  ()
#define ERT_METHOD_CALL_LIST_DeadlinePollMethod ()

#define ERT_METHOD_TYPE_PREFIX     Ert_
#define ERT_METHOD_FUNCTION_PREFIX ert_

#define ERT_METHOD_NAME      DeadlinePollMethod
#define ERT_METHOD_RETURN    ERT_METHOD_RETURN_DeadlinePollMethod
#define ERT_METHOD_CONST     ERT_METHOD_CONST_DeadlinePollMethod
#define ERT_METHOD_ARG_LIST  ERT_METHOD_ARG_LIST_DeadlinePollMethod
#define ERT_METHOD_CALL_LIST ERT_METHOD_CALL_LIST_DeadlinePollMethod
#include "ert/method.h"

#define Ert_DeadlinePollMethod(Object_, Method_) \
    ERT_METHOD_TRAMPOLINE(                           \
        Object_, Method_,                        \
        Ert_DeadlinePollMethod_,                 \
        ERT_METHOD_RETURN_DeadlinePollMethod,    \
        ERT_METHOD_CONST_DeadlinePollMethod,     \
        ERT_METHOD_ARG_LIST_DeadlinePollMethod,  \
        ERT_METHOD_CALL_LIST_DeadlinePollMethod)

/* -------------------------------------------------------------------------- */
#define ERT_METHOD_DEFINITION
#define ERT_METHOD_RETURN_DeadlineWaitMethod    int
#define ERT_METHOD_CONST_DeadlineWaitMethod
#define ERT_METHOD_ARG_LIST_DeadlineWaitMethod  \
    (const struct Duration *aTimeout_)
#define ERT_METHOD_CALL_LIST_DeadlineWaitMethod (aTimeout_)

#define ERT_METHOD_TYPE_PREFIX     Ert_
#define ERT_METHOD_FUNCTION_PREFIX ert_

#define ERT_METHOD_NAME      DeadlineWaitMethod
#define ERT_METHOD_RETURN    ERT_METHOD_RETURN_DeadlineWaitMethod
#define ERT_METHOD_CONST     ERT_METHOD_CONST_DeadlineWaitMethod
#define ERT_METHOD_ARG_LIST  ERT_METHOD_ARG_LIST_DeadlineWaitMethod
#define ERT_METHOD_CALL_LIST ERT_METHOD_CALL_LIST_DeadlineWaitMethod
#include "ert/method.h"

#define Ert_DeadlineWaitMethod(Object_, Method_) \
    ERT_METHOD_TRAMPOLINE(                           \
        Object_, Method_,                        \
        Ert_DeadlineWaitMethod_,                 \
        ERT_METHOD_RETURN_DeadlineWaitMethod,    \
        ERT_METHOD_CONST_DeadlineWaitMethod,     \
        ERT_METHOD_ARG_LIST_DeadlineWaitMethod,  \
        ERT_METHOD_CALL_LIST_DeadlineWaitMethod)

/* -------------------------------------------------------------------------- */
ERT_BEGIN_C_SCOPE;

struct Ert_Deadline
{
    struct EventClockTime        mSince;
    struct EventClockTime        mTime;
    struct Duration              mRemaining;
    struct ProcessSigContTracker mSigContTracker;
    struct Duration              mDuration_;
    struct Duration             *mDuration;
    bool                         mExpired;
};

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
ert_createDeadline(struct Ert_Deadline *self, const struct Duration *aDuration);

ERT_CHECKED int
ert_checkDeadlineExpired(struct Ert_Deadline *self,
                         struct Ert_DeadlinePollMethod aPollMethod,
                         struct Ert_DeadlineWaitMethod aWaitMethod);

bool
ert_ownDeadlineExpired(const struct Ert_Deadline *self);

ERT_CHECKED struct Ert_Deadline *
ert_closeDeadline(struct Ert_Deadline *self);

/* -------------------------------------------------------------------------- */

ERT_END_C_SCOPE;

#endif /* ERT_DEADLINE_H */
