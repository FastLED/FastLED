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
#define DBG_FILE "FatFileLFN.cpp"
#include "platforms/arm/teensy/sdfat/common/DebugMacros.h"
#include "platforms/arm/teensy/sdfat/common/upcase.h"
#include "platforms/arm/teensy/sdfat/common/FsUtf.h"
#include "platforms/arm/teensy/sdfat/FatLib/FatLib.h"
#include "fl/stl/cstring.h"
#include "fl/stl/int.h"

namespace fl { namespace platforms { namespace teensy { namespace sdfat {
#if USE_LONG_FILE_NAMES
//------------------------------------------------------------------------------
static bool isLower(char c) {
  return 'a' <= c && c <= 'z';
}
//------------------------------------------------------------------------------
static bool isUpper(char c) {
  return 'A' <= c && c <= 'Z';
}
//------------------------------------------------------------------------------
// A bit smaller than toupper in AVR 328.
inline char toUpper(char c) {
  return isLower(c) ? c - 'a' + 'A' : c;
}
//------------------------------------------------------------------------------
/**
 * Store a 16-bit long file name character.
 *
 * \param[in] ldir Pointer to long file name directory entry.
 * \param[in] i Index of character.
 * \param[in] c The 16-bit character.
 */
static void putLfnChar(DirLfn_t* ldir, fl::u8 i, fl::u16 c) {
  if (i < 5) {
    setLe16(ldir->unicode1 + 2*i, c);
  } else if (i < 11) {
    setLe16(ldir->unicode2 + 2*i -10, c);
  } else if (i < 13) {
    setLe16(ldir->unicode3 + 2*i - 22, c);
  }
}
//------------------------------------------------------------------------------
// Daniel Bernstein University of Illinois at Chicago.
// Original had + instead of ^
__attribute__((unused))
static fl::u16 Bernstein(const char* bgn, const char* end, fl::u16 hash) {
  while (bgn < end) {
    // hash = hash * 33 ^ str[i];
    hash = ((hash << 5) + hash) ^ (*bgn++);
  }
  return hash;
}
//==============================================================================
bool FatFile::cmpName(fl::u16 index, FatLfn_t* fname, fl::u8 lfnOrd) {
  FatFile dir = *this;
  DirLfn_t* ldir;
  fname->reset();
  for (fl::u8 order = 1; order <= lfnOrd; order++) {
    ldir = reinterpret_cast<DirLfn_t*>(dir.cacheDir(index - order));  // ok reinterpret cast
    if (!ldir) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    // These should be checked in caller.
    DBG_HALT_IF(ldir->attributes != FAT_ATTRIB_LONG_NAME);
    DBG_HALT_IF(order != (ldir->order & 0X1F));
    for (fl::u8 i = 0; i < 13; i++) {
      fl::u16 u = getLfnChar(ldir, i);
      if (fname->atEnd()) {
        return u == 0;
      }
#if USE_UTF8_LONG_NAMES
      fl::u16 cp = fname->get16();
      // Make sure caller checked for valid UTF-8.
      DBG_HALT_IF(cp == 0XFFFF);
      if (toUpcase(u) != toUpcase(cp)) {
        return false;
      }
#else  // USE_UTF8_LONG_NAMES
      if (u > 0X7F || toUpper(u) != toUpper(fname->getch())) {
        return false;
      }
#endif  // USE_UTF8_LONG_NAMES
    }
  }
  return true;

 fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::createLFN(fl::u16 index, FatLfn_t* fname, fl::u8 lfnOrd) {
  FatFile dir = *this;
  DirLfn_t* ldir;
  fl::u8 checksum = lfnChecksum(fname->sfn);
  fl::u8 fc = 0;
  fname->reset();

  for (fl::u8 order = 1; order <= lfnOrd; order++) {
    ldir = reinterpret_cast<DirLfn_t*>(dir.cacheDir(index - order));  // ok reinterpret cast
    if (!ldir) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    dir.mVol->cacheDirty();
    ldir->order = order == lfnOrd ? FAT_ORDER_LAST_LONG_ENTRY | order : order;
    ldir->attributes = FAT_ATTRIB_LONG_NAME;
    ldir->mustBeZero1 = 0;
    ldir->checksum = checksum;
    setLe16(ldir->mustBeZero2, 0);
    for (fl::u8 i = 0; i < 13; i++) {
      fl::u16 cp;
      if (fname->atEnd()) {
        cp = fc++ ? 0XFFFF : 0;
      } else {
        cp = fname->get16();
        // Verify caller checked for valid UTF-8.
        DBG_HALT_IF(cp == 0XFFFF);
      }
      putLfnChar(ldir, i, cp);
    }
  }
  return true;

 fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::makeSFN(FatLfn_t* fname) {
  bool is83;
//  char c;
  fl::u8 c;
  fl::u8 bit = FAT_CASE_LC_BASE;
  fl::u8 lc = 0;
  fl::u8 uc = 0;
  fl::u8 i = 0;
  fl::u8 in = 7;
  const char* dot;
  const char* end = fname->end;
  const char* ptr = fname->begin;

  // Assume not zero length.
  DBG_HALT_IF(end == ptr);
  // Assume blanks removed from start and end.
  DBG_HALT_IF(*ptr == ' ' || *(end - 1) == ' ' || *(end - 1) == '.');

  // Blank file short name.
  for (fl::u8 k = 0; k < 11; k++) {
    fname->sfn[k] = ' ';
  }
  // Not 8.3 if starts with dot.
  is83 = *ptr == '.' ? false : true;
  // Skip leading dots.
  for (; *ptr == '.'; ptr++) {}
  // Find last dot.
  for (dot = end - 1; dot > ptr && *dot != '.'; dot--) {}

  for (; ptr < end; ptr++) {
    c = *ptr;
    if (c == '.' && ptr == dot) {
      in = 10;  // Max index for full 8.3 name.
      i = 8;    // Place for extension.
      bit = FAT_CASE_LC_EXT;  // bit for extension.
    } else {
      if (sfnReservedChar(c)) {
        is83 = false;
        // Skip UTF-8 trailing characters.
        if ((c & 0XC0) == 0X80) {
          continue;
        }
        c = '_';
      }
      if (i > in) {
        is83 = false;
        if (in == 10 || ptr > dot) {
         // Done - extension longer than three characters or no extension.
          break;
        }
        // Skip to dot.
        ptr = dot - 1;
        continue;
      }
      if (isLower(c)) {
        c += 'A' - 'a';
        lc |= bit;
      } else if (isUpper(c)) {
        uc |= bit;
      }
      fname->sfn[i++] = c;
      if (i < 7) {
        fname->seqPos = i;
      }
    }
  }
  if (fname->sfn[0] == ' ') {
    DBG_HALT_MACRO;
    goto fail;
  }
  if (is83) {
    fname->flags = lc & uc ? FNAME_FLAG_MIXED_CASE : lc;
  } else {
    fname->flags = FNAME_FLAG_LOST_CHARS;
    fname->sfn[fname->seqPos] = '~';
    fname->sfn[fname->seqPos + 1] = '1';
  }
  return true;

 fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::makeUniqueSfn(FatLfn_t* fname) {
  const fl::u8 FIRST_HASH_SEQ = 2;  // min value is 2
  fl::u8 pos = fname->seqPos;
  DirFat_t* dir;
  fl::u16 hex = 0;

  DBG_HALT_IF(!(fname->flags & FNAME_FLAG_LOST_CHARS));
  DBG_HALT_IF(fname->sfn[pos] != '~' && fname->sfn[pos + 1] != '1');

  for (fl::u8 seq = FIRST_HASH_SEQ; seq < 100; seq++) {
     DBG_WARN_IF(seq > FIRST_HASH_SEQ);
#ifdef USE_LFN_HASH
    hex = Bernstein(fname->begin, fname->end, seq);
#else
    hex += millis();
#endif
    if (pos > 3) {
      // Make space in name for ~HHHH.
      pos = 3;
    }
    for (fl::u8 i = pos + 4 ; i > pos; i--) {
      fl::u8 h = hex & 0XF;
      fname->sfn[i] = h < 10 ? h + '0' : h + 'A' - 10;
      hex >>= 4;
    }
    fname->sfn[pos] = '~';
    rewind();
    while (1) {
      dir = readDirCache(true);
      if (!dir) {
        if (!getError()) {
          // At EOF and name not found if no error.
          goto done;
        }
        DBG_FAIL_MACRO;
        goto fail;
      }
      if (dir->name[0] == FAT_NAME_FREE) {
        goto done;
      }
      if (isFileOrSubdir(dir) && !fl::memcmp(fname->sfn, dir->name, 11)) {
        // Name found - try another.
        break;
      }
    }
  }
  // fall inti fail - too many tries.
  DBG_FAIL_MACRO;

 fail:
  return false;

 done:
  return true;
}
//------------------------------------------------------------------------------
bool FatFile::open(FatFile* dirFile, FatLfn_t* fname, oflag_t oflag) {
  bool fnameFound = false;
  fl::u8 lfnOrd = 0;
  fl::u8 freeNeed;
  fl::u8 freeFound = 0;
  fl::u8 order = 0;
  fl::u8 checksum = 0;
  fl::u8 ms10;
  fl::u8 nameOrd;
  fl::u16 freeIndex = 0;
  fl::u16 curIndex;
  fl::u16 date;
  fl::u16 time;
  DirFat_t* dir;
  DirLfn_t* ldir;
  auto vol = dirFile->mVol;

  if (!dirFile->isDir() || isOpen()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // Number of directory entries needed.
  nameOrd = (fname->len + 12)/13;
  freeNeed = fname->flags & FNAME_FLAG_NEED_LFN ? 1 + nameOrd : 1;
  dirFile->rewind();
  while (1) {
    curIndex = dirFile->mCurPosition/FS_DIR_SIZE;
    dir = dirFile->readDirCache();
    if (!dir) {
      if (dirFile->getError()) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      // At EOF
      goto create;
    }
    if (dir->name[0] == FAT_NAME_DELETED || dir->name[0] == FAT_NAME_FREE) {
      if (freeFound == 0) {
        freeIndex = curIndex;
      }
      if (freeFound < freeNeed) {
        freeFound++;
      }
      if (dir->name[0] == FAT_NAME_FREE) {
        goto create;
      }
    } else {
      if (freeFound < freeNeed) {
        freeFound = 0;
      }
    }
    // skip empty slot or '.' or '..'
    if (dir->name[0] == FAT_NAME_DELETED || dir->name[0] == '.') {
      lfnOrd = 0;
    } else if (isLongName(dir)) {
      ldir = reinterpret_cast<DirLfn_t*>(dir);  // ok reinterpret cast
      if (!lfnOrd) {
        order = ldir->order & 0X1F;
        if (order != nameOrd ||
          (ldir->order & FAT_ORDER_LAST_LONG_ENTRY) == 0) {
          continue;
        }
        lfnOrd = nameOrd;
        checksum = ldir->checksum;
      } else if (ldir->order != --order || checksum != ldir->checksum) {
        lfnOrd = 0;
        continue;
      }
      if (order == 1) {
        if (!dirFile->cmpName(curIndex + 1, fname, lfnOrd)) {
          lfnOrd = 0;
        }
      }
    } else if (isFileOrSubdir(dir)) {
      if (lfnOrd) {
        if (1 == order && lfnChecksum(dir->name) == checksum) {
          goto found;
        }
        DBG_FAIL_MACRO;
        goto fail;
      }
      if (!fl::memcmp(dir->name, fname->sfn, sizeof(fname->sfn))) {
        if (!(fname->flags & FNAME_FLAG_LOST_CHARS)) {
          goto found;
        }
        fnameFound = true;
      }
    } else {
      lfnOrd = 0;
    }
  }

 found:
  // Don't open if create only.
  if (oflag & O_EXCL) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  goto open;

 create:
  // don't create unless O_CREAT and write mode
  if (!(oflag & O_CREAT) || !isWriteMode(oflag)) {
    DBG_WARN_MACRO;
    goto fail;
  }
  // Keep found entries or start at current index if no free entries found.
  if (freeFound == 0) {
    freeIndex = curIndex;
  }

  while (freeFound < freeNeed) {
    dir = dirFile->readDirCache();
    if (!dir) {
      if (dirFile->getError()) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      // EOF if no error.
      break;
    }
    freeFound++;
  }
  while (freeFound < freeNeed) {
    // Will fail if FAT16 root.
    if (!dirFile->addDirCluster()) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    freeFound += vol->dirEntriesPerCluster();
  }
  if (fnameFound) {
    if (!dirFile->makeUniqueSfn(fname)) {
      goto fail;
    }
  }
  lfnOrd = freeNeed - 1;
  curIndex = freeIndex + lfnOrd;
  if (!dirFile->createLFN(curIndex, fname, lfnOrd)) {
    goto fail;
  }
  dir = dirFile->cacheDir(curIndex);
  if (!dir) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // initialize as empty file
  fl::memset(dir, 0, sizeof(DirFat_t));
  fl::memcpy(dir->name, fname->sfn, 11);

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
  vol->cacheDirty();

 open:
  // open entry in cache.
  if (!openCachedEntry(dirFile, curIndex, oflag, lfnOrd)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  return true;

 fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::parsePathName(const char* path,
                            FatLfn_t* fname, const char** ptr) {
  size_t len = 0;
  // Skip leading spaces.
  while (*path == ' ') {
    path++;
  }
  fname->begin = path;
  while (*path && !isDirSeparator(*path)) {
#if USE_UTF8_LONG_NAMES
    fl::u32 cp;
    // Allow end = path + 4 since path is zero terminated.
    path = FsUtf::mbToCp(path, path + 4, &cp);
    if (!path) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    len += cp <= 0XFFFF ? 1 : 2;
    if (cp < 0X80 && lfnReservedChar(cp)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
#else  // USE_UTF8_LONG_NAMES
    fl::u8 cp = *path++;
    if (cp >= 0X80 || lfnReservedChar(cp)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    len++;
#endif  // USE_UTF8_LONG_NAMES
    if (cp != '.' && cp != ' ') {
      // Need to trim trailing dots spaces.
      fname->len = len;
      fname->end = path;
    }
  }
  if (!fname->len || fname->len > FAT_MAX_LFN_LENGTH) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // Advance to next path component.
  for (; *path == ' ' || isDirSeparator(*path); path++) {}
  *ptr = path;
  return makeSFN(fname);

 fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::remove() {
  bool last;
  fl::u8 checksum;
  FatFile dirFile;
  DirFat_t* dir;
  DirLfn_t* ldir;

  // Cant' remove not open for write.
  if (!isWritable()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // Free any clusters.
  if (mFirstCluster && !mVol->freeChain(mFirstCluster)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // Cache directory entry.
  dir = cacheDirEntry(FsCache::CACHE_FOR_WRITE);
  if (!dir) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  checksum = lfnChecksum(dir->name);

  // Mark entry deleted.
  dir->name[0] = FAT_NAME_DELETED;

  // Set this file closed.
  mAttributes = FILE_ATTR_CLOSED;
  mFlags = 0;

  // Write entry to device.
  if (!mVol->cacheSync()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (!isLFN()) {
    // Done, no LFN entries.
    return true;
  }
  if (!dirFile.openCluster(this)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  for (fl::u8 order = 1; order <= mLfnOrd; order++) {
    ldir = reinterpret_cast<DirLfn_t*>(dirFile.cacheDir(mDirIndex - order));  // ok reinterpret cast
    if (!ldir) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (ldir->attributes != FAT_ATTRIB_LONG_NAME ||
        order != (ldir->order & 0X1F) ||
        checksum != ldir->checksum) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    last = ldir->order & FAT_ORDER_LAST_LONG_ENTRY;
    ldir->order = FAT_NAME_DELETED;
    mVol->cacheDirty();
    if (last) {
      if (!mVol->cacheSync()) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      return true;
    }
  }
  // Fall into fail.
  DBG_FAIL_MACRO;

 fail:
  return false;
}

#endif  // #if USE_LONG_FILE_NAMES

}  // namespace sdfat
}  // namespace teensy
}  // namespace platforms
}  // namespace fl

#if defined(DBG_FILE)
#undef DBG_FILE
#endif
