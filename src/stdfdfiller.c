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

#include "ert/stdfdfiller.h"
#include "ert/error.h"

#include <unistd.h>

/* -------------------------------------------------------------------------- */
int
ert_createStdFdFiller(
    struct Ert_StdFdFiller *self)
{
    int rc = -1;

    for (unsigned ix = 0; ERT_NUMBEROF(self->mFile) > ix; ++ix)
        self->mFile[ix] = 0;

    int fd[2];

    ERT_ERROR_IF(
        pipe(fd),
        {
            fd[0] = -1;
            fd[1] = -1;
        });

    ERT_ERROR_IF(
        -1 == fd[0] || -1 == fd[1],
        {
            errno = EBADF;
        });

    /* Close the writing end of the pipe, leaving only the reading
     * end of the pipe. Any attempt to write will fail, and any
     * attempt to read will yield EOF. */

    fd[1] = ert_closeFd(fd[1]);

    ERT_ERROR_IF(
        ert_createFile(&self->mFile_[0], fd[0]));
    self->mFile[0] = &self->mFile_[0];

    fd[0] = -1;

    ERT_ERROR_IF(
        ert_duplicateFile(&self->mFile_[1], &self->mFile_[0]));
    self->mFile[1] = &self->mFile_[1];

    ERT_ERROR_IF(
        ert_duplicateFile(&self->mFile_[2], &self->mFile_[0]));
    self->mFile[2] = &self->mFile_[2];

    rc = 0;

Ert_Finally:

    ERT_FINALLY
    ({
        fd[0] = ert_closeFd(fd[0]);
        fd[1] = ert_closeFd(fd[1]);

        if (rc)
            self = ert_closeStdFdFiller(self);
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
struct Ert_StdFdFiller *
ert_closeStdFdFiller(
    struct Ert_StdFdFiller *self)
{
    if (self)
    {
        for (unsigned ix = 0; ERT_NUMBEROF(self->mFile) > ix; ++ix)
            self->mFile[ix] = ert_closeFile(self->mFile[ix]);
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
