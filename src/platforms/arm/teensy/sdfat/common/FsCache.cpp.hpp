// IWYU pragma: private
/**
 * Copyright (c) 2011-2021 Bill Greiman
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
// Private implementation unit, included by sdfat/_build.cpp.hpp.
#define DBG_FILE "FsCache.cpp"
#include "DebugMacros.h"  // ok include path - upstream SdFat local header
#include "FsCache.h"  // ok include path - upstream SdFat local header
#include "fl/stl/int.h"
namespace fl { namespace platforms { namespace teensy { namespace sdfat {
//------------------------------------------------------------------------------
fl::u8* FsCache::prepare(fl::u32 sector, fl::u8 option) {
  if (!m_blockDev) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (m_sector != sector) {
    if (!sync()) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (!(option & CACHE_OPTION_NO_READ)) {
      if (!mBlockDev->readSector(sector, mBuffer)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    }
    m_status = 0;
    m_sector = sector;
  }
  m_status |= option & CACHE_STATUS_MASK;
  return mBuffer;

 fail:
  return nullptr;
}
//------------------------------------------------------------------------------
bool FsCache::sync() {
  if (m_status & CACHE_STATUS_DIRTY) {
    if (!mBlockDev->writeSector(mSector, mBuffer)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    // mirror second FAT
    if (m_status & CACHE_STATUS_MIRROR_FAT) {
      fl::u32 sector = mSector + mMirrorOffset;
      if (!mBlockDev->writeSector(sector, mBuffer)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    }
    m_status &= ~CACHE_STATUS_DIRTY;
  }
  return true;

 fail:
  return false;
}

} } } }  // namespace fl::platforms::teensy::sdfat
#ifdef DBG_FILE
#undef DBG_FILE
#endif
