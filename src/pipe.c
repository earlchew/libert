/* -*- c-basic-offset:4; indent-tabs-mode:nil -*- vi: set sw=4 et: */
/*
// Copyright (c) 2015, Earl Chew
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

#include "pipe.h"
#include "fd.h"
#include "error.h"
#include "macros.h"

#include <unistd.h>
#include <errno.h>

/* -------------------------------------------------------------------------- */
int
createPipe(struct Pipe *self)
{
    int rc = -1;

    self->mRdFile = 0;
    self->mWrFile = 0;

    int fd[2];

    if (pipe(fd))
        goto Finally;

    if (-1 == fd[0] || -1 == fd[1])
        goto Finally;

    ensure( ! stdFd(fd[0]));
    ensure( ! stdFd(fd[1]));

    if (createFileDescriptor(&self->mRdFile_, fd[0]))
        goto Finally;
    self->mRdFile = &self->mRdFile_;

    fd[0] = -1;

    if (createFileDescriptor(&self->mWrFile_, fd[1]))
        goto Finally;
    self->mWrFile = &self->mWrFile_;

    fd[1] = -1;

    rc = 0;

Finally:

    FINALLY
    ({
        if (-1 != fd[0])
            close(fd[0]);
        if (-1 != fd[1])
            close(fd[1]);

        if (rc)
        {
            closeFileDescriptor(self->mRdFile);
            closeFileDescriptor(self->mWrFile);
        }
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
int
closePipe(struct Pipe *self)
{
    return closeFileDescriptorPair(&self->mRdFile, &self->mWrFile);
}

/* -------------------------------------------------------------------------- */
