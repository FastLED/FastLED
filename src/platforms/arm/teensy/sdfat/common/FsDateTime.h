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
// IWYU pragma: private
#ifndef FsDateTime_h
#define FsDateTime_h
#include "fl/stl/int.h"
#include "platforms/arm/teensy/sdfat/common/CompileDateTime.h"
#include "platforms/arm/teensy/sdfat/common/SysCall.h"
namespace fl { namespace platforms { namespace teensy { namespace sdfat {

/** Backward compatible definition. */
#define FAT_DATE(y, m, d) FS_DATE(y, m, d)

/** Backward compatible definition. */
#define FAT_TIME(h, m, s) FS_TIME(h, m, s)

/** Date time callback */
namespace FsDateTime {
  /** Date time callback. */
  extern void (*callback)(fl::u16* date, fl::u16* time, fl::u8* ms10);
  /** Date time callback. */
  extern void (*callback2)(fl::u16* date, fl::u16* time);
  /** Cancel callback. */
  void clearCallback();
   /** Set the date/time callback function.
   *
   * \param[in] dateTime The user's call back function.  The callback.
   * function is of the form:
   *
   * \code
   * void dateTime(fl::u16* date, fl::u16* time) {
   *   fl::u16 year;
   *   fl::u8 month, day, hour, minute, second;
   *
   *   // User gets date and time from GPS or real-time clock here.
   *
   *   // Return date using FS_DATE macro to format fields.
   *   *date = FS_DATE(year, month, day);
   *
   *   // Return time using FS_TIME macro to format fields.
   *   *time = FS_TIME(hour, minute, second);
   * }
   * \endcode
   *
   * Sets the function that is called when a file is created or when
   * a file's directory entry is modified by sync(). All timestamps,
   * access, creation, and modify, are set when a file is created.
   * sync() maintains the last access date and last modify date/time.
   *
   */
  void setCallback(void (*dateTime)(fl::u16* date, fl::u16* time));
   /** Set the date/time callback function.
   *
   * \param[in] dateTime The user's call back function.  The callback
   * function is of the form:
   *
   * \code
   * void dateTime(fl::u16* date, fl::u16* time, fl::u8* ms10) {
   *   fl::u16 year;
   *   fl::u8 month, day, hour, minute, second;
   *
   *   // User gets date and time from GPS or real-time clock here.
   *
   *   // Return date using FS_DATE macro to format fields
   *   *date = FS_DATE(year, month, day);
   *
   *   // Return time using FS_TIME macro to format fields
   *   *time = FS_TIME(hour, minute, second);
   *
   *   // Return the time since the last even second in units of 10 ms.
   *   // The granularity of the seconds part of FS_TIME is 2 seconds so
   *   // this field is a count of hundredth of a second and its valid
   *   // range is 0-199 inclusive.
   *   // For a simple RTC return 100*(seconds & 1).
   *   *ms10 = <tens of ms since even second>
   * }
   * \endcode
   *
   * Sets the function that is called when a file is created or when
   * a file's directory entry is modified by sync(). All timestamps,
   * access, creation, and modify, are set when a file is created.
   * sync() maintains the last access date and last modify date/time.
   *
   */
  void setCallback(
    void (*dateTime)(fl::u16* date, fl::u16* time, fl::u8* ms10));
}  // namespace FsDateTime

/** date field for directory entry
 * \param[in] year [1980,2107]
 * \param[in] month [1,12]
 * \param[in] day [1,31]
 *
 * \return Packed date for directory entry.
 */
static inline fl::u16 FS_DATE(fl::u16 year, fl::u8 month, fl::u8 day) {
  year -= 1980;
  return year > 127 || month > 12 || day > 31 ? 0 :
         year << 9 | month << 5 | day;
}
/** year part of FAT directory date field
 * \param[in] fatDate Date in packed dir format.
 *
 * \return Extracted year [1980,2107]
 */
static inline fl::u16 FS_YEAR(fl::u16 fatDate) {
  return 1980 + (fatDate >> 9);
}
/** month part of FAT directory date field
 * \param[in] fatDate Date in packed dir format.
 *
 * \return Extracted month [1,12]
 */
static inline fl::u8 FS_MONTH(fl::u16 fatDate) {
  return (fatDate >> 5) & 0XF;
}
/** day part of FAT directory date field
 * \param[in] fatDate Date in packed dir format.
 *
 * \return Extracted day [1,31]
 */
static inline fl::u8 FS_DAY(fl::u16 fatDate) {
  return fatDate & 0X1F;
}
/** time field for directory entry
 * \param[in] hour [0,23]
 * \param[in] minute [0,59]
 * \param[in] second [0,59]
 *
 * \return Packed time for directory entry.
 */
static inline fl::u16 FS_TIME(fl::u8 hour, fl::u8 minute, fl::u8 second) {
  return hour > 23 || minute > 59 || second > 59 ? 0 :
         hour << 11 | minute << 5 | second >> 1;
}
/** hour part of FAT directory time field
 * \param[in] fatTime Time in packed dir format.
 *
 * \return Extracted hour [0,23]
 */
static inline fl::u8 FS_HOUR(fl::u16 fatTime) {
  return fatTime >> 11;
}
/** minute part of FAT directory time field
 * \param[in] fatTime Time in packed dir format.
 *
 * \return Extracted minute [0,59]
 */
static inline fl::u8 FS_MINUTE(fl::u16 fatTime) {
  return (fatTime >> 5) & 0X3F;
}
/** second part of FAT directory time field
 * N\note second/2 is stored in packed time.
 *
 * \param[in] fatTime Time in packed dir format.
 *
 * \return Extracted second [0,58]
 */
static inline fl::u8 FS_SECOND(fl::u16 fatTime) {
  return 2*(fatTime & 0X1F);
}
char* fsFmtDate(char* str, fl::u16 date);
char* fsFmtTime(char* str, fl::u16 time);
char* fsFmtTime(char* str, fl::u16 time, fl::u8 sec100);
char* fsFmtTimeZone(char* str, fl::i8 tz);
fl::size_t fsPrintDate(print_t* pr, fl::u16 date);
fl::size_t fsPrintDateTime(print_t* pr, fl::u16 date, fl::u16 time);
fl::size_t fsPrintDateTime(print_t* pr, fl::u32 dateTime);
fl::size_t fsPrintDateTime(print_t* pr, fl::u32 dateTime, fl::u8 s100, fl::i8 tz);
fl::size_t fsPrintTime(print_t* pr, fl::u16 time);
fl::size_t fsPrintTime(print_t* pr, fl::u16 time, fl::u8 sec100);
fl::size_t fsPrintTimeZone(print_t* pr, fl::i8 tz);
} } } }  // namespace fl::platforms::teensy::sdfat
#endif  // FsDateTime_h
