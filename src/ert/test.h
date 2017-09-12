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
#ifndef ERT_TEST_H
#define ERT_TEST_H

#include "ert/compiler.h"

#include <inttypes.h>
#include <stdbool.h>

ERT_BEGIN_C_SCOPE;

struct Ert_ErrorFrame;

struct Ert_TestModule
{
    struct Ert_TestModule *mModule;
};

/* -------------------------------------------------------------------------- */
#define ERT_TEST_RACE(...)                      \
    do                                          \
    {                                           \
        ert_testSleep(Ert_TestLevelRace);       \
        __VA_ARGS__;                            \
        ert_testSleep(Ert_TestLevelRace);       \
    } while (0)

/* -------------------------------------------------------------------------- */
enum Ert_TestLevel
{
    Ert_TestLevelNone  = 0,
    Ert_TestLevelRace  = 1,
    Ert_TestLevelError = 2,
    Ert_TestLevelSync  = 3
};

/* -------------------------------------------------------------------------- */
bool
ert_testSleep(enum Ert_TestLevel aLevel);

bool
ert_testAction(enum Ert_TestLevel aLevel);

bool
ert_testMode(enum Ert_TestLevel aLevel);

bool
ert_testFinally(const struct Ert_ErrorFrame *aFrame);

uint64_t
ert_testErrorLevel(void);

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
Ert_Test_init(
    struct Ert_TestModule *self,
    const char            *aErrorEnv);

ERT_CHECKED struct Ert_TestModule *
Ert_Test_exit(
    struct Ert_TestModule *self);

/* -------------------------------------------------------------------------- */

ERT_END_C_SCOPE;

#endif /* ERT_TEST_H */
