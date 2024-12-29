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
#ifndef FsCache_h
#define FsCache_h
/**
 * \file
 * \brief Common cache code for exFAT and FAT.
 */
#include "FsBlockDevice.h"
#include "SysCall.h"
/**
 * \class FsCache
 * \brief Sector cache.
 */
class FsCache {
 public:
  /** Cached sector is dirty */
  static const uint8_t CACHE_STATUS_DIRTY = 1;
  /** Cashed sector is FAT entry and must be mirrored in second FAT. */
  static const uint8_t CACHE_STATUS_MIRROR_FAT = 2;
  /** Cache sector status bits */
  static const uint8_t CACHE_STATUS_MASK =
      CACHE_STATUS_DIRTY | CACHE_STATUS_MIRROR_FAT;
  /** Sync existing sector but do not read new sector. */
  static const uint8_t CACHE_OPTION_NO_READ = 4;
  /** Cache sector for read. */
  static const uint8_t CACHE_FOR_READ = 0;
  /** Cache sector for write. */
  static const uint8_t CACHE_FOR_WRITE = CACHE_STATUS_DIRTY;
  /** Reserve cache sector for write - do not read from sector device. */
  static const uint8_t CACHE_RESERVE_FOR_WRITE =
      CACHE_STATUS_DIRTY | CACHE_OPTION_NO_READ;
  //----------------------------------------------------------------------------
  /** Cobstructor. */
  FsCache() { init(nullptr); }
  /** \return Cache buffer address. */
  uint8_t* cacheBuffer() { return m_buffer; }
  /**
   * Cache safe read of a sector.
   *
   * \param[in] sector Logical sector to be read.
   * \param[out] dst Pointer to the location that will receive the data.
   * \return true for success or false for failure.
   */
  bool cacheSafeRead(uint32_t sector, uint8_t* dst) {
    if (isCached(sector)) {
      memcpy(dst, m_buffer, 512);
      return true;
    }
    return m_blockDev->readSector(sector, dst);
  }
  /**
   * Cache safe read of multiple sectors.
   *
   * \param[in] sector Logical sector to be read.
   * \param[in] count Number of sectors to be read.
   * \param[out] dst Pointer to the location that will receive the data.
   * \return true for success or false for failure.
   */
  bool cacheSafeRead(uint32_t sector, uint8_t* dst, size_t count) {
    if (isCached(sector, count) && !sync()) {
      return false;
    }
    return m_blockDev->readSectors(sector, dst, count);
  }
  /**
   * Cache safe write of a sectors.
   *
   * \param[in] sector Logical sector to be written.
   * \param[in] src Pointer to the location of the data to be written.
   * \return true for success or false for failure.
   */
  bool cacheSafeWrite(uint32_t sector, const uint8_t* src) {
    if (isCached(sector)) {
      invalidate();
    }
    return m_blockDev->writeSector(sector, src);
  }
  /**
   * Cache safe write of multiple sectors.
   *
   * \param[in] sector Logical sector to be written.
   * \param[in] src Pointer to the location of the data to be written.
   * \param[in] count Number of sectors to be written.
   * \return true for success or false for failure.
   */
  bool cacheSafeWrite(uint32_t sector, const uint8_t* src, size_t count) {
    if (isCached(sector, count)) {
      invalidate();
    }
    return m_blockDev->writeSectors(sector, src, count);
  }
  /** \return Clear the cache and returns a pointer to the cache. */
  uint8_t* clear() {
    if (isDirty() && !sync()) {
      return nullptr;
    }
    invalidate();
    return m_buffer;
  }
  /** Set current sector dirty. */
  void dirty() { m_status |= CACHE_STATUS_DIRTY; }
  /** Initialize the cache.
   * \param[in] blockDev Block device for this cache.
   */
  void init(FsBlockDevice* blockDev) {
    m_blockDev = blockDev;
    invalidate();
  }
  /** Invalidate current cache sector. */
  void invalidate() {
    m_status = 0;
    m_sector = 0XFFFFFFFF;
  }
  /** Check if a sector is in the cache.
   * \param[in] sector Sector to checked.
   * \return true if the sector is cached.
   */
  bool isCached(uint32_t sector) const { return sector == m_sector; }
  /** Check if the cache contains a sector from a range.
   * \param[in] sector Start sector of the range.
   * \param[in] count Number of sectors in the range.
   * \return true if a sector in the range is cached.
   */
  bool isCached(uint32_t sector, size_t count) {
    return sector <= m_sector && m_sector < (sector + count);
  }
  /** \return dirty status */
  bool isDirty() { return m_status & CACHE_STATUS_DIRTY; }
  /** Prepare cache to access sector.
   * \param[in] sector Sector to read.
   * \param[in] option mode for cached sector.
   * \return Address of cached sector.
   */
  uint8_t* prepare(uint32_t sector, uint8_t option);
  /** \return Logical sector number for cached sector. */
  uint32_t sector() { return m_sector; }
  /** Set the offset to the second FAT for mirroring.
   * \param[in] offset Sector offset to second FAT.
   */
  void setMirrorOffset(uint32_t offset) { m_mirrorOffset = offset; }
  /** Write current sector if dirty.
   * \return true for success or false for failure.
   */
  bool sync();

 private:
  uint8_t m_status;
  FsBlockDevice* m_blockDev;
  uint32_t m_sector;
  uint32_t m_mirrorOffset;
  uint8_t m_buffer[512];
};
#endif  // FsCache_h
