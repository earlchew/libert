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
#ifndef ERT_EVENTLATCH_H
#define ERT_EVENTLATCH_H

#include "ert/compiler.h"
#include "ert/thread.h"
#include "ert/method.h"
#include "ert/queue.h"

#include <stdio.h>

/* -------------------------------------------------------------------------- */
ERT_BEGIN_C_SCOPE;
struct Ert_EventClockTime;
ERT_END_C_SCOPE;

#define ERT_METHOD_DEFINITION
#define ERT_METHOD_RETURN_EventLatchMethod    int
#define ERT_METHOD_CONST_EventLatchMethod
#define ERT_METHOD_ARG_LIST_EventLatchMethod  ( \
    bool                         aEnabled_,     \
    const struct Ert_EventClockTime *aPollTime_)
#define ERT_METHOD_CALL_LIST_EventLatchMethod (aEnabled_, aPollTime_)

#define ERT_METHOD_TYPE_PREFIX     Ert_
#define ERT_METHOD_FUNCTION_PREFIX ert_

#define ERT_METHOD_NAME      EventLatchMethod
#define ERT_METHOD_RETURN    ERT_METHOD_RETURN_EventLatchMethod
#define ERT_METHOD_CONST     ERT_METHOD_CONST_EventLatchMethod
#define ERT_METHOD_ARG_LIST  ERT_METHOD_ARG_LIST_EventLatchMethod
#define ERT_METHOD_CALL_LIST ERT_METHOD_CALL_LIST_EventLatchMethod
#include "ert/method.h"

#define Ert_EventLatchMethod(Object_, Method_) \
    ERT_METHOD_TRAMPOLINE(                     \
        Object_, Method_,                      \
        Ert_EventLatchMethod_,                 \
        ERT_METHOD_RETURN_EventLatchMethod,    \
        ERT_METHOD_CONST_EventLatchMethod,     \
        ERT_METHOD_ARG_LIST_EventLatchMethod,  \
        ERT_METHOD_CALL_LIST_EventLatchMethod)

/* -------------------------------------------------------------------------- */
ERT_BEGIN_C_SCOPE;

struct Ert_EventPipe;
struct Ert_EventLatch;

struct Ert_EventLatchListEntry
{
    struct Ert_EventLatch              *mLatch;
    struct Ert_EventLatchMethod         mMethod;
    LIST_ENTRY(Ert_EventLatchListEntry) mEntry;
};

struct Ert_EventLatchList
{
    LIST_HEAD(, Ert_EventLatchListEntry) mList;
};

struct Ert_EventLatch
{
    struct Ert_ThreadSigMutex           mMutex_;
    struct Ert_ThreadSigMutex          *mMutex;
    unsigned                        mEvent;
    struct Ert_EventPipe               *mPipe;
    char                           *mName;
    struct Ert_EventLatchListEntry  mList;
};

enum Ert_EventLatchSetting
{
    Ert_EventLatchSettingError    = -1,
    Ert_EventLatchSettingDisabled =  0,
    Ert_EventLatchSettingOff      =  1,
    Ert_EventLatchSettingOn       =  2,
};

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
ert_pollEventLatchListEntry(struct Ert_EventLatchListEntry  *self,
                            const struct Ert_EventClockTime *aPollTime);

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
ert_createEventLatch(struct Ert_EventLatch *self, const char *aName);

ERT_CHECKED struct Ert_EventLatch *
ert_closeEventLatch(struct Ert_EventLatch *self);

int
ert_printEventLatch(const struct Ert_EventLatch *self, FILE *aFile);

ERT_CHECKED enum Ert_EventLatchSetting
ert_bindEventLatchPipe(struct Ert_EventLatch       *self,
                       struct Ert_EventPipe        *aPipe,
                       struct Ert_EventLatchMethod  aMethod);

ERT_CHECKED enum Ert_EventLatchSetting
ert_unbindEventLatchPipe(struct Ert_EventLatch *self);

ERT_CHECKED enum Ert_EventLatchSetting
ert_disableEventLatch(struct Ert_EventLatch *self);

ERT_CHECKED enum Ert_EventLatchSetting
ert_setEventLatch(struct Ert_EventLatch *self);

ERT_CHECKED enum Ert_EventLatchSetting
ert_resetEventLatch(struct Ert_EventLatch *self);

enum Ert_EventLatchSetting
ert_ownEventLatchSetting(const struct Ert_EventLatch *self);

/* -------------------------------------------------------------------------- */

ERT_END_C_SCOPE;

#endif /* ERT_EVENTLATCH_H */
