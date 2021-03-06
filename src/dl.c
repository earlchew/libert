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

#include "ert/dl.h"
#include "ert/thread.h"
#include "ert/error.h"

#include <string.h>
#include <link.h>

/* -------------------------------------------------------------------------- */
static struct Ert_ThreadSigMutex dlSigMutex_ =
    ERT_THREAD_SIG_MUTEX_INITIALIZER(dlSigMutex_);

/* Some versions of libdl are not thread safe, though more recent versions
 * might have rectified this. Since shared library operations are fairly
 * rare, ensure that the use of the library is synchronised. */

ERT_THREAD_FORK_SENTRY(
    ert_lockThreadSigMutex(&dlSigMutex_),
    ert_unlockThreadSigMutex(&dlSigMutex_));

/* -------------------------------------------------------------------------- */
struct DlSymbolVisitor_
{
    uintptr_t mSoAddr;
    char     *mSoPath;
};

static ERT_CHECKED int
dlSymbolVisitor_(struct dl_phdr_info *aInfo, size_t aSize, void *aVisitor)
{
    int rc = -1;

    int matched = 0;

    struct DlSymbolVisitor_ *visitor = aVisitor;

    for (unsigned ix = 0; ix < aInfo->dlpi_phnum; ++ix)
    {
        uintptr_t addr = aInfo->dlpi_addr + aInfo->dlpi_phdr[ix].p_vaddr;
        size_t    size = aInfo->dlpi_phdr[ix].p_memsz;

        if (addr <= visitor->mSoAddr && visitor->mSoAddr < addr + size)
        {
            ERT_ERROR_UNLESS(
                aInfo->dlpi_name,
                {
                    errno = ENOENT;
                });

            char *sopath;
            ERT_ERROR_UNLESS(
                (sopath = strdup(aInfo->dlpi_name)));

            visitor->mSoPath = sopath;

            matched = 1;
            break;
        }
    }

    rc = matched;

Ert_Finally:

    ERT_FINALLY({});

    return rc;
}

/* -------------------------------------------------------------------------- */
char *
ert_findDlSymbol(const char *aSymName, uintptr_t *aSymAddr, const char **aErr)
{
    char *rc = 0;

    struct Ert_ThreadSigMutex *lock = ert_lockThreadSigMutex(&dlSigMutex_);

    if (aErr)
        *aErr = 0;

    /* PIC implementations resolve symbols to an intermediate thunk.
     * Repeatedly try to resolve the symbol to find the actual
     * implementation of the symbol. */

    void *symbol;

    {
        dlerror();
        void       *next = dlsym(RTLD_DEFAULT, aSymName);
        const char *err  = "<ERROR>";

        ERT_ERROR_IF(
            err = dlerror(),
            {
                if (aErr)
                    *aErr = err;
            });

        do
        {
            symbol = next;
            next   = dlsym(RTLD_NEXT, aSymName);
            err    = dlerror();
        } while ( ! err && symbol != next && next);
    }

    struct DlSymbolVisitor_ visitor =
    {
        .mSoAddr = (uintptr_t) symbol,
        .mSoPath = 0,
    };

    if (0 < dl_iterate_phdr(dlSymbolVisitor_, &visitor))
    {
        if (aSymAddr)
            *aSymAddr = visitor.mSoAddr;
        rc = visitor.mSoPath;
    }

Ert_Finally:

    ERT_FINALLY
    ({
        lock = ert_unlockThreadSigMutex(lock);
    });

    return rc;
}

/* -------------------------------------------------------------------------- */
