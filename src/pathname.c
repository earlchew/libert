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

#include "ert/pathname.h"
#include "ert/error.h"

#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/stat.h>

/* -------------------------------------------------------------------------- */
enum Ert_PathNameStatus
ert_createPathName(struct Ert_PathName *self, const char *aFileName)
{
    int rc = -1;

    enum Ert_PathNameStatus status = Ert_PathNameStatusOk;

    self->mFileName  = 0;
    self->mBaseName_ = 0;
    self->mBaseName  = 0;
    self->mDirName_  = 0;
    self->mDirName   = 0;
    self->mDirFile   = 0;

    ERROR_UNLESS(
        (self->mFileName = strdup(aFileName)));
    ERROR_UNLESS(
        (self->mDirName_ = strdup(self->mFileName)));
    ERROR_UNLESS(
        (self->mBaseName_ = strdup(self->mFileName)));
    ERROR_UNLESS(
        (self->mDirName  = strdup(dirname(self->mDirName_))));
    ERROR_UNLESS(
        (self->mBaseName = strdup(basename(self->mBaseName_))));

    do
    {
        if (ert_createFile(
                &self->mDirFile_,
                ert_openFd(self->mDirName, O_RDONLY | O_CLOEXEC, 0)))
        {
            status = Ert_PathNameStatusInaccessible;
            break;
        }

        self->mDirFile = &self->mDirFile_;

    } while (0);

    rc = 0;

Finally:

    FINALLY
    ({
        if (rc)
            status = Ert_PathNameStatusError;

        if (Ert_PathNameStatusOk != status)
            self = ert_closePathName(self);
    });

    return status;
}

/* -------------------------------------------------------------------------- */
struct Ert_PathName *
ert_closePathName(struct Ert_PathName *self)
{
    if (self)
    {
        self->mDirFile = ert_closeFile(self->mDirFile);

        free(self->mFileName);
        free(self->mBaseName_);
        free(self->mBaseName);
        free(self->mDirName_);
        free(self->mDirName);

        self->mFileName  = 0;
        self->mBaseName_ = 0;
        self->mBaseName  = 0;
        self->mDirName_  = 0;
        self->mDirName   = 0;
        self->mDirFile   = 0;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
int
ert_openPathName(struct Ert_PathName *self, int aFlags, mode_t aMode)
{
    return openat(self->mDirFile->mFd, self->mBaseName, aFlags, aMode);
}

/* -------------------------------------------------------------------------- */
int
ert_unlinkPathName(struct Ert_PathName *self, int aFlags)
{
    return unlinkat(self->mDirFile->mFd, self->mBaseName, aFlags);
}

/* -------------------------------------------------------------------------- */
int
ert_fstatPathName(const struct Ert_PathName *self, struct stat *aStat, int aFlags)
{
    return fstatat(self->mDirFile->mFd, self->mBaseName, aStat, aFlags);
}

/* -------------------------------------------------------------------------- */
