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
#ifndef FsFile_h
#define FsFile_h
/**
 * \file
 * \brief FsBaseFile include file.
 */
#include "ExFatLib/ExFatLib.h"
#include "FatLib/FatLib.h"
#include "FsNew.h"
#include "FsVolume.h"
/**
 * \class FsBaseFile
 * \brief FsBaseFile class.
 */
class FsBaseFile {
 public:
  /** Create an instance. */
  FsBaseFile() = default;
  /**  Create a file object and open it in the current working directory.
   *
   * \param[in] path A path for a file to be opened.
   *
   * \param[in] oflag Values for \a oflag are constructed by a bitwise-inclusive
   * OR of open flags. see FatFile::open(FatFile*, const char*, uint8_t).
   */
  FsBaseFile(const char* path, oflag_t oflag) { open(path, oflag); }

  /** Copy from to this.
   * \param[in] from Source file.
   */
  void copy(const FsBaseFile* from);

  /** move from to this.
   * \param[in] from Source file.
   */
  void move(FsBaseFile* from);

#if FILE_COPY_CONSTRUCTOR_SELECT == FILE_COPY_CONSTRUCTOR_PUBLIC
  /** Copy constructor.
   * \param[in] from Object used to initialize this instance.
   */
  FsBaseFile(const FsBaseFile& from);
  /** Copy assignment operator
   * \param[in] from Object used to initialize this instance.
   * \return assigned object.
   */
  FsBaseFile& operator=(const FsBaseFile& from);
#elif FILE_COPY_CONSTRUCTOR_SELECT == FILE_COPY_CONSTRUCTOR_PRIVATE

 private:
  FsBaseFile(const FsBaseFile& from);
  FsBaseFile& operator=(const FsBaseFile& from);

 public:
#else   // FILE_COPY_CONSTRUCTOR_SELECT
  FsBaseFile(const FsBaseFile& from) = delete;
  FsBaseFile& operator=(const FsBaseFile& from) = delete;
#endif  // FILE_COPY_CONSTRUCTOR_SELECT

#if FILE_MOVE_CONSTRUCTOR_SELECT
  /** Move constructor.
   * \param[in] from File to move.
   */
  FsBaseFile(FsBaseFile&& from) { move(&from); }
  /** Move assignment operator.
   * \param[in] from File to move.
   * \return Assigned file.
   */
  FsBaseFile& operator=(FsBaseFile&& from) {
    move(&from);
    return *this;
  }
#else   // FILE_MOVE_CONSTRUCTOR_SELECT
  FsBaseFile(FsBaseFile&& from) = delete;
  FsBaseFile& operator=(FsBaseFile&& from) = delete;
#endif  // FILE_MOVE_CONSTRUCTOR_SELECT

#if DESTRUCTOR_CLOSES_FILE
  ~FsBaseFile() {
    if (isOpen()) {
      close();
    }
  }
#else  // DESTRUCTOR_CLOSES_FILE
  ~FsBaseFile() = default;
#endif  // DESTRUCTOR_CLOSES_FILE

  /** The parenthesis operator.
   *
   * \return true if a file is open.
   */
  operator bool() const { return isOpen(); }
  /**
   * \return user settable file attributes for success else -1.
   */
  int attrib() {
    return m_fFile ? m_fFile->attrib() : m_xFile ? m_xFile->attrib() : -1;
  }
  /** Set file attributes
   *
   * \param[in] bits bit-wise or of selected attributes: FS_ATTRIB_READ_ONLY,
   *            FS_ATTRIB_HIDDEN, FS_ATTRIB_SYSTEM, FS_ATTRIB_ARCHIVE.
   *
   * \note attrib() will fail for set read-only if the file is open for write.
   * \return true for success or false for failure.
   */
  bool attrib(uint8_t bits) {
    return m_fFile   ? m_fFile->attrib(bits)
           : m_xFile ? m_xFile->attrib(bits)
                     : false;
  }
  /** \return number of bytes available from the current position to EOF
   *   or INT_MAX if more than INT_MAX bytes are available.
   */
  int available() const {
    return m_fFile ? m_fFile->available() : m_xFile ? m_xFile->available() : 0;
  }
  /** \return The number of bytes available from the current position
   * to EOF for normal files.  Zero is returned for directory files.
   */
  uint64_t available64() const {
    return m_fFile   ? m_fFile->available32()
           : m_xFile ? m_xFile->available64()
                     : 0;
  }
  /** Clear writeError. */
  void clearWriteError() {
    if (m_fFile) m_fFile->clearWriteError();
    if (m_xFile) m_xFile->clearWriteError();
  }
  /** Close a file and force cached data and directory information
   *  to be written to the storage device.
   *
   * \return true for success or false for failure.
   */
  bool close();
  /** Check for contiguous file and return its raw sector range.
   *
   * \param[out] bgnSector the first sector address for the file.
   * \param[out] endSector the last  sector address for the file.
   *
   * Set contiguous flag for FAT16/FAT32 files.
   * Parameters may be nullptr.
   *
   * \return true for success or false for failure.
   */
  bool contiguousRange(uint32_t* bgnSector, uint32_t* endSector) {
    return m_fFile   ? m_fFile->contiguousRange(bgnSector, endSector)
           : m_xFile ? m_xFile->contiguousRange(bgnSector, endSector)
                     : false;
  }
  /** \return The current cluster number for a file or directory. */
  uint32_t curCluster() const {
    return m_fFile   ? m_fFile->curCluster()
           : m_xFile ? m_xFile->curCluster()
                     : 0;
  }
  /** \return The current position for a file or directory. */
  uint64_t curPosition() const {
    return m_fFile   ? m_fFile->curPosition()
           : m_xFile ? m_xFile->curPosition()
                     : 0;
  }
  /** \return Total allocated length for file. */
  uint64_t dataLength() const {
    return m_fFile ? m_fFile->fileSize() : m_xFile ? m_xFile->dataLength() : 0;
  }
  /** \return Directory entry index. */
  uint32_t dirIndex() const {
    return m_fFile ? m_fFile->dirIndex() : m_xFile ? m_xFile->dirIndex() : 0;
  }
  /** Test for the existence of a file in a directory
   *
   * \param[in] path Path of the file to be tested for.
   *
   * The calling instance must be an open directory file.
   *
   * dirFile.exists("TOFIND.TXT") searches for "TOFIND.TXT" in  the directory
   * dirFile.
   *
   * \return true if the file exists else false.
   */
  bool exists(const char* path) {
    return m_fFile   ? m_fFile->exists(path)
           : m_xFile ? m_xFile->exists(path)
                     : false;
  }
  /** get position for streams
   * \param[out] pos struct to receive position
   */
  void fgetpos(fspos_t* pos) const {
    if (m_fFile) m_fFile->fgetpos(pos);
    if (m_xFile) m_xFile->fgetpos(pos);
  }
  /**
   * Get a string from a file.
   *
   * fgets() reads bytes from a file into the array pointed to by \a str, until
   * \a num - 1 bytes are read, or a delimiter is read and transferred to \a
   * str, or end-of-file is encountered. The string is then terminated with a
   * null byte.
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
  int fgets(char* str, int num, char* delim = nullptr) {
    return m_fFile   ? m_fFile->fgets(str, num, delim)
           : m_xFile ? m_xFile->fgets(str, num, delim)
                     : -1;
  }
  /** \return The total number of bytes in a file. */
  uint64_t fileSize() const {
    return m_fFile ? m_fFile->fileSize() : m_xFile ? m_xFile->fileSize() : 0;
  }
  /** \return Address of first sector or zero for empty file. */
  uint32_t firstSector() const {
    return m_fFile   ? m_fFile->firstSector()
           : m_xFile ? m_xFile->firstSector()
                     : 0;
  }
  /** Ensure that any bytes written to the file are saved to the SD card. */
  void flush() { sync(); }
  /** set position for streams
   * \param[in] pos struct with value for new position
   */
  void fsetpos(const fspos_t* pos) {
    if (m_fFile) m_fFile->fsetpos(pos);
    if (m_xFile) m_xFile->fsetpos(pos);
  }
  /** Get a file's access date and time.
   *
   * \param[out] pdate Packed date for directory entry.
   * \param[out] ptime Packed time for directory entry.
   *
   * \return true for success or false for failure.
   */
  bool getAccessDateTime(uint16_t* pdate, uint16_t* ptime) {
    return m_fFile   ? m_fFile->getAccessDateTime(pdate, ptime)
           : m_xFile ? m_xFile->getAccessDateTime(pdate, ptime)
                     : false;
  }
  /** Get a file's create date and time.
   *
   * \param[out] pdate Packed date for directory entry.
   * \param[out] ptime Packed time for directory entry.
   *
   * \return true for success or false for failure.
   */
  bool getCreateDateTime(uint16_t* pdate, uint16_t* ptime) {
    return m_fFile   ? m_fFile->getCreateDateTime(pdate, ptime)
           : m_xFile ? m_xFile->getCreateDateTime(pdate, ptime)
                     : false;
  }
  /** \return All error bits. */
  uint8_t getError() const {
    return m_fFile ? m_fFile->getError() : m_xFile ? m_xFile->getError() : 0XFF;
  }
  /** Get a file's Modify date and time.
   *
   * \param[out] pdate Packed date for directory entry.
   * \param[out] ptime Packed time for directory entry.
   *
   * \return true for success or false for failure.
   */
  bool getModifyDateTime(uint16_t* pdate, uint16_t* ptime) {
    return m_fFile   ? m_fFile->getModifyDateTime(pdate, ptime)
           : m_xFile ? m_xFile->getModifyDateTime(pdate, ptime)
                     : false;
  }
  /**
   * Get a file's name followed by a zero byte.
   *
   * \param[out] name An array of characters for the file's name.
   * \param[in] len The size of the array in bytes. The array
   *             must be at least 13 bytes long.  The file's name will be
   *             truncated if the file's name is too long.
   * \return The length of the returned string.
   */
  size_t getName(char* name, size_t len) {
    *name = 0;
    return m_fFile   ? m_fFile->getName(name, len)
           : m_xFile ? m_xFile->getName(name, len)
                     : 0;
  }

  /** \return value of writeError */
  bool getWriteError() const {
    return m_fFile   ? m_fFile->getWriteError()
           : m_xFile ? m_xFile->getWriteError()
                     : true;
  }
  /**
   * Check for FsBlockDevice busy.
   *
   * \return true if busy else false.
   */
  bool isBusy() {
    return m_fFile ? m_fFile->isBusy() : m_xFile ? m_xFile->isBusy() : true;
  }
  /** \return True if the file is contiguous. */
  bool isContiguous() const {
#if USE_FAT_FILE_FLAG_CONTIGUOUS
    return m_fFile   ? m_fFile->isContiguous()
           : m_xFile ? m_xFile->isContiguous()
                     : false;
#else   // USE_FAT_FILE_FLAG_CONTIGUOUS
    return m_xFile ? m_xFile->isContiguous() : false;
#endif  // USE_FAT_FILE_FLAG_CONTIGUOUS
  }
  /** \return True if this is a directory else false. */
  bool isDir() const {
    return m_fFile ? m_fFile->isDir() : m_xFile ? m_xFile->isDir() : false;
  }
  /** This function reports if the current file is a directory or not.
   * \return true if the file is a directory.
   */
  bool isDirectory() const { return isDir(); }
  /** \return True if this is a normal file. */
  bool isFile() const {
    return m_fFile ? m_fFile->isFile() : m_xFile ? m_xFile->isFile() : false;
  }
  /** \return True if this is a normal file or sub-directory. */
  bool isFileOrSubDir() const {
    return m_fFile   ? m_fFile->isFileOrSubDir()
           : m_xFile ? m_xFile->isFileOrSubDir()
                     : false;
  }
  /** \return True if this is a hidden file else false. */
  bool isHidden() const {
    return m_fFile   ? m_fFile->isHidden()
           : m_xFile ? m_xFile->isHidden()
                     : false;
  }
  /** \return True if this is an open file/directory else false. */
  bool isOpen() const { return m_fFile || m_xFile; }
  /** \return True file is readable. */
  bool isReadable() const {
    return m_fFile   ? m_fFile->isReadable()
           : m_xFile ? m_xFile->isReadable()
                     : false;
  }
  /** \return True if file is read-only */
  bool isReadOnly() const {
    return m_fFile   ? m_fFile->isReadOnly()
           : m_xFile ? m_xFile->isReadOnly()
                     : false;
  }
  /** \return True if this is a sub-directory file else false. */
  bool isSubDir() const {
    return m_fFile   ? m_fFile->isSubDir()
           : m_xFile ? m_xFile->isSubDir()
                     : false;
  }
  /** \return True file is writable. */
  bool isWritable() const {
    return m_fFile   ? m_fFile->isWritable()
           : m_xFile ? m_xFile->isWritable()
                     : false;
  }
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
   * \return true for success or false for failure.
   */
  bool ls(uint8_t flags) { return ls(&Serial, flags); }
  /** List directory contents.
   * \return true for success or false for failure.
   */
  bool ls() { return ls(&Serial); }
#endif  // ENABLE_ARDUINO_SERIAL
  /** List directory contents.
   *
   * \param[in] pr Print object.
   *
   * \return true for success or false for failure.
   */
  bool ls(print_t* pr) {
    return m_fFile ? m_fFile->ls(pr) : m_xFile ? m_xFile->ls(pr) : false;
  }
  /** List directory contents.
   *
   * \param[in] pr Print object.
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
  bool ls(print_t* pr, uint8_t flags) {
    return m_fFile   ? m_fFile->ls(pr, flags)
           : m_xFile ? m_xFile->ls(pr, flags)
                     : false;
  }
  /** Make a new directory.
   *
   * \param[in] dir An open FatFile instance for the directory that will
   *                   contain the new directory.
   *
   * \param[in] path A path with a valid 8.3 DOS name for the new directory.
   *
   * \param[in] pFlag Create missing parent directories if true.
   *
   * \return true for success or false for failure.
   */
  bool mkdir(FsBaseFile* dir, const char* path, bool pFlag = true);
  /** Open a file or directory by name.
   *
   * \param[in] dir An open file instance for the directory containing
   *                    the file to be opened.
   *
   * \param[in] path A path with a valid 8.3 DOS name for a file to be opened.
   *
   * \param[in] oflag Values for \a oflag are constructed by a
   *                  bitwise-inclusive OR of flags from the following list
   *
   * O_RDONLY - Open for reading only..
   *
   * O_READ - Same as O_RDONLY.
   *
   * O_WRONLY - Open for writing only.
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
   * successfully opened and is not read only, its length shall be truncated to
   * 0.
   *
   * WARNING: A given file must not be opened by more than one file object
   * or file corruption may occur.
   *
   * \note Directory files must be opened read only.  Write and truncation is
   * not allowed for directory files.
   *
   * \return true for success or false for failure.
   */
  bool open(FsBaseFile* dir, const char* path, oflag_t oflag = O_RDONLY);
  /** Open a file by index.
   *
   * \param[in] dir An open FsFile instance for the directory.
   *
   * \param[in] index The \a index of the directory entry for the file to be
   * opened.  The value for \a index is (directory file position)/32.
   *
   * \param[in] oflag bitwise-inclusive OR of open flags.
   *            See see FsFile::open(FsFile*, const char*, uint8_t).
   *
   * See open() by path for definition of flags.
   * \return true for success or false for failure.
   */
  bool open(FsBaseFile* dir, uint32_t index, oflag_t oflag = O_RDONLY);
  /** Open a file or directory by name.
   *
   * \param[in] vol Volume where the file is located.
   *
   * \param[in] path A path for a file to be opened.
   *
   * \param[in] oflag Values for \a oflag are constructed by a
   *                  bitwise-inclusive OR of open flags.
   *
   * \return true for success or false for failure.
   */
  bool open(FsVolume* vol, const char* path, oflag_t oflag = O_RDONLY);
  /** Open a file or directory by name.
   *
   * \param[in] path A path for a file to be opened.
   *
   * \param[in] oflag Values for \a oflag are constructed by a
   *                  bitwise-inclusive OR of open flags.
   *
   * \return true for success or false for failure.
   */
  bool open(const char* path, oflag_t oflag = O_RDONLY) {
    return FsVolume::m_cwv && open(FsVolume::m_cwv, path, oflag);
  }
  /** Open a file or directory by index in the current working directory.
   *
   * \param[in] index The \a index of the directory entry for the file to be
   * opened.  The value for \a index is (directory file position)/32.
   *
   * \param[in] oflag Values for \a oflag are constructed by a
   *                  bitwise-inclusive OR of open flags.
   *
   * \return true for success or false for failure.
   */
  bool open(uint32_t index, oflag_t oflag = O_RDONLY) {
    FsBaseFile cwd;
    return cwd.openCwd() && open(&cwd, index, oflag);
  }
  /** Open the current working directory.
   *
   * \return true for success or false for failure.
   */
  bool openCwd();
  /** Opens the next file or folder in a directory.
   * \param[in] dir directory containing files.
   * \param[in] oflag open flags.
   * \return a file object.
   */
  bool openNext(FsBaseFile* dir, oflag_t oflag = O_RDONLY);
  /** Open a volume's root directory.
   *
   * \param[in] vol The SdFs volume containing the root directory to be opened.
   *
   * \return true for success or false for failure.
   */
  bool openRoot(FsVolume* vol);
  /** \return the current file position. */
  uint64_t position() const { return curPosition(); }
  /** Return the next available byte without consuming it.
   *
   * \return The byte if no error and not at eof else -1;
   */
  int peek() {
    return m_fFile ? m_fFile->peek() : m_xFile ? m_xFile->peek() : -1;
  }
  /** Allocate contiguous clusters to an empty file.
   *
   * The file must be empty with no clusters allocated.
   *
   * The file will contain uninitialized data for FAT16/FAT32 files.
   * exFAT files will have zero validLength and dataLength will equal
   * the requested length.
   *
   * \param[in] length size of the file in bytes.
   * \return true for success or false for failure.
   */
  bool preAllocate(uint64_t length) {
    return m_fFile   ? length < (1ULL << 32) && m_fFile->preAllocate(length)
           : m_xFile ? m_xFile->preAllocate(length)
                     : false;
  }
  /** Print a file's access date and time
   *
   * \param[in] pr Print stream for output.
   *
   * \return true for success or false for failure.
   */
  size_t printAccessDateTime(print_t* pr) {
    return m_fFile   ? m_fFile->printAccessDateTime(pr)
           : m_xFile ? m_xFile->printAccessDateTime(pr)
                     : 0;
  }
  /** Print a file's creation date and time
   *
   * \param[in] pr Print stream for output.
   *
   * \return true for success or false for failure.
   */
  size_t printCreateDateTime(print_t* pr) {
    return m_fFile   ? m_fFile->printCreateDateTime(pr)
           : m_xFile ? m_xFile->printCreateDateTime(pr)
                     : 0;
  }
  /** Print a number followed by a field terminator.
   * \param[in] value The number to be printed.
   * \param[in] term The field terminator.  Use '\\n' for CR LF.
   * \param[in] prec Number of digits after decimal point.
   * \return The number of bytes written or -1 if an error occurs.
   */
  size_t printField(double value, char term, uint8_t prec = 2) {
    return m_fFile   ? m_fFile->printField(value, term, prec)
           : m_xFile ? m_xFile->printField(value, term, prec)
                     : 0;
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
    return m_fFile   ? m_fFile->printField(value, term)
           : m_xFile ? m_xFile->printField(value, term)
                     : 0;
  }
  /** Print a file's size.
   *
   * \param[in] pr Print stream for output.
   *
   * \return The number of characters printed is returned
   *         for success and zero is returned for failure.
   */
  size_t printFileSize(print_t* pr) {
    return m_fFile   ? m_fFile->printFileSize(pr)
           : m_xFile ? m_xFile->printFileSize(pr)
                     : 0;
  }
  /** Print a file's modify date and time
   *
   * \param[in] pr Print stream for output.
   *
   * \return true for success or false for failure.
   */
  size_t printModifyDateTime(print_t* pr) {
    return m_fFile   ? m_fFile->printModifyDateTime(pr)
           : m_xFile ? m_xFile->printModifyDateTime(pr)
                     : 0;
  }
  /** Print a file's name
   *
   * \param[in] pr Print stream for output.
   *
   * \return true for success or false for failure.
   */
  size_t printName(print_t* pr) {
    return m_fFile   ? m_fFile->printName(pr)
           : m_xFile ? m_xFile->printName(pr)
                     : 0;
  }
  /** Read the next byte from a file.
   *
   * \return For success return the next byte in the file as an int.
   * If an error occurs or end of file is reached return -1.
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
   * A value less than \a count, including zero, will be returned
   * if end of file is reached.
   * If an error occurs, read() returns -1.  Possible errors include
   * read() called before a file has been opened, corrupt file system
   * or an I/O error occurred.
   */
  int read(void* buf, size_t count) {
    return m_fFile   ? m_fFile->read(buf, count)
           : m_xFile ? m_xFile->read(buf, count)
                     : -1;
  }
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
  bool remove(const char* path) {
    return m_fFile   ? m_fFile->remove(path)
           : m_xFile ? m_xFile->remove(path)
                     : false;
  }
  /** Rename a file or subdirectory.
   *
   * \param[in] newPath New path name for the file/directory.
   *
   * \return true for success or false for failure.
   */
  bool rename(const char* newPath) {
    return m_fFile   ? m_fFile->rename(newPath)
           : m_xFile ? m_xFile->rename(newPath)
                     : false;
  }
  /** Rename a file or subdirectory.
   *
   * \param[in] dir Directory for the new path.
   * \param[in] newPath New path name for the file/directory.
   *
   * \return true for success or false for failure.
   */
  bool rename(FsBaseFile* dir, const char* newPath) {
    return m_fFile && dir->m_fFile   ? m_fFile->rename(dir->m_fFile, newPath)
           : m_xFile && dir->m_xFile ? m_xFile->rename(dir->m_xFile, newPath)
                                     : false;
  }
  /** Set the file's current position to zero. */
  void rewind() {
    if (m_fFile) m_fFile->rewind();
    if (m_xFile) m_xFile->rewind();
  }
  /** Rewind a file if it is a directory */
  void rewindDirectory() {
    if (isDir()) rewind();
  }
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
  /** Seek to a new position in the file, which must be between
   * 0 and the size of the file (inclusive).
   *
   * \param[in] pos the new file position.
   * \return true for success or false for failure.
   */
  bool seek(uint64_t pos) { return seekSet(pos); }
  /** Set the files position to current position + \a pos. See seekSet().
   * \param[in] offset The new position in bytes from the current position.
   * \return true for success or false for failure.
   */
  bool seekCur(int64_t offset) { return seekSet(curPosition() + offset); }
  /** Set the files position to end-of-file + \a offset. See seekSet().
   * Can't be used for directory files since file size is not defined.
   * \param[in] offset The new position in bytes from end-of-file.
   * \return true for success or false for failure.
   */
  bool seekEnd(int64_t offset = 0) { return seekSet(fileSize() + offset); }
  /** Sets a file's position.
   *
   * \param[in] pos The new position in bytes from the beginning of the file.
   *
   * \return true for success or false for failure.
   */
  bool seekSet(uint64_t pos) {
    return m_fFile   ? pos < (1ULL << 32) && m_fFile->seekSet((uint32_t)pos)
           : m_xFile ? m_xFile->seekSet(pos)
                     : false;
  }
  /** \return the file's size. */
  uint64_t size() const { return fileSize(); }
  /** The sync() call causes all modified data and directory fields
   * to be written to the storage device.
   *
   * \return true for success or false for failure.
   */
  bool sync() {
    return m_fFile ? m_fFile->sync() : m_xFile ? m_xFile->sync() : false;
  }
  /** Set a file's timestamps in its directory entry.
   *
   * \param[in] flags Values for \a flags are constructed by a bitwise-inclusive
   * OR of flags from the following list
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
                 uint8_t hour, uint8_t minute, uint8_t second) {
    return m_fFile   ? m_fFile->timestamp(flags, year, month, day, hour, minute,
                                          second)
           : m_xFile ? m_xFile->timestamp(flags, year, month, day, hour, minute,
                                          second)
                     : false;
  }
  /** Truncate a file to the current position.
   *
   * \return true for success or false for failure.
   */
  bool truncate() {
    return m_fFile   ? m_fFile->truncate()
           : m_xFile ? m_xFile->truncate()
                     : false;
  }
  /** Truncate a file to a specified length.
   * The current file position will be set to end of file.
   *
   * \param[in] length The desired length for the file.
   *
   * \return true for success or false for failure.
   */
  bool truncate(uint64_t length) {
    return m_fFile   ? length < (1ULL << 32) && m_fFile->truncate(length)
           : m_xFile ? m_xFile->truncate(length)
                     : false;
  }
  /** Write a string to a file. Used by the Arduino Print class.
   * \param[in] str Pointer to the string.
   * Use getWriteError to check for errors.
   * \return count of characters written for success or -1 for failure.
   */
  size_t write(const char* str) { return write(str, strlen(str)); }
  /** Write a byte to a file. Required by the Arduino Print class.
   * \param[in] b the byte to be written.
   * Use getWriteError to check for errors.
   * \return 1 for success and 0 for failure.
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
   * \a nbyte.  If an error occurs, write() returns zero and writeError is set.
   */
  size_t write(const void* buf, size_t count) {
    return m_fFile   ? m_fFile->write(buf, count)
           : m_xFile ? m_xFile->write(buf, count)
                     : 0;
  }

 private:
  newalign_t m_fileMem[FS_ALIGN_DIM(ExFatFile, FatFile)];
  FatFile* m_fFile = nullptr;
  ExFatFile* m_xFile = nullptr;
};
/**
 * \class FsFile
 * \brief FsBaseFile file with Arduino Stream.
 */
class FsFile : public StreamFile<FsBaseFile, uint64_t> {
 public:
  /** Opens the next file or folder in a directory.
   *
   * \param[in] oflag open flags.
   * \return a FatStream object.
   */
  FsFile openNextFile(oflag_t oflag = O_RDONLY) {
    FsFile tmpFile;
    tmpFile.openNext(this, oflag);
    return tmpFile;
  }
};
#endif  // FsFile_h
