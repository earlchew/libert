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

#include "ert/method.h"

#include "gtest/gtest.h"

/* -------------------------------------------------------------------------- */
struct Test { };

/* -------------------------------------------------------------------------- */
#define ERT_METHOD_DEFINITION
#define ERT_METHOD_RETURN_TestMethod    int
#define ERT_METHOD_CONST_TestMethod
#define ERT_METHOD_ARG_LIST_TestMethod  (int *aValue_)
#define ERT_METHOD_CALL_LIST_TestMethod (aValue_)

#define ERT_METHOD_NAME      TestMethod
#define ERT_METHOD_RETURN    ERT_METHOD_RETURN_TestMethod
#define ERT_METHOD_CONST     ERT_METHOD_CONST_TestMethod
#define ERT_METHOD_ARG_LIST  ERT_METHOD_ARG_LIST_TestMethod
#define ERT_METHOD_CALL_LIST ERT_METHOD_CALL_LIST_TestMethod
#include "ert/method.h"

#define TestMethod(Method_, Object_)         \
    ERT_METHOD_TRAMPOLINE(                       \
        Method_, Object_,                    \
        TestMethod_,                         \
        ERT_METHOD_RETURN_TestMethod,            \
        ERT_METHOD_CONST_TestMethod,             \
        ERT_METHOD_ARG_LIST_TestMethod,          \
        ERT_METHOD_CALL_LIST_TestMethod)

/* -------------------------------------------------------------------------- */
#define ERT_METHOD_DEFINITION
#define ERT_METHOD_RETURN_ConstTestMethod    int
#define ERT_METHOD_CONST_ConstTestMethod     const
#define ERT_METHOD_ARG_LIST_ConstTestMethod  (const int *aValue_)
#define ERT_METHOD_CALL_LIST_ConstTestMethod (aValue_)

#define ERT_METHOD_NAME      ConstTestMethod
#define ERT_METHOD_RETURN    ERT_METHOD_RETURN_ConstTestMethod
#define ERT_METHOD_CONST     ERT_METHOD_CONST_ConstTestMethod
#define ERT_METHOD_ARG_LIST  ERT_METHOD_ARG_LIST_ConstTestMethod
#define ERT_METHOD_CALL_LIST ERT_METHOD_CALL_LIST_ConstTestMethod
#include "ert/method.h"

#define ConstTestMethod(Method_, Object_)    \
    ERT_METHOD_TRAMPOLINE(                   \
        Method_, Object_,                    \
        ConstTestMethod_,                    \
        ERT_METHOD_RETURN_ConstTestMethod,   \
        ERT_METHOD_CONST_ConstTestMethod,    \
        ERT_METHOD_ARG_LIST_ConstTestMethod, \
        ERT_METHOD_CALL_LIST_ConstTestMethod)

/* -------------------------------------------------------------------------- */
struct TestMethodContext
{
    int mValue;
};

/* -------------------------------------------------------------------------- */
static int
calledTestMethod(struct TestMethodContext *self, int *aValue)
{
    return self->mValue + *aValue;
}

TEST(MethodTest, CallMethod)
{
    struct TestMethodContext testMethodContext =
    {
        mValue : 2,
    };

    struct TestMethodContext *testMethodContextPtr =
        &testMethodContext;

    struct TestMethod testMethod =
        TestMethod(
            testMethodContextPtr,
            calledTestMethod);

    int value = 3;

    int *valuePtr = &value;

    EXPECT_EQ(5, callTestMethod(testMethod, valuePtr));
}

/* -------------------------------------------------------------------------- */
static int
calledConstTestMethod(const struct TestMethodContext *self, const int *aValue)
{
    return self->mValue + *aValue;
}

TEST(MethodTest, CallConstMethod)
{
    struct TestMethodContext testMethodContext =
    {
        mValue : 2,
    };

    const struct TestMethodContext *constTestMethodContextPtr =
        &testMethodContext;

    struct ConstTestMethod constTestMethod =
        ConstTestMethod(
            constTestMethodContextPtr,
            calledConstTestMethod);

    int value = 3;

    const int *constValuePtr = &value;

    EXPECT_EQ(5, callConstTestMethod(constTestMethod, constValuePtr));
}

#include "_test_.h"
