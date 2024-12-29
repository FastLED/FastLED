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
#define DBG_FILE "FatFile.cpp"
#include "../common/DebugMacros.h"
#include "FatLib.h"
//------------------------------------------------------------------------------
// Add a cluster to a file.
bool FatFile::addCluster() {
#if USE_FAT_FILE_FLAG_CONTIGUOUS
  uint32_t cc = m_curCluster;
  if (!m_vol->allocateCluster(m_curCluster, &m_curCluster)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (cc == 0) {
    m_flags |= FILE_FLAG_CONTIGUOUS;
  } else if (m_curCluster != (cc + 1)) {
    m_flags &= ~FILE_FLAG_CONTIGUOUS;
  }
  m_flags |= FILE_FLAG_DIR_DIRTY;
  return true;

fail:
  return false;
#else   // USE_FAT_FILE_FLAG_CONTIGUOUS
  m_flags |= FILE_FLAG_DIR_DIRTY;
  return m_vol->allocateCluster(m_curCluster, &m_curCluster);
#endif  // USE_FAT_FILE_FLAG_CONTIGUOUS
}
//------------------------------------------------------------------------------
// Add a cluster to a directory file and zero the cluster.
// Return with first sector of cluster in the cache.
bool FatFile::addDirCluster() {
  uint32_t sector;
  uint8_t* pc;

  if (isRootFixed()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // max folder size
  if (m_curPosition >= 512UL * 4095) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (!addCluster()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  sector = m_vol->clusterStartSector(m_curCluster);
  for (uint8_t i = 0; i < m_vol->sectorsPerCluster(); i++) {
    pc = m_vol->dataCachePrepare(sector + i, FsCache::CACHE_RESERVE_FOR_WRITE);
    if (!pc) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    memset(pc, 0, m_vol->bytesPerSector());
  }
  // Set position to EOF to avoid inconsistent curCluster/curPosition.
  m_curPosition += m_vol->bytesPerCluster();
  return true;

fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::attrib(uint8_t bits) {
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
// cache a file's directory entry
// return pointer to cached entry or null for failure
DirFat_t* FatFile::cacheDirEntry(uint8_t action) {
  uint8_t* pc = m_vol->dataCachePrepare(m_dirSector, action);
  DirFat_t* dir = reinterpret_cast<DirFat_t*>(pc);
  if (!dir) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  return dir + (m_dirIndex & 0XF);

fail:
  return nullptr;
}
//------------------------------------------------------------------------------
bool FatFile::close() {
  bool rtn = sync();
  m_attributes = FILE_ATTR_CLOSED;
  m_flags = 0;
  return rtn;
}
//------------------------------------------------------------------------------
bool FatFile::contiguousRange(uint32_t* bgnSector, uint32_t* endSector) {
  // error if no clusters
  if (!isFile() || m_firstCluster == 0) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  for (uint32_t c = m_firstCluster;; c++) {
    uint32_t next;
    int8_t fg = m_vol->fatGet(c, &next);
    if (fg < 0) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    // check for contiguous
    if (fg == 0 || next != (c + 1)) {
      // error if not end of chain
      if (fg) {
        DBG_FAIL_MACRO;
        goto fail;
      }
#if USE_FAT_FILE_FLAG_CONTIGUOUS
      m_flags |= FILE_FLAG_CONTIGUOUS;
#endif  // USE_FAT_FILE_FLAG_CONTIGUOUS
      if (bgnSector) {
        *bgnSector = m_vol->clusterStartSector(m_firstCluster);
      }
      if (endSector) {
        *endSector =
            m_vol->clusterStartSector(c) + m_vol->sectorsPerCluster() - 1;
      }
      return true;
    }
  }

fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::createContiguous(const char* path, uint32_t size) {
  if (!open(FatVolume::cwv(), path, O_CREAT | O_EXCL | O_RDWR)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (preAllocate(size)) {
    return true;
  }
  close();
fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::createContiguous(FatFile* dirFile, const char* path,
                               uint32_t size) {
  if (!open(dirFile, path, O_CREAT | O_EXCL | O_RDWR)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (preAllocate(size)) {
    return true;
  }
  close();
fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::dirEntry(DirFat_t* dst) {
  DirFat_t* dir;
  // Make sure fields on device are correct.
  if (!sync()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // read entry
  dir = cacheDirEntry(FsCache::CACHE_FOR_READ);
  if (!dir) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // copy to caller's struct
  memcpy(dst, dir, sizeof(DirFat_t));
  return true;

fail:
  return false;
}
//------------------------------------------------------------------------------
uint32_t FatFile::dirSize() {
  int8_t fg;
  if (!isDir()) {
    return 0;
  }
  if (isRootFixed()) {
    return FS_DIR_SIZE * m_vol->rootDirEntryCount();
  }
  uint16_t n = 0;
  uint32_t c = isRoot32() ? m_vol->rootDirStart() : m_firstCluster;
  do {
    fg = m_vol->fatGet(c, &c);
    if (fg < 0 || n > 4095) {
      return 0;
    }
    n += m_vol->sectorsPerCluster();
  } while (fg);
  return 512UL * n;
}
//------------------------------------------------------------------------------
int FatFile::fgets(char* str, int num, char* delim) {
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
void FatFile::fgetpos(fspos_t* pos) const {
  pos->position = m_curPosition;
  pos->cluster = m_curCluster;
}
//------------------------------------------------------------------------------
uint32_t FatFile::firstSector() const {
  return m_firstCluster ? m_vol->clusterStartSector(m_firstCluster) : 0;
}
//------------------------------------------------------------------------------
void FatFile::fsetpos(const fspos_t* pos) {
  m_curPosition = pos->position;
  m_curCluster = pos->cluster;
}
//------------------------------------------------------------------------------
bool FatFile::getAccessDate(uint16_t* pdate) {
  DirFat_t dir;
  if (!dirEntry(&dir)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  *pdate = getLe16(dir.accessDate);
  return true;

fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::getCreateDateTime(uint16_t* pdate, uint16_t* ptime) {
  DirFat_t dir;
  if (!dirEntry(&dir)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  *pdate = getLe16(dir.createDate);
  *ptime = getLe16(dir.createTime);
  return true;

fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::getModifyDateTime(uint16_t* pdate, uint16_t* ptime) {
  DirFat_t dir;
  if (!dirEntry(&dir)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  *pdate = getLe16(dir.modifyDate);
  *ptime = getLe16(dir.modifyTime);
  return true;

fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::isBusy() { return m_vol->isBusy(); }
//------------------------------------------------------------------------------
bool FatFile::mkdir(FatFile* parent, const char* path, bool pFlag) {
  FatName_t fname;
  FatFile tmpDir;

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
    if (!open(parent, &fname, O_RDONLY)) {
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
bool FatFile::mkdir(FatFile* parent, FatName_t* fname) {
  uint32_t sector;
  DirFat_t dot;
  DirFat_t* dir;
  uint8_t* pc;

  if (!parent->isDir()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // create a normal file
  if (!open(parent, fname, O_CREAT | O_EXCL | O_RDWR)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // convert file to directory
  m_flags = FILE_FLAG_READ;
  m_attributes = FILE_ATTR_SUBDIR;

  // allocate and zero first cluster
  if (!addDirCluster()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  m_firstCluster = m_curCluster;
  // Set to start of dir
  rewind();
  // force entry to device
  if (!sync()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // cache entry - should already be in cache due to sync() call
  dir = cacheDirEntry(FsCache::CACHE_FOR_WRITE);
  if (!dir) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // change directory entry attribute
  dir->attributes = FS_ATTRIB_DIRECTORY;

  // make entry for '.'
  memcpy(&dot, dir, sizeof(dot));
  dot.name[0] = '.';
  for (uint8_t i = 1; i < 11; i++) {
    dot.name[i] = ' ';
  }

  // cache sector for '.'  and '..'
  sector = m_vol->clusterStartSector(m_firstCluster);
  pc = m_vol->dataCachePrepare(sector, FsCache::CACHE_FOR_WRITE);
  dir = reinterpret_cast<DirFat_t*>(pc);
  if (!dir) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // copy '.' to sector
  memcpy(&dir[0], &dot, sizeof(dot));
  // make entry for '..'
  dot.name[1] = '.';
  setLe16(dot.firstClusterLow, parent->m_firstCluster & 0XFFFF);
  setLe16(dot.firstClusterHigh, parent->m_firstCluster >> 16);
  // copy '..' to sector
  memcpy(&dir[1], &dot, sizeof(dot));
  // write first sector
  return m_vol->cacheSync();

fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::open(const char* path, oflag_t oflag) {
  return open(FatVolume::cwv(), path, oflag);
}
//------------------------------------------------------------------------------
bool FatFile::open(FatVolume* vol, const char* path, oflag_t oflag) {
  return vol && open(vol->vwd(), path, oflag);
}
//------------------------------------------------------------------------------
bool FatFile::open(FatFile* dirFile, const char* path, oflag_t oflag) {
  FatFile tmpDir;
  FatName_t fname;

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
    if (!open(dirFile, &fname, O_RDONLY)) {
      DBG_WARN_MACRO;
      goto fail;
    }
    // tmpDir = *this;
    tmpDir.copy(this);
    dirFile = &tmpDir;
    close();
  }
  return open(dirFile, &fname, oflag);

fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::open(uint16_t index, oflag_t oflag) {
  FatVolume* vol = FatVolume::cwv();
  return vol ? open(vol->vwd(), index, oflag) : false;
}
//------------------------------------------------------------------------------
bool FatFile::open(FatFile* dirFile, uint16_t index, oflag_t oflag) {
  if (index) {
    // Find start of LFN.
    DirLfn_t* ldir;
    uint8_t n = index < 20 ? index : 20;
    for (uint8_t i = 1; i <= n; i++) {
      ldir = reinterpret_cast<DirLfn_t*>(dirFile->cacheDir(index - i));
      if (!ldir) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      if (ldir->attributes != FAT_ATTRIB_LONG_NAME) {
        break;
      }
      if (ldir->order & FAT_ORDER_LAST_LONG_ENTRY) {
        if (!dirFile->seekSet(32UL * (index - i))) {
          DBG_FAIL_MACRO;
          goto fail;
        }
        break;
      }
    }
  } else {
    dirFile->rewind();
  }
  if (!openNext(dirFile, oflag)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (dirIndex() != index) {
    close();
    DBG_FAIL_MACRO;
    goto fail;
  }
  return true;

fail:
  return false;
}
//------------------------------------------------------------------------------
// open a cached directory entry.
bool FatFile::openCachedEntry(FatFile* dirFile, uint16_t dirIndex,
                              oflag_t oflag, uint8_t lfnOrd) {
  uint32_t firstCluster;
  memset(this, 0, sizeof(FatFile));
  // location of entry in cache
  m_vol = dirFile->m_vol;
  m_dirIndex = dirIndex;
  m_dirCluster = dirFile->m_firstCluster;
  DirFat_t* dir = reinterpret_cast<DirFat_t*>(m_vol->cacheAddress());
  dir += 0XF & dirIndex;

  // Must be file or subdirectory.
  if (!isFatFileOrSubdir(dir)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  m_attributes = dir->attributes & FS_ATTRIB_COPY;
  if (isFatFile(dir)) {
    m_attributes |= FILE_ATTR_FILE;
  }
  m_lfnOrd = lfnOrd;

  switch (oflag & O_ACCMODE) {
    case O_RDONLY:
      if (oflag & O_TRUNC) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      m_flags = FILE_FLAG_READ;
      break;

    case O_RDWR:
      m_flags = FILE_FLAG_READ | FILE_FLAG_WRITE;
      break;

    case O_WRONLY:
      m_flags = FILE_FLAG_WRITE;
      break;

    default:
      DBG_FAIL_MACRO;
      goto fail;
  }

  if (m_flags & FILE_FLAG_WRITE) {
    if (isSubDir() || isReadOnly()) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    m_attributes |= FS_ATTRIB_ARCHIVE;
  }
  m_flags |= (oflag & O_APPEND ? FILE_FLAG_APPEND : 0);

  m_dirSector = m_vol->cacheSectorNumber();

  // copy first cluster number for directory fields
  firstCluster = ((uint32_t)getLe16(dir->firstClusterHigh) << 16) |
                 getLe16(dir->firstClusterLow);

  if (oflag & O_TRUNC) {
    if (firstCluster && !m_vol->freeChain(firstCluster)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    // need to update directory entry
    m_flags |= FILE_FLAG_DIR_DIRTY;
  } else {
    m_firstCluster = firstCluster;
    m_fileSize = getLe32(dir->fileSize);
  }
  if ((oflag & O_AT_END) && !seekSet(m_fileSize)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  return true;

fail:
  m_attributes = FILE_ATTR_CLOSED;
  m_flags = 0;
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::openCluster(FatFile* file) {
  if (file->m_dirCluster == 0) {
    return openRoot(file->m_vol);
  }
  memset(this, 0, sizeof(FatFile));
  m_attributes = FILE_ATTR_SUBDIR;
  m_flags = FILE_FLAG_READ;
  m_vol = file->m_vol;
  m_firstCluster = file->m_dirCluster;
  return true;
}
//------------------------------------------------------------------------------
bool FatFile::openCwd() {
  if (isOpen() || !FatVolume::cwv()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // *this = *FatVolume::cwv()->vwd();
  this->copy(FatVolume::cwv()->vwd());
  rewind();
  return true;

fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::openNext(FatFile* dirFile, oflag_t oflag) {
  uint8_t checksum = 0;
  DirLfn_t* ldir;
  uint8_t lfnOrd = 0;
  uint16_t index;

  // Check for not open and valid directory..
  if (isOpen() || !dirFile->isDir() || (dirFile->curPosition() & 0X1F)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  while (1) {
    // read entry into cache
    index = dirFile->curPosition() / FS_DIR_SIZE;
    DirFat_t* dir = dirFile->readDirCache();
    if (!dir) {
      if (dirFile->getError()) {
        DBG_FAIL_MACRO;
      }
      goto fail;
    }
    // done if last entry
    if (dir->name[0] == FAT_NAME_FREE) {
      goto fail;
    }
    // skip empty slot or '.' or '..'
    if (dir->name[0] == '.' || dir->name[0] == FAT_NAME_DELETED) {
      lfnOrd = 0;
    } else if (isFatFileOrSubdir(dir)) {
      if (lfnOrd && checksum != lfnChecksum(dir->name)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      if (!openCachedEntry(dirFile, index, oflag, lfnOrd)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      return true;
    } else if (isFatLongName(dir)) {
      ldir = reinterpret_cast<DirLfn_t*>(dir);
      if (ldir->order & FAT_ORDER_LAST_LONG_ENTRY) {
        lfnOrd = ldir->order & 0X1F;
        checksum = ldir->checksum;
      }
    } else {
      lfnOrd = 0;
    }
  }

fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::openRoot(FatVolume* vol) {
  // error if file is already open
  if (isOpen()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  memset(this, 0, sizeof(FatFile));

  m_vol = vol;
  switch (vol->fatType()) {
#if FAT12_SUPPORT
    case 12:
#endif  // FAT12_SUPPORT
    case 16:
      m_attributes = FILE_ATTR_ROOT_FIXED;
      break;

    case 32:
      m_attributes = FILE_ATTR_ROOT32;
      break;

    default:
      DBG_FAIL_MACRO;
      goto fail;
  }
  // read only
  m_flags = FILE_FLAG_READ;
  return true;

fail:
  return false;
}
//------------------------------------------------------------------------------
int FatFile::peek() {
  uint32_t saveCurPosition = m_curPosition;
  uint32_t saveCurCluster = m_curCluster;
  int c = read();
  m_curPosition = saveCurPosition;
  m_curCluster = saveCurCluster;
  return c;
}
//------------------------------------------------------------------------------
bool FatFile::preAllocate(uint32_t length) {
  uint32_t need;
  if (!length || !isWritable() || m_firstCluster) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  need = 1 + ((length - 1) >> m_vol->bytesPerClusterShift());
  // allocate clusters
  if (!m_vol->allocContiguous(need, &m_firstCluster)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  m_fileSize = length;

#if USE_FAT_FILE_FLAG_CONTIGUOUS
  // Mark contiguous and insure sync() will update dir entry
  m_flags |= FILE_FLAG_PREALLOCATE | FILE_FLAG_CONTIGUOUS | FILE_FLAG_DIR_DIRTY;
#else   // USE_FAT_FILE_FLAG_CONTIGUOUS
  // insure sync() will update dir entry
  m_flags |= FILE_FLAG_DIR_DIRTY;
#endif  // USE_FAT_FILE_FLAG_CONTIGUOUS
  return sync();

fail:
  return false;
}
//------------------------------------------------------------------------------
int FatFile::read(void* buf, size_t nbyte) {
  int8_t fg;
  uint8_t sectorOfCluster = 0;
  uint8_t* dst = reinterpret_cast<uint8_t*>(buf);
  uint16_t offset;
  size_t toRead;
  uint32_t sector;  // raw device sector number
  uint8_t* pc;
  // error if not open for read
  if (!isReadable()) {
    DBG_FAIL_MACRO;
    goto fail;
  }

  if (isFile()) {
    uint32_t tmp32 = m_fileSize - m_curPosition;
    if (nbyte >= tmp32) {
      nbyte = tmp32;
    }
  } else if (isRootFixed()) {
    uint16_t tmp16 =
        FS_DIR_SIZE * m_vol->m_rootDirEntryCount - (uint16_t)m_curPosition;
    if (nbyte > tmp16) {
      nbyte = tmp16;
    }
  }
  toRead = nbyte;
  while (toRead) {
    size_t n;
    offset = m_curPosition & m_vol->sectorMask();  // offset in sector
    if (isRootFixed()) {
      sector = m_vol->rootDirStart() +
               (m_curPosition >> m_vol->bytesPerSectorShift());
    } else {
      sectorOfCluster = m_vol->sectorOfCluster(m_curPosition);
      if (offset == 0 && sectorOfCluster == 0) {
        // start of new cluster
        if (m_curPosition == 0) {
          // use first cluster in file
          m_curCluster = isRoot32() ? m_vol->rootDirStart() : m_firstCluster;
#if USE_FAT_FILE_FLAG_CONTIGUOUS
        } else if (isFile() && isContiguous()) {
          m_curCluster++;
#endif  // USE_FAT_FILE_FLAG_CONTIGUOUS
        } else {
          // get next cluster from FAT
          fg = m_vol->fatGet(m_curCluster, &m_curCluster);
          if (fg < 0) {
            DBG_FAIL_MACRO;
            goto fail;
          }
          if (fg == 0) {
            if (isDir()) {
              break;
            }
            DBG_FAIL_MACRO;
            goto fail;
          }
        }
      }
      sector = m_vol->clusterStartSector(m_curCluster) + sectorOfCluster;
    }
    if (offset != 0 || toRead < m_vol->bytesPerSector() ||
        sector == m_vol->cacheSectorNumber()) {
      // amount to be read from current sector
      n = m_vol->bytesPerSector() - offset;
      if (n > toRead) {
        n = toRead;
      }
      // read sector to cache and copy data to caller
      pc = m_vol->dataCachePrepare(sector, FsCache::CACHE_FOR_READ);
      if (!pc) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      uint8_t* src = pc + offset;
      memcpy(dst, src, n);
#if USE_MULTI_SECTOR_IO
    } else if (toRead >= 2 * m_vol->bytesPerSector()) {
      uint32_t ns = toRead >> m_vol->bytesPerSectorShift();
      if (!isRootFixed()) {
        uint32_t mb = m_vol->sectorsPerCluster() - sectorOfCluster;
        if (mb < ns) {
          ns = mb;
        }
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
  return nbyte - toRead;

fail:
  m_error |= READ_ERROR;
  return -1;
}
//------------------------------------------------------------------------------
int8_t FatFile::readDir(DirFat_t* dir) {
  // if not a directory file or miss-positioned return an error
  if (!isDir() || (0X1F & m_curPosition)) {
    return -1;
  }

  while (1) {
    int16_t n = read(dir, sizeof(DirFat_t));
    if (n != sizeof(DirFat_t)) {
      return n == 0 ? 0 : -1;
    }
    // last entry if FAT_NAME_FREE
    if (dir->name[0] == FAT_NAME_FREE) {
      return 0;
    }
    // skip empty entries and entry for .  and ..
    if (dir->name[0] == FAT_NAME_DELETED || dir->name[0] == '.') {
      continue;
    }
    // return if normal file or subdirectory
    if (isFatFileOrSubdir(dir)) {
      return n;
    }
  }
}
//------------------------------------------------------------------------------
// Read next directory entry into the cache.
// Assumes file is correctly positioned.
DirFat_t* FatFile::readDirCache(bool skipReadOk) {
  DBG_HALT_IF(m_curPosition & 0X1F);
  uint8_t i = (m_curPosition >> 5) & 0XF;

  if (i == 0 || !skipReadOk) {
    int8_t n = read(&n, 1);
    if (n != 1) {
      if (n != 0) {
        DBG_FAIL_MACRO;
      }
      goto fail;
    }
    m_curPosition += FS_DIR_SIZE - 1;
  } else {
    m_curPosition += FS_DIR_SIZE;
  }
  // return pointer to entry
  return reinterpret_cast<DirFat_t*>(m_vol->cacheAddress()) + i;

fail:
  return nullptr;
}
//------------------------------------------------------------------------------
bool FatFile::remove(const char* path) {
  FatFile file;
  if (!file.open(this, path, O_WRONLY)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  return file.remove();

fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::rename(const char* newPath) {
  return rename(m_vol->vwd(), newPath);
}
//------------------------------------------------------------------------------
bool FatFile::rename(FatFile* dirFile, const char* newPath) {
  DirFat_t entry;
  uint32_t dirCluster = 0;
  FatFile file;
  FatFile oldFile;
  uint8_t* pc;
  DirFat_t* dir;

  // Must be an open file or subdirectory.
  if (!(isFile() || isSubDir())) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // Can't rename LFN in 8.3 mode.
  if (!USE_LONG_FILE_NAMES && isLFN()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // Can't move file to new volume.
  if (m_vol != dirFile->m_vol) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // sync() and cache directory entry
  sync();
  // oldFile = *this;
  oldFile.copy(this);
  dir = cacheDirEntry(FsCache::CACHE_FOR_READ);
  if (!dir) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // save directory entry
  memcpy(&entry, dir, sizeof(entry));
  // make directory entry for new path
  if (isFile()) {
    if (!file.open(dirFile, newPath, O_CREAT | O_EXCL | O_WRONLY)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  } else {
    // don't create missing path prefix components
    if (!file.mkdir(dirFile, newPath, false)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    // save cluster containing new dot dot
    dirCluster = file.m_firstCluster;
  }
  // change to new directory entry

  m_dirSector = file.m_dirSector;
  m_dirIndex = file.m_dirIndex;
  m_lfnOrd = file.m_lfnOrd;
  m_dirCluster = file.m_dirCluster;
  // mark closed to avoid possible destructor close call
  file.m_attributes = FILE_ATTR_CLOSED;
  file.m_flags = 0;

  // cache new directory entry
  dir = cacheDirEntry(FsCache::CACHE_FOR_WRITE);
  if (!dir) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // copy all but name and name flags to new directory entry
  memcpy(&dir->createTimeMs, &entry.createTimeMs,
         sizeof(entry) - sizeof(dir->name) - 2);
  dir->attributes = entry.attributes;

  // update dot dot if directory
  if (dirCluster) {
    // get new dot dot
    uint32_t sector = m_vol->clusterStartSector(dirCluster);
    pc = m_vol->dataCachePrepare(sector, FsCache::CACHE_FOR_READ);
    dir = reinterpret_cast<DirFat_t*>(pc);
    if (!dir) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    memcpy(&entry, &dir[1], sizeof(entry));

    // free unused cluster
    if (!m_vol->freeChain(dirCluster)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    // store new dot dot
    sector = m_vol->clusterStartSector(m_firstCluster);
    pc = m_vol->dataCachePrepare(sector, FsCache::CACHE_FOR_WRITE);
    dir = reinterpret_cast<DirFat_t*>(pc);
    if (!dir) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    memcpy(&dir[1], &entry, sizeof(entry));
  }
  // Remove old directory entry;
  oldFile.m_firstCluster = 0;
  oldFile.m_flags = FILE_FLAG_WRITE;
  oldFile.m_attributes = FILE_ATTR_FILE;
  if (!oldFile.remove()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  return m_vol->cacheSync();

fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::rmdir() {
  // must be open subdirectory
  if (!isSubDir() || (!USE_LONG_FILE_NAMES && isLFN())) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  rewind();

  // make sure directory is empty
  while (1) {
    DirFat_t* dir = readDirCache(true);
    if (!dir) {
      // EOF if no error.
      if (!getError()) {
        break;
      }
      DBG_FAIL_MACRO;
      goto fail;
    }
    // done if past last used entry
    if (dir->name[0] == FAT_NAME_FREE) {
      break;
    }
    // skip empty slot, '.' or '..'
    if (dir->name[0] == FAT_NAME_DELETED || dir->name[0] == '.') {
      continue;
    }
    // error not empty
    if (isFatFileOrSubdir(dir)) {
      DBG_FAIL_MACRO;
      goto fail;
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
bool FatFile::rmRfStar() {
  uint16_t index;
  FatFile f;
  if (!isDir()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  rewind();
  while (1) {
    // remember position
    index = m_curPosition / FS_DIR_SIZE;

    DirFat_t* dir = readDirCache();
    if (!dir) {
      // At EOF if no error.
      if (!getError()) {
        break;
      }
      DBG_FAIL_MACRO;
      goto fail;
    }
    // done if past last entry
    if (dir->name[0] == FAT_NAME_FREE) {
      break;
    }

    // skip empty slot or '.' or '..'
    if (dir->name[0] == FAT_NAME_DELETED || dir->name[0] == '.') {
      continue;
    }

    // skip if part of long file name or volume label in root
    if (!isFatFileOrSubdir(dir)) {
      continue;
    }

    if (!f.open(this, index, O_RDONLY)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (f.isSubDir()) {
      // recursively delete
      if (!f.rmRfStar()) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    } else {
      // ignore read-only
      f.m_flags |= FILE_FLAG_WRITE;
      if (!f.remove()) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    }
    // position to next entry if required
    if (m_curPosition != (32UL * (index + 1))) {
      if (!seekSet(32UL * (index + 1))) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    }
  }
  // don't try to delete root
  if (!isRoot()) {
    if (!rmdir()) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }
  return true;

fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::seekSet(uint32_t pos) {
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
    if (pos > m_fileSize) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  } else if (isRootFixed()) {
    if (pos <= FS_DIR_SIZE * m_vol->rootDirEntryCount()) {
      goto done;
    }
    DBG_FAIL_MACRO;
    goto fail;
  }
  // calculate cluster index for new position
  nNew = (pos - 1) >> (m_vol->bytesPerClusterShift());
#if USE_FAT_FILE_FLAG_CONTIGUOUS
  if (isContiguous()) {
    m_curCluster = m_firstCluster + nNew;
    goto done;
  }
#endif  // USE_FAT_FILE_FLAG_CONTIGUOUS
  // calculate cluster index for current position
  nCur = (m_curPosition - 1) >> (m_vol->bytesPerClusterShift());

  if (nNew < nCur || m_curPosition == 0) {
    // must follow chain from first cluster
    m_curCluster = isRoot32() ? m_vol->rootDirStart() : m_firstCluster;
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
  m_flags &= ~FILE_FLAG_PREALLOCATE;
  return true;

fail:
  m_curCluster = tmp;
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::sync() {
  uint16_t date, time;
  uint8_t ms10;
  if (!isOpen()) {
    return true;
  }
  if (m_flags & FILE_FLAG_DIR_DIRTY) {
    DirFat_t* dir = cacheDirEntry(FsCache::CACHE_FOR_WRITE);
    // check for deleted by another open file object
    if (!dir || dir->name[0] == FAT_NAME_DELETED) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    dir->attributes = m_attributes & FS_ATTRIB_COPY;
    // do not set filesize for dir files
    if (isFile()) {
      setLe32(dir->fileSize, m_fileSize);
    }
    // update first cluster fields
    setLe16(dir->firstClusterLow, m_firstCluster & 0XFFFF);
    setLe16(dir->firstClusterHigh, m_firstCluster >> 16);

    // set modify time if user supplied a callback date/time function
    if (FsDateTime::callback) {
      FsDateTime::callback(&date, &time, &ms10);
      setLe16(dir->modifyDate, date);
      setLe16(dir->accessDate, date);
      setLe16(dir->modifyTime, time);
    }
    // clear directory dirty
    m_flags &= ~FILE_FLAG_DIR_DIRTY;
  }
  if (m_vol->cacheSync()) {
    return true;
  }
  DBG_FAIL_MACRO;

fail:
  m_error |= WRITE_ERROR;
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::timestamp(uint8_t flags, uint16_t year, uint8_t month,
                        uint8_t day, uint8_t hour, uint8_t minute,
                        uint8_t second) {
  uint16_t dirDate;
  uint16_t dirTime;
  DirFat_t* dir;

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
  dir = cacheDirEntry(FsCache::CACHE_FOR_WRITE);
  if (!dir) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  dirDate = FS_DATE(year, month, day);
  dirTime = FS_TIME(hour, minute, second);
  if (flags & T_ACCESS) {
    setLe16(dir->accessDate, dirDate);
  }
  if (flags & T_CREATE) {
    setLe16(dir->createDate, dirDate);
    setLe16(dir->createTime, dirTime);
    // units of 10 ms
    dir->createTimeMs = second & 1 ? 100 : 0;
  }
  if (flags & T_WRITE) {
    setLe16(dir->modifyDate, dirDate);
    setLe16(dir->modifyTime, dirTime);
  }
  return m_vol->cacheSync();

fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::truncate() {
  uint32_t toFree;
  // error if not a normal file or read-only
  if (!isWritable()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (m_firstCluster == 0) {
    return true;
  }
  if (m_curCluster) {
    toFree = 0;
    int8_t fg = m_vol->fatGet(m_curCluster, &toFree);
    if (fg < 0) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (fg) {
      // current cluster is end of chain
      if (!m_vol->fatPutEOC(m_curCluster)) {
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
  m_fileSize = m_curPosition;

  // need to update directory entry
  m_flags |= FILE_FLAG_DIR_DIRTY;
  return sync();

fail:
  return false;
}
//------------------------------------------------------------------------------
size_t FatFile::write(const void* buf, size_t nbyte) {
  // convert void* to uint8_t*  -  must be before goto statements
  const uint8_t* src = reinterpret_cast<const uint8_t*>(buf);
  uint8_t* pc;
  uint8_t cacheOption;
  // number of bytes left to write  -  must be before goto statements
  size_t nToWrite = nbyte;
  size_t n;
  // error if not a normal file or is read-only
  if (!isWritable()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // seek to end of file if append flag
  if ((m_flags & FILE_FLAG_APPEND)) {
    if (!seekSet(m_fileSize)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }
  // Don't exceed max fileSize.
  if (nbyte > (0XFFFFFFFF - m_curPosition)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  while (nToWrite) {
    uint8_t sectorOfCluster = m_vol->sectorOfCluster(m_curPosition);
    uint16_t sectorOffset = m_curPosition & m_vol->sectorMask();
    if (sectorOfCluster == 0 && sectorOffset == 0) {
      // start of new cluster
      if (m_curCluster != 0) {
#if USE_FAT_FILE_FLAG_CONTIGUOUS
        int8_t fg;
        if (isContiguous() && m_fileSize > m_curPosition) {
          m_curCluster++;
          fg = 1;
        } else {
          fg = m_vol->fatGet(m_curCluster, &m_curCluster);
          if (fg < 0) {
            DBG_FAIL_MACRO;
            goto fail;
          }
        }
#else   // USE_FAT_FILE_FLAG_CONTIGUOUS
        int8_t fg = m_vol->fatGet(m_curCluster, &m_curCluster);
        if (fg < 0) {
          DBG_FAIL_MACRO;
          goto fail;
        }
#endif  // USE_FAT_FILE_FLAG_CONTIGUOUS
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
    uint32_t sector = m_vol->clusterStartSector(m_curCluster) + sectorOfCluster;

    if (sectorOffset != 0 || nToWrite < m_vol->bytesPerSector()) {
      // partial sector - must use cache
      // max space in sector
      n = m_vol->bytesPerSector() - sectorOffset;
      // lesser of space and amount to write
      if (n > nToWrite) {
        n = nToWrite;
      }

      if (sectorOffset == 0 &&
          (m_curPosition >= m_fileSize || m_flags & FILE_FLAG_PREALLOCATE)) {
        // start of new sector don't need to read into cache
        cacheOption = FsCache::CACHE_RESERVE_FOR_WRITE;
      } else {
        // rewrite part of sector
        cacheOption = FsCache::CACHE_FOR_WRITE;
      }
      pc = m_vol->dataCachePrepare(sector, cacheOption);
      if (!pc) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      uint8_t* dst = pc + sectorOffset;
      memcpy(dst, src, n);
      if (m_vol->bytesPerSector() == (n + sectorOffset)) {
        // Force write if sector is full - improves large writes.
        if (!m_vol->cacheSyncData()) {
          DBG_FAIL_MACRO;
          goto fail;
        }
      }
#if USE_MULTI_SECTOR_IO
    } else if (nToWrite >= 2 * m_vol->bytesPerSector()) {
      // use multiple sector write command
      uint32_t maxSectors = m_vol->sectorsPerCluster() - sectorOfCluster;
      uint32_t nSector = nToWrite >> m_vol->bytesPerSectorShift();
      if (nSector > maxSectors) {
        nSector = maxSectors;
      }
      n = nSector << m_vol->bytesPerSectorShift();
      if (!m_vol->cacheSafeWrite(sector, src, nSector)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
#endif  // USE_MULTI_SECTOR_IO
    } else {
      // use single sector write command
      n = m_vol->bytesPerSector();
      if (!m_vol->cacheSafeWrite(sector, src)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    }
    m_curPosition += n;
    src += n;
    nToWrite -= n;
  }
  if (m_curPosition > m_fileSize) {
    // update fileSize and insure sync will update dir entry
    m_fileSize = m_curPosition;
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
