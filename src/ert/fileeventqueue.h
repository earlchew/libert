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
struct FileEventQueue;
ERT_END_C_SCOPE;

#define ERT_METHOD_DEFINITION
#define ERT_METHOD_RETURN_FileEventQueueActivityMethod    int
#define ERT_METHOD_CONST_FileEventQueueActivityMethod
#define ERT_METHOD_ARG_LIST_FileEventQueueActivityMethod  ()
#define ERT_METHOD_CALL_LIST_FileEventQueueActivityMethod ()

#define ERT_METHOD_NAME      FileEventQueueActivityMethod
#define ERT_METHOD_RETURN    ERT_METHOD_RETURN_FileEventQueueActivityMethod
#define ERT_METHOD_CONST     ERT_METHOD_CONST_FileEventQueueActivityMethod
#define ERT_METHOD_ARG_LIST  ERT_METHOD_ARG_LIST_FileEventQueueActivityMethod
#define ERT_METHOD_CALL_LIST ERT_METHOD_CALL_LIST_FileEventQueueActivityMethod
#include "ert/method.h"

#define FileEventQueueActivityMethod(Object_, Method_)      \
    ERT_METHOD_TRAMPOLINE(                                  \
        Object_, Method_,                                   \
        FileEventQueueActivityMethod_,                      \
        ERT_METHOD_RETURN_FileEventQueueActivityMethod,     \
        ERT_METHOD_CONST_FileEventQueueActivityMethod,      \
        ERT_METHOD_ARG_LIST_FileEventQueueActivityMethod,   \
        ERT_METHOD_CALL_LIST_FileEventQueueActivityMethod)

/* -------------------------------------------------------------------------- */
ERT_BEGIN_C_SCOPE;

struct Duration;

enum EventQueuePollTrigger
{
    EventQueuePollDisconnect,
    EventQueuePollRead,
    EventQueuePollWrite,
    EventQueuePollTriggers,
};

/* -------------------------------------------------------------------------- */
struct FileEventQueue
{
    struct File         mFile_;
    struct File        *mFile;
    struct epoll_event *mQueue;
    int                 mQueueSize;
    int                 mQueuePending;
    int                 mNumArmed;
    int                 mNumPending;
};

struct FileEventQueueActivity
{
    struct FileEventQueue               *mQueue;
    struct File                         *mFile;
    struct epoll_event                  *mPending;
    unsigned                             mArmed;
    struct FileEventQueueActivityMethod  mMethod;
};

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
createFileEventQueue(struct FileEventQueue *self, int aQueueSize);

ERT_CHECKED struct FileEventQueue *
closeFileEventQueue(struct FileEventQueue *self);

ERT_CHECKED int
pollFileEventQueueActivity(struct FileEventQueue *self,
                           const struct Duration *aTimeout);

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
createFileEventQueueActivity(struct FileEventQueueActivity *self,
                             struct FileEventQueue         *aQueue,
                             struct File                   *aFile);

ERT_CHECKED int
armFileEventQueueActivity(struct FileEventQueueActivity      *self,
                          enum EventQueuePollTrigger          aTrigger,
                          struct FileEventQueueActivityMethod aMethod);

ERT_CHECKED struct FileEventQueueActivity *
closeFileEventQueueActivity(struct FileEventQueueActivity *self);

/* -------------------------------------------------------------------------- */

ERT_END_C_SCOPE;

#endif /* ERT_FILEEVENTQUEUE_H */
