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
#ifndef ERT_PRINTF_H
#define ERT_PRINTF_H

#include "ert/compiler.h"
#include "ert/method.h"

#include <stdio.h>
#include <stdarg.h>

/* -------------------------------------------------------------------------- */
#define ERT_METHOD_DEFINITION
#define ERT_METHOD_RETURN_PrintfMethod    int
#define ERT_METHOD_CONST_PrintfMethod     const
#define ERT_METHOD_ARG_LIST_PrintfMethod  (FILE *aFile_)
#define ERT_METHOD_CALL_LIST_PrintfMethod (aFile_)

#define ERT_METHOD_TYPE_PREFIX     Ert_
#define ERT_METHOD_FUNCTION_PREFIX ert_

#define ERT_METHOD_NAME      PrintfMethod_
#define ERT_METHOD_RETURN    ERT_METHOD_RETURN_PrintfMethod
#define ERT_METHOD_CONST     ERT_METHOD_CONST_PrintfMethod
#define ERT_METHOD_ARG_LIST  ERT_METHOD_ARG_LIST_PrintfMethod
#define ERT_METHOD_CALL_LIST ERT_METHOD_CALL_LIST_PrintfMethod
#include "ert/method.h"

#define Ert_PrintfMethod_(Object_, Method_)     \
    ERT_METHOD_TRAMPOLINE(                      \
        Object_, Method_,                       \
        Ert_PrintfMethod__,                     \
        ERT_METHOD_RETURN_PrintfMethod,         \
        ERT_METHOD_CONST_PrintfMethod,          \
        ERT_METHOD_ARG_LIST_PrintfMethod,       \
        ERT_METHOD_CALL_LIST_PrintfMethod)

/* -------------------------------------------------------------------------- */
ERT_BEGIN_C_SCOPE;

struct Ert_Type;

struct Ert_PrintfModule
{
    struct Ert_PrintfModule *mModule;
};

/* -------------------------------------------------------------------------- */
extern const struct Ert_Type * const ert_printfMethodType_;

struct Ert_PrintfMethod
{
    const struct Ert_Type * const *mType;

    struct Ert_PrintfMethod_ mMethod;
};

/* The C compiler is quite willing to take the address of the temporary
 * without emitting a warning, but the C++ compiler generates a warning.
 * This context is safe because the temporary is only required within
 * the context of the full-expression, during which time the temporary
 * remains alive. Provide a helper function to silence the warning in
 * this context. */

#ifndef __cplusplus
#define Ert_PrintfMethodPtr_(aMethod) (&(aMethod))
#else
static inline
const struct Ert_PrintfMethod *
Ert_PrintfMethodPtr_(const struct Ert_PrintfMethod &aMethod)
{
    return &aMethod;
}
#endif

#define Ert_PrintfMethod(Object_, Method_)                      \
(                                                               \
    Ert_PrintfMethodPtr_(                                       \
        ((struct Ert_PrintfMethod)                              \
        {                                                       \
            mType   : &ert_printfMethodType_,                   \
            mMethod : Ert_PrintfMethod_((Object_), (Method_)),  \
        }))                                                     \
)

/* -------------------------------------------------------------------------- */
/* Accumulate Printed Output Count
 *
 * The printf() family of functions return a negative result on error,
 * otherwise a count of the number of characters printed. When output
 * comprises a series of printf() calls, it is necessary to sum the
 * contributions, but at the same time watching for an error. */

#define ERT_PRINTF(IOCount_, Expr_)             \
    ({                                          \
        int io_ = (Expr_);                      \
                                                \
        0 > io_ ? io_ : (IOCount_ += io_, 0);   \
    })

/* -------------------------------------------------------------------------- */
#define PRIs_Ert_Method "%%p<struct Ert_PrintfMethod>%%"
#define FMTs_Ert_Method(Object_, Method_) ( \
        Ert_PrintfMethod((Object_), (Method_)) )

/* -------------------------------------------------------------------------- */
int
ert_printf(const char *aFmt, ...)
    __attribute__ ((__format__(__printf__, 1, 2)));

int
ert_fprintf(FILE *aFile, const char *aFmt, ...)
    __attribute__ ((__format__(__printf__, 2, 3)));

ERT_CHECKED int
ert_snprintf(char *aBuf, size_t aSize, const char *aFmt, ...)
    __attribute__ ((__format__(__printf__, 3, 4)));

int
ert_dprintf(int aFd, const char *aFmt, ...)
    __attribute__ ((__format__(__printf__, 2, 3)));

/* -------------------------------------------------------------------------- */
int
ert_vfprintf(FILE *aFile, const char *aFmt, va_list);

ERT_CHECKED int
ert_vsnprintf(char *aBuf, size_t aSize, const char *aFmt, va_list);

int
ert_vdprintf(int aFd, const char *aFmt, va_list);

/* -------------------------------------------------------------------------- */
ERT_CHECKED int
Ert_Printf_init(struct Ert_PrintfModule *self);

ERT_CHECKED struct Ert_PrintfModule *
Ert_Printf_exit(struct Ert_PrintfModule *self);

/* -------------------------------------------------------------------------- */

ERT_END_C_SCOPE;

#endif /* ERT_PRINTF_H */
