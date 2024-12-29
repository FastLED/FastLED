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
#include <math.h>
#define DBG_FILE "FatFilePrint.cpp"
#include "../common/DebugMacros.h"
#include "FatLib.h"

//------------------------------------------------------------------------------
bool FatFile::ls(print_t* pr, uint8_t flags, uint8_t indent) {
  FatFile file;
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
size_t FatFile::printAccessDate(print_t* pr) {
  uint16_t date;
  if (getAccessDate(&date)) {
    return fsPrintDate(pr, date);
  }
  return 0;
}
//------------------------------------------------------------------------------
size_t FatFile::printCreateDateTime(print_t* pr) {
  uint16_t date;
  uint16_t time;
  if (getCreateDateTime(&date, &time)) {
    return fsPrintDateTime(pr, date, time);
  }
  return 0;
}
//------------------------------------------------------------------------------
size_t FatFile::printModifyDateTime(print_t* pr) {
  uint16_t date;
  uint16_t time;
  if (getModifyDateTime(&date, &time)) {
    return fsPrintDateTime(pr, date, time);
  }
  return 0;
}
//------------------------------------------------------------------------------
size_t FatFile::printFileSize(print_t* pr) {
  char buf[11];
  char* ptr = buf + sizeof(buf);
  *--ptr = 0;
  ptr = fmtBase10(ptr, fileSize());
  while (ptr > buf) {
    *--ptr = ' ';
  }
  return pr->write(buf);
}
