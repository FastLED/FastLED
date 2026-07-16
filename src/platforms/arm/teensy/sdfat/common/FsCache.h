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
#ifndef FsCache_h
#define FsCache_h
/**
 * \file
 * \brief Common cache code for exFAT and FAT.
 */
#include "SysCall.h"  // ok include path: upstream SdFat local header.
#include "FsBlockDevice.h"  // ok include path: supplied by the SdFat integration.
#include "fl/stl/cstring.h"
#include "fl/stl/int.h"
namespace fl { namespace platforms { namespace teensy { namespace sdfat {

// Compatibility aliases for the upstream FsCache.cpp implementation.
#define m_buffer mBuffer
#define m_blockDev mBlockDev
#define m_mirrorOffset mMirrorOffset
#define m_sector mSector
#define m_status mStatus
/**
 * \class FsCache
 * \brief Sector cache.
 */
class FsCache {
 public:
  /** Cached sector is dirty */
  static const fl::u8 CACHE_STATUS_DIRTY = 1;
  /** Cashed sector is FAT entry and must be mirrored in second FAT. */
  static const fl::u8 CACHE_STATUS_MIRROR_FAT = 2;
  /** Cache sector status bits */
  static const fl::u8 CACHE_STATUS_MASK =
    CACHE_STATUS_DIRTY | CACHE_STATUS_MIRROR_FAT;
  /** Sync existing sector but do not read new sector. */
  static const fl::u8 CACHE_OPTION_NO_READ = 4;
  /** Cache sector for read. */
  static const fl::u8 CACHE_FOR_READ = 0;
  /** Cache sector for write. */
  static const fl::u8 CACHE_FOR_WRITE = CACHE_STATUS_DIRTY;
  /** Reserve cache sector for write - do not read from sector device. */
  static const fl::u8 CACHE_RESERVE_FOR_WRITE =
    CACHE_STATUS_DIRTY | CACHE_OPTION_NO_READ;
  //----------------------------------------------------------------------------
  /** \return Cache buffer address. */
  fl::u8* cacheBuffer() {
    return mBuffer;
  }
  /**
   * Cache safe read of a sector.
   *
   * \param[in] sector Logical sector to be read.
   * \param[out] dst Pointer to the location that will receive the data.
   * \return true for success or false for failure.
   */
  bool cacheSafeRead(fl::u32 sector, fl::u8* dst) {
    if (isCached(sector)) {
      fl::memcpy(dst, mBuffer, 512);
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
  bool cacheSafeRead(fl::u32 sector, fl::u8* dst, size_t count) {
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
  bool cacheSafeWrite(fl::u32 sector, const fl::u8* src) {
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
  bool cacheSafeWrite(fl::u32 sector, const fl::u8* src, size_t count) {
     if (isCached(sector, count)) {
      invalidate();
    }
    return m_blockDev->writeSectors(sector, src, count);
  }
  /** \return Clear the cache and returns a pointer to the cache. */
  fl::u8* clear() {
    if (isDirty() && !sync()) {
      return nullptr;
    }
    invalidate();
    return mBuffer;
  }
  /** Set current sector dirty. */
  void dirty() {
    m_status |= CACHE_STATUS_DIRTY;
  }
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
  bool isCached(fl::u32 sector) const {return sector == mSector;}
   /** Check if the cache contains a sector from a range.
   * \param[in] sector Start sector of the range.
   * \param[in] count Number of sectors in the range.
   * \return true if a sector in the range is cached.
   */
  bool isCached(fl::u32 sector, size_t count) {
    return sector <= mSector && mSector < (sector + count);
  }
  /** \return dirty status */
  bool isDirty() {
    return m_status & CACHE_STATUS_DIRTY;
  }
  /** Prepare cache to access sector.
   * \param[in] sector Sector to read.
   * \param[in] option mode for cached sector.
   * \return Address of cached sector.
   */
  fl::u8* prepare(fl::u32 sector, fl::u8 option);
  /** \return Logical sector number for cached sector. */
  fl::u32 sector() {
    return mSector;
  }
  /** Set the offset to the second FAT for mirroring.
   * \param[in] offset Sector offset to second FAT.
   */
  void setMirrorOffset(fl::u32 offset) {
    m_mirrorOffset = offset;
  }
  /** Write current sector if dirty.
   * \return true for success or false for failure.
   */
  bool sync();

 private:
  fl::u8 mStatus;
  FsBlockDevice* mBlockDev;
  fl::u32 mMirrorOffset;
  fl::u32 mSector;
  fl::u8 mBuffer[512];
};
} } } }  // namespace fl::platforms::teensy::sdfat
#endif  // FsCache_h
