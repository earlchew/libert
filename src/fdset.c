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
#include "ert/file.h"
#include "ert/error.h"

#include <stdlib.h>
#include <limits.h>

/* -------------------------------------------------------------------------- */
static int
ert_rankFdSetElement_(
    struct Ert_FdSetElement_ *aLhs, struct Ert_FdSetElement_ *aRhs)
{
    int rank =
        ( (aLhs->mRange.mLhs > aRhs->mRange.mLhs) -
          (aLhs->mRange.mLhs < aRhs->mRange.mLhs) );

    if ( ! rank)
        rank =
            ( (aLhs->mRange.mRhs > aRhs->mRange.mRhs) -
              (aLhs->mRange.mRhs < aRhs->mRange.mRhs) );

    return rank;
}

RB_GENERATE_STATIC(
    Ert_FdSetTree_, Ert_FdSetElement_, mTree, ert_rankFdSetElement_)

/* -------------------------------------------------------------------------- */
int
ert_printFdSet(const struct Ert_FdSet *self, FILE *aFile)
{
    int rc = -1;

    int printed = 0;

    ERROR_IF(
        PRINTF(printed, fprintf(aFile, "<fdset %p", self)));

    struct Ert_FdSetElement_ *elem;
    RB_FOREACH(elem, Ert_FdSetTree_, &((struct Ert_FdSet *) self)->mRoot)
    {
        ERROR_IF(
            PRINTF(
                printed,
                fprintf(aFile,
                        " (%d,%d)", elem->mRange.mLhs, elem->mRange.mRhs)));
    }

    ERROR_IF(
        PRINTF(printed, fprintf(aFile, ">")));

    rc = 0;

Finally:

    FINALLY({});

    return printed ? printed : rc;
}

/* -------------------------------------------------------------------------- */
struct Ert_FdSet *
ert_closeFdSet(struct Ert_FdSet *self)
{
    if (self)
    {
        ert_clearFdSet(self);
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
int
ert_createFdSet(struct Ert_FdSet *self)
{
    int rc = -1;

    self->mSize = 0;
    RB_INIT(&self->mRoot);

    rc = 0;

Finally:

    FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
void
ert_clearFdSet(struct Ert_FdSet *self)
{
    struct Ert_FdSetElement_ *elem = RB_ROOT(&self->mRoot);

    while (elem)
    {
        /* The loop invariant is that the subtree rooted at elem is still
         * valid, and must be cleared. Clear the two subtrees before freeing
         * the root of the subtree. */

        struct Ert_FdSetElement_ *lh = RB_LEFT(elem, mTree);
        struct Ert_FdSetElement_ *rh = RB_RIGHT(elem, mTree);

        if (lh)
        {
            elem = lh;
            continue;
        }

        if (rh)
        {
            elem = rh;
            continue;
        }

        /* Only free the root of the subtree while walking up the
         * subtree, thus ensuring that the left and right child
         * subtrees have already been released. */

        do
        {
            /* The loop-invariant is that children, if any, of the subtree
             * rooted at elem have been cleared, leaving the root itself
             * to be freed. */

            struct Ert_FdSetElement_ *parent = RB_PARENT(elem, mTree);

            free(elem);

            if (parent)
            {
                struct Ert_FdSetElement_ *plhs = RB_LEFT(parent, mTree);
                struct Ert_FdSetElement_ *prhs = RB_RIGHT(parent, mTree);

                if (elem == plhs && prhs)
                {
                    /* If the subtree rooted at elem is the left subtree
                     * of its parent, and there is a right subtree, then
                     * the next step is to process the right subtree. */

                    elem = prhs;
                    break;
                }
            }

            elem = parent;

        } while (elem);
    }

    RB_INIT(&self->mRoot);
}

/* -------------------------------------------------------------------------- */
int
ert_invertFdSet(struct Ert_FdSet *self)
{
    int rc = -1;

    struct Ert_FdSetElement_ *elem = 0;

    /* Inverting the set can be modelled as creating a new set of the gaps
     * between the ranges of the old set. If there are N ranges in the old
     * set, there will be up to N+1 ranges in the new inverted set. */

    if (RB_EMPTY(&self->mRoot))
    {
        ERROR_IF(
            ert_insertFdSetRange(self, Ert_FdRange(0, INT_MAX)));
    }
    else
    {
        struct Ert_FdSetElement_ *next = RB_MAX(Ert_FdSetTree_, &self->mRoot);

        if (INT_MAX == next->mRange.mRhs)
        {
            do
            {
                if ( ! next->mRange.mLhs)
                {
                    RB_REMOVE(Ert_FdSetTree_, &self->mRoot, next);
                    free(next);
                    next = 0;
                }
                else
                {
                    struct Ert_FdSetElement_ *prev =
                        RB_PREV(Ert_FdSetTree_, &self->mRoot, next);

                    next->mRange.mRhs = next->mRange.mLhs - 1;
                    next->mRange.mLhs = prev ? prev->mRange.mRhs + 1 : 0;

                    next = prev;
                }
            } while (next);
        }
        else
        {
            struct Ert_FdSetElement_ *prev =
                RB_MIN(Ert_FdSetTree_, &self->mRoot);

            if (prev->mRange.mLhs)
            {
                ERROR_UNLESS(
                    elem = malloc(sizeof(*elem)));

                elem->mRange = Ert_FdRange(0, prev->mRange.mLhs - 1);

                ERROR_IF(
                    RB_INSERT(Ert_FdSetTree_, &self->mRoot, elem),
                    {
                        errno = EEXIST;
                    });

                elem = 0;
            }

            do
            {
                next = RB_NEXT(Ert_FdSetTree_, &self->mRoot, prev);

                prev->mRange.mLhs = prev->mRange.mRhs + 1;
                prev->mRange.mRhs = next ? next->mRange.mLhs - 1 : INT_MAX;

                prev = next;

            } while (prev);
        }
    }

    rc = 0;

Finally:

    FINALLY
    ({
        free(elem);
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_fillFdSet(struct Ert_FdSet *self)
{
    ert_clearFdSet(self);
    return ert_invertFdSet(self);
}

/* -------------------------------------------------------------------------- */
int
ert_insertFdSet(struct Ert_FdSet *self, int aFd)
{
    return ert_insertFdSetRange(self, Ert_FdRange(aFd, aFd));
}

/* -------------------------------------------------------------------------- */
int
ert_removeFdSet(struct Ert_FdSet *self, int aFd)
{
    return ert_removeFdSetRange(self, Ert_FdRange(aFd, aFd));
}

/* -------------------------------------------------------------------------- */
int
ert_insertFdSetFile(struct Ert_FdSet *self, const struct Ert_File *aFile)
{
    return ert_insertFdSetRange(self, Ert_FdRange(aFile->mFd, aFile->mFd));
}

/* -------------------------------------------------------------------------- */
int
ert_removeFdSetFile(struct Ert_FdSet *self, const struct Ert_File *aFile)
{
    return ert_removeFdSetRange(self, Ert_FdRange(aFile->mFd, aFile->mFd));
}

/* -------------------------------------------------------------------------- */
int
ert_insertFdSetRange(struct Ert_FdSet *self, struct Ert_FdRange aRange)
{
    int rc = -1;

    bool inserted = false;

    struct Ert_FdSetElement_ *elem = 0;

    ERROR_UNLESS(
        elem = malloc(sizeof(*elem)));

    elem->mRange = aRange;

    ERROR_IF(
        RB_INSERT(Ert_FdSetTree_, &self->mRoot, elem),
        {
            errno = EEXIST;
        });
    inserted = true;

    struct Ert_FdSetElement_ *prev =
        RB_PREV(Ert_FdSetTree_, &self->mRoot, elem);
    struct Ert_FdSetElement_ *next =
        RB_NEXT(Ert_FdSetTree_, &self->mRoot, elem);

    ERROR_UNLESS(
        ! prev || ert_leftFdRangeOf(elem->mRange, prev->mRange),
        {
            errno = EEXIST;
        });

    ERROR_UNLESS(
        ! next || ert_rightFdRangeOf(elem->mRange, next->mRange),
        {
            errno = EEXIST;
        });

    if (prev && ert_leftFdRangeNeighbour(elem->mRange, prev->mRange))
    {
        RB_REMOVE(Ert_FdSetTree_, &self->mRoot, elem);

        prev->mRange.mRhs = elem->mRange.mRhs;
        free(elem);

        elem = prev;
    }

    if (next && ert_rightFdRangeNeighbour(elem->mRange, next->mRange))
    {
        RB_REMOVE(Ert_FdSetTree_, &self->mRoot, elem);

        next->mRange.mLhs = elem->mRange.mLhs;
        free(elem);

        elem = next;
    }

    elem = 0;

    rc = 0;

Finally:

    FINALLY
    ({
        if (inserted && elem)
            RB_REMOVE(Ert_FdSetTree_, &self->mRoot, elem);

        free(elem);
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_removeFdSetRange(struct Ert_FdSet *self, struct Ert_FdRange aRange)
{
    int rc = -1;

    struct Ert_FdRange  fdRange_;
    struct Ert_FdRange *fdRange = 0;

    struct Ert_FdSetElement_ find =
    {
        .mRange = aRange,
    };

    ERROR_IF(
        RB_EMPTY(&self->mRoot),
        {
            errno = ENOENT;
        });

    struct Ert_FdSetElement_ *elem = RB_NFIND(
        Ert_FdSetTree_, &self->mRoot, &find);

    if ( ! elem)
        elem = RB_MAX(Ert_FdSetTree_, &self->mRoot);

    int contained = ert_containsFdRange(elem->mRange, aRange);

    if ( ! contained)
    {
        ERROR_UNLESS(
            elem = RB_PREV(Ert_FdSetTree_, &self->mRoot, elem),
            {
                errno = ENOENT;
            });

        ERROR_UNLESS(
            contained = ert_containsFdRange(elem->mRange, aRange),
            {
                errno = ENOENT;
            });
    }

    switch (contained)
    {
    case 1:
        elem->mRange.mLhs = aRange.mRhs + 1;
        break;

    case 3:
        RB_REMOVE(Ert_FdSetTree_, &self->mRoot, elem);
        free(elem);
        break;

    case 2:
        elem->mRange.mRhs = aRange.mLhs - 1;
        break;

    default:
        {
          struct Ert_FdRange lhSide = Ert_FdRange(
              elem->mRange.mLhs,aRange.mLhs-1);

          struct Ert_FdRange rhSide = Ert_FdRange(
              aRange.mRhs+1,    elem->mRange.mRhs);

          fdRange_ = elem->mRange;
          fdRange  = &fdRange_;

          elem->mRange = lhSide;
          ERROR_IF(
              ert_insertFdSetRange(self, rhSide));

          fdRange = 0;
        }
        break;
    }

    rc = 0;

Finally:

    FINALLY
    ({
        if (rc && fdRange)
            elem->mRange = *fdRange;
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
ssize_t
ert_visitFdSet(const struct Ert_FdSet *self, struct Ert_FdSetVisitor aVisitor)
{
    ssize_t rc = -1;

    ssize_t visited = 0;

    struct Ert_FdSetElement_ *elem;
    RB_FOREACH(elem, Ert_FdSetTree_, &((struct Ert_FdSet *) self)->mRoot)
    {
        int err;
        ERROR_IF(
            (err = ert_callFdSetVisitor(aVisitor, elem->mRange),
             -1 == err));

        ++visited;

        if (err)
            break;
    }

    rc = 0;

Finally:

    FINALLY({});

    return rc ? rc : visited;
}

/* -------------------------------------------------------------------------- */
struct Ert_FdRange
Ert_FdRange_(int aLhs, int aRhs)
{
    ensure(0 <= aLhs && aLhs <= aRhs);

    return (struct Ert_FdRange)
    {
        .mLhs = aLhs,
        .mRhs = aRhs,
    };
}

/* -------------------------------------------------------------------------- */
static inline bool
ert_containsFdRange_(struct Ert_FdRange self, int aFd)
{
    return self.mLhs <= aFd && aFd <= self.mRhs;
}

/* -------------------------------------------------------------------------- */
int
ert_containsFdRange(struct Ert_FdRange self, struct Ert_FdRange aOther)
{
    int contained = 0;

    if (ert_containsFdRange_(self, aOther.mLhs) &&
        ert_containsFdRange_(self, aOther.mRhs) )
    {
        int lhs = (self.mLhs == aOther.mLhs) << 0;
        int rhs = (self.mRhs == aOther.mRhs) << 1;

        contained = lhs | rhs;

        if ( ! contained)
            contained = -1;
    }

    return contained;
}

/* -------------------------------------------------------------------------- */
bool
ert_leftFdRangeOf(struct Ert_FdRange self, struct Ert_FdRange aOther)
{
    return aOther.mRhs < self.mLhs;
}

/* -------------------------------------------------------------------------- */
bool
ert_rightFdRangeOf(struct Ert_FdRange self, struct Ert_FdRange aOther)
{
    return self.mRhs < aOther.mLhs;
}

/* -------------------------------------------------------------------------- */
bool
ert_leftFdRangeNeighbour(struct Ert_FdRange self, struct Ert_FdRange aOther)
{
    return self.mLhs - 1 == aOther.mRhs;
}

/* -------------------------------------------------------------------------- */
bool
ert_rightFdRangeNeighbour(struct Ert_FdRange self, struct Ert_FdRange aOther)
{
    return self.mRhs + 1 == aOther.mLhs;
}

/* -------------------------------------------------------------------------- */
