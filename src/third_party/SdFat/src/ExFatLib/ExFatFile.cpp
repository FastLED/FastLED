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
#define DBG_FILE "ExFatFile.cpp"
#include "../common/DebugMacros.h"
#include "../common/FsUtf.h"
#include "ExFatLib.h"
//------------------------------------------------------------------------------
/** test for legal character.
 *
 * \param[in] c character to be tested.
 *
 * \return true for legal character else false.
 */
inline bool lfnLegalChar(uint8_t c) {
#if USE_UTF8_LONG_NAMES
  return !lfnReservedChar(c);
#else   // USE_UTF8_LONG_NAMES
  return !(lfnReservedChar(c) || c & 0X80);
#endif  // USE_UTF8_LONG_NAMES
}
//------------------------------------------------------------------------------
bool ExFatFile::attrib(uint8_t bits) {
  if (!isFileOrSubDir() || (bits & FS_ATTRIB_USER_SETTABLE) != bits) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // Don't allow read-only to be set if the file is open for write.
  if ((bits & FS_ATTRIB_READ_ONLY) && isWritable()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  m_attributes = (m_attributes & ~FS_ATTRIB_USER_SETTABLE) | bits;
  // insure sync() will update dir entry
  m_flags |= FILE_FLAG_DIR_DIRTY;
  if (!sync()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  return true;

fail:
  return false;
}
//------------------------------------------------------------------------------
uint8_t* ExFatFile::dirCache(uint8_t set, uint8_t options) {
  DirPos_t pos = m_dirPos;
  if (m_vol->dirSeek(&pos, FS_DIR_SIZE * set) != 1) {
    return nullptr;
  }
  return m_vol->dirCache(&pos, options);
}
//------------------------------------------------------------------------------
bool ExFatFile::close() {
  bool rtn = sync();
  m_attributes = FILE_ATTR_CLOSED;
  m_flags = 0;
  return rtn;
}
//------------------------------------------------------------------------------
bool ExFatFile::contiguousRange(uint32_t* bgnSector, uint32_t* endSector) {
  if (!isContiguous()) {
    return false;
  }
  if (bgnSector) {
    *bgnSector = firstSector();
  }
  if (endSector) {
    *endSector =
        firstSector() + ((m_validLength - 1) >> m_vol->bytesPerSectorShift());
  }
  return true;
}
//------------------------------------------------------------------------------
void ExFatFile::fgetpos(fspos_t* pos) const {
  pos->position = m_curPosition;
  pos->cluster = m_curCluster;
}
//------------------------------------------------------------------------------
int ExFatFile::fgets(char* str, int num, char* delim) {
  char ch;
  int n = 0;
  int r = -1;
  while ((n + 1) < num && (r = read(&ch, 1)) == 1) {
    // delete CR
    if (ch == '\r') {
      continue;
    }
    str[n++] = ch;
    if (!delim) {
      if (ch == '\n') {
        break;
      }
    } else {
      if (strchr(delim, ch)) {
        break;
      }
    }
  }
  if (r < 0) {
    // read error
    return -1;
  }
  str[n] = '\0';
  return n;
}
//------------------------------------------------------------------------------
uint32_t ExFatFile::firstSector() const {
  return m_firstCluster ? m_vol->clusterStartSector(m_firstCluster) : 0;
}
//------------------------------------------------------------------------------
void ExFatFile::fsetpos(const fspos_t* pos) {
  m_curPosition = pos->position;
  m_curCluster = pos->cluster;
}
//------------------------------------------------------------------------------
bool ExFatFile::getAccessDateTime(uint16_t* pdate, uint16_t* ptime) {
  DirFile_t* df = reinterpret_cast<DirFile_t*>(
      m_vol->dirCache(&m_dirPos, FsCache::CACHE_FOR_READ));
  if (!df) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  *pdate = getLe16(df->accessDate);
  *ptime = getLe16(df->accessTime);
  return true;

fail:
  return false;
}
//------------------------------------------------------------------------------
bool ExFatFile::getCreateDateTime(uint16_t* pdate, uint16_t* ptime) {
  DirFile_t* df = reinterpret_cast<DirFile_t*>(
      m_vol->dirCache(&m_dirPos, FsCache::CACHE_FOR_READ));
  if (!df) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  *pdate = getLe16(df->createDate);
  *ptime = getLe16(df->createTime);
  return true;

fail:
  return false;
}
//------------------------------------------------------------------------------
bool ExFatFile::getModifyDateTime(uint16_t* pdate, uint16_t* ptime) {
  DirFile_t* df = reinterpret_cast<DirFile_t*>(
      m_vol->dirCache(&m_dirPos, FsCache::CACHE_FOR_READ));
  if (!df) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  *pdate = getLe16(df->modifyDate);
  *ptime = getLe16(df->modifyTime);
  return true;

fail:
  return false;
}
//------------------------------------------------------------------------------
bool ExFatFile::isBusy() { return m_vol->isBusy(); }
//------------------------------------------------------------------------------
bool ExFatFile::open(const char* path, oflag_t oflag) {
  return open(ExFatVolume::cwv(), path, oflag);
}
//------------------------------------------------------------------------------
bool ExFatFile::open(ExFatVolume* vol, const char* path, oflag_t oflag) {
  return vol && open(vol->vwd(), path, oflag);
}
//------------------------------------------------------------------------------
bool ExFatFile::open(ExFatFile* dirFile, const char* path, oflag_t oflag) {
  ExFatFile tmpDir;
  ExName_t fname;
  // error if already open
  if (isOpen() || !dirFile->isDir()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (isDirSeparator(*path)) {
    while (isDirSeparator(*path)) {
      path++;
    }
    if (*path == 0) {
      return openRoot(dirFile->m_vol);
    }
    if (!tmpDir.openRoot(dirFile->m_vol)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    dirFile = &tmpDir;
  }
  while (1) {
    if (!parsePathName(path, &fname, &path)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (*path == 0) {
      break;
    }
    if (!openPrivate(dirFile, &fname, O_RDONLY)) {
      DBG_WARN_MACRO;
      goto fail;
    }
    // tmpDir = *this;
    tmpDir.copy(this);
    dirFile = &tmpDir;
    close();
  }
  return openPrivate(dirFile, &fname, oflag);

fail:
  return false;
}
//------------------------------------------------------------------------------
bool ExFatFile::open(uint32_t index, oflag_t oflag) {
  ExFatVolume* vol = ExFatVolume::cwv();
  return vol ? open(vol->vwd(), index, oflag) : false;
}
//------------------------------------------------------------------------------
bool ExFatFile::open(ExFatFile* dirFile, uint32_t index, oflag_t oflag) {
  if (dirFile->seekSet(FS_DIR_SIZE * index) && openNext(dirFile, oflag)) {
    if (dirIndex() == index) {
      return true;
    }
    close();
    DBG_FAIL_MACRO;
  }
  return false;
}
//------------------------------------------------------------------------------
bool ExFatFile::openCwd() {
  if (isOpen() || !ExFatVolume::cwv()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // *this = *ExFatVolume::cwv()->vwd();
  this->copy(ExFatVolume::cwv()->vwd());
  rewind();
  return true;

fail:
  return false;
}
//------------------------------------------------------------------------------
bool ExFatFile::openNext(ExFatFile* dir, oflag_t oflag) {
  if (isOpen() || !dir->isDir() || (dir->curPosition() & 0X1F)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  return openPrivate(dir, nullptr, oflag);

fail:
  return false;
}
//------------------------------------------------------------------------------
bool ExFatFile::openPrivate(ExFatFile* dir, ExName_t* fname, oflag_t oflag) {
  int n;
  uint8_t modeFlags;
  uint8_t* cache __attribute__((unused));
  DirPos_t freePos __attribute__((unused));
  DirFile_t* dirFile;
  DirStream_t* dirStream;
  DirName_t* dirName;
  uint8_t buf[FS_DIR_SIZE];
  uint8_t freeCount = 0;
  uint8_t freeNeed = 3;
  bool inSet = false;

  // error if already open, no access mode, or no directory.
  if (isOpen() || !dir->isDir()) {
    DBG_FAIL_MACRO;
    goto fail;
  }

  switch (oflag & O_ACCMODE) {
    case O_RDONLY:
      modeFlags = FILE_FLAG_READ;
      break;
    case O_WRONLY:
      modeFlags = FILE_FLAG_WRITE;
      break;
    case O_RDWR:
      modeFlags = FILE_FLAG_READ | FILE_FLAG_WRITE;
      break;
    default:
      DBG_FAIL_MACRO;
      goto fail;
  }
  modeFlags |= oflag & O_APPEND ? FILE_FLAG_APPEND : 0;

  if (fname) {
    freeNeed = 2 + (fname->nameLength + 14) / 15;
    dir->rewind();
  }

  while (1) {
    n = dir->read(buf, FS_DIR_SIZE);
    if (n == 0) {
      goto create;
    }
    if (n != FS_DIR_SIZE) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (!(buf[0] & EXFAT_TYPE_USED)) {
      // Unused entry.
      if (freeCount == 0) {
        freePos.position = dir->curPosition() - FS_DIR_SIZE;
        freePos.cluster = dir->curCluster();
      }
      if (freeCount < freeNeed) {
        freeCount++;
      }
      if (buf[0] == EXFAT_TYPE_END_DIR) {
        if (fname) {
          goto create;
        }
        // Likely openNext call.
        DBG_WARN_MACRO;
        goto fail;
      }
      inSet = false;
    } else if (!inSet) {
      if (freeCount < freeNeed) {
        freeCount = 0;
      }
      if (buf[0] != EXFAT_TYPE_FILE) {
        continue;
      }
      inSet = true;
      memset(this, 0, sizeof(ExFatFile));
      dirFile = reinterpret_cast<DirFile_t*>(buf);
      m_setCount = dirFile->setCount;
      m_attributes = getLe16(dirFile->attributes) & FS_ATTRIB_COPY;
      if (!(m_attributes & FS_ATTRIB_DIRECTORY)) {
        m_attributes |= FILE_ATTR_FILE;
      }
      m_vol = dir->volume();
      m_dirPos.cluster = dir->curCluster();
      m_dirPos.position = dir->curPosition() - FS_DIR_SIZE;
      m_dirPos.isContiguous = dir->isContiguous();
    } else if (buf[0] == EXFAT_TYPE_STREAM) {
      dirStream = reinterpret_cast<DirStream_t*>(buf);
      m_flags = modeFlags;
      if (dirStream->flags & EXFAT_FLAG_CONTIGUOUS) {
        m_flags |= FILE_FLAG_CONTIGUOUS;
      }
      m_validLength = getLe64(dirStream->validLength);
      m_firstCluster = getLe32(dirStream->firstCluster);
      m_dataLength = getLe64(dirStream->dataLength);
      if (!fname) {
        goto found;
      }
      fname->reset();
      if (fname->nameLength != dirStream->nameLength ||
          fname->nameHash != getLe16(dirStream->nameHash)) {
        inSet = false;
      }
    } else if (buf[0] == EXFAT_TYPE_NAME) {
      dirName = reinterpret_cast<DirName_t*>(buf);
      if (!cmpName(dirName, fname)) {
        inSet = false;
        continue;
      }
      if (fname->atEnd()) {
        goto found;
      }
    } else {
      inSet = false;
    }
  }

found:
  // Don't open if create only.
  if (oflag & O_EXCL) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // Write, truncate, or at end is an error for a directory or read-only file.
  if ((oflag & (O_TRUNC | O_AT_END)) || (m_flags & FILE_FLAG_WRITE)) {
    if (isSubDir() || isReadOnly() || EXFAT_READ_ONLY) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }

#if !EXFAT_READ_ONLY
  if (oflag & O_TRUNC) {
    if (!(m_flags & FILE_FLAG_WRITE)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (!truncate(0)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  } else if ((oflag & O_AT_END) && !seekSet(fileSize())) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (isWritable()) {
    m_attributes |= FS_ATTRIB_ARCHIVE;
  }
#endif  // !EXFAT_READ_ONLY
  return true;

create:
#if EXFAT_READ_ONLY
  DBG_FAIL_MACRO;
  goto fail;
#else   // EXFAT_READ_ONLY
  // don't create unless O_CREAT and write
  if (!(oflag & O_CREAT) || !(modeFlags & FILE_FLAG_WRITE) || !fname) {
    DBG_WARN_MACRO;
    goto fail;
  }
  while (freeCount < freeNeed) {
    n = dir->read(buf, FS_DIR_SIZE);
    if (n == 0) {
      uint32_t saveCurCluster = dir->m_curCluster;
      if (!dir->addDirCluster()) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      dir->m_curCluster = saveCurCluster;
      continue;
    }
    if (n != FS_DIR_SIZE) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (freeCount == 0) {
      freePos.position = dir->curPosition() - FS_DIR_SIZE;
      freePos.cluster = dir->curCluster();
    }
    freeCount++;
  }
  freePos.isContiguous = dir->isContiguous();
  memset(this, 0, sizeof(ExFatFile));
  m_vol = dir->volume();
  m_attributes = FILE_ATTR_FILE | FS_ATTRIB_ARCHIVE;
  m_dirPos = freePos;
  fname->reset();
  for (uint8_t i = 0; i < freeNeed; i++) {
    cache = dirCache(i, FsCache::CACHE_FOR_WRITE);
    if (!cache || (cache[0] & 0x80)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    memset(cache, 0, FS_DIR_SIZE);
    if (i == 0) {
      dirFile = reinterpret_cast<DirFile_t*>(cache);
      dirFile->type = EXFAT_TYPE_FILE;
      m_setCount = freeNeed - 1;
      dirFile->setCount = m_setCount;

      if (FsDateTime::callback) {
        uint16_t date, time;
        uint8_t ms10;
        FsDateTime::callback(&date, &time, &ms10);
        setLe16(dirFile->createDate, date);
        setLe16(dirFile->createTime, time);
        dirFile->createTimeMs = ms10;
      } else {
        setLe16(dirFile->createDate, FS_DEFAULT_DATE);
        setLe16(dirFile->modifyDate, FS_DEFAULT_DATE);
        setLe16(dirFile->accessDate, FS_DEFAULT_DATE);
        if (FS_DEFAULT_TIME) {
          setLe16(dirFile->createTime, FS_DEFAULT_TIME);
          setLe16(dirFile->modifyTime, FS_DEFAULT_TIME);
          setLe16(dirFile->accessTime, FS_DEFAULT_TIME);
        }
      }
    } else if (i == 1) {
      dirStream = reinterpret_cast<DirStream_t*>(cache);
      dirStream->type = EXFAT_TYPE_STREAM;
      dirStream->flags = EXFAT_FLAG_ALWAYS1;
      m_flags = modeFlags | FILE_FLAG_DIR_DIRTY;
      dirStream->nameLength = fname->nameLength;
      setLe16(dirStream->nameHash, fname->nameHash);
    } else {
      dirName = reinterpret_cast<DirName_t*>(cache);
      dirName->type = EXFAT_TYPE_NAME;
      for (size_t k = 0; k < 15; k++) {
        if (fname->atEnd()) {
          break;
        }
        uint16_t u = fname->get16();
        setLe16(dirName->unicode + 2 * k, u);
      }
    }
  }
  return sync();
#endif  // EXFAT_READ_ONLY

fail:
  // close file
  m_attributes = FILE_ATTR_CLOSED;
  m_flags = 0;
  return false;
}
//------------------------------------------------------------------------------
bool ExFatFile::openRoot(ExFatVolume* vol) {
  if (isOpen()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  memset(this, 0, sizeof(ExFatFile));
  m_attributes = FILE_ATTR_ROOT;
  m_vol = vol;
  m_flags = FILE_FLAG_READ;
  return true;

fail:
  return false;
}
//------------------------------------------------------------------------------
bool ExFatFile::parsePathName(const char* path, ExName_t* fname,
                              const char** ptr) {
  // Skip leading spaces.
  while (*path == ' ') {
    path++;
  }
  fname->begin = path;
  fname->end = path;
  while (*path && !isDirSeparator(*path)) {
    uint8_t c = *path++;
    if (!lfnLegalChar(c)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (c != '.' && c != ' ') {
      // Need to trim trailing dots spaces.
      fname->end = path;
    }
  }
  // Advance to next path component.
  for (; *path == ' ' || isDirSeparator(*path); path++) {
  }
  *ptr = path;
  return hashName(fname);

fail:
  return false;
}
//------------------------------------------------------------------------------
int ExFatFile::peek() {
  uint64_t saveCurPosition = m_curPosition;
  uint32_t saveCurCluster = m_curCluster;
  int c = read();
  m_curPosition = saveCurPosition;
  m_curCluster = saveCurCluster;
  return c;
}
//------------------------------------------------------------------------------
int ExFatFile::read(void* buf, size_t count) {
  uint8_t* dst = reinterpret_cast<uint8_t*>(buf);
  int8_t fg;
  size_t toRead = count;
  size_t n;
  uint8_t* cache;
  uint16_t sectorOffset;
  uint32_t sector;
  uint32_t clusterOffset;

  if (!isReadable()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (isContiguous() || isFile()) {
    if ((m_curPosition + count) > m_validLength) {
      count = toRead = m_validLength - m_curPosition;
    }
  }
  while (toRead) {
    clusterOffset = m_curPosition & m_vol->clusterMask();
    sectorOffset = clusterOffset & m_vol->sectorMask();
    if (clusterOffset == 0) {
      if (m_curPosition == 0) {
        m_curCluster =
            isRoot() ? m_vol->rootDirectoryCluster() : m_firstCluster;
      } else if (isContiguous()) {
        m_curCluster++;
      } else {
        fg = m_vol->fatGet(m_curCluster, &m_curCluster);
        if (fg < 0) {
          DBG_FAIL_MACRO;
          goto fail;
        }
        if (fg == 0) {
          // EOF if directory.
          if (isDir()) {
            break;
          }
          DBG_FAIL_MACRO;
          goto fail;
        }
      }
    }
    sector = m_vol->clusterStartSector(m_curCluster) +
             (clusterOffset >> m_vol->bytesPerSectorShift());
    if (sectorOffset != 0 || toRead < m_vol->bytesPerSector() ||
        sector == m_vol->dataCacheSector()) {
      n = m_vol->bytesPerSector() - sectorOffset;
      if (n > toRead) {
        n = toRead;
      }
      // read sector to cache and copy data to caller
      cache = m_vol->dataCachePrepare(sector, FsCache::CACHE_FOR_READ);
      if (!cache) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      uint8_t* src = cache + sectorOffset;
      memcpy(dst, src, n);
#if USE_MULTI_SECTOR_IO
    } else if (toRead >= 2 * m_vol->bytesPerSector()) {
      uint32_t ns = toRead >> m_vol->bytesPerSectorShift();
      // Limit reads to current cluster.
      uint32_t maxNs = m_vol->sectorsPerCluster() -
                       (clusterOffset >> m_vol->bytesPerSectorShift());
      if (ns > maxNs) {
        ns = maxNs;
      }
      n = ns << m_vol->bytesPerSectorShift();
      if (!m_vol->cacheSafeRead(sector, dst, ns)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
#endif  // USE_MULTI_SECTOR_IO
    } else {
      // read single sector
      n = m_vol->bytesPerSector();
      if (!m_vol->cacheSafeRead(sector, dst)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    }
    dst += n;
    m_curPosition += n;
    toRead -= n;
  }
  return count - toRead;

fail:
  m_error |= READ_ERROR;
  return -1;
}
//------------------------------------------------------------------------------
bool ExFatFile::remove(const char* path) {
  ExFatFile file;
  if (!file.open(this, path, O_WRONLY)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  return file.remove();

fail:
  return false;
}
//------------------------------------------------------------------------------
bool ExFatFile::seekSet(uint64_t pos) {
  uint32_t nCur;
  uint32_t nNew;
  uint32_t tmp = m_curCluster;
  // error if file not open
  if (!isOpen()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // Optimize O_APPEND writes.
  if (pos == m_curPosition) {
    return true;
  }
  if (pos == 0) {
    // set position to start of file
    m_curCluster = 0;
    goto done;
  }
  if (isFile()) {
    if (pos > m_validLength) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }
  // calculate cluster index for new position
  nNew = (pos - 1) >> m_vol->bytesPerClusterShift();
  if (isContiguous()) {
    m_curCluster = m_firstCluster + nNew;
    goto done;
  }
  // calculate cluster index for current position
  nCur = (m_curPosition - 1) >> m_vol->bytesPerClusterShift();
  if (nNew < nCur || m_curPosition == 0) {
    // must follow chain from first cluster
    m_curCluster = isRoot() ? m_vol->rootDirectoryCluster() : m_firstCluster;
  } else {
    // advance from curPosition
    nNew -= nCur;
  }
  while (nNew--) {
    if (m_vol->fatGet(m_curCluster, &m_curCluster) <= 0) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }

done:
  m_curPosition = pos;
  return true;

fail:
  m_curCluster = tmp;
  return false;
}
