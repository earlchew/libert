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
#ifndef ERT_UID_H
#define ERT_UID_H

#include "ert/compiler.h"

#include <sys/types.h>

ERT_BEGIN_C_SCOPE;

/* -------------------------------------------------------------------------- */
struct Ert_Uid
Ert_Uid_(uid_t aUid);

#define PRId_Ert_Uid "jd"
#define FMTd_Ert_Uid(Uid) ((intmax_t) (Uid).mUid)
struct Ert_Uid
{
#ifdef __cplusplus
    explicit Ert_Uid(uid_t aUid)
    { *this = Ert_Uid_(aUid); }
#endif

    uid_t mUid;
};

#ifndef __cplusplus
static inline struct Ert_Uid
Ert_Uid(uid_t aUid)
{
    return Ert_Uid_(aUid);
}
#endif

/* -------------------------------------------------------------------------- */
struct Ert_Gid
Ert_Gid_(gid_t aGid);

#define PRId_Ert_Gid "jd"
#define FMTd_Ert_Gid(Gid) ((intmax_t) (Gid).mGid)
struct Ert_Gid
{
#ifdef __cplusplus
    explicit Ert_Gid(gid_t aGid)
    { *this = Ert_Gid_(aGid); }
#endif

    gid_t mGid;
};

#ifndef __cplusplus
static inline struct Ert_Gid
Ert_Gid(gid_t aGid)
{
    return Ert_Gid_(aGid);
}
#endif

/* -------------------------------------------------------------------------- */

ERT_END_C_SCOPE;

#endif /* ERT_UID_H */
