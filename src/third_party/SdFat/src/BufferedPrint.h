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
#ifndef BufferedPrint_h
#define BufferedPrint_h
/**
 * \file
 * \brief Fast buffered print.
 */
#ifdef __AVR__
#include <avr/pgmspace.h>
#endif  // __AVR__
#include "common/FmtNumber.h"
/**
 * \class BufferedPrint
 * \brief Fast buffered print template.
 */
template <typename WriteClass, uint8_t BUF_DIM>
class BufferedPrint {
 public:
  BufferedPrint() : m_wr(nullptr), m_in(0) {}
  /** BufferedPrint constructor.
   * \param[in] wr Print destination.
   */
  explicit BufferedPrint(WriteClass* wr) : m_wr(wr), m_in(0) {}
  /** Initialize the BuffedPrint class.
   * \param[in] wr Print destination.
   */
  void begin(WriteClass* wr) {
    m_wr = wr;
    m_in = 0;
  }
  /** Flush the buffer - same as sync() with no status return. */
  void flush() { sync(); }
  /** Print a character followed by a field terminator.
   * \param[in] c character to print.
   * \param[in] term The field terminator.  Use '\\n' for CR LF.
   * \return true for success or false if an error occurs.
   */
  size_t printField(char c, char term) {
    char buf[3];
    char* str = buf + sizeof(buf);
    if (term) {
      *--str = term;
      if (term == '\n') {
        *--str = '\r';
      }
    }
    *--str = c;
    return write(str, buf + sizeof(buf) - str);
  }
  /** Print a string stored in AVR flash followed by a field terminator.
   * \param[in] fsh string to print.
   * \param[in] term The field terminator.  Use '\\n' for CR LF.
   * \return true for success or false if an error occurs.
   */
  size_t printField(const __FlashStringHelper* fsh, char term) {
#ifdef __AVR__
    size_t rtn = 0;
    PGM_P p = reinterpret_cast<PGM_P>(fsh);
    char c;
    while ((c = pgm_read_byte(p++))) {
      if (!write(&c, 1)) {
        return 0;
      }
      rtn++;
    }
    if (term) {
      char buf[2];
      char* str = buf + sizeof(buf);
      *--str = term;
      if (term == '\n') {
        *--str = '\r';
      }
      rtn += write(str, buf + sizeof(buf) - str);
    }
    return rtn;
#else   // __AVR__
    return printField(reinterpret_cast<const char*>(fsh), term);
#endif  // __AVR__
  }
  /** Print a string followed by a field terminator.
   * \param[in] str string to print.
   * \param[in] term The field terminator.  Use '\\n' for CR LF.
   * \return true for success or false if an error occurs.
   */
  size_t printField(const char* str, char term) {
    size_t rtn = write(str, strlen(str));
    if (term) {
      char buf[2];
      char* ptr = buf + sizeof(buf);
      *--ptr = term;
      if (term == '\n') {
        *--ptr = '\r';
      }
      rtn += write(ptr, buf + sizeof(buf) - ptr);
    }
    return rtn;
  }
  /** Print a double followed by a field terminator.
   * \param[in] d The number to be printed.
   * \param[in] term The field terminator.  Use '\\n' for CR LF.
   * \param[in] prec Number of digits after decimal point.
   * \return true for success or false if an error occurs.
   */
  size_t printField(double d, char term, uint8_t prec = 2) {
    char buf[24];
    char* str = buf + sizeof(buf);
    if (term) {
      *--str = term;
      if (term == '\n') {
        *--str = '\r';
      }
    }
    str = fmtDouble(str, d, prec, false);
    return write(str, buf + sizeof(buf) - str);
  }
  /** Print a float followed by a field terminator.
   * \param[in] f The number to be printed.
   * \param[in] term The field terminator.  Use '\\n' for CR LF.
   * \param[in] prec Number of digits after decimal point.
   * \return true for success or false if an error occurs.
   */
  size_t printField(float f, char term, uint8_t prec = 2) {
    return printField(static_cast<double>(f), term, prec);
  }
  /** Print an integer value for 8, 16, and 32 bit signed and unsigned types.
   * \param[in] n The value to print.
   * \param[in] term The field terminator.  Use '\\n' for CR LF.
   * \return true for success or false if an error occurs.
   */
  template <typename Type>
  size_t printField(Type n, char term) {
    const uint8_t DIM = sizeof(Type) <= 2 ? 8 : 13;
    char buf[DIM];
    char* str = buf + sizeof(buf);

    if (term) {
      *--str = term;
      if (term == '\n') {
        *--str = '\r';
      }
    }
    Type p = n < 0 ? -n : n;
    if (sizeof(Type) <= 2) {
      str = fmtBase10(str, (uint16_t)p);
    } else {
      str = fmtBase10(str, (uint32_t)p);
    }
    if (n < 0) {
      *--str = '-';
    }
    return write(str, buf + sizeof(buf) - str);
  }
  /** Print CR LF.
   * \return true for success or false if an error occurs.
   */
  size_t println() {
    char buf[2];
    buf[0] = '\r';
    buf[1] = '\n';
    return write(buf, 2);
  }
  /** Print a double.
   * \param[in] d The number to be printed.
   * \param[in] prec Number of digits after decimal point.
   * \return true for success or false if an error occurs.
   */
  size_t print(double d, uint8_t prec = 2) { return printField(d, 0, prec); }
  /** Print a double followed by CR LF.
   * \param[in] d The number to be printed.
   * \param[in] prec Number of digits after decimal point.
   * \return true for success or false if an error occurs.
   */
  size_t println(double d, uint8_t prec = 2) {
    return printField(d, '\n', prec);
  }
  /** Print a float.
   * \param[in] f The number to be printed.
   * \param[in] prec Number of digits after decimal point.
   * \return true for success or false if an error occurs.
   */
  size_t print(float f, uint8_t prec = 2) {
    return printField(static_cast<double>(f), 0, prec);
  }
  /** Print a float followed by CR LF.
   * \param[in] f The number to be printed.
   * \param[in] prec Number of digits after decimal point.
   * \return true for success or false if an error occurs.
   */
  size_t println(float f, uint8_t prec) {
    return printField(static_cast<double>(f), '\n', prec);
  }
  /** Print character, string, or number.
   * \param[in] v item to print.
   * \return true for success or false if an error occurs.
   */
  template <typename Type>
  size_t print(Type v) {
    return printField(v, 0);
  }
  /** Print character, string, or number followed by CR LF.
   * \param[in] v item to print.
   * \return true for success or false if an error occurs.
   */
  template <typename Type>
  size_t println(Type v) {
    return printField(v, '\n');
  }

  /** Flush the buffer.
   * \return true for success or false if an error occurs.
   */
  bool sync() {
    if (!m_wr || m_wr->write(m_buf, m_in) != m_in) {
      return false;
    }
    m_in = 0;
    return true;
  }
  /** Write data to an open file.
   * \param[in] src Pointer to the location of the data to be written.
   *
   * \param[in] n Number of bytes to write.
   *
   * \return For success write() returns the number of bytes written, always
   * \a n.
   */
  size_t write(const void* src, size_t n) {
    if ((m_in + n) > sizeof(m_buf)) {
      if (!sync()) {
        return 0;
      }
      if (n >= sizeof(m_buf)) {
        return n == m_wr->write((const uint8_t*)src, n) ? n : 0;
      }
    }
    memcpy(m_buf + m_in, src, n);
    m_in += n;
    return n;
  }

 private:
  WriteClass* m_wr;
  uint8_t m_in;
  // Insure room for double.
  uint8_t m_buf[BUF_DIM < 24 ? 24 : BUF_DIM];  // NOLINT
};
#endif  // BufferedPrint_h
