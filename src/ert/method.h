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
#ifndef ERT_METHOD_H
#define ERT_METHOD_H

#include "ert/compiler.h"
#include "ert/lambda.h"
#include "ert/macros.h"

ERT_BEGIN_C_SCOPE;

/* -------------------------------------------------------------------------- */
#ifndef __cplusplus
#define METHOD_CTOR_(Const_, Struct_)
#else
#define METHOD_CTOR_(Const_, Struct_)                   \
    explicit Struct_()                                  \
    : mObject(0),                                       \
      mMethod(0)                                        \
    { }                                                 \
                                                        \
    explicit Struct_(Const_ void        *aObject,       \
                     CONCAT(Struct_, T_) aMethod)       \
    : mObject(aObject),                                 \
      mMethod(aMethod)                                  \
    { }
#endif

/* -------------------------------------------------------------------------- */
#define METHOD_TRAMPOLINE(                                               \
    Object_, Method_, Name_, Return_, Const_, ArgList_, CallList_)       \
({                                                                       \
    typedef Const_ ERT_DECLTYPE(*(Object_)) *ObjectT_;                       \
                                                                         \
    Const_ void *ValidateObject_ = (Object_);                            \
                                                                         \
    Return_ (*ValidateMethod_)(ObjectT_ ARGS ArgList_) = (Method_);      \
                                                                         \
    Name_(                                                               \
        (ValidateObject_),                                               \
        ( ! ValidateMethod_                                              \
          ? 0                                                            \
          : LAMBDA(                                                      \
              Return_, (Const_ void *Self_ ARGS ArgList_),               \
              {                                                          \
                  Return_ (*method_)(                                    \
                    ObjectT_ ARGS ArgList_) = (Method_);                 \
                                                                         \
                  return method_(                                        \
                      (ObjectT_) Self_ ARGS CallList_);                  \
              })));                                                      \
})

/* -------------------------------------------------------------------------- */
void
methodEnsure_(const char *aFunction, const char *aFile, unsigned aLine,
              const char *aPredicate)
    ERT_NORETURN;

#define METHOD_ENSURE_(aPredicate)                                      \
    do                                                                  \
        if ( ! (aPredicate))                                            \
            methodEnsure_(__func__, __FILE__, __LINE__, # aPredicate);  \
    while (0);

ERT_END_C_SCOPE;

#endif /* ERT_METHOD_H */

/* -------------------------------------------------------------------------- */
#ifdef METHOD_DEFINITION
#undef METHOD_DEFINITION

#include <stdbool.h>

ERT_BEGIN_C_SCOPE;

typedef METHOD_RETURN (*CONCAT(METHOD_NAME, T_))(
    METHOD_CONST void *self EXPAND(ARGS METHOD_ARG_LIST));

static __inline__ struct METHOD_NAME
CONCAT(METHOD_NAME, _) (METHOD_CONST void      *aObject,
                        CONCAT(METHOD_NAME, T_) aMethod);

struct METHOD_NAME
{
    METHOD_CTOR_(METHOD_CONST, METHOD_NAME)

    METHOD_CONST void      *mObject;
    CONCAT(METHOD_NAME, T_) mMethod;
};

static __inline__ struct METHOD_NAME
CONCAT(METHOD_NAME, _) (METHOD_CONST void      *aObject,
                        CONCAT(METHOD_NAME, T_) aMethod)
{
    METHOD_ENSURE_(aMethod || ! aObject);

    /* In C++ programs, this initialiser will use the struct ctor, so
     * the struct ctor must not use this function to avoid death
     * by recursion. */

    return (struct METHOD_NAME)
    {
        mObject : aObject,
        mMethod : aMethod,
    };
}

static __inline__ METHOD_RETURN
CONCAT(call, METHOD_NAME) (struct METHOD_NAME self
                           EXPAND(ARGS METHOD_ARG_LIST))
{
    METHOD_ENSURE_(self.mMethod);

    return self.mMethod(self.mObject EXPAND(ARGS METHOD_CALL_LIST));
}

static __inline__ bool
CONCAT(CONCAT(own, METHOD_NAME), Nil)(struct METHOD_NAME self)
{
    return ! self.mMethod;
}

static __inline__ struct METHOD_NAME
CONCAT(METHOD_NAME, Nil)(void)
{
    return CONCAT(METHOD_NAME, _)(0, 0);
}

ERT_END_C_SCOPE;

/* -------------------------------------------------------------------------- */

#undef METHOD_NAME
#undef METHOD_RETURN
#undef METHOD_CONST
#undef METHOD_ARG_LIST
#undef METHOD_CALL_LIST

#endif /* METHOD_DEFINITION */
