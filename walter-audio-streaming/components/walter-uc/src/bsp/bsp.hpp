/**
 * @file bsp.hpp
 * @author Daan Pape <daan@dptechnics.com>, Arnoud Devoogdt <arnoud@dptechnics.com>, Robbe Beernaert
 * @date 21 Aug 2025
 * @copyright DPTechnics bv
 * @brief Walter unified comms library
 *
 * @section LICENSE
 *
 * Copyright (C) 2025, DPTechnics bv
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *      conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright notice, this list of
 *      conditions and the following disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *   3. Neither the name of DPTechnics bv nor the names of its contributors may be used to endorse
 *      or promote products derived from this software without specific prior written permission.
 *
 *   4. This software, with or without modification, must only be used with a Walter board from
 *      DPTechnics bv.
 *
 *   5. Any software provided in binary form under this license must not be reverse engineered,
 *      decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY DPTECHNICS BV “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT, AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL DPTECHNICS BV OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @section DESCRIPTION
 *
 * This file contains the unified comms library for Walter.
 */

#ifndef _BSP_H_
#define _BSP_H_

#include <uc.hpp>
#define EXPAND(x) x

#define APPLY_1(f, x) f(x)
#define APPLY_2(f, x, ...) f(x), APPLY_1(f, __VA_ARGS__)
#define APPLY_3(f, x, ...) f(x), APPLY_2(f, __VA_ARGS__)
#define APPLY_4(f, x, ...) f(x), APPLY_3(f, __VA_ARGS__)
#define APPLY_5(f, x, ...) f(x), APPLY_4(f, __VA_ARGS__)
#define APPLY_6(f, x, ...) f(x), APPLY_5(f, __VA_ARGS__)
#define APPLY_7(f, x, ...) f(x), APPLY_6(f, __VA_ARGS__)
#define APPLY_8(f, x, ...) f(x), APPLY_7(f, __VA_ARGS__)
#define APPLY_9(f, x, ...) f(x), APPLY_8(f, __VA_ARGS__)
#define APPLY_10(f, x, ...) f(x), APPLY_9(f, __VA_ARGS__)

#define APPLY_1_NC(f, x) f(x)
#define APPLY_2_NC(f, x, ...) f(x) APPLY_1_NC(f, __VA_ARGS__)
#define APPLY_3_NC(f, x, ...) f(x) APPLY_2_NC(f, __VA_ARGS__)
#define APPLY_4_NC(f, x, ...) f(x) APPLY_3_NC(f, __VA_ARGS__)
#define APPLY_5_NC(f, x, ...) f(x) APPLY_4_NC(f, __VA_ARGS__)
#define APPLY_6_NC(f, x, ...) f(x) APPLY_5_NC(f, __VA_ARGS__)
#define APPLY_7_NC(f, x, ...) f(x) APPLY_6_NC(f, __VA_ARGS__)
#define APPLY_8_NC(f, x, ...) f(x) APPLY_7_NC(f, __VA_ARGS__)
#define APPLY_9_NC(f, x, ...) f(x) APPLY_8_NC(f, __VA_ARGS__)
#define APPLY_10_NC(f, x, ...) f(x) APPLY_9_NC(f, __VA_ARGS__)

#define GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, NAME, ...) NAME
#define FOR_EACH(f, ...)                                                                           \
  EXPAND(GET_MACRO(__VA_ARGS__, APPLY_10, APPLY_9, APPLY_8, APPLY_7, APPLY_6, APPLY_5, APPLY_4,    \
                   APPLY_3, APPLY_2, APPLY_1)(f, __VA_ARGS__))

#define GET_MACRO_NC(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, NAME, ...) NAME
#define FOR_EACH_NO_COMMA(f, ...)                                                                  \
  EXPAND(GET_MACRO_NC(__VA_ARGS__, APPLY_10_NC, APPLY_9_NC, APPLY_8_NC, APPLY_7_NC, APPLY_6_NC,    \
                      APPLY_5_NC, APPLY_4_NC, APPLY_3_NC, APPLY_2_NC, APPLY_1_NC)(f, __VA_ARGS__))

// --- Driver Transformation ---
#define ADDRESS(x) &x
#define DRIVERS(...) { FOR_EACH(ADDRESS, __VA_ARGS__), nullptr }

#define MEMBERNAME(x) driver::Driver* x;
#define MEMBERNAMES(...) FOR_EACH_NO_COMMA(MEMBERNAME, __VA_ARGS__)

#define MEMBER(x) .x = &x
#define MEMBERS(...) FOR_EACH(MEMBER, __VA_ARGS__)

#define CELL_DRV(cellularDriver) ((driver::cellular::CellularDriver*) cellularDriver)

// --- BSP(Board Support) ---

#define BSP(boardName, ...)                                                                        \
  struct sUnifiedComm {                                                                            \
    UnifiedController controller;                                                                  \
    MEMBERNAMES(__VA_ARGS__)                                                                       \
  };                                                                                               \
  sUnifiedCommInternal __ucInternal = { .name = boardName, .drivers = DRIVERS(__VA_ARGS__) };      \
  sUnifiedComm uc = {                                                                              \
    .controller = { &__ucInternal },                                                               \
    MEMBERS(__VA_ARGS__),                                                                          \
  };
#endif