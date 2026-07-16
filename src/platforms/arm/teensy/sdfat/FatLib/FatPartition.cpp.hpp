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
// IWYU pragma: private
#include "fl/stl/int.h"
#define DBG_FILE "FatPartition.cpp"
#include "../common/DebugMacros.h"  // ok relative include; ok include path  // IWYU pragma: keep
#include "FatLib.h"  // ok include path - imported SdFat layout

namespace fl { namespace platforms { namespace teensy { namespace sdfat {
//------------------------------------------------------------------------------
bool FatPartition::allocateCluster(fl::u32 current, fl::u32* next) {
  fl::u32 find;
  bool setStart;
  if (mAllocSearchStart < current) {
    // Try to keep file contiguous. Start just after current cluster.
    find = current;
    setStart = false;
  } else {
    find = mAllocSearchStart;
    setStart = true;
  }
  while (1) {
    find++;
    if (find > mLastCluster) {
      if (setStart) {
        // Can't find space, checked all clusters.
        DBG_FAIL_MACRO;
        goto fail;
      }
      find = mAllocSearchStart;
      setStart = true;
      continue;
    }
    if (find == current) {
      // Can't find space, already searched clusters after current.
      DBG_FAIL_MACRO;
      goto fail;
    }
    fl::u32 f;
    fl::i8 fg = fatGet(find, &f);
    if (fg < 0) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (fg && f == 0) {
      break;
    }
  }
  if (setStart) {
    mAllocSearchStart = find;
  }
  // Mark end of chain.
  if (!fatPutEOC(find)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (current) {
    // Link clusters.
    if (!fatPut(current, find)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }
  updateFreeClusterCount(-1);
  *next = find;
  return true;

 fail:
  return false;
}

#ifdef DBG_FILE
#undef DBG_FILE
#endif

//------------------------------------------------------------------------------
// find a contiguous group of clusters
bool FatPartition::allocContiguous(fl::u32 count, fl::u32* firstCluster) {
  // flag to save place to start next search
  bool setStart = true;
  // start of group
  fl::u32 bgnCluster;
  // end of group
  fl::u32 endCluster;
  // Start at cluster after last allocated cluster.
  endCluster = bgnCluster = mAllocSearchStart + 1;

  // search the FAT for free clusters
  while (1) {
    if (endCluster > mLastCluster) {
      // Can't find space.
      DBG_FAIL_MACRO;
      goto fail;
    }
    fl::u32 f;
    fl::i8 fg = fatGet(endCluster, &f);
    if (fg < 0) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (f || fg == 0) {
      // don't update search start if unallocated clusters before endCluster.
      if (bgnCluster != endCluster) {
        setStart = false;
      }
      // cluster in use try next cluster as bgnCluster
      bgnCluster = endCluster + 1;
    } else if ((endCluster - bgnCluster + 1) == count) {
      // done - found space
      break;
    }
    endCluster++;
  }
  // Remember possible next free cluster.
  if (setStart) {
    mAllocSearchStart = endCluster;
  }
  // mark end of chain
  if (!fatPutEOC(endCluster)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // link clusters
  while (endCluster > bgnCluster) {
    if (!fatPut(endCluster - 1, endCluster)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    endCluster--;
  }
  // Maintain count of free clusters.
  updateFreeClusterCount(-count);

  // return first cluster number to caller
  *firstCluster = bgnCluster;
  return true;

 fail:
  return false;
}

//------------------------------------------------------------------------------
// Fetch a FAT entry - return -1 error, 0 EOC, else 1.
fl::i8 FatPartition::fatGet(fl::u32 cluster, fl::u32* value) {
  fl::u32 sector;
  fl::u32 next;
  fl::u8* pc;

  // error if reserved cluster of beyond FAT
  if (cluster < 2 || cluster > mLastCluster) {
    DBG_FAIL_MACRO;
    goto fail;
  }

  if (fatType() == 32) {
    sector = mFatStartSector + (cluster >> (mBytesPerSectorShift - 2));
    pc = fatCachePrepare(sector, FsCache::CACHE_FOR_READ);
    if (!pc) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    fl::u16 offset = (cluster << 2) & mSectorMask;
    next = getLe32(pc + offset);
  } else if (fatType() == 16) {
    cluster &= 0XFFFF;
    sector = mFatStartSector + (cluster >> (mBytesPerSectorShift - 1) );
    pc = fatCachePrepare(sector, FsCache::CACHE_FOR_READ);
    if (!pc) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    fl::u16 offset = (cluster << 1) & mSectorMask;
    next = getLe16(pc + offset);
  } else if (FAT12_SUPPORT && fatType() == 12) {
    fl::u16 index = cluster;
    index += index >> 1;
    sector = mFatStartSector + (index >> mBytesPerSectorShift);
    pc = fatCachePrepare(sector, FsCache::CACHE_FOR_READ);
    if (!pc) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    index &= mSectorMask;
    fl::u16 tmp = pc[index];
    index++;
    if (index == mBytesPerSector) {
      pc = fatCachePrepare(sector + 1, FsCache::CACHE_FOR_READ);
      if (!pc) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      index = 0;
    }
    tmp |= pc[index] << 8;
    next = cluster & 1 ? tmp >> 4 : tmp & 0XFFF;
  } else {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (isEOC(next)) {
    return 0;
  }
  *value = next;
  return 1;

 fail:
  return -1;
}
//------------------------------------------------------------------------------
// Store a FAT entry
bool FatPartition::fatPut(fl::u32 cluster, fl::u32 value) {
  fl::u32 sector;
  fl::u8* pc;

  // error if reserved cluster of beyond FAT
  if (cluster < 2 || cluster > mLastCluster) {
    DBG_FAIL_MACRO;
    goto fail;
  }

  if (fatType() == 32) {
    sector = mFatStartSector + (cluster >> (mBytesPerSectorShift - 2));
    pc = fatCachePrepare(sector, FsCache::CACHE_FOR_WRITE);
    if (!pc) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    fl::u16 offset = (cluster << 2) & mSectorMask;
    setLe32(pc + offset, value);
    return true;
  }

  if (fatType() == 16) {
    cluster &= 0XFFFF;
    sector = mFatStartSector + (cluster >> (mBytesPerSectorShift - 1) );
    pc = fatCachePrepare(sector, FsCache::CACHE_FOR_WRITE);
    if (!pc) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    fl::u16 offset = (cluster << 1) & mSectorMask;
    setLe16(pc + offset, value);
    return true;
  }

  if (FAT12_SUPPORT && fatType() == 12) {
    fl::u16 index = cluster;
    index += index >> 1;
    sector = mFatStartSector + (index >> mBytesPerSectorShift);
    pc = fatCachePrepare(sector, FsCache::CACHE_FOR_WRITE);
    if (!pc) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    index &= mSectorMask;
    fl::u8 tmp = value;
    if (cluster & 1) {
      tmp = (pc[index] & 0XF) | tmp << 4;
    }
    pc[index] = tmp;

    index++;
    if (index == mBytesPerSector) {
      sector++;
      index = 0;
      pc = fatCachePrepare(sector, FsCache::CACHE_FOR_WRITE);
      if (!pc) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    }
    tmp = value >> 4;
    if (!(cluster & 1)) {
      tmp = ((pc[index] & 0XF0)) | tmp >> 4;
    }
    pc[index] = tmp;
    return true;
  } else {
    DBG_FAIL_MACRO;
    goto fail;
  }

 fail:
  return false;
}
//------------------------------------------------------------------------------
// free a cluster chain
bool FatPartition::freeChain(fl::u32 cluster) {
  fl::u32 next;
  fl::i8 fg;
  do {
    fg = fatGet(cluster, &next);
    if (fg < 0) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    // free cluster
    if (!fatPut(cluster, 0)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    // Add one to count of free clusters.
    updateFreeClusterCount(1);
    if (cluster < mAllocSearchStart) {
      mAllocSearchStart = cluster - 1;
    }
    cluster = next;
  } while (fg);

  return true;

 fail:
  return false;
}

// Structure to use for doing free cluster count using callbacks
struct FreeClusterCountStruct {
  fl::u32 clusters_to_do;
  fl::u32 free_count;
};

//------------------------------------------------------------------------------
void FatPartition::freeClusterCount_cb_fat16(fl::u32 sector, fl::u8 *buf, void *context) {
   struct FreeClusterCountStruct *state = (struct FreeClusterCountStruct *)context;
  fl::u16 *p = (fl::u16 *)buf;
  fl::u32 n = state->clusters_to_do;
  if (n > 256) n = 256;
  fl::u16 *e = p + n;
  while (p < e) {
    if (*p++ == 0) state->free_count++;
  }
  state->clusters_to_do -= n;
}

//------------------------------------------------------------------------------
void FatPartition::freeClusterCount_cb_fat32(fl::u32 sector, fl::u8 *buf, void *context) {
  struct FreeClusterCountStruct *state = (struct FreeClusterCountStruct *)context;
  fl::u32 *p = (fl::u32 *)buf;
  fl::u32 n = state->clusters_to_do;
  if (n > 128) n = 128;
  fl::u32 *e = p + n;
  while (p < e) {
    if (*p++ == 0) state->free_count++;
  }
  state->clusters_to_do -= n;
}

//------------------------------------------------------------------------------
fl::i32 FatPartition::freeClusterCount() {
#if MAINTAIN_FREE_CLUSTER_COUNT
  if (mFreeClusterCount >= 0) {
    return mFreeClusterCount;
  }
#endif  // MAINTAIN_FREE_CLUSTER_COUNT
 if (FAT12_SUPPORT && fatType() == 12) {
    fl::u32 free = 0;
    fl::u32 todo = mLastCluster + 1;
    for (unsigned i = 2; i < todo; i++) {
      fl::u32 c;
      fl::i8 fg = fatGet(i, &c);
      if (fg < 0) {
        DBG_FAIL_MACRO;
        return -1;
      }
      if (fg && c == 0) {
        free++;
      }
    }
    return free;
  }

  struct FreeClusterCountStruct state;

  state.free_count = 0;
  state.clusters_to_do = mLastCluster + 1;

  fl::u32 num_sectors;

  //num_sectors = SD.sdfs.m_fVol->sectorsPerFat(); // edit FsVolume.h for public
  //Serial.printf("  num_sectors = %u\n", num_sectors);

  num_sectors = mSectorsPerFat;
  //Serial.printf("  num_sectors = %u\n", num_sectors);
#if USE_SEPARATE_FAT_CACHE
  fl::u8 *buf = mFatCache.clear();  // will clear out anything and return buffer
#else
  fl::u8 *buf = mCache.clear();  // will clear out anything and return buffer
#endif  // USE_SEPARATE_FAT_CACHE
  if (buf == nullptr) return -1;
  if (fatType() == FAT_TYPE_FAT32) {
    if (!mBlockDev->readSectorsCallback(mFatStartSector, buf, num_sectors, freeClusterCount_cb_fat32, &state)) return -1;
  } else {
    if (!mBlockDev->readSectorsCallback(mFatStartSector, buf, num_sectors, freeClusterCount_cb_fat16, &state)) return -1;
  }

  setFreeClusterCount(state.free_count);
  return state.free_count;
}


//------------------------------------------------------------------------------
bool FatPartition::init(FsBlockDevice* dev, fl::u8 part) {
//  Serial.printf(" FatPartition::init(%x %u)\n", (uint32_t)dev, part);
  fl::u32 clusterCount;
  fl::u32 totalSectors;
  fl::u32 volumeStartSector = 0;
  mBlockDev = dev;
  pbs_t* pbs;
  BpbFat32_t* bpb;
  MbrSector_t* mbr;
  fl::u8 tmp;
  mFatType = 0;
  mAllocSearchStart = 1;
  mCache.init(dev);
#if USE_SEPARATE_FAT_CACHE
  mFatCache.init(dev);
#endif  // USE_SEPARATE_FAT_CACHE
  // if part == 0 assume super floppy with FAT boot sector in sector zero
  // if part > 0 assume mbr volume with partition table
  if (part) {
    if (part > 4) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    mbr = reinterpret_cast<MbrSector_t*>  // ok reinterpret cast - imported SdFat packed layout
          (dataCachePrepare(0, FsCache::CACHE_FOR_READ));
    MbrPart_t* mp = mbr->part + part - 1;

    if (!mbr || mp->type == 0 || (mp->boot != 0 && mp->boot != 0X80)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    volumeStartSector = getLe32(mp->relativeSectors);
  }
  pbs = reinterpret_cast<pbs_t*>  // ok reinterpret cast - imported SdFat packed layout
        (dataCachePrepare(volumeStartSector, FsCache::CACHE_FOR_READ));
  bpb = reinterpret_cast<BpbFat32_t*>(pbs->bpb);  // ok reinterpret cast - imported SdFat packed layout
  if (!pbs || getLe16(bpb->bytesPerSector) != mBytesPerSector) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // handle fat counts 1 or 2...
  mFatCount = bpb->fatCount;
  if ((mFatCount != 1) && (mFatCount != 2)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  mSectorsPerCluster = bpb->sectorsPerCluster;
  mClusterSectorMask = mSectorsPerCluster - 1;
  // determine shift that is same as multiply by mSectorsPerCluster
  mSectorsPerClusterShift = 0;
  for (tmp = 1; mSectorsPerCluster != tmp; tmp <<= 1) {
    if (tmp == 0) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    mSectorsPerClusterShift++;
  }
  mSectorsPerFat = getLe16(bpb->sectorsPerFat16);
  if (mSectorsPerFat == 0) {
    mSectorsPerFat = getLe32(bpb->sectorsPerFat32);
  }
  mFatStartSector = volumeStartSector + getLe16(bpb->reservedSectorCount);

  // count for FAT16 zero for FAT32
  mRootDirEntryCount = getLe16(bpb->rootDirEntryCount);

  // directory start for FAT16 dataStart for FAT32
  mRootDirStart = mFatStartSector + bpb->fatCount * mSectorsPerFat;
  // data start for FAT16 and FAT32
  mDataStartSector = mRootDirStart +
    ((FS_DIR_SIZE*mRootDirEntryCount + mBytesPerSector - 1)/mBytesPerSector);

  // total sectors for FAT16 or FAT32
  totalSectors = getLe16(bpb->totalSectors16);
  if (totalSectors == 0) {
    totalSectors = getLe32(bpb->totalSectors32);
  }
  // total data sectors
  clusterCount = totalSectors - (mDataStartSector - volumeStartSector);

  // divide by cluster size to get cluster count
  clusterCount >>= mSectorsPerClusterShift;
  mLastCluster = clusterCount + 1;

  // Indicate unknown number of free clusters.
  setFreeClusterCount(-1);
  // FAT type is determined by cluster count
  if (clusterCount < 4085) {
    mFatType = 12;
    if (!FAT12_SUPPORT) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  } else if (clusterCount < 65525) {
    mFatType = 16;
  } else {
    mRootDirStart = getLe32(bpb->fat32RootCluster);
    mFatType = 32;
  }
  mCache.setMirrorOffset(mSectorsPerFat);
#if USE_SEPARATE_FAT_CACHE
  mFatCache.setMirrorOffset(mSectorsPerFat);
#endif  // USE_SEPARATE_FAT_CACHE
  return true;

 fail:
  return false;
}

#if defined(DBG_FILE)
#undef DBG_FILE
#endif

//------------------------------------------------------------------------------
bool FatPartition::init(FsBlockDevice* dev, fl::u32 firstSector, fl::u32 numSectors) {
//  Serial.printf(" FatPartition::init(%x %u %u)\n", (uint32_t)dev, firstSector, numSectors);
  fl::u32 clusterCount;
  fl::u32 totalSectors;
  fl::u32 volumeStartSector = firstSector;
  mBlockDev = dev;
  pbs_t* pbs;
  BpbFat32_t* bpb;
  fl::u8 tmp;

  mFatType = 0;
  mAllocSearchStart = 1;
  mCache.init(dev);
#if USE_SEPARATE_FAT_CACHE
  mFatCache.init(dev);
#endif  // USE_SEPARATE_FAT_CACHE
  pbs = reinterpret_cast<pbs_t*>  // ok reinterpret cast - imported SdFat packed layout
        (dataCachePrepare(volumeStartSector, FsCache::CACHE_FOR_READ));
  bpb = reinterpret_cast<BpbFat32_t*>(pbs->bpb);  // ok reinterpret cast - imported SdFat packed layout
  if (!pbs || getLe16(bpb->bytesPerSector) != mBytesPerSector) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  mFatCount = bpb->fatCount;
  // handle fat counts 1 or 2...
  if ((mFatCount != 1) && (mFatCount != 2)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  mSectorsPerCluster = bpb->sectorsPerCluster;
  mClusterSectorMask = mSectorsPerCluster - 1;
  // determine shift that is same as multiply by mSectorsPerCluster
  mSectorsPerClusterShift = 0;
  for (tmp = 1; mSectorsPerCluster != tmp; tmp <<= 1) {
    if (tmp == 0) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    mSectorsPerClusterShift++;
  }
  mSectorsPerFat = getLe16(bpb->sectorsPerFat16);
  if (mSectorsPerFat == 0) {
    mSectorsPerFat = getLe32(bpb->sectorsPerFat32);
  }
  mFatStartSector = volumeStartSector + getLe16(bpb->reservedSectorCount);

  // count for FAT16 zero for FAT32
  mRootDirEntryCount = getLe16(bpb->rootDirEntryCount);

  // directory start for FAT16 dataStart for FAT32
  mRootDirStart = mFatStartSector + bpb->fatCount * mSectorsPerFat;
  // data start for FAT16 and FAT32
  mDataStartSector = mRootDirStart +
    ((FS_DIR_SIZE*mRootDirEntryCount + mBytesPerSector - 1)/mBytesPerSector);

  // total sectors for FAT16 or FAT32
  totalSectors = getLe16(bpb->totalSectors16);
  if (totalSectors == 0) {
    totalSectors = getLe32(bpb->totalSectors32);
  }
  if (totalSectors > numSectors) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // total data sectors
  clusterCount = totalSectors - (mDataStartSector - volumeStartSector);

  // divide by cluster size to get cluster count
  clusterCount >>= mSectorsPerClusterShift;
  mLastCluster = clusterCount + 1;

  // Indicate unknown number of free clusters.
  setFreeClusterCount(-1);
  // FAT type is determined by cluster count
  if (clusterCount < 4085) {
    mFatType = 12;
    if (!FAT12_SUPPORT) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  } else if (clusterCount < 65525) {
    mFatType = 16;
  } else {
    mRootDirStart = getLe32(bpb->fat32RootCluster);
    mFatType = 32;
  }
  mCache.setMirrorOffset(mSectorsPerFat);
#if USE_SEPARATE_FAT_CACHE
  mFatCache.setMirrorOffset(mSectorsPerFat);
#endif  // USE_SEPARATE_FAT_CACHE
  return true;

 fail:
  return false;
}

}  // namespace sdfat
}  // namespace teensy
}  // namespace platforms
}  // namespace fl
