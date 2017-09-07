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
#define ERT_METHOD_CTOR_(Const_, Struct_)
#else
#define ERT_METHOD_CTOR_(Const_, Struct_)               \
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
#define ERT_METHOD_TRAMPOLINE(                                           \
    Object_, Method_, Name_, Return_, Const_, ArgList_, CallList_)       \
({                                                                       \
    typedef Const_ ERT_DECLTYPE(*(Object_)) *ObjectT_;                   \
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
ert_methodEnsure_(const char *aFunction, const char *aFile, unsigned aLine,
                  const char *aPredicate)
    ERT_NORETURN;

#define ERT_METHOD_ENSURE_(aPredicate)                                      \
    do                                                                      \
        if ( ! (aPredicate))                                                \
            ert_methodEnsure_(__func__, __FILE__, __LINE__, # aPredicate);  \
    while (0);

ERT_END_C_SCOPE;

#endif /* ERT_METHOD_H */

/* -------------------------------------------------------------------------- */
#ifdef ERT_METHOD_DEFINITION
#undef ERT_METHOD_DEFINITION

#include <stdbool.h>

ERT_BEGIN_C_SCOPE;

#ifndef ERT_METHOD_TYPE_PREFIX
#define ERT_METHOD_TYPE_PREFIX
#endif

#ifndef ERT_METHOD_FUNCTION_PREFIX
#define ERT_METHOD_FUNCTION_PREFIX
#endif

#define ERT_METHOD_NAME_ CONCAT(ERT_METHOD_TYPE_PREFIX, ERT_METHOD_NAME)

typedef ERT_METHOD_RETURN (*CONCAT(ERT_METHOD_NAME_, T_))(
    ERT_METHOD_CONST void *self EXPAND(ARGS ERT_METHOD_ARG_LIST));

static __inline__ struct ERT_METHOD_NAME_
CONCAT(ERT_METHOD_NAME_, _) (ERT_METHOD_CONST void       *aObject,
                             CONCAT(ERT_METHOD_NAME_, T_) aMethod);

struct ERT_METHOD_NAME_
{
    ERT_METHOD_CTOR_(ERT_METHOD_CONST, ERT_METHOD_NAME_)

    ERT_METHOD_CONST void       *mObject;
    CONCAT(ERT_METHOD_NAME_, T_) mMethod;
};

static __inline__ struct ERT_METHOD_NAME_
CONCAT(ERT_METHOD_NAME_, _) (ERT_METHOD_CONST void       *aObject,
                             CONCAT(ERT_METHOD_NAME_, T_) aMethod)
{
    ERT_METHOD_ENSURE_(aMethod || ! aObject);

    /* In C++ programs, this initialiser will use the struct ctor, so
     * the struct ctor must not use this function to avoid death
     * by recursion. */

    return (struct ERT_METHOD_NAME_)
    {
        mObject : aObject,
        mMethod : aMethod,
    };
}

static __inline__ ERT_METHOD_RETURN
CONCAT(
    CONCAT(ERT_METHOD_FUNCTION_PREFIX, call), ERT_METHOD_NAME) (
        struct ERT_METHOD_NAME_ self
        EXPAND(ARGS ERT_METHOD_ARG_LIST))
{
    ERT_METHOD_ENSURE_(self.mMethod);

    return self.mMethod(self.mObject EXPAND(ARGS ERT_METHOD_CALL_LIST));
}

static __inline__ bool
CONCAT(CONCAT(own, ERT_METHOD_NAME_), Nil)(struct ERT_METHOD_NAME_ self)
{
    return ! self.mMethod;
}

static __inline__ struct ERT_METHOD_NAME_
CONCAT(ERT_METHOD_NAME_, Nil)(void)
{
    return CONCAT(ERT_METHOD_NAME_, _)(0, 0);
}

ERT_END_C_SCOPE;

/* -------------------------------------------------------------------------- */

#undef ERT_METHOD_NAME_

#undef ERT_METHOD_TYPE_PREFIX
#undef ERT_METHOD_FUNCTION_PREFIX

#undef ERT_METHOD_NAME
#undef ERT_METHOD_RETURN
#undef ERT_METHOD_CONST
#undef ERT_METHOD_ARG_LIST
#undef ERT_METHOD_CALL_LIST

#endif /* ERT_METHOD_DEFINITION */
