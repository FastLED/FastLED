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
#include <string.h>
#define DBG_FILE "FatPartition.cpp"
#include "../common/DebugMacros.h"
#include "FatLib.h"
//------------------------------------------------------------------------------
bool FatPartition::allocateCluster(uint32_t current, uint32_t* next) {
  uint32_t find;
  bool setStart;
  if (m_allocSearchStart < current) {
    // Try to keep file contiguous. Start just after current cluster.
    find = current;
    setStart = false;
  } else {
    find = m_allocSearchStart;
    setStart = true;
  }
  while (1) {
    find++;
    if (find > m_lastCluster) {
      if (setStart) {
        // Can't find space, checked all clusters.
        DBG_FAIL_MACRO;
        goto fail;
      }
      find = m_allocSearchStart;
      setStart = true;
      continue;
    }
    if (find == current) {
      // Can't find space, already searched clusters after current.
      DBG_FAIL_MACRO;
      goto fail;
    }
    uint32_t f;
    int8_t fg = fatGet(find, &f);
    if (fg < 0) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (fg && f == 0) {
      break;
    }
  }
  if (setStart) {
    m_allocSearchStart = find;
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
//------------------------------------------------------------------------------
// find a contiguous group of clusters
bool FatPartition::allocContiguous(uint32_t count, uint32_t* firstCluster) {
  // flag to save place to start next search
  bool setStart = true;
  // start of group
  uint32_t bgnCluster;
  // end of group
  uint32_t endCluster;
  // Start at cluster after last allocated cluster.
  endCluster = bgnCluster = m_allocSearchStart + 1;

  // search the FAT for free clusters
  while (1) {
    if (endCluster > m_lastCluster) {
      // Can't find space.
      DBG_FAIL_MACRO;
      goto fail;
    }
    uint32_t f;
    int8_t fg = fatGet(endCluster, &f);
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
    m_allocSearchStart = endCluster;
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
int8_t FatPartition::fatGet(uint32_t cluster, uint32_t* value) {
  uint32_t sector;
  uint32_t next;
  uint8_t* pc;

  // error if reserved cluster of beyond FAT
  if (cluster < 2 || cluster > m_lastCluster) {
    DBG_FAIL_MACRO;
    goto fail;
  }

  if (fatType() == 32) {
    sector = m_fatStartSector + (cluster >> (m_bytesPerSectorShift - 2));
    pc = fatCachePrepare(sector, FsCache::CACHE_FOR_READ);
    if (!pc) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    uint16_t offset = (cluster << 2) & m_sectorMask;
    next = getLe32(pc + offset);
  } else if (fatType() == 16) {
    cluster &= 0XFFFF;
    sector = m_fatStartSector + (cluster >> (m_bytesPerSectorShift - 1));
    pc = fatCachePrepare(sector, FsCache::CACHE_FOR_READ);
    if (!pc) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    uint16_t offset = (cluster << 1) & m_sectorMask;
    next = getLe16(pc + offset);
  } else if (FAT12_SUPPORT && fatType() == 12) {
    uint16_t index = cluster;
    index += index >> 1;
    sector = m_fatStartSector + (index >> m_bytesPerSectorShift);
    pc = fatCachePrepare(sector, FsCache::CACHE_FOR_READ);
    if (!pc) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    index &= m_sectorMask;
    uint16_t tmp = pc[index];
    index++;
    if (index == m_bytesPerSector) {
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
bool FatPartition::fatPut(uint32_t cluster, uint32_t value) {
  uint32_t sector;
  uint8_t* pc;

  // error if reserved cluster of beyond FAT
  if (cluster < 2 || cluster > m_lastCluster) {
    DBG_FAIL_MACRO;
    goto fail;
  }

  if (fatType() == 32) {
    sector = m_fatStartSector + (cluster >> (m_bytesPerSectorShift - 2));
    pc = fatCachePrepare(sector, FsCache::CACHE_FOR_WRITE);
    if (!pc) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    uint16_t offset = (cluster << 2) & m_sectorMask;
    setLe32(pc + offset, value);
    return true;
  }

  if (fatType() == 16) {
    cluster &= 0XFFFF;
    sector = m_fatStartSector + (cluster >> (m_bytesPerSectorShift - 1));
    pc = fatCachePrepare(sector, FsCache::CACHE_FOR_WRITE);
    if (!pc) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    uint16_t offset = (cluster << 1) & m_sectorMask;
    setLe16(pc + offset, value);
    return true;
  }

  if (FAT12_SUPPORT && fatType() == 12) {
    uint16_t index = cluster;
    index += index >> 1;
    sector = m_fatStartSector + (index >> m_bytesPerSectorShift);
    pc = fatCachePrepare(sector, FsCache::CACHE_FOR_WRITE);
    if (!pc) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    index &= m_sectorMask;
    uint8_t tmp = value;
    if (cluster & 1) {
      tmp = (pc[index] & 0XF) | tmp << 4;
    }
    pc[index] = tmp;

    index++;
    if (index == m_bytesPerSector) {
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
bool FatPartition::freeChain(uint32_t cluster) {
  uint32_t next;
  int8_t fg;
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
    if (cluster < m_allocSearchStart) {
      m_allocSearchStart = cluster - 1;
    }
    cluster = next;
  } while (fg);

  return true;

fail:
  return false;
}
//------------------------------------------------------------------------------
int32_t FatPartition::freeClusterCount() {
#if MAINTAIN_FREE_CLUSTER_COUNT
  if (m_freeClusterCount >= 0) {
    return m_freeClusterCount;
  }
#endif  // MAINTAIN_FREE_CLUSTER_COUNT
  uint32_t free = 0;
  uint32_t sector;
  uint32_t todo = m_lastCluster + 1;
  uint16_t n;

  if (FAT12_SUPPORT && fatType() == 12) {
    for (unsigned i = 2; i < todo; i++) {
      uint32_t c;
      int8_t fg = fatGet(i, &c);
      if (fg < 0) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      if (fg && c == 0) {
        free++;
      }
    }
  } else if (fatType() == 16 || fatType() == 32) {
    sector = m_fatStartSector;
    while (todo) {
      uint8_t* pc = fatCachePrepare(sector++, FsCache::CACHE_FOR_READ);
      if (!pc) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      n = fatType() == 16 ? m_bytesPerSector / 2 : m_bytesPerSector / 4;
      if (todo < n) {
        n = todo;
      }
      if (fatType() == 16) {
        uint16_t* p16 = reinterpret_cast<uint16_t*>(pc);
        for (uint16_t i = 0; i < n; i++) {
          if (p16[i] == 0) {
            free++;
          }
        }
      } else {
        uint32_t* p32 = reinterpret_cast<uint32_t*>(pc);
        for (uint16_t i = 0; i < n; i++) {
          if (p32[i] == 0) {
            free++;
          }
        }
      }
      todo -= n;
    }
  } else {
    // invalid FAT type
    DBG_FAIL_MACRO;
    goto fail;
  }
  setFreeClusterCount(free);
  return free;

fail:
  return -1;
}
//------------------------------------------------------------------------------
bool FatPartition::init(FsBlockDevice* dev, uint8_t part, uint32_t volStart) {
  uint32_t countOfClusters;
  uint32_t totalSectors;
  m_blockDev = dev;
  pbs_t* pbs;
  BpbFat32_t* bpb;
  MbrSector_t* mbr;
  uint8_t tmp;
  m_fatType = 0;
  m_allocSearchStart = 1;
  m_cache.init(dev);
#if USE_SEPARATE_FAT_CACHE
  m_fatCache.init(dev);
#endif  // USE_SEPARATE_FAT_CACHE
  // if part == 0 assume super floppy with FAT boot sector in sector zero
  // if part > 0 assume mbr volume with partition table
  if (part) {
    if (part > 4) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    mbr = reinterpret_cast<MbrSector_t*>(
        dataCachePrepare(0, FsCache::CACHE_FOR_READ));
    if (!mbr) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    MbrPart_t* mp = mbr->part + part - 1;
    if (mp->type == 0 || (mp->boot != 0 && mp->boot != 0X80)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    volStart = getLe32(mp->relativeSectors);
  }
  pbs = reinterpret_cast<pbs_t*>(
      dataCachePrepare(volStart, FsCache::CACHE_FOR_READ));
  if (!pbs) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  bpb = reinterpret_cast<BpbFat32_t*>(pbs->bpb);
  if (bpb->fatCount != 2 || getLe16(bpb->bytesPerSector) != m_bytesPerSector) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  m_sectorsPerCluster = bpb->sectorsPerCluster;
  m_clusterSectorMask = m_sectorsPerCluster - 1;
  // determine shift that is same as multiply by m_sectorsPerCluster
  m_sectorsPerClusterShift = 0;
  for (tmp = 1; m_sectorsPerCluster != tmp; tmp <<= 1) {
    if (tmp == 0) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    m_sectorsPerClusterShift++;
  }
  m_sectorsPerFat = getLe16(bpb->sectorsPerFat16);
  if (m_sectorsPerFat == 0) {
    m_sectorsPerFat = getLe32(bpb->sectorsPerFat32);
  }
  m_fatStartSector = volStart + getLe16(bpb->reservedSectorCount);

  // count for FAT16 zero for FAT32
  m_rootDirEntryCount = getLe16(bpb->rootDirEntryCount);

  // directory start for FAT16 dataStart for FAT32
  m_rootDirStart = m_fatStartSector + 2 * m_sectorsPerFat;
  // data start for FAT16 and FAT32
  m_dataStartSector =
      m_rootDirStart +
      ((FS_DIR_SIZE * m_rootDirEntryCount + m_bytesPerSector - 1) /
       m_bytesPerSector);

  // total sectors for FAT16 or FAT32
  totalSectors = getLe16(bpb->totalSectors16);
  if (totalSectors == 0) {
    totalSectors = getLe32(bpb->totalSectors32);
  }
  // total data sectors
  countOfClusters = totalSectors - (m_dataStartSector - volStart);

  // divide by cluster size to get cluster count
  countOfClusters >>= m_sectorsPerClusterShift;
  m_lastCluster = countOfClusters + 1;

  // Indicate unknown number of free clusters.
  setFreeClusterCount(-1);
  // FAT type is determined by cluster count
  if (countOfClusters < 4085) {
    m_fatType = 12;
    if (!FAT12_SUPPORT) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  } else if (countOfClusters < 65525) {
    m_fatType = 16;
  } else {
    m_rootDirStart = getLe32(bpb->fat32RootCluster);
    m_fatType = 32;
  }
  m_cache.setMirrorOffset(m_sectorsPerFat);
#if USE_SEPARATE_FAT_CACHE
  m_fatCache.setMirrorOffset(m_sectorsPerFat);
#endif  // USE_SEPARATE_FAT_CACHE
  return true;

fail:
  return false;
}
