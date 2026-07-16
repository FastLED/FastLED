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
// IWYU pragma: private
#ifndef SdCardInterface_h
#define SdCardInterface_h
#include "fl/stl/int.h"
#include "platforms/arm/teensy/sdfat/SdCard/SdCardInfo.h"
#include "platforms/arm/teensy/sdfat/common/FsBlockDeviceInterface.h"
namespace fl { namespace platforms { namespace teensy { namespace sdfat {

/**
 * \class SdCardInterface
 * \brief Abstract interface for an SD card.
 */
class SdCardInterface : public FsBlockDeviceInterface {
 public:
  /** end use of card */
  virtual void end() = 0;
   /** Erase a range of sectors.
   *
   * \param[in] firstSector The address of the first sector in the range.
   * \param[in] lastSector The address of the last sector in the range.
   *
   * \return true for success or false for failure.
   */
  virtual bool erase(fl::u32 firstSector, fl::u32 lastSector) = 0;
  /** \return error code. */
  virtual fl::u8 errorCode() const = 0;
  /** \return error data. */
  virtual fl::u32 errorData() const = 0;
  /** \return true if card is busy. */
  virtual bool isBusy() = 0;
  /** \return false by default */
  virtual bool hasDedicatedSpi() {return false;}
  /** \return false by default */
  bool virtual isDedicatedSpi() {return false;}
  /** Set SPI sharing state
   * \param[in] value desired state.
   * \return false by default.
   */
  virtual bool setDedicatedSpi(bool value) {
    (void)value;
    return false;
  }
  /**
   * Read a card's CID register.
   *
   * \param[out] cid pointer to area for returned data.
   *
   * \return true for success or false for failure.
   */
  virtual bool readCID(cid_t* cid) = 0;
   /**
   * Read a card's CSD register.
   *
   * \param[out] csd pointer to area for returned data.
   *
   * \return true for success or false for failure.
   */
  virtual bool readCSD(csd_t* csd) = 0;
  /** Read OCR register.
   *
   * \param[out] ocr Value of OCR register.
   * \return true for success or false for failure.
   */
  virtual bool readOCR(fl::u32* ocr) = 0;
  /**
   * Determine the size of an SD flash memory card.
   *
   * \return The number of 512 byte data sectors in the card
   *         or zero if an error occurs.
   */
  virtual fl::u32 sectorCount() = 0;
  /** \return card status. */
  virtual fl::u32 status() {return 0XFFFFFFFF;}
  /** Return the card type: SD V1, SD V2 or SDHC/SDXC
   * \return 0 - SD V1, 1 - SD V2, or 3 - SDHC/SDXC.
   */
  virtual fl::u8 type() const = 0;
  /** Write one data sector in a multiple sector write sequence.
   * \param[in] src Pointer to the location of the data to be written.
   * \return true for success or false for failure.
   */

  virtual bool writeData(const fl::u8* src) = 0;
  /** Start a write multiple sectors sequence.
   *
   * \param[in] sector Address of first sector in sequence.
   *
   * \return true for success or false for failure.
   */
  virtual bool writeStart(fl::u32 sector) = 0;
  /** End a write multiple sectors sequence.
   * \return true for success or false for failure.
   */
  virtual bool writeStop() = 0;
};
} } } }  // namespace fl::platforms::teensy::sdfat
#endif  // SdCardInterface_h
