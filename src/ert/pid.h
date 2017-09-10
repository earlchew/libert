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
#ifndef ERT_PID_H
#define ERT_PID_H

#include "ert/compiler.h"

#include <sys/types.h>

ERT_BEGIN_C_SCOPE;

/* -------------------------------------------------------------------------- */
struct Ert_Tid
Ert_Tid_(pid_t aTid);

#define PRId_Ert_Tid "jd"
#define FMTd_Ert_Tid(Tid) ((intmax_t) (Tid).mTid)
struct Ert_Tid
{
#ifdef __cplusplus
    explicit Ert_Tid()
    { *this = Ert_Tid_(0); }

    explicit Ert_Tid(pid_t aTid)
    { *this = Ert_Tid_(aTid); }
#endif

    pid_t mTid;
};

#ifndef __cplusplus
static inline struct Ert_Tid
Ert_Tid(pid_t aTid)
{
    return Ert_Tid_(aTid);
}
#endif

/* -------------------------------------------------------------------------- */
struct Ert_Pid
Ert_Pid_(pid_t aPid);

#define PRId_Ert_Pid "jd"
#define FMTd_Ert_Pid(Pid) ((intmax_t) (Pid).mPid)
struct Ert_Pid
{
#ifdef __cplusplus
    explicit Ert_Pid()
    { *this = Ert_Pid_(0); }

    explicit Ert_Pid(pid_t aPid)
    { *this = Ert_Pid_(aPid); }
#endif

    pid_t mPid;
};

#ifndef __cplusplus
static inline struct Ert_Pid
Ert_Pid(pid_t aPid)
{
    return Ert_Pid_(aPid);
}
#endif

/* -------------------------------------------------------------------------- */
struct Ert_Pgid
Ert_Pgid_(pid_t aPgid);

#define PRId_Ert_Pgid "jd"
#define FMTd_Ert_Pgid(Pgid) ((intmax_t) (Pgid).mPgid)
struct Ert_Pgid
{
#ifdef __cplusplus
    explicit Ert_Pgid()
    { *this = Ert_Pgid_(0); }

    explicit Ert_Pgid(pid_t aPgid)
    { *this = Ert_Pgid_(aPgid); }
#endif

    pid_t mPgid;
};

#ifndef __cplusplus
static inline struct Ert_Pgid
Ert_Pgid(pid_t aPgid)
{
    return Ert_Pgid_(aPgid);
}
#endif

/* -------------------------------------------------------------------------- */

ERT_END_C_SCOPE;

#endif /* ERT_PID_H */
