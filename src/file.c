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
#include "ert/fdset.h"
#include "ert/process.h"
#include "ert/thread.h"
#include "ert/timekeeping.h"
#include "ert/socketpair.h"

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/stat.h>


/* -------------------------------------------------------------------------- */
#ifdef __linux__
#ifndef O_TMPFILE
#define O_TMPFILE 020000000
#endif
#endif

/* -------------------------------------------------------------------------- */
static struct
{
    LIST_HEAD(, File) mHead;
    pthread_mutex_t   mMutex;
}
fileList_ =
{
    .mHead  = LIST_HEAD_INITIALIZER(File),
    .mMutex = PTHREAD_MUTEX_INITIALIZER,
};

THREAD_FORK_SENTRY(
    lockMutex(&fileList_.mMutex),
    unlockMutex(&fileList_.mMutex));

/* -------------------------------------------------------------------------- */
int
createFile(struct File *self, int aFd)
{
    int rc = -1;

    self->mFd = aFd;

    /* If the file descriptor is invalid, take care to have preserved
     * errno so that the caller can rely on interpreting errno to
     * discover why the file descriptor is invalid. */

    int err = errno;
    ERROR_IF(
        (-1 == aFd),
        {
            errno = err;
        });

    pthread_mutex_t *lock = lockMutex(&fileList_.mMutex);
    {
        LIST_INSERT_HEAD(&fileList_.mHead, self, mList);
    }
    lock = unlockMutex(lock);

    rc = 0;

Finally:

    FINALLY
    ({
        if (rc && -1 != self->mFd)
            self->mFd = ert_closeFd(self->mFd);
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
struct TemporaryFileName_
{
    char mName[sizeof("0123456789012345")];
};

static void
temporaryFileName_(struct TemporaryFileName_ *self, uint32_t *aRandom)
{
    static const char hexDigits[16] = "0123456789abcdef";

    char *bp = self->mName;
    char *sp = bp + sizeof(self->mName) / 4 * 4;
    char *ep = bp + sizeof(self->mName);

    while (1)
    {
        /* LCG(2^32, 69069, 0, 1)
         * http://mathforum.org/kb/message.jspa?messageID=1608043 */

        *aRandom = *aRandom * 69069 + 1;

        uint32_t rnd = *aRandom;

        if (bp == sp)
        {
            while (bp != ep)
            {
                *bp++ = hexDigits[rnd % sizeof(hexDigits)]; rnd >>= 8;
            }

            *--bp = 0;

            break;
        }

        bp[0] = hexDigits[rnd % sizeof(hexDigits)]; rnd >>= 8;
        bp[1] = hexDigits[rnd % sizeof(hexDigits)]; rnd >>= 8;
        bp[2] = hexDigits[rnd % sizeof(hexDigits)]; rnd >>= 8;
        bp[3] = hexDigits[rnd % sizeof(hexDigits)]; rnd >>= 8;

        bp += 4;
    }
}

static ERT_CHECKED int
temporaryFileCreate_(const char *aDirName)
{
    int rc = -1;

    int dirFd;
    ERROR_IF(
        (dirFd = ert_openFd(aDirName, O_RDONLY | O_CLOEXEC, 0),
         -1 == dirFd));

    uint32_t rnd =
        ownProcessId().mPid ^ MSECS(monotonicTime().monotonic).ms;

    struct TemporaryFileName_ fileName;

    int fd;

    do
    {
        temporaryFileName_(&fileName, &rnd);

        ERROR_IF(
            (fd = openat(dirFd,
                         fileName.mName,
                         O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0),
             -1 == fd && EEXIST != errno));

    } while (-1 == fd);

    /* A race here is unavoidable because the creation of the file
     * and unlinking of the newly created file must be performed
     * in separate steps. */

    TEST_RACE
    ({
        ERROR_IF(
            unlinkat(dirFd, fileName.mName, 0) && ENOENT == errno);
    });

    rc = fd;

Finally:

    FINALLY
    ({
        dirFd = ert_closeFd(dirFd);
    });

    return rc;
}

struct TemporaryFileProcess_
{
    int                mFd;
    int                mErr;
    const char        *mDirName;
    struct SocketPair  mSocketPair_;
    struct SocketPair *mSocketPair;
    struct Thread      mThread_;
    struct Thread     *mThread;
};

static ERT_CHECKED struct TemporaryFileProcess_ *
closeTemporaryFileProcess_(struct TemporaryFileProcess_ *self)
{
    if (self)
    {
        self->mSocketPair = closeSocketPair(self->mSocketPair);
        self->mThread     = closeThread(self->mThread);
    }

    return 0;
}

static ERT_CHECKED int
createTemporaryFileProcess_(struct TemporaryFileProcess_ *self,
                            const char                   *aDirName)
{
    self->mFd         = -1;
    self->mErr        = 0;
    self->mDirName    = aDirName;
    self->mSocketPair = 0;
    self->mThread     = 0;

    return 0;
}

static ERT_CHECKED int
recvTemporaryFileProcessFd_(struct TemporaryFileProcess_ *self)
{
    int rc = -1;

    ssize_t rdlen;
    ERROR_IF(
        (rdlen = recvUnixSocket(
            self->mSocketPair->mParentSocket,
            (void *) &self->mErr,
            sizeof(self->mErr)),
         -1 == rdlen ||
         (errno = 0, sizeof(self->mErr) != rdlen)));

    ERROR_IF(
        self->mErr,
        {
            errno = self->mErr;
        });

    int tmpFd;
    ERROR_IF(
        (tmpFd = recvUnixSocketFd(self->mSocketPair->mParentSocket, O_CLOEXEC),
         -1 == tmpFd));

    self->mFd = tmpFd;

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

static ERT_CHECKED int
waitTemporaryFileProcessSocket_(struct TemporaryFileProcess_ *self)
{
    int rc = -1;

    ERROR_IF(
        joinThread(self->mThread));

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

static ERT_CHECKED int
prepareTemporaryFileProcessSocket_(struct TemporaryFileProcess_ *self,
                                   const struct PreForkProcess  *aFork)
{
    int rc = -1;

    ERROR_IF(
        createSocketPair(&self->mSocketPair_, O_CLOEXEC));
    self->mSocketPair = &self->mSocketPair_;

    ERROR_UNLESS(
        self->mThread = createThread(
            &self->mThread_,
            0,
            0,
            ThreadMethod(self, recvTemporaryFileProcessFd_)));

    ERROR_IF(
        ert_insertFdSetFile(
            aFork->mWhitelistFds,
            self->mSocketPair->mParentSocket->mSocket->mFile));

    ERROR_IF(
        ert_insertFdSetFile(
            aFork->mWhitelistFds,
            self->mSocketPair->mChildSocket->mSocket->mFile));

    rc = 0;

Finally:

    FINALLY
    ({
        if (rc)
            self = closeTemporaryFileProcess_(self);
    });

    return rc;
}

static ERT_CHECKED int
sendTemporaryFileProcessFd_(struct TemporaryFileProcess_ *self)
{
    int rc = -1;

    int fd;
    {
        struct ThreadSigMask  sigMask_;
        struct ThreadSigMask *sigMask = 0;

        sigMask = pushThreadSigMask(&sigMask_, ThreadSigMaskBlock, 0);

        fd  = temporaryFileCreate_(self->mDirName);

        sigMask = popThreadSigMask(sigMask);
    }

    self->mErr = -1 != fd ? 0 : (errno ? errno : EIO);

    ssize_t wrlen;
    ERROR_IF(
        (wrlen = sendUnixSocket(
            self->mSocketPair->mChildSocket,
            (void *) &self->mErr, sizeof(self->mErr)),
         -1 == wrlen || (errno = 0, sizeof(self->mErr) != wrlen)));

    if ( ! self->mErr)
        ERROR_IF(
            sendUnixSocketFd(
                self->mSocketPair->mChildSocket, fd));

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

static ERT_CHECKED int
temporaryFile_(const char *aDirName)
{
    int rc = -1;

    int tmpFd = -1;

    struct Pid tempPid = Pid(-1);

    /* Because the of the inherent race in creating an anonymous
     * temporary file, try to minimise the chance of the littering
     * the file system with the named temporary file by:
     *
     * o Blocking signal delivery in the thread creating the file
     * o Running that thread in a separate process
     * o Placing that process in a separate session and process group
     */

    struct TemporaryFileProcess_  temporaryFileProcess_;
    struct TemporaryFileProcess_ *temporaryFileProcess;

    ERROR_IF(
        createTemporaryFileProcess_(&temporaryFileProcess_, aDirName));
    temporaryFileProcess = &temporaryFileProcess_;

    ERROR_IF(
        (tempPid = forkProcessChild(
            ForkProcessSetSessionLeader,
            Pgid(0),
            PreForkProcessMethod(
                temporaryFileProcess,
                LAMBDA(
                    int, (struct TemporaryFileProcess_ *self,
                          const struct PreForkProcess  *aFork),
                    {
                        return prepareTemporaryFileProcessSocket_(self, aFork);
                    })),
            PostForkChildProcessMethod(
                temporaryFileProcess,
                LAMBDA(
                    int, (struct TemporaryFileProcess_ *self),
                    {
                        closeSocketPairParent(self->mSocketPair);

                        return sendTemporaryFileProcessFd_(self);
                    })),
            PostForkParentProcessMethod(
                temporaryFileProcess,
                LAMBDA(
                    int, (struct TemporaryFileProcess_ *self,
                          struct Pid                    aChildPid),
                    {
                        closeSocketPairChild(self->mSocketPair);

                        return waitTemporaryFileProcessSocket_(
                            temporaryFileProcess);
                    })),
            ForkProcessMethodNil()),
         -1 == tempPid.mPid));

    int status;
    ERROR_IF(
        reapProcessChild(tempPid, &status));

    struct ExitCode exitCode =
        extractProcessExitStatus(status, tempPid);

    ERROR_IF(
        exitCode.mStatus,
        {
            errno = ECHILD;
        });

    int retFd = temporaryFileProcess->mFd;

    ensure(-1 != retFd);

    tmpFd = -1;
    rc    = retFd;

Finally:

    FINALLY
    ({
        temporaryFileProcess = closeTemporaryFileProcess_(temporaryFileProcess);

        if (-1 != tmpFd)
            tmpFd = ert_closeFd(tmpFd);
    });

    return rc;
}

int
temporaryFile(struct File *self)
{
    int rc = -1;

    const char *tmpDir = getenv("TMPDIR");

#ifdef P_tmpdir
    if ( ! tmpDir)
         tmpDir = P_tmpdir;
#endif

    int fd;

    do
    {

#ifdef __linux__
        /* From https://lwn.net/Articles/619146/ for circa Linux 3.18:
         *
         * o O_RDWR or O_WRONLY is required otherwise O_TMPFILE will fail.
         * o O_TMPFILE fails with openat()
         *
         * The above is only of passing interest for this use case. */

        if ( ! testAction(TestLevelRace))
        {
            ERROR_IF(
                (fd = ert_openFd(tmpDir,
                                 O_TMPFILE | O_RDWR | O_DIRECTORY | O_CLOEXEC,
                                 S_IWUSR | S_IRUSR),
                 -1 == fd && EISDIR != errno && EOPNOTSUPP != errno));

            if (-1 != fd)
                break;
        }
#endif

        ERROR_IF(
            (fd = temporaryFile_(tmpDir),
             -1 == fd));

    } while (0);

    ERROR_IF(
        createFile(self, fd));

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
detachFile(struct File *self)
{
    int rc = -1;

    if (self)
    {
        ERROR_IF(
            -1 == self->mFd);

        pthread_mutex_t *lock = lockMutex(&fileList_.mMutex);
        {
            LIST_REMOVE(self, mList);
        }
        lock = unlockMutex(lock);

        self->mFd = -1;
    }

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
struct File *
closeFile(struct File *self)
{
    if (self && -1 != self->mFd)
    {
        pthread_mutex_t *lock = lockMutex(&fileList_.mMutex);
        {
            LIST_REMOVE(self, mList);
        }
        lock = unlockMutex(lock);

        self->mFd = ert_closeFd(self->mFd);
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
bool
ownFileValid(const struct File *self)
{
    return self && -1 != self->mFd;
}

/* -------------------------------------------------------------------------- */
void
walkFileList(struct FileVisitor aVisitor)
{
    pthread_mutex_t *lock = lockMutex(&fileList_.mMutex);
    {
        const struct File *filePtr;

        LIST_FOREACH(filePtr, &fileList_.mHead, mList)
        {
            if (callFileVisitor(aVisitor, filePtr))
                break;
        }
    }
    lock = unlockMutex(lock);
}

/* -------------------------------------------------------------------------- */
int
duplicateFile(struct File *self, const struct File *aFile)
{
    int rc = -1;

    ERROR_IF(
        createFile(self, ert_duplicateFd(aFile->mFd, -1)));

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
nonBlockingFile(struct File *self, unsigned aNonBlocking)
{
    return ert_nonBlockingFd(self->mFd, aNonBlocking);
}

/* -------------------------------------------------------------------------- */
int
ownFileNonBlocking(const struct File *self)
{
    return ert_ownFdNonBlocking(self->mFd);
}

/* -------------------------------------------------------------------------- */
int
closeFileOnExec(struct File *self, unsigned aCloseOnExec)
{
    return ert_closeFdOnExec(self->mFd, aCloseOnExec);
}

/* -------------------------------------------------------------------------- */
int
ownFileCloseOnExec(const struct File *self)
{
    return ert_ownFdCloseOnExec(self->mFd);
}

/* -------------------------------------------------------------------------- */
int
lockFile(struct File *self, struct Ert_LockType ert_aLockType)
{
    return ert_lockFd(self->mFd, ert_aLockType);
}

/* -------------------------------------------------------------------------- */
int
unlockFile(struct File *self)
{
    return ert_unlockFd(self->mFd);
}

/* -------------------------------------------------------------------------- */
int
lockFileRegion(
    struct File *self,
    struct Ert_LockType ert_aLockType, off_t aPos, off_t aLen)
{
    return ert_lockFdRegion(self->mFd, ert_aLockType, aPos, aLen);
}

/* -------------------------------------------------------------------------- */
int
unlockFileRegion(struct File *self, off_t aPos, off_t aLen)
{
    return ert_unlockFdRegion(self->mFd, aPos, aLen);
}

/* -------------------------------------------------------------------------- */
struct Ert_LockType
ownFileRegionLocked(const struct File *self, off_t aPos, off_t aLen)
{
    return ert_ownFdRegionLocked(self->mFd, aPos, aLen);
}

/* -------------------------------------------------------------------------- */
ssize_t
writeFile(struct File *self,
          const char *aBuf, size_t aLen, const struct Duration *aTimeout)
{
    return ert_writeFd(self->mFd, aBuf, aLen, aTimeout);
}

/* -------------------------------------------------------------------------- */
ssize_t
readFile(struct File *self,
         char *aBuf, size_t aLen, const struct Duration *aTimeout)
{
    return ert_readFd(self->mFd, aBuf, aLen, aTimeout);
}

/* -------------------------------------------------------------------------- */
ssize_t
writeFileDeadline(struct File *self,
                  const char *aBuf, size_t aLen,
                  struct Ert_Deadline *aDeadline)
{
    return ert_writeFdDeadline(self->mFd, aBuf, aLen, aDeadline);
}

/* -------------------------------------------------------------------------- */
ssize_t
readFileDeadline(struct File *self,
                 char *aBuf, size_t aLen, struct Ert_Deadline *aDeadline)
{
    return ert_readFdDeadline(self->mFd, aBuf, aLen, aDeadline);
}

/* -------------------------------------------------------------------------- */
off_t
lseekFile(struct File *self, off_t aOffset, struct Ert_WhenceType aWhenceType)
{
    return ert_lseekFd(self->mFd, aOffset, aWhenceType);
}

/* -------------------------------------------------------------------------- */
int
fstatFile(struct File *self, struct stat *aStat)
{
    return fstat(self->mFd, aStat);
}

/* -------------------------------------------------------------------------- */
int
fcntlFileGetFlags(struct File *self)
{
    return fcntl(self->mFd, F_GETFL);
}

/* -------------------------------------------------------------------------- */
int
ftruncateFile(struct File *self, off_t aLength)
{
    return ftruncate(self->mFd, aLength);
}

/* -------------------------------------------------------------------------- */
int
waitFileWriteReady(const struct File     *self,
                   const struct Duration *aTimeout)
{
    return ert_waitFdWriteReady(self->mFd, aTimeout);
}

/* -------------------------------------------------------------------------- */
int
waitFileReadReady(const struct File     *self,
                  const struct Duration *aTimeout)
{
    return ert_waitFdReadReady(self->mFd, aTimeout);
}

/* -------------------------------------------------------------------------- */
