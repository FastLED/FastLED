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
#include "FsDateTime.h"

#include "FmtNumber.h"
#include "SysCall.h"

static void dateTimeMs10(uint16_t* date, uint16_t* time, uint8_t* ms10) {
  *ms10 = 0;
  FsDateTime::callback2(date, time);
}
//------------------------------------------------------------------------------
/** Date time callback. */
namespace FsDateTime {
void (*callback)(uint16_t* date, uint16_t* time, uint8_t* ms10) = nullptr;
void (*callback2)(uint16_t* date, uint16_t* time) = nullptr;
void clearCallback() { callback = nullptr; }
void setCallback(void (*dateTime)(uint16_t* date, uint16_t* time)) {
  callback = dateTimeMs10;
  callback2 = dateTime;
}
void setCallback(void (*dateTime)(uint16_t* date, uint16_t* time,
                                  uint8_t* ms10)) {
  callback = dateTime;
}
}  // namespace FsDateTime
//------------------------------------------------------------------------------
static char* fsFmtField(char* str, uint16_t n, char sep) {
  if (sep) {
    *--str = sep;
  }
  str = fmtBase10(str, n);
  if (n < 10) {
    *--str = '0';
  }
  return str;
}
//------------------------------------------------------------------------------
char* fsFmtDate(char* str, uint16_t date) {
  str = fsFmtField(str, date & 31, 0);
  date >>= 5;
  str = fsFmtField(str, date & 15, '-');
  date >>= 4;
  return fsFmtField(str, 1980 + date, '-');
}
//------------------------------------------------------------------------------
char* fsFmtTime(char* str, uint16_t time) {
  time >>= 5;
  str = fsFmtField(str, time & 63, 0);
  return fsFmtField(str, time >> 6, ':');
}
//------------------------------------------------------------------------------
char* fsFmtTime(char* str, uint16_t time, uint8_t sec100) {
  str = fsFmtField(str, 2 * (time & 31) + (sec100 < 100 ? 0 : 1), 0);
  *--str = ':';
  return fsFmtTime(str, time);
}
//------------------------------------------------------------------------------
char* fsFmtTimeZone(char* str, int8_t tz) {
  if (tz & 0X80) {
    char sign;
    if (tz & 0X40) {
      sign = '-';
      tz = -tz;
    } else {
      sign = '+';
      tz &= 0X7F;
    }
    if (tz) {
      str = fsFmtField(str, 15 * (tz % 4), 0);
      str = fsFmtField(str, tz / 4, ':');
      *--str = sign;
    }
    *--str = 'C';
    *--str = 'T';
    *--str = 'U';
  }
  return str;
}
//------------------------------------------------------------------------------
size_t fsPrintDate(print_t* pr, uint16_t date) {
  // Allow YYYY-MM-DD
  char buf[sizeof("YYYY-MM-DD") - 1];
  char* str = buf + sizeof(buf);
  if (date) {
    str = fsFmtDate(str, date);
  } else {
    do {
      *--str = ' ';
    } while (str > buf);
  }
  return pr->write(reinterpret_cast<uint8_t*>(str), buf + sizeof(buf) - str);
}
//------------------------------------------------------------------------------
size_t fsPrintDateTime(print_t* pr, uint16_t date, uint16_t time) {
  // Allow YYYY-MM-DD hh:mm
  char buf[sizeof("YYYY-MM-DD hh:mm") - 1];
  char* str = buf + sizeof(buf);
  if (date) {
    str = fsFmtTime(str, time);
    *--str = ' ';
    str = fsFmtDate(str, date);
  } else {
    do {
      *--str = ' ';
    } while (str > buf);
  }
  return pr->write(reinterpret_cast<uint8_t*>(str), buf + sizeof(buf) - str);
}
//------------------------------------------------------------------------------
size_t fsPrintDateTime(print_t* pr, uint32_t dateTime) {
  return fsPrintDateTime(pr, dateTime >> 16, dateTime & 0XFFFF);
}
//------------------------------------------------------------------------------
size_t fsPrintDateTime(print_t* pr, uint32_t dateTime, uint8_t s100,
                       int8_t tz) {
  // Allow YYYY-MM-DD hh:mm:ss UTC+hh:mm
  char buf[sizeof("YYYY-MM-DD hh:mm:ss UTC+hh:mm") - 1];
  char* str = buf + sizeof(buf);
  if (tz) {
    str = fsFmtTimeZone(str, tz);
    *--str = ' ';
  }
  str = fsFmtTime(str, (uint16_t)dateTime, s100);
  *--str = ' ';
  str = fsFmtDate(str, (uint16_t)(dateTime >> 16));
  return pr->write(reinterpret_cast<uint8_t*>(str), buf + sizeof(buf) - str);
}
//------------------------------------------------------------------------------
size_t fsPrintTime(print_t* pr, uint16_t time) {
  // Allow hh:mm
  char buf[sizeof("hh:mm") - 1];
  char* str = buf + sizeof(buf);
  str = fsFmtTime(str, time);
  return pr->write(reinterpret_cast<uint8_t*>(str), buf + sizeof(buf) - str);
}
//------------------------------------------------------------------------------
size_t fsPrintTime(print_t* pr, uint16_t time, uint8_t sec100) {
  // Allow hh:mm:ss
  char buf[sizeof("hh:mm:ss") - 1];
  char* str = buf + sizeof(buf);
  str = fsFmtTime(str, time, sec100);
  return pr->write(reinterpret_cast<uint8_t*>(str), buf + sizeof(buf) - str);
}
//------------------------------------------------------------------------------
size_t fsPrintTimeZone(print_t* pr, int8_t tz) {
  // Allow UTC+hh:mm
  char buf[sizeof("UTC+hh:mm") - 1];
  char* str = buf + sizeof(buf);
  str = fsFmtTimeZone(str, tz);
  return pr->write(reinterpret_cast<uint8_t*>(str), buf + sizeof(buf) - str);
}
