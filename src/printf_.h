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
#ifndef PRINTF_H
#define PRINTF_H

#include "ert/compiler.h"
#include "ert/method.h"

#include <stdio.h>
#include <stdarg.h>

/* -------------------------------------------------------------------------- */
#define METHOD_DEFINITION
#define METHOD_RETURN_PrintfMethod    int
#define METHOD_CONST_PrintfMethod     const
#define METHOD_ARG_LIST_PrintfMethod  (FILE *aFile_)
#define METHOD_CALL_LIST_PrintfMethod (aFile_)

#define METHOD_NAME      PrintfMethod_
#define METHOD_RETURN    METHOD_RETURN_PrintfMethod
#define METHOD_CONST     METHOD_CONST_PrintfMethod
#define METHOD_ARG_LIST  METHOD_ARG_LIST_PrintfMethod
#define METHOD_CALL_LIST METHOD_CALL_LIST_PrintfMethod
#include "ert/method.h"

#define PrintfMethod_(Object_, Method_)         \
    METHOD_TRAMPOLINE(                          \
        Object_, Method_,                       \
        PrintfMethod__,                         \
        METHOD_RETURN_PrintfMethod,             \
        METHOD_CONST_PrintfMethod,              \
        METHOD_ARG_LIST_PrintfMethod,           \
        METHOD_CALL_LIST_PrintfMethod)

/* -------------------------------------------------------------------------- */
BEGIN_C_SCOPE;

struct Type;

struct PrintfModule
{
    struct PrintfModule *mModule;
};

/* -------------------------------------------------------------------------- */
extern const struct Type * const printfMethodType_;

struct PrintfMethod
{
    const struct Type * const *mType;

    struct PrintfMethod_ mMethod;
};

/* The C compiler is quite willing to take the address of the temporary
 * without emitting a warning, but the C++ compiler generates a warning.
 * This context is safe because the temporary is only required within
 * the context of the full-expression, during which time the temporary
 * remains alive. Provide a helper function to silence the warning in
 * this context. */

#ifndef __cplusplus
#define PrintfMethodPtr_(aMethod) (&(aMethod))
#else
static inline
const struct PrintfMethod *
PrintfMethodPtr_(const struct PrintfMethod &aMethod)
{
    return &aMethod;
}
#endif

#define PrintfMethod(Object_, Method_)                          \
(                                                               \
    PrintfMethodPtr_(                                           \
        ((struct PrintfMethod)                                  \
        {                                                       \
            mType   : &printfMethodType_,                       \
            mMethod : PrintfMethod_((Object_), (Method_)),      \
        }))                                                     \
)

/* -------------------------------------------------------------------------- */
/* Accumulate Printed Output Count
 *
 * The printf() family of functions return a negative result on error,
 * otherwise a count of the number of characters printed. When output
 * comprises a series of printf() calls, it is necessary to sum the
 * contributions, but at the same time watching for an error. */

#define PRINTF(IOCount_, Expr_)                 \
    ({                                          \
        int io_ = (Expr_);                      \
                                                \
        0 > io_ ? io_ : (IOCount_ += io_, 0);   \
    })

/* -------------------------------------------------------------------------- */
#define PRIs_Method "%%p<struct PrintfMethod>%%"
#define FMTs_Method(Object_, Method_) ( PrintfMethod((Object_), (Method_)) )

/* -------------------------------------------------------------------------- */
int
xprintf(const char *aFmt, ...)
    __attribute__ ((__format__(__printf__, 1, 2)));

int
xfprintf(FILE *aFile, const char *aFmt, ...)
    __attribute__ ((__format__(__printf__, 2, 3)));

CHECKED int
xsnprintf(char *aBuf, size_t aSize, const char *aFmt, ...)
    __attribute__ ((__format__(__printf__, 3, 4)));

int
xdprintf(int aFd, const char *aFmt, ...)
    __attribute__ ((__format__(__printf__, 2, 3)));

/* -------------------------------------------------------------------------- */
int
xvfprintf(FILE *aFile, const char *aFmt, va_list);

CHECKED int
xvsnprintf(char *aBuf, size_t aSize, const char *aFmt, va_list);

int
xvdprintf(int aFd, const char *aFmt, va_list);

/* -------------------------------------------------------------------------- */
CHECKED int
Printf_init(struct PrintfModule *self);

CHECKED struct PrintfModule *
Printf_exit(struct PrintfModule *self);

/* -------------------------------------------------------------------------- */

END_C_SCOPE;

#endif /* PRINTF_H */
