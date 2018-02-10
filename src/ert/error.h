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
#ifndef ERT_ERROR_H
#define ERT_ERROR_H

#include "ert/compiler.h"
#include "ert/options.h"
#include "ert/printf.h"
#include "ert/test.h"
#include "ert/macros.h"

#include <errno.h>

ERT_BEGIN_C_SCOPE;

struct Ert_ErrorModule
{
    struct Ert_ErrorModule *mModule;

    struct Ert_PrintfModule  mPrintfModule_;
    struct Ert_PrintfModule *mPrintfModule;
};

/* -------------------------------------------------------------------------- */
/* Light weight assertion
 *
 * Some <assert.h> implementations depend on malloc, whose wrapper in turn
 * references ert/error.h causing infinite recursion.
 */

#define ert_Error_assert_(aPredicate)                           \
    do                                                          \
    {                                                           \
        if (!(aPredicate))                                      \
            ert_errorAssert_(# aPredicate, __FILE__, __LINE__); \
    }                                                           \
    while (0)

void
ert_errorAssert_(const char *aPredicate, const char *aFile, unsigned aLine);

/* -------------------------------------------------------------------------- */
/* Finally Unwinding
 *
 * Use ERT_ERROR_IF() and ERROR_UNLESS() to wrap calls to functions
 * that have error returns.
 *
 * Only wrap function calls because error simulation will conditionally
 * simulate error returns from these functions.
 *
 * Do not use these to wrap destructors since the destructors will
 * be used on the error return path. Destructors should have void
 * returns to avoid inadvertentl having an error return. */

#define ERT_ERROR_IF_(Sense_, Predicate_, Message_, ...) \
    do                                                   \
    {                                                    \
        /* Do not allow error management within          \
         * ERT_FINALLY() blocks. */                      \
                                                         \
        const void *ert_finally_                         \
        __attribute__((__unused__)) = 0;                 \
                                                         \
        struct Ert_ErrorFrame frame_ =                   \
            ERT_ERRORFRAME_INIT( (Message_) );           \
                                                         \
        /* Activation of a new error frame implies       \
         * a new stack unwinding sequence. */            \
                                                         \
        ert_restartErrorFrameSequence_();                \
                                                         \
        if (ert_testFinally(&frame_) ||                  \
            Sense_ (Predicate_))                         \
        {                                                \
            __VA_ARGS__                                  \
                                                         \
            ert_addErrorFrame_(&frame_, errno);          \
            goto Ert_Error_;                             \
        }                                                \
                                                         \
        /* Stack unwinding restarts if the               \
         * function completes without error.             \
         *                                               \
         * Minor optimisation, defer restarting          \
         * the error frame sequence until the finally    \
         * block to avoid gratuitous restarts when one   \
         * error block activation and deactivation,      \
         * is followed by another error block            \
         * activation and deactivation, etc, until       \
         * the finally block is activated. */            \
                                                         \
        /* ert_restartErrorFrameSequence_(); */          \
    }                                                    \
    while (0)

#define ERT_ERROR_IF(Predicate_, ...)                  \
    ERT_ERROR_IF_(/*!!*/, Predicate_, # Predicate_, ## __VA_ARGS__)

#define ERT_ERROR_UNLESS(Predicate_, ...)              \
    ERT_ERROR_IF_(!, Predicate_, # Predicate_, ##  __VA_ARGS__)

/* -------------------------------------------------------------------------- */
#define ERT_ABORT_IF(Predicate_, ...)                   \
    ERT_UNWIND_IF_(                                     \
        terminate, ert_errorTerminate, /*!!*/,          \
        Predicate_, # Predicate_, ## __VA_ARGS__)

#define ERT_ABORT_UNLESS(Predicate_, ...)               \
    ERT_UNWIND_IF_(                                     \
        terminate, ert_errorTerminate, !,               \
        Predicate_, # Predicate_, ## __VA_ARGS__)

#define ERT_UNWIND_IF_(                                         \
    Action_, Actor_, Sense_, Predicate_, Message_, ...)         \
    do                                                          \
    {                                                           \
        /* Allow program termination to be triggered both       \
         * inside and outside ERT_FINALLY() blocks.             \
         *                                                      \
         * Mark the beginning of new frame sequences should     \
         * program termination be triggered here. */            \
                                                                \
        struct Ert_ErrorFrameSequence frameSequence_ =          \
            ert_pushErrorFrameSequence();                       \
                                                                \
        if (Sense_ (Predicate_))                                \
        {                                                       \
            ert_logErrorFrameSequence();                        \
                                                                \
            ERT_AUTO(Action_, &Actor_);                         \
                                                                \
            __VA_ARGS__                                         \
                                                                \
            do                                                  \
                (Action_)(                                      \
                    errno,                                      \
                    __func__, __FILE__, __LINE__,               \
                    "%s", (Message_));                          \
            while (0);                                          \
        }                                                       \
                                                                \
        ert_popErrorFrameSequence(frameSequence_);              \
    }                                                           \
    while (0)

/* -------------------------------------------------------------------------- */
/* Finally Block
 *
 * Split definitions across ERT_FINALLY and Ert_Finally so that the compiler
 * will produce an error if the components of the idiom are not used
 * as a pair:
 *
 * Ert_Finally:
 *
 *     ERT_FINALLY({ ... });
 */

#define Ert_Finally                             \
        int ert_finally_                        \
        __attribute__((__unused__));            \
                                                \
        /* Clean up any sequence of error       \
         * block activation and deactivation    \
         * pairs. */                            \
                                                \
        ert_restartErrorFrameSequence_();       \
                                                \
        goto Ert_Error_;                        \
                                                \
    Ert_Error_:                                 \
                                                \
        do                                      \
        {                                       \
            switch (0)                          \
            {                                   \
            default

#define ERT_FINALLY(...)                        \
                                                \
                ERT_SCOPED_ERRNO(__VA_ARGS__);  \
            }                                   \
        } while (0)

#define ert_finally_warn_if_(Sense_, Predicate_, Self_, PrintfMethod_, ...)   \
    do                                                                        \
    {                                                                         \
        if ( Sense_ (Predicate_))                                             \
        {                                                                     \
            Ert_Error_warn_(0,                                                \
                 "%" PRIs_Ert_Method                                          \
                 ERT_IFEMPTY("", " ", ERT_CAR(__VA_ARGS__))                   \
                 ERT_CAR(__VA_ARGS__),                                        \
                 FMTs_Ert_Method(Self_, PrintfMethod_)                        \
                 ERT_CDR(__VA_ARGS__));                                       \
        }                                                                     \
    } while (0)

#define ert_finally_warn_if(Predicate_, Self_, PrintfMethod_, ...)      \
    ert_finally_warn_if_(                                               \
        /*!!*/, Predicate_, Self_, PrintfMethod_, ## __VA_ARGS__)

#define ert_finally_warn_unless(Predicate_, Self_, PrintfMethod_, ...)   \
    ert_finally_warn_if_(                                                \
        !, Predicate_, Self_, PrintfMethod_, ## __VA_ARGS__)

/* -------------------------------------------------------------------------- */
#define ERT_SCOPED_ERRNO(...)                  \
    do                                         \
    {                                          \
        int ert_errno_ = errno;                \
                                               \
        /* Guard against inadvertent breaks */ \
        do { __VA_ARGS__; } while (0);         \
                                               \
        errno = ert_errno_;                    \
    } while (0)

/* -------------------------------------------------------------------------- */
struct Ert_ErrorFrame
{
    const char *mFile;
    unsigned    mLine;
    const char *mName;
    const char *mText;
    int         mErrno;
};

#define ERT_ERRORFRAME_INIT(aText) { __FILE__, __LINE__, __func__, aText }

struct Ert_ErrorFrameChunk;

struct Ert_ErrorFrameIter
{
    unsigned                    mIndex;
    struct Ert_ErrorFrame      *mFrame;
    struct Ert_ErrorFrameChunk *mChunk;
};

struct Ert_ErrorFrameSequence
{
    struct Ert_ErrorFrameIter mIter;
};

enum Ert_ErrorFrameStackKind
{
    Ert_ErrorFrameStackThread = 0,
    Ert_ErrorFrameStackSignal,
    Ert_ErrorFrameStackKinds,
};

void
ert_addErrorFrame_(const struct Ert_ErrorFrame *aFrame, int aErrno);

void
ert_restartErrorFrameSequence_(void);

int
ert_freezeErrorFrameSequence(int aFd);

int
ert_thawErrorFrameSequence(int aFd);

unsigned
ert_ownErrorFrameLevel_(void);

const struct Ert_ErrorFrame *
ert_ownErrorFrame_(enum Ert_ErrorFrameStackKind aStack, unsigned aLevel);

void
ert_logErrorFrameSequence(void);

struct Ert_ErrorFrameSequence
ert_pushErrorFrameSequence(void);

void
ert_popErrorFrameSequence(struct Ert_ErrorFrameSequence aSequence);

enum Ert_ErrorFrameStackKind
ert_switchErrorFrameStack(enum Ert_ErrorFrameStackKind aStack);

/* -------------------------------------------------------------------------- */
#ifndef __cplusplus
#define ert_breadcrumb Ert_Error_breadcrumb_
#define ert_debug      Ert_Error_debug_
#define ert_debuglevel Ert_Error_debuglevel_
#endif

#define Ert_Error_breadcrumb_() \
    ert_errorDebug(__func__, __FILE__, __LINE__, ".")

#define Ert_Error_debuglevel_(aLevel) \
    ((aLevel) < gErtOptions_.mDebug)

#define Ert_Error_debug_(aLevel, ...)                                    \
    do                                                                   \
        if (Ert_Error_debuglevel_((aLevel)))                             \
            ert_errorDebug(__func__, __FILE__, __LINE__, ## __VA_ARGS__);\
    while (0)

void
ert_errorDebug(
    const char *aFunction, const char *aFile, unsigned aLine,
    const char *aFmt, ...)
    __attribute__ ((__format__(__printf__, 4, 5)));

/* -------------------------------------------------------------------------- */
#ifndef __cplusplus
#define ert_ensure Ert_Error_ensure_
#endif

#define Ert_Error_ensure_(aPredicate)                                   \
    do                                                                  \
        if ( ! (aPredicate))                                            \
            ert_errorEnsure(__func__, __FILE__, __LINE__, # aPredicate);\
    while (0)

void
ert_errorEnsure(const char *aFunction, const char *aFile, unsigned aLine,
                const char *aPredicate)
    __attribute__ ((__noreturn__));

/* -------------------------------------------------------------------------- */
#ifndef __cplusplus
#define ert_warn Ert_Error_warn_
#endif

#define Ert_Error_warn_(aErrCode, ...) \
    ert_errorWarn((aErrCode), __func__, __FILE__, __LINE__, ## __VA_ARGS__)

void
ert_errorWarn(
    int aErrCode,
    const char *aFunction, const char *aFile, unsigned aLine,
    const char *aFmt, ...)
    __attribute__ ((__format__(__printf__, 5, 6)));

/* -------------------------------------------------------------------------- */
#ifndef __cplusplus
#define ert_message Ert_Error_message_
#endif

#define Ert_Error_message_(aErrCode, ...) \
    ert_errorMessage((aErrCode), __func__, __FILE__, __LINE__, ## __VA_ARGS__)

void
ert_errorMessage(
    int aErrCode,
    const char *aFunction, const char *aFile, unsigned aLine,
    const char *aFmt, ...)
    __attribute__ ((__format__(__printf__, 5, 6)));

/* -------------------------------------------------------------------------- */
#ifndef __cplusplus
#define ert_terminate Ert_Error_terminate_
#endif

#define Ert_Error_terminate_(aErrCode, ...) \
    ert_errorTerminate((aErrCode), __func__, __FILE__, __LINE__, ## __VA_ARGS__)

void
ert_errorTerminate(
    int aErrCode,
    const char *aFunction, const char *aFile, unsigned aLine,
    const char *aFmt, ...)
    __attribute__ ((__format__(__printf__, 5, 6), __noreturn__));

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
Ert_Error_init(struct Ert_ErrorModule *self);

ERT_CHECKED struct Ert_ErrorModule *
Ert_Error_exit(struct Ert_ErrorModule *self);

/* -------------------------------------------------------------------------- */

ERT_END_C_SCOPE;

#endif /* ERT_ERROR_H */
