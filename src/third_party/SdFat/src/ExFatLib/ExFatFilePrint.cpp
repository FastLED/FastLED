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
#define DBG_FILE "ExFatFilePrint.cpp"
#include "../common/DebugMacros.h"
#include "../common/FsUtf.h"
#include "ExFatLib.h"
//------------------------------------------------------------------------------
bool ExFatFile::ls(print_t* pr) {
  ExFatFile file;
  if (!isDir()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  rewind();
  while (file.openNext(this, O_RDONLY)) {
    if (!file.isHidden()) {
      file.printName(pr);
      if (file.isDir()) {
        pr->write('/');
      }
      pr->write('\r');
      pr->write('\n');
    }
    file.close();
  }
  if (getError()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  return true;

fail:
  return false;
}
//------------------------------------------------------------------------------
bool ExFatFile::ls(print_t* pr, uint8_t flags, uint8_t indent) {
  ExFatFile file;
  if (!isDir()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  rewind();
  while (file.openNext(this, O_RDONLY)) {
    // indent for dir level
    if (!file.isHidden() || (flags & LS_A)) {
      for (uint8_t i = 0; i < indent; i++) {
        pr->write(' ');
      }
      if (flags & LS_DATE) {
        file.printModifyDateTime(pr);
        pr->write(' ');
      }
      if (flags & LS_SIZE) {
        file.printFileSize(pr);
        pr->write(' ');
      }
      file.printName(pr);
      if (file.isDir()) {
        pr->write('/');
      }
      pr->write('\r');
      pr->write('\n');
      if ((flags & LS_R) && file.isDir()) {
        file.ls(pr, flags, indent + 2);
      }
    }
    file.close();
  }
  if (getError()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  return true;

fail:
  return false;
}
//------------------------------------------------------------------------------
size_t ExFatFile::printAccessDateTime(print_t* pr) {
  uint16_t date;
  uint16_t time;
  if (getAccessDateTime(&date, &time)) {
    return fsPrintDateTime(pr, date, time);
  }
  return 0;
}
//------------------------------------------------------------------------------
size_t ExFatFile::printCreateDateTime(print_t* pr) {
  uint16_t date;
  uint16_t time;
  if (getCreateDateTime(&date, &time)) {
    return fsPrintDateTime(pr, date, time);
  }
  return 0;
}
//------------------------------------------------------------------------------
size_t ExFatFile::printFileSize(print_t* pr) {
  uint64_t n = m_validLength;
  char buf[21];
  char* str = &buf[sizeof(buf) - 1];
  char* bgn = str - 12;
  *str = '\0';
  do {
    uint64_t m = n;
    n /= 10;
    *--str = m - 10 * n + '0';
  } while (n);
  while (str > bgn) {
    *--str = ' ';
  }
  return pr->write(str);
}
//------------------------------------------------------------------------------
size_t ExFatFile::printModifyDateTime(print_t* pr) {
  uint16_t date;
  uint16_t time;
  if (getModifyDateTime(&date, &time)) {
    return fsPrintDateTime(pr, date, time);
  }
  return 0;
}
//------------------------------------------------------------------------------
size_t ExFatFile::printName7(print_t* pr) {
  DirName_t* dn;
  size_t n = 0;
  uint8_t in;
  uint8_t buf[15];
  if (!isOpen()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  for (uint8_t is = 2; is <= m_setCount; is++) {
    dn = reinterpret_cast<DirName_t*>(dirCache(is, FsCache::CACHE_FOR_READ));
    if (!dn || dn->type != EXFAT_TYPE_NAME) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    for (in = 0; in < 15; in++) {
      uint16_t c = getLe16(dn->unicode + 2 * in);
      if (!c) {
        break;
      }
      buf[in] = c < 0X7F ? c : '?';
      n++;
    }
    pr->write(buf, in);
  }
  return n;

fail:
  return 0;
}
//------------------------------------------------------------------------------
size_t ExFatFile::printName8(print_t* pr) {
  DirName_t* dn;
  uint16_t hs = 0;
  uint32_t cp;
  size_t n = 0;
  uint8_t in;
  char buf[5];
  if (!isOpen()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  for (uint8_t is = 2; is <= m_setCount; is++) {
    dn = reinterpret_cast<DirName_t*>(dirCache(is, FsCache::CACHE_FOR_READ));
    if (!dn || dn->type != EXFAT_TYPE_NAME) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    for (in = 0; in < 15; in++) {
      uint16_t c = getLe16(dn->unicode + 2 * in);
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
      char* str = FsUtf::cpToMb(cp, buf, buf + sizeof(buf));
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
