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
#ifndef COMPILER_H
#define COMPILER_H

#include "macros_.h"

/* -------------------------------------------------------------------------- */
/* Scoping in C++
 *
 * Provide a clean way to scope areas containing C declarations when using
 * a C++ compiler.
 */
#ifdef __cplusplus
#define BEGIN_C_SCOPE struct CppScope_; extern "C" { struct CScope_
#define END_C_SCOPE   } struct CppScope_
#else
#define BEGIN_C_SCOPE struct CScope_
#define END_C_SCOPE   struct CScope_
#endif

/* -------------------------------------------------------------------------- */
/* Instance Counter
 *
 * Provide a way to generate unique names by having a instance counter.
 * If this is not available, the current line number is usually a good
 * enough fallback.
 */

#ifdef __COUNTER__
#define COUNTER __COUNTER__
#else
#error
#define COUNTER __LINE__
#endif

/* -------------------------------------------------------------------------- */
/* Ordered Initialisation
 *
 * Provide a way to order the initialisation of modules in libraries
 * and executables.
 */

#define EARLY_INITIALISER(Name_, Ctor_, Dtor_)  \
    EARLY_INITIALISER_(Name_, Ctor_, Dtor_)

#define EARLY_INITIALISER_(Name_, Ctor_, Dtor_) \
                                                \
static __attribute__((__constructor__(101)))    \
void Name_ ## _ctor_(void)                      \
{                                               \
    do                                          \
        EXPAND Ctor_                            \
    while (0);                                  \
}                                               \
                                                \
static __attribute__((__destructor__(101)))     \
void Name_ ## _dtor_(void)                      \
{                                               \
    do                                          \
        EXPAND Dtor_                            \
    while (0);                                  \
}                                               \
                                                \
struct EarlyInit

/* -------------------------------------------------------------------------- */
/* No Return
 *
 * Mark a function as not returning. This only makes sense if the function
 * has a void return. */

#define NORETURN __attribute__((__noreturn__))

/* -------------------------------------------------------------------------- */
/* Checked Return Value
 *
 * Where there are return code that need to be checked, this decorator
 * is used to have the compiler enforce policy. */

#define CHECKED __attribute__((__warn_unused_result__))

/* -------------------------------------------------------------------------- */
/* Unused Object
 *
 * This decorator is used to mark variables and functions that are defined
 * but not used. */

#define UNUSED __attribute__((__unused__))

/* -------------------------------------------------------------------------- */
/* Deprecated Object
 *
 * This decorator is used to mark variables and functions whose use is not
 * longer advised. */

#define DEPRECATED     __attribute__((__deprecated__))
#define NOTIMPLEMENTED __attribute__((__deprecated__))

/* -------------------------------------------------------------------------- */
/* Abort
 *
 * Prefer to have the application call abortProcess() directly rather than
 * abort(). See abort_.c for the rationale. */

static __inline__ void
abort_(void)
    __attribute__((__deprecated__));

static __inline__ void
abort_(void)
{ }

#define abort(Arg_) IFEMPTY(abort_(), abort(Arg_), Arg_)

/* -------------------------------------------------------------------------- */
/* Type Introspection
 *
 * Retrieve the type of the expression. This can use decltype() directly
 * in C++ implementations, otherwise __typeof__. */

#ifndef  __cplusplus

#define DECLTYPE(Expr_) __typeof__((Expr_))

#else

#include <type_traits>
#define DECLTYPE(Expr_) std::remove_reference<decltype((Expr_))>::type

#endif

/* -------------------------------------------------------------------------- */
/* Type Inferred Variables
 *
 * Create a variable of type that matches the initialising expression. Use
 * auto directly in C++ implementations, otherwise __typeof__. */

#ifndef __cplusplus

#define AUTO(Var_, Value_) DECLTYPE((Value_)) Var_ = (Value_)

#else

#define AUTO(Var_, Value_) auto Var_ = (Value_)

#endif

#endif /* COMPILER_H */