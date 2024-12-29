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
/**
 * \file
 * \brief Abstract interface for an SD card.
 */
#ifndef SdCardInterface_h
#define SdCardInterface_h
#include "../common/FsBlockDeviceInterface.h"
#include "SdCardInfo.h"
/**
 * \class SdCardInterface
 * \brief Abstract interface for an SD card.
 */
class SdCardInterface : public FsBlockDeviceInterface {
 public:
  /** CMD6 Switch mode: Check Function Set Function.
   * \param[in] arg CMD6 argument.
   * \param[out] status return status data.
   *
   * \return true for success or false for failure.
   */
  virtual bool cardCMD6(uint32_t arg, uint8_t* status) = 0;
  /** Erase a range of sectors.
   *
   * \param[in] firstSector The address of the first sector in the range.
   * \param[in] lastSector The address of the last sector in the range.
   *
   * \return true for success or false for failure.
   */
  virtual bool erase(uint32_t firstSector, uint32_t lastSector) = 0;
  /** \return error code. */
  virtual uint8_t errorCode() const = 0;
  /** \return error data. */
  virtual uint32_t errorData() const = 0;
  /** \return false by default */
  virtual bool hasDedicatedSpi() { return false; }
  /** \return false by default */
  bool virtual isDedicatedSpi() { return false; }
  /** \return false by default */
  bool virtual isSpi() { return false; }
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
  virtual bool readOCR(uint32_t* ocr) = 0;
  /** Read SCR register.
   *
   * \param[out] scr Value of SCR register.
   * \return true for success or false for failure.
   */
  virtual bool readSCR(scr_t* scr) = 0;
  /** Return the 64 byte SD Status register.
   * \param[out] sds location for 64 status bytes.
   * \return true for success or false for failure.
   */
  virtual bool readSDS(sds_t* sds) = 0;
  /** \return card status. */
  virtual uint32_t status() { return 0XFFFFFFFF; }
  /** Return the card type: SD V1, SD V2 or SDHC/SDXC
   * \return 0 - SD V1, 1 - SD V2, or 3 - SDHC/SDXC.
   */
  virtual uint8_t type() const = 0;
  /** Write one data sector in a multiple sector write sequence.
   * \param[in] src Pointer to the location of the data to be written.
   * \return true for success or false for failure.
   */

  virtual bool writeData(const uint8_t* src) = 0;
  /** Start a write multiple sectors sequence.
   *
   * \param[in] sector Address of first sector in sequence.
   *
   * \return true for success or false for failure.
   */
  virtual bool writeStart(uint32_t sector) = 0;
  /** End a write multiple sectors sequence.
   * \return true for success or false for failure.
   */
  virtual bool writeStop() = 0;
};
#endif  // SdCardInterface_h
