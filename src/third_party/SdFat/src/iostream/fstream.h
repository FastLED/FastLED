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
/**
 * \file
 * \brief iostreams for files.
 */
#ifndef fstream_h
#define fstream_h
#include "iostream.h"
//------------------------------------------------------------------------------
/**
 * \class StreamBaseClass
 * \brief base type for FAT and exFAT streams
 */
class StreamBaseClass : protected StreamBaseFile, virtual public ios {
 protected:
  void clearWriteError() { StreamBaseFile::clearWriteError(); }
  /* Internal do not use
   * \return mode
   */
  int16_t getch();
  bool getWriteError() { return StreamBaseFile::getWriteError(); }
  void open(const char* path, ios::openmode mode);
  /** Internal do not use
   * \return mode
   */
  ios::openmode getmode() { return m_mode; }
  void putch(char c);
  void putstr(const char* str);
  bool seekoff(off_type off, seekdir way);
  /** Internal do not use
   * \param[in] pos
   */
  bool seekpos(pos_type pos) { return StreamBaseFile::seekSet(pos); }
  /** Internal do not use
   * \param[in] mode
   */
  void setmode(ios::openmode mode) { m_mode = mode; }
  int write(const void* buf, size_t n);
  void write(char c);

 private:
  ios::openmode m_mode;
};
//==============================================================================
/**
 * \class fstream
 * \brief file input/output stream.
 */
class fstream : public iostream, StreamBaseClass {
 public:
  using iostream::peek;
  fstream() {}
  /** Constructor with open
   * \param[in] path file to open
   * \param[in] mode open mode
   */
  explicit fstream(const char* path, openmode mode = in | out) {
    open(path, mode);
  }
#if DESTRUCTOR_CLOSES_FILE
  ~fstream() {}
#endif  // DESTRUCTOR_CLOSES_FILE
  /** Clear state and writeError
   * \param[in] state new state for stream
   */
  void clear(iostate state = goodbit) {
    ios::clear(state);
    StreamBaseClass::clearWriteError();
  }
  /**  Close a file and force cached data and directory information
   *  to be written to the storage device.
   */
  void close() { StreamBaseClass::close(); }
  /** Open a fstream
   * \param[in] path path to open
   * \param[in] mode open mode
   *
   * Valid open modes are (at end, ios::ate, and/or ios::binary may be added):
   *
   * ios::in - Open file for reading.
   *
   * ios::out or ios::out | ios::trunc - Truncate to 0 length, if existent,
   * or create a file for writing only.
   *
   * ios::app or ios::out | ios::app - Append; open or create file for
   * writing at end-of-file.
   *
   * ios::in | ios::out - Open file for update (reading and writing).
   *
   * ios::in | ios::out | ios::trunc - Truncate to zero length, if existent,
   * or create file for update.
   *
   * ios::in | ios::app or ios::in | ios::out | ios::app - Append; open or
   * create text file for update, writing at end of file.
   */
  void open(const char* path, openmode mode = in | out) {
    StreamBaseClass::open(path, mode);
  }
  /** \return True if stream is open else false. */
  bool is_open() { return StreamBaseFile::isOpen(); }

 protected:
  /// @cond SHOW_PROTECTED
  /** Internal - do not use
   * \return
   */
  int16_t getch() { return StreamBaseClass::getch(); }
  /** Internal - do not use
   * \param[out] pos
   */
  void getpos(pos_t* pos) { StreamBaseFile::fgetpos(pos); }
  /** Internal - do not use
   * \param[in] c
   */
  void putch(char c) { StreamBaseClass::putch(c); }
  /** Internal - do not use
   * \param[in] str
   */
  void putstr(const char* str) { StreamBaseClass::putstr(str); }
  /** Internal - do not use
   * \param[in] pos
   */
  bool seekoff(off_type off, seekdir way) {
    return StreamBaseClass::seekoff(off, way);
  }
  bool seekpos(pos_type pos) { return StreamBaseClass::seekpos(pos); }
  void setpos(pos_t* pos) { StreamBaseFile::fsetpos(pos); }
  bool sync() { return StreamBaseClass::sync(); }
  pos_type tellpos() { return StreamBaseFile::curPosition(); }
  /// @endcond
};
//==============================================================================
/**
 * \class ifstream
 * \brief file input stream.
 */
class ifstream : public istream, StreamBaseClass {
 public:
  using istream::peek;
  ifstream() {}
  /** Constructor with open
   * \param[in] path file to open
   * \param[in] mode open mode
   */
  explicit ifstream(const char* path, openmode mode = in) { open(path, mode); }
#if DESTRUCTOR_CLOSES_FILE
  ~ifstream() {}
#endif  // DESTRUCTOR_CLOSES_FILE
  /**  Close a file and force cached data and directory information
   *  to be written to the storage device.
   */
  void close() { StreamBaseClass::close(); }
  /** \return True if stream is open else false. */
  bool is_open() { return StreamBaseFile::isOpen(); }
  /** Open an ifstream
   * \param[in] path file to open
   * \param[in] mode open mode
   *
   * \a mode See fstream::open() for valid modes.
   */
  void open(const char* path, openmode mode = in) {
    StreamBaseClass::open(path, mode | in);
  }

 protected:
  /// @cond SHOW_PROTECTED
  /** Internal - do not use
   * \return
   */
  int16_t getch() override { return StreamBaseClass::getch(); }
  /** Internal - do not use
   * \param[out] pos
   */
  void getpos(pos_t* pos) override { StreamBaseFile::fgetpos(pos); }
  /** Internal - do not use
   * \param[in] pos
   */
  bool seekoff(off_type off, seekdir way) override {
    return StreamBaseClass::seekoff(off, way);
  }
  bool seekpos(pos_type pos) override { return StreamBaseClass::seekpos(pos); }
  void setpos(pos_t* pos) override { StreamBaseFile::fsetpos(pos); }
  pos_type tellpos() override { return StreamBaseFile::curPosition(); }
  /// @endcond
};
//==============================================================================
/**
 * \class ofstream
 * \brief file output stream.
 */
class ofstream : public ostream, StreamBaseClass {
 public:
  ofstream() {}
  /** Constructor with open
   * \param[in] path file to open
   * \param[in] mode open mode
   */
  explicit ofstream(const char* path, openmode mode = out) { open(path, mode); }
#if DESTRUCTOR_CLOSES_FILE
  ~ofstream() {}
#endif  // DESTRUCTOR_CLOSES_FILE
  /** Clear state and writeError
   * \param[in] state new state for stream
   */
  void clear(iostate state = goodbit) {
    ios::clear(state);
    StreamBaseClass::clearWriteError();
  }
  /**  Close a file and force cached data and directory information
   *  to be written to the storage device.
   */
  void close() { StreamBaseClass::close(); }
  /** Open an ofstream
   * \param[in] path file to open
   * \param[in] mode open mode
   *
   * \a mode See fstream::open() for valid modes.
   */
  void open(const char* path, openmode mode = out) {
    StreamBaseClass::open(path, mode | out);
  }
  /** \return True if stream is open else false. */
  bool is_open() { return StreamBaseFile::isOpen(); }

 protected:
  /// @cond SHOW_PROTECTED
  /**
   * Internal do not use
   * \param[in] c
   */
  void putch(char c) override { StreamBaseClass::putch(c); }
  void putstr(const char* str) override { StreamBaseClass::putstr(str); }
  bool seekoff(off_type off, seekdir way) override {
    return StreamBaseClass::seekoff(off, way);
  }
  bool seekpos(pos_type pos) override { return StreamBaseClass::seekpos(pos); }
  /**
   * Internal do not use
   * \param[in] b
   */
  bool sync() override { return StreamBaseClass::sync(); }
  pos_type tellpos() override { return StreamBaseFile::curPosition(); }
  /// @endcond
};
#endif  // fstream_h
