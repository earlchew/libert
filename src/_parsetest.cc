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

#include "ert/parse.h"

#include <string>

#include "gtest/gtest.h"

TEST(ParseTest, ArgListCopy)
{
    struct Ert_ParseArgList  argList_;
    struct Ert_ParseArgList *argList = 0;

    EXPECT_EQ(0, ert_createParseArgListCopy(&argList_, 0));
    argList = &argList_;
    EXPECT_EQ(0U, argList->mArgc);
    EXPECT_FALSE(argList->mArgv);
    argList = ert_closeParseArgList(argList);

    {
        const char * argv[] =
        {
            0
        };

        EXPECT_EQ(0, ert_createParseArgListCopy(&argList_, argv));
        argList = &argList_;
        EXPECT_EQ(0U, argList->mArgc);
        EXPECT_FALSE(argList->mArgv[0]);
        argList = ert_closeParseArgList(argList);
    }

    {
        const char * argv[] =
        {
            "a",
            0
        };

        EXPECT_EQ(0, ert_createParseArgListCopy(&argList_, argv));
        argList = &argList_;
        EXPECT_EQ(1U, argList->mArgc);
        EXPECT_EQ(std::string("a"), argList->mArgv[0]);
        EXPECT_FALSE(argList->mArgv[1]);
        argList = ert_closeParseArgList(argList);
    }

    {
        const char * argv[] =
        {
            "a",
            "b",
            0
        };

        EXPECT_EQ(0, ert_createParseArgListCopy(&argList_, argv));
        argList = &argList_;
        EXPECT_EQ(2U, argList->mArgc);
        EXPECT_EQ(std::string("a"), argList->mArgv[0]);
        EXPECT_EQ(std::string("b"), argList->mArgv[1]);
        EXPECT_FALSE(argList->mArgv[2]);
        argList = ert_closeParseArgList(argList);
    }
}

TEST(ParseTest, ArgListCSV)
{
    struct Ert_ParseArgList  argList_;
    struct Ert_ParseArgList *argList = 0;

    EXPECT_EQ(0,  ert_createParseArgListCSV(&argList_, 0));
    argList = &argList_;
    EXPECT_EQ(0U, argList->mArgc);
    EXPECT_FALSE(argList->mArgv);
    argList = ert_closeParseArgList(argList);

    EXPECT_EQ(0,  ert_createParseArgListCSV(&argList_, ""));
    argList = &argList_;
    EXPECT_EQ(0U, argList->mArgc);
    EXPECT_FALSE(argList->mArgv[0]);
    argList = ert_closeParseArgList(argList);

    EXPECT_EQ(0,  ert_createParseArgListCSV(&argList_, " "));
    argList = &argList_;
    EXPECT_EQ(0U, argList->mArgc);
    EXPECT_FALSE(argList->mArgv[0]);
    argList = ert_closeParseArgList(argList);

    EXPECT_EQ(0,  ert_createParseArgListCSV(&argList_, "  "));
    argList = &argList_;
    EXPECT_EQ(0U, argList->mArgc);
    EXPECT_FALSE(argList->mArgv[0]);
    argList = ert_closeParseArgList(argList);

    EXPECT_EQ(0,  ert_createParseArgListCSV(&argList_, "a"));
    argList = &argList_;
    EXPECT_EQ(1U, argList->mArgc);
    EXPECT_EQ(std::string("a"), argList->mArgv[0]);
    EXPECT_FALSE(argList->mArgv[1]);
    argList = ert_closeParseArgList(argList);

    EXPECT_EQ(0,  ert_createParseArgListCSV(&argList_, " a"));
    argList = &argList_;
    EXPECT_EQ(1U, argList->mArgc);
    EXPECT_EQ(std::string("a"), argList->mArgv[0]);
    EXPECT_FALSE(argList->mArgv[1]);
    argList = ert_closeParseArgList(argList);

    EXPECT_EQ(0,  ert_createParseArgListCSV(&argList_, "a "));
    argList = &argList_;
    EXPECT_EQ(1U, argList->mArgc);
    EXPECT_EQ(std::string("a"), argList->mArgv[0]);
    EXPECT_FALSE(argList->mArgv[1]);
    argList = ert_closeParseArgList(argList);

    EXPECT_EQ(0,  ert_createParseArgListCSV(&argList_, " a "));
    argList = &argList_;
    EXPECT_EQ(1U, argList->mArgc);
    EXPECT_EQ(std::string("a"), argList->mArgv[0]);
    EXPECT_FALSE(argList->mArgv[1]);
    argList = ert_closeParseArgList(argList);

    EXPECT_EQ(0,  ert_createParseArgListCSV(&argList_, ","));
    argList = &argList_;
    EXPECT_EQ(2U, argList->mArgc);
    EXPECT_EQ(std::string(""), argList->mArgv[0]);
    EXPECT_EQ(std::string(""), argList->mArgv[1]);
    EXPECT_FALSE(argList->mArgv[2]);
    argList = ert_closeParseArgList(argList);

    EXPECT_EQ(0,  ert_createParseArgListCSV(&argList_, "  ,  "));
    argList = &argList_;
    EXPECT_EQ(2U, argList->mArgc);
    EXPECT_EQ(std::string(""), argList->mArgv[0]);
    EXPECT_EQ(std::string(""), argList->mArgv[1]);
    EXPECT_FALSE(argList->mArgv[2]);
    argList = ert_closeParseArgList(argList);

    EXPECT_EQ(0,  ert_createParseArgListCSV(&argList_, "a,"));
    argList = &argList_;
    EXPECT_EQ(2U, argList->mArgc);
    EXPECT_EQ(std::string("a"), argList->mArgv[0]);
    EXPECT_EQ(std::string(""), argList->mArgv[1]);
    EXPECT_FALSE(argList->mArgv[2]);
    argList = ert_closeParseArgList(argList);

    EXPECT_EQ(0,  ert_createParseArgListCSV(&argList_, ",b"));
    argList = &argList_;
    EXPECT_EQ(2U, argList->mArgc);
    EXPECT_EQ(std::string(""), argList->mArgv[0]);
    EXPECT_EQ(std::string("b"), argList->mArgv[1]);
    EXPECT_FALSE(argList->mArgv[2]);
    argList = ert_closeParseArgList(argList);

    EXPECT_EQ(0,  ert_createParseArgListCSV(&argList_, "a,b"));
    argList = &argList_;
    EXPECT_EQ(2U, argList->mArgc);
    EXPECT_EQ(std::string("a"), argList->mArgv[0]);
    EXPECT_EQ(std::string("b"), argList->mArgv[1]);
    EXPECT_FALSE(argList->mArgv[2]);
    argList = ert_closeParseArgList(argList);

    EXPECT_EQ(0,  ert_createParseArgListCSV(&argList_, "  a  ,  b  "));
    argList = &argList_;
    EXPECT_EQ(2U, argList->mArgc);
    EXPECT_EQ(std::string("a"), argList->mArgv[0]);
    EXPECT_EQ(std::string("b"), argList->mArgv[1]);
    EXPECT_FALSE(argList->mArgv[2]);
    argList = ert_closeParseArgList(argList);

    EXPECT_EQ(0,  ert_createParseArgListCSV(&argList_, ",,"));
    argList = &argList_;
    EXPECT_EQ(3U, argList->mArgc);
    EXPECT_EQ(std::string(""), argList->mArgv[0]);
    EXPECT_EQ(std::string(""), argList->mArgv[1]);
    EXPECT_EQ(std::string(""), argList->mArgv[2]);
    EXPECT_FALSE(argList->mArgv[3]);
    argList = ert_closeParseArgList(argList);

    EXPECT_EQ(0,  ert_createParseArgListCSV(&argList_, "  ,  ,  "));
    argList = &argList_;
    EXPECT_EQ(3U, argList->mArgc);
    EXPECT_EQ(std::string(""), argList->mArgv[0]);
    EXPECT_EQ(std::string(""), argList->mArgv[1]);
    EXPECT_EQ(std::string(""), argList->mArgv[2]);
    EXPECT_FALSE(argList->mArgv[3]);
    argList = ert_closeParseArgList(argList);

    EXPECT_EQ(0,  ert_createParseArgListCSV(&argList_, "  a  ,  b  ,  c  "));
    argList = &argList_;
    EXPECT_EQ(3U, argList->mArgc);
    EXPECT_EQ(std::string("a"), argList->mArgv[0]);
    EXPECT_EQ(std::string("b"), argList->mArgv[1]);
    EXPECT_EQ(std::string("c"), argList->mArgv[2]);
    EXPECT_FALSE(argList->mArgv[3]);
    argList = ert_closeParseArgList(argList);
}

#include "_test_.h"
