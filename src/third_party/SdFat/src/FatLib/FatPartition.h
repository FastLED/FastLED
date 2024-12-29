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
#ifndef FatPartition_h
#define FatPartition_h
/**
 * \file
 * \brief FatPartition class
 */
#include <stddef.h>

#include "../common/FsBlockDevice.h"
#include "../common/FsCache.h"
#include "../common/FsStructs.h"
#include "../common/SysCall.h"

/** Type for FAT12 partition */
const uint8_t FAT_TYPE_FAT12 = 12;

/** Type for FAT12 partition */
const uint8_t FAT_TYPE_FAT16 = 16;

/** Type for FAT12 partition */
const uint8_t FAT_TYPE_FAT32 = 32;

//==============================================================================
/**
 * \class FatPartition
 * \brief Access FAT16 and FAT32 partitions on raw file devices.
 */
class FatPartition {
 public:
  /** Create an instance of FatPartition
   */
  FatPartition() = default;

  /** \return The shift count required to multiply by bytesPerCluster. */
  uint8_t bytesPerClusterShift() const {
    return m_sectorsPerClusterShift + m_bytesPerSectorShift;
  }
  /** \return Number of bytes in a cluster. */
  uint16_t bytesPerCluster() const {
    return m_bytesPerSector << m_sectorsPerClusterShift;
  }
  /** \return Number of bytes per sector. */
  uint16_t bytesPerSector() const { return m_bytesPerSector; }
  /** \return The shift count required to multiply by bytesPerCluster. */
  uint8_t bytesPerSectorShift() const { return m_bytesPerSectorShift; }
  /** \return Number of directory entries per cluster. */
  uint16_t dirEntriesPerCluster() const {
    return m_sectorsPerCluster * (m_bytesPerSector / FS_DIR_SIZE);
  }
  /** \return Mask for sector offset. */
  uint16_t sectorMask() const { return m_sectorMask; }
  /** \return The volume's cluster size in sectors. */
  uint8_t sectorsPerCluster() const { return m_sectorsPerCluster; }
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  uint8_t __attribute__((error("use sectorsPerCluster()"))) blocksPerCluster();
#endif  // DOXYGEN_SHOULD_SKIP_THIS
  /** \return The number of sectors in one FAT. */
  uint32_t sectorsPerFat() const { return m_sectorsPerFat; }
  /** Clear the cache and returns a pointer to the cache.  Not for normal apps.
   * \return A pointer to the cache buffer or zero if an error occurs.
   */
  uint8_t* cacheClear() { return m_cache.clear(); }
  /** \return The total number of clusters in the volume. */
  uint32_t clusterCount() const { return m_lastCluster - 1; }
  /** \return The shift count required to multiply by sectorsPerCluster. */
  uint8_t sectorsPerClusterShift() const { return m_sectorsPerClusterShift; }
  /** \return The logical sector number for the start of file data. */
  uint32_t dataStartSector() const { return m_dataStartSector; }
  /** End access to volume
   * \return pointer to sector size buffer for format.
   */
  uint8_t* end() {
    m_fatType = 0;
    return cacheClear();
  }
  /** \return The number of File Allocation Tables. */
  uint8_t fatCount() const { return 2; }
  /** \return The logical sector number for the start of the first FAT. */
  uint32_t fatStartSector() const { return m_fatStartSector; }
  /** \return The FAT type of the volume. Values are 12, 16 or 32. */
  uint8_t fatType() const { return m_fatType; }
  /** \return free cluster count or -1 if an error occurs. */
  int32_t freeClusterCount();
  /** Initialize a FAT partition.
   *
   * \param[in] dev FsBlockDevice for this partition.
   * \param[in] part The partition to be used.  Legal values for \a part are
   * 1-4 to use the corresponding partition on a device formatted with
   * a MBR, Master Boot Record, or zero if the device is formatted as
   * a super floppy with the FAT boot sector in sector volStart.
   * \param[in] volStart location of volume if part is zero.
   *
   * \return true for success or false for failure.
   */
  bool init(FsBlockDevice* dev, uint8_t part = 1, uint32_t volStart = 0);
  /** \return The number of entries in the root directory for FAT16 volumes. */
  uint16_t rootDirEntryCount() const { return m_rootDirEntryCount; }
  /** \return The logical sector number for the start of the root directory
       on FAT16 volumes or the first cluster number on FAT32 volumes. */
  uint32_t rootDirStart() const { return m_rootDirStart; }
  /** \return The number of sectors in the volume */
  uint32_t volumeSectorCount() const {
    return sectorsPerCluster() * clusterCount();
  }
  /** Debug access to FAT table
   *
   * \param[in] n cluster number.
   * \param[out] v value of entry
   * \return -1 error, 0 EOC, else 1.
   */
  int8_t dbgFat(uint32_t n, uint32_t* v) { return fatGet(n, v); }
  /**
   * Check for FsBlockDevice busy.
   *
   * \return true if busy else false.
   */
  bool isBusy() { return m_blockDev->isBusy(); }
  //----------------------------------------------------------------------------
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  bool dmpDirSector(print_t* pr, uint32_t sector);
  void dmpFat(print_t* pr, uint32_t start, uint32_t count);
  bool dmpRootDir(print_t* pr, uint32_t n = 0);
  void dmpSector(print_t* pr, uint32_t sector, uint8_t bits = 8);
#endif  // DOXYGEN_SHOULD_SKIP_THIS
  //----------------------------------------------------------------------------
 private:
  /** FatFile allowed access to private members. */
  friend class FatFile;
  //----------------------------------------------------------------------------
  static const uint8_t m_bytesPerSectorShift = 9;
  static const uint16_t m_bytesPerSector = 1 << m_bytesPerSectorShift;
  static const uint16_t m_sectorMask = m_bytesPerSector - 1;
  //----------------------------------------------------------------------------
  FsBlockDevice* m_blockDev;         // sector device
  uint8_t m_sectorsPerCluster;       // Cluster size in sectors.
  uint8_t m_clusterSectorMask;       // Mask to extract sector of cluster.
  uint8_t m_sectorsPerClusterShift;  // Cluster count to sector count shift.
  uint8_t m_fatType = 0;             // Volume type (12, 16, OR 32).
  uint16_t m_rootDirEntryCount;      // Number of entries in FAT16 root dir.
  uint32_t m_allocSearchStart;       // Start cluster for alloc search.
  uint32_t m_sectorsPerFat;          // FAT size in sectors
  uint32_t m_dataStartSector;        // First data sector number.
  uint32_t m_fatStartSector;         // Start sector for first FAT.
  uint32_t m_lastCluster;            // Last cluster number in FAT.
  uint32_t m_rootDirStart;           // Start sector FAT16, cluster FAT32.
  //----------------------------------------------------------------------------
  // sector I/O functions.
  bool cacheSafeRead(uint32_t sector, uint8_t* dst) {
    return m_cache.cacheSafeRead(sector, dst);
  }
  bool cacheSafeRead(uint32_t sector, uint8_t* dst, size_t count) {
    return m_cache.cacheSafeRead(sector, dst, count);
  }
  bool cacheSafeWrite(uint32_t sector, const uint8_t* dst) {
    return m_cache.cacheSafeWrite(sector, dst);
  }
  bool cacheSafeWrite(uint32_t sector, const uint8_t* dst, size_t count) {
    return m_cache.cacheSafeWrite(sector, dst, count);
  }
  bool syncDevice() { return m_blockDev->syncDevice(); }
#if MAINTAIN_FREE_CLUSTER_COUNT
  int32_t m_freeClusterCount;  // Count of free clusters in volume.
  void setFreeClusterCount(int32_t value) { m_freeClusterCount = value; }
  void updateFreeClusterCount(int32_t change) {
    if (m_freeClusterCount >= 0) {
      m_freeClusterCount += change;
    }
  }
#else   // MAINTAIN_FREE_CLUSTER_COUNT
  void setFreeClusterCount(int32_t value) { (void)value; }
  void updateFreeClusterCount(int32_t change) { (void)change; }
#endif  // MAINTAIN_FREE_CLUSTER_COUNT
        // sector caches
  FsCache m_cache;
  FsCache* dataCache() { return &m_cache; }
#if USE_SEPARATE_FAT_CACHE
  FsCache m_fatCache;
  uint8_t* fatCachePrepare(uint32_t sector, uint8_t options) {
    options |= FsCache::CACHE_STATUS_MIRROR_FAT;
    return m_fatCache.prepare(sector, options);
  }
  bool cacheSync() {
    return m_cache.sync() && m_fatCache.sync() && syncDevice();
  }
#else   // USE_SEPARATE_FAT_CACHE
  uint8_t* fatCachePrepare(uint32_t sector, uint8_t options) {
    options |= FsCache::CACHE_STATUS_MIRROR_FAT;
    return dataCachePrepare(sector, options);
  }
  bool cacheSync() { return m_cache.sync() && syncDevice(); }
#endif  // USE_SEPARATE_FAT_CACHE
  uint8_t* dataCachePrepare(uint32_t sector, uint8_t options) {
    return m_cache.prepare(sector, options);
  }
  bool cacheSyncData() { return m_cache.sync(); }
  uint8_t* cacheAddress() { return m_cache.cacheBuffer(); }
  uint32_t cacheSectorNumber() { return m_cache.sector(); }
  void cacheDirty() { m_cache.dirty(); }
  //----------------------------------------------------------------------------
  bool allocateCluster(uint32_t current, uint32_t* next);
  bool allocContiguous(uint32_t count, uint32_t* firstCluster);
  uint8_t sectorOfCluster(uint32_t position) const {
    return (position >> 9) & m_clusterSectorMask;
  }
  uint32_t clusterStartSector(uint32_t cluster) const {
    return m_dataStartSector + ((cluster - 2) << m_sectorsPerClusterShift);
  }
  int8_t fatGet(uint32_t cluster, uint32_t* value);
  bool fatPut(uint32_t cluster, uint32_t value);
  bool fatPutEOC(uint32_t cluster) { return fatPut(cluster, 0x0FFFFFFF); }
  bool freeChain(uint32_t cluster);
  bool isEOC(uint32_t cluster) const { return cluster > m_lastCluster; }
};
#endif  // FatPartition
