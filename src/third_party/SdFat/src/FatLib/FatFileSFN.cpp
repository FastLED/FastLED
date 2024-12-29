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
#define DBG_FILE "FatFileSFN.cpp"
#include "../common/DebugMacros.h"
#include "FatLib.h"
//------------------------------------------------------------------------------
// open with filename in fname
#define SFN_OPEN_USES_CHKSUM 0
bool FatFile::open(FatFile* dirFile, FatSfn_t* fname, oflag_t oflag) {
  uint16_t date;
  uint16_t time;
  uint8_t ms10;
  bool emptyFound = false;
#if SFN_OPEN_USES_CHKSUM
  uint8_t checksum;
#endif  // SFN_OPEN_USES_CHKSUM
  uint8_t lfnOrd = 0;
  uint16_t emptyIndex = 0;
  uint16_t index = 0;
  DirFat_t* dir;
  DirLfn_t* ldir;

  dirFile->rewind();
  while (true) {
    dir = dirFile->readDirCache(true);
    if (!dir) {
      if (dirFile->getError()) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      // At EOF if no error.
      break;
    }
    if (dir->name[0] == FAT_NAME_DELETED || dir->name[0] == FAT_NAME_FREE) {
      if (!emptyFound) {
        emptyIndex = index;
        emptyFound = true;
      }
      if (dir->name[0] == FAT_NAME_FREE) {
        break;
      }
      lfnOrd = 0;
    } else if (isFatFileOrSubdir(dir)) {
      if (!memcmp(fname->sfn, dir->name, 11)) {
        // don't open existing file if O_EXCL
        if (oflag & O_EXCL) {
          DBG_FAIL_MACRO;
          goto fail;
        }
#if SFN_OPEN_USES_CHKSUM
        if (lfnOrd && checksum != lfnChecksum(dir->name)) {
          DBG_FAIL_MACRO;
          goto fail;
        }
#endif  // SFN_OPEN_USES_CHKSUM
        if (!openCachedEntry(dirFile, index, oflag, lfnOrd)) {
          DBG_FAIL_MACRO;
          goto fail;
        }
        return true;
      } else {
        lfnOrd = 0;
      }
    } else if (isFatLongName(dir)) {
      ldir = reinterpret_cast<DirLfn_t*>(dir);
      if (ldir->order & FAT_ORDER_LAST_LONG_ENTRY) {
        lfnOrd = ldir->order & 0X1F;
#if SFN_OPEN_USES_CHKSUM
        checksum = ldir->checksum;
#endif  // SFN_OPEN_USES_CHKSUM
      }
    } else {
      lfnOrd = 0;
    }
    index++;
  }
  // don't create unless O_CREAT and write mode
  if (!(oflag & O_CREAT) || !isWriteMode(oflag)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (emptyFound) {
    index = emptyIndex;
  } else {
    if (!dirFile->addDirCluster()) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }
  dir = reinterpret_cast<DirFat_t*>(dirFile->cacheDir(index));
  if (!dir) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // initialize as empty file
  memset(dir, 0, sizeof(DirFat_t));
  memcpy(dir->name, fname->sfn, 11);

  // Set base-name and extension lower case bits.
  dir->caseFlags = (FAT_CASE_LC_BASE | FAT_CASE_LC_EXT) & fname->flags;

  // Set timestamps.
  if (FsDateTime::callback) {
    // call user date/time function
    FsDateTime::callback(&date, &time, &ms10);
    setLe16(dir->createDate, date);
    setLe16(dir->createTime, time);
    dir->createTimeMs = ms10;
  } else {
    setLe16(dir->createDate, FS_DEFAULT_DATE);
    setLe16(dir->modifyDate, FS_DEFAULT_DATE);
    setLe16(dir->accessDate, FS_DEFAULT_DATE);
    if (FS_DEFAULT_TIME) {
      setLe16(dir->createTime, FS_DEFAULT_TIME);
      setLe16(dir->modifyTime, FS_DEFAULT_TIME);
    }
  }
  // Force write of entry to device.
  dirFile->m_vol->cacheDirty();

  // open entry in cache.
  return openCachedEntry(dirFile, index, oflag, 0);

fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::openExistingSFN(const char* path) {
  FatSfn_t fname;
  auto vol = FatVolume::cwv();
  while (*path == '/') {
    path++;
  }
  if (*path == 0) {
    return openRoot(vol);
  }
  // *this = *vol->vwd();
  this->copy(vol->vwd());
  do {
    if (!parsePathName(path, &fname, &path)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (!openSFN(&fname)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  } while (*path);
  return true;

fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::openSFN(FatSfn_t* fname) {
  DirFat_t dir;
  DirLfn_t* ldir;
  auto vol = m_vol;
  uint8_t lfnOrd = 0;
  if (!isDir()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  while (true) {
    if (read(&dir, sizeof(dir)) != sizeof(dir)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (dir.name[0] == 0) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (isFatFileOrSubdir(&dir) && memcmp(fname->sfn, dir.name, 11) == 0) {
      uint16_t saveDirIndex = (m_curPosition - sizeof(dir)) >> 5;
      uint32_t saveDirCluster = m_firstCluster;
      memset(this, 0, sizeof(FatFile));
      m_attributes = dir.attributes & FS_ATTRIB_COPY;
      m_flags = FILE_FLAG_READ;
      if (isFatFile(&dir)) {
        m_attributes |= FILE_ATTR_FILE;
        if (!isReadOnly()) {
          m_attributes |= FS_ATTRIB_ARCHIVE;
          m_flags |= FILE_FLAG_WRITE;
        }
      }
      m_lfnOrd = lfnOrd;
      m_firstCluster = (uint32_t)getLe16(dir.firstClusterHigh) << 16;
      m_firstCluster |= getLe16(dir.firstClusterLow);
      m_fileSize = getLe32(dir.fileSize);
      m_vol = vol;
      m_dirCluster = saveDirCluster;
      m_dirSector = m_vol->cacheSectorNumber();
      m_dirIndex = saveDirIndex;
      return true;
    } else if (isFatLongName(&dir)) {
      ldir = reinterpret_cast<DirLfn_t*>(&dir);
      if (ldir->order & FAT_ORDER_LAST_LONG_ENTRY) {
        lfnOrd = ldir->order & 0X1F;
      }
    } else {
      lfnOrd = 0;
    }
  }

fail:
  return false;
}
//------------------------------------------------------------------------------
// format directory name field from a 8.3 name string
bool FatFile::parsePathName(const char* path, FatSfn_t* fname,
                            const char** ptr) {
  uint8_t uc = 0;
  uint8_t lc = 0;
  uint8_t bit = FNAME_FLAG_LC_BASE;
  // blank fill name and extension
  for (uint8_t i = 0; i < 11; i++) {
    fname->sfn[i] = ' ';
  }
  for (uint8_t i = 0, n = 7;; path++) {
    uint8_t c = *path;
    if (c == 0 || isDirSeparator(c)) {
      // Done.
      break;
    }
    if (c == '.' && n == 7) {
      n = 10;  // max index for full 8.3 name
      i = 8;   // place for extension

      // bit for extension.
      bit = FNAME_FLAG_LC_EXT;
    } else {
      if (sfnReservedChar(c) || i > n) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      if ('a' <= c && c <= 'z') {
        c += 'A' - 'a';
        lc |= bit;
      } else if ('A' <= c && c <= 'Z') {
        uc |= bit;
      }
      fname->sfn[i++] = c;
    }
  }
  // must have a file name, extension is optional
  if (fname->sfn[0] == ' ') {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // Set base-name and extension bits.
  fname->flags = (lc & uc) ? 0 : lc;
  while (isDirSeparator(*path)) {
    path++;
  }
  *ptr = path;
  return true;

fail:
  return false;
}
#if !USE_LONG_FILE_NAMES
//------------------------------------------------------------------------------
bool FatFile::remove() {
  DirFat_t* dir;
  // Can't remove if LFN or not open for write.
  if (!isWritable() || isLFN()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // Free any clusters.
  if (m_firstCluster && !m_vol->freeChain(m_firstCluster)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // Cache directory entry.
  dir = cacheDirEntry(FsCache::CACHE_FOR_WRITE);
  if (!dir) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // Mark entry deleted.
  dir->name[0] = FAT_NAME_DELETED;

  // Set this file closed.
  m_attributes = FILE_ATTR_CLOSED;
  m_flags = 0;

  // Write entry to device.
  return m_vol->cacheSync();

fail:
  return false;
}
#endif  // !USE_LONG_FILE_NAMES
