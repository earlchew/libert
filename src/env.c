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

#include "ert/env.h"
#include "ert/parse.h"
#include "ert/error.h"

#include <stdlib.h>
#include <limits.h>

/* -------------------------------------------------------------------------- */
int
ert_deleteEnv(const char *aName)
{
    int rc = -1;

    ERT_ERROR_UNLESS(
        getenv(aName),
        {
            errno = ENOENT;
        });

    ERT_ERROR_IF(
        unsetenv(aName),
        {
            if (ENOENT == errno)
                errno = EINVAL;
        });

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_getEnvString(const char *aName, const char **aValue)
{
    int rc = -1;

    ERT_ERROR_UNLESS(
        *aValue = getenv(aName),
        {
            errno = ENOENT;
        });

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
const char *
ert_setEnvString(const char *aName, const char *aValue)
{
    const char *rc = 0;

    ERT_ERROR_IF(
        setenv(aName, aValue, 1));

    const char *env;

    ERT_ERROR_IF(
        ert_getEnvString(aName, &env));

    rc = env;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_getEnvInt(const char *aName, int *aValue)
{
    int rc = -1;

    const char *env;

    ERT_ERROR_IF(
        ert_getEnvString(aName, &env));

    ERT_ERROR_IF(
        ert_parseInt(env, aValue));

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
const char *
ert_setEnvInt(const char *aName, int aValue)
{
    const char *rc = 0;

    char value[sizeof("-") + sizeof(aValue) * CHAR_BIT];

    ERT_ERROR_IF(
        0 > sprintf(value, "%d", aValue));

    const char *env = 0;

    ERT_ERROR_UNLESS(
        (env = ert_setEnvString(aName, value)));

    rc = env;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_getEnvUInt(const char *aName, unsigned *aValue)
{
    int rc = -1;

    const char *env;

    ERT_ERROR_IF(
        ert_getEnvString(aName, &env));

    ERT_ERROR_IF(
        ert_parseUInt(env, aValue));

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
const char *
ert_setEnvUInt(const char *aName, unsigned aValue)
{
    const char *rc = 0;

    char value[sizeof(aValue) * CHAR_BIT];

    ERT_ERROR_IF(
        0 > sprintf(value, "%u", aValue));

    const char *env = 0;

    ERT_ERROR_UNLESS(
        (env = ert_setEnvString(aName, value)));

    rc = env;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_getEnvUInt64(const char *aName, uint64_t *aValue)
{
    int rc = -1;

    const char *env;

    ERT_ERROR_IF(
        ert_getEnvString(aName, &env));

    ERT_ERROR_IF(
        ert_parseUInt64(env, aValue));

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
const char *
ert_setEnvUInt64(const char *aName, uint64_t aValue)
{
    const char *rc = 0;

    char value[sizeof("-") + sizeof(aValue) * CHAR_BIT];

    ERT_ERROR_IF(
        0 > sprintf(value, "%" PRIu64, aValue));

    const char *env = 0;

    ERT_ERROR_UNLESS(
        (env = ert_setEnvString(aName, value)));

    rc = env;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_getEnvPid(const char *aName, struct Ert_Pid *aValue)
{
    int rc = -1;

    const char *env;

    ERT_ERROR_IF(
        ert_getEnvString(aName, &env));

    ERT_ERROR_IF(
        ert_parsePid(env, aValue));

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
const char *
ert_setEnvPid(const char *aName, struct Ert_Pid aValue)
{
    const char *rc = 0;

    char value[sizeof("-") + sizeof(aValue) * CHAR_BIT];

    ERT_ERROR_IF(
        0 > sprintf(value, "%" PRId_Ert_Pid, FMTd_Ert_Pid(aValue)));

    const char *env = 0;

    ERT_ERROR_UNLESS(
        (env = ert_setEnvString(aName, value)));

    rc = env;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
