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
#ifndef ERT_FILEEVENTQUEUE_H
#define ERT_FILEEVENTQUEUE_H

#include "ert/compiler.h"
#include "ert/file.h"
#include "ert/method.h"

/* -------------------------------------------------------------------------- */
ERT_BEGIN_C_SCOPE;
struct Ert_FileEventQueue;
ERT_END_C_SCOPE;

#define ERT_METHOD_DEFINITION
#define ERT_METHOD_RETURN_FileEventQueueActivityMethod    int
#define ERT_METHOD_CONST_FileEventQueueActivityMethod
#define ERT_METHOD_ARG_LIST_FileEventQueueActivityMethod  ()
#define ERT_METHOD_CALL_LIST_FileEventQueueActivityMethod ()

#define ERT_METHOD_TYPE_PREFIX     Ert_
#define ERT_METHOD_FUNCTION_PREFIX ert_

#define ERT_METHOD_NAME      FileEventQueueActivityMethod
#define ERT_METHOD_RETURN    ERT_METHOD_RETURN_FileEventQueueActivityMethod
#define ERT_METHOD_CONST     ERT_METHOD_CONST_FileEventQueueActivityMethod
#define ERT_METHOD_ARG_LIST  ERT_METHOD_ARG_LIST_FileEventQueueActivityMethod
#define ERT_METHOD_CALL_LIST ERT_METHOD_CALL_LIST_FileEventQueueActivityMethod
#include "ert/method.h"

#define Ert_FileEventQueueActivityMethod(Object_, Method_)  \
    ERT_METHOD_TRAMPOLINE(                                  \
        Object_, Method_,                                   \
        Ert_FileEventQueueActivityMethod_,                  \
        ERT_METHOD_RETURN_FileEventQueueActivityMethod,     \
        ERT_METHOD_CONST_FileEventQueueActivityMethod,      \
        ERT_METHOD_ARG_LIST_FileEventQueueActivityMethod,   \
        ERT_METHOD_CALL_LIST_FileEventQueueActivityMethod)

/* -------------------------------------------------------------------------- */
ERT_BEGIN_C_SCOPE;

struct Duration;

enum Ert_FileEventQueuePollTrigger
{
    Ert_FileEventQueuePollDisconnect,
    Ert_FileEventQueuePollRead,
    Ert_FileEventQueuePollWrite,
    Ert_FileEventQueuePollTriggers,
};

/* -------------------------------------------------------------------------- */
struct Ert_FileEventQueue
{
    struct Ert_File     mFile_;
    struct Ert_File    *mFile;
    struct epoll_event *mQueue;
    int                 mQueueSize;
    int                 mQueuePending;
    int                 mNumArmed;
    int                 mNumPending;
};

struct Ert_FileEventQueueActivity
{
    struct Ert_FileEventQueue              *mQueue;
    struct Ert_File                        *mFile;
    struct epoll_event                     *mPending;
    unsigned                                mArmed;
    struct Ert_FileEventQueueActivityMethod mMethod;
};

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
ert_createFileEventQueue(struct Ert_FileEventQueue *self, int aQueueSize);

ERT_CHECKED struct Ert_FileEventQueue *
ert_closeFileEventQueue(struct Ert_FileEventQueue *self);

ERT_CHECKED int
ert_pollFileEventQueueActivity(struct Ert_FileEventQueue *self,
                               const struct Duration     *aTimeout);

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
ert_createFileEventQueueActivity(struct Ert_FileEventQueueActivity *self,
                                 struct Ert_FileEventQueue         *aQueue,
                                 struct Ert_File                   *aFile);

ERT_CHECKED int
ert_armFileEventQueueActivity(
    struct Ert_FileEventQueueActivity      *self,
    enum Ert_FileEventQueuePollTrigger      aTrigger,
    struct Ert_FileEventQueueActivityMethod aMethod);

ERT_CHECKED struct Ert_FileEventQueueActivity *
ert_closeFileEventQueueActivity(struct Ert_FileEventQueueActivity *self);

/* -------------------------------------------------------------------------- */

ERT_END_C_SCOPE;

#endif /* ERT_FILEEVENTQUEUE_H */
