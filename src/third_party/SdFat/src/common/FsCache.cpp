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
#define DBG_FILE "FsCache.cpp"
#include "FsCache.h"

#include "DebugMacros.h"
//------------------------------------------------------------------------------
uint8_t* FsCache::prepare(uint32_t sector, uint8_t option) {
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
      if (!m_blockDev->readSector(sector, m_buffer)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    }
    m_status = 0;
    m_sector = sector;
  }
  m_status |= option & CACHE_STATUS_MASK;
  return m_buffer;

fail:
  return nullptr;
}
//------------------------------------------------------------------------------
bool FsCache::sync() {
  if (m_status & CACHE_STATUS_DIRTY) {
    if (!m_blockDev->writeSector(m_sector, m_buffer)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    // mirror second FAT
    if (m_status & CACHE_STATUS_MIRROR_FAT) {
      if (!m_blockDev->writeSector(m_sector + m_mirrorOffset, m_buffer)) {
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
