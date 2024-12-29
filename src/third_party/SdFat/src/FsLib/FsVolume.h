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
#ifndef FsVolume_h
#define FsVolume_h
/**
 * \file
 * \brief FsVolume include file.
 */
#include "../ExFatLib/ExFatLib.h"
#include "../FatLib/FatLib.h"
#include "FsNew.h"

class FsFile;
/**
 * \class FsVolume
 * \brief FsVolume class.
 */
class FsVolume {
 public:
  FsVolume() = default;

  ~FsVolume() { end(); }
  /** Get file's user settable attributes.
   * \param[in] path path to file.
   * \return user settable file attributes for success else -1.
   */
  int attrib(const char* path) {
    return m_fVol ? m_fVol->attrib(path) : m_xVol ? m_xVol->attrib(path) : -1;
  }
  /** Set file's user settable attributes.
   * \param[in] path path to file.
   * \param[in] bits bit-wise or of selected attributes: FS_ATTRIB_READ_ONLY,
   *            FS_ATTRIB_HIDDEN, FS_ATTRIB_SYSTEM, FS_ATTRIB_ARCHIVE.
   *
   * \return true for success or false for failure.
   */
  bool attrib(const char* path, uint8_t bits) {
    return m_fVol   ? m_fVol->attrib(path, bits)
           : m_xVol ? m_xVol->attrib(path, bits)
                    : false;
  }
  /**
   * Initialize an FatVolume object.
   * \param[in] blockDev Device block driver.
   * \param[in] setCwv Set current working volume if true.
   * \param[in] part partition to initialize.
   * \param[in] volStart Start sector of volume if part is zero.
   * \return true for success or false for failure.
   */
  bool begin(FsBlockDevice* blockDev, bool setCwv = true, uint8_t part = 1,
             uint32_t volStart = 0);
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  uint32_t __attribute__((error("use sectorsPerCluster()"))) blocksPerCluster();
#endif  // DOXYGEN_SHOULD_SKIP_THIS
  /** \return the number of bytes in a cluster. */
  uint32_t bytesPerCluster() const {
    return m_fVol   ? m_fVol->bytesPerCluster()
           : m_xVol ? m_xVol->bytesPerCluster()
                    : 0;
  }
  /**
   * Set volume working directory to root.
   * \return true for success or false for failure.
   */
  bool chdir() {
    return m_fVol ? m_fVol->chdir() : m_xVol ? m_xVol->chdir() : false;
  }
  /**
   * Set volume working directory.
   * \param[in] path Path for volume working directory.
   * \return true for success or false for failure.
   */
  bool chdir(const char* path) {
    return m_fVol ? m_fVol->chdir(path) : m_xVol ? m_xVol->chdir(path) : false;
  }
  /** Change global working volume to this volume. */
  void chvol() { m_cwv = this; }
  /** \return The total number of clusters in the volume. */
  uint32_t clusterCount() const {
    return m_fVol   ? m_fVol->clusterCount()
           : m_xVol ? m_xVol->clusterCount()
                    : 0;
  }
  /** \return The logical sector number for the start of file data. */
  uint32_t dataStartSector() const {
    return m_fVol   ? m_fVol->dataStartSector()
           : m_xVol ? m_xVol->clusterHeapStartSector()
                    : 0;
  }
  /** End access to volume
   * \return pointer to sector size buffer for format.
   */
  uint8_t* end() {
    m_fVol = nullptr;
    m_xVol = nullptr;
    static_assert(sizeof(m_volMem) >= 512, "m_volMem too small");
    return reinterpret_cast<uint8_t*>(m_volMem);
  }
  /** Test for the existence of a file in a directory
   *
   * \param[in] path Path of the file to be tested for.
   *
   * \return true if the file exists else false.
   */
  bool exists(const char* path) {
    return m_fVol   ? m_fVol->exists(path)
           : m_xVol ? m_xVol->exists(path)
                    : false;
  }
  /** \return The logical sector number for the start of the first FAT. */
  uint32_t fatStartSector() const {
    return m_fVol   ? m_fVol->fatStartSector()
           : m_xVol ? m_xVol->fatStartSector()
                    : 0;
  }
  /** \return Partition type, FAT_TYPE_EXFAT, FAT_TYPE_FAT32,
   *          FAT_TYPE_FAT16, or zero for error.
   */
  uint8_t fatType() const {
    return m_fVol ? m_fVol->fatType() : m_xVol ? m_xVol->fatType() : 0;
  }
  /** \return free cluster count or -1 if an error occurs. */
  int32_t freeClusterCount() const {
    return m_fVol   ? m_fVol->freeClusterCount()
           : m_xVol ? m_xVol->freeClusterCount()
                    : -1;
  }
  /**
   * Check for device busy.
   *
   * \return true if busy else false.
   */
  bool isBusy() {
    return m_fVol ? m_fVol->isBusy() : m_xVol ? m_xVol->isBusy() : false;
  }
  /** List directory contents.
   *
   * \param[in] pr Print object.
   *
   * \return true for success or false for failure.
   */
  bool ls(print_t* pr) {
    return m_fVol ? m_fVol->ls(pr) : m_xVol ? m_xVol->ls(pr) : false;
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
    return m_fVol   ? m_fVol->ls(pr, flags)
           : m_xVol ? m_xVol->ls(pr, flags)
                    : false;
  }
  /** List the directory contents of a directory.
   *
   * \param[in] pr Print stream for list.
   *
   * \param[in] path directory to list.
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
  bool ls(print_t* pr, const char* path, uint8_t flags);
  /** Make a subdirectory in the volume root directory.
   *
   * \param[in] path A path with a valid 8.3 DOS name for the subdirectory.
   *
   * \param[in] pFlag Create missing parent directories if true.
   *
   * \return true for success or false for failure.
   */
  bool mkdir(const char* path, bool pFlag = true) {
    return m_fVol   ? m_fVol->mkdir(path, pFlag)
           : m_xVol ? m_xVol->mkdir(path, pFlag)
                    : false;
  }
  /** open a file
   *
   * \param[in] path location of file to be opened.
   * \param[in] oflag open flags.
   * \return a FsBaseFile object.
   */
  FsFile open(const char* path, oflag_t oflag = O_RDONLY);
  /** Remove a file from the volume root directory.
   *
   * \param[in] path A path with a valid 8.3 DOS name for the file.
   *
   * \return true for success or false for failure.
   */
  bool remove(const char* path) {
    return m_fVol   ? m_fVol->remove(path)
           : m_xVol ? m_xVol->remove(path)
                    : false;
  }
  /** Rename a file or subdirectory.
   *
   * \param[in] oldPath Path name to the file or subdirectory to be renamed.
   *
   * \param[in] newPath New path name of the file or subdirectory.
   *
   * The \a newPath object must not exist before the rename call.
   *
   * The file to be renamed must not be open.  The directory entry may be
   * moved and file system corruption could occur if the file is accessed by
   * a file object that was opened before the rename() call.
   *
   * \return true for success or false for failure.
   */
  bool rename(const char* oldPath, const char* newPath) {
    return m_fVol   ? m_fVol->rename(oldPath, newPath)
           : m_xVol ? m_xVol->rename(oldPath, newPath)
                    : false;
  }
  /** Remove a subdirectory from the volume's root directory.
   *
   * \param[in] path A path with a valid 8.3 DOS name for the subdirectory.
   *
   * The subdirectory file will be removed only if it is empty.
   *
   * \return true for success or false for failure.
   */
  bool rmdir(const char* path) {
    return m_fVol ? m_fVol->rmdir(path) : m_xVol ? m_xVol->rmdir(path) : false;
  }
  /** \return The volume's cluster size in sectors. */
  uint32_t sectorsPerCluster() const {
    return m_fVol   ? m_fVol->sectorsPerCluster()
           : m_xVol ? m_xVol->sectorsPerCluster()
                    : 0;
  }
#if ENABLE_ARDUINO_SERIAL
  /** List directory contents.
   * \return true for success or false for failure.
   */
  bool ls() { return ls(&Serial); }
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
  bool ls(uint8_t flags) { return ls(&Serial, flags); }
  /** List the directory contents of a directory to Serial.
   *
   * \param[in] path directory to list.
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
   *
   * \return true for success or false for failure.
   */
  bool ls(const char* path, uint8_t flags = 0) {
    return ls(&Serial, path, flags);
  }
#endif  // ENABLE_ARDUINO_SERIAL
#if ENABLE_ARDUINO_STRING
  /**
   * Set volume working directory.
   * \param[in] path Path for volume working directory.
   * \return true for success or false for failure.
   */
  bool chdir(const String& path) { return chdir(path.c_str()); }
  /** Test for the existence of a file in a directory
   *
   * \param[in] path Path of the file to be tested for.
   *
   * \return true if the file exists else false.
   */
  bool exists(const String& path) { return exists(path.c_str()); }
  /** Make a subdirectory in the volume root directory.
   *
   * \param[in] path A path with a valid 8.3 DOS name for the subdirectory.
   *
   * \param[in] pFlag Create missing parent directories if true.
   *
   * \return true for success or false for failure.
   */
  bool mkdir(const String& path, bool pFlag = true) {
    return mkdir(path.c_str(), pFlag);
  }
  /** open a file
   *
   * \param[in] path location of file to be opened.
   * \param[in] oflag open flags.
   * \return a FsBaseFile object.
   */
  FsFile open(const String& path, oflag_t oflag = O_RDONLY);
  /** Remove a file from the volume root directory.
   *
   * \param[in] path A path with a valid 8.3 DOS name for the file.
   *
   * \return true for success or false for failure.
   */
  bool remove(const String& path) { return remove(path.c_str()); }
  /** Rename a file or subdirectory.
   *
   * \param[in] oldPath Path name to the file or subdirectory to be renamed.
   *
   * \param[in] newPath New path name of the file or subdirectory.
   *
   * The \a newPath object must not exist before the rename call.
   *
   * The file to be renamed must not be open.  The directory entry may be
   * moved and file system corruption could occur if the file is accessed by
   * a file object that was opened before the rename() call.
   *
   * \return true for success or false for failure.
   */
  bool rename(const String& oldPath, const String& newPath) {
    return rename(oldPath.c_str(), newPath.c_str());
  }
  /** Remove a subdirectory from the volume's root directory.
   *
   * \param[in] path A path with a valid 8.3 DOS name for the subdirectory.
   *
   * The subdirectory file will be removed only if it is empty.
   *
   * \return true for success or false for failure.
   */
  bool rmdir(const String& path) { return rmdir(path.c_str()); }
  /** Rename a file or subdirectory.
   *
   * \param[in] oldPath Path name to the file or subdirectory to be renamed.
   *
   * \param[in] newPath New path name of the file or subdirectory.
   *
   * The \a newPath object must not exist before the rename call.
   *
   * The file to be renamed must not be open.  The directory entry may be
   * moved and file system corruption could occur if the file is accessed by
   * a file object that was opened before the rename() call.
   *
   * \return true for success or false for failure.
   */
#endif  // ENABLE_ARDUINO_STRING

 protected:
  newalign_t m_volMem[FS_ALIGN_DIM(ExFatVolume, FatVolume)];

 private:
  /** FsBaseFile allowed access to private members. */
  friend class FsBaseFile;
  static FsVolume* cwv() { return m_cwv; }
  FsVolume(const FsVolume& from);
  FsVolume& operator=(const FsVolume& from);

  static FsVolume* m_cwv;
  FatVolume* m_fVol = nullptr;
  ExFatVolume* m_xVol = nullptr;
};
#endif  // FsVolume_h
