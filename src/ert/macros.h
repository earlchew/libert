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
#ifndef ERT_MACROS_H
#define ERT_MACROS_H

#define ERT_NUMBEROF(Vector_) (sizeof((Vector_))/sizeof((Vector_)[0]))

#define ERT_STRINGIFY_(Text_) #Text_
#define ERT_STRINGIFY(Text_)  ERT_STRINGIFY_(Text_)

#define ERT_ARGS(...)   , ## __VA_ARGS__
#define ERT_EXPAND(...) __VA_ARGS__

#define ERT_CONCAT_(Lhs_, Rhs_) Lhs_ ## Rhs_
#define ERT_CONCAT(Lhs_, Rhs_)  ERT_CONCAT_(Lhs_, Rhs_)

#define ERT_CAR(...)        ERT_CAR_(__VA_ARGS__)
#define ERT_CDR(...)        ERT_CDR_(__VA_ARGS__)
#define ERT_CAR_(Car_, ...) Car_
#define ERT_CDR_(Car_, ...) , ## __VA_ARGS__

#define ERT_IFEMPTY(True_, False_, ...)  \
    ERT_IFEMPTY_(True_, False_, __VA_ARGS__)
#define ERT_IFEMPTY_(True_, False_, ...) \
    ERT_IFEMPTY_1_(ERT_IFEMPTY_COMMA_ __VA_ARGS__ (), True_, False_)
#define ERT_IFEMPTY_COMMA_()             ,
#define ERT_IFEMPTY_1_(A_, B_, C_)       ERT_IFEMPTY_2_(A_, B_, C_)
#define ERT_IFEMPTY_2_(A_, B_, C_, ...)  C_

#define ERT_SWAP(Lhs_, Rhs_)                    \
do                                              \
{                                               \
    ERT_AUTO(swp_, Lhs_);                       \
    Lhs_ = Rhs_;                                \
    Rhs_ = swp_;                                \
} while (0)

#define ERT_MIN(Lhs_, Rhs_)                     \
({                                              \
    ERT_AUTO(lhs_, (Lhs_));                     \
    ERT_AUTO(rhs_, (Rhs_));                     \
                                                \
    lhs_ < rhs_ ? lhs_ : rhs_;                  \
})

#define ERT_MAX(Lhs_, Rhs_)                     \
({                                              \
    ERT_AUTO(lhs_, (Lhs_));                     \
    ERT_AUTO(rhs_, (Rhs_));                     \
                                                \
    lhs_ < rhs_ ? rhs_ : lhs_;                  \
})

#define ERT_ROUNDUP(Value_, Alignment_)                         \
({                                                              \
    ERT_AUTO(alignment_, (Alignment_));                         \
                                                                \
    ( (Value_) + alignment_ - 1 ) / alignment_ * alignment_;    \
})

#define ERT_ROUNDDOWN(Value_, Alignment_)       \
({                                              \
    ERT_AUTO(alignment_, (Alignment_));         \
                                                \
    (Value_) / alignment_ * alignment_;         \
})

#endif /* ERT_MACROS_H */
