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
#ifndef ERT_FDSET_H
#define ERT_FDSET_H

#include "ert/compiler.h"

#include "ert/tree.h"

#include <stdio.h>

#include <sys/types.h>

/* -------------------------------------------------------------------------- */
ERT_BEGIN_C_SCOPE;
struct Ert_File;
struct Ert_FdRange;

struct Ert_FdRange
Ert_FdRange_(int aLhs, int aRhs);

struct Ert_FdRange
{
#ifdef __cplusplus
    Ert_FdRange(int aLhs, int aRhs)
    { *this = Ert_FdRange_(aLhs, aRhs); }
#endif

    int mLhs;
    int mRhs;
};
ERT_END_C_SCOPE;

/* -------------------------------------------------------------------------- */
#define ERT_METHOD_DEFINITION
#define ERT_METHOD_RETURN_FdSetVisitor    int
#define ERT_METHOD_CONST_FdSetVisitor
#define ERT_METHOD_ARG_LIST_FdSetVisitor  (struct Ert_FdRange aFdRange_)
#define ERT_METHOD_CALL_LIST_FdSetVisitor (aFdRange_)

#define ERT_METHOD_TYPE_PREFIX     Ert_
#define ERT_METHOD_FUNCTION_PREFIX ert_

#define ERT_METHOD_NAME      FdSetVisitor
#define ERT_METHOD_RETURN    ERT_METHOD_RETURN_FdSetVisitor
#define ERT_METHOD_CONST     ERT_METHOD_CONST_FdSetVisitor
#define ERT_METHOD_ARG_LIST  ERT_METHOD_ARG_LIST_FdSetVisitor
#define ERT_METHOD_CALL_LIST ERT_METHOD_CALL_LIST_FdSetVisitor
#include "ert/method.h"

#define Ert_FdSetVisitor(Object_, Method_)      \
    ERT_METHOD_TRAMPOLINE(                      \
        Object_, Method_,                       \
        Ert_FdSetVisitor_,                      \
        ERT_METHOD_RETURN_FdSetVisitor,         \
        ERT_METHOD_CONST_FdSetVisitor,          \
        ERT_METHOD_ARG_LIST_FdSetVisitor,       \
        ERT_METHOD_CALL_LIST_FdSetVisitor)

/* -------------------------------------------------------------------------- */
ERT_BEGIN_C_SCOPE;

struct Ert_FdSetElement_
{
    RB_ENTRY(Ert_FdSetElement_) mTree;

    struct Ert_FdRange mRange;
};

typedef RB_HEAD(Ert_FdSetTree_, Ert_FdSetElement_) Ert_FdSetTreeT_;

struct Ert_FdSet
{
    Ert_FdSetTreeT_ mRoot;

    size_t mSize;
};

#define ERT_FDSET_INITIALIZER(FdSet_) \
    { .mRoot = RB_INITIALIZER((FdSet_).mRoot) }

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
ert_createFdSet(
    struct Ert_FdSet *self);

struct Ert_FdSet *
ert_closeFdSet(
    struct Ert_FdSet *self);

int
ert_printFdSet(
    const struct Ert_FdSet *self,
    FILE                   *aFile);

void
ert_clearFdSet(
    struct Ert_FdSet *self);

ERT_CHECKED int
ert_invertFdSet(
    struct Ert_FdSet *self);

ERT_CHECKED int
ert_fillFdSet(
    struct Ert_FdSet *self);

ERT_CHECKED int
ert_insertFdSetRange(
    struct Ert_FdSet  *self,
    struct Ert_FdRange aRange);

ERT_CHECKED int
ert_removeFdSetRange(
    struct Ert_FdSet  *self,
    struct Ert_FdRange aRange);

ERT_CHECKED int
ert_insertFdSet(
    struct Ert_FdSet *self,
    int              aFd);

ERT_CHECKED int
ert_removeFdSet(
    struct Ert_FdSet *self,
    int               aFd);

ERT_CHECKED int
ert_insertFdSetFile(
    struct Ert_FdSet      *self,
    const struct Ert_File *aFile);

ERT_CHECKED int
ert_removeFdSetFile(
    struct Ert_FdSet      *self,
    const struct Ert_File *aFile);

ERT_CHECKED ssize_t
ert_visitFdSet(
    const struct Ert_FdSet *self,
    struct Ert_FdSetVisitor aVisitor);

/* -------------------------------------------------------------------------- */
#ifndef __cplusplus
static inline struct Ert_FdRange
Ert_FdRange(int aLhs, int aRhs)
{
    return Ert_FdRange_(aLhs, aRhs);
}
#endif

int
ert_containsFdRange(
    struct Ert_FdRange self,
    struct Ert_FdRange aOther);

bool
ert_leftFdRangeOf(
    struct Ert_FdRange self,
    struct Ert_FdRange aOther);

bool
ert_rightFdRangeOf(
    struct Ert_FdRange self,
    struct Ert_FdRange aOther);

bool
ert_leftFdRangeNeighbour(
    struct Ert_FdRange self,
    struct Ert_FdRange aOther);

bool
ert_rightFdRangeNeighbour(
    struct Ert_FdRange self,
    struct Ert_FdRange aOther);

/* -------------------------------------------------------------------------- */

ERT_END_C_SCOPE;

#endif /* ERT_FDSET_H */
