/**
 * Copyright (c) 2011-2024 Bill Greiman
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
#define DBG_FILE "ExFatFileWrite.cpp"
#include "../common/DebugMacros.h"
#include "ExFatLib.h"
//==============================================================================
#if EXFAT_READ_ONLY
bool ExFatFile::mkdir(ExFatFile* parent, const char* path, bool pFlag) {
  (void)parent;
  (void)path;
  (void)pFlag;
  return false;
}
bool ExFatFile::preAllocate(uint64_t length) {
  (void)length;
  return false;
}
bool ExFatFile::rename(const char* newPath) {
  (void)newPath;
  return false;
}
bool ExFatFile::rename(ExFatFile* dirFile, const char* newPath) {
  (void)dirFile;
  (void)newPath;
  return false;
}
bool ExFatFile::sync() { return false; }
bool ExFatFile::truncate() { return false; }
size_t ExFatFile::write(const void* buf, size_t nbyte) {
  (void)buf;
  (void)nbyte;
  return false;
}
//==============================================================================
#else  // EXFAT_READ_ONLY
//------------------------------------------------------------------------------
static uint16_t exFatDirChecksum(const uint8_t* data, uint16_t checksum) {
  bool skip = data[0] == EXFAT_TYPE_FILE;
  for (size_t i = 0; i < 32; i += i == 1 && skip ? 3 : 1) {
    checksum = ((checksum << 15) | (checksum >> 1)) + data[i];
  }
  return checksum;
}
//------------------------------------------------------------------------------
bool ExFatFile::addCluster() {
  uint32_t find = m_vol->bitmapFind(m_curCluster ? m_curCluster + 1 : 0, 1);
  if (find < 2) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (!m_vol->bitmapModify(find, 1, 1)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (m_curCluster == 0) {
    m_flags |= FILE_FLAG_CONTIGUOUS;
    goto done;
  }
  if (isContiguous()) {
    if (find == (m_curCluster + 1)) {
      goto done;
    }
    // No longer contiguous so make FAT chain.
    m_flags &= ~FILE_FLAG_CONTIGUOUS;

    for (uint32_t c = m_firstCluster; c < m_curCluster; c++) {
      if (!m_vol->fatPut(c, c + 1)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    }
  }
  // New cluster is EOC.
  if (!m_vol->fatPut(find, EXFAT_EOC)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // Connect new cluster to existing chain.
  if (m_curCluster) {
    if (!m_vol->fatPut(m_curCluster, find)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }

done:
  m_curCluster = find;
  return true;

fail:
  return false;
}
//------------------------------------------------------------------------------
bool ExFatFile::addDirCluster() {
  uint32_t sector;
  uint32_t dl = isRoot() ? m_vol->rootLength() : m_dataLength;
  uint8_t* cache;
  dl += m_vol->bytesPerCluster();
  if (dl >= 0X4000000) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (!addCluster()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  sector = m_vol->clusterStartSector(m_curCluster);
  for (uint32_t i = 0; i < m_vol->sectorsPerCluster(); i++) {
    cache =
        m_vol->dataCachePrepare(sector + i, FsCache::CACHE_RESERVE_FOR_WRITE);
    if (!cache) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    memset(cache, 0, m_vol->bytesPerSector());
  }
  if (!isRoot()) {
    m_flags |= FILE_FLAG_DIR_DIRTY;
    m_dataLength += m_vol->bytesPerCluster();
    m_validLength += m_vol->bytesPerCluster();
  }
  return sync();

fail:
  return false;
}
//------------------------------------------------------------------------------
bool ExFatFile::mkdir(ExFatFile* parent, const char* path, bool pFlag) {
  ExName_t fname;
  ExFatFile tmpDir;

  if (isOpen() || !parent->isDir()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (isDirSeparator(*path)) {
    while (isDirSeparator(*path)) {
      path++;
    }
    if (!tmpDir.openRoot(parent->m_vol)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    parent = &tmpDir;
  }
  while (1) {
    if (!parsePathName(path, &fname, &path)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (!*path) {
      break;
    }
    if (!openPrivate(parent, &fname, O_RDONLY)) {
      if (!pFlag || !mkdir(parent, &fname)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    }
    // tmpDir = *this;
    tmpDir.copy(this);
    parent = &tmpDir;
    close();
  }
  return mkdir(parent, &fname);

fail:
  return false;
}
//------------------------------------------------------------------------------
bool ExFatFile::mkdir(ExFatFile* parent, ExName_t* fname) {
  if (!parent->isDir()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // create a normal file
  if (!openPrivate(parent, fname, O_CREAT | O_EXCL | O_RDWR)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // convert file to directory
  m_attributes = FILE_ATTR_SUBDIR | FS_ATTRIB_ARCHIVE;

  // allocate and zero first cluster
  if (!addDirCluster()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  m_firstCluster = m_curCluster;

  // Set to start of dir
  rewind();
  m_flags = FILE_FLAG_READ | FILE_FLAG_CONTIGUOUS | FILE_FLAG_DIR_DIRTY;
  return sync();

fail:
  return false;
}
//------------------------------------------------------------------------------
bool ExFatFile::preAllocate(uint64_t length) {
  uint32_t find;
  uint32_t need;
  if (!length || !isWritable() || m_firstCluster) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  need = 1 + ((length - 1) >> m_vol->bytesPerClusterShift());
  find = m_vol->bitmapFind(0, need);
  if (find < 2) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (!m_vol->bitmapModify(find, need, 1)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  m_dataLength = length;
  m_firstCluster = find;
  m_flags |= FILE_FLAG_DIR_DIRTY | FILE_FLAG_CONTIGUOUS;
  if (!sync()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  return true;

fail:
  return false;
}
//------------------------------------------------------------------------------
bool ExFatFile::remove() {
  uint8_t* cache;
  if (!isWritable()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // Free any clusters.
  if (m_firstCluster) {
    if (isContiguous()) {
      uint32_t nc = 1 + ((m_dataLength - 1) >> m_vol->bytesPerClusterShift());
      if (!m_vol->bitmapModify(m_firstCluster, nc, 0)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    } else {
      if (!m_vol->freeChain(m_firstCluster)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    }
  }

  for (uint8_t is = 0; is <= m_setCount; is++) {
    cache = dirCache(is, FsCache::CACHE_FOR_WRITE);
    if (!cache) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    // Mark entry not used.
    cache[0] &= 0x7F;
  }
  // Set this file closed.
  m_attributes = FILE_ATTR_CLOSED;
  m_flags = 0;

  // Write entry to device.
  return m_vol->cacheSync();

fail:
  return false;
}
//------------------------------------------------------------------------------
bool ExFatFile::rename(const char* newPath) {
  return rename(m_vol->vwd(), newPath);
}
//------------------------------------------------------------------------------
bool ExFatFile::rename(ExFatFile* dirFile, const char* newPath) {
  ExFatFile file;
  ExFatFile oldFile;

  // Must be an open file or subdirectory.
  if (!(isFile() || isSubDir())) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // Can't move file to new volume.
  if (m_vol != dirFile->m_vol) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (!file.open(dirFile, newPath, O_CREAT | O_EXCL | O_WRONLY)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // oldFile = *this;
  oldFile.copy(this);
  m_dirPos = file.m_dirPos;
  m_setCount = file.m_setCount;
  m_flags |= FILE_FLAG_DIR_DIRTY;
  if (!sync()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // Remove old directory entry;
  oldFile.m_firstCluster = 0;
  oldFile.m_flags = FILE_FLAG_WRITE;
  oldFile.m_attributes = FILE_ATTR_FILE;
  return oldFile.remove();

fail:
  return false;
}
//------------------------------------------------------------------------------
bool ExFatFile::rmdir() {
  int n;
  uint8_t dir[FS_DIR_SIZE];
  // must be open subdirectory
  if (!isSubDir()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  rewind();

  // make sure directory is empty
  while (1) {
    n = read(dir, FS_DIR_SIZE);
    if (n == 0) {
      break;
    }
    if (n != FS_DIR_SIZE || dir[0] & 0X80) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (dir[0] == 0) {
      break;
    }
  }
  // convert empty directory to normal file for remove
  m_attributes = FILE_ATTR_FILE;
  m_flags |= FILE_FLAG_WRITE;
  return remove();

fail:
  return false;
}
//------------------------------------------------------------------------------
bool ExFatFile::sync() {
  if (!isOpen()) {
    return true;
  }
  if (m_flags & FILE_FLAG_DIR_DIRTY) {
    // clear directory dirty
    m_flags &= ~FILE_FLAG_DIR_DIRTY;
    return syncDir();
  }
  if (!m_vol->cacheSync()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  return true;

fail:
  m_error |= WRITE_ERROR;
  return false;
}
//------------------------------------------------------------------------------
bool ExFatFile::syncDir() {
  DirFile_t* df;
  DirStream_t* ds;
  uint8_t* cache;
  uint16_t checksum = 0;

  for (uint8_t is = 0; is <= m_setCount; is++) {
    cache = dirCache(is, FsCache::CACHE_FOR_READ);
    if (!cache) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    switch (cache[0]) {
      case EXFAT_TYPE_FILE:
        df = reinterpret_cast<DirFile_t*>(cache);
        setLe16(df->attributes, m_attributes & FS_ATTRIB_COPY);
        if (FsDateTime::callback) {
          uint16_t date, time;
          uint8_t ms10;
          FsDateTime::callback(&date, &time, &ms10);
          df->modifyTimeMs = ms10;
          setLe16(df->modifyTime, time);
          setLe16(df->modifyDate, date);
          setLe16(df->accessTime, time);
          setLe16(df->accessDate, date);
        }
        m_vol->dataCacheDirty();
        break;

      case EXFAT_TYPE_STREAM:
        ds = reinterpret_cast<DirStream_t*>(cache);
        if (isContiguous()) {
          ds->flags |= EXFAT_FLAG_CONTIGUOUS;
        } else {
          ds->flags &= ~EXFAT_FLAG_CONTIGUOUS;
        }
        setLe64(ds->validLength, m_validLength);
        setLe32(ds->firstCluster, m_firstCluster);
        setLe64(ds->dataLength, m_dataLength);
        m_vol->dataCacheDirty();
        break;

      case EXFAT_TYPE_NAME:
        break;

      default:
        DBG_FAIL_MACRO;
        goto fail;
        break;
    }
    checksum = exFatDirChecksum(cache, checksum);
  }
  df = reinterpret_cast<DirFile_t*>(
      m_vol->dirCache(&m_dirPos, FsCache::CACHE_FOR_WRITE));
  if (!df) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  setLe16(df->setChecksum, checksum);
  if (!m_vol->cacheSync()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  return true;

fail:
  m_error |= WRITE_ERROR;
  return false;
}
//------------------------------------------------------------------------------
bool ExFatFile::timestamp(uint8_t flags, uint16_t year, uint8_t month,
                          uint8_t day, uint8_t hour, uint8_t minute,
                          uint8_t second) {
  DirFile_t* df;
  uint8_t* cache;
  uint16_t checksum = 0;
  uint16_t date;
  uint16_t time;
  uint8_t ms10;

  if (!isFileOrSubDir() || year < 1980 || year > 2107 || month < 1 ||
      month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 ||
      second > 59) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // update directory entry
  if (!sync()) {
    DBG_FAIL_MACRO;
    goto fail;
  }

  date = FS_DATE(year, month, day);
  time = FS_TIME(hour, minute, second);
  ms10 = second & 1 ? 100 : 0;

  for (uint8_t is = 0; is <= m_setCount; is++) {
    cache = dirCache(is, FsCache::CACHE_FOR_READ);
    if (!cache) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    switch (cache[0]) {
      case EXFAT_TYPE_FILE:
        df = reinterpret_cast<DirFile_t*>(cache);
        setLe16(df->attributes, m_attributes & FS_ATTRIB_COPY);
        m_vol->dataCacheDirty();
        if (flags & T_ACCESS) {
          setLe16(df->accessTime, time);
          setLe16(df->accessDate, date);
        }
        if (flags & T_CREATE) {
          df->createTimeMs = ms10;
          setLe16(df->createTime, time);
          setLe16(df->createDate, date);
        }
        if (flags & T_WRITE) {
          df->modifyTimeMs = ms10;
          setLe16(df->modifyTime, time);
          setLe16(df->modifyDate, date);
        }
        break;

      case EXFAT_TYPE_STREAM:
        break;

      case EXFAT_TYPE_NAME:
        break;

      default:
        DBG_FAIL_MACRO;
        goto fail;
        break;
    }
    checksum = exFatDirChecksum(cache, checksum);
  }
  df = reinterpret_cast<DirFile_t*>(
      m_vol->dirCache(&m_dirPos, FsCache::CACHE_FOR_WRITE));
  if (!df) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  setLe16(df->setChecksum, checksum);
  if (!m_vol->cacheSync()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  return true;

fail:
  return false;
}
//------------------------------------------------------------------------------
bool ExFatFile::truncate() {
  uint32_t toFree;
  // error if not a normal file or read-only
  if (!isWritable()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (m_firstCluster == 0) {
    return true;
  }
  if (isContiguous()) {
    uint32_t nc = 1 + ((m_dataLength - 1) >> m_vol->bytesPerClusterShift());
    if (m_curCluster) {
      toFree = m_curCluster + 1;
      nc -= 1 + m_curCluster - m_firstCluster;
    } else {
      toFree = m_firstCluster;
      m_firstCluster = 0;
    }
    if (nc && !m_vol->bitmapModify(toFree, nc, 0)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  } else {
    // need to free chain
    if (m_curCluster) {
      toFree = 0;
      int8_t fg = m_vol->fatGet(m_curCluster, &toFree);
      if (fg < 0) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      if (fg) {
        // current cluster is end of chain
        if (!m_vol->fatPut(m_curCluster, EXFAT_EOC)) {
          DBG_FAIL_MACRO;
          goto fail;
        }
      }
    } else {
      toFree = m_firstCluster;
      m_firstCluster = 0;
    }
    if (toFree) {
      if (!m_vol->freeChain(toFree)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    }
  }
  m_dataLength = m_curPosition;
  m_validLength = m_curPosition;
  m_flags |= FILE_FLAG_DIR_DIRTY;
  return sync();

fail:
  return false;
}
//------------------------------------------------------------------------------
size_t ExFatFile::write(const void* buf, size_t nbyte) {
  // convert void* to uint8_t*  -  must be before goto statements
  const uint8_t* src = reinterpret_cast<const uint8_t*>(buf);
  uint8_t* cache;
  uint8_t cacheOption;
  uint16_t sectorOffset;
  uint32_t sector;
  uint32_t clusterOffset;

  // number of bytes left to write  -  must be before goto statements
  size_t toWrite = nbyte;
  size_t n;
  // error if not an open file or is read-only
  if (!isWritable()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // seek to end of file if append flag
  if ((m_flags & FILE_FLAG_APPEND)) {
    if (!seekSet(m_validLength)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }
  while (toWrite) {
    clusterOffset = m_curPosition & m_vol->clusterMask();
    sectorOffset = clusterOffset & m_vol->sectorMask();
    if (clusterOffset == 0) {
      // start of new cluster
      if (m_curCluster != 0) {
        int fg;

        if (isContiguous()) {
          uint32_t lc = m_firstCluster;
          lc += (m_dataLength - 1) >> m_vol->bytesPerClusterShift();
          if (m_curCluster < lc) {
            m_curCluster++;
            fg = 1;
          } else {
            fg = 0;
          }
        } else {
          fg = m_vol->fatGet(m_curCluster, &m_curCluster);
          if (fg < 0) {
            DBG_FAIL_MACRO;
            goto fail;
          }
        }
        if (fg == 0) {
          // add cluster if at end of chain
          if (!addCluster()) {
            DBG_FAIL_MACRO;
            goto fail;
          }
        }
      } else {
        if (m_firstCluster == 0) {
          // allocate first cluster of file
          if (!addCluster()) {
            DBG_FAIL_MACRO;
            goto fail;
          }
          m_firstCluster = m_curCluster;
        } else {
          m_curCluster = m_firstCluster;
        }
      }
    }
    // sector for data write
    sector = m_vol->clusterStartSector(m_curCluster) +
             (clusterOffset >> m_vol->bytesPerSectorShift());

    if (sectorOffset != 0 || toWrite < m_vol->bytesPerSector()) {
      // partial sector - must use cache
      // max space in sector
      n = m_vol->bytesPerSector() - sectorOffset;
      // lesser of space and amount to write
      if (n > toWrite) {
        n = toWrite;
      }

      if (sectorOffset == 0 && m_curPosition >= m_validLength) {
        // start of new sector don't need to read into cache
        cacheOption = FsCache::CACHE_RESERVE_FOR_WRITE;
      } else {
        // rewrite part of sector
        cacheOption = FsCache::CACHE_FOR_WRITE;
      }
      cache = m_vol->dataCachePrepare(sector, cacheOption);
      if (!cache) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      uint8_t* dst = cache + sectorOffset;
      memcpy(dst, src, n);
      if (m_vol->bytesPerSector() == (n + sectorOffset)) {
        // Force write if sector is full - improves large writes.
        if (!m_vol->dataCacheSync()) {
          DBG_FAIL_MACRO;
          goto fail;
        }
      }
#if USE_MULTI_SECTOR_IO
    } else if (toWrite >= 2 * m_vol->bytesPerSector()) {
      // use multiple sector write command
      uint32_t ns = toWrite >> m_vol->bytesPerSectorShift();
      // Limit writes to current cluster.
      uint32_t maxNs = m_vol->sectorsPerCluster() -
                       (clusterOffset >> m_vol->bytesPerSectorShift());
      if (ns > maxNs) {
        ns = maxNs;
      }
      n = ns << m_vol->bytesPerSectorShift();
      if (!m_vol->cacheSafeWrite(sector, src, ns)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
#endif  // USE_MULTI_SECTOR_IO
    } else {
      n = m_vol->bytesPerSector();
      if (!m_vol->cacheSafeWrite(sector, src)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    }
    m_curPosition += n;
    src += n;
    toWrite -= n;
    if (m_curPosition > m_validLength) {
      m_flags |= FILE_FLAG_DIR_DIRTY;
      m_validLength = m_curPosition;
    }
  }
  if (m_curPosition > m_dataLength) {
    m_dataLength = m_curPosition;
    // update fileSize and insure sync will update dir entry
    m_flags |= FILE_FLAG_DIR_DIRTY;
  } else if (FsDateTime::callback) {
    // insure sync will update modified date and time
    m_flags |= FILE_FLAG_DIR_DIRTY;
  }
  return nbyte;

fail:
  // return for write error
  m_error |= WRITE_ERROR;
  return 0;
}
#endif  // EXFAT_READ_ONLY
