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
#ifndef ERT_FILE_H
#define ERT_FILE_H

#include "ert/compiler.h"
#include "ert/fd.h"
#include "ert/method.h"
#include "ert/queue.h"

#include <stdbool.h>

/* -------------------------------------------------------------------------- */
ERT_BEGIN_C_SCOPE;
struct Ert_File;
ERT_END_C_SCOPE;

#define ERT_METHOD_DEFINITION
#define ERT_METHOD_RETURN_FileVisitor    int
#define ERT_METHOD_CONST_FileVisitor
#define ERT_METHOD_ARG_LIST_FileVisitor  (const struct Ert_File *aFile_)
#define ERT_METHOD_CALL_LIST_FileVisitor (aFile_)

#define ERT_METHOD_TYPE_PREFIX     Ert_
#define ERT_METHOD_FUNCTION_PREFIX ert_

#define ERT_METHOD_NAME      FileVisitor
#define ERT_METHOD_RETURN    ERT_METHOD_RETURN_FileVisitor
#define ERT_METHOD_CONST     ERT_METHOD_CONST_FileVisitor
#define ERT_METHOD_ARG_LIST  ERT_METHOD_ARG_LIST_FileVisitor
#define ERT_METHOD_CALL_LIST ERT_METHOD_CALL_LIST_FileVisitor
#include "ert/method.h"

#define Ert_FileVisitor(Object_, Method_)          \
    ERT_METHOD_TRAMPOLINE(                         \
        Object_, Method_,                          \
        Ert_FileVisitor_,                          \
        ERT_METHOD_RETURN_FileVisitor,             \
        ERT_METHOD_CONST_FileVisitor,              \
        ERT_METHOD_ARG_LIST_FileVisitor,           \
        ERT_METHOD_CALL_LIST_FileVisitor)

/* -------------------------------------------------------------------------- */
ERT_BEGIN_C_SCOPE;

struct stat;
struct sockaddr;
struct ucred;

struct Duration;

struct Ert_File
{
    int                  mFd;
    LIST_ENTRY(Ert_File) mList;
};

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
ert_temporaryFile(struct Ert_File *self);

ERT_CHECKED int
ert_createFile(struct Ert_File *self, int aFd);

ERT_CHECKED int
ert_detachFile(struct Ert_File *self);

ERT_CHECKED struct Ert_File *
ert_closeFile(struct Ert_File *self);

void
ert_walkFileList(struct Ert_FileVisitor aVisitor);

ERT_CHECKED int
ert_duplicateFile(struct Ert_File *self, const struct Ert_File *aOther);

ERT_CHECKED int
nonBlockingFile(struct Ert_File *self, unsigned aNonBlocking);

ERT_CHECKED int
ert_ownFileNonBlocking(const struct Ert_File *self);

ERT_CHECKED int
ert_closeFileOnExec(struct Ert_File *self, unsigned aCloseOnExec);

ERT_CHECKED int
ert_ownFileCloseOnExec(const struct Ert_File *self);

ERT_CHECKED off_t
ert_lseekFile(struct Ert_File *self,
              off_t aOffset, struct Ert_WhenceType aWhenceType);

ERT_CHECKED int
ert_fstatFile(struct Ert_File *self, struct stat *aStat);

ERT_CHECKED int
ert_fcntlFileGetFlags(struct Ert_File *self);

ERT_CHECKED int
ert_ftruncateFile(struct Ert_File *self, off_t aLength);

bool
ert_ownFileValid(const struct Ert_File *self);

/* -------------------------------------------------------------------------- */
ERT_CHECKED ssize_t
ert_writeFile(struct Ert_File *self,
          const char *aBuf, size_t aLen, const struct Duration *aTimeout);

ERT_CHECKED ssize_t
ert_readFile(struct Ert_File *self,
         char *aBuf, size_t aLen, const struct Duration *aTimeout);

/* -------------------------------------------------------------------------- */
ERT_CHECKED ssize_t
ert_writeFileDeadline(struct Ert_File *self,
                  const char *aBuf, size_t aLen,
                  struct Ert_Deadline *aDeadline);

ERT_CHECKED ssize_t
ert_readFileDeadline(struct Ert_File *self,
                 char *aBuf, size_t aLen,
                 struct Ert_Deadline *aDeadline);

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
ert_waitFileWriteReady(const struct Ert_File     *self,
                   const struct Duration *aTimeout);

ERT_CHECKED int
ert_waitFileReadReady(const struct Ert_File     *self,
                  const struct Duration *aTimeout);

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
ert_lockFile(struct Ert_File *self, struct Ert_LockType ert_aLockType);

ERT_CHECKED int
ert_unlockFile(struct Ert_File *self);

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
ert_lockFileRegion(
    struct Ert_File *self,
    struct Ert_LockType ert_aLockType, off_t aPos, off_t aLen);

ERT_CHECKED int
ert_unlockFileRegion(struct Ert_File *self, off_t aPos, off_t aLen);

struct Ert_LockType
ert_ownFileRegionLocked(const struct Ert_File *self, off_t aPos, off_t aLen);

/* -------------------------------------------------------------------------- */

ERT_END_C_SCOPE;

#endif /* ERT_FILE_H */
