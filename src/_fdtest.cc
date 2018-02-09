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

#include "ert/fd.h"
#include "ert/pipe.h"
#include "ert/fdset.h"
#include "ert/pid.h"
#include "ert/process.h"

#include "gtest/gtest.h"

#include <limits.h>
#include <fcntl.h>

#include <sys/resource.h>

TEST(FdTest, ReadFully)
{
    {
        char *buf = 0;

        EXPECT_EQ(-1, ert_readFdFully(-1, &buf, 0));
        EXPECT_EQ(0,  buf);
    }

    {
        char  buf_;
        char *buf = &buf_;

        struct Ert_Pipe  pipe_;
        struct Ert_Pipe *pipe = 0;

        EXPECT_EQ(0, ert_createPipe(&pipe_, 0));
        pipe = &pipe_;

        ert_closePipeWriter(pipe);

        EXPECT_EQ(0, ert_readFdFully(pipe->mRdFile->mFd, &buf, 0));
        EXPECT_EQ(0, buf);

        pipe = ert_closePipe(pipe);
    }

    {
        char  buf_;
        char *buf = &buf_;

        struct Ert_Pipe  pipe_;
        struct Ert_Pipe *pipe = 0;

        EXPECT_EQ(0, ert_createPipe(&pipe_, 0));
        pipe = &pipe_;

        EXPECT_EQ(1, ert_writeFd(pipe->mWrFile->mFd, "1", 1, 0));
        ert_closePipeWriter(pipe);

        EXPECT_EQ(1, ert_readFdFully(pipe->mRdFile->mFd, &buf, 0));
        EXPECT_EQ(0, strncmp("1", buf, 1));

        free(buf);

        pipe = ert_closePipe(pipe);
    }

    {
        char  buf_;
        char *buf = &buf_;

        struct Ert_Pipe  pipe_;
        struct Ert_Pipe *pipe = 0;

        EXPECT_EQ(0, ert_createPipe(&pipe_, 0));
        pipe = &pipe_;

        EXPECT_EQ(4, ert_writeFd(pipe->mWrFile->mFd, "1234", 4, 0));
        ert_closePipeWriter(pipe);

        EXPECT_EQ(4, ert_readFdFully(pipe->mRdFile->mFd, &buf, 0));
        EXPECT_EQ(0, strncmp("1234", buf, 4));

        free(buf);

        pipe = ert_closePipe(pipe);
    }

    {
        char  buf_;
        char *buf = &buf_;

        struct Ert_Pipe  pipe_;
        struct Ert_Pipe *pipe = 0;

        EXPECT_EQ(0, ert_createPipe(&pipe_, 0));
        pipe = &pipe_;

        EXPECT_EQ(5, ert_writeFd(pipe->mWrFile->mFd, "12345", 5, 0));
        ert_closePipeWriter(pipe);

        EXPECT_EQ(5, ert_readFdFully(pipe->mRdFile->mFd, &buf, 0));
        EXPECT_EQ(0, strncmp("12345", buf, 5));

        free(buf);

        pipe = ert_closePipe(pipe);
    }
}

TEST(FdTest, CloseExceptWhiteList)
{
    int pipefd[4];

    EXPECT_EQ(0, pipe(pipefd + 0));
    EXPECT_EQ(0, pipe(pipefd + 2));

    struct Ert_FdSet  fdset_;
    struct Ert_FdSet *fdset = 0;

    struct rlimit fdLimit;
    EXPECT_EQ(0, getrlimit(RLIMIT_NOFILE, &fdLimit));

    EXPECT_EQ(0, ert_createFdSet(&fdset_));
    fdset = &fdset_;

    EXPECT_EQ(0,
              ert_insertFdSetRange(
                  fdset, Ert_FdRange(STDERR_FILENO,STDERR_FILENO)));
    EXPECT_EQ(0,
              ert_insertFdSetRange(
                  fdset, Ert_FdRange(pipefd[1], pipefd[1])));
    EXPECT_EQ(0,
              ert_insertFdSetRange(
                  fdset, Ert_FdRange(pipefd[2], pipefd[2])));

    /* Half the time, include a range that exceeds the number of
     * available file descriptors. */

    if ((getpid() / 2) & 1)
        EXPECT_EQ(0,
                  ert_insertFdSetRange(
                      fdset,
                      Ert_FdRange(fdLimit.rlim_cur, INT_MAX)));

    pid_t childpid = fork();

    EXPECT_NE(-1, childpid);

    if ( ! childpid)
    {
        int rc = -1;

        do
        {
            if (ert_closeFdExceptWhiteList(fdset))
            {
                fprintf(stderr, "%u\n", __LINE__);
                break;
            }

            if (ert_ownFdValid(pipefd[0]))
            {
                fprintf(stderr, "%u\n", __LINE__);
                break;
            }

            if ( ! ert_ownFdValid(pipefd[1]))
            {
                fprintf(stderr, "%u\n", __LINE__);
                break;
            }

            if ( ! ert_ownFdValid(pipefd[2]))
            {
                fprintf(stderr, "%u\n", __LINE__);
                break;
            }

            if (ert_ownFdValid(pipefd[3]))
            {
                fprintf(stderr, "%u\n", __LINE__);
                break;
            }

            unsigned numFds = 0;
            for (unsigned fd = 0; fd < fdLimit.rlim_cur; ++fd)
            {
                if (ert_ownFdValid(fd))
                    ++numFds;
            }

            if (3 != numFds)
            {
                fprintf(stderr, "%u\n", __LINE__);
                break;
            }

            rc = 0;

        } while (0);

        if (rc)
        {
            execl("/bin/false", "false", (char *) 0);
            _exit(EXIT_FAILURE);
        }

        execl("/bin/true", "true", (char *) 0);
        _exit(EXIT_FAILURE);
    }

    int status;
    EXPECT_EQ(0, ert_reapProcessChild(Ert_Pid(childpid), &status));
    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_EQ(EXIT_SUCCESS, WEXITSTATUS(status));

    fdset = ert_closeFdSet(fdset);

    while (ert_closeFd(pipefd[0])) break;
    while (ert_closeFd(pipefd[1])) break;
}

TEST(FdTest, CloseOnlyBlackList)
{
    int pipefd[2];

    EXPECT_EQ(0, pipe(pipefd));

    struct Ert_FdSet  fdset_;
    struct Ert_FdSet *fdset = 0;

    struct rlimit fdLimit;
    EXPECT_EQ(0, getrlimit(RLIMIT_NOFILE, &fdLimit));

    EXPECT_EQ(0, ert_createFdSet(&fdset_));
    fdset = &fdset_;

    EXPECT_EQ(0,
              ert_insertFdSetRange(
                  fdset, Ert_FdRange(STDIN_FILENO,STDIN_FILENO)));
    EXPECT_EQ(0,
              ert_insertFdSetRange(
                  fdset, Ert_FdRange(STDOUT_FILENO,STDOUT_FILENO)));
    EXPECT_EQ(0,
              ert_insertFdSetRange(
                  fdset, Ert_FdRange(pipefd[0], pipefd[0])));

    /* Half the time, include a range that exceeds the number of
     * available file descriptors. */

    if ((getpid() / 2) & 1)
        EXPECT_EQ(0,
                  ert_insertFdSetRange(
                      fdset,
                      Ert_FdRange(fdLimit.rlim_cur, INT_MAX)));

    pid_t childpid = fork();

    EXPECT_NE(-1, childpid);

    if ( ! childpid)
    {
        int rc = -1;

        do
        {
            unsigned openFds = 0;
            for (unsigned fd = 0; fd < fdLimit.rlim_cur; ++fd)
            {
                if (ert_ownFdValid(fd))
                    ++openFds;
            }

            if (ert_closeFdOnlyBlackList(fdset))
            {
                fprintf(stderr, "%u\n", __LINE__);
                break;
            }

            if ( ! ert_ownFdValid(pipefd[1]))
            {
                fprintf(stderr, "%u\n", __LINE__);
                break;
            }

            if (ert_ownFdValid(pipefd[0]))
            {
                fprintf(stderr, "%u\n", __LINE__);
                break;
            }

            unsigned numFds = 0;
            for (unsigned fd = 0; fd < fdLimit.rlim_cur; ++fd)
            {
                if (ert_ownFdValid(fd))
                    ++numFds;
            }

            if (numFds + 3 != openFds)
            {
                fprintf(stderr, "%u %u %u\n", __LINE__, numFds, openFds);
                break;
            }

            rc = 0;

        } while (0);

        if (rc)
        {
            execl("/bin/false", "false", (char *) 0);
            _exit(EXIT_FAILURE);
        }

        execl("/bin/true", "true", (char *) 0);
        _exit(EXIT_FAILURE);
    }

    int status;
    EXPECT_EQ(0, ert_reapProcessChild(Ert_Pid(childpid), &status));
    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_EQ(EXIT_SUCCESS, WEXITSTATUS(status));

    fdset = ert_closeFdSet(fdset);

    while (ert_closeFd(pipefd[0])) break;
    while (ert_closeFd(pipefd[1])) break;
}

static int
checkChildCloseOnExec(int aFd, int aErrFd)
{
    int rc = -1;

    do
    {
        if (1 != ert_ownFdCloseOnExec(aFd))
        {
            dprintf(aErrFd, "@@@ %u\n", __LINE__);
            break;
        }

        pid_t childpid = fork();

        if (-1 == childpid)
            break;

        if ( ! childpid)
        {
            do
            {
                int fd = ert_duplicateFd(aFd, -1);
                if (-1 == fd)
                {
                    dprintf(aErrFd, "@@@ %u\n", __LINE__);
                    break;
                }

                int nullfd = ert_openFd("/dev/null", O_RDWR, 0);
                if (-1 == nullfd)
                {
                    dprintf(aErrFd, "@@@ %u\n", __LINE__);
                    break;
                }

                if (-1 == dup2(aErrFd, STDERR_FILENO))
                {
                    dprintf(aErrFd, "@@@ %u\n", __LINE__);
                    break;
                }

                if (-1 == dup2(nullfd, STDIN_FILENO))
                {
                    dprintf(aErrFd, "@@@ %u\n", __LINE__);
                    break;
                }

                if (-1 == dup2(nullfd, STDOUT_FILENO))
                {
                    dprintf(aErrFd, "@@@ %u\n", __LINE__);
                    break;
                }

                if (-1 == ert_duplicateFd(fd, 3))
                {
                    dprintf(STDERR_FILENO, "@@@ %u\n", __LINE__);
                    break;
                }

                execlp("sh",
                       "/bin/sh",
                       "-c",
                       "set -e ; "
                       /*"set -x ; " */
                       /*"ls -l /proc/$$/fd >&2 ; "*/
                       "exec 2>/dev/null ; ! ( exec >&3 )",
                       (const char *) 0);

            } while (0);

            execl("/bin/false", "false", (char *) 0);
            _exit(EXIT_FAILURE);
        }

        int status;
        if (ert_reapProcessChild(Ert_Pid(childpid), &status))
        {
            dprintf(aErrFd, "@@@ %u\n", __LINE__);
            break;
        }
        if ( ! WIFEXITED(status))
        {
            dprintf(aErrFd, "@@@ %u\n", __LINE__);
            break;
        }
        if (EXIT_SUCCESS != WEXITSTATUS(status))
        {
            dprintf(aErrFd, "@@@ %u\n", __LINE__);
            break;
        }

        rc = 0;

    } while (0);

    return rc;
}

static int
fillStdFds_(int aErrFd)
{
    int rc = -1;

    do
    {
        int pipefd[2];

        if (pipe(pipefd))
        {
            dprintf(aErrFd, "@@@ %u\n", __LINE__);
            break;
        }

        if (-1 == dup2(pipefd[0], STDIN_FILENO))
        {
            dprintf(aErrFd, "@@@ %u\n", __LINE__);
            break;
        }

        if (STDIN_FILENO == pipefd[0])
            while (ert_closeFd(pipefd[0])) break;

        if (-1 == dup2(pipefd[1], STDOUT_FILENO))
        {
            dprintf(aErrFd, "@@@ %u\n", __LINE__);
            break;
        }

        if (STDOUT_FILENO == pipefd[1])
            while (ert_closeFd(pipefd[1])) break;

        if (-1 == dup2(STDOUT_FILENO, STDERR_FILENO))
        {
            dprintf(aErrFd, "@@@ %u\n", __LINE__);
            break;
        }

        rc = 0;

    } while (0);

    return rc;
}

TEST(FdTest, OpenStdFds)
{
    pid_t childpid = fork();

    EXPECT_NE(-1, childpid);

    if ( ! childpid)
    {
        int rc = -1;

        // Use a child process to verify that openStdFds() will create
        // file descriptors to fill vacant stdin, stdout and stderr
        // file descriptors, and ensure that the filled descriptors
        // will be closed when exec() is called.

        do
        {
            char buf[1];

            int errfd = dup(STDERR_FILENO);
            if (-1 == errfd)
            {
                dprintf(STDERR_FILENO, "@@@ %u\n", __LINE__);
                break;
            }

            if (SIG_ERR == signal(SIGPIPE, SIG_IGN))
            {
                dprintf(errfd, "@@@ %u\n", __LINE__);
                break;
            }

            // Test that if stdin, stdout and stderr are already
            // opened, that openStdFds() does not overlay these existing
            // file descriptors.

            if (fillStdFds_(errfd))
            {
                dprintf(errfd, "@@@ %u\n", __LINE__);
                break;
            }

            if (ert_ownFdCloseOnExec(STDIN_FILENO))
            {
                dprintf(errfd, "@@@ %u\n", __LINE__);
                break;
            }

            if (ert_ownFdCloseOnExec(STDOUT_FILENO))
            {
                dprintf(errfd, "@@@ %u\n", __LINE__);
                break;
            }

            if (ert_ownFdCloseOnExec(STDERR_FILENO))
            {
                dprintf(errfd, "@@@ %u\n", __LINE__);
                break;
            }

            // Test that if all stdin, stdout and stderr are closed, that
            // openStdFds() overlays these close file dscriptors. Furthermore
            // verify that the overlaid stdin responds with eof on read,
            // and the overlaid stdout and stdout respond with EPIPE
            // on write.

            while (ert_closeFd(STDIN_FILENO)) break;
            while (ert_closeFd(STDOUT_FILENO)) break;
            while (ert_closeFd(STDERR_FILENO)) break;

            if (ert_openStdFds())
            {
                dprintf(errfd, "@@@ %u\n", __LINE__);
                break;
            }

            if (checkChildCloseOnExec(STDIN_FILENO, errfd))
            {
                dprintf(errfd, "@@@ %u\n", __LINE__);
                break;
            }

            if (checkChildCloseOnExec(STDOUT_FILENO, errfd))
            {
                dprintf(errfd, "@@@ %u\n", __LINE__);
                break;
            }

            if (checkChildCloseOnExec(STDERR_FILENO, errfd))
            {
                dprintf(errfd, "@@@ %u\n", __LINE__);
                break;
            }

            if ( ! ert_ownFdValid(STDIN_FILENO) ||
                   ert_readFd(STDIN_FILENO, buf, 1, 0))
            {
                dprintf(errfd, "@@@ %u\n", __LINE__);
                break;
            }

            if ( ! ert_ownFdValid(STDOUT_FILENO) ||
                 -1 != ert_writeFd(STDOUT_FILENO, buf, 1, 0) || EPIPE != errno)
            {
                dprintf(errfd, "@@@ %u\n", __LINE__);
                break;
            }

            if ( ! ert_ownFdValid(STDERR_FILENO) ||
                 -1 != ert_writeFd(STDERR_FILENO, buf, 1, 0) || EPIPE != errno)
            {
                dprintf(errfd, "@@@ %u\n", __LINE__);
                break;
            }

            // Test that if only stdin is closed, it will be the only
            // one that is overlaid.

            if (fillStdFds_(errfd))
            {
                dprintf(errfd, "@@@ %u\n", __LINE__);
                break;
            }

            while (ert_closeFd(STDIN_FILENO)) break;

            if (ert_openStdFds())
            {
                dprintf(errfd, "@@@ %u\n", __LINE__);
                break;
            }

            if (checkChildCloseOnExec(STDIN_FILENO, errfd))
            {
                dprintf(errfd, "@@@ %u\n", __LINE__);
                break;
            }

            if ( ! ert_ownFdValid(STDIN_FILENO) ||
                   ert_readFd(STDIN_FILENO, buf, 1, 0))
            {
                dprintf(errfd, "@@@ %u\n", __LINE__);
                break;
            }

            // Test that if only stdout is closed, it will be the only
            // one that is overlaid.

            if (fillStdFds_(errfd))
            {
                dprintf(errfd, "@@@ %u\n", __LINE__);
                break;
            }

            while (ert_closeFd(STDOUT_FILENO)) break;

            if (ert_openStdFds())
            {
                dprintf(errfd, "@@@ %u\n", __LINE__);
                break;
            }

            if ( ! ert_ownFdValid(STDOUT_FILENO) ||
                 -1 != ert_writeFd(STDOUT_FILENO, buf, 1, 0) || EPIPE != errno)
            {
                dprintf(errfd, "@@@ %u\n", __LINE__);
                break;
            }

            // Test that if only stderr is closed, it will be the only
            // one that is overlaid.

            if (fillStdFds_(errfd))
            {
                dprintf(errfd, "@@@ %u\n", __LINE__);
                break;
            }

            while (ert_closeFd(STDERR_FILENO)) break;

            if (ert_openStdFds())
            {
                dprintf(errfd, "@@@ %u\n", __LINE__);
                break;
            }

            if ( ! ert_ownFdValid(STDERR_FILENO) ||
                 -1 != ert_writeFd(STDERR_FILENO, buf, 1, 0) || EPIPE != errno)
            {
                dprintf(errfd, "@@@ %u\n", __LINE__);
                break;
            }

            rc = 0;

        } while (0);

        if (rc)
        {
            execl("/bin/false", "false", (char *) 0);
            _exit(EXIT_FAILURE);
        }

        execl("/bin/true", "true", (char *) 0);
        _exit(EXIT_FAILURE);
    }

    int status;
    EXPECT_EQ(0, ert_reapProcessChild(Ert_Pid(childpid), &status));
    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_EQ(EXIT_SUCCESS, WEXITSTATUS(status));
}

#include "_test_.h"
