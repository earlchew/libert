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
    LIST_HEAD(, Ert_File) mHead;
    pthread_mutex_t       mMutex;
}
fileList_ =
{
    .mHead  = LIST_HEAD_INITIALIZER(Ert_File),
    .mMutex = PTHREAD_MUTEX_INITIALIZER,
};

ERT_THREAD_FORK_SENTRY(
    ert_lockMutex(&fileList_.mMutex),
    ert_unlockMutex(&fileList_.mMutex));

/* -------------------------------------------------------------------------- */
int
ert_createFile(
    struct Ert_File *self,
    int              aFd)
{
    int rc = -1;

    self->mFd = aFd;

    /* If the file descriptor is invalid, take care to have preserved
     * errno so that the caller can rely on interpreting errno to
     * discover why the file descriptor is invalid. */

    int err = errno;
    ERT_ERROR_IF(
        (-1 == aFd),
        {
            errno = err;
        });

    pthread_mutex_t *lock = ert_lockMutex(&fileList_.mMutex);
    {
        LIST_INSERT_HEAD(&fileList_.mHead, self, mList);
    }
    lock = ert_unlockMutex(lock);

    rc = 0;

Ert_Finally:

    ERT_FINALLY
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
    ERT_ERROR_IF(
        (dirFd = ert_openFd(aDirName, O_RDONLY | O_CLOEXEC, 0),
         -1 == dirFd));

    /* To be safe, ensure that the temporary directory is not world
     * writable, or else is either owned by the effective user or
     * is owned by root and has its sticky bit set. */

    struct stat dirStat;
    ERT_ERROR_IF(
        fstat(dirFd, &dirStat));

    if (dirStat.st_mode & S_IWOTH)
    {
        ERT_ERROR_UNLESS(
            0 == dirStat.st_uid && (dirStat.st_mode & S_ISVTX) ||
            geteuid() == dirStat.st_uid,
            {
                errno = EACCES;
            });
    }

    uint32_t rnd =
        ert_ownProcessId().mPid ^ ERT_MSECS(ert_monotonicTime().monotonic).ms;

    struct TemporaryFileName_ fileName;

    int fd;

    do
    {
        temporaryFileName_(&fileName, &rnd);

        ERT_ERROR_IF(
            (fd = openat(dirFd,
                         fileName.mName,
                         O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0),
             -1 == fd && EEXIST != errno));

    } while (-1 == fd);

    /* A race here is unavoidable because the creation of the file
     * and unlinking of the newly created file must be performed
     * in separate steps. */

    ERT_TEST_RACE
    ({
        ERT_ERROR_IF(
            unlinkat(dirFd, fileName.mName, 0) && ENOENT == errno);
    });

    rc = fd;

Ert_Finally:

    ERT_FINALLY
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
    struct Ert_SocketPair  mSocketPair_;
    struct Ert_SocketPair *mSocketPair;
    struct Ert_Thread      mThread_;
    struct Ert_Thread     *mThread;
};

static ERT_CHECKED struct TemporaryFileProcess_ *
closeTemporaryFileProcess_(
    struct TemporaryFileProcess_ *self)
{
    if (self)
    {
        self->mSocketPair = ert_closeSocketPair(self->mSocketPair);
        self->mThread     = ert_closeThread(self->mThread);
    }

    return 0;
}

static ERT_CHECKED int
createTemporaryFileProcess_(
    struct TemporaryFileProcess_ *self,
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
recvTemporaryFileProcessFd_(
    struct TemporaryFileProcess_ *self)
{
    int rc = -1;

    ssize_t rdlen;
    ERT_ERROR_IF(
        (rdlen = ert_recvUnixSocket(
            self->mSocketPair->mParentSocket,
            (void *) &self->mErr,
            sizeof(self->mErr)),
         -1 == rdlen ||
         (errno = 0, sizeof(self->mErr) != rdlen)));

    ERT_ERROR_IF(
        self->mErr,
        {
            errno = self->mErr;
        });

    int tmpFd;
    ERT_ERROR_IF(
        (tmpFd = ert_recvUnixSocketFd(
              self->mSocketPair->mParentSocket, O_CLOEXEC),
         -1 == tmpFd));

    self->mFd = tmpFd;

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

static ERT_CHECKED int
waitTemporaryFileProcessSocket_(
    struct TemporaryFileProcess_ *self)
{
    int rc = -1;

    ERT_ERROR_IF(
        ert_joinThread(self->mThread));

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

static ERT_CHECKED int
prepareTemporaryFileProcessSocket_(
    struct TemporaryFileProcess_    *self,
    const struct Ert_PreForkProcess *aFork)
{
    int rc = -1;

    ERT_ERROR_IF(
        ert_createSocketPair(&self->mSocketPair_, O_CLOEXEC));
    self->mSocketPair = &self->mSocketPair_;

    ERT_ERROR_UNLESS(
        self->mThread = ert_createThread(
            &self->mThread_,
            0,
            0,
            Ert_ThreadMethod(self, recvTemporaryFileProcessFd_)));

    ERT_ERROR_IF(
        ert_insertFdSetFile(
            aFork->mWhitelistFds,
            self->mSocketPair->mParentSocket->mSocket->mFile));

    ERT_ERROR_IF(
        ert_insertFdSetFile(
            aFork->mWhitelistFds,
            self->mSocketPair->mChildSocket->mSocket->mFile));

    rc = 0;

Ert_Finally:

    ERT_FINALLY
    ({
        if (rc)
            self = closeTemporaryFileProcess_(self);
    });

    return rc;
}

static ERT_CHECKED int
sendTemporaryFileProcessFd_(
    struct TemporaryFileProcess_ *self)
{
    int rc = -1;

    int fd;
    {
        struct Ert_ThreadSigMask  sigMask_;
        struct Ert_ThreadSigMask *sigMask = 0;

        sigMask = ert_pushThreadSigMask(&sigMask_, Ert_ThreadSigMaskBlock, 0);

        fd  = temporaryFileCreate_(self->mDirName);

        sigMask = ert_popThreadSigMask(sigMask);
    }

    self->mErr = -1 != fd ? 0 : (errno ? errno : EIO);

    ssize_t wrlen;
    ERT_ERROR_IF(
        (wrlen = ert_sendUnixSocket(
            self->mSocketPair->mChildSocket,
            (void *) &self->mErr, sizeof(self->mErr)),
         -1 == wrlen || (errno = 0, sizeof(self->mErr) != wrlen)));

    if ( ! self->mErr)
        ERT_ERROR_IF(
            ert_sendUnixSocketFd(
                self->mSocketPair->mChildSocket, fd));

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

static ERT_CHECKED int
temporaryFile_(const char *aDirName)
{
    int rc = -1;

    int tmpFd = -1;

    struct Ert_Pid tempPid = Ert_Pid(-1);

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

    ERT_ERROR_IF(
        createTemporaryFileProcess_(&temporaryFileProcess_, aDirName));
    temporaryFileProcess = &temporaryFileProcess_;

    ERT_ERROR_IF(
        (tempPid = ert_forkProcessChild(
            Ert_ForkProcessSetSessionLeader,
            Ert_Pgid(0),
            Ert_PreForkProcessMethod(
                temporaryFileProcess,
                ERT_LAMBDA(
                    int, (struct TemporaryFileProcess_ *self,
                          const struct Ert_PreForkProcess  *aFork),
                    {
                        return prepareTemporaryFileProcessSocket_(self, aFork);
                    })),
            Ert_PostForkChildProcessMethod(
                temporaryFileProcess,
                ERT_LAMBDA(
                    int, (struct TemporaryFileProcess_ *self),
                    {
                        ert_closeSocketPairParent(self->mSocketPair);

                        return sendTemporaryFileProcessFd_(self);
                    })),
            Ert_PostForkParentProcessMethod(
                temporaryFileProcess,
                ERT_LAMBDA(
                    int, (struct TemporaryFileProcess_ *self,
                          struct Ert_Pid                    aChildPid),
                    {
                        ert_closeSocketPairChild(self->mSocketPair);

                        return waitTemporaryFileProcessSocket_(
                            temporaryFileProcess);
                    })),
            Ert_ForkProcessMethodNil()),
         -1 == tempPid.mPid));

    int status;
    ERT_ERROR_IF(
        ert_reapProcessChild(tempPid, &status));

    struct Ert_ExitCode exitCode =
        ert_extractProcessExitStatus(status, tempPid);

    ERT_ERROR_IF(
        exitCode.mStatus,
        {
            errno = ECHILD;
        });

    int retFd = temporaryFileProcess->mFd;

    ert_ensure(-1 != retFd);

    tmpFd = -1;
    rc    = retFd;

Ert_Finally:

    ERT_FINALLY
    ({
        temporaryFileProcess = closeTemporaryFileProcess_(temporaryFileProcess);

        if (-1 != tmpFd)
            tmpFd = ert_closeFd(tmpFd);
    });

    return rc;
}

int
ert_temporaryFile(
    struct Ert_File *self,
    const char      *aDirPath)
{
    int rc = -1;

    const char *tmpDir = aDirPath ? aDirPath : getenv("TMPDIR");

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

        if ( ! ert_testAction(Ert_TestLevelRace))
        {
            ERT_ERROR_IF(
                (fd = ert_openFd(tmpDir,
                                 O_TMPFILE | O_RDWR | O_DIRECTORY | O_CLOEXEC,
                                 S_IWUSR | S_IRUSR),
                 -1 == fd && EISDIR != errno && EOPNOTSUPP != errno));

            if (-1 != fd)
                break;
        }
#endif

        ERT_ERROR_IF(
            (fd = temporaryFile_(tmpDir),
             -1 == fd));

    } while (0);

    ERT_ERROR_IF(
        ert_createFile(self, fd));

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_detachFile(
    struct Ert_File *self)
{
    int rc = -1;

    if (self)
    {
        ERT_ERROR_IF(
            -1 == self->mFd);

        pthread_mutex_t *lock = ert_lockMutex(&fileList_.mMutex);
        {
            LIST_REMOVE(self, mList);
        }
        lock = ert_unlockMutex(lock);

        self->mFd = -1;
    }

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
struct Ert_File *
ert_closeFile(
    struct Ert_File *self)
{
    if (self && -1 != self->mFd)
    {
        pthread_mutex_t *lock = ert_lockMutex(&fileList_.mMutex);
        {
            LIST_REMOVE(self, mList);
        }
        lock = ert_unlockMutex(lock);

        self->mFd = ert_closeFd(self->mFd);
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
bool
ert_ownFileValid(
    const struct Ert_File *self)
{
    return self && -1 != self->mFd;
}

/* -------------------------------------------------------------------------- */
void
ert_walkFileList(
    struct Ert_FileVisitor aVisitor)
{
    pthread_mutex_t *lock = ert_lockMutex(&fileList_.mMutex);
    {
        const struct Ert_File *filePtr;

        LIST_FOREACH(filePtr, &fileList_.mHead, mList)
        {
            if (ert_callFileVisitor(aVisitor, filePtr))
                break;
        }
    }
    lock = ert_unlockMutex(lock);
}

/* -------------------------------------------------------------------------- */
int
ert_duplicateFile(
    struct Ert_File       *self,
    const struct Ert_File *aFile)
{
    int rc = -1;

    ERT_ERROR_IF(
        ert_createFile(self, ert_duplicateFd(aFile->mFd, -1)));

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_nonBlockingFile(
    struct Ert_File *self,
    unsigned         aNonBlocking)
{
    return ert_nonBlockingFd(self->mFd, aNonBlocking);
}

/* -------------------------------------------------------------------------- */
int
ert_ownFileNonBlocking(
    const struct Ert_File *self)
{
    return ert_ownFdNonBlocking(self->mFd);
}

/* -------------------------------------------------------------------------- */
int
ert_closeFileOnExec(
    struct Ert_File *self,
    unsigned         aCloseOnExec)
{
    return ert_closeFdOnExec(self->mFd, aCloseOnExec);
}

/* -------------------------------------------------------------------------- */
int
ert_ownFileCloseOnExec(
    const struct Ert_File *self)
{
    return ert_ownFdCloseOnExec(self->mFd);
}

/* -------------------------------------------------------------------------- */
int
ert_lockFile(
    struct Ert_File    *self,
    struct Ert_LockType aLockType)
{
    return ert_lockFd(self->mFd, aLockType);
}

/* -------------------------------------------------------------------------- */
int
ert_unlockFile(
    struct Ert_File *self)
{
    return ert_unlockFd(self->mFd);
}

/* -------------------------------------------------------------------------- */
int
ert_lockFileRegion(
    struct Ert_File *self,
    struct Ert_LockType aLockType, off_t aPos, off_t aLen)
{
    return ert_lockFdRegion(self->mFd, aLockType, aPos, aLen);
}

/* -------------------------------------------------------------------------- */
int
ert_unlockFileRegion(
    struct Ert_File *self,
    off_t aPos, off_t aLen)
{
    return ert_unlockFdRegion(self->mFd, aPos, aLen);
}

/* -------------------------------------------------------------------------- */
struct Ert_LockType
ert_ownFileRegionLocked(
    const struct Ert_File *self,
    off_t aPos, off_t aLen)
{
    return ert_ownFdRegionLocked(self->mFd, aPos, aLen);
}

/* -------------------------------------------------------------------------- */
ssize_t
ert_writeFile(
    struct Ert_File *self,
    const char *aBuf, size_t aLen, const struct Ert_Duration *aTimeout)
{
    return ert_writeFd(self->mFd, aBuf, aLen, aTimeout);
}

/* -------------------------------------------------------------------------- */
ssize_t
ert_readFile(
    struct Ert_File *self,
    char *aBuf, size_t aLen, const struct Ert_Duration *aTimeout)
{
    return ert_readFd(self->mFd, aBuf, aLen, aTimeout);
}

/* -------------------------------------------------------------------------- */
ssize_t
ert_writeFileDeadline(
    struct Ert_File *self,
    const char *aBuf, size_t aLen, struct Ert_Deadline *aDeadline)
{
    return ert_writeFdDeadline(self->mFd, aBuf, aLen, aDeadline);
}

/* -------------------------------------------------------------------------- */
ssize_t
ert_readFileDeadline(
    struct Ert_File *self,
    char *aBuf, size_t aLen, struct Ert_Deadline *aDeadline)
{
    return ert_readFdDeadline(self->mFd, aBuf, aLen, aDeadline);
}

/* -------------------------------------------------------------------------- */
off_t
ert_lseekFile(
    struct Ert_File *self,
    off_t aOffset, struct Ert_WhenceType aWhenceType)
{
    return ert_lseekFd(self->mFd, aOffset, aWhenceType);
}

/* -------------------------------------------------------------------------- */
int
ert_fstatFile(
    struct Ert_File *self,
    struct stat     *aStat)
{
    return fstat(self->mFd, aStat);
}

/* -------------------------------------------------------------------------- */
int
ert_fcntlFileGetFlags(struct Ert_File *self)
{
    return fcntl(self->mFd, F_GETFL);
}

/* -------------------------------------------------------------------------- */
int
ert_ftruncateFile(
    struct Ert_File *self,
    off_t            aLength)
{
    return ftruncate(self->mFd, aLength);
}

/* -------------------------------------------------------------------------- */
int
ert_waitFileWriteReady(
    const struct Ert_File     *self,
    const struct Ert_Duration *aTimeout)
{
    return ert_waitFdWriteReady(self->mFd, aTimeout);
}

/* -------------------------------------------------------------------------- */
int
ert_waitFileReadReady(
    const struct Ert_File     *self,
    const struct Ert_Duration *aTimeout)
{
    return ert_waitFdReadReady(self->mFd, aTimeout);
}

/* -------------------------------------------------------------------------- */
