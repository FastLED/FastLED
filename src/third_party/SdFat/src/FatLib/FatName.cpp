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
#define DBG_FILE "FatName.cpp"
#include "../common/DebugMacros.h"
#include "../common/FsUtf.h"
#include "FatLib.h"
//------------------------------------------------------------------------------
uint16_t FatFile::getLfnChar(DirLfn_t* ldir, uint8_t i) {
  if (i < 5) {
    return getLe16(ldir->unicode1 + 2 * i);
  } else if (i < 11) {
    return getLe16(ldir->unicode2 + 2 * (i - 5));
  } else if (i < 13) {
    return getLe16(ldir->unicode3 + 2 * (i - 11));
  }
  DBG_HALT_IF(i >= 13);
  return 0;
}
//------------------------------------------------------------------------------
size_t FatFile::getName(char* name, size_t size) {
#if !USE_LONG_FILE_NAMES
  return getSFN(name, size);
#elif USE_UTF8_LONG_NAMES
  return getName8(name, size);
#else
  return getName7(name, size);
#endif  // !USE_LONG_FILE_NAMES
}
//------------------------------------------------------------------------------
size_t FatFile::getName7(char* name, size_t size) {
  FatFile dir;
  DirLfn_t* ldir;
  size_t n = 0;
  if (!isOpen()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (!isLFN()) {
    return getSFN(name, size);
  }
  if (!dir.openCluster(this)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  for (uint8_t order = 1; order <= m_lfnOrd; order++) {
    ldir = reinterpret_cast<DirLfn_t*>(dir.cacheDir(m_dirIndex - order));
    if (!ldir) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (ldir->attributes != FAT_ATTRIB_LONG_NAME ||
        order != (ldir->order & 0X1F)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    for (uint8_t i = 0; i < 13; i++) {
      uint16_t c = getLfnChar(ldir, i);
      if (c == 0) {
        goto done;
      }
      if ((n + 1) >= size) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      name[n++] = c >= 0X7F ? '?' : c;
    }
  }
done:
  name[n] = 0;
  return n;

fail:
  name[0] = '\0';
  return 0;
}
//------------------------------------------------------------------------------
size_t FatFile::getName8(char* name, size_t size) {
  char* end = name + size;
  char* str = name;
  char* ptr;
  FatFile dir;
  DirLfn_t* ldir;
  uint16_t hs = 0;
  uint32_t cp;
  if (!isOpen()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (!isLFN()) {
    return getSFN(name, size);
  }
  if (!dir.openCluster(this)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  for (uint8_t order = 1; order <= m_lfnOrd; order++) {
    ldir = reinterpret_cast<DirLfn_t*>(dir.cacheDir(m_dirIndex - order));
    if (!ldir) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (ldir->attributes != FAT_ATTRIB_LONG_NAME ||
        order != (ldir->order & 0X1F)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    for (uint8_t i = 0; i < 13; i++) {
      uint16_t c = getLfnChar(ldir, i);
      if (hs) {
        if (!FsUtf::isLowSurrogate(c)) {
          DBG_FAIL_MACRO;
          goto fail;
        }
        cp = FsUtf::u16ToCp(hs, c);
        hs = 0;
      } else if (!FsUtf::isSurrogate(c)) {
        if (c == 0) {
          goto done;
        }
        cp = c;
      } else if (FsUtf::isHighSurrogate(c)) {
        hs = c;
        continue;
      } else {
        DBG_FAIL_MACRO;
        goto fail;
      }
      // Save space for zero byte.
      ptr = FsUtf::cpToMb(cp, str, end - 1);
      if (!ptr) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      str = ptr;
    }
  }
done:
  *str = '\0';
  return str - name;

fail:
  *name = 0;
  return 0;
}
//------------------------------------------------------------------------------
size_t FatFile::getSFN(char* name, size_t size) {
  char c;
  uint8_t j = 0;
  uint8_t lcBit = FAT_CASE_LC_BASE;
  uint8_t* ptr;
  DirFat_t* dir;
  if (!isOpen()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (isRoot()) {
    if (size < 2) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    name[0] = '/';
    name[1] = '\0';
    return 1;
  }
  // cache entry
  dir = cacheDirEntry(FsCache::CACHE_FOR_READ);
  if (!dir) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  ptr = dir->name;
  // format name
  for (uint8_t i = 0; i < 12; i++) {
    if (i == 8) {
      if (*ptr == ' ') {
        break;
      }
      lcBit = FAT_CASE_LC_EXT;
      c = '.';
    } else {
      c = *ptr++;
      if ('A' <= c && c <= 'Z' && (lcBit & dir->caseFlags)) {
        c += 'a' - 'A';
      }
      if (c == ' ') {
        continue;
      }
    }
    if ((j + 1u) >= size) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    name[j++] = c;
  }
  name[j] = '\0';
  return j;

fail:
  name[0] = '\0';
  return 0;
}
//------------------------------------------------------------------------------
size_t FatFile::printName(print_t* pr) {
#if !USE_LONG_FILE_NAMES
  return printSFN(pr);
#elif USE_UTF8_LONG_NAMES
  return printName8(pr);
#else   // USE_LONG_FILE_NAMES
  return printName7(pr);
#endif  // !USE_LONG_FILE_NAMES
}
//------------------------------------------------------------------------------
size_t FatFile::printName7(print_t* pr) {
  FatFile dir;
  DirLfn_t* ldir;
  size_t n = 0;
  uint8_t buf[13];
  uint8_t i;

  if (!isOpen()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (!isLFN()) {
    return printSFN(pr);
  }
  if (!dir.openCluster(this)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  for (uint8_t order = 1; order <= m_lfnOrd; order++) {
    ldir = reinterpret_cast<DirLfn_t*>(dir.cacheDir(m_dirIndex - order));
    if (!ldir) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (ldir->attributes != FAT_ATTRIB_LONG_NAME ||
        order != (ldir->order & 0X1F)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    for (i = 0; i < 13; i++) {
      uint16_t u = getLfnChar(ldir, i);
      if (u == 0) {
        // End of name.
        break;
      }
      buf[i] = u < 0X7F ? u : '?';
      n++;
    }
    pr->write(buf, i);
  }
  return n;

fail:
  return 0;
}
//------------------------------------------------------------------------------
size_t FatFile::printName8(print_t* pr) {
  FatFile dir;
  DirLfn_t* ldir;
  uint16_t hs = 0;
  uint32_t cp;
  size_t n = 0;
  char buf[5];
  char* end = buf + sizeof(buf);
  if (!isOpen()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (!isLFN()) {
    return printSFN(pr);
  }
  if (!dir.openCluster(this)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  for (uint8_t order = 1; order <= m_lfnOrd; order++) {
    ldir = reinterpret_cast<DirLfn_t*>(dir.cacheDir(m_dirIndex - order));
    if (!ldir) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (ldir->attributes != FAT_ATTRIB_LONG_NAME ||
        order != (ldir->order & 0X1F)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    for (uint8_t i = 0; i < 13; i++) {
      uint16_t c = getLfnChar(ldir, i);
      if (hs) {
        if (!FsUtf::isLowSurrogate(c)) {
          DBG_FAIL_MACRO;
          goto fail;
        }
        cp = FsUtf::u16ToCp(hs, c);
        hs = 0;
      } else if (!FsUtf::isSurrogate(c)) {
        if (c == 0) {
          break;
        }
        cp = c;
      } else if (FsUtf::isHighSurrogate(c)) {
        hs = c;
        continue;
      } else {
        DBG_FAIL_MACRO;
        goto fail;
      }
      char* str = FsUtf::cpToMb(cp, buf, end);
      if (!str) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      n += pr->write(reinterpret_cast<uint8_t*>(buf), str - buf);
    }
  }
  return n;

fail:
  return 0;
}
//------------------------------------------------------------------------------
size_t FatFile::printSFN(print_t* pr) {
  char name[13];
  if (!getSFN(name, sizeof(name))) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  return pr->write(name);

fail:
  return 0;
}
