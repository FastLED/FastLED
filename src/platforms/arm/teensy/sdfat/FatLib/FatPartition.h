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
#ifndef FatPartition_h
#define FatPartition_h
/**
 * \file
 * \brief FatPartition class
 */
#include "fl/stl/stddef.h"
#include "fl/stl/int.h"
#include "platforms/arm/teensy/sdfat/common/SysCall.h"
#include "platforms/arm/teensy/sdfat/common/FsBlockDevice.h"
#include "platforms/arm/teensy/sdfat/common/FsCache.h"
#include "platforms/arm/teensy/sdfat/common/FsStructs.h"

namespace fl { namespace platforms { namespace teensy { namespace sdfat {

/** Type for FAT12 partition */
const fl::u8 FAT_TYPE_FAT12 = 12;

/** Type for FAT12 partition */
const fl::u8 FAT_TYPE_FAT16 = 16;

/** Type for FAT12 partition */
const fl::u8 FAT_TYPE_FAT32 = 32;

//==============================================================================
/**
 * \class FatPartition
 * \brief Access FAT16 and FAT32 partitions on raw file devices.
 */
class FatPartition {
 public:
  /** Create an instance of FatPartition
   */
  FatPartition() {}

  /** \return The shift count required to multiply by bytesPerCluster. */
  fl::u8 bytesPerClusterShift() const {
    return mSectorsPerClusterShift + mBytesPerSectorShift;
  }
  /** \return Number of bytes in a cluster. */
  fl::u16 bytesPerCluster() const {
    return mBytesPerSector << mSectorsPerClusterShift;
  }
  /** \return Number of bytes per sector. */
  fl::u16 bytesPerSector() const {
    return mBytesPerSector;
  }
  /** \return The shift count required to multiply by bytesPerCluster. */
  fl::u8 bytesPerSectorShift() const {
    return mBytesPerSectorShift;
  }
  /** \return Number of directory entries per sector. */
  fl::u16 dirEntriesPerCluster() const {
    return mSectorsPerCluster * (mBytesPerSector / FS_DIR_SIZE);
  }
  /** \return Mask for sector offset. */
  fl::u16 sectorMask() const {
    return mSectorMask;
  }
  /** \return The volume's cluster size in sectors. */
  fl::u8 sectorsPerCluster() const {
    return mSectorsPerCluster;
  }
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  fl::u8 __attribute__((error("use sectorsPerCluster()"))) blocksPerCluster();
#endif  // DOXYGEN_SHOULD_SKIP_THIS
  /** \return The number of sectors in one FAT. */
  fl::u32 sectorsPerFat() const {
    return mSectorsPerFat;
  }
  /** Clear the cache and returns a pointer to the cache.  Not for normal apps.
   * \return A pointer to the cache buffer or zero if an error occurs.
   */
  fl::u8* cacheClear() {
    return mCache.clear();
  }
  /** \return The total number of clusters in the volume. */
  fl::u32 clusterCount() const {
    return mLastCluster - 1;
  }
  /** \return The shift count required to multiply by sectorsPerCluster. */
  fl::u8 sectorsPerClusterShift() const {
    return mSectorsPerClusterShift;
  }
  /** \return The logical sector number for the start of file data. */
  fl::u32 dataStartSector() const {
    return mDataStartSector;
  }
  /** End access to volume
   * \return pointer to sector size buffer for format.
   */
  fl::u8* end() {
    mFatType = 0;
    return cacheClear();
  }
  /** \return The number of File Allocation Tables. */
  fl::u8 fatCount() const {
    return mFatCount;
  }
  /** \return The logical sector number for the start of the first FAT. */
  fl::u32 fatStartSector() const {
    return mFatStartSector;
  }
  /** \return The FAT type of the volume. Values are 12, 16 or 32. */
  fl::u8 fatType() const {
    return mFatType;
  }
  /** Volume free space in clusters.
   *
   * \return Count of free clusters for success or -1 if an error occurs.
   */
  fl::i32 freeClusterCount();
  /** Initialize a FAT partition.
   *
   * \param[in] dev FsBlockDevice for this partition.
   * \param[in] part The partition to be used.  Legal values for \a part are
   * 1-4 to use the corresponding partition on a device formatted with
   * a MBR, Master Boot Record, or zero if the device is formatted as
   * a super floppy with the FAT boot sector in sector zero.
   *
   * \return true for success or false for failure.
   */
  bool init(FsBlockDevice* dev, fl::u8 part = 1);
  bool init(FsBlockDevice* dev, fl::u32 firstSector, fl::u32 numSectors);
  /** \return The number of entries in the root directory for FAT16 volumes. */
  fl::u16 rootDirEntryCount() const {
    return mRootDirEntryCount;
  }
  /** \return The logical sector number for the start of the root directory
       on FAT16 volumes or the first cluster number on FAT32 volumes. */
  fl::u32 rootDirStart() const {
    return mRootDirStart;
  }
  /** \return The number of sectors in the volume */
  fl::u32 volumeSectorCount() const {
    return sectorsPerCluster() * clusterCount();
  }
  /** Debug access to FAT table
   *
   * \param[in] n cluster number.
   * \param[out] v value of entry
   * \return -1 error, 0 EOC, else 1.
   */
  fl::i8 dbgFat(fl::u32 n, fl::u32* v) {
    return fatGet(n, v);
  }
  /**
   * Check for FsBlockDevice busy.
   *
   * \return true if busy else false.
   */
  bool isBusy() { return mBlockDev->isBusy(); }
  //----------------------------------------------------------------------------
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  bool dmpDirSector(print_t* pr, fl::u32 sector);
  void dmpFat(print_t* pr, fl::u32 start, fl::u32 count);
  bool dmpRootDir(print_t* pr, fl::u32 n = 0);
  void dmpSector(print_t* pr, fl::u32 sector, fl::u8 bits = 8);
#endif  // DOXYGEN_SHOULD_SKIP_THIS
  //----------------------------------------------------------------------------
 private:
  /** FatFile allowed access to private members. */
  friend class FatFile;
  //----------------------------------------------------------------------------
  static const fl::u8 mBytesPerSectorShift = 9;
  static const fl::u16 mBytesPerSector = 1 << mBytesPerSectorShift;
  static const fl::u16 mSectorMask = mBytesPerSector - 1;
  //----------------------------------------------------------------------------
  FsBlockDevice* mBlockDev;            // sector device
  fl::u8 mSectorsPerCluster;           // Cluster size in sectors.
  fl::u8 mClusterSectorMask;           // Mask to extract sector of cluster.
  fl::u8 mSectorsPerClusterShift;      // Cluster count to sector count shift.
  fl::u8 mFatType = 0;                 // Volume type (12, 16, OR 32).
  fl::u8 mFatCount = 2;                // How many fats mostly 2 will support 1
  fl::u16 mRootDirEntryCount;          // Number of entries in FAT16 root dir.
  fl::u32 mAllocSearchStart;           // Start cluster for alloc search.
  fl::u32 mSectorsPerFat;              // FAT size in sectors
  fl::u32 mDataStartSector;            // First data sector number.
  fl::u32 mFatStartSector;             // Start sector for first FAT.
  fl::u32 mLastCluster;                // Last cluster number in FAT.
  fl::u32 mRootDirStart;               // Start sector FAT16, cluster FAT32.
  //----------------------------------------------------------------------------
  // sector I/O functions.
  bool cacheSafeRead(fl::u32 sector, fl::u8* dst) {
    return mCache.cacheSafeRead(sector, dst);
  }
  bool cacheSafeRead(fl::u32 sector, fl::u8* dst, fl::size_t count) {
    return mCache.cacheSafeRead(sector, dst, count);
  }
  bool cacheSafeWrite(fl::u32 sector, const fl::u8* dst) {
    return mCache.cacheSafeWrite(sector, dst);
  }
  bool cacheSafeWrite(fl::u32 sector, const fl::u8* dst, fl::size_t count) {
    return mCache.cacheSafeWrite(sector, dst, count);
  }
  bool syncDevice() {
    return mBlockDev->syncDevice();
  }
#if MAINTAIN_FREE_CLUSTER_COUNT
  fl::i32 mFreeClusterCount;  // Count of free clusters in volume.
  void setFreeClusterCount(fl::i32 value) {
    mFreeClusterCount = value;
  }
  void updateFreeClusterCount(fl::i32 change) {
    if (mFreeClusterCount >= 0) {
      mFreeClusterCount += change;
    }
  }
#else  // MAINTAIN_FREE_CLUSTER_COUNT
  void setFreeClusterCount(fl::i32 value) {
    (void)value;
  }
  void updateFreeClusterCount(fl::i32 change) {
    (void)change;
  }
#endif  // MAINTAIN_FREE_CLUSTER_COUNT
// sector caches
  FsCache mCache;
  bool cachePrepare(fl::u32 sector, fl::u8 option) {
    return mCache.prepare(sector, option);
  }
  FsCache* dataCache() { return &mCache; }
#if USE_SEPARATE_FAT_CACHE
  FsCache mFatCache;
  fl::u8* fatCachePrepare(fl::u32 sector, fl::u8 options) {
    if (mFatCount == 2) options |= FsCache::CACHE_STATUS_MIRROR_FAT;
    return mFatCache.prepare(sector, options);
  }
  bool cacheSync() {
    return mCache.sync() && mFatCache.sync() && syncDevice();
  }
#else  // USE_SEPARATE_FAT_CACHE
  fl::u8* fatCachePrepare(fl::u32 sector, fl::u8 options) {
    if (mFatCount == 2) options |= FsCache::CACHE_STATUS_MIRROR_FAT;
    return dataCachePrepare(sector, options);
  }
  bool cacheSync() {
    return mCache.sync() && syncDevice();
  }
#endif  // USE_SEPARATE_FAT_CACHE
  fl::u8* dataCachePrepare(fl::u32 sector, fl::u8 options) {
    return mCache.prepare(sector, options);
  }
  void cacheInvalidate() {
    mCache.invalidate();
  }
  bool cacheSyncData() {
    return mCache.sync();
  }
  fl::u8* cacheAddress() {
    return mCache.cacheBuffer();
  }
  fl::u32 cacheSectorNumber() {
    return mCache.sector();
  }
  void cacheDirty() {
    mCache.dirty();
  }
  //----------------------------------------------------------------------------
  bool allocateCluster(fl::u32 current, fl::u32* next);
  bool allocContiguous(fl::u32 count, fl::u32* firstCluster);
  fl::u8 sectorOfCluster(fl::u32 position) const {
    return (position >> 9) & mClusterSectorMask;
  }
  fl::u32 clusterStartSector(fl::u32 cluster) const {
    return mDataStartSector + ((cluster - 2) << mSectorsPerClusterShift);
  }
  fl::i8 fatGet(fl::u32 cluster, fl::u32* value);
  bool fatPut(fl::u32 cluster, fl::u32 value);
  bool fatPutEOC(fl::u32 cluster) {
    return fatPut(cluster, 0x0FFFFFFF);
  }
  bool freeChain(fl::u32 cluster);
  bool isEOC(fl::u32 cluster) const {
    return cluster > mLastCluster;
  }
  // freeClusterCount static helper functions
  static void freeClusterCount_cb_fat16(fl::u32 sector, fl::u8* buf, void* context);
  static void freeClusterCount_cb_fat32(fl::u32 sector, fl::u8* buf, void* context);
};

}  // namespace sdfat
}  // namespace teensy
}  // namespace platforms
}  // namespace fl

#endif  // FatPartition
