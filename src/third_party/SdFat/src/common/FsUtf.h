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
#ifndef FsUtf_h
#define FsUtf_h
/**
 * \file
 * \brief Unicode Transformation Format functions.
 */
#include <stddef.h>
#include <stdint.h>
namespace FsUtf {
/** High surrogate for a code point.
 * \param{in} cp code point.
 * \return high surrogate.
 */
inline uint16_t highSurrogate(uint32_t cp) {
  return (cp >> 10) + (0XD800 - (0X10000 >> 10));
}
/** Low surrogate for a code point.
 * \param{in} cp code point.
 * \return low surrogate.
 */
inline uint16_t lowSurrogate(uint32_t cp) { return (cp & 0X3FF) + 0XDC00; }
/** Check for a valid code point.
 * \param[in] cp code point.
 * \return true if valid else false.
 */
inline bool isValidCp(uint32_t cp) {
  return cp <= 0x10FFFF && (cp < 0XD800 || cp > 0XDFFF);
}
/** Check for UTF-16 surrogate.
 * \param[in] c UTF-16 unit.
 * \return true if c is a surrogate else false.
 */
inline bool isSurrogate(uint16_t c) { return 0XD800 <= c && c <= 0XDFFF; }
/** Check for UTF-16 high surrogate.
 * \param[in] c UTF-16 unit..
 * \return true if c is a high surrogate else false.
 */
inline bool isHighSurrogate(uint16_t c) { return 0XD800 <= c && c <= 0XDBFF; }
/** Check for UTF-16 low surrogate.
 * \param[in] c UTF-16 unit..
 * \return true if c is a low surrogate else false.
 */
inline bool isLowSurrogate(uint16_t c) { return 0XDC00 <= c && c <= 0XDFFF; }
/** Convert UFT-16 surrogate pair to code point.
 * \param[in] hs high surrogate.
 * \param[in] ls low surrogate.
 * \return code point.
 */
inline uint32_t u16ToCp(uint16_t hs, uint16_t ls) {
  return 0X10000 + (((hs & 0X3FF) << 10) | (ls & 0X3FF));
}
/** Encodes a 32 bit code point as a UTF-8 sequence.
 * \param[in] cp code point to encode.
 * \param[out] str location for UTF-8 sequence.
 * \param[in] end location following last character of str.
 * \return location one beyond last encoded character.
 */
char* cpToMb(uint32_t cp, char* str, const char* end);
/** Get next code point from a UTF-8 sequence.
 * \param[in] str location for UTF-8 sequence.
 * \param[in] end location following last character of str.
 *            May be nullptr if str is zero terminated.
 * \param[out] rtn location for the code point.
 * \return location of next UTF-8 character in str of nullptr for error.
 */
const char* mbToCp(const char* str, const char* end, uint32_t* rtn);
/** Get next code point from a UTF-8 sequence as UTF-16.
 * \param[in] str location for UTF-8 sequence.
 * \param[in] end location following last character of str.
 * \param[out] hs location for the code point or high surrogate.
 * \param[out] ls location for zero or high surrogate.
 * \return location of next UTF-8 character in str of nullptr for error.
 */
const char* mbToU16(const char* str, const char* end, uint16_t* hs,
                    uint16_t* ls);
}  // namespace FsUtf
#endif  // FsUtf_h
