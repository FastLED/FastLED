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
#define DBG_FILE "ExFatPartition.cpp"
#include "../common/DebugMacros.h"
#include "ExFatLib.h"
//------------------------------------------------------------------------------
// return 0 if error, 1 if no space, else start cluster.
uint32_t ExFatPartition::bitmapFind(uint32_t cluster, uint32_t count) {
  uint32_t start = cluster ? cluster - 2 : m_bitmapStart;
  if (start >= m_clusterCount) {
    start = 0;
  }
  uint32_t endAlloc = start;
  uint32_t bgnAlloc = start;
  uint16_t sectorSize = 1 << m_bytesPerSectorShift;
  size_t i = (start >> 3) & (sectorSize - 1);
  uint8_t* cache;
  uint8_t mask = 1 << (start & 7);
  while (true) {
    uint32_t sector =
        m_clusterHeapStartSector + (endAlloc >> (m_bytesPerSectorShift + 3));
    cache = bitmapCachePrepare(sector, FsCache::CACHE_FOR_READ);
    if (!cache) {
      return 0;
    }
    for (; i < sectorSize; i++) {
      for (; mask; mask <<= 1) {
        endAlloc++;
        if (!(mask & cache[i])) {
          if ((endAlloc - bgnAlloc) == count) {
            if (cluster == 0 && count == 1) {
              // Start at found sector.  bitmapModify may increase this.
              m_bitmapStart = bgnAlloc;
            }
            return bgnAlloc + 2;
          }
        } else {
          bgnAlloc = endAlloc;
        }
        if (endAlloc == start) {
          return 1;
        }
        if (endAlloc >= m_clusterCount) {
          endAlloc = bgnAlloc = 0;
          i = sectorSize;
          break;
        }
      }
      mask = 1;
    }
    i = 0;
  }
  return 0;
}
//------------------------------------------------------------------------------
bool ExFatPartition::bitmapModify(uint32_t cluster, uint32_t count,
                                  bool value) {
  uint32_t sector;
  uint32_t start = cluster - 2;
  size_t i;
  uint8_t* cache;
  uint8_t mask;
  cluster -= 2;
  if ((start + count) > m_clusterCount) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (value) {
    if (start <= m_bitmapStart && m_bitmapStart < (start + count)) {
      m_bitmapStart = (start + count) < m_clusterCount ? start + count : 0;
    }
  } else {
    if (start < m_bitmapStart) {
      m_bitmapStart = start;
    }
  }
  mask = 1 << (start & 7);
  sector = m_clusterHeapStartSector + (start >> (m_bytesPerSectorShift + 3));
  i = (start >> 3) & m_sectorMask;
  while (true) {
    cache = bitmapCachePrepare(sector++, FsCache::CACHE_FOR_WRITE);
    if (!cache) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    for (; i < m_bytesPerSector; i++) {
      for (; mask; mask <<= 1) {
        if (value == static_cast<bool>(cache[i] & mask)) {
          DBG_FAIL_MACRO;
          goto fail;
        }
        cache[i] ^= mask;
        if (--count == 0) {
          return true;
        }
      }
      mask = 1;
    }
    i = 0;
  }

fail:
  return false;
}
//------------------------------------------------------------------------------
uint32_t ExFatPartition::chainSize(uint32_t cluster) {
  uint32_t n = 0;
  int8_t status;
  do {
    status = fatGet(cluster, &cluster);
    if (status < 0) return 0;
    n++;
  } while (status);
  return n;
}
//------------------------------------------------------------------------------
uint8_t* ExFatPartition::dirCache(DirPos_t* pos, uint8_t options) {
  uint32_t sector = clusterStartSector(pos->cluster);
  sector += (m_clusterMask & pos->position) >> m_bytesPerSectorShift;
  uint8_t* cache = dataCachePrepare(sector, options);
  return cache ? cache + (pos->position & m_sectorMask) : nullptr;
}
//------------------------------------------------------------------------------
// return -1 error, 0 EOC, 1 OK
int8_t ExFatPartition::dirSeek(DirPos_t* pos, uint32_t offset) {
  int8_t status;
  uint32_t tmp = (m_clusterMask & pos->position) + offset;
  pos->position += offset;
  tmp >>= bytesPerClusterShift();
  while (tmp--) {
    if (pos->isContiguous) {
      pos->cluster++;
    } else {
      status = fatGet(pos->cluster, &pos->cluster);
      if (status != 1) {
        return status;
      }
    }
  }
  return 1;
}
//------------------------------------------------------------------------------
// return -1 error, 0 EOC, 1 OK
int8_t ExFatPartition::fatGet(uint32_t cluster, uint32_t* value) {
  uint8_t* cache;
  uint32_t next;
  uint32_t sector;

  if (cluster > (m_clusterCount + 1)) {
    DBG_FAIL_MACRO;
    return -1;
  }
  sector = m_fatStartSector + (cluster >> (m_bytesPerSectorShift - 2));

  cache = dataCachePrepare(sector, FsCache::CACHE_FOR_READ);
  if (!cache) {
    return -1;
  }
  next = getLe32(cache + ((cluster << 2) & m_sectorMask));
  if (next == EXFAT_EOC) {
    return 0;
  }
  *value = next;
  return 1;
}
//------------------------------------------------------------------------------
bool ExFatPartition::fatPut(uint32_t cluster, uint32_t value) {
  uint32_t sector;
  uint8_t* cache;
  if (cluster < 2 || cluster > (m_clusterCount + 1)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  sector = m_fatStartSector + (cluster >> (m_bytesPerSectorShift - 2));
  cache = dataCachePrepare(sector, FsCache::CACHE_FOR_WRITE);
  if (!cache) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  setLe32(cache + ((cluster << 2) & m_sectorMask), value);
  return true;

fail:
  return false;
}
//------------------------------------------------------------------------------
bool ExFatPartition::freeChain(uint32_t cluster) {
  uint32_t next;
  uint32_t start = cluster;
  int8_t status;
  do {
    status = fatGet(cluster, &next);
    if (status < 0) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (!fatPut(cluster, 0)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (status == 0 || (cluster + 1) != next) {
      if (!bitmapModify(start, cluster - start + 1, 0)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      start = next;
    }
    cluster = next;
  } while (status);

  return true;

fail:
  return false;
}
//------------------------------------------------------------------------------
int32_t ExFatPartition::freeClusterCount() {
  uint32_t nc = 0;
  uint32_t sector = m_clusterHeapStartSector;
  uint32_t usedCount = 0;
  uint8_t* cache;

  while (true) {
    cache = dataCachePrepare(sector++, FsCache::CACHE_FOR_READ);
    if (!cache) {
      return -1;
    }
    for (size_t i = 0; i < m_bytesPerSector; i++) {
      if (cache[i] == 0XFF) {
        usedCount += 8;
      } else if (cache[i]) {
        for (uint8_t mask = 1; mask; mask <<= 1) {
          if ((mask & cache[i])) {
            usedCount++;
          }
        }
      }
      nc += 8;
      if (nc >= m_clusterCount) {
        return m_clusterCount - usedCount;
      }
    }
  }
}
//------------------------------------------------------------------------------
bool ExFatPartition::init(FsBlockDevice* dev, uint8_t part, uint32_t volStart) {
  pbs_t* pbs;
  BpbExFat_t* bpb;
  MbrSector_t* mbr;
  m_fatType = 0;
  m_blockDev = dev;
  cacheInit(m_blockDev);
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
  if (strncmp(pbs->oemName, "EXFAT", 5)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  bpb = reinterpret_cast<BpbExFat_t*>(pbs->bpb);
  if (bpb->bytesPerSectorShift != m_bytesPerSectorShift) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  m_fatStartSector = volStart + getLe32(bpb->fatOffset);
  m_fatLength = getLe32(bpb->fatLength);
  m_clusterHeapStartSector = volStart + getLe32(bpb->clusterHeapOffset);
  m_clusterCount = getLe32(bpb->clusterCount);
  m_rootDirectoryCluster = getLe32(bpb->rootDirectoryCluster);
  m_sectorsPerClusterShift = bpb->sectorsPerClusterShift;
  m_bytesPerCluster = 1UL << (m_bytesPerSectorShift + m_sectorsPerClusterShift);
  m_clusterMask = m_bytesPerCluster - 1;
  // Set m_bitmapStart to first free cluster.
  m_bitmapStart = 0;
  bitmapFind(0, 1);
  m_fatType = FAT_TYPE_EXFAT;
  return true;

fail:
  return false;
}
//------------------------------------------------------------------------------
uint32_t ExFatPartition::rootLength() {
  uint32_t nc = chainSize(m_rootDirectoryCluster);
  return nc << bytesPerClusterShift();
}
