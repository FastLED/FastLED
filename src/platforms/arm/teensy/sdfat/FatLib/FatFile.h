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
#ifndef FatFile_h
#define FatFile_h
/**
 * \file
 * \brief FatFile class
 */
#include "fl/stl/cstddef.h"
#include "fl/stl/cstring.h"
#include "fl/stl/int.h"
#include "fl/stl/limits.h"
// IWYU pragma: begin_keep
#include "../common/ArduinoFiles.h"  // ok include path; // ok relative include - imported SdFat layout.
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "../common/FmtNumber.h"  // ok include path; // ok relative include - imported SdFat layout.
#include "../common/FsApiConstants.h"  // ok include path; // ok relative include - imported SdFat layout.
#include "../common/FsDateTime.h"  // ok include path; // ok relative include - imported SdFat layout.
#include "../common/FsName.h"  // ok include path; // ok relative include - imported SdFat layout.
#include "FatPartition.h"  // ok include path; // ok relative include - imported SdFat layout.
// IWYU pragma: end_keep
//------------------------------------------------------------------------------
// Stuff to store strings in AVR flash.
#ifdef __AVR__
// IWYU pragma: begin_keep
#include <avr/pgmspace.h>
// IWYU pragma: end_keep
#else  // __AVR__
#ifndef PSTR
/** store literal string in flash for ARM */
#define PSTR(x) (x)
#endif  // PSTR
#ifndef pgm_read_byte
/** read 8-bits from flash for ARM */
#define pgm_read_byte(addr) (*(const unsigned char*)(addr))
#endif  // pgm_read_byte
#ifndef pgm_read_word
/** read 16-bits from flash for ARM */
#define pgm_read_word(addr) (*(const fl::u16*)(addr))
#endif  // pgm_read_word
#ifndef PROGMEM
/** store in flash for ARM */
#define PROGMEM
#endif  // PROGMEM
#endif  // __AVR__

namespace fl { namespace platforms { namespace teensy { namespace sdfat {

class FatVolume;
//------------------------------------------------------------------------------
/**
 * \struct FatPos_t
 * \brief Internal type for file position - do not use in user apps.
 */
struct FatPos_t {
  /** stream position */
  fl::u32 position;
  /** cluster for position */
  fl::u32 cluster;
};
//------------------------------------------------------------------------------
/** Expression for path name separator. */
#define isDirSeparator(c) ((c) == '/')
//------------------------------------------------------------------------------
/**
 * \class FatLfn_t
 * \brief Internal type for Long File Name - do not use in user apps.
 */

class FatLfn_t : public FsName {
 public:
  /** UTF-16 length of Long File Name */
  fl::size len;
  /** Position for sequence number. */
  fl::u8 seqPos;
  /** Flags for base and extension character case and LFN. */
  fl::u8 flags;
  /** Short File Name */
  fl::u8 sfn[11];
};
/**
 * \class FatSfn_t
 * \brief Internal type for Short 8.3 File Name - do not use in user apps.
 */
class FatSfn_t {
 public:
  /** Flags for base and extension character case and LFN. */
  fl::u8 flags;
  /** Short File Name */
  fl::u8 sfn[11];
};

#if USE_LONG_FILE_NAMES
/** Internal class for file names */
typedef FatLfn_t FatName_t;
#else  // USE_LONG_FILE_NAMES
/** Internal class for file names */
typedef FatSfn_t FatName_t;
#endif  // USE_LONG_FILE_NAMES

/** Derived from a LFN with loss or conversion of characters. */
const fl::u8 FNAME_FLAG_LOST_CHARS = 0X01;
/** Base-name or extension has mixed case. */
const fl::u8 FNAME_FLAG_MIXED_CASE = 0X02;
/** LFN entries are required for file name. */
const fl::u8 FNAME_FLAG_NEED_LFN =
  FNAME_FLAG_LOST_CHARS | FNAME_FLAG_MIXED_CASE;
/** Filename base-name is all lower case */
const fl::u8 FNAME_FLAG_LC_BASE = FAT_CASE_LC_BASE;
/** Filename extension is all lower case. */
const fl::u8 FNAME_FLAG_LC_EXT = FAT_CASE_LC_EXT;
#if FNAME_FLAG_NEED_LFN & (FAT_CASE_LC_BASE || FAT_CASE_LC_EXT)
#error FNAME_FLAG_NEED_LFN & (FAT_CASE_LC_BASE || FAT_CASE_LC_EXT)
#endif  // FNAME_FLAG_NEED_LFN & (FAT_CASE_LC_BASE || FAT_CASE_LC_EXT)
//==============================================================================
/**
 * \class FatFile
 * \brief Basic file class.
 */
class FatFile {
 public:
  /** Create an instance. */
  FatFile() {}
  /**  Create a file object and open it in the current working directory.
   *
   * \param[in] path A path for a file to be opened.
   *
   * \param[in] oflag Values for \a oflag are constructed by a bitwise-inclusive
   * OR of open flags. see FatFile::open(FatFile*, const char*, uint8_t).
   */
  FatFile(const char* path, oflag_t oflag) {
    open(path, oflag);
  }
#if DESTRUCTOR_CLOSES_FILE
  /** Destructor */
  ~FatFile() {
    if (isOpen()) {
      close();
    }
  }
#endif  // DESTRUCTOR_CLOSES_FILE
  /** The parenthesis operator.
   *
   * \return true if a file is open.
   */
  operator bool() const {return isOpen();}
  /** \return The number of bytes available from the current position
   * to EOF for normal files.  INT_MAX is returned for very large files.
   *
   * available32() is recomended for very large files.
   *
   * Zero is returned for directory files.
   *
   */
  int available() const {
    fl::u32 n = available32();
    return n > (fl::numeric_limits<int>::max)() ? (fl::numeric_limits<int>::max)() : n;
  }
  /** \return The number of bytes available from the current position
   * to EOF for normal files.  Zero is returned for directory files.
   */
  fl::u32 available32() const {
    return isFile() ? fileSize() - curPosition() : 0;
  }
  /** Clear all error bits. */
  void clearError() {
    mError = 0;
  }
  /** Set writeError to zero */
  void clearWriteError() {
    mError &= ~WRITE_ERROR;
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
   * \param[out] endSector the last sector address for the file.
   *
   * Set the contiguous flag if the file is contiguous.
   * The parameters may be nullptr to only set the flag.
   * \return true for success or false for failure.
   */
  bool contiguousRange(fl::u32* bgnSector, fl::u32* endSector);
  /** Create and open a new contiguous file of a specified size.
   *
   * \param[in] dirFile The directory where the file will be created.
   * \param[in] path A path with a valid file name.
   * \param[in] size The desired file size.
   *
   * \return true for success or false for failure.
   */
  bool createContiguous(FatFile* dirFile,
                        const char* path, fl::u32 size);
  /** Create and open a new contiguous file of a specified size.
   *
   * \param[in] path A path with a valid file name.
   * \param[in] size The desired file size.
   *
   * \return true for success or false for failure.
   */
  bool createContiguous(const char* path, fl::u32 size);
  /** \return The current cluster number for a file or directory. */
  fl::u32 curCluster() const {return mCurCluster;}

  /** \return The current position for a file or directory. */
  fl::u32 curPosition() const {return mCurPosition;}
  /** Return a file's directory entry.
   *
   * \param[out] dir Location for return of the file's directory entry.
   *
   * \return true for success or false for failure.
   */
  bool dirEntry(DirFat_t* dir);
  /** \return Directory entry index. */
  fl::u16 dirIndex() const {return mDirIndex;}
  /** \return The number of bytes allocated to a directory or zero
   *         if an error occurs.
   */
  fl::u32 dirSize();
  /** Dump file in Hex
   * \param[in] pr Print stream for list.
   * \param[in] pos Start position in file.
   * \param[in] n number of locations to dump.
   */
  void dmpFile(print_t* pr, fl::u32 pos, fl::size n);
  /** Test for the existence of a file in a directory
   *
   * \param[in] path Path of the file to be tested for.
   *
   * The calling instance must be an open directory file.
   *
   * dirFile.exists("TOFIND.TXT") searches for "TOFIND.TXT" in the directory
   * dirFile.
   *
   * \return True if the file exists.
   */
  bool exists(const char* path) {
    FatFile file;
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
  fl::u32 fileSize() const {return mFileSize;}
  /** \return first sector of file or zero for empty file. */
  fl::u32 firstBlock() const {return firstSector();}
  /** \return Address of first sector or zero for empty file. */
  fl::u32 firstSector() const;
  /** Arduino name for sync() */
  void flush() {sync();}
  /** set position for streams
   * \param[in] pos struct with value for new position
   */
  void fsetpos(const fspos_t* pos);
  /** Get a file's access date.
   *
   * \param[out] pdate Packed date for directory entry.
   *
   * \return true for success or false for failure.
   */
  bool getAccessDate(fl::u16* pdate);
  /** Get a file's access date and time.
   *
   * \param[out] pdate Packed date for directory entry.
   * \param[out] ptime return zero since FAT has no time.
   *
   * This function is for comparability in FsFile.
   *
   * \return true for success or false for failure.
   */
  bool getAccessDateTime(fl::u16* pdate, fl::u16* ptime) {
    if (!getAccessDate(pdate)) {
      return false;
    }
    *ptime = 0;
    return true;
  }
  /** Get a file's create date and time.
   *
   * \param[out] pdate Packed date for directory entry.
   * \param[out] ptime Packed time for directory entry.
   *
   * \return true for success or false for failure.
   */
  bool getCreateDateTime(fl::u16* pdate, fl::u16* ptime);
  /** \return All error bits. */
  fl::u8 getError() const {return mError;}
  /** Get a file's modify date and time.
   *
   * \param[out] pdate Packed date for directory entry.
   * \param[out] ptime Packed time for directory entry.
   *
   * \return true for success or false for failure.
   */
  bool getModifyDateTime(fl::u16* pdate, fl::u16* ptime);
  /**
   * Get a file's name followed by a zero byte.
   *
   * \param[out] name An array of characters for the file's name.
   * \param[in] size The size of the array in bytes. The array
   *             must be at least 13 bytes long.  The file's name will be
   *             truncated if the file's name is too long.
   * \return length for success or zero for failure.
   */
  fl::size getName(char* name, fl::size size);
  /**
   * Get a file's ASCII name followed by a zero.
   *
   * \param[out] name An array of characters for the file's name.
   * \param[in] size The size of the array in characters.
   * \return the name length.
   */
  fl::size getName7(char* name, fl::size size);
  /**
   * Get a file's UTF-8 name followed by a zero.
   *
   * \param[out] name An array of characters for the file's name.
   * \param[in] size The size of the array in characters.
   * \return the name length.
   */
  fl::size getName8(char* name, fl::size size);
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  fl::size __attribute__((error("use getSFN(name, size)"))) getSFN(char* name);
#endif  // DOXYGEN_SHOULD_SKIP_THIS
  /**
   * Get a file's Short File Name followed by a zero byte.
   *
   * \param[out] name An array of characters for the file's name.
   *                  The array should be at least 13 bytes long.
   * \param[in] size size of name array.
   * \return true for success or false for failure.
   */
  fl::size getSFN(char* name, fl::size size);
  /** \return value of writeError */
  bool getWriteError() const {
    return isOpen() ? mError & WRITE_ERROR : true;
  }
  /**
   * Check for device busy.
   *
   * \return true if busy else false.
   */
  bool isBusy();
#if USE_FAT_FILE_FLAG_CONTIGUOUS
    /** \return True if the file is contiguous. */
  bool isContiguous() const {return mFlags & FILE_FLAG_CONTIGUOUS;}
#endif  // USE_FAT_FILE_FLAG_CONTIGUOUS
  /** \return True if this is a directory. */
  bool isDir() const {return mAttributes & FILE_ATTR_DIR;}
  /** \return True if this is a normal file. */
  bool isFile() const {return mAttributes & FILE_ATTR_FILE;}
  /** \return True if this is a hidden file. */
  bool isHidden() const {return mAttributes & FILE_ATTR_HIDDEN;}
  /** \return true if this file has a Long File Name. */
  bool isLFN() const {return mLfnOrd;}
  /** \return True if this is an open file/directory. */
  bool isOpen() const {return mAttributes;}
  /** \return True file is readable. */
  bool isReadable() const {return mFlags & FILE_FLAG_READ;}
  /** \return True if file is read-only */
  bool isReadOnly() const {return mAttributes & FILE_ATTR_READ_ONLY;}
  /** \return True if this is the root directory. */
  bool isRoot() const {return mAttributes & FILE_ATTR_ROOT;}
  /** \return True if this is the FAT32 root directory. */
  bool isRoot32() const {return mAttributes & FILE_ATTR_ROOT32;}
  /** \return True if this is the FAT12 of FAT16 root directory. */
  bool isRootFixed() const {return mAttributes & FILE_ATTR_ROOT_FIXED;}
  /** \return True if this is a subdirectory. */
  bool isSubDir() const {return mAttributes & FILE_ATTR_SUBDIR;}
  /** \return True if this is a system file. */
  bool isSystem() const {return mAttributes & FILE_ATTR_SYSTEM;}
  /** \return True file is writable. */
  bool isWritable() const {return mFlags & FILE_FLAG_WRITE;}
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
   * LS_R - Recursive list of subdirectories.
   *
   * \param[in] indent Amount of space before file name. Used for recursive
   * list to indicate subdirectory level.
   *
   * \return true for success or false for failure.
   */
  bool ls(print_t* pr, fl::u8 flags = 0, fl::u8 indent = 0);
  /** Make a new directory.
   *
   * \param[in] dir An open FatFile instance for the directory that will
   *                   contain the new directory.
   *
   * \param[in] path A path with a valid name for the new directory.
   *
   * \param[in] pFlag Create missing parent directories if true.
   *
   * \return true for success or false for failure.
   */
  bool mkdir(FatFile* dir, const char* path, bool pFlag = true);
  /** Open a file in the volume root directory.
   *
   * \param[in] vol Volume where the file is located.
   *
   * \param[in] path with a valid name for a file to be opened.
   *
   * \param[in] oflag bitwise-inclusive OR of open flags.
   *                  See see FatFile::open(FatFile*, const char*, uint8_t).
   *
   * \return true for success or false for failure.
   */
  bool open(FatVolume* vol, const char* path, oflag_t oflag);
  /** Open a file by index.
   *
   * \param[in] dirFile An open FatFile instance for the directory.
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
  bool open(FatFile* dirFile, fl::u16 index, oflag_t oflag);
  /** Open a file or directory by name.
   *
   * \param[in] dirFile An open FatFile instance for the directory containing
   *                    the file to be opened.
   *
   * \param[in] path A path with a valid name for a file to be opened.
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
   * WARNING: A given file must not be opened by more than one FatFile object
   * or file corruption may occur.
   *
   * \note Directory files must be opened read only.  Write and truncation is
   * not allowed for directory files.
   *
   * \return true for success or false for failure.
   */
  bool open(FatFile* dirFile, const char* path, oflag_t oflag);
  /** Open a file in the current working volume.
   *
   * \param[in] path A path with a valid name for a file to be opened.
   *
   * \param[in] oflag bitwise-inclusive OR of open flags.
   *                  See see FatFile::open(FatFile*, const char*, uint8_t).
   *
   * \return true for success or false for failure.
   */
  bool open(const char* path, oflag_t oflag = O_RDONLY);
  /** Open existing file wih Short 8.3 names.
   * \param[in] path with short 8.3 names.
   *
   * the purpose of this function is to save flash on Uno
   * and other small boards.
   *
   * Directories will be opened O_RDONLY, files O_RDWR.
   * \return true for success or false for failure.
   */
  bool openExistingSFN(const char* path);
  /** Open the next file or subdirectory in a directory.
   *
   * \param[in] dirFile An open FatFile instance for the directory
   *                    containing the file to be opened.
   *
   * \param[in] oflag bitwise-inclusive OR of open flags.
   *                  See see FatFile::open(FatFile*, const char*, uint8_t).
   *
   * \return true for success or false for failure.
   */
  bool openNext(FatFile* dirFile, oflag_t oflag = O_RDONLY);
  /** Open a volume's root directory.
   *
   * \param[in] vol The FAT volume containing the root directory to be opened.
   *
   * \return true for success or false for failure.
   */
  bool openRoot(FatVolume* vol);

  /** Return the next available byte without consuming it.
   *
   * \return The byte if no error and not at eof else -1;
   */
  int peek();
  /** Allocate contiguous clusters to an empty file.
   *
   * The file must be empty with no clusters allocated.
   *
   * The file will contain uninitialized data.
   *
   * \param[in] length size of the file in bytes.
   * \return true for success or false for failure.
   */
  bool preAllocate(fl::u32 length);
  /** Print a file's access date
   *
   * \param[in] pr Print stream for output.
   *
   * \return The number of characters printed.
   */
  fl::size printAccessDate(print_t* pr);
  /** Print a file's access date
   *
   * \param[in] pr Print stream for output.
   *
   * \return The number of characters printed.
   */
  fl::size printAccessDateTime(print_t* pr) {
    return printAccessDate(pr);
  }
  /** Print a file's creation date and time
   *
   * \param[in] pr Print stream for output.
   *
   * \return The number of bytes printed.
   */
  fl::size printCreateDateTime(print_t* pr);
  /** %Print a directory date field.
   *
   *  Format is yyyy-mm-dd.
   *
   * \param[in] pr Print stream for output.
   * \param[in] fatDate The date field from a directory entry.
   */
  static void printFatDate(print_t* pr, fl::u16 fatDate);
  /** %Print a directory time field.
   *
   * Format is hh:mm:ss.
   *
   * \param[in] pr Print stream for output.
   * \param[in] fatTime The time field from a directory entry.
   */
  static void printFatTime(print_t* pr, fl::u16 fatTime);
  /** Print a number followed by a field terminator.
   * \param[in] value The number to be printed.
   * \param[in] term The field terminator.  Use '\\n' for CR LF.
   * \param[in] prec Number of digits after decimal point.
   * \return The number of bytes written or -1 if an error occurs.
   */
  fl::size printField(double value, char term, fl::u8 prec = 2) {
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
  fl::size printField(float value, char term, fl::u8 prec = 2) {
    return printField(static_cast<double>(value), term, prec);
  }
  /** Print a number followed by a field terminator.
   * \param[in] value The number to be printed.
   * \param[in] term The field terminator.  Use '\\n' for CR LF.
   * \return The number of bytes written or -1 if an error occurs.
   */
  template <typename Type>
  fl::size printField(Type value, char term) {
    char sign = 0;
    char buf[3*sizeof(Type) + 3];
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
      str = fmtBase10(str, (fl::u16)value);
    } else {
      str = fmtBase10(str, (fl::u32)value);
    }
    if (sign) {
      *--str = sign;
    }
    return write(str, &buf[sizeof(buf)] - str);
  }
  /** Print a file's size.
   *
   * \param[in] pr Print stream for output.
   *
   * \return The number of characters printed is returned
   *         for success and zero is returned for failure.
   */
  fl::size printFileSize(print_t* pr);
  /** Print a file's modify date and time
   *
   * \param[in] pr Print stream for output.
   *
   * \return The number of characters printed.
   */
  fl::size printModifyDateTime(print_t* pr);
  /** Print a file's name
   *
   * \param[in] pr Print stream for output.
   *
   * \return length for success or zero for failure.
   */
  fl::size printName(print_t* pr);
  /** Print a file's ASCII name
   *
   * \param[in] pr Print stream for output.
   *
   * \return true for success or false for failure.
   */
  fl::size printName7(print_t* pr);
  /** Print a file's UTF-8 name
   *
   * \param[in] pr Print stream for output.
   *
   * \return true for success or false for failure.
   */
  fl::size printName8(print_t* pr);
  /** Print a file's Short File Name.
   *
   * \param[in] pr Print stream for output.
   *
   * \return The number of characters printed is returned
   *         for success and zero is returned for failure.
   */
  fl::size printSFN(print_t* pr);
  /** Read the next byte from a file.
   *
   * \return For success read returns the next byte in the file as an int.
   * If an error occurs or end of file is reached -1 is returned.
   */
  int read() {
    fl::u8 b;
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
  int read(void* buf, fl::size count);
  /** Read the next directory entry from a directory file.
   *
   * \param[out] dir The DirFat_t struct that will receive the data.
   *
   * \return For success readDir() returns the number of bytes read.
   * A value of zero will be returned if end of file is reached.
   * If an error occurs, readDir() returns -1.  Possible errors include
   * readDir() called before a directory has been opened, this is not
   * a directory file or an I/O error occurred.
   */
  fl::i8 readDir(DirFat_t* dir);
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
   * \note the renamed file will be moved to the current volume working
   * directory.
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
  bool rename(FatFile* dirFile, const char* newPath);
  /** Set the file's current position to zero. */
  void rewind() {
    seekSet(0);
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
  /** Recursively delete a directory and all contained files.
   *
   * This is like the Unix/Linux 'rm -rf *' if called with the root directory
   * hence the name.
   *
   * Warning - This will remove all contents of the directory including
   * subdirectories.  The directory will then be removed if it is not root.
   * The read-only attribute for files will be ignored.
   *
   * \note This function should not be used to delete the 8.3 version of
   * a directory that has a long name.  See remove() and rmdir().
   *
   * \return true for success or false for failure.
   */
  bool rmRfStar();
  /** Set the files position to current position + \a pos. See seekSet().
   * \param[in] offset The new position in bytes from the current position.
   * \return true for success or false for failure.
   */
  bool seekCur(fl::i32 offset) {
    return seekSet(mCurPosition + offset);
  }
  /** Set the files position to end-of-file + \a offset. See seekSet().
   * Can't be used for directory files since file size is not defined.
   * \param[in] offset The new position in bytes from end-of-file.
   * \return true for success or false for failure.
   */
  bool seekEnd(fl::i32 offset = 0) {
    return isFile() ? seekSet(mFileSize + offset) : false;
  }
  /** Sets a file's position.
   *
   * \param[in] pos The new position in bytes from the beginning of the file.
   *
   * \return true for success or false for failure.
   */
  bool seekSet(fl::u32 pos);
  /** The sync() call causes all modified data and directory fields
   * to be written to the storage device.
   *
   * \return true for success or false for failure.
   */
  bool sync();
  /** Set a file's timestamps in its directory entry.
   *
   * \param[in] flags Values for \a flags are constructed by a bitwise-inclusive
   * OR of flags from the following list
   *
   * T_ACCESS - Set the file's last access date.
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
  bool timestamp(fl::u8 flags, fl::u16 year, fl::u8 month, fl::u8 day,
                 fl::u8 hour, fl::u8 minute, fl::u8 second);

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
  bool truncate(fl::u32 length) {
    return seekSet(length) && truncate();
  }
  /** Write a string to a file. Used by the Arduino Print class.
   * \param[in] str Pointer to the string.
   * Use getWriteError to check for errors.
   * \return count of characters written for success or -1 for failure.
   */
  fl::size write(const char* str) {
    return write(str, fl::strlen(str));
  }
  /** Write a single byte.
   * \param[in] b The byte to be written.
   * \return +1 for success or -1 for failure.
   */
  fl::size write(fl::u8 b) {
    return write(&b, 1);
  }
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
   * \a count.  If an error occurs, write() returns zero and writeError is set.
   *
   */
  fl::size write(const void* buf, fl::size count);
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
  bool ls(fl::u8 flags = 0) {
    return ls(&Serial, flags);
  }
  /** Print a file's name.
   *
   * \return length for success or zero for failure.
   */
  fl::size printName() {
    return FatFile::printName(&Serial);
  }
#endif  // ENABLE_ARDUINO_SERIAL

 private:
  /** FatVolume allowed access to private members. */
  friend class FatVolume;

  /** This file has not been opened. */
  static const fl::u8 FILE_ATTR_CLOSED = 0;
  /** File is read-only. */
  static const fl::u8 FILE_ATTR_READ_ONLY = FAT_ATTRIB_READ_ONLY;
  /** File should be hidden in directory listings. */
  static const fl::u8 FILE_ATTR_HIDDEN = FAT_ATTRIB_HIDDEN;
  /** Entry is for a system file. */
  static const fl::u8 FILE_ATTR_SYSTEM = FAT_ATTRIB_SYSTEM;
  /** Entry for normal data file */
  static const fl::u8 FILE_ATTR_FILE = 0X08;
  /** Entry is for a subdirectory */
  static const fl::u8 FILE_ATTR_SUBDIR = FAT_ATTRIB_DIRECTORY;
  /** A FAT12 or FAT16 root directory */
  static const fl::u8 FILE_ATTR_ROOT_FIXED = 0X20;
  /** A FAT32 root directory */
  static const fl::u8 FILE_ATTR_ROOT32 = 0X40;
  /** Entry is for root. */
  static const fl::u8 FILE_ATTR_ROOT =
                       FILE_ATTR_ROOT_FIXED | FILE_ATTR_ROOT32;
  /** Directory type bits */
  static const fl::u8 FILE_ATTR_DIR = FILE_ATTR_SUBDIR | FILE_ATTR_ROOT;
  /** Attributes to copy from directory entry */
  static const fl::u8 FILE_ATTR_COPY =
                       FAT_ATTRIB_READ_ONLY | FAT_ATTRIB_HIDDEN |
                       FAT_ATTRIB_SYSTEM | FAT_ATTRIB_DIRECTORY;

  // private functions

  bool addCluster();
  bool addDirCluster();
  DirFat_t* cacheDir(fl::u16 index) {
    return seekSet(32UL*index) ? readDirCache() : nullptr;
  }
  DirFat_t* cacheDirEntry(fl::u8 action);
  bool cmpName(fl::u16 index, FatLfn_t* fname, fl::u8 lfnOrd);
  bool createLFN(fl::u16 index, FatLfn_t* fname, fl::u8 lfnOrd);
  fl::u16 getLfnChar(DirLfn_t* ldir, fl::u8 i);
  fl::u8 lfnChecksum(fl::u8* name) {
    fl::u8 sum = 0;
    for (fl::u8 i = 0; i < 11; i++) {
        sum = (((sum & 1) << 7) | (sum >> 1)) + name[i];
    }
    return sum;
  }
  static bool makeSFN(FatLfn_t* fname);
  bool makeUniqueSfn(FatLfn_t* fname);
  bool openCluster(FatFile* file);
  bool parsePathName(const char* str, FatLfn_t* fname, const char** ptr);
  bool parsePathName(const char* str, FatSfn_t* fname, const char** ptr);
  bool mkdir(FatFile* parent, FatName_t* fname);
  bool open(FatFile* dirFile, FatLfn_t* fname, oflag_t oflag);
  bool open(FatFile* dirFile, FatSfn_t* fname, oflag_t oflag);
  bool openSFN(FatSfn_t* fname);
  bool openCachedEntry(FatFile* dirFile, fl::u16 cacheIndex, oflag_t oflag,
                       fl::u8 lfnOrd);
  DirFat_t* readDirCache(bool skipReadOk = false);

  // bits defined in mFlags
  static const fl::u8 FILE_FLAG_READ = 0X01;
  static const fl::u8 FILE_FLAG_WRITE = 0X02;
  static const fl::u8 FILE_FLAG_APPEND = 0X08;
  // treat curPosition as valid length.
  static const fl::u8 FILE_FLAG_PREALLOCATE = 0X20;
  // file is contiguous
  static const fl::u8 FILE_FLAG_CONTIGUOUS  = 0X40;
  // sync of directory entry required
  static const fl::u8 FILE_FLAG_DIR_DIRTY = 0X80;

  // private data
  static const fl::u8 WRITE_ERROR = 0X1;
  static const fl::u8 READ_ERROR  = 0X2;

  fl::u8    mAttributes = FILE_ATTR_CLOSED;
  fl::u8    mError = 0;        // Error bits.
  fl::u8    mFlags = 0;        // See above for definition of mFlags bits
  fl::u8    mLfnOrd;
  fl::u16   mDirIndex;         // index of directory entry in dir file
  FatVolume* mVol;              // volume where file is located
  fl::u32   mDirCluster;
  fl::u32   mCurCluster;       // cluster for current file position
  fl::u32   mCurPosition;      // current file position
  fl::u32   mDirSector;        // sector for this files directory entry
  fl::u32   mFileSize;         // file size in bytes
  fl::u32   mFirstCluster;     // first cluster of file
};

/**
 * \class File32
 * \brief FAT16/FAT32 file with Arduino Stream.
 */
class File32 : public StreamFile<FatFile, fl::u32> {
 public:
   /** Opens the next file or folder in a directory.
   *
   * \param[in] oflag open flags.
   * \return a FatStream object.
   */
  File32 openNextFile(oflag_t oflag = O_RDONLY) {
    File32 tmpFile;
    tmpFile.openNext(this, oflag);
    return tmpFile;
  }
};
}  // namespace sdfat
}  // namespace teensy
}  // namespace platforms
}  // namespace fl

#endif  // FatFile_h
