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
#ifndef ExFatPartition_h
#define ExFatPartition_h
/**
 * \file
 * \brief ExFatPartition include file.
 */
#include "../common/FsBlockDevice.h"
#include "../common/FsCache.h"
#include "../common/FsStructs.h"
#include "../common/SysCall.h"
/** Set EXFAT_READ_ONLY non-zero for read only */
#ifndef EXFAT_READ_ONLY
#define EXFAT_READ_ONLY 0
#endif  // EXFAT_READ_ONLY
/** Type for exFAT partition */
const uint8_t FAT_TYPE_EXFAT = 64;

class ExFatFile;
//------------------------------------------------------------------------------
/**
 * \struct DirPos_t
 * \brief Internal type for position in directory file.
 */
struct DirPos_t {
  /** current cluster */
  uint32_t cluster;
  /** offset */
  uint32_t position;
  /** directory is contiguous */
  bool isContiguous;
};
//==============================================================================
/**
 * \class ExFatPartition
 * \brief Access exFat partitions on raw file devices.
 */
class ExFatPartition {
 public:
  ExFatPartition() = default;
  /** \return the number of bytes in a cluster. */
  uint32_t bytesPerCluster() const { return m_bytesPerCluster; }
  /** \return the power of two for bytesPerCluster. */
  uint8_t bytesPerClusterShift() const {
    return m_bytesPerSectorShift + m_sectorsPerClusterShift;
  }
  /** \return the number of bytes in a sector. */
  uint16_t bytesPerSector() const { return m_bytesPerSector; }
  /** \return the power of two for bytesPerSector. */
  uint8_t bytesPerSectorShift() const { return m_bytesPerSectorShift; }

  /** Clear the cache and returns a pointer to the cache.  Not for normal apps.
   * \return A pointer to the cache buffer or zero if an error occurs.
   */
  uint8_t* cacheClear() { return m_dataCache.clear(); }
  /** \return the cluster count for the partition. */
  uint32_t clusterCount() const { return m_clusterCount; }
  /** \return the cluster heap start sector. */
  uint32_t clusterHeapStartSector() const { return m_clusterHeapStartSector; }
  /** End access to volume
   * \return pointer to sector size buffer for format.
   */
  uint8_t* end() {
    m_fatType = 0;
    return cacheClear();
  }
  /** \return the FAT length in sectors */
  uint32_t fatLength() const { return m_fatLength; }
  /** \return the FAT start sector number. */
  uint32_t fatStartSector() const { return m_fatStartSector; }
  /** \return Type FAT_TYPE_EXFAT for exFAT partition or zero for error. */
  uint8_t fatType() const { return m_fatType; }
  /** \return free cluster count or -1 if an error occurs. */
  int32_t freeClusterCount();
  /** Initialize a exFAT partition.
   * \param[in] dev The blockDevice for the partition.
   * \param[in] part The partition to be used.  Legal values for \a part are
   * 1-4 to use the corresponding partition on a device formatted with
   * a MBR, Master Boot Record, or zero if the device is formatted as
   * a super floppy with the FAT boot sector in sector volStart.
   * \param[in] volStart location of volume if part is zero.
   *
   * \return true for success or false for failure.
   */
  bool init(FsBlockDevice* dev, uint8_t part, uint32_t volStart = 0);
  /**
   * Check for device busy.
   *
   * \return true if busy else false.
   */
  bool isBusy() { return m_blockDev->isBusy(); }
  /** \return the root directory start cluster number. */
  uint32_t rootDirectoryCluster() const { return m_rootDirectoryCluster; }
  /** \return the root directory length. */
  uint32_t rootLength();
  /** \return the number of sectors in a cluster. */
  uint32_t sectorsPerCluster() const { return 1UL << m_sectorsPerClusterShift; }
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  uint32_t __attribute__((error("use sectorsPerCluster()"))) blocksPerCluster();
#endif  // DOXYGEN_SHOULD_SKIP_THIS
  /** \return the power of two for sectors per cluster. */
  uint8_t sectorsPerClusterShift() const { return m_sectorsPerClusterShift; }
  //----------------------------------------------------------------------------
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  void checkUpcase(print_t* pr);
  bool printDir(print_t* pr, ExFatFile* file);
  void dmpBitmap(print_t* pr);
  void dmpCluster(print_t* pr, uint32_t cluster, uint32_t offset,
                  uint32_t count);
  void dmpFat(print_t* pr, uint32_t start, uint32_t count);
  void dmpSector(print_t* pr, uint32_t sector);
  bool printVolInfo(print_t* pr);
  void printFat(print_t* pr);
  void printUpcase(print_t* pr);
#endif  // DOXYGEN_SHOULD_SKIP_THIS
  //----------------------------------------------------------------------------
 private:
  /** ExFatFile allowed access to private members. */
  friend class ExFatFile;
  uint32_t bitmapFind(uint32_t cluster, uint32_t count);
  bool bitmapModify(uint32_t cluster, uint32_t count, bool value);
  //----------------------------------------------------------------------------
  // Cache functions.
  uint8_t* bitmapCachePrepare(uint32_t sector, uint8_t option) {
#if USE_EXFAT_BITMAP_CACHE
    return m_bitmapCache.prepare(sector, option);
#else   // USE_EXFAT_BITMAP_CACHE
    return m_dataCache.prepare(sector, option);
#endif  // USE_EXFAT_BITMAP_CACHE
  }
  void cacheInit(FsBlockDevice* dev) {
#if USE_EXFAT_BITMAP_CACHE
    m_bitmapCache.init(dev);
#endif  // USE_EXFAT_BITMAP_CACHE
    m_dataCache.init(dev);
  }
  bool cacheSync() {
#if USE_EXFAT_BITMAP_CACHE
    return m_bitmapCache.sync() && m_dataCache.sync() && syncDevice();
#else   // USE_EXFAT_BITMAP_CACHE
    return m_dataCache.sync() && syncDevice();
#endif  // USE_EXFAT_BITMAP_CACHE
  }
  void dataCacheDirty() { m_dataCache.dirty(); }
  void dataCacheInvalidate() { m_dataCache.invalidate(); }
  uint8_t* dataCachePrepare(uint32_t sector, uint8_t option) {
    return m_dataCache.prepare(sector, option);
  }
  uint32_t dataCacheSector() { return m_dataCache.sector(); }
  bool dataCacheSync() { return m_dataCache.sync(); }
  //----------------------------------------------------------------------------
  uint32_t clusterMask() const { return m_clusterMask; }
  uint32_t clusterStartSector(uint32_t cluster) {
    return m_clusterHeapStartSector +
           ((cluster - 2) << m_sectorsPerClusterShift);
  }
  uint8_t* dirCache(DirPos_t* pos, uint8_t options);
  int8_t dirSeek(DirPos_t* pos, uint32_t offset);
  int8_t fatGet(uint32_t cluster, uint32_t* value);
  bool fatPut(uint32_t cluster, uint32_t value);
  uint32_t chainSize(uint32_t cluster);
  bool freeChain(uint32_t cluster);
  uint16_t sectorMask() const { return m_sectorMask; }
  bool syncDevice() { return m_blockDev->syncDevice(); }
  bool cacheSafeRead(uint32_t sector, uint8_t* dst) {
    return m_dataCache.cacheSafeRead(sector, dst);
  }
  bool cacheSafeWrite(uint32_t sector, const uint8_t* src) {
    return m_dataCache.cacheSafeWrite(sector, src);
  }
  bool cacheSafeRead(uint32_t sector, uint8_t* dst, size_t count) {
    return m_dataCache.cacheSafeRead(sector, dst, count);
  }
  bool cacheSafeWrite(uint32_t sector, const uint8_t* src, size_t count) {
    return m_dataCache.cacheSafeWrite(sector, src, count);
  }
  bool readSector(uint32_t sector, uint8_t* dst) {
    return m_blockDev->readSector(sector, dst);
  }
  bool writeSector(uint32_t sector, const uint8_t* src) {
    return m_blockDev->writeSector(sector, src);
  }
  //----------------------------------------------------------------------------
  static const uint8_t m_bytesPerSectorShift = 9;
  static const uint16_t m_bytesPerSector = 1 << m_bytesPerSectorShift;
  static const uint16_t m_sectorMask = m_bytesPerSector - 1;
  //----------------------------------------------------------------------------
#if USE_EXFAT_BITMAP_CACHE
  FsCache m_bitmapCache;
#endif  // USE_EXFAT_BITMAP_CACHE
  FsCache m_dataCache;
  uint32_t m_bitmapStart;
  uint32_t m_fatStartSector;
  uint32_t m_fatLength;
  uint32_t m_clusterHeapStartSector;
  uint32_t m_clusterCount;
  uint32_t m_rootDirectoryCluster;
  uint32_t m_clusterMask;
  uint32_t m_bytesPerCluster;
  FsBlockDevice* m_blockDev;
  uint8_t m_fatType = 0;
  uint8_t m_sectorsPerClusterShift;
};
#endif  // ExFatPartition_h
