/**
 * Copyright (c) 2011-2024 Bill Greiman
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
#define DBG_FILE "ExFatVolume.cpp"
#include "../common/DebugMacros.h"
#include "ExFatLib.h"
ExFatVolume* ExFatVolume::m_cwv = nullptr;
//-----------------------------------------------------------------------------
bool ExFatVolume::chdir(const char* path) {
  ExFatFile dir;
  if (!dir.open(vwd(), path, O_RDONLY)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (!dir.isDir()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // m_vwd = dir;
  m_vwd.copy(&dir);
  return true;

fail:
  return false;
}
