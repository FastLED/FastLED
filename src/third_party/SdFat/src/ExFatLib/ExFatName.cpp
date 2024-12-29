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
#define DBG_FILE "ExFatName.cpp"
#include "../common/DebugMacros.h"
#include "../common/FsUtf.h"
#include "../common/upcase.h"
#include "ExFatLib.h"
//------------------------------------------------------------------------------
static char toUpper(char c) { return 'a' <= c && c <= 'z' ? c - 'a' + 'A' : c; }
//------------------------------------------------------------------------------
inline uint16_t exFatHash(char c, uint16_t hash) {
  uint8_t u = toUpper(c);
  hash = ((hash << 15) | (hash >> 1)) + u;
  hash = ((hash << 15) | (hash >> 1));
  return hash;
}
//------------------------------------------------------------------------------
inline uint16_t exFatHash(uint16_t u, uint16_t hash) {
  uint16_t c = toUpcase(u);
  hash = ((hash << 15) | (hash >> 1)) + (c & 0XFF);
  hash = ((hash << 15) | (hash >> 1)) + (c >> 8);
  return hash;
}
//------------------------------------------------------------------------------
bool ExFatFile::cmpName(const DirName_t* dirName, ExName_t* fname) {
  for (uint8_t i = 0; i < 15; i++) {
    uint16_t u = getLe16(dirName->unicode + 2 * i);
    if (fname->atEnd()) {
      return u == 0;
    }
#if USE_UTF8_LONG_NAMES
    uint16_t cp = fname->get16();
    if (toUpcase(cp) != toUpcase(u)) {
      return false;
    }
#else   // USE_UTF8_LONG_NAMES
    char c = fname->getch();
    if (u >= 0x7F || toUpper(c) != toUpper(u)) {
      return false;
    }
#endif  // USE_UTF8_LONG_NAMES
  }
  return true;
}
//------------------------------------------------------------------------------
size_t ExFatFile::getName7(char* name, size_t count) {
  DirName_t* dn;
  size_t n = 0;
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
    for (uint8_t in = 0; in < 15; in++) {
      uint16_t c = getLe16(dn->unicode + 2 * in);
      if (c == 0) {
        goto done;
      }
      if ((n + 1) >= count) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      name[n++] = c < 0X7F ? c : '?';
    }
  }
done:
  name[n] = 0;
  return n;

fail:
  *name = 0;
  return 0;
}
//------------------------------------------------------------------------------
size_t ExFatFile::getName8(char* name, size_t count) {
  char* end = name + count;
  char* str = name;
  char* ptr;
  DirName_t* dn;
  uint16_t hs = 0;
  uint32_t cp;
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
    for (uint8_t in = 0; in < 15; in++) {
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
bool ExFatFile::hashName(ExName_t* fname) {
  uint16_t hash = 0;
  fname->reset();
#if USE_UTF8_LONG_NAMES
  fname->nameLength = 0;
  while (!fname->atEnd()) {
    uint16_t u = fname->get16();
    if (u == 0XFFFF) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    hash = exFatHash(u, hash);
    fname->nameLength++;
  }
#else   // USE_UTF8_LONG_NAMES
  while (!fname->atEnd()) {
    // Convert to byte for smaller exFatHash.
    char c = fname->getch();
    hash = exFatHash(c, hash);
  }
  fname->nameLength = fname->end - fname->begin;
#endif  // USE_UTF8_LONG_NAMES
  fname->nameHash = hash;
  if (!fname->nameLength || fname->nameLength > EXFAT_MAX_NAME_LENGTH) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  return true;

fail:
  return false;
}
