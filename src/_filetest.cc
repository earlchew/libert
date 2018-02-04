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

#include "ert/file.h"
#include "ert/process.h"
#include "ert/fdset.h"
#include "ert/env.h"

#include "gtest/gtest.h"

static void
testTemporaryFile(const char *aDirPath)
{
    struct Ert_File  file_;
    struct Ert_File *file = 0;

    ASSERT_EQ(0, ert_temporaryFile(&file_, aDirPath));
    file = &file_;

    EXPECT_EQ(1, ert_writeFile(file, "A", 1, 0));

    EXPECT_EQ(0, ert_lseekFile(file, 0, Ert_WhenceTypeStart));

    char buf[1];
    EXPECT_EQ(1, ert_readFile(file, buf, 1, 0));

    EXPECT_EQ('A', buf[0]);

    file = ert_closeFile(file);
}

TEST(FileTest, TemporaryFile)
{
    /* Create the temporary file using the system defaults */

    testTemporaryFile(0);

    /* Create the temporary file in the current directory */

    testTemporaryFile(".");
}

struct Ert_LockType
checkLock(struct Ert_File *aFile)
{
    struct Ert_Pid checkPid =
        ert_forkProcessChild(
            Ert_ForkProcessInheritProcessGroup,
            Ert_Pgid(0),
            Ert_PreForkProcessMethod(
                aFile,
                ERT_LAMBDA(
                    int, (struct Ert_File                 *self,
                          const struct Ert_PreForkProcess *aPreFork),
                    {
                        return ert_insertFdSetFile(
                            aPreFork->mWhitelistFds, self);
                    })),
            Ert_PostForkChildProcessMethodNil(),
            Ert_PostForkParentProcessMethodNil(),
            Ert_ForkProcessMethod(
                aFile,
                ERT_LAMBDA(
                    int, (struct Ert_File *self),
                    {
                        struct Ert_LockType ert_lockType =
                            ert_ownFileRegionLocked(self, 0, 1);

                        int rc = 0;

                        if (Ert_LockTypeUnlocked.mType == ert_lockType.mType)
                            rc = 1;
                        else if (Ert_LockTypeRead.mType == ert_lockType.mType)
                            rc = 2;
                        else if (Ert_LockTypeWrite.mType == ert_lockType.mType)
                            rc = 3;

                        return rc;
                    })));

    if (-1 == checkPid.mPid)
        return Ert_LockTypeError;

    int status;
    if (ert_reapProcessChild(checkPid, &status))
        return Ert_LockTypeError;

    switch (ert_extractProcessExitStatus(status, checkPid).mStatus)
    {
    default:
        return Ert_LockTypeError;
    case 1:
        return Ert_LockTypeUnlocked;
    case 2:
        return Ert_LockTypeRead;
    case 3:
        return Ert_LockTypeWrite;
    }
}

TEST(FileTest, LockFileRegion)
{
    struct Ert_File  file_;
    struct Ert_File *file = 0;

    {
        unsigned optTest = gErtOptions_.mTest;

        gErtOptions_.mTest = Ert_TestLevelRace;

        ASSERT_EQ(0, ert_temporaryFile(&file_, 0));
        file = &file_;

        gErtOptions_.mTest = optTest;
    }

    // If a process holds a region lock, querying the lock state from
    // that process will always show the region as unlocked, but another
    // process will see the region as locked.

    EXPECT_EQ(
        Ert_LockTypeUnlocked.mType, ert_ownFileRegionLocked(file, 0, 0).mType);
    EXPECT_EQ(
        Ert_LockTypeUnlocked.mType, checkLock(file).mType);

    {
        EXPECT_EQ(0, ert_lockFileRegion(file, Ert_LockTypeWrite, 0, 0));

        EXPECT_EQ(
            Ert_LockTypeUnlocked.mType,
            ert_ownFileRegionLocked(file, 0, 0).mType);
        EXPECT_EQ(
            Ert_LockTypeWrite.mType, checkLock(file).mType);

        EXPECT_EQ(0, ert_unlockFileRegion(file, 0, 0));

        EXPECT_EQ(
            Ert_LockTypeUnlocked.mType,
            ert_ownFileRegionLocked(file, 0, 0).mType);
        EXPECT_EQ(
            Ert_LockTypeUnlocked.mType, checkLock(file).mType);
    }

    {
        EXPECT_EQ(0, ert_lockFileRegion(file, Ert_LockTypeRead, 0, 0));

        EXPECT_EQ(
            Ert_LockTypeUnlocked.mType,
            ert_ownFileRegionLocked(file, 0, 0).mType);
        EXPECT_EQ(
            Ert_LockTypeRead.mType, checkLock(file).mType);

        EXPECT_EQ(0, ert_unlockFileRegion(file, 0, 0));

        EXPECT_EQ(
            Ert_LockTypeUnlocked.mType,
            ert_ownFileRegionLocked(file, 0, 0).mType);
        EXPECT_EQ(
            Ert_LockTypeUnlocked.mType, checkLock(file).mType);
    }

    {
        EXPECT_EQ(0, ert_lockFileRegion(file, Ert_LockTypeWrite, 0, 0));

        EXPECT_EQ(
            Ert_LockTypeUnlocked.mType,
            ert_ownFileRegionLocked(file, 0, 0).mType);
        EXPECT_EQ(
            Ert_LockTypeWrite.mType, checkLock(file).mType);

        EXPECT_EQ(0, ert_lockFileRegion(file, Ert_LockTypeRead, 0, 0));

        EXPECT_EQ(
            Ert_LockTypeUnlocked.mType,
            ert_ownFileRegionLocked(file, 0, 0).mType);
        EXPECT_EQ(
            Ert_LockTypeRead.mType, checkLock(file).mType);

        EXPECT_EQ(0, ert_lockFileRegion(file, Ert_LockTypeWrite, 0, 0));

        EXPECT_EQ(
            Ert_LockTypeUnlocked.mType,
            ert_ownFileRegionLocked(file, 0, 0).mType);
        EXPECT_EQ(
            Ert_LockTypeWrite.mType, checkLock(file).mType);

        EXPECT_EQ(0, ert_unlockFileRegion(file, 0, 0));

        EXPECT_EQ(
            Ert_LockTypeUnlocked.mType,
            ert_ownFileRegionLocked(file, 0, 0).mType);
        EXPECT_EQ(
            Ert_LockTypeUnlocked.mType, checkLock(file).mType);
    }

    file = ert_closeFile(file);
}

#include "../googletest/src/gtest_main.cc"
