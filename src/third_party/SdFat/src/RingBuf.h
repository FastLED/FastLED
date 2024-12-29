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
#ifndef RingBuf_h
#define RingBuf_h
/**
 * \file
 * \brief Ring buffer for data loggers.
 */
#include "common/FmtNumber.h"
#include "common/SysCall.h"

#ifndef DOXYGEN_SHOULD_SKIP_THIS
//  Teensy 3.5/3.6 has hard fault at 0x20000000 for unaligned memcpy.
#if defined(__MK64FX512__) || defined(__MK66FX1M0__)
inline bool is_aligned(const void* ptr, uintptr_t alignment) {
  auto iptr = reinterpret_cast<uintptr_t>(ptr);
  return !(iptr % alignment);
}
inline void memcpyBuf(void* dst, const void* src, size_t len) {
  const uint8_t* b = reinterpret_cast<const uint8_t*>(0X20000000UL);
  uint8_t* d = reinterpret_cast<uint8_t*>(dst);
  const uint8_t* s = reinterpret_cast<const uint8_t*>(src);
  if ((is_aligned(d, 4) && is_aligned(s, 4) && (len & 3) == 0) ||
      !((d < b && b <= (d + len)) || (s < b && b <= (s + len)))) {
    memcpy(dst, src, len);
  } else {
    while (len--) {
      *d++ = *s++;
    }
  }
}
#else   // defined(__MK64FX512__) || defined(__MK66FX1M0__)
inline void memcpyBuf(void* dst, const void* src, size_t len) {
  memcpy(dst, src, len);
}
#endif  // defined(__MK64FX512__) || defined(__MK66FX1M0__)
#endif  // DOXYGEN_SHOULD_SKIP_THIS
/**
 * \class RingBuf
 * \brief Ring buffer for data loggers and data transmitters.
 *
 * This ring buffer may be used in ISRs. Use beginISR(), endISR(), write()
 * and print() in the ISR and use writeOut() in non-interrupt code
 * to write data to a file.
 *
 * Use beginISR(), endISR() and read() in an ISR with readIn() in non-interrupt
 * code to provide file data to an ISR.
 */
template <class F, size_t Size>
class RingBuf : public Print {
 public:
  /**
   * RingBuf Constructor.
   */
  RingBuf() { begin(nullptr); }
  /**
   * Initialize RingBuf.
   * \param[in] file Underlying file.
   */
  void begin(F* file) {
    m_file = file;
    m_count = 0;
    m_head = 0;
    m_tail = 0;
    m_inISR = false;
    clearWriteError();
  }
  /**
   *  Disable protection of m_count by noInterrupts()/interrupts.
   */
  void beginISR() { m_inISR = true; }
  /**
   * \return the RingBuf free space in bytes.
   */
  size_t bytesFree() const { return Size - bytesUsed(); }
  /**
   * \return the RingBuf used space in bytes.
   */
  size_t bytesUsed() const {
    if (m_inISR) {
      return m_count;
    } else {
      noInterrupts();
      size_t rtn = m_count;
      interrupts();
      return rtn;
    }
  }
  /**
   * Enable protection of m_count by noInterrupts()/interrupts.
   */
  void endISR() { m_inISR = false; }
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  // See write(), read(), beginISR() and endISR().
  size_t __attribute__((error("use write(buf, count), beginISR(), endISR()")))
  memcpyIn(const void* buf, size_t count);
  size_t __attribute__((error("use read(buf, count), beginISR(), endISR()")))
  memcpyOut(void* buf, size_t count);
#endif  // DOXYGEN_SHOULD_SKIP_THIS
  /** Print a number followed by a field terminator.
   * \param[in] value The number to be printed.
   * \param[in] term The field terminator.  Use '\\n' for CR LF.
   * \param[in] prec Number of digits after decimal point.
   * \return The number of bytes written.
   */
  size_t printField(double value, char term, uint8_t prec = 2) {
    char buf[24];
    char* str = buf + sizeof(buf);
    if (term) {
      *--str = term;
      if (term == '\n') {
        *--str = '\r';
      }
    }
    str = fmtDouble(str, value, prec, false);
    return write(str, buf + sizeof(buf) - str);
  }
  /** Print a number followed by a field terminator.
   * \param[in] value The number to be printed.
   * \param[in] term The field terminator.  Use '\\n' for CR LF.
   * \param[in] prec Number of digits after decimal point.
   * \return The number of bytes written or -1 if an error occurs.
   */
  size_t printField(float value, char term, uint8_t prec = 2) {
    return printField(static_cast<double>(value), term, prec);
  }
  /** Print a number followed by a field terminator.
   * \param[in] value The number to be printed.
   * \param[in] term The field terminator.  Use '\\n' for CR LF.
   * \return The number of bytes written or -1 if an error occurs.
   */
  template <typename Type>
  size_t printField(Type value, char term) {
    char sign = 0;
    char buf[3 * sizeof(Type) + 3];
    char* str = buf + sizeof(buf);

    if (term) {
      *--str = term;
      if (term == '\n') {
        *--str = '\r';
      }
    }
    if (value < 0) {
      value = -value;
      sign = '-';
    }
    if (sizeof(Type) < 4) {
      str = fmtBase10(str, (uint16_t)value);
    } else {
      str = fmtBase10(str, (uint32_t)value);
    }
    if (sign) {
      *--str = sign;
    }
    return write((const uint8_t*)str, &buf[sizeof(buf)] - str);
  }
  /** Read data from RingBuf.
   * \param[out] buf destination for data.
   * \param[in] count number of bytes to read.
   * \return Actual count of bytes read.
   */
  size_t read(void* buf, size_t count) {
    size_t n = bytesFree();
    if (count > n) {
      count = n;
    }
    uint8_t* dst = reinterpret_cast<uint8_t*>(buf);
    n = minSize(Size - m_tail, count);
    if (n == count) {
      memcpyBuf(dst, m_buf + m_tail, n);
      m_tail = advance(m_tail, n);
    } else {
      memcpyBuf(dst, m_buf + m_tail, n);
      memcpyBuf(dst + n, m_buf, count - n);
      m_tail = count - n;
    }
    adjustCount(-count);
    return count;
  }
  /**
   * Efficient read for small types.
   *
   * \param[in] data location for data item.
   * \return true for success else false.
   */
  template <typename Type>
  bool read(Type* data) {
    if (bytesUsed() < sizeof(Type)) {
      return false;
    }
    uint8_t* ptr = reinterpret_cast<uint8_t*>(data);
    for (size_t i = 0; i < sizeof(Type); i++) {
      ptr[i] = m_buf[m_tail];
      m_tail = advance(m_tail);
    }
    adjustCount(-sizeof(Type));
    return true;
  }
  /**
   * Read data into the RingBuf from the underlying file.
   * the number of bytes read may be less than count if
   * bytesFree is less than count.
   *
   * This function must not be used in an ISR.
   *
   * \param[in] count number of bytes to be read.
   * \return Number of bytes actually read or negative for read error.
   */
  int readIn(size_t count) {
    size_t n = bytesFree();
    if (count > n) {
      count = n;
    }
    n = minSize(Size - m_head, count);
    auto rtn = m_file->read(m_buf + m_head, n);
    if (rtn <= 0) {
      return rtn;
    }
    size_t nread = rtn;
    if (n < count && nread == n) {
      rtn = m_file->read(m_buf, count - n);
      if (rtn > 0) {
        nread += rtn;
      }
    }
    m_head = advance(m_head, nread);
    adjustCount(nread);
    return nread;
  }
  /**
   * Write all data in the RingBuf to the underlying file.
   * \return true for success.
   */
  bool sync() {
    size_t n = bytesUsed();
    return n ? writeOut(n) == n : true;
  }
  /**
   * Copy data to the RingBuf from buf.
   *
   * No data will be copied if count is greater than bytesFree.
   * Use getWriteError() to check for print errors and
   * clearWriteError() to clear the error.
   *
   * \param[in] buf Location of data to be written.
   * \param[in] count number of bytes to be written.
   * \return Number of bytes actually written.
   */
  size_t write(const void* buf, size_t count) {
    if (bytesFree() < count) {
      setWriteError();
      return 0;
    }
    const uint8_t* src = (const uint8_t*)buf;
    size_t n = minSize(Size - m_head, count);
    if (n == count) {
      memcpyBuf(m_buf + m_head, src, n);
      m_head = advance(m_head, n);
    } else {
      memcpyBuf(m_buf + m_head, src, n);
      memcpyBuf(m_buf, src + n, count - n);
      m_head = count - n;
    }
    adjustCount(count);
    return count;
  }
  /**
   * Copy str to RingBuf.
   *
   * \param[in] str Location of data to be written.
   * \return Number of bytes actually written.
   */
  size_t write(const char* str) { return Print::write(str); }
  /**
   * Override virtual function in Print for efficiency.
   *
   * \param[in] buf Location of data to be written.
   * \param[in] count number of bytes to be written.
   * \return Number of bytes actually written.
   */
  size_t write(const uint8_t* buf, size_t count) override {
    return write((const void*)buf, count);
  }
  /**
   * Efficient write for small types.
   * \param[in] data Item to be written.
   * \return Number of bytes actually written.
   */
  template <typename Type>
  size_t write(Type data) {
    uint8_t* ptr = reinterpret_cast<uint8_t*>(&data);
    if (bytesFree() < sizeof(Type)) {
      setWriteError();
      return 0;
    }
    for (size_t i = 0; i < sizeof(Type); i++) {
      m_buf[m_head] = ptr[i];
      m_head = advance(m_head);
    }
    adjustCount(sizeof(Type));
    return sizeof(Type);
  }
  /**
   * Required function for Print.
   * \param[in] data Byte to be written.
   * \return Number of bytes actually written.
   *
   * Try to force devirtualization by using final and always_inline.
   */
  size_t write(uint8_t data) final __attribute__((always_inline)) {
    // Use this if above does not compile  size_t write(uint8_t data) final {
    return write<uint8_t>(data);
  }
  /**
   * Write data to file from RingBuf buffer.
   * \param[in] count number of bytes to be written.
   *
   * The number of bytes written may be less than count if
   * bytesUsed is less than count or if an error occurs.
   *
   * This function must only be used in non-interrupt code.
   *
   * \return Number of bytes actually written.
   */
  size_t writeOut(size_t count) {
    size_t n = bytesUsed();  // Protected from interrupts;
    if (count > n) {
      count = n;
    }
    n = minSize(Size - m_tail, count);
    auto rtn = m_file->write(m_buf + m_tail, n);
    if (rtn <= 0) {
      return 0;
    }
    size_t nwrite = rtn;
    if (n < count && nwrite == n) {
      rtn = m_file->write(m_buf, count - n);
      if (rtn > 0) {
        nwrite += rtn;
      }
    }
    m_tail = advance(m_tail, nwrite);
    adjustCount(-nwrite);
    return nwrite;
  }

 private:
  uint8_t __attribute__((aligned(4))) m_buf[Size];
  F* m_file;
  volatile size_t m_count;
  size_t m_head;
  size_t m_tail;
  volatile bool m_inISR;

  void adjustCount(int amount) {
    if (m_inISR) {
      m_count += amount;
    } else {
      noInterrupts();
      m_count += amount;
      interrupts();
    }
  }
  size_t advance(size_t index) {
    if (!((Size - 1) & Size)) {
      return (index + 1) & (Size - 1);
    }
    return index + 1 < Size ? index + 1 : 0;
  }
  size_t advance(size_t index, size_t n) {
    index += n;
    return index < Size ? index : index - Size;
  }
  // avoid macro MIN
  size_t minSize(size_t a, size_t b) { return a < b ? a : b; }
};
#endif  // RingBuf_h
