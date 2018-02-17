/* -*- c-basic-offset:4; indent-tabs-mode:nil -*- vi: set sw=4 et: */
/*
// Copyright (c) 2018, Earl Chew
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
#ifndef ERT_TEST_H_
#define ERT_TEST_H_

#include "ert/options.h"
#include "ert/test.h"
#include "ert/error.h"

#include "gtest/gtest.h"

#include <cstdio>

class TestEventListener : public testing::EmptyTestEventListener
{
    virtual void OnTestPartResult(
        const testing::TestPartResult &aTestPartResult)
    {
        if ( ! aTestPartResult.passed())
        {
            struct Ert_ErrorFileDescriptor stdOut =
                ERT_ERRORFILEDESCRIPTOR_INIT(fileno(stdout));

            std::cout << "\n++ Error frame sequence start" << std::endl;
            ert_logErrorFrameSequence(&stdOut);
            std::cout << "-- Error frame sequence end\n" << std::endl;
        }
    }
};

GTEST_API_ int main(int argc, char **argv)
{
    ert_initOptions(
        & ((const struct Ert_Options)
        {
            mDebug : 0,
            mTest : Ert_TestLevelRace
        }));

    testing::InitGoogleTest(&argc, argv);

    testing::UnitTest::GetInstance()->listeners().Append(
        new TestEventListener());

    return RUN_ALL_TESTS();
}
#endif /* ERT_TEST_H_ */
