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
/**
 * \file
 * \brief FsBlockDeviceInterface include file.
 */
// IWYU pragma: private
#ifndef FsBlockDeviceInterface_h
#define FsBlockDeviceInterface_h
#include "fl/stl/int.h"  // IWYU pragma: keep
namespace fl { namespace platforms { namespace teensy { namespace sdfat {
/**
 * \class FsBlockDeviceInterface
 * \brief FsBlockDeviceInterface class.
 */
class FsBlockDeviceInterface {
 public:
  virtual ~FsBlockDeviceInterface() {}

  /** end use of device */
  virtual void end() {}
  /**
   * Check for FsBlockDevice busy.
   *
   * \return true if busy else false.
   */
  virtual bool isBusy() = 0;
  /**
   * Read a sector.
   *
   * \param[in] sector Logical sector to be read.
   * \param[out] dst Pointer to the location that will receive the data.
   * \return true for success or false for failure.
   */
  virtual bool readSector(fl::u32 sector, fl::u8* dst) = 0;

  /**
   * Read multiple sectors.
   *
   * \param[in] sector Logical sector to be read.
   * \param[in] ns Number of sectors to be read.
   * \param[out] dst Pointer to the location that will receive the data.
   * \return true for success or false for failure.
   */
  virtual bool readSectors(fl::u32 sector, fl::u8* dst, size_t ns) = 0;

  /**
   * Read multiple sectors with callback as each sector's data
   *
   * \param[in] sector Logical sector to be read.
   * \param[in] ns Number of sectors to be read.
   * \param[out] dst Pointer to the location that will receive the data.
   * \param[in] callback Function to be called with each sector's data
   * \param[in] context Pointer to be passed to the callback function
   * \return true for success or false for failure.
   */
  virtual bool readSectorsCallback(fl::u32 sector, fl::u8* dst, size_t ns,
   void (*callback)(fl::u32 sector, fl::u8 *buf, void *context), void *context) {
     for (size_t i = 0; i < ns; i++) {
       if (!readSector(sector + i, dst)) return false;
       callback(sector + i, dst, context);
     }
     return true;
  }

  /** \return device size in sectors. */
  virtual fl::u32 sectorCount() = 0;

  /** End multi-sector transfer and go to idle state.
   * \return true for success or false for failure.
   */
  virtual bool syncDevice() = 0;

  /**
   * Writes a sector.
   *
   * \param[in] sector Logical sector to be written.
   * \param[in] src Pointer to the location of the data to be written.
   * \return true for success or false for failure.
   */
  virtual bool writeSector(fl::u32 sector, const fl::u8* src) = 0;

  /**
   * Write multiple sectors.
   *
   * \param[in] sector Logical sector to be written.
   * \param[in] ns Number of sectors to be written.
   * \param[in] src Pointer to the location of the data to be written.
   * \return true for success or false for failure.
   */
  virtual bool writeSectors(fl::u32 sector, const fl::u8* src, size_t ns) = 0;

  /**
   * Write multiple sectors with callback for each sector's data
   *
   * \param[in] sector Logical sector to be written.
   * \param[in] ns Number of sectors to be written.
   * \param[in] callback Function to be called for each sector's data
   * \param[in] context Context to pass to callback function
   * \return true for success or false for failure.
   */
  virtual bool writeSectorsCallback(fl::u32 sector, size_t ns,
   const fl::u8 * (*callback)(fl::u32 sector, void *context), void *context) {
     for (size_t i = 0; i < ns; i++) {
       if (!writeSector(sector + i, callback(sector + i, context))) return false;
     }
    return true;
  }
};
} } } }  // namespace fl::platforms::teensy::sdfat
#endif  // FsBlockDeviceInterface_h
