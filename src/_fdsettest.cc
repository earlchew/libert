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

#include "ert/fdset.h"

#include "gtest/gtest.h"

#include <sys/resource.h>

#include <limits.h>

TEST(FdTest, RangeContains)
{
    EXPECT_EQ( 0, ert_containsFdRange(Ert_FdRange(20, 29), Ert_FdRange(10, 19)));
    EXPECT_EQ( 0, ert_containsFdRange(Ert_FdRange(20, 29), Ert_FdRange(10, 20)));
    EXPECT_EQ( 0, ert_containsFdRange(Ert_FdRange(20, 29), Ert_FdRange(10, 25)));
    EXPECT_EQ(+1, ert_containsFdRange(Ert_FdRange(20, 29), Ert_FdRange(20, 20)));
    EXPECT_EQ(+1, ert_containsFdRange(Ert_FdRange(20, 29), Ert_FdRange(20, 25)));
    EXPECT_EQ(+3, ert_containsFdRange(Ert_FdRange(20, 29), Ert_FdRange(20, 29)));
    EXPECT_EQ(-1, ert_containsFdRange(Ert_FdRange(20, 29), Ert_FdRange(21, 28)));
    EXPECT_EQ(+2, ert_containsFdRange(Ert_FdRange(20, 29), Ert_FdRange(25, 29)));
    EXPECT_EQ(+2, ert_containsFdRange(Ert_FdRange(20, 29), Ert_FdRange(29, 29)));
    EXPECT_EQ( 0, ert_containsFdRange(Ert_FdRange(20, 29), Ert_FdRange(25, 35)));
    EXPECT_EQ( 0, ert_containsFdRange(Ert_FdRange(20, 29), Ert_FdRange(30, 39)));
}

TEST(FdTest, RightNeighbour)
{
    EXPECT_FALSE(ert_rightFdRangeNeighbour(Ert_FdRange(20, 29), Ert_FdRange(10, 19)));
    EXPECT_FALSE(ert_rightFdRangeNeighbour(Ert_FdRange(20, 29), Ert_FdRange(10, 20)));
    EXPECT_FALSE(ert_rightFdRangeNeighbour(Ert_FdRange(20, 29), Ert_FdRange(10, 25)));
    EXPECT_FALSE(ert_rightFdRangeNeighbour(Ert_FdRange(20, 29), Ert_FdRange(20, 20)));
    EXPECT_FALSE(ert_rightFdRangeNeighbour(Ert_FdRange(20, 29), Ert_FdRange(20, 25)));
    EXPECT_FALSE(ert_rightFdRangeNeighbour(Ert_FdRange(20, 29), Ert_FdRange(25, 29)));
    EXPECT_FALSE(ert_rightFdRangeNeighbour(Ert_FdRange(20, 29), Ert_FdRange(29, 29)));
    EXPECT_FALSE(ert_rightFdRangeNeighbour(Ert_FdRange(20, 29), Ert_FdRange(25, 35)));
    EXPECT_TRUE(ert_rightFdRangeNeighbour(Ert_FdRange(20, 29),  Ert_FdRange(30, 39)));
    EXPECT_FALSE(ert_rightFdRangeNeighbour(Ert_FdRange(20, 29), Ert_FdRange(35, 39)));
}

TEST(FdTest, LeftNeighbour)
{
    EXPECT_FALSE(ert_leftFdRangeNeighbour(Ert_FdRange(20, 29), Ert_FdRange(10, 15)));
    EXPECT_TRUE(ert_leftFdRangeNeighbour(Ert_FdRange(20, 29),  Ert_FdRange(10, 19)));
    EXPECT_FALSE(ert_leftFdRangeNeighbour(Ert_FdRange(20, 29), Ert_FdRange(10, 20)));
    EXPECT_FALSE(ert_leftFdRangeNeighbour(Ert_FdRange(20, 29), Ert_FdRange(10, 25)));
    EXPECT_FALSE(ert_leftFdRangeNeighbour(Ert_FdRange(20, 29), Ert_FdRange(20, 20)));
    EXPECT_FALSE(ert_leftFdRangeNeighbour(Ert_FdRange(20, 29), Ert_FdRange(20, 25)));
    EXPECT_FALSE(ert_leftFdRangeNeighbour(Ert_FdRange(20, 29), Ert_FdRange(25, 29)));
    EXPECT_FALSE(ert_leftFdRangeNeighbour(Ert_FdRange(20, 29), Ert_FdRange(29, 29)));
    EXPECT_FALSE(ert_leftFdRangeNeighbour(Ert_FdRange(20, 29), Ert_FdRange(25, 35)));
    EXPECT_FALSE(ert_leftFdRangeNeighbour(Ert_FdRange(20, 29), Ert_FdRange(30, 39)));
}

TEST(FdTest, RightOf)
{
    EXPECT_FALSE(ert_rightFdRangeOf(Ert_FdRange(20, 29), Ert_FdRange(10, 19)));
    EXPECT_FALSE(ert_rightFdRangeOf(Ert_FdRange(20, 29), Ert_FdRange(10, 20)));
    EXPECT_FALSE(ert_rightFdRangeOf(Ert_FdRange(20, 29), Ert_FdRange(10, 25)));
    EXPECT_FALSE(ert_rightFdRangeOf(Ert_FdRange(20, 29), Ert_FdRange(20, 20)));
    EXPECT_FALSE(ert_rightFdRangeOf(Ert_FdRange(20, 29), Ert_FdRange(20, 25)));
    EXPECT_FALSE(ert_rightFdRangeOf(Ert_FdRange(20, 29), Ert_FdRange(25, 29)));
    EXPECT_FALSE(ert_rightFdRangeOf(Ert_FdRange(20, 29), Ert_FdRange(29, 29)));
    EXPECT_FALSE(ert_rightFdRangeOf(Ert_FdRange(20, 29), Ert_FdRange(25, 35)));
    EXPECT_TRUE(ert_rightFdRangeOf(Ert_FdRange(20, 29),  Ert_FdRange(30, 39)));
    EXPECT_TRUE(ert_rightFdRangeOf(Ert_FdRange(20, 29),  Ert_FdRange(35, 39)));
}

TEST(FdTest, LeftOf)
{
    EXPECT_TRUE(ert_leftFdRangeOf(Ert_FdRange(20, 29),  Ert_FdRange(10, 15)));
    EXPECT_TRUE(ert_leftFdRangeOf(Ert_FdRange(20, 29),  Ert_FdRange(10, 19)));
    EXPECT_FALSE(ert_leftFdRangeOf(Ert_FdRange(20, 29), Ert_FdRange(10, 20)));
    EXPECT_FALSE(ert_leftFdRangeOf(Ert_FdRange(20, 29), Ert_FdRange(10, 25)));
    EXPECT_FALSE(ert_leftFdRangeOf(Ert_FdRange(20, 29), Ert_FdRange(20, 20)));
    EXPECT_FALSE(ert_leftFdRangeOf(Ert_FdRange(20, 29), Ert_FdRange(20, 25)));
    EXPECT_FALSE(ert_leftFdRangeOf(Ert_FdRange(20, 29), Ert_FdRange(25, 29)));
    EXPECT_FALSE(ert_leftFdRangeOf(Ert_FdRange(20, 29), Ert_FdRange(29, 29)));
    EXPECT_FALSE(ert_leftFdRangeOf(Ert_FdRange(20, 29), Ert_FdRange(25, 35)));
    EXPECT_FALSE(ert_leftFdRangeOf(Ert_FdRange(20, 29), Ert_FdRange(30, 39)));
}

TEST(FdTest, CreateDestroy)
{
    struct Ert_FdSet  fdset_;
    struct Ert_FdSet *fdset = 0;

    EXPECT_EQ(0, ert_createFdSet(&fdset_));
    fdset = &fdset_;

    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(0, 2)));
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(1, 2)));
    EXPECT_EQ(EEXIST, errno);

    fdset = ert_closeFdSet(fdset);
}

TEST(FdTest, Clear)
{
    struct Ert_FdSet  fdset_;
    struct Ert_FdSet *fdset = 0;

    EXPECT_EQ(0, ert_createFdSet(&fdset_));
    fdset = &fdset_;

    struct rlimit fdLimit;
    EXPECT_EQ(0, getrlimit(RLIMIT_NOFILE, &fdLimit));

    int fdList[fdLimit.rlim_cur / 2 - 1];

    for (unsigned ix = 0; ERT_NUMBEROF(fdList) > ix; ++ix)
        fdList[ix] = 2 * ix;

    for (unsigned ix = 0; ERT_NUMBEROF(fdList) > ix; ++ix)
    {
        unsigned jx = random() % (ERT_NUMBEROF(fdList) - ix) + ix;

        ERT_SWAP(fdList[ix], fdList[jx]);
    }

    for (unsigned ix = 0; ERT_NUMBEROF(fdList) > ix; ++ix)
        EXPECT_EQ(0, ert_insertFdSetRange(fdset,
                                      Ert_FdRange(fdList[ix], fdList[ix])));

    for (unsigned ix = 0; ERT_NUMBEROF(fdList) > ix; ++ix)
    {
        errno = 0;
        EXPECT_NE(0, ert_insertFdSetRange(fdset,
                                      Ert_FdRange(fdList[ix], fdList[ix])));
        EXPECT_EQ(EEXIST, errno);
    }

    ert_clearFdSet(fdset);

    for (unsigned ix = 0; ERT_NUMBEROF(fdList) > ix; ++ix)
        EXPECT_EQ(0, ert_insertFdSetRange(fdset,
                                      Ert_FdRange(fdList[ix], fdList[ix])));

    fdset = ert_closeFdSet(fdset);
}

TEST(FdTest, InvertEmpty)
{
    struct Ert_FdSet  fdset_;
    struct Ert_FdSet *fdset = 0;

    EXPECT_EQ(0, ert_createFdSet(&fdset_));
    fdset = &fdset_;

    EXPECT_EQ(0, ert_invertFdSet(fdset));
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(0, 0)));
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(INT_MAX, INT_MAX)));
    EXPECT_EQ(0, ert_invertFdSet(fdset));
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(0, 0)));
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(INT_MAX, INT_MAX)));

    fdset = ert_closeFdSet(fdset);
}

TEST(FdTest, InvertSingle)
{
    struct Ert_FdSet  fdset_;
    struct Ert_FdSet *fdset = 0;

    EXPECT_EQ(0, ert_createFdSet(&fdset_));
    fdset = &fdset_;

    /* Single left side fd */
    ert_clearFdSet(fdset);
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(0, 0)));
    EXPECT_EQ(0, ert_invertFdSet(fdset));
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(1, 1)));
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(INT_MAX, INT_MAX)));
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(0, 0)));

    /* Range left side fd */
    ert_clearFdSet(fdset);
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(0, 1)));
    EXPECT_EQ(0, ert_invertFdSet(fdset));
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(2, 2)));
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(INT_MAX, INT_MAX)));
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(0, 1)));

    /* Single right side fd */
    ert_clearFdSet(fdset);
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(INT_MAX, INT_MAX)));
    EXPECT_EQ(0, ert_invertFdSet(fdset));
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(0, 0)));
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(INT_MAX-1, INT_MAX-1)));
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(INT_MAX, INT_MAX)));

    /* Range right side fd */
    ert_clearFdSet(fdset);
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(INT_MAX-1, INT_MAX)));
    EXPECT_EQ(0, ert_invertFdSet(fdset));
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(0, 0)));
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(INT_MAX-2, INT_MAX-2)));
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(INT_MAX-1, INT_MAX)));

    /* Two and three ranges */
    ert_clearFdSet(fdset);
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(0, 1)));
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(INT_MAX-1, INT_MAX)));
    EXPECT_EQ(0, ert_invertFdSet(fdset));
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(2, 2)));
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(INT_MAX-2, INT_MAX-2)));
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(0, 0)));
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(INT_MAX, INT_MAX)));
    EXPECT_EQ(0, ert_removeFdSetRange(fdset, Ert_FdRange(0, 0)));
    EXPECT_EQ(0, ert_removeFdSetRange(fdset, Ert_FdRange(INT_MAX, INT_MAX)));

    EXPECT_EQ(0, ert_invertFdSet(fdset));

    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(0, 0)));
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(1, 1)));
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(INT_MAX-1, INT_MAX-1)));
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(INT_MAX, INT_MAX)));

    fdset = ert_closeFdSet(fdset);
}

TEST(FdTest, InsertRemove)
{
    struct Ert_FdSet  fdset_;
    struct Ert_FdSet *fdset = 0;

    EXPECT_EQ(0, ert_createFdSet(&fdset_));
    fdset = &fdset_;

    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(0, 2)));
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(0, 0)));
    EXPECT_EQ(0, ert_removeFdSetRange(fdset, Ert_FdRange(0, 2)));
    EXPECT_NE(0, ert_removeFdSetRange(fdset, Ert_FdRange(0, 0)));
    EXPECT_NE(0, ert_removeFdSetRange(fdset, Ert_FdRange(0, 2)));
    EXPECT_EQ(ENOENT, errno);

    fdset = ert_closeFdSet(fdset);
}

TEST(FdTest, InsertOverlap)
{
    struct Ert_FdSet  fdset_;
    struct Ert_FdSet *fdset = 0;

    EXPECT_EQ(0, ert_createFdSet(&fdset_));
    fdset = &fdset_;

    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(0, 2)));
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(4, 6)));

    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(0, 1)));
    EXPECT_EQ(EEXIST, errno);
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(0, 2)));
    EXPECT_EQ(EEXIST, errno);
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(0, 3)));
    EXPECT_EQ(EEXIST, errno);

    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(0, 1)));
    EXPECT_EQ(EEXIST, errno);
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(0, 2)));
    EXPECT_EQ(EEXIST, errno);
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(0, 3)));
    EXPECT_EQ(EEXIST, errno);

    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(1, 2)));
    EXPECT_EQ(EEXIST, errno);
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(1, 3)));
    EXPECT_EQ(EEXIST, errno);

    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(3, 4)));
    EXPECT_EQ(EEXIST, errno);
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(3, 5)));
    EXPECT_EQ(EEXIST, errno);
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(3, 6)));
    EXPECT_EQ(EEXIST, errno);
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(3, 7)));
    EXPECT_EQ(EEXIST, errno);

    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(4, 4)));
    EXPECT_EQ(EEXIST, errno);
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(4, 5)));
    EXPECT_EQ(EEXIST, errno);
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(4, 6)));
    EXPECT_EQ(EEXIST, errno);
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(4, 7)));
    EXPECT_EQ(EEXIST, errno);

    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(5, 6)));
    EXPECT_EQ(EEXIST, errno);
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(5, 7)));
    EXPECT_EQ(EEXIST, errno);

    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(6, 6)));
    EXPECT_EQ(EEXIST, errno);
    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(6, 7)));
    EXPECT_EQ(EEXIST, errno);

    fdset = ert_closeFdSet(fdset);
}

TEST(FdTest, RemoveOverlap)
{
    struct Ert_FdSet  fdset_;
    struct Ert_FdSet *fdset = 0;

    EXPECT_EQ(0, ert_createFdSet(&fdset_));
    fdset = &fdset_;

    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(0, 2)));
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(4, 6)));
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(8, 10)));

    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(0, 1)));
    EXPECT_EQ(0, ert_removeFdSetRange(fdset, Ert_FdRange(0, 1)));
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(0, 1)));

    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(0, 2)));
    EXPECT_EQ(0, ert_removeFdSetRange(fdset, Ert_FdRange(0, 2)));
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(0, 2)));

    EXPECT_NE(0, ert_insertFdSetRange(fdset, Ert_FdRange(1, 2)));
    EXPECT_EQ(0, ert_removeFdSetRange(fdset, Ert_FdRange(1, 2)));
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(1, 2)));

    EXPECT_NE(0, ert_removeFdSetRange(fdset, Ert_FdRange(3, 3)));
    EXPECT_NE(0, ert_removeFdSetRange(fdset, Ert_FdRange(3, 4)));
    EXPECT_NE(0, ert_removeFdSetRange(fdset, Ert_FdRange(3, 5)));
    EXPECT_NE(0, ert_removeFdSetRange(fdset, Ert_FdRange(3, 6)));
    EXPECT_NE(0, ert_removeFdSetRange(fdset, Ert_FdRange(3, 7)));

    EXPECT_EQ(0, ert_removeFdSetRange(fdset, Ert_FdRange(4, 4)));
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(4, 4)));

    EXPECT_EQ(0, ert_removeFdSetRange(fdset, Ert_FdRange(4, 5)));
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(4, 5)));

    EXPECT_EQ(0, ert_removeFdSetRange(fdset, Ert_FdRange(5, 6)));
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(5, 6)));

    EXPECT_EQ(0, ert_removeFdSetRange(fdset, Ert_FdRange(6, 6)));
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(6, 6)));

    EXPECT_EQ(0, ert_removeFdSetRange(fdset, Ert_FdRange(8, 8)));
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(8, 8)));

    EXPECT_EQ(0, ert_removeFdSetRange(fdset, Ert_FdRange(8, 9)));
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(8, 9)));

    EXPECT_EQ(0, ert_removeFdSetRange(fdset, Ert_FdRange(9, 10)));
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(9, 10)));

    EXPECT_EQ(0, ert_removeFdSetRange(fdset, Ert_FdRange(10, 10)));
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(10, 10)));

    fdset = ert_closeFdSet(fdset);
}

struct TestVisitor
{
    int mNext;
    int mStop;

    static int
    visit(struct TestVisitor *self, struct Ert_FdRange aRange)
    {
        EXPECT_EQ(aRange.mLhs, aRange.mRhs);
        EXPECT_EQ(self->mNext, aRange.mLhs);

        if (aRange.mLhs == self->mStop)
            return 1;

        self->mNext += 2;

        return 0;
    }
};

TEST(FdTest, Visitor)
{
    struct Ert_FdSet  fdset_;
    struct Ert_FdSet *fdset = 0;

    EXPECT_EQ(0, ert_createFdSet(&fdset_));
    fdset = &fdset_;

    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(0, 0)));
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(2, 2)));
    EXPECT_EQ(0, ert_insertFdSetRange(fdset, Ert_FdRange(4, 4)));

    struct TestVisitor testVisitor;

    testVisitor.mNext = 0;
    testVisitor.mStop = -1;

    EXPECT_EQ(
        3,
        ert_visitFdSet(fdset,
                       Ert_FdSetVisitor(&testVisitor, TestVisitor::visit)));

    EXPECT_EQ(6, testVisitor.mNext);

    testVisitor.mNext = 0;
    testVisitor.mStop = 2;

    EXPECT_EQ(
        2,
        ert_visitFdSet(fdset,
                       Ert_FdSetVisitor(&testVisitor, TestVisitor::visit)));

    EXPECT_EQ(2, testVisitor.mNext);

    fdset = ert_closeFdSet(fdset);
}

#include "../googletest/src/gtest_main.cc"
