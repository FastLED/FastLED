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
#ifndef ArduinoFiles_h
#define ArduinoFiles_h
#include "SysCall.h"
//------------------------------------------------------------------------------
/** Arduino SD.h style flag for open for read. */
#ifndef FILE_READ
#define FILE_READ O_RDONLY
#endif  // FILE_READ
/** Arduino SD.h style flag for open at EOF for read/write with create. */
#ifndef FILE_WRITE
#define FILE_WRITE (O_RDWR | O_CREAT | O_AT_END)
#endif  // FILE_WRITE
//------------------------------------------------------------------------------
/**
 * \class PrintFile
 * \brief PrintFile class.
 */
template <class BaseFile>
class PrintFile : public print_t, public BaseFile {
 public:
  using BaseFile::clearWriteError;
  using BaseFile::getWriteError;
  using BaseFile::read;
  using BaseFile::write;
  /** Write a single byte.
   * \param[in] b byte to write.
   * \return one for success.
   */
  size_t write(uint8_t b) { return BaseFile::write(&b, 1); }
};
//------------------------------------------------------------------------------
/**
 * \class StreamFile
 * \brief StreamFile class.
 */
template <class BaseFile, typename PosType>
class StreamFile : public stream_t, public BaseFile {
 public:
  using BaseFile::clearWriteError;
  using BaseFile::getWriteError;
  using BaseFile::read;
  using BaseFile::write;

  StreamFile() {}

  /** \return number of bytes available from the current position to EOF
   *   or INT_MAX if more than INT_MAX bytes are available.
   */
  int available() { return BaseFile::available(); }
  /** Ensure that any bytes written to the file are saved to the SD card. */
  void flush() { BaseFile::sync(); }
  /** This function reports if the current file is a directory or not.
   * \return true if the file is a directory.
   */
  bool isDirectory() { return BaseFile::isDir(); }
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  char* __attribute__((error("use getName(name, size)"))) name();
#endif  // DOXYGEN_SHOULD_SKIP_THIS
  /** Return the next available byte without consuming it.
   *
   * \return The byte if no error and not at eof else -1;
   */
  int peek() { return BaseFile::peek(); }
  /** \return the current file position. */
  PosType position() { return BaseFile::curPosition(); }
  /** Read the next byte from a file.
   *
   * \return For success return the next byte in the file as an int.
   * If an error occurs or end of file is reached return -1.
   */
  int read() { return BaseFile::read(); }
  /** Rewind a file if it is a directory */
  void rewindDirectory() {
    if (BaseFile::isDir()) {
      BaseFile::rewind();
    }
  }
  /**
   * Seek to a new position in the file, which must be between
   * 0 and the size of the file (inclusive).
   *
   * \param[in] pos the new file position.
   * \return true for success or false for failure.
   */
  bool seek(PosType pos) { return BaseFile::seekSet(pos); }
  /** \return the file's size. */
  PosType size() { return BaseFile::fileSize(); }
  /** Write a byte to a file. Required by the Arduino Print class.
   * \param[in] b the byte to be written.
   * Use getWriteError to check for errors.
   * \return 1 for success and 0 for failure.
   */
  size_t write(uint8_t b) { return BaseFile::write(b); }
  /** Write data to an open file.
   *
   * \note Data is moved to the cache but may not be written to the
   * storage device until sync() is called.
   *
   * \param[in] buffer Pointer to the location of the data to be written.
   *
   * \param[in] size Number of bytes to write.
   *
   * \return For success write() returns the number of bytes written, always
   * \a size.
   */
  size_t write(const uint8_t* buffer, size_t size) {
    return BaseFile::write(buffer, size);
  }
};
#endif  // ArduinoFiles_h
