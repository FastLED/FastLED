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
#include "fstream.h"
//------------------------------------------------------------------------------
int16_t StreamBaseClass::getch() {
  uint8_t c;
  int8_t s = StreamBaseFile::read(&c, 1);
  if (s != 1) {
    if (s < 0) {
      setstate(badbit);
    } else {
      setstate(eofbit);
    }
    return -1;
  }
  if (c != '\r' || (getmode() & ios::binary)) {
    return c;
  }
  s = StreamBaseFile::read(&c, 1);
  if (s == 1 && c == '\n') {
    return c;
  }
  if (s == 1) {
    StreamBaseFile::seekCur(-1);
  }
  return '\r';
}
//------------------------------------------------------------------------------
void StreamBaseClass::open(const char* path, ios::openmode mode) {
  oflag_t oflag;
  clearWriteError();
  switch (mode & (app | in | out | trunc)) {
    case app | in:
    case app | in | out:
      oflag = O_RDWR | O_APPEND | O_CREAT;
      break;

    case app:
    case app | out:
      oflag = O_WRONLY | O_APPEND | O_CREAT;
      break;

    case in:
      oflag = O_RDONLY;
      break;

    case in | out:
      oflag = O_RDWR | O_CREAT;
      break;

    case in | out | trunc:
      oflag = O_RDWR | O_TRUNC | O_CREAT;
      break;

    case out:
    case out | trunc:
      oflag = O_WRONLY | O_TRUNC | O_CREAT;
      break;

    default:
      goto fail;
  }
  if (mode & ios::ate) {
    oflag |= O_AT_END;
  }
  if (!StreamBaseFile::open(path, oflag)) {
    goto fail;
  }
  setmode(mode);
  clear();
  return;

fail:
  StreamBaseFile::close();
  setstate(failbit);
  return;
}
//------------------------------------------------------------------------------
void StreamBaseClass::putch(char c) {
  if (c == '\n' && !(getmode() & ios::binary)) {
    write('\r');
  }
  write(c);
  if (getWriteError()) {
    setstate(badbit);
  }
}
//------------------------------------------------------------------------------
void StreamBaseClass::putstr(const char* str) {
  size_t n = 0;
  while (1) {
    char c = str[n];
    if (c == '\0' || (c == '\n' && !(getmode() & ios::binary))) {
      if (n > 0) {
        write(str, n);
      }
      if (c == '\0') {
        break;
      }
      write('\r');
      str += n;
      n = 0;
    }
    n++;
  }
  if (getWriteError()) {
    setstate(badbit);
  }
}
//------------------------------------------------------------------------------
bool StreamBaseClass::seekoff(off_type off, seekdir way) {
  pos_type pos;
  switch (way) {
    case beg:
      pos = off;
      break;

    case cur:
      pos = StreamBaseFile::curPosition() + off;
      break;

    case end:
      pos = StreamBaseFile::fileSize() + off;
      break;

    default:
      return false;
  }
  return seekpos(pos);
}
//------------------------------------------------------------------------------
int StreamBaseClass::write(const void* buf, size_t n) {
  return StreamBaseFile::write(buf, n);
}
//------------------------------------------------------------------------------
void StreamBaseClass::write(char c) { StreamBaseFile::write(&c, 1); }
