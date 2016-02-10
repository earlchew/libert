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

#include "_k9.h"

#include "env_.h"
#include "macros_.h"
#include "pipe_.h"
#include "socketpair_.h"
#include "stdfdfiller_.h"
#include "pidfile_.h"
#include "thread_.h"
#include "error_.h"
#include "pollfd_.h"
#include "test_.h"
#include "fd_.h"
#include "dl_.h"
#include "type_.h"

#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/poll.h>
#include <sys/ioctl.h>
#include <sys/un.h>

/* TODO
 *
 * cmdRunCommand() is too big, break it up
 * Add test case for SIGKILL of watchdog and child not watching tether
 * Fix vgcore being dropped everywhere
 * Implement process lock in child process
 * Remove *_unused arguments
 * When announcing child, ensure pid is active first
 * Use SIGHUP to kill umbilical monitor
 * Use struct Type for other poll loops
 * Handle SIGCHLD continue/stopped
 * Propagate signal termination from child to parent
 */

/* -------------------------------------------------------------------------- */
enum PollFdKind
{
    POLL_FD_TETHER,
    POLL_FD_CHILD,
    POLL_FD_UMBILICAL,
    POLL_FD_KINDS
};

static const char *sPollFdNames[POLL_FD_KINDS] =
{
    [POLL_FD_TETHER]    = "tether",
    [POLL_FD_CHILD]     = "child",
    [POLL_FD_UMBILICAL] = "umbilical",
};

/* -------------------------------------------------------------------------- */
enum PollFdTimerKind
{
    POLL_FD_TIMER_TETHER,
    POLL_FD_TIMER_UMBILICAL,
    POLL_FD_TIMER_ORPHAN,
    POLL_FD_TIMER_TERMINATION,
    POLL_FD_TIMER_DISCONNECTION,
    POLL_FD_TIMER_KINDS
};

static const char *sPollFdTimerNames[POLL_FD_TIMER_KINDS] =
{
    [POLL_FD_TIMER_TETHER]        = "tether",
    [POLL_FD_TIMER_UMBILICAL]     = "umbilical",
    [POLL_FD_TIMER_ORPHAN]        = "orphan",
    [POLL_FD_TIMER_TERMINATION]   = "termination",
    [POLL_FD_TIMER_DISCONNECTION] = "disconnection",
};

/* -------------------------------------------------------------------------- */
enum PollFdMonitorKind
{
    POLL_FD_MONITOR_UMBILICAL,
    POLL_FD_MONITOR_KINDS
};

static const char *sPollFdMonitorNames[POLL_FD_MONITOR_KINDS] =
{
    [POLL_FD_MONITOR_UMBILICAL] = "umbilical",
};

/* -------------------------------------------------------------------------- */
enum PollFdMonitorTimerKind
{
    POLL_FD_MONITOR_TIMER_UMBILICAL,
    POLL_FD_MONITOR_TIMER_KINDS
};

static const char *sPollFdMonitorTimerNames[POLL_FD_MONITOR_TIMER_KINDS] =
{
    [POLL_FD_MONITOR_TIMER_UMBILICAL] = "umbilical",
};

/* -------------------------------------------------------------------------- */
struct ChildProcess
{
    pid_t mPid;
    pid_t mPgid;

    struct Pipe  mChildPipe_;
    struct Pipe* mChildPipe;
    struct Pipe  mTetherPipe_;
    struct Pipe* mTetherPipe;
};

/* -------------------------------------------------------------------------- */
static const struct Type * const sUmbilicalMonitorType =
    TYPE("UmbilicalMonitor");

struct UmbilicalMonitorPoll
{
    const struct Type *mType;

    struct File              *mUmbilicalFile;
    struct pollfd             mPollFds[POLL_FD_MONITOR_KINDS];
    struct PollFdAction       mPollFdActions[POLL_FD_MONITOR_KINDS];
    struct PollFdTimerAction  mPollFdTimerActions[POLL_FD_MONITOR_TIMER_KINDS];
    unsigned                  mCycleCount;
    unsigned                  mCycleLimit;
    pid_t                     mParentPid;
};

/* -------------------------------------------------------------------------- */
static void
pollFdMonitorUmbilical(void                        *self_,
                       struct pollfd               *aPollFds_unused,
                       const struct EventClockTime *aPollTime)
{
    struct UmbilicalMonitorPoll *self = self_;

    ensure(sUmbilicalMonitorType == self->mType);

    char buf[1];

    ssize_t rdlen = read(
        self->mPollFds[POLL_FD_MONITOR_UMBILICAL].fd, buf, sizeof(buf));

    if (-1 == rdlen)
    {
        if (EINTR != errno)
            terminate(
                errno,
                "Unable to read umbilical connection");
    }
    else if ( ! rdlen)
    {
        warn(0, "Broken umbilical connection");
        self->mPollFds[POLL_FD_MONITOR_UMBILICAL].events = 0;
    }
    else
    {
        /* Once activity is detected on the umbilical, reset the
         * umbilical timer, but configure the timer so that it is
         * out-of-phase with the expected activity on the umbilical
         * to avoid having to deal with races when there is a tight
         * finish. */

        struct PollFdTimerAction *umbilicalTimer =
            &self->mPollFdTimerActions[POLL_FD_MONITOR_TIMER_UMBILICAL];

        lapTimeTrigger(&umbilicalTimer->mSince,
                       umbilicalTimer->mPeriod, aPollTime);

        lapTimeDelay(
            &umbilicalTimer->mSince,
            Duration(NanoSeconds(umbilicalTimer->mPeriod.duration.ns / 2)));

        self->mCycleCount = 0;
    }
}

static void
pollFdMonitorTimerUmbilical(
    void                        *self_,
    struct PollFdTimerAction    *aPollFdTimerAction_unused,
    const struct EventClockTime *aPollTime)
{
    struct UmbilicalMonitorPoll *self = self_;

    ensure(sUmbilicalMonitorType == self->mType);

    /* If nothing is available from the umbilical connection after the
     * timeout period expires, then assume that the watchdog itself
     * is stuck. */

    enum ProcessStatus parentstatus = fetchProcessState(self->mParentPid);

    if (ProcessStateStopped == parentstatus)
    {
        debug(
            0,
            "umbilical timeout deferred due to parent status %c",
            parentstatus);
        self->mCycleCount = 0;
    }
    else if (++self->mCycleCount >= self->mCycleLimit)
    {
        warn(0, "Umbilical connection timed out");
        self->mPollFds[POLL_FD_MONITOR_UMBILICAL].events = 0;
    }
}

static bool
pollFdMonitorCompletion(void                     *self_,
                        struct pollfd            *aPollFds_unused,
                        struct PollFdTimerAction *aPollFdTimer_unused)
{
    struct UmbilicalMonitorPoll *self = self_;

    ensure(sUmbilicalMonitorType == self->mType);

    return ! self->mPollFds[POLL_FD_MONITOR_UMBILICAL].events;
}

/* -------------------------------------------------------------------------- */
static void
createChild(struct ChildProcess *self)
{
    self->mPid = 0;
    self->mPgid = 0;

    /* Only the reading end of the tether is marked non-blocking. The
     * writing end must be used by the child process (and perhaps inherited
     * by any subsequent process that it forks), so only the reading
     * end is marked non-blocking. */

    if (createPipe(&self->mTetherPipe_, 0))
        terminate(
            errno,
            "Unable to create tether pipe");
    self->mTetherPipe = &self->mTetherPipe_;

    if (closeFileOnExec(self->mTetherPipe->mRdFile, O_CLOEXEC))
        terminate(
            errno,
            "Unable to set close on exec for tether");

    if (nonblockingFile(self->mTetherPipe->mRdFile))
        terminate(
            errno,
            "Unable to mark tether non-blocking");

    if (createPipe(&self->mChildPipe_, O_CLOEXEC | O_NONBLOCK))
        terminate(
            errno,
            "Unable to create child pipe");
    self->mChildPipe = &self->mChildPipe_;
}

/* -------------------------------------------------------------------------- */
static void
reapChild(void *self_)
{
    struct ChildProcess *self = self_;

    /* Check that the child process being monitored is the one
     * is the subject of the signal. Here is a way for a parent
     * to be surprised by the presence of an adopted child:
     *
     *  sleep 5 & exec sh -c 'sleep 1 & wait'
     *
     * The new shell inherits the earlier sleep as a child even
     * though it did not create it. */

    enum ProcessStatus childstatus = monitorProcess(self->mPid);

    if (ProcessStatusError == childstatus)
        terminate(
            errno,
            "Unable to determine status of pid %jd",
            (intmax_t) self->mPid);

    if (ProcessStatusRunning == childstatus)
    {
        /* Only write when the child starts running again. This avoids
         * questionable semantics should the pipe overflow since the
         * only significance of the pipe is whether or not there is content. */

        char buf[1] = { 0 };

        if (-1 == writeFile(self->mChildPipe->mWrFile, buf, sizeof(buf)))
        {
            if (EWOULDBLOCK != errno)
                terminate(
                    errno,
                    "Unable to write status to child pipe");
        }
    }
    else if (ProcessStatusExited != childstatus &&
             ProcessStatusKilled != childstatus &&
             ProcessStatusDumped != childstatus)
    {
        debug(1,
              "child pid %jd status %c",
              (intmax_t) self->mPid, childstatus);
    }
    else
    {
        if (closePipeWriter(self->mChildPipe))
            terminate(
                errno,
                "Unable to close child pipe writer");
    }
}

/* -------------------------------------------------------------------------- */
static void
killChild(void *self_, int aSigNum)
{
    struct ChildProcess *self = self_;

    if ( ! self->mPid)
        terminate(
            0,
            "Signal race when trying to deliver signal %d", aSigNum);

    debug(0,
          "sending signal %d to child pid %jd",
          aSigNum,
          (intmax_t) self->mPid);

    if (kill(self->mPid, aSigNum))
    {
        if (ESRCH != errno)
            terminate(
                errno,
                "Unable to deliver signal %d to child pid %jd",
                aSigNum,
                (intmax_t) self->mPid);
    }
}

/* -------------------------------------------------------------------------- */
static int
forkChild(
    struct ChildProcess  *self,
    char                **aCmd,
    struct StdFdFiller   *aStdFdFiller,
    struct Pipe          *aSyncPipe,
    struct SocketPair    *aUmbilicalSocket)
{
    int rc = -1;

    /* Both the parent and child share the same signal handler configuration.
     * In particular, no custom signal handlers are configured, so
     * signals delivered to either will likely caused them to terminate.
     *
     * This is safe because that would cause one of end the synchronisation
     * pipe to close, and the other end will eventually notice. */

    pid_t childPid = forkProcess(
        gOptions.mSetPgid
        ? ForkProcessSetProcessGroup : ForkProcessShareProcessGroup, 0);

    /* Although it would be better to ensure that the child process and watchdog
     * in the same process group, switching the process group of the watchdog
     * will likely cause a race in an inattentive parent of the watchdog.
     * For example upstart(8) has:
     *
     *    pgid = getpgid(pid);
     *    kill(pgid > 0 ? -pgid : pid, signal);
     */

    if (-1 == childPid)
        goto Finally;

    if ( ! childPid)
    {
        childPid = getpid();

        debug(0, "starting child process");

        /* The forked child has all its signal handlers reset, but
         * note that the parent will wait for the child to synchronise
         * before sending it signals, so that there is no race here.
         *
         * Close the StdFdFiller in case this will free up stdin, stdout or
         * stderr. The remaining operations will close the remaining
         * unwanted file descriptors. */

        if (closeStdFdFiller(aStdFdFiller))
            terminate(
                errno,
                "Unable to close stdin, stdout and stderr fillers");

        if (closePipe(self->mChildPipe))
            terminate(
                errno,
                "Unable to close child pipe");
        self->mChildPipe = 0;

        /* Wait until the parent has created the pidfile. This
         * invariant can be used to determine if the pidfile
         * is really associated with the process possessing
         * the specified pid. */

        debug(0, "synchronising child process");

        RACE
        ({
            while (true)
            {
                char buf[1];

                switch (read(aSyncPipe->mRdFile->mFd, buf, 1))
                {
                default:
                        break;

                case -1:
                    if (EINTR == errno)
                        continue;
                    terminate(
                        errno,
                        "Unable to synchronise child");
                    break;

                case 0:
                    _exit(EXIT_FAILURE);
                    break;
                }

                break;
            }
        });

        if (closePipe(aSyncPipe))
            terminate(
                errno,
                "Unable to close sync pipe");

        do
        {
            /* Close the reading end of the tether pipe separately
             * because it might turn out that the writing end
             * will not need to be duplicated. */

            if (closePipeReader(self->mTetherPipe))
                terminate(
                    errno,
                    "Unable to close tether pipe reader");

            if (closeSocketPair(aUmbilicalSocket))
                terminate(
                    errno,
                    "Unable to close umbilical socket");
            aUmbilicalSocket = 0;

            if (gOptions.mTether)
            {
                int tetherFd = *gOptions.mTether;

                if (0 > tetherFd)
                    tetherFd = self->mTetherPipe->mWrFile->mFd;

                char tetherArg[sizeof(int) * CHAR_BIT + 1];

                sprintf(tetherArg, "%d", tetherFd);

                if (gOptions.mName)
                {
                    bool useEnv = isupper(gOptions.mName[0]);

                    for (unsigned ix = 1; useEnv && gOptions.mName[ix]; ++ix)
                    {
                        unsigned char ch = gOptions.mName[ix];

                        if ( ! isupper(ch) && ! isdigit(ch) && ch != '_')
                            useEnv = false;
                    }

                    if (useEnv)
                    {
                        if (setenv(gOptions.mName, tetherArg, 1))
                            terminate(
                                errno,
                                "Unable to set environment variable '%s'",
                                gOptions.mName);
                    }
                    else
                    {
                        /* Start scanning from the first argument, leaving
                         * the command name intact. */

                        char *matchArg = 0;

                        for (unsigned ix = 1; aCmd[ix]; ++ix)
                        {
                            matchArg = strstr(aCmd[ix], gOptions.mName);

                            if (matchArg)
                            {
                                char replacedArg[
                                    strlen(aCmd[ix])       -
                                    strlen(gOptions.mName) +
                                    strlen(tetherArg)      + 1];

                                sprintf(replacedArg,
                                        "%.*s%s%s",
                                        matchArg - aCmd[ix],
                                        aCmd[ix],
                                        tetherArg,
                                        matchArg + strlen(gOptions.mName));

                                aCmd[ix] = strdup(replacedArg);

                                if ( ! aCmd[ix])
                                    terminate(
                                        errno,
                                        "Unable to duplicate '%s'",
                                        replacedArg);
                                break;
                            }
                        }

                        if ( ! matchArg)
                            terminate(
                                0,
                                "Unable to find matching argument '%s'",
                                gOptions.mName);
                    }
                }

                if (tetherFd == self->mTetherPipe->mWrFile->mFd)
                    break;

                if (dup2(self->mTetherPipe->mWrFile->mFd, tetherFd) != tetherFd)
                    terminate(
                        errno,
                        "Unable to dup tether pipe fd %d to fd %d",
                        self->mTetherPipe->mWrFile->mFd,
                        tetherFd);
            }

            if (closePipe(self->mTetherPipe))
                terminate(
                    errno,
                    "Unable to close tether pipe");

        } while (0);

        debug(0, "child process synchronised");

        /* The child process does not close the process lock because it
         * might need to emit a diagnostic if execvp() fails. Rely on
         * O_CLOEXEC to close the underlying file descriptors. */

        execvp(aCmd[0], aCmd);
        terminate(
            errno,
            "Unable to execute '%s'", aCmd[0]);
    }

    /* Even if the child has terminated, it remains a zombie until reaped,
     * so it is safe to query it to determine its process group. */

    self->mPid  = childPid;
    self->mPgid = gOptions.mSetPgid ? self->mPid : 0;

    debug(0,
          "running child pid %jd in pgid %jd",
          (intmax_t) self->mPid,
          (intmax_t) self->mPgid);

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
static void
closeChildTether(struct ChildProcess *self)
{
    ensure(self->mTetherPipe);

    if (closePipe(self->mTetherPipe))
        terminate(
            errno,
            "Unable to close tether pipe");
    self->mTetherPipe = 0;
}

/* -------------------------------------------------------------------------- */
static void
closeChildFiles(struct ChildProcess *self)
{
    if (closePipe(self->mChildPipe))
        terminate(
            errno,
            "Unable to close child pipe");
    self->mChildPipe = 0;

    if (closePipe(self->mTetherPipe))
        terminate(
            errno,
            "Unable to close tether pipe");
    self->mTetherPipe = 0;
}

/* -------------------------------------------------------------------------- */
static int
closeChild(struct ChildProcess *self)
{
    closeChildFiles(self);

    int status;

    if (reapProcess(self->mPid, &status))
        terminate(
            errno,
            "Unable to reap child pid '%jd'",
            (intmax_t) self->mPid);

    return status;
}

/* -------------------------------------------------------------------------- */
static void
monitorChildUmbilical(struct ChildProcess *self, pid_t aParentPid)
{
    /* This function is called in the context of the umbilical process
     * to monitor the umbilical, and if the umbilical fails, to kill
     * the child.
     *
     * The caller has already configured stdin to be used to read data
     * from the umbilical pipe.
     */

    closeChildFiles(self);

    /* The umbilical process is not the parent of the child process being
     * watched, so that there is no reliable way to send a signal to that
     * process alone because the pid might be recycled by the time the signal
     * is sent. Instead rely on the umbilical monitor being in the same
     * process group as the child process and use the process group as
     * a means of controlling the cild process. */

    unsigned cycleLimit = 2;

    struct UmbilicalMonitorPoll monitorpoll =
    {
        .mType       = sUmbilicalMonitorType,
        .mCycleLimit = cycleLimit,
        .mParentPid  = aParentPid,

        .mPollFds =
        {
            [POLL_FD_MONITOR_UMBILICAL] = { .fd     = STDIN_FILENO,
                                            .events = POLL_INPUTEVENTS },
        },

        .mPollFdActions =
        {
            [POLL_FD_MONITOR_UMBILICAL] = { pollFdMonitorUmbilical },
        },

        .mPollFdTimerActions =
        {
            [POLL_FD_MONITOR_TIMER_UMBILICAL] =
            {
                pollFdMonitorTimerUmbilical,
                Duration(
                    NanoSeconds(
                        NSECS(Seconds(gOptions.mTimeout.mUmbilical_s)).ns
                        / cycleLimit)),
            },
        },
    };

    /* Use a blocking read synchronise with the watchdog to avoid timing
     * races. The watchdog will write to the umbilical when it is ready
     * to start timing. */

    debug(0, "synchronising umbilical");

    if (-1 == waitFdReadReady(STDIN_FILENO, 0))
        terminate(
            errno,
            "Unable to wait for umbilical synchronisation");

    pollFdMonitorUmbilical(&monitorpoll, 0, 0);

    debug(0, "synchronised umbilical");

    struct PollFd pollfd;
    if (createPollFd(
            &pollfd,
            monitorpoll.mPollFds,
            monitorpoll.mPollFdActions,
            sPollFdMonitorNames, POLL_FD_MONITOR_KINDS,
            monitorpoll.mPollFdTimerActions,
            sPollFdMonitorTimerNames, POLL_FD_MONITOR_TIMER_KINDS,
            pollFdMonitorCompletion, &monitorpoll))
        terminate(
            errno,
            "Unable to initialise polling loop");

    if (runPollFdLoop(&pollfd))
        terminate(
            errno,
            "Unable to run polling loop");

    if (closePollFd(&pollfd))
        terminate(
            errno,
            "Unable to close polling loop");

    pid_t pgid = getpgid(0);

    warn(0, "Killing child pgid %jd", (intmax_t) pgid);

    if (kill(0, SIGKILL))
        terminate(
            errno,
            "Unable to kill child pgid %jd", (intmax_t) pgid);
}

/* -------------------------------------------------------------------------- */
/* Tether Thread
 *
 * The purpose of the tether thread is to isolate the event loop
 * in the main thread from blocking that might arise when writing to
 * the destination file descriptor. The destination file descriptor
 * cannot be guaranteed to be non-blocking because it is inherited
 * when the watchdog process is started. */

enum TetherThreadState
{
    TETHER_THREAD_STOPPED,
    TETHER_THREAD_RUNNING,
    TETHER_THREAD_STOPPING,
};

struct TetherThread
{
    pthread_t    mThread;
    struct Pipe  mControlPipe;
    struct Pipe *mNullPipe;
    bool         mFlushed;

    struct {
        pthread_mutex_t       mMutex;
        struct EventClockTime mSince;
    } mActivity;

    struct {
        pthread_mutex_t        mMutex;
        pthread_cond_t         mCond;
        enum TetherThreadState mValue;
    } mState;
};

enum TetherFdKind
{
    TETHER_FD_CONTROL,
    TETHER_FD_INPUT,
    TETHER_FD_OUTPUT,
    TETHER_FD_KINDS
};

static const char *sTetherFdNames[] =
{
    [TETHER_FD_CONTROL] = "control",
    [TETHER_FD_INPUT]   = "input",
    [TETHER_FD_OUTPUT]  = "output",
};

enum TetherFdTimerKind
{
    TETHER_FD_TIMER_DISCONNECT,
    TETHER_FD_TIMER_KINDS
};

static const char *sTetherFdTimerNames[] =
{
    [TETHER_FD_TIMER_DISCONNECT] = "disconnection",
};

struct TetherPoll
{
    struct TetherThread     *mThread;
    int                      mSrcFd;
    int                      mDstFd;

    struct pollfd            mPollFds[TETHER_FD_KINDS];
    struct PollFdAction      mPollFdActions[TETHER_FD_KINDS];
    struct PollFdTimerAction mPollFdTimerActions[TETHER_FD_TIMER_KINDS];
};

static void
polltethercontrol(void                        *self_,
                  struct pollfd               *aPollFds_unused,
                  const struct EventClockTime *aPollTime)
{
    struct TetherPoll *self = self_;

    char buf[1];

    if (0 > readFd(self->mPollFds[TETHER_FD_CONTROL].fd, buf, sizeof(buf)))
        terminate(
            errno,
            "Unable to read tether control");

    debug(0, "tether disconnection request received");

    /* Note that gOptions.mTimeout.mDrain_s might be zero to indicate
     * that the no drain timeout is to be enforced. */

    self->mPollFdTimerActions[TETHER_FD_TIMER_DISCONNECT].mPeriod =
        Duration(NSECS(Seconds(gOptions.mTimeout.mDrain_s)));
}

static void
polltetherdrain(void                        *self_,
                struct pollfd               *aPollFds_unused,
                const struct EventClockTime *aPollTime)
{
    struct TetherPoll *self = self_;

    if (self->mPollFds[TETHER_FD_CONTROL].events)
    {
        {
            lockMutex(&self->mThread->mActivity.mMutex);
            self->mThread->mActivity.mSince = eventclockTime();
            unlockMutex(&self->mThread->mActivity.mMutex);
        }

        bool drained = true;

        do
        {
            /* The output file descriptor must have been closed if:
             *
             *  o There is no input available, so the poll must have
             *    returned because an output disconnection event was detected
             *  o Input was available, but none could be written to the output
             */

            int available;

            if (ioctl(self->mSrcFd, FIONREAD, &available))
                terminate(
                    errno,
                    "Unable to find amount of readable data in fd %d",
                    self->mSrcFd);

            if ( ! available)
            {
                debug(0, "tether drain input empty");
                break;
            }

            /* This splice(2) call will likely block if it is unable to
             * write all the data to the output file descriptor immediately.
             * Note that it cannot block on reading the input file descriptor
             * because that file descriptor is private to this process, the
             * amount of input available is known and is only read by this
             * thread. */

            ssize_t bytes = spliceFd(
                self->mSrcFd, self->mDstFd, available, SPLICE_F_MOVE);

            if ( ! bytes)
            {
                debug(0, "tether drain output closed");
                break;
            }

            if (-1 == bytes)
            {
                if (EPIPE == errno)
                {
                    debug(0, "tether drain output broken");
                    break;
                }

                if (EWOULDBLOCK != errno && EINTR != errno)
                    terminate(
                        errno,
                        "Unable to splice %d bytes from fd %d to fd %d",
                        available,
                        self->mSrcFd,
                        self->mDstFd);
            }
            else
            {
                debug(1,
                      "drained %zd bytes from fd %d to fd %d",
                      bytes, self->mSrcFd, self->mDstFd);
            }

            drained = false;

        } while (0);

        if (drained)
            self->mPollFds[TETHER_FD_CONTROL].events = 0;
    }
}

static void
polltetherdisconnected(void                        *self_,
                       struct PollFdTimerAction    *aPollFdTimerAction_unused,
                       const struct EventClockTime *aPollTime)
{
    struct TetherPoll *self = self_;

    /* Once the tether drain timeout expires, disable the timer, and
     * force completion of the tether thread. */

    self->mPollFdTimerActions[TETHER_FD_TIMER_DISCONNECT].mPeriod =
        Duration(NanoSeconds(0));

    self->mPollFds[TETHER_FD_CONTROL].events = 0;
}

static bool
polltethercompletion(void                     *self_,
                     struct pollfd            *aPollFds_unused,
                     struct PollFdTimerAction *aPollFdTimer_unused)
{
    struct TetherPoll *self = self_;

    return ! self->mPollFds[TETHER_FD_CONTROL].events;
}

static void *
tetherThreadMain_(void *self_)
{
    struct TetherThread *self = self_;

    {
        lockMutex(&self->mState.mMutex);
        self->mState.mValue = TETHER_THREAD_RUNNING;
        unlockMutexSignal(&self->mState.mMutex, &self->mState.mCond);
    }

    /* Do not open, or close files in this thread because it will race
     * the main thread forking the child process. When forking the
     * child process, it is important to control the file descriptors
     * inherited by the chlid. */

    int srcFd     = STDIN_FILENO;
    int dstFd     = STDOUT_FILENO;
    int controlFd = self->mControlPipe.mRdFile->mFd;

    /* The file descriptor for stdin is a pipe created by the watchdog
     * so it is known to be nonblocking. The file descriptor for stdout
     * is inherited, so it is likely blocking. */

    ensure(nonblockingFd(srcFd));

    /* The tether thread is configured to receive SIGALRM, but
     * these signals are not delivered until the thread is
     * flushed after the child process has terminated. */

    struct ThreadSigMask threadSigMask;

    const int sigList[] = { SIGALRM, 0 };

    if (pushThreadSigMask(&threadSigMask, ThreadSigMaskUnblock, sigList))
        terminate(errno, "Unable to push thread signal mask");

    struct TetherPoll tetherpoll =
    {
        .mThread = self,
        .mSrcFd  = srcFd,
        .mDstFd  = dstFd,

        .mPollFds =
        {
            [TETHER_FD_CONTROL]= {.fd= controlFd,.events= POLL_INPUTEVENTS },
            [TETHER_FD_INPUT]  = {.fd= srcFd,    .events= POLL_INPUTEVENTS },
            [TETHER_FD_OUTPUT] = {.fd= dstFd,    .events= POLL_DISCONNECTEVENT},
        },

        .mPollFdActions =
        {
            [TETHER_FD_CONTROL] = { polltethercontrol },
            [TETHER_FD_INPUT]   = { polltetherdrain },
            [TETHER_FD_OUTPUT]  = { polltetherdrain },
        },

        .mPollFdTimerActions =
        {
            [TETHER_FD_TIMER_DISCONNECT] = { polltetherdisconnected },
        },
    };

    struct PollFd pollfd;
    if (createPollFd(
            &pollfd,
            tetherpoll.mPollFds,
            tetherpoll.mPollFdActions,
            sTetherFdNames, TETHER_FD_KINDS,
            tetherpoll.mPollFdTimerActions,
            sTetherFdTimerNames, TETHER_FD_TIMER_KINDS,
            polltethercompletion, &tetherpoll))
        terminate(
            errno,
            "Unable to initialise polling loop");

    if (runPollFdLoop(&pollfd))
        terminate(
            errno,
            "Unable to run polling loop");

    if (closePollFd(&pollfd))
        terminate(
            errno,
            "Unable to close polling loop");

    if (popThreadSigMask(&threadSigMask))
        terminate(errno, "Unable to push process signal mask");

    /* Close the input file descriptor so that there is a chance
     * to propagte SIGPIPE to the child process. */

    if (dup2(self->mNullPipe->mRdFile->mFd, srcFd) != srcFd)
        terminate(
            errno,
            "Unable to dup fd %d to fd %d",
            self->mNullPipe->mRdFile->mFd,
            srcFd);

    /* Shut down the end of the control pipe controlled by this thread,
     * without closing the control pipe file descriptor itself. The
     * monitoring loop is waiting for the control pipe to close before
     * exiting the event loop. */

    if (dup2(self->mNullPipe->mRdFile->mFd, controlFd) != controlFd)
        terminate(errno, "Unable to shut down tether thread control");

    debug(0, "tether emptied");

    {
        lockMutex(&self->mState.mMutex);

        while (TETHER_THREAD_RUNNING == self->mState.mValue)
            waitCond(&self->mState.mCond, &self->mState.mMutex);

        unlockMutex(&self->mState.mMutex);
    }

    return 0;
}

static void
createTetherThread(struct TetherThread *self, struct Pipe *aNullPipe)
{
    if (createPipe(&self->mControlPipe, O_CLOEXEC | O_NONBLOCK))
        terminate(errno, "Unable to create tether control pipe");

    if (errno = pthread_mutex_init(&self->mActivity.mMutex, 0))
        terminate(errno, "Unable to create activity mutex");

    if (errno = pthread_mutex_init(&self->mState.mMutex, 0))
        terminate(errno, "Unable to create state mutex");

    if (errno = pthread_cond_init(&self->mState.mCond, 0))
        terminate(errno, "Unable to create state condition");

    self->mNullPipe        = aNullPipe;
    self->mActivity.mSince = eventclockTime();
    self->mState.mValue    = TETHER_THREAD_STOPPED;
    self->mFlushed         = false;

    {
        struct ThreadSigMask threadSigMask;

        if (pushThreadSigMask(&threadSigMask, ThreadSigMaskBlock, 0))
            terminate(errno, "Unable to push thread signal mask");

        createThread(&self->mThread, 0, tetherThreadMain_, self);

        if (popThreadSigMask(&threadSigMask))
            terminate(errno, "Unable to pop thread signal mask");
    }

    {
        lockMutex(&self->mState.mMutex);

        while (TETHER_THREAD_STOPPED == self->mState.mValue)
            waitCond(&self->mState.mCond, &self->mState.mMutex);

        unlockMutex(&self->mState.mMutex);
    }
}

static void
pingTetherThread(struct TetherThread *self)
{
    debug(0, "ping tether thread");

    if (errno = pthread_kill(self->mThread, SIGALRM))
        terminate(
            errno,
            "Unable to signal tether thread");
}

static void
flushTetherThread(struct TetherThread *self)
{
    debug(0, "flushing tether thread");

    if (watchProcessClock(VoidMethod(0, 0), Duration(NanoSeconds(0))))
        terminate(
            errno,
            "Unable to configure synchronisation clock");

    char buf[1] = { 0 };

    if (sizeof(buf) != writeFile(self->mControlPipe.mWrFile, buf, sizeof(buf)))
    {
        /* This code will race the tether thread which might finished
         * because it already has detected that the child process has
         * terminated and closed its file descriptors. */

        if (EPIPE != errno)
            terminate(
                errno,
                "Unable to flush tether thread");
    }

    self->mFlushed = true;
}

static void
closeTetherThread(struct TetherThread *self)
{
    ensure(self->mFlushed);

    /* This method is not called until the tether thread has closed
     * its end of the control pipe to indicate that it has completed.
     * At that point the thread is waiting for the thread state
     * to change so that it can exit. */

    debug(0, "synchronising tether thread");

    {
        lockMutex(&self->mState.mMutex);

        ensure(TETHER_THREAD_RUNNING == self->mState.mValue);
        self->mState.mValue = TETHER_THREAD_STOPPING;

        unlockMutexSignal(&self->mState.mMutex, &self->mState.mCond);
    }

    (void) joinThread(&self->mThread);

    if (unwatchProcessClock())
        terminate(
            errno,
            "Unable to reset synchronisation clock");

    if (errno = pthread_cond_destroy(&self->mState.mCond))
        terminate(errno, "Unable to destroy state condition");

    if (errno = pthread_mutex_destroy(&self->mState.mMutex))
        terminate(errno, "Unable to destroy state mutex");

    if (errno = pthread_mutex_destroy(&self->mActivity.mMutex))
        terminate(errno, "Unable to destroy activity mutex");

    if (closePipe(&self->mControlPipe))
        terminate(errno, "Unable to close tether control pipe");
}

/* -------------------------------------------------------------------------- */
/* Child Process Monitoring
 *
 * The child process must be monitored for activity, and also for
 * termination.
 */

static const struct Type * const sChildMonitorType = TYPE("ChildMonitor");

struct ChildSignalPlan
{
    pid_t mPid;
    int   mSig;
};

struct ChildMonitor
{
    const struct Type *mType;

    pid_t mChildPid;

    struct Pipe         *mNullPipe;
    struct TetherThread *mTetherThread;

    struct
    {
        const struct ChildSignalPlan *mSignalPlan;
        struct Duration               mSignalPeriod;
    } mTermination;

    struct
    {
        struct File *mFile;
    } mUmbilical;

    struct
    {
        unsigned mCycleCount;       /* Current number of cycles */
        unsigned mCycleLimit;       /* Cycles before triggering */
    } mTether;

    struct pollfd            mPollFds[POLL_FD_KINDS];
    struct PollFdAction      mPollFdActions[POLL_FD_KINDS];
    struct PollFdTimerAction mPollFdTimerActions[POLL_FD_TIMER_KINDS];
};

/* -------------------------------------------------------------------------- */
/* Child Termination State Machine
 *
 * When it is necessary to terminate the child process, run a state
 * machine to sequence through a signal plan that walks through
 * an escalating series of signals. */

static void
activateFdTimerTermination(struct ChildMonitor         *self,
                           const struct EventClockTime *aPollTime)
{
    /* When it is necessary to terminate the child process, the child
     * process might already have terminated. No special action is
     * taken with the expectation that the termination code should
     * fully expect that child the terminate at any time */

    struct PollFdTimerAction *terminationTimer =
        &self->mPollFdTimerActions[POLL_FD_TIMER_TERMINATION];

    if ( ! terminationTimer->mPeriod.duration.ns)
    {
        debug(1, "activating termination timer");

        terminationTimer->mPeriod =
            self->mTermination.mSignalPeriod;

        lapTimeTrigger(
            &terminationTimer->mSince, terminationTimer->mPeriod, aPollTime);
    }
}

static void
pollFdTimerTermination(void                        *self_,
                       struct PollFdTimerAction    *aPollFdTimerAction_unused,
                       const struct EventClockTime *aPollTime)
{
    struct ChildMonitor *self = self_;

    ensure(sChildMonitorType == self->mType);

    /* Remember that this function races termination of the child process.
     * The child process might have terminated by the time this function
     * attempts to deliver the next signal. This should be handled
     * correctly because the child process will remain as a zombie
     * and signals will be delivered successfully, but without effect. */

    pid_t pidNum = self->mTermination.mSignalPlan->mPid;
    int   sigNum = self->mTermination.mSignalPlan->mSig;

    if (self->mTermination.mSignalPlan[1].mSig)
        ++self->mTermination.mSignalPlan;

    warn(0, "Killing child pid %jd with signal %d", (intmax_t) pidNum, sigNum);

    if (kill(pidNum, sigNum))
        terminate(
            errno,
            "Unable to kill child pid %jd with signal %d",
            (intmax_t) pidNum,
            sigNum);
}

/* -------------------------------------------------------------------------- */
/* Maintain Umbilical Connection
 *
 * This connection allows the umbilical monitor to terminate the child
 * process if it detects that the watchdog is no longer functioning
 * properly. This is important in scenarios where the supervisor
 * init(8) kills the watchdog without giving the watchdog a chance
 * to clean up, or if the watchdog fails catatrophically. */

static void
pollFdUmbilical(void                        *self_,
                struct pollfd               *aPollFds_unused,
                const struct EventClockTime *aPollTime)
{
    struct ChildMonitor *self = self_;

    debug(0, "umbilical connection closed");

    self->mPollFds[POLL_FD_UMBILICAL].fd     = self->mNullPipe->mRdFile->mFd;
    self->mPollFds[POLL_FD_UMBILICAL].events = 0;

    struct PollFdTimerAction *umbilicalTimer =
        &self->mPollFdTimerActions[POLL_FD_TIMER_UMBILICAL];

    umbilicalTimer->mPeriod = Duration(NanoSeconds(0));

    struct PollFdTimerAction *tetherTimer =
        &self->mPollFdTimerActions[POLL_FD_TIMER_TETHER];

    tetherTimer->mPeriod = Duration(NanoSeconds(0));

    activateFdTimerTermination(self, aPollTime);
}

static int
pollFdWriteUmbilical(struct ChildMonitor *self)
{
    int rc = -1;

    char buf[1] = { 0 };

    ssize_t wrlen = write(
        self->mUmbilical.mFile->mFd, buf, sizeof(buf));

    if (-1 != wrlen)
        debug(1, "wrote umbilical %zd", wrlen);
    else
    {
        switch (errno)
        {
        default:
            terminate(errno, "Unable to write to umbilical");

        case EPIPE:
            debug(1, "writing to umbilical closed");
            break;

        case EWOULDBLOCK:
            debug(1, "writing to umbilical blocked");
            break;

        case EINTR:
            debug(1, "umbilical write interrupted");
            break;
        }

        goto Finally;
    }

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

static void
pollFdContUmbilical(void *self_)
{
    struct ChildMonitor *self = self_;

    ensure(sChildMonitorType == self->mType);

    /* This function is called when the process receives SIGCONT. The
     * function indicates to the umbilical monitor that the process
     * has just woken, so that the monitor can restart the timeout. */

    pollFdWriteUmbilical(self);
}

static void
pollFdTimerUmbilical(void                        *self_,
                     struct PollFdTimerAction    *aPollFdTimerAction_unused,
                     const struct EventClockTime *aPollTime)
{
    struct ChildMonitor *self = self_;

    ensure(sChildMonitorType == self->mType);

    /* Remember that the umbilical timer will race with child termination.
     * By the time this function runs, the child might already have
     * terminated so the umbilical socket might be closed. */

    if (pollFdWriteUmbilical(self))
    {
        if (EINTR == errno)
        {
            struct PollFdTimerAction *umbilicalTimer =
                &self->mPollFdTimerActions[POLL_FD_TIMER_UMBILICAL];

            /* Do not loop here on EINTR since it is important
             * to take care that the monitoring loop is
             * non-blocking. Instead, mark the timer as expired
             * for force the monitoring loop to retry immediately. */

            lapTimeTrigger(&umbilicalTimer->mSince,
                           umbilicalTimer->mPeriod, aPollTime);
        }
    }
}

/* -------------------------------------------------------------------------- */
/* Watchdog Tether
 *
 * The main tether used by the watchdog to monitor the child process requires
 * the child process to maintain some activity on the tether to demonstrate
 * that the child is functioning correctly. Data transfer on the tether
 * occurs in a separate thread since it might block. The main thread
 * is non-blocking and waits for the tether to be closed. */

static void
disconnectPollFdTether(struct ChildMonitor *self)
{
    debug(0, "disconnect tether control");

    self->mPollFds[POLL_FD_TETHER].fd     = self->mNullPipe->mRdFile->mFd;
    self->mPollFds[POLL_FD_TETHER].events = 0;
}

static void
pollFdTether(void                        *self_,
             struct pollfd               *aPollFds_unused,
             const struct EventClockTime *aPollTime)
{
    struct ChildMonitor *self = self_;

    ensure(sChildMonitorType == self->mType);

    /* The tether thread control pipe will be closed when the tether
     * between the child process and watchdog is shut down. */

    disconnectPollFdTether(self);
}

static void
restartFdTimerTether(struct ChildMonitor         *self,
                     const struct EventClockTime *aPollTime)
{
    self->mTether.mCycleCount = 0;

    lapTimeRestart(
        &self->mPollFdTimerActions[POLL_FD_TIMER_TETHER].mSince,
        aPollTime);
}

static void
pollFdTimerTether(void                        *self_,
                  struct PollFdTimerAction    *aPollFdTimerAction_unused,
                  const struct EventClockTime *aPollTime)
{
    struct ChildMonitor *self = self_;

    ensure(sChildMonitorType == self->mType);

    /* The tether timer is only active if there is a tether and it was
     * configured with a timeout. The timeout expires if there was
     * no activity on the tether with the consequence that the monitored
     * child will be terminated. */

    do
    {
        struct PollFdTimerAction *tetherTimer =
            &self->mPollFdTimerActions[POLL_FD_TIMER_TETHER];

        enum ProcessStatus childstatus = monitorProcess(self->mChildPid);

        if (ProcessStatusError == childstatus)
        {
            if (ECHILD != errno)
                terminate(
                    errno,
                    "Unable to check for status of child pid %jd",
                    (intmax_t) self->mChildPid);

            /* The child process is no longer active, so it makes
             * sense to proceed as if the child process should
             * be terminated. */
        }
        else if (ProcessStatusTrapped == childstatus ||
                 ProcessStatusStopped == childstatus)
        {
            debug(0, "deferred timeout child status %c", childstatus);

            self->mTether.mCycleCount = 0;
            break;
        }
        else
        {
            /* Find when the tether was last active and use it to
             * determine if a timeout has actually occurred. If
             * there was recent activity, use the time of that
             * activity to reschedule the timer in order to align
             * the timeout with the activity. */

            struct EventClockTime since;
            {
                lockMutex(&self->mTetherThread->mActivity.mMutex);
                since = self->mTetherThread->mActivity.mSince;
                unlockMutex(&self->mTetherThread->mActivity.mMutex);
            }

            if (aPollTime->eventclock.ns <
                since.eventclock.ns + tetherTimer->mPeriod.duration.ns)
            {
                lapTimeRestart(&tetherTimer->mSince, &since);
                self->mTether.mCycleCount = 0;
                break;
            }

            if (++self->mTether.mCycleCount < self->mTether.mCycleLimit)
                break;

            self->mTether.mCycleCount = self->mTether.mCycleLimit;
        }

        /* Once the timeout has expired, the timer can be cancelled because
         * there is no further need to run this state machine. */

        debug(0, "timeout after %ds", gOptions.mTimeout.mTether_s);

        tetherTimer->mPeriod = Duration(NanoSeconds(0));

        activateFdTimerTermination(self, aPollTime);

    } while (0);
}

/* -------------------------------------------------------------------------- */
static void
pollFdTimerOrphan(void                        *self_,
                  struct PollFdTimerAction    *aPollFdTimerAction_unused,
                  const struct EventClockTime *aPollTime)
{
    struct ChildMonitor *self = self_;

    ensure(sChildMonitorType == self->mType);

    /* Using PR_SET_PDEATHSIG is very attractive however the detailed
     * discussion at the end of this thread is important:
     *
     * https://bugzilla.kernel.org/show_bug.cgi?id=43300
     *
     * In the most general case, PR_SET_PDEATHSIG is useless because
     * it tracks the termination of the parent thread, not the parent
     * process. */

    if (1 == getppid())
    {
        debug(0, "orphaned");

        self->mPollFdTimerActions[POLL_FD_TIMER_ORPHAN].mPeriod =
            Duration(NanoSeconds(0));

        activateFdTimerTermination(self, aPollTime);
    }
}

/* -------------------------------------------------------------------------- */
static bool
pollfdcompletion(void                     *self_,
                 struct pollfd            *aPollFds_unused,
                 struct PollFdTimerAction *aPollFdTimer_unused)
{
    struct ChildMonitor *self = self_;

    ensure(sChildMonitorType == self->mType);

    /* Wait until the child process has terminated, and the tether thread
     * has completed. */

    return
        ! (self->mPollFds[POLL_FD_CHILD].events |
           self->mPollFds[POLL_FD_TETHER].events);
}

/* -------------------------------------------------------------------------- */
/* Child Termination
 *
 * The watchdog will receive SIGCHLD when the child process terminates,
 * though no direct indication will be received if the child process
 * performs an execv(2). The SIGCHLD signal will be delivered to the
 * event loop on a pipe, at which point the child process is known
 * to be dead. */

static void
pollFdChild(void                        *self_,
            struct pollfd               *aPollFds_ununsed,
            const struct EventClockTime *aPollTime)
{
    struct ChildMonitor *self = self_;

    ensure(sChildMonitorType == self->mType);

    /* There is a race here between receiving the indication that the
     * child process has terminated, and the other watchdog actions
     * that might be taking place to actively monitor or terminate
     * the child process. In other words, those actions might be
     * attempting to manage a child process that is already dead,
     * or declare the child process errant when it has already exited.
     *
     * Actively test the race by occasionally delaying this activity
     * when in test mode. */

    if ( ! testSleep())
    {
        struct PollEventText pollEventText;
        debug(
            1,
            "detected child %s",
            createPollEventText(
                &pollEventText,
                self->mPollFds[POLL_FD_CHILD].revents));

        ensure(self->mPollFds[POLL_FD_CHILD].events);

        char buf[1];

        switch (read(self->mPollFds[POLL_FD_CHILD].fd, buf, sizeof(buf)))
        {
        default:
            /* The child process is running again after being stopped for
             * some time. Restart the tether timeout so that the stoppage
             * is not mistaken for a failure. */

            debug(0,
                  "child pid %jd is running",
                  (intmax_t) self->mChildPid);

            restartFdTimerTether(self, aPollTime);
            break;

        case -1:
            if (EINTR != errno)
                terminate(
                    errno,
                    "Unable to read child pipe");
            break;

        case 0:
            /* The child process has terminated, so there is no longer
             * any need to monitor for SIGCHLD. */

            debug(0,
                  "child pid %jd has terminated",
                  (intmax_t) self->mChildPid);

            self->mPollFds[POLL_FD_CHILD].fd    = self->mNullPipe->mRdFile->mFd;
            self->mPollFds[POLL_FD_CHILD].events= 0;

            /* Record when the child has terminated, but do not exit
             * the event loop until all the IO has been flushed. With the
             * child terminated, no further input can be produced so indicate
             * to the tether thread that it should start flushing data now. */

            flushTetherThread(self->mTetherThread);

            /* Once the child process has terminated, start the disconnection
             * timer that sends a periodic signal to the tether thread
             * to ensure that it will not block. */

            self->mPollFdTimerActions[POLL_FD_TIMER_DISCONNECTION].mPeriod =
                Duration(NSECS(Seconds(1)));

            break;
        }
    }
}

static void
pollFdTimerChild(void                        *self_,
                 struct PollFdTimerAction    *aPollFdTimerAction_unused,
                 const struct EventClockTime *aPollTime)
{
    struct ChildMonitor *self = self_;

    ensure(sChildMonitorType == self->mType);

    debug(0, "disconnecting tether thread");

    pingTetherThread(self->mTetherThread);
}

/* -------------------------------------------------------------------------- */
static void
monitorChild(struct ChildProcess *self, struct File *aUmbilicalFile)
{
    debug(0, "start monitoring child");

    /* Remember that the child process might be in its own process group,
     * or might be in the same process group as the watchdog.
     *
     * When terminating the child process, first request that the child
     * terminate by sending it SIGTERM, and if the child does not terminate,
     * resort to sending SIGKILL. */

    struct ChildSignalPlan signalPlan[] =
    {
        {  self->mPid,  SIGTERM },
        { -self->mPgid, SIGKILL },
        { 0 }
    };

    struct Pipe nullPipe;
    if (createPipe(&nullPipe, O_CLOEXEC | O_NONBLOCK))
        terminate(
            errno,
            "Unable to create null pipe");

    /* Create a thread to use a blocking copy to transfer data from a
     * local pipe to stdout. This is primarily because SPLICE_F_NONBLOCK
     * cannot guarantee that the operation is non-blocking unless both
     * source and destination file descriptors are also themselves non-blocking.
     *
     * The child thread is used to perform a potentially blocking
     * transfer between an intermediate pipe and stdout, while
     * the main monitoring thread deals exclusively with non-blocking
     * file descriptors. */

    struct TetherThread tetherThread;
    createTetherThread(&tetherThread, &nullPipe);

    /* Divide the timeout into two cycles so that if the child process is
     * stopped, the first cycle will have a chance to detect it and
     * defer the timeout. */

    const unsigned timeoutCycles = 2;

    struct ChildMonitor childmonitor =
    {
        .mType = sChildMonitorType,

        .mChildPid     = self->mPid,
        .mNullPipe     = &nullPipe,
        .mTetherThread = &tetherThread,

        .mTermination =
        {
            .mSignalPlan   = signalPlan,
            .mSignalPeriod = Duration(
                NSECS(Seconds(gOptions.mTimeout.mSignal_s))),
        },

        .mUmbilical =
        {
            .mFile = aUmbilicalFile,
        },

        .mTether =
        {
            .mCycleCount = 0,
            .mCycleLimit = timeoutCycles,
        },

        /* Experiments at http://www.greenend.org.uk/rjk/tech/poll.html show
         * that it is best not to put too much trust in POLLHUP vs POLLIN,
         * and to treat the presence of either as a trigger to attempt to
         * read from the file descriptor.
         *
         * For the writing end of the pipe, Linux returns POLLERR if the
         * far end reader is no longer available (to match EPIPE), but
         * the documentation suggests that POLLHUP might also be reasonable
         * in this context. */

        .mPollFds =
        {
            [POLL_FD_CHILD] =
            {
                .fd     = self->mChildPipe->mRdFile->mFd,
                .events = POLL_INPUTEVENTS,
            },

            [POLL_FD_UMBILICAL] =
            {
                .fd     = aUmbilicalFile->mFd,
                .events = POLL_DISCONNECTEVENT,
            },

            [POLL_FD_TETHER] =
            {
                .fd     = tetherThread.mControlPipe.mWrFile->mFd,
                .events = POLL_DISCONNECTEVENT,
            },
        },

        .mPollFdActions =
        {
            [POLL_FD_CHILD]     = { pollFdChild },
            [POLL_FD_UMBILICAL] = { pollFdUmbilical },
            [POLL_FD_TETHER]    = { pollFdTether },
        },

        .mPollFdTimerActions =
        {
            [POLL_FD_TIMER_TETHER] =
            {
                /* Note that a zero value for gOptions.mTimeout.mTether_s will
                 * disable the tether timeout in which case the watchdog will
                 * supervise the child, but not impose any timing requirements
                 * on activity on the tether. */

                .mAction = pollFdTimerTether,
                .mSince  = EVENTCLOCKTIME_INIT,
                .mPeriod = Duration(NanoSeconds(
                    NSECS(Seconds(gOptions.mTether
                                  ? gOptions.mTimeout.mTether_s
                                  : 0)).ns / timeoutCycles)),
            },

            [POLL_FD_TIMER_UMBILICAL] =
            {
                .mAction = pollFdTimerUmbilical,
                .mSince  = EVENTCLOCKTIME_INIT,
                .mPeriod = Duration(
                    NanoSeconds(
                        NSECS(Seconds(gOptions.mTimeout.mUmbilical_s)).ns / 2)),
            },

            [POLL_FD_TIMER_ORPHAN] =
            {
                /* If requested to be aware when the watchdog becomes an orphan,
                 * check if init(8) is the parent of this process. If this is
                 * detected, start sending signals to the child to encourage it
                 * to exit. */

                .mAction = pollFdTimerOrphan,
                .mSince  = EVENTCLOCKTIME_INIT,
                .mPeriod = Duration(NSECS(Seconds(gOptions.mOrphaned ? 3 : 0))),
            },

            [POLL_FD_TIMER_TERMINATION] =
            {
                .mAction = pollFdTimerTermination,
                .mSince  = EVENTCLOCKTIME_INIT,
                .mPeriod = Duration(NanoSeconds(0)),
            },

            [POLL_FD_TIMER_DISCONNECTION] =
            {
                .mAction = pollFdTimerChild,
                .mSince  = EVENTCLOCKTIME_INIT,
                .mPeriod = Duration(NanoSeconds(0)),
            },
        },
    };

    if ( ! gOptions.mTether)
        disconnectPollFdTether(&childmonitor);

    /* Make the umbilical timer expire immediately so that the umbilical
     * process is activated to monitor the watchdog. */

    lapTimeTrigger(
        &childmonitor.mPollFdTimerActions[POLL_FD_TIMER_UMBILICAL].mSince,
        childmonitor.mPollFdTimerActions[POLL_FD_TIMER_UMBILICAL].mPeriod, 0);

    watchProcessSigCont(VoidMethod(pollFdContUmbilical, &childmonitor));

    /* It is unfortunate that O_NONBLOCK is an attribute of the underlying
     * open file, rather than of each file descriptor. Since stdin and
     * stdout are typically inherited from the parent, setting O_NONBLOCK
     * would affect all file descriptors referring to the same open file,
     so this approach cannot be employed directly. */

    for (size_t ix = 0; NUMBEROF(childmonitor.mPollFds) > ix; ++ix)
    {
        if ( ! ownFdNonBlocking(childmonitor.mPollFds[ix].fd))
            terminate(
                0,
                "Expected %s fd %d to be non-blocking",
                sPollFdNames[ix],
                childmonitor.mPollFds[ix].fd);
    }

    struct PollFd pollfd;
    if (createPollFd(
            &pollfd,

            childmonitor.mPollFds,
            childmonitor.mPollFdActions,
            sPollFdNames, POLL_FD_KINDS,

            childmonitor.mPollFdTimerActions,
            sPollFdTimerNames, POLL_FD_TIMER_KINDS,

            pollfdcompletion, &childmonitor))
        terminate(
            errno,
            "Unable to initialise polling loop");

    if (runPollFdLoop(&pollfd))
        terminate(
            errno,
            "Unable to run polling loop");

    if (closePollFd(&pollfd))
        terminate(
            errno,
            "Unable to close polling loop");

    unwatchProcessSigCont();

    closeTetherThread(&tetherThread);

    if (closePipe(&nullPipe))
        terminate(
            errno,
            "Unable to close null pipe");

    debug(0, "stop monitoring child");
}

/* -------------------------------------------------------------------------- */
static void
announceChild(pid_t aPid, struct PidFile *aPidFile, const char *aPidFileName)
{
    for (int zombie = -1; zombie; )
    {
        if (0 < zombie)
        {
            debug(0, "discarding zombie pid file '%s'", aPidFileName);

            if (closePidFile(aPidFile))
                terminate(
                    errno,
                    "Cannot close pid file '%s'", aPidFileName);
        }

        if (createPidFile(aPidFile, aPidFileName))
            terminate(
                errno,
                "Cannot create pid file '%s'", aPidFileName);

        /* It is not possible to create the pidfile and acquire a flock
         * as an atomic operation. The flock can only be acquired after
         * the pidfile exists. Since this newly created pidfile is empty,
         * it resembles an closed pidfile, and in the intervening time,
         * another process might have removed it and replaced it with
         * another. */

        if (acquireWriteLockPidFile(aPidFile))
            terminate(
                errno,
                "Cannot acquire write lock on pid file '%s'", aPidFileName);

        zombie = detectPidFileZombie(aPidFile);

        if (0 > zombie)
            terminate(
                errno,
                "Unable to obtain status of pid file '%s'", aPidFileName);
    }

    debug(0, "initialised pid file '%s'", aPidFileName);

    if (writePidFile(aPidFile, aPid))
        terminate(
            errno,
            "Cannot write to pid file '%s'", aPidFileName);

    /* The pidfile was locked on creation, and now that it
     * is completely initialised, it is ok to release
     * the flock. */

    if (releaseLockPidFile(aPidFile))
        terminate(
            errno,
            "Cannot unlock pid file '%s'", aPidFileName);
}

/* -------------------------------------------------------------------------- */
static struct ExitCode
cmdPrintPidFile(const char *aFileName)
{
    struct ExitCode exitCode = { 1 };

    struct PidFile pidFile;

    if (openPidFile(&pidFile, aFileName))
    {
        if (ENOENT != errno)
            terminate(
                errno,
                "Unable to open pid file '%s'", aFileName);
        return exitCode;
    }

    if (acquireReadLockPidFile(&pidFile))
        terminate(
            errno,
            "Unable to acquire read lock on pid file '%s'", aFileName);

    pid_t pid = readPidFile(&pidFile);

    switch (pid)
    {
    default:
        if (-1 != dprintf(STDOUT_FILENO, "%jd\n", (intmax_t) pid))
            exitCode.mStatus = 0;
        break;
    case 0:
        break;
    case -1:
        terminate(
            errno,
            "Unable to read pid file '%s'", aFileName);
    }

    FINALLY
    ({
        if (closePidFile(&pidFile))
            terminate(
                errno,
                "Unable to close pid file '%s'", aFileName);
    });

    return exitCode;
}

/* -------------------------------------------------------------------------- */
static struct ExitCode
cmdRunCommand(char **aCmd)
{
    ensure(aCmd);

    debug(0,
          "watchdog process pid %jd pgid %jd",
          (intmax_t) getpid(),
          (intmax_t) getpgid(0));

    if (ignoreProcessSigPipe())
        terminate(
            errno,
            "Unable to ignore SIGPIPE");

    /* The instance of the StdFdFiller guarantees that any further file
     * descriptors that are opened will not be mistaken for stdin,
     * stdout or stderr. */

    struct StdFdFiller stdFdFiller;

    if (createStdFdFiller(&stdFdFiller))
        terminate(
            errno,
            "Unable to create stdin, stdout, stderr filler");

    struct SocketPair umbilicalSocket;
    if (createSocketPair(&umbilicalSocket))
        terminate(
            errno,
            "Unable to create umbilical socket");

    struct ChildProcess childProcess;
    createChild(&childProcess);

    if (watchProcessChildren(VoidMethod(reapChild, &childProcess)))
        terminate(
            errno,
            "Unable to add watch on child process termination");

    struct Pipe syncPipe;
    if (createPipe(&syncPipe, 0))
        terminate(
            errno,
            "Unable to create sync pipe");

    if (forkChild(
            &childProcess, aCmd, &stdFdFiller, &syncPipe, &umbilicalSocket))
        terminate(
            errno,
            "Unable to fork child");

    /* Be prepared to deliver signals to the child process only after
     * the child exists. Before this point, these signals will cause
     * the watchdog to terminate, and the new child process will
     * notice via its synchronisation pipe. */

    if (watchProcessSignals(VoidIntMethod(killChild, &childProcess)))
        terminate(
            errno,
            "Unable to add watch on signals");

    /* Only identify the watchdog process after all the signal handlers
     * have been installed. The functional tests can use this as an
     * indicator that the watchdog is ready to run the child process.
     *
     * Although the watchdog process can be announed at this point,
     * the announcement is deferred so that it and the umbilical can
     * be announced in a single line at one point. */

    struct PidFile  pidFile_;
    struct PidFile *pidFile = 0;

    if (gOptions.mPidFile)
    {
        const char *pidFileName = gOptions.mPidFile;

        pid_t pid = gOptions.mPid;

        switch (pid)
        {
        default:
            break;
        case -1:
            pid = getpid(); break;
        case 0:
            pid = childProcess.mPid; break;
        }

        pidFile = &pidFile_;

        announceChild(pid, pidFile, pidFileName);
    }

    /* With the child process launched, close the instance of StdFdFiller
     * so that stdin, stdout and stderr become available for manipulation
     * and will not be closed multiple times. */

    if (closeStdFdFiller(&stdFdFiller))
        terminate(
            errno,
            "Unable to close stdin, stdout and stderr fillers");

    /* Discard the origin stdin file descriptor, and instead attach
     * the reading end of the tether as stdin. This means that the
     * watchdog does not contribute any more references to the
     * original stdin file table entry. */

    if (STDIN_FILENO != dup2(
            childProcess.mTetherPipe->mRdFile->mFd, STDIN_FILENO))
        terminate(
            errno,
            "Unable to dup tether pipe to stdin");

    /* Avoid closing the original stdout file descriptor only if
     * there is a need to copy the contents of the tether to it.
     * Otherwise, close the original stdout and open it as a sink so
     * that the watchdog does not contribute any more references to the
     * original stdout file table entry. */

    bool discardStdout = gOptions.mQuiet;

    if ( ! gOptions.mTether)
        discardStdout = true;
    else
    {
        switch (ownFdValid(STDOUT_FILENO))
        {
        default:
            break;

        case -1:
            terminate(
                errno,
                "Unable to check validity of stdout");

        case 0:
            discardStdout = true;
            break;
        }
    }

    if (discardStdout)
    {
        if (nullifyFd(STDOUT_FILENO))
            terminate(
                errno,
                "Unable to nullify stdout");
    }

    /* Now that the tether has been duplicated onto stdin and stdout
     * as required, it is important to close the tether to ensure that
     * the only possible references to the tether pipe remain in the
     * child process, if required, and stdin and stdout in this process. */

    closeChildTether(&childProcess);

    if (purgeProcessOrphanedFds())
        terminate(
            errno,
            "Unable to purge orphaned files");

    /* Monitor the umbilical using another process so that a failure
     * of this process can be detected independently. Only create the
     * monitoring process after all the file descriptors have been
     * purged so that the monitor does not inadvertently hold file
     * descriptors that should only be held by the child.
     *
     * Note that forkProcess() will reset all signal handlers in
     * the child process. */

    pid_t watchdogPid  = getpid();
    pid_t watchdogPgid = getpgid(0);
    pid_t umbilicalPid = forkProcess(
        gOptions.mSetPgid
        ? ForkProcessSetProcessGroup
        : ForkProcessShareProcessGroup, childProcess.mPgid);

    if (-1 == umbilicalPid)
        terminate(
            errno,
            "Unable to fork umbilical monitor");

    if ( ! umbilicalPid)
    {
        debug(0, "start monitoring umbilical process pid %jd pgid %jd",
              (intmax_t) getpid(),
              (intmax_t) getpgid(0));

        if (childProcess.mPgid)
            ensure(childProcess.mPgid == getpgid(0));
        else
            ensure(watchdogPgid == getpgid(0));

        if (STDIN_FILENO !=
            dup2(umbilicalSocket.mChildFile->mFd, STDIN_FILENO))
            terminate(
                errno,
                "Unable to dup %d to stdin", umbilicalSocket.mChildFile->mFd);

        if (STDOUT_FILENO != dup2(
                umbilicalSocket.mChildFile->mFd, STDOUT_FILENO))
            terminate(
                errno,
                "Unable to dup %d to stdout", umbilicalSocket.mChildFile->mFd);

        if (pidFile)
        {
            if (acquireReadLockPidFile(pidFile))
                terminate(
                    errno,
                    "Unable to acquire read lock on pid file '%s'",
                    pidFile->mPathName.mFileName);

            if (closePidFile(pidFile))
                terminate(
                    errno,
                    "Cannot close pid file '%s'", pidFile->mPathName.mFileName);
            pidFile = 0;
        }

        if (closePipe(&syncPipe))
            terminate(
                errno,
                "Unable to close sync pipe");

        if (closeSocketPair(&umbilicalSocket))
            terminate(
                errno,
                "Unable to close umbilical socket");

        monitorChildUmbilical(&childProcess, watchdogPid);
        _exit(EXIT_FAILURE);
    }

    if (closeSocketPairChild(&umbilicalSocket))
        terminate(
            errno,
            "Unable to close umbilical child socket");

    if (gOptions.mIdentify)
        RACE
        ({
            if (-1 == dprintf(STDOUT_FILENO,
                              "%jd %jd\n",
                              (intmax_t) getpid(), (intmax_t) umbilicalPid))
                terminate(
                    errno,
                    "Unable to print parent and umbilical pid");
        });

    /* With the child process announced, and the umbilical monitor
     * prepared, allow the child process to run the target program. */

    RACE
    ({
        static char buf[1];

        if (1 != writeFile(syncPipe.mWrFile, buf, 1))
            terminate(
                errno,
                "Unable to synchronise child process");
    });

    if (closePipe(&syncPipe))
        terminate(
            errno,
            "Unable to close sync pipe");

    if (gOptions.mIdentify)
    {
        RACE
        ({
            if (-1 == dprintf(STDOUT_FILENO,
                              "%jd\n", (intmax_t) childProcess.mPid))
                terminate(
                    errno,
                    "Unable to print child pid");
        });
    }

    /* Monitor the running child until it has either completed of
     * its own accord, or terminated. Once the child has stopped
     * running, release the pid file if one was allocated. */

    monitorChild(&childProcess, umbilicalSocket.mParentFile);

    if (unwatchProcessSignals())
        terminate(
            errno,
            "Unable to remove watch from signals");

    if (unwatchProcessChildren())
        terminate(
            errno,
            "Unable to remove watch on child process termination");

    /* Since the child process has completed, there is no further
     * need for the umbilical monitor. Kill the umbilical monitor
     * as the surest means to have it terminate. */

    if (-1 != umbilicalPid)
    {
        debug(0, "killing umbilical pid %jd", (intmax_t) umbilicalPid);

        if (kill(umbilicalPid, SIGKILL))
            terminate(
                errno,
                "Unable to kill umbilical pid %jd", (intmax_t) umbilicalPid);

        int status;
        if (reapProcess(umbilicalPid, &status))
            terminate(
                errno,
                "Unable to reap umbilical pid %jd", (intmax_t) umbilicalPid);

        debug(0,
              "reaped umbilical pid %jd status %d",
              (intmax_t) umbilicalPid, status);

        umbilicalPid = -1;
    }

    if (pidFile)
    {
        if (acquireWriteLockPidFile(pidFile))
            terminate(
                errno,
                "Cannot lock pid file '%s'", pidFile->mPathName.mFileName);

        if (closePidFile(pidFile))
            terminate(
                errno,
                "Cannot close pid file '%s'", pidFile->mPathName.mFileName);

        pidFile = 0;
    }

    /* Reap the child only after the pid file is released. This ensures
     * that any competing reader that manages to sucessfully lock and
     * read the pid file will see that the process exists. */

    debug(0, "reaping child pid %jd", (intmax_t) childProcess.mPid);

    pid_t childPid = childProcess.mPid;

    int status = closeChild(&childProcess);

    debug(0, "reaped child pid %jd status %d", (intmax_t) childPid, status);

    if (closeSocketPair(&umbilicalSocket))
        terminate(
            errno,
            "Unable to close umbilical socket");

    if (resetProcessSigPipe())
        terminate(
            errno,
            "Unable to reset SIGPIPE");

    return extractProcessExitStatus(status);
}

/* -------------------------------------------------------------------------- */
int
main(int argc, char **argv)
{
    if (Timekeeping_init())
        terminate(
            0,
            "Unable to initialise timekeeping module");

    if (Process_init(argv[0]))
        terminate(
            errno,
            "Unable to initialise process state");

    struct ExitCode exitCode;

    {
        char **cmd = processOptions(argc, argv);

        if ( ! cmd && gOptions.mPidFile)
            exitCode = cmdPrintPidFile(gOptions.mPidFile);
        else
            exitCode = cmdRunCommand(cmd);
    }

    if (Process_exit())
        terminate(
            errno,
            "Unable to finalise process state");

    if (Timekeeping_exit())
        terminate(
            0,
            "Unable to finalise timekeeping module");

    return exitCode.mStatus;
}

/* -------------------------------------------------------------------------- */
