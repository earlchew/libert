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
#ifndef ERT_PATHNAME_H
#define ERT_PATHNAME_H

#include "ert/compiler.h"
#include "ert/file.h"

ERT_BEGIN_C_SCOPE;

struct stat;

struct Ert_PathName
{
    char *mFileName;
    char *mDirName_;
    char *mDirName;
    char *mBaseName_;
    char *mBaseName;

    struct Ert_File  mDirFile_;
    struct Ert_File *mDirFile;
};

enum Ert_PathNameStatus
{
    Ert_PathNameStatusError        = -1,
    Ert_PathNameStatusOk           = 0,
    Ert_PathNameStatusInaccessible = 1,
};

/* -------------------------------------------------------------------------- */
ERT_CHECKED enum Ert_PathNameStatus
ert_createPathName(
    struct Ert_PathName *self,
    const char *aFileName);

ERT_CHECKED struct Ert_PathName *
ert_closePathName(
    struct Ert_PathName *self);

ERT_CHECKED int
ert_openPathName(
    struct Ert_PathName *self,
    int aFlags, mode_t aMode);

ERT_CHECKED int
ert_unlinkPathName(
    struct Ert_PathName *self,
    int aFlags);

ERT_CHECKED int
ert_fstatPathName(
    const struct Ert_PathName *self,
    struct stat *aStat, int aFlags);

/* -------------------------------------------------------------------------- */

ERT_END_C_SCOPE;

#endif /* ERT_PATHNAME_H */
