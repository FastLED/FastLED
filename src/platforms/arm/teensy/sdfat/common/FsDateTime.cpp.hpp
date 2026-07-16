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
#include "platforms/arm/teensy/sdfat/common/SysCall.h"
#include "platforms/arm/teensy/sdfat/common/FsDateTime.h"
#include "platforms/arm/teensy/sdfat/common/FmtNumber.h"
#include "fl/stl/int.h"
#include "fl/stl/singleton.h"

namespace fl { namespace platforms { namespace teensy { namespace sdfat {

struct FsDateTimeState {
  void (*callback)(u16* date, u16* time, u8* ms10) = nullptr;
  void (*callback2)(u16* date, u16* time) = nullptr;
};

static FsDateTimeState& fsDateTimeState() {
  return Singleton<FsDateTimeState>::instance();
}

static void dateTimeMs10(u16* date, u16* time, u8* ms10) {
  *ms10 = 0;
  fsDateTimeState().callback2(date, time);
}

//------------------------------------------------------------------------------
/** Date time callback. */
namespace FsDateTime {
  void (*callback)(fl::u16* date, fl::u16* time, fl::u8* ms10) = nullptr;
  void (*callback2)(fl::u16* date, fl::u16* time) = nullptr;
  void clearCallback() {
    callback = nullptr;
    fsDateTimeState().callback = nullptr;
    fsDateTimeState().callback2 = nullptr;
  }
  void setCallback(void (*dateTime)(fl::u16* date, fl::u16* time)) {
    callback = dateTimeMs10;
    callback2 = dateTime;
    fsDateTimeState().callback = dateTimeMs10;
    fsDateTimeState().callback2 = dateTime;
  }
  void setCallback(
    void (*dateTime)(fl::u16* date, fl::u16* time, fl::u8* ms10)) {
    callback = dateTime;
    fsDateTimeState().callback = dateTime;
    fsDateTimeState().callback2 = nullptr;
  }
}  // namespace FsDateTime
//------------------------------------------------------------------------------
static char* fsFmtField(char* str, fl::u16 n, char sep) {
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
char* fsFmtDate(char* str, fl::u16 date) {
  str = fsFmtField(str, date & 31, 0);
  date >>= 5;
  str = fsFmtField(str, date & 15, '-');
  date >>= 4;
  return fsFmtField(str, 1980 + date, '-');
}
//------------------------------------------------------------------------------
char* fsFmtTime(char* str, fl::u16 time) {
  time >>= 5;
  str = fsFmtField(str, time & 63, 0);
  return fsFmtField(str, time >> 6, ':');
}
//------------------------------------------------------------------------------
char* fsFmtTime(char* str, fl::u16 time, fl::u8 sec100) {
  str = fsFmtField(str, 2*(time & 31) + (sec100 < 100 ? 0 : 1), 0);
  *--str = ':';
  return fsFmtTime(str, time);
}
//------------------------------------------------------------------------------
char* fsFmtTimeZone(char* str, fl::i8 tz) {
  char sign;
  if (tz & 0X80) {
    if (tz & 0X40) {
      sign = '-';
      tz = -tz;
    } else {
      sign = '+';
      tz &= 0X7F;
    }
    if (tz) {
      str = fsFmtField(str, 15*(tz%4), 0);
      str = fsFmtField(str, tz/4, ':');
      *--str = sign;
    }
    *--str = 'C';
    *--str = 'T';
    *--str = 'U';
  }
  return str;
}
//------------------------------------------------------------------------------
size_t fsPrintDate(print_t* pr, fl::u16 date) {
  // Allow YYYY-MM-DD
  char buf[sizeof("YYYY-MM-DD") -1];
  char* str = buf + sizeof(buf);
  if (date) {
    str = fsFmtDate(str, date);
  } else {
     do {
      *--str = ' ';
    } while (str > buf);
  }
  return pr->write(reinterpret_cast<fl::u8*>(str), buf + sizeof(buf) - str);  // ok reinterpret cast
}
//------------------------------------------------------------------------------
size_t fsPrintDateTime(print_t* pr, fl::u16 date, fl::u16 time) {
  // Allow YYYY-MM-DD hh:mm
  char buf[sizeof("YYYY-MM-DD hh:mm") -1];
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
  return pr->write(reinterpret_cast<fl::u8*>(str), buf + sizeof(buf) - str);  // ok reinterpret cast
}
//------------------------------------------------------------------------------
size_t fsPrintDateTime(print_t* pr, fl::u32 dateTime) {
  return fsPrintDateTime(pr, dateTime >> 16, dateTime & 0XFFFF);
}
//------------------------------------------------------------------------------
size_t fsPrintDateTime(print_t* pr,
                       fl::u32 dateTime, fl::u8 s100, fl::i8 tz) {
  // Allow YYYY-MM-DD hh:mm:ss UTC+hh:mm
  char buf[sizeof("YYYY-MM-DD hh:mm:ss UTC+hh:mm") -1];
  char* str = buf + sizeof(buf);
  if (tz) {
    str = fsFmtTimeZone(str, tz);
    *--str = ' ';
  }
  str = fsFmtTime(str, static_cast<fl::u16>(dateTime), s100);
  *--str = ' ';
  str = fsFmtDate(str, static_cast<fl::u16>(dateTime >> 16));
  return pr->write(reinterpret_cast<fl::u8*>(str), buf + sizeof(buf) - str);  // ok reinterpret cast
}
//------------------------------------------------------------------------------
size_t fsPrintTime(print_t* pr, fl::u16 time) {
  // Allow hh:mm
  char buf[sizeof("hh:mm") -1];
  char* str = buf + sizeof(buf);
  str = fsFmtTime(str, time);
  return pr->write(reinterpret_cast<fl::u8*>(str), buf + sizeof(buf) - str);  // ok reinterpret cast
}
//------------------------------------------------------------------------------
size_t fsPrintTime(print_t* pr, fl::u16 time, fl::u8 sec100) {
  // Allow hh:mm:ss
  char buf[sizeof("hh:mm:ss") -1];
  char* str = buf + sizeof(buf);
  str = fsFmtTime(str, time, sec100);
  return pr->write(reinterpret_cast<fl::u8*>(str), buf + sizeof(buf) - str);  // ok reinterpret cast
}
//------------------------------------------------------------------------------
size_t fsPrintTimeZone(print_t* pr, fl::i8 tz) {
  // Allow UTC+hh:mm
  char buf[sizeof("UTC+hh:mm") -1];
  char* str = buf + sizeof(buf);
  str = fsFmtTimeZone(str, tz);
  return pr->write(reinterpret_cast<fl::u8*>(str), buf + sizeof(buf) - str);  // ok reinterpret cast
}
} } } }  // namespace fl::platforms::teensy::sdfat
