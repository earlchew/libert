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
#ifndef ERT_TIMESCALE_H
#define ERT_TIMESCALE_H

#include "ert/compiler.h"

#include <inttypes.h>
#include <stddef.h>

ERT_BEGIN_C_SCOPE;

struct timespec;
struct timeval;
struct itimerval;

#ifndef __cplusplus
#define ERT_TIMESCALE_CTOR_(Struct_, Type_, Field_)
#else
#define ERT_TIMESCALE_CTOR_(Struct_, Type_, Field_)     \
    Struct_()                                           \
    { *this = Struct_ ## _(static_cast<Type_>(0)); }    \
                                                        \
    Struct_(Type_ Field_ ## _)                          \
    { *this = Struct_ ## _(Field_ ## _); }
#endif

enum Ert_TimeScale
{
    Ert_TimeScale_ns = 1000 * 1000 * 1000,
    Ert_TimeScale_us = 1000 * 1000,
    Ert_TimeScale_ms = 1000,
    Ert_TimeScale_s  = 1,
};

/* -------------------------------------------------------------------------- */
#define PRIu_Ert_NanoSeconds PRIu64
#define PRIs_Ert_NanoSeconds PRIu64 ".%09" PRIu64 "s"
#define FMTs_Ert_NanoSeconds(NanoSeconds) \
    ((NanoSeconds).ns / Ert_TimeScale_ns), ((NanoSeconds).ns % Ert_TimeScale_ns)

struct Ert_NanoSeconds
Ert_NanoSeconds_(uint64_t ns);

struct Ert_NanoSeconds
{
    ERT_TIMESCALE_CTOR_(Ert_NanoSeconds, uint64_t, ns)

    union
    {
        uint64_t ns;
        uint64_t Value_;
        char   (*Scale_)[Ert_TimeScale_ns];
    };
};

/* -------------------------------------------------------------------------- */
#define PRIu_Ert_MicroSeconds PRIu64
#define PRIs_Ert_MicroSeconds PRIu64 ".%06" PRIu64 "s"
#define FMTs_Ert_MicroSeconds(MicroSeconds) \
    ((MicroSeconds).us / 1000000), ((MicroSeconds).us % 1000000)

struct Ert_MicroSeconds
Ert_MicroSeconds_(uint64_t us);

struct Ert_MicroSeconds
{
    ERT_TIMESCALE_CTOR_(Ert_MicroSeconds, uint64_t, us)

    union
    {
        uint64_t us;
        uint64_t Value_;
        char   (*Scale_)[Ert_TimeScale_us];
    };
};

/* -------------------------------------------------------------------------- */
#define PRIu_Ert_MilliSeconds PRIu64
#define PRIs_Ert_MilliSeconds PRIu64 ".%03" PRIu64 "s"
#define FMTs_Ert_MilliSeconds(MilliSeconds) \
    ((MilliSeconds).ms / 1000), ((MilliSeconds).ms % 1000)

struct Ert_MilliSeconds
Ert_MilliSeconds_(uint64_t ms);

struct Ert_MilliSeconds
{
    ERT_TIMESCALE_CTOR_(Ert_MilliSeconds, uint64_t, ms)

    union
    {
        uint64_t ms;
        uint64_t Value_;
        char   (*Scale_)[Ert_TimeScale_ms];
    };
};

/* -------------------------------------------------------------------------- */
#define PRIu_Ert_Seconds PRIu64
#define PRIs_Ert_Seconds PRIu64 "s"
#define FMTs_Ert_Seconds(Seconds) ((Seconds).s)

struct Ert_Seconds
Ert_Seconds_(uint64_t s);

struct Ert_Seconds
{
    ERT_TIMESCALE_CTOR_(Ert_Seconds, uint64_t, s)

    union
    {
        uint64_t s;
        uint64_t Value_;
        char   (*Scale_)[Ert_TimeScale_s];
    };
};

/* -------------------------------------------------------------------------- */
#define PRIs_Ert_Duration PRIs_Ert_NanoSeconds
#define FMTs_Ert_Duration(Duration) FMTs_Ert_NanoSeconds((Duration).duration)

struct Ert_Duration
Ert_Duration_(struct Ert_NanoSeconds duration);

struct Ert_Duration
{
#ifdef __cplusplus
    explicit Ert_Duration()
    : duration(Ert_NanoSeconds(0))
    { }

    explicit Ert_Duration(struct Ert_NanoSeconds duration_)
    : duration(duration_)
    { }
#endif

    struct Ert_NanoSeconds duration;
};

extern const struct Ert_Duration ZeroDuration;

/* -------------------------------------------------------------------------- */
#ifndef __cplusplus
static inline struct Ert_NanoSeconds
Ert_NanoSeconds(uint64_t ns)
{
    return Ert_NanoSeconds_(ns);
}

static inline struct Ert_MicroSeconds
Ert_MicroSeconds(uint64_t ns)
{
    return Ert_MicroSeconds_(ns);
}

static inline struct Ert_MilliSeconds
Ert_MilliSeconds(uint64_t ms)
{
    return Ert_MilliSeconds_(ms);
}

static inline struct Ert_Seconds
Ert_Seconds(uint64_t s)
{
    return Ert_Seconds_(s);
}

static inline struct Ert_Duration
Ert_Duration(struct Ert_NanoSeconds aDuration)
{
    return Ert_Duration_(aDuration);
}
#endif

/* -------------------------------------------------------------------------- */
#define ERT_NSECS(Time)                                         \
    ( (struct Ert_NanoSeconds)                                  \
      { {                                                       \
          Value_ : ert_changeTimeScale_((Time).Value_,          \
                                    sizeof(*(Time).Scale_),     \
                                    Ert_TimeScale_ns)           \
      } } )

#define ERT_USECS(Time)                                         \
    ( (struct Ert_MicroSeconds)                                 \
      { {                                                       \
          Value_ : ert_changeTimeScale_((Time).Value_,          \
                                    sizeof(*(Time).Scale_),     \
                                    Ert_TimeScale_us)           \
      } } )

#define ERT_MSECS(Time)                                         \
    ( (struct Ert_MilliSeconds)                                 \
      { {                                                       \
          Value_ : ert_changeTimeScale_((Time).Value_,          \
                                    sizeof(*(Time).Scale_),     \
                                    Ert_TimeScale_ms)           \
      } } )

#define ERT_SECS(Time)                                          \
    ( (struct Ert_Seconds)                                      \
      { {                                                       \
          Value_ : ert_changeTimeScale_((Time).Value_,          \
                                    sizeof(*(Time).Scale_),     \
                                    Ert_TimeScale_s)            \
      } } )

uint64_t
ert_changeTimeScale_(uint64_t aSrcTime, size_t aSrcScale, size_t aDstScale);

/* -------------------------------------------------------------------------- */
struct Ert_NanoSeconds
ert_timeValToNanoSeconds(const struct timeval *aTimeVal);

struct Ert_NanoSeconds
ert_timeSpecToNanoSeconds(const struct timespec *aTimeSpec);

struct timeval
ert_timeValFromNanoSeconds(struct Ert_NanoSeconds aNanoSeconds);

struct timespec
ert_timeSpecFromNanoSeconds(struct Ert_NanoSeconds aNanoSeconds);

/* -------------------------------------------------------------------------- */
struct itimerval
ert_shortenIntervalTime(const struct itimerval *aTimer,
                        struct Ert_Duration     aElapsed);

/* -------------------------------------------------------------------------- */
struct timespec
ert_earliestTime(const struct timespec *aLhs, const struct timespec *aRhs);

/* -------------------------------------------------------------------------- */

ERT_END_C_SCOPE;

#endif /* ERT_TIMESCALE_H */
