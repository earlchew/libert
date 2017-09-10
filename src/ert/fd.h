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
#ifndef ERT_FD_H
#define ERT_FD_H

#include "ert/compiler.h"

#include <stdbool.h>

#include <sys/types.h>

ERT_BEGIN_C_SCOPE;

struct Ert_Deadline;
struct Duration;
struct Ert_FdSet;

/* -------------------------------------------------------------------------- */
struct Ert_LockType
{
    enum
    {
        Ert_LockTypeError_    = -1,
        Ert_LockTypeUnlocked_ = 0,
        Ert_LockTypeRead_     = 1,
        Ert_LockTypeWrite_    = 2,
    } mType;
};

#ifdef __cplusplus
#define Ert_LockTypeError_    Ert_LockType::Ert_LockTypeError_
#define Ert_LockTypeUnlocked_ Ert_LockType::Ert_LockTypeUnlocked_
#define Ert_LockTypeRead_     Ert_LockType::Ert_LockTypeRead_
#define Ert_LockTypeWrite_    Ert_LockType::Ert_LockTypeWrite_
#endif

#define Ert_LockType_(Value_) ((struct Ert_LockType) { mType: (Value_) })

#define Ert_LockTypeError    (Ert_LockType_(Ert_LockTypeError_))
#define Ert_LockTypeUnlocked (Ert_LockType_(Ert_LockTypeUnlocked_))
#define Ert_LockTypeRead     (Ert_LockType_(Ert_LockTypeRead_))
#define Ert_LockTypeWrite    (Ert_LockType_(Ert_LockTypeWrite_))

/* -------------------------------------------------------------------------- */
struct Ert_WhenceType
{
    enum
    {
        Ert_WhenceTypeStart_,
        Ert_WhenceTypeHere_,
        Ert_WhenceTypeEnd_,
    } mType;
};

#ifdef __cplusplus
#define Ert_WhenceTypeStart_ Ert_WhenceType::Ert_WhenceTypeStart_
#define Ert_WhenceTypeHere_  Ert_WhenceType::Ert_WhenceTypeHere_
#define Ert_WhenceTypeEnd_   Ert_WhenceType::Ert_WhenceTypeEnd_
#endif

#define Ert_WhenceType_(Value_) ((struct Ert_WhenceType) { mType: (Value_) })

#define Ert_WhenceTypeStart (Ert_WhenceType_(Ert_WhenceTypeStart_))
#define Ert_WhenceTypeHere  (Ert_WhenceType_(Ert_WhenceTypeHere_))
#define Ert_WhenceTypeEnd   (Ert_WhenceType_(Ert_WhenceTypeEnd_))

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
ert_openFd(const char *aPathName, int aFlags, mode_t aMode);

ERT_CHECKED int
ert_openStdFds(void);

ERT_CHECKED int
ert_closeFd(int aFd);

ERT_CHECKED int
ert_closeFdDescriptors(const int *aWhiteList, size_t aWhiteListLen);

ERT_CHECKED int
ert_closeFdExceptWhiteList(const struct Ert_FdSet *aFdSet);

ERT_CHECKED int
ert_closeFdOnlyBlackList(const struct Ert_FdSet *aFdSet);

bool
ert_stdFd(int aFd);

ERT_CHECKED int
ert_duplicateFd(int aFd, int aTargetFd);

ERT_CHECKED int
ert_nullifyFd(int aFd);

ERT_CHECKED int
ert_nonBlockingFd(int aFd, unsigned aNonBlocking);

ERT_CHECKED int
ert_ownFdNonBlocking(int aFd);

ERT_CHECKED int
ert_ownFdValid(int aFd);

ERT_CHECKED int
ert_ownFdFlags(int aFd);

ERT_CHECKED int
ert_closeFdOnExec(int aFd, unsigned aCloseOnExec);

ERT_CHECKED int
ert_ownFdCloseOnExec(int aFd);

ERT_CHECKED int
ert_ioctlFd(int aFd, int aReq, void *aArg);

ERT_CHECKED ssize_t
ert_spliceFd(int aSrcFd, int aDstFd, size_t aLen, unsigned aFlags);

ERT_CHECKED off_t
ert_lseekFd(int aFd, off_t aOffset, struct Ert_WhenceType aWhenceType);

ERT_CHECKED ssize_t
ert_readFdFully(int aFd, char **aBuf, size_t aBufSize);

/* -------------------------------------------------------------------------- */
ERT_CHECKED ssize_t
ert_writeFd(int aFd,
            const char *aBuf, size_t aLen, const struct Duration *aTimeout);

ERT_CHECKED ssize_t
ert_writeFdRaw(int aFd,
               const char *aBuf, size_t aLen, const struct Duration *aTimeout);

ERT_CHECKED ssize_t
ert_readFd(int aFd,
           char *aBuf, size_t aLen, const struct Duration *aTimeout);

ERT_CHECKED ssize_t
ert_readFdRaw(int aFd,
              char *aBuf, size_t aLen, const struct Duration *aTimeout);

/* -------------------------------------------------------------------------- */
ERT_CHECKED ssize_t
ert_writeFdDeadline(
    int aFd,
    const char *aBuf, size_t aLen, struct Ert_Deadline *aDeadline);

ERT_CHECKED ssize_t
ert_readFdDeadline(
    int aFd,
    char *aBuf, size_t aLen, struct Ert_Deadline *aDeadline);

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
ert_lockFd(int aFd, struct Ert_LockType aLockType);

ERT_CHECKED int
ert_unlockFd(int aFd);

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
ert_lockFdRegion(
    int aFd, struct Ert_LockType aLockType, off_t aPos, off_t aLen);

ERT_CHECKED int
ert_unlockFdRegion(
    int aFd, off_t aPos, off_t aLen);

struct Ert_LockType
ert_ownFdRegionLocked(
    int aFd, off_t aPos, off_t aLen);

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
ert_waitFdWriteReady(int aFd, const struct Duration *aTimeout);

ERT_CHECKED int
ert_waitFdReadReady(int aFd, const struct Duration *aTimeout);

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
ert_waitFdWriteReadyDeadline(int aFd, struct Ert_Deadline *aDeadline);

ERT_CHECKED int
ert_waitFdReadReadyDeadline(int aFd, struct Ert_Deadline *aDeadline);

/* -------------------------------------------------------------------------- */

ERT_END_C_SCOPE;

#endif /* ERT_FD_H */
