/**
 * Copyright (c) 2011-2024 Bill Greiman
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
#ifndef ExFatFile_h
#define ExFatFile_h
/**
 * \file
 * \brief ExFatFile class
 */
#include <limits.h>
#include <string.h>

#include "../common/FmtNumber.h"
#include "../common/FsApiConstants.h"
#include "../common/FsDateTime.h"
#include "../common/FsName.h"
#include "ExFatPartition.h"

class ExFatVolume;
//------------------------------------------------------------------------------
/** Expression for path name separator. */
#define isDirSeparator(c) ((c) == '/')
//------------------------------------------------------------------------------
/**
 * \class ExName_t
 * \brief Internal type for file name - do not use in user apps.
 */
class ExName_t : public FsName {
 public:
  /** Length of UTF-16 name */
  size_t nameLength;
  /** Hash for UTF-16 name */
  uint16_t nameHash;
};
//------------------------------------------------------------------------------
/**
 * \class ExFatFile
 * \brief Basic file class.
 */
class ExFatFile {
 public:
  /** Create an instance. */
  ExFatFile() {}
  /**  Create a file object and open it in the current working directory.
   *
   * \param[in] path A path for a file to be opened.
   *
   * \param[in] oflag Values for \a oflag are constructed by a bitwise-inclusive
   * OR of open flags. see FatFile::open(FatFile*, const char*, uint8_t).
   */
  ExFatFile(const char* path, oflag_t oflag) { open(path, oflag); }

  /** Copy from to this.
   * \param[in] from Source file.
   */
  void copy(const ExFatFile* from) {
    if (from != this) {
#if FILE_COPY_CONSTRUCTOR_SELECT
      *this = *from;
#else   // FILE_COPY_CONSTRUCTOR_SELECT
      memcpy(this, from, sizeof(ExFatFile));
#endif  // FILE_COPY_CONSTRUCTOR_SELECT
    }
  }
  /** move from to this.
   * \param[in] from Source file.
   */
  void move(ExFatFile* from) {
    if (from != this) {
      copy(from);
      from->m_attributes = FILE_ATTR_CLOSED;
    }
  }

#if FILE_COPY_CONSTRUCTOR_SELECT == FILE_COPY_CONSTRUCTOR_PUBLIC
  /** Copy constructor.
   * \param[in] from Move from file.
   *
   */
  ExFatFile(const ExFatFile& from) = default;
  /** Copy assignment operator.
   * \param[in] from Move from file.
   * \return Copied file.
   */
  ExFatFile& operator=(const ExFatFile& from) = default;
#elif FILE_COPY_CONSTRUCTOR_SELECT == FILE_COPY_CONSTRUCTOR_PRIVATE

 private:
  ExFatFile(const ExFatFile& from) = default;
  ExFatFile& operator=(const ExFatFile& from) = default;

 public:
#else   // FILE_COPY_CONSTRUCTOR_SELECT
  ExFatFile(const ExFatFile& from) = delete;
  ExFatFile& operator=(const ExFatFile& from) = delete;
#endif  // FILE_COPY_CONSTRUCTOR_SELECT

#if FILE_MOVE_CONSTRUCTOR_SELECT
  /** Move constructor.
   * \param[in] from Move from file.
   */
  ExFatFile(ExFatFile&& from) { move(&from); }
  /** Move assignment operator.
   * \param[in] from Move from file.
   * \return Moved file.
   */
  ExFatFile& operator=(ExFatFile&& from) {
    move(&from);
    return *this;
  }
#else  // FILE_MOVE_CONSTRUCTOR_SELECT
  ExFatFile(ExFatFile&& from) = delete;
  ExFatFile& operator=(ExFatFile&& from) = delete;
#endif

  /** Destructor */
#if DESTRUCTOR_CLOSES_FILE
  ~ExFatFile() {
    if (isOpen()) {
      close();
    }
  }
#else   // DESTRUCTOR_CLOSES_FILE
  ~ExFatFile() = default;
#endif  // DESTRUCTOR_CLOSES_FILE

  /** The parenthesis operator.
   *
   * \return true if a file is open.
   */
  operator bool() { return isOpen(); }
  /**
   * \return user settable file attributes for success else -1.
   */
  int attrib() { return isFileOrSubDir() ? m_attributes & FS_ATTRIB_COPY : -1; }
  /** Set file attributes
   *
   * \param[in] bits bit-wise or of selected attributes: FS_ATTRIB_READ_ONLY,
   *            FS_ATTRIB_HIDDEN, FS_ATTRIB_SYSTEM, FS_ATTRIB_ARCHIVE.
   *
   * \note attrib() will fail for set read-only if the file is open for write.
   * \return true for success or false for failure.
   */
  bool attrib(uint8_t bits);
  /** \return The number of bytes available from the current position
   * to EOF for normal files.  INT_MAX is returned for very large files.
   *
   * available64() is recommended for very large files.
   *
   * Zero is returned for directory files.
   *
   */
  int available() {
    uint64_t n = available64();
    return n > INT_MAX ? INT_MAX : n;
  }
  /** \return The number of bytes available from the current position
   * to EOF for normal files.  Zero is returned for directory files.
   */
  uint64_t available64() { return isFile() ? fileSize() - curPosition() : 0; }
  /** Clear all error bits. */
  void clearError() { m_error = 0; }
  /** Clear writeError. */
  void clearWriteError() { m_error &= ~WRITE_ERROR; }
  /** Close a file and force cached data and directory information
   *  to be written to the storage device.
   *
   * \return true for success or false for failure.
   */
  bool close();
  /** Check for contiguous file and return its raw sector range.
   *
   * \param[out] bgnSector the first sector address for the file.
   * \param[out] endSector the last sector address for the file.
   *
   * Parameters may be nullptr.
   *
   * \return true for success or false for failure.
   */
  bool contiguousRange(uint32_t* bgnSector, uint32_t* endSector);
  /** \return The current cluster number for a file or directory. */
  uint32_t curCluster() const { return m_curCluster; }
  /** \return The current position for a file or directory. */
  uint64_t curPosition() const { return m_curPosition; }
  /** \return Total data length for file. */
  uint64_t dataLength() const { return m_dataLength; }
  /** \return Directory entry index. */
  uint32_t dirIndex() const { return m_dirPos.position / FS_DIR_SIZE; }
  /** Test for the existence of a file in a directory
   *
   * \param[in] path Path of the file to be tested for.
   *
   * The calling instance must be an open directory file.
   *
   * dirFile.exists("TOFIND.TXT") searches for "TOFIND.TXT" in the directory
   * dirFile.
   *
   * \return true if the file exists else false.
   */
  bool exists(const char* path) {
    ExFatFile file;
    return file.open(this, path, O_RDONLY);
  }
  /** get position for streams
   * \param[out] pos struct to receive position
   */
  void fgetpos(fspos_t* pos) const;
  /**
   * Get a string from a file.
   *
   * fgets() reads bytes from a file into the array pointed to by \a str, until
   * \a num - 1 bytes are read, or a delimiter is read and transferred to
   * \a str, or end-of-file is encountered. The string is then terminated
   * with a null byte.
   *
   * fgets() deletes CR, '\\r', from the string.  This insures only a '\\n'
   * terminates the string for Windows text files which use CRLF for newline.
   *
   * \param[out] str Pointer to the array where the string is stored.
   * \param[in] num Maximum number of characters to be read
   * (including the final null byte). Usually the length
   * of the array \a str is used.
   * \param[in] delim Optional set of delimiters. The default is "\n".
   *
   * \return For success fgets() returns the length of the string in \a str.
   * If no data is read, fgets() returns zero for EOF or -1 if an error
   * occurred.
   */
  int fgets(char* str, int num, char* delim = nullptr);
  /** \return The total number of bytes in a file. */
  uint64_t fileSize() const { return m_validLength; }
  /** \return Address of first sector or zero for empty file. */
  uint32_t firstSector() const;
  /** Set position for streams
   * \param[in] pos struct with value for new position
   */
  void fsetpos(const fspos_t* pos);
  /** Arduino name for sync() */
  void flush() { sync(); }
  /** Get a file's access date and time.
   *
   * \param[out] pdate Packed date for directory entry.
   * \param[out] ptime Packed time for directory entry.
   *
   * \return true for success or false for failure.
   */
  bool getAccessDateTime(uint16_t* pdate, uint16_t* ptime);
  /** Get a file's create date and time.
   *
   * \param[out] pdate Packed date for directory entry.
   * \param[out] ptime Packed time for directory entry.
   *
   * \return true for success or false for failure.
   */
  bool getCreateDateTime(uint16_t* pdate, uint16_t* ptime);
  /** \return All error bits. */
  uint8_t getError() const { return isOpen() ? m_error : 0XFF; }
  /** Get a file's modify date and time.
   *
   * \param[out] pdate Packed date for directory entry.
   * \param[out] ptime Packed time for directory entry.
   *
   * \return true for success or false for failure.
   */
  bool getModifyDateTime(uint16_t* pdate, uint16_t* ptime);
  /**
   * Get a file's name followed by a zero.
   *
   * \param[out] name An array of characters for the file's name.
   * \param[in] size The size of the array in characters.
   * \return length for success or zero for failure.
   */
  size_t getName(char* name, size_t size) {
#if USE_UTF8_LONG_NAMES
    return getName8(name, size);
#else   // USE_UTF8_LONG_NAMES
    return getName7(name, size);
#endif  // USE_UTF8_LONG_NAMES
  }
  /**
   * Get a file's ASCII name followed by a zero.
   *
   * \param[out] name An array of characters for the file's name.
   * \param[in] size The size of the array in characters.
   * \return the name length.
   */
  size_t getName7(char* name, size_t size);
  /**
   * Get a file's UTF-8 name followed by a zero.
   *
   * \param[out] name An array of characters for the file's name.
   * \param[in] size The size of the array in characters.
   * \return the name length.
   */
  size_t getName8(char* name, size_t size);
  /** \return value of writeError */
  bool getWriteError() const { return isOpen() ? m_error & WRITE_ERROR : true; }
  /**
   * Check for FsBlockDevice busy.
   *
   * \return true if busy else false.
   */
  bool isBusy();
  /** \return True if the file is contiguous. */
  bool isContiguous() const { return m_flags & FILE_FLAG_CONTIGUOUS; }
  /** \return True if this is a directory. */
  bool isDir() const { return m_attributes & FILE_ATTR_DIR; }
  /** \return True if this is a normal file. */
  bool isFile() const { return m_attributes & FILE_ATTR_FILE; }
  /** \return True if this is a normal file or sub-directory. */
  bool isFileOrSubDir() const { return isFile() || isSubDir(); }
  /** \return True if this is a hidden. */
  bool isHidden() const { return m_attributes & FS_ATTRIB_HIDDEN; }
  /** \return true if the file is open. */
  bool isOpen() const { return m_attributes; }
  /** \return True if file is read-only */
  bool isReadOnly() const { return m_attributes & FS_ATTRIB_READ_ONLY; }
  /** \return True if this is the root directory. */
  bool isRoot() const { return m_attributes & FILE_ATTR_ROOT; }
  /** \return True file is readable. */
  bool isReadable() const { return m_flags & FILE_FLAG_READ; }
  /** \return True if this is a sub-directory. */
  bool isSubDir() const { return m_attributes & FILE_ATTR_SUBDIR; }
  /** \return True if this is a system file. */
  bool isSystem() const { return m_attributes & FS_ATTRIB_SYSTEM; }
  /** \return True file is writable. */
  bool isWritable() const { return m_flags & FILE_FLAG_WRITE; }
  /** List directory contents.
   *
   * \param[in] pr Print stream for list.
   * \return true for success or false for failure.
   */
  bool ls(print_t* pr);
  /** List directory contents.
   *
   * \param[in] pr Print stream for list.
   *
   * \param[in] flags The inclusive OR of
   *
   * LS_DATE - %Print file modification date
   *
   * LS_SIZE - %Print file size.
   *
   * LS_R - Recursive list of sub-directories.
   *
   * \param[in] indent Amount of space before file name. Used for recursive
   * list to indicate sub-directory level.
   *
   * \return true for success or false for failure.
   */
  bool ls(print_t* pr, uint8_t flags, uint8_t indent = 0);
  /** Make a new directory.
   *
   * \param[in] parent An open directory file that will
   *                   contain the new directory.
   *
   * \param[in] path A path with a valid name for the new directory.
   *
   * \param[in] pFlag Create missing parent directories if true.
   *
   * \return true for success or false for failure.
   */
  bool mkdir(ExFatFile* parent, const char* path, bool pFlag = true);
  /** Open a file or directory by name.
   *
   * \param[in] dirFile An open directory containing the file to be opened.
   *
   * \param[in] path The path for a file to be opened.
   *
   * \param[in] oflag Values for \a oflag are constructed by a
   *                  bitwise-inclusive OR of flags from the following list.
   *                  Only one of O_RDONLY, O_READ, O_WRONLY, O_WRITE, or
   *                  O_RDWR is allowed.
   *
   * O_RDONLY - Open for reading.
   *
   * O_READ - Same as O_RDONLY.
   *
   * O_WRONLY - Open for writing.
   *
   * O_WRITE - Same as O_WRONLY.
   *
   * O_RDWR - Open for reading and writing.
   *
   * O_APPEND - If set, the file offset shall be set to the end of the
   * file prior to each write.
   *
   * O_AT_END - Set the initial position at the end of the file.
   *
   * O_CREAT - If the file exists, this flag has no effect except as noted
   * under O_EXCL below. Otherwise, the file shall be created
   *
   * O_EXCL - If O_CREAT and O_EXCL are set, open() shall fail if the file
   * exists.
   *
   * O_TRUNC - If the file exists and is a regular file, and the file is
   * successfully opened and is not read only, its length shall be truncated
   * to 0.
   *
   * WARNING: A given file must not be opened by more than one file object
   * or file corruption may occur.
   *
   * \note Directory files must be opened read only.  Write and truncation is
   * not allowed for directory files.
   *
   * \return true for success or false for failure.
   */
  bool open(ExFatFile* dirFile, const char* path, oflag_t oflag = O_RDONLY);
  /** Open a file in the volume working directory.
   *
   * \param[in] vol Volume where the file is located.
   *
   * \param[in] path with a valid name for a file to be opened.
   *
   * \param[in] oflag bitwise-inclusive OR of open flags.
   *                  See see open(ExFatFile*, const char*, uint8_t).
   *
   * \return true for success or false for failure.
   */
  bool open(ExFatVolume* vol, const char* path, oflag_t oflag = O_RDONLY);
  /** Open a file by index.
   *
   * \param[in] dirFile An open ExFatFile instance for the directory.
   *
   * \param[in] index The \a index of the directory entry for the file to be
   * opened.  The value for \a index is (directory file position)/32.
   *
   * \param[in] oflag bitwise-inclusive OR of open flags.
   *            See see ExFatFile::open(ExFatFile*, const char*, uint8_t).
   *
   * See open() by path for definition of flags.
   * \return true for success or false for failure.
   */
  bool open(ExFatFile* dirFile, uint32_t index, oflag_t oflag = O_RDONLY);
  /** Open a file by index in the current working directory.
   *
   * \param[in] index The \a index of the directory entry for the file to be
   * opened.  The value for \a index is (directory file position)/32.
   *
   * \param[in] oflag bitwise-inclusive OR of open flags.
   *                  See see FatFile::open(FatFile*, const char*, uint8_t).
   *
   * See open() by path for definition of flags.
   * \return true for success or false for failure.
   */
  bool open(uint32_t index, oflag_t oflag = O_RDONLY);
  /** Open a file in the current working directory.
   *
   * \param[in] path A path with a valid name for a file to be opened.
   *
   * \param[in] oflag bitwise-inclusive OR of open flags.
   *                  See see ExFatFile::open(ExFatFile*, const char*, uint8_t).
   *
   * \return true for success or false for failure.
   */
  bool open(const char* path, oflag_t oflag = O_RDONLY);
  /** Open the current working directory.
   *
   * \return true for success or false for failure.
   */
  bool openCwd();
  /** Open the next file or subdirectory in a directory.
   *
   * \param[in] dirFile An open instance for the directory
   *                    containing the file to be opened.
   *
   * \param[in] oflag bitwise-inclusive OR of open flags.
   *                  See see open(ExFatFile*, const char*, uint8_t).
   *
   * \return true for success or false for failure.
   */
  bool openNext(ExFatFile* dirFile, oflag_t oflag = O_RDONLY);
  /** Open a volume's root directory.
   *
   * \param[in] vol The FAT volume containing the root directory to be opened.
   *
   * \return true for success or false for failure.
   */
  bool openRoot(ExFatVolume* vol);
  /** Return the next available byte without consuming it.
   *
   * \return The byte if no error and not at eof else -1;
   */
  int peek();
  /** Allocate contiguous clusters to an empty file.
   *
   * The file must be empty with no clusters allocated.
   *
   * The file will have zero validLength and dataLength
   * will equal the requested length.
   *
   * \param[in] length size of allocated space in bytes.
   * \return true for success or false for failure.
   */
  bool preAllocate(uint64_t length);
  /** Print a file's access date and time
   *
   * \param[in] pr Print stream for output.
   *
   * \return true for success or false for failure.
   */
  size_t printAccessDateTime(print_t* pr);
  /** Print a file's creation date and time
   *
   * \param[in] pr Print stream for output.
   *
   * \return true for success or false for failure.
   */
  size_t printCreateDateTime(print_t* pr);
  /** Print a number followed by a field terminator.
   * \param[in] value The number to be printed.
   * \param[in] term The field terminator.  Use '\\n' for CR LF.
   * \param[in] prec Number of digits after decimal point.
   * \return The number of bytes written or -1 if an error occurs.
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
    return write(str, &buf[sizeof(buf)] - str);
  }
  /** Print a file's size in bytes.
   * \param[in] pr Prtin stream for the output.
   * \return The number of bytes printed.
   */
  size_t printFileSize(print_t* pr);
  /** Print a file's modify date and time
   *
   * \param[in] pr Print stream for output.
   *
   * \return true for success or false for failure.
   */
  size_t printModifyDateTime(print_t* pr);
  /** Print a file's name
   *
   * \param[in] pr Print stream for output.
   *
   * \return length for success or zero for failure.
   */
  size_t printName(print_t* pr) {
#if USE_UTF8_LONG_NAMES
    return printName8(pr);
#else   // USE_UTF8_LONG_NAMES
    return printName7(pr);
#endif  // USE_UTF8_LONG_NAMES
  }
  /** Print a file's ASCII name
   *
   * \param[in] pr Print stream for output.
   *
   * \return true for success or false for failure.
   */
  size_t printName7(print_t* pr);
  /** Print a file's UTF-8 name
   *
   * \param[in] pr Print stream for output.
   *
   * \return true for success or false for failure.
   */
  size_t printName8(print_t* pr);
  /** Read the next byte from a file.
   *
   * \return For success read returns the next byte in the file as an int.
   * If an error occurs or end of file is reached -1 is returned.
   */
  int read() {
    uint8_t b;
    return read(&b, 1) == 1 ? b : -1;
  }
  /** Read data from a file starting at the current position.
   *
   * \param[out] buf Pointer to the location that will receive the data.
   *
   * \param[in] count Maximum number of bytes to read.
   *
   * \return For success read() returns the number of bytes read.
   * A value less than \a nbyte, including zero, will be returned
   * if end of file is reached.
   * If an error occurs, read() returns -1.
   */
  int read(void* buf, size_t count);
  /** Remove a file.
   *
   * The directory entry and all data for the file are deleted.
   *
   * \note This function should not be used to delete the 8.3 version of a
   * file that has a long name. For example if a file has the long name
   * "New Text Document.txt" you should not delete the 8.3 name "NEWTEX~1.TXT".
   *
   * \return true for success or false for failure.
   */
  bool remove();
  /** Remove a file.
   *
   * The directory entry and all data for the file are deleted.
   *
   * \param[in] path Path for the file to be removed.
   *
   * Example use: dirFile.remove(filenameToRemove);
   *
   * \note This function should not be used to delete the 8.3 version of a
   * file that has a long name. For example if a file has the long name
   * "New Text Document.txt" you should not delete the 8.3 name "NEWTEX~1.TXT".
   *
   * \return true for success or false for failure.
   */
  bool remove(const char* path);
  /** Rename a file or subdirectory.
   *
   * \param[in] newPath New path name for the file/directory.
   *
   * \return true for success or false for failure.
   */
  bool rename(const char* newPath);
  /** Rename a file or subdirectory.
   *
   * \param[in] dirFile Directory for the new path.
   * \param[in] newPath New path name for the file/directory.
   *
   * \return true for success or false for failure.
   */
  bool rename(ExFatFile* dirFile, const char* newPath);
  /** Set the file's current position to zero. */
  void rewind() { seekSet(0); }
  /** Remove a directory file.
   *
   * The directory file will be removed only if it is empty and is not the
   * root directory.  rmdir() follows DOS and Windows and ignores the
   * read-only attribute for the directory.
   *
   * \note This function should not be used to delete the 8.3 version of a
   * directory that has a long name. For example if a directory has the
   * long name "New folder" you should not delete the 8.3 name "NEWFOL~1".
   *
   * \return true for success or false for failure.
   */
  bool rmdir();
  /** Set the files position to current position + \a pos. See seekSet().
   * \param[in] offset The new position in bytes from the current position.
   * \return true for success or false for failure.
   */
  bool seekCur(int64_t offset) { return seekSet(m_curPosition + offset); }
  /** Set the files position to end-of-file + \a offset. See seekSet().
   * Can't be used for directory files since file size is not defined.
   * \param[in] offset The new position in bytes from end-of-file.
   * \return true for success or false for failure.
   */
  bool seekEnd(int64_t offset = 0) {
    return isFile() ? seekSet(m_validLength + offset) : false;
  }
  /** Sets a file's position.
   *
   * \param[in] pos The new position in bytes from the beginning of the file.
   *
   * \return true for success or false for failure.
   */
  bool seekSet(uint64_t pos);
  /** \return directory set count */
  uint8_t setCount() const { return m_setCount; }
  /** The sync() call causes all modified data and directory fields
   * to be written to the storage device.
   *
   * \return true for success or false for failure.
   */
  bool sync();
  /** Truncate a file at the current file position.
   *
   * \return true for success or false for failure.
   */
  /** Set a file's timestamps in its directory entry.
   *
   * \param[in] flags Values for \a flags are constructed by a
   * bitwise-inclusive OR of flags from the following list
   *
   * T_ACCESS - Set the file's last access date and time.
   *
   * T_CREATE - Set the file's creation date and time.
   *
   * T_WRITE - Set the file's last write/modification date and time.
   *
   * \param[in] year Valid range 1980 - 2107 inclusive.
   *
   * \param[in] month Valid range 1 - 12 inclusive.
   *
   * \param[in] day Valid range 1 - 31 inclusive.
   *
   * \param[in] hour Valid range 0 - 23 inclusive.
   *
   * \param[in] minute Valid range 0 - 59 inclusive.
   *
   * \param[in] second Valid range 0 - 59 inclusive
   *
   * \note It is possible to set an invalid date since there is no check for
   * the number of days in a month.
   *
   * \note
   * Modify and access timestamps may be overwritten if a date time callback
   * function has been set by dateTimeCallback().
   *
   * \return true for success or false for failure.
   */
  bool timestamp(uint8_t flags, uint16_t year, uint8_t month, uint8_t day,
                 uint8_t hour, uint8_t minute, uint8_t second);
  /** Truncate a file at the current file position.
   * will be maintained if it is less than or equal to \a length otherwise
   * it will be set to end of file.
   *
   * \return true for success or false for failure.
   */
  bool truncate();
  /** Truncate a file to a specified length.  The current file position
   * will be set to end of file.
   *
   * \param[in] length The desired length for the file.
   *
   * \return true for success or false for failure.
   */
  bool truncate(uint64_t length) { return seekSet(length) && truncate(); }

  /** \return The valid number of bytes in a file. */
  uint64_t validLength() const { return m_validLength; }
  /** Write a string to a file. Used by the Arduino Print class.
   * \param[in] str Pointer to the string.
   * Use getWriteError to check for errors.
   * \return count of characters written for success or -1 for failure.
   */
  size_t write(const char* str) { return write(str, strlen(str)); }
  /** Write a single byte.
   * \param[in] b The byte to be written.
   * \return +1 for success or zero for failure.
   */
  size_t write(uint8_t b) { return write(&b, 1); }
  /** Write data to an open file.
   *
   * \note Data is moved to the cache but may not be written to the
   * storage device until sync() is called.
   *
   * \param[in] buf Pointer to the location of the data to be written.
   *
   * \param[in] count Number of bytes to write.
   *
   * \return For success write() returns the number of bytes written, always
   * \a count. If an error occurs, write() returns zero and writeError is set.
   */
  size_t write(const void* buf, size_t count);
//------------------------------------------------------------------------------
#if ENABLE_ARDUINO_SERIAL
  /** List directory contents.
   *
   * \param[in] flags The inclusive OR of
   *
   * LS_DATE - %Print file modification date
   *
   * LS_SIZE - %Print file size.
   *
   * LS_R - Recursive list of subdirectories.
   *
   * \return true for success or false for failure.
   */
  bool ls(uint8_t flags = 0) { return ls(&Serial, flags); }
  /** Print a file's name.
   *
   * \return length for success or zero for failure.
   */
  size_t printName() { return ExFatFile::printName(&Serial); }
#endif  // ENABLE_ARDUINO_SERIAL

 private:
  /** ExFatVolume allowed access to private members. */
  friend class ExFatVolume;
  bool addCluster();
  bool addDirCluster();
  bool cmpName(const DirName_t* dirName, ExName_t* fname);
  uint8_t* dirCache(uint8_t set, uint8_t options);
  bool hashName(ExName_t* fname);
  bool mkdir(ExFatFile* parent, ExName_t* fname);

  bool openPrivate(ExFatFile* dir, ExName_t* fname, oflag_t oflag);
  bool parsePathName(const char* path, ExName_t* fname, const char** ptr);
  ExFatVolume* volume() const { return m_vol; }
  bool syncDir();
  //----------------------------------------------------------------------------
  static const uint8_t WRITE_ERROR = 0X1;
  static const uint8_t READ_ERROR = 0X2;

  /** This file has not been opened. */
  static const uint8_t FILE_ATTR_CLOSED = 0;
  /** Entry for normal data file */
  static const uint8_t FILE_ATTR_FILE = 0X08;
  /** Entry is for a subdirectory */
  static const uint8_t FILE_ATTR_SUBDIR = FS_ATTRIB_DIRECTORY;
  /** Root directory */
  static const uint8_t FILE_ATTR_ROOT = 0X40;
  /** Directory type bits */
  static const uint8_t FILE_ATTR_DIR = FILE_ATTR_SUBDIR | FILE_ATTR_ROOT;

  static const uint8_t FILE_FLAG_READ = 0X01;
  static const uint8_t FILE_FLAG_WRITE = 0X02;
  static const uint8_t FILE_FLAG_APPEND = 0X08;
  static const uint8_t FILE_FLAG_CONTIGUOUS = 0X40;
  static const uint8_t FILE_FLAG_DIR_DIRTY = 0X80;

  uint64_t m_curPosition;
  uint64_t m_dataLength;
  uint64_t m_validLength;
  uint32_t m_curCluster;
  uint32_t m_firstCluster;
  ExFatVolume* m_vol;
  DirPos_t m_dirPos;
  uint8_t m_setCount;
  uint8_t m_attributes = FILE_ATTR_CLOSED;
  uint8_t m_error = 0;
  uint8_t m_flags = 0;
};

#include "../common/ArduinoFiles.h"
/**
 * \class ExFile
 * \brief exFAT file with Arduino Stream.
 */
class ExFile : public StreamFile<ExFatFile, uint64_t> {
 public:
  /** Opens the next file or folder in a directory.
   *
   * \param[in] oflag open flags.
   * \return a FatStream object.
   */
  ExFile openNextFile(oflag_t oflag = O_RDONLY) {
    ExFile tmpFile;
    tmpFile.openNext(this, oflag);
    return tmpFile;
  }
};
#endif  // ExFatFile_h
