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

#include "ert/printf.h"

#include "ert/error.h"
#include "ert/type.h"

#include <printf.h>
#include <string.h>

static unsigned moduleInit_;
static bool     moduleInitPrintf_;

#define PRINTF_PRI_LEADER  "%%%p<"

#define PRINTF_SPEC_METHOD 'M'

const struct Ert_Type * const ert_printfMethodType_ = ERT_TYPE("PrintfMethod");

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
vprintf_(
    void       *self,
    int       (*aPrintf)(void *self, const char *aFmt, va_list aArg),
    const char *aFmt,
    va_list     aArg)
{
    int rc = -1;

    const char *priPtr = strstr(aFmt, PRINTF_PRI_LEADER);

    if ( ! priPtr)
        rc = aPrintf(self, aFmt, aArg);
    else
    {
        char fmtBuf[strlen(aFmt) + 1];

        strcpy(fmtBuf, aFmt);

        char *wrPtr = &fmtBuf[priPtr - aFmt];
        char *rdPtr = wrPtr;

        char *fmtPtr = rdPtr;

        do
        {
            memcpy(wrPtr, rdPtr, fmtPtr - rdPtr);

            wrPtr += fmtPtr - rdPtr;
            rdPtr  = fmtPtr;

            do
            {
                if (*rdPtr)
                {
                    if ( ! memcmp(
                             rdPtr+1,
                             PRIs_Ert_Method, sizeof(PRIs_Ert_Method)-1))
                    {
                        rdPtr += 1 + sizeof(PRIs_Ert_Method) - 1;

                        *wrPtr++ = '%';
                        *wrPtr++ = PRINTF_SPEC_METHOD;

                        break;
                    }
                }

                *wrPtr++ = *rdPtr++;

            } while (0);

            fmtPtr = strstr(rdPtr, PRINTF_PRI_LEADER);

        } while (fmtPtr);

        memmove(wrPtr, rdPtr, strlen(rdPtr) + 1);

        rc = aPrintf(self, fmtBuf, aArg);
    }

    return rc;
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
vfprintf_(void *self, const char *aFmt, va_list aArg)
{
    return vfprintf(self, aFmt, aArg);
}

int
ert_vfprintf(FILE *aFile, const char *aFmt, va_list aArg)
{
    return vprintf_(aFile, vfprintf_, aFmt, aArg);
}

/* -------------------------------------------------------------------------- */
struct PrintfString
{
    char  *mBufPtr;
    size_t mBufLen;
};

static ERT_CHECKED int
vsnprintf_(void *self_, const char *aFmt, va_list aArg)
{
    struct PrintfString *self = self_;

    return vsnprintf(self->mBufPtr, self->mBufLen, aFmt, aArg);
}

int
ert_vsnprintf(char *aBuf, size_t aSize, const char *aFmt, va_list aArg)
{
    struct PrintfString self_ =
    {
        .mBufPtr = aBuf,
        .mBufLen = aSize,
    };

    return vprintf_(&self_, vsnprintf_, aFmt, aArg);
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
vdprintf_(void *self_, const char *aFmt, va_list aArg)
{
    const int *self = self_;

    return vdprintf(*self, aFmt, aArg);
}

int
ert_vdprintf(int aFd, const char *aFmt, va_list aArg)
{
    return vprintf_(&aFd, vdprintf_, aFmt, aArg);
}

/* -------------------------------------------------------------------------- */
int
ert_printf(const char *aFmt, ...)
{
    int rc = -1;

    va_list argp;

    va_start(argp, aFmt);
    rc = ert_vfprintf(stdout, aFmt, argp);
    va_end(argp);

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_fprintf(FILE *aFile, const char *aFmt, ...)
{
    int rc = -1;

    va_list argp;

    va_start(argp, aFmt);
    rc = ert_vfprintf(aFile, aFmt, argp);
    va_end(argp);

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_snprintf(char *aBuf, size_t aSize, const char *aFmt, ...)
{
    int rc = -1;

    va_list argp;

    va_start(argp, aFmt);
    rc = ert_vsnprintf(aBuf, aSize, aFmt, argp);
    va_end(argp);

    return rc;
}

/* -------------------------------------------------------------------------- */
int
ert_dprintf(int aFd, const char *aFmt, ...)
{
    int rc = -1;

    va_list argp;

    va_start(argp, aFmt);
    rc = ert_vdprintf(aFd, aFmt, argp);
    va_end(argp);

    return rc;
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
printf_method_call_(
    FILE                     *aFile,
    const struct printf_info *aInfo,
    const void * const       *aArgs)
{
    const void *self_;

    memcpy(&self_, aArgs[0], sizeof(self_));

    const struct Ert_PrintfMethod *self = self_;

    ert_ensure(self->mType == &ert_printfMethodType_);

    return ert_callPrintfMethod_(self->mMethod, aFile);
}

/* -------------------------------------------------------------------------- */
static ERT_CHECKED int
printf_method_info_(
    const struct printf_info *aInfo,
    size_t                    aSize,
    int                      *aArgTypes,
    int                      *aArgSizes)
{
    if (1 <= aSize)
        aArgTypes[0] = PA_POINTER;

    return 1;
}

/* -------------------------------------------------------------------------- */
int
Ert_Printf_init(struct Ert_PrintfModule *self)
{
    int rc = -1;

    self->mModule = self;

    if ( ! moduleInit_)
    {
        if ( ! moduleInitPrintf_)
        {
            ERT_ERROR_IF(
                register_printf_specifier(
                    PRINTF_SPEC_METHOD,
                    printf_method_call_,
                    printf_method_info_));

            moduleInitPrintf_ = true;
        }
    }

    ++moduleInit_;

    rc = 0;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
struct Ert_PrintfModule *
Ert_Printf_exit(struct Ert_PrintfModule *self)
{
    if (self)
        --moduleInit_;

    return 0;
}

/* -------------------------------------------------------------------------- */
