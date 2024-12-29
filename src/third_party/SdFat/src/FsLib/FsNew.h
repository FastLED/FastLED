/**
 * Copyright (c) 2011-2022 Bill Greiman
 * This file is part of the SdFat library for SD memory cards.
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#ifndef FsNew_h
#define FsNew_h
#include <stddef.h>
#include <stdint.h>

/** 32-bit alignment */
typedef uint32_t newalign_t;

/** Size required for exFAT or FAT class. */
#define FS_SIZE(etype, ftype) \
  (sizeof(ftype) < sizeof(etype) ? sizeof(etype) : sizeof(ftype))

/** Dimension of aligned area. */
#define NEW_ALIGN_DIM(n) \
  (((size_t)(n) + sizeof(newalign_t) - 1U) / sizeof(newalign_t))

/** Dimension of aligned area for etype or ftype class. */
#define FS_ALIGN_DIM(etype, ftype) NEW_ALIGN_DIM(FS_SIZE(etype, ftype))

/** Custom new placement operator */
void* operator new(size_t size, newalign_t* ptr);
#endif  // FsNew_h
