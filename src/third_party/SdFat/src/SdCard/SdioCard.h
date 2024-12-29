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
 * \brief Classes for SDIO cards.
 */
#ifndef SdioCard_h
#define SdioCard_h
#include "../common/SysCall.h"
#include "SdCardInterface.h"
/** Use programmed I/O with FIFO. */
#define FIFO_SDIO 0
/** Use programmed I/O with DMA. */
#define DMA_SDIO 1
/**
 * \class SdioConfig
 * \brief SDIO card configuration.
 */
class SdioConfig {
 public:
  SdioConfig() {}
  /**
   * SdioConfig constructor.
   * \param[in] opt SDIO options.
   */
  explicit SdioConfig(uint8_t opt) : m_options(opt) {}
  /** \return SDIO card options. */
  uint8_t options() { return m_options; }
  /** \return true if DMA_SDIO. */
  bool useDma() { return m_options & DMA_SDIO; }

 private:
  uint8_t m_options = FIFO_SDIO;
};
//------------------------------------------------------------------------------
/**
 * \class SdioCard
 * \brief Raw SDIO access to SD and SDHC flash memory cards.
 */
class SdioCard : public SdCardInterface {
 public:
  /** Initialize the SD card.
   * \param[in] sdioConfig SDIO card configuration.
   * \return true for success or false for failure.
   */
  bool begin(SdioConfig sdioConfig);
  /** CMD6 Switch mode: Check Function Set Function.
   * \param[in] arg CMD6 argument.
   * \param[out] status return status data.
   *
   * \return true for success or false for failure.
   */
  bool cardCMD6(uint32_t arg, uint8_t* status);
  /** Disable an SDIO card.
   * not implemented.
   */
  void end() {}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
  uint32_t __attribute__((error("use sectorCount()"))) cardSize();
#endif  // DOXYGEN_SHOULD_SKIP_THIS
  /** Erase a range of sectors.
   *
   * \param[in] firstSector The address of the first sector in the range.
   * \param[in] lastSector The address of the last sector in the range.
   *
   * \note This function requests the SD card to do a flash erase for a
   * range of sectors.  The data on the card after an erase operation is
   * either 0 or 1, depends on the card vendor.  The card must support
   * single sector erase.
   *
   * \return true for success or false for failure.
   */
  bool erase(uint32_t firstSector, uint32_t lastSector);
  /**
   * \return code for the last error. See SdCardInfo.h for a list of error
   * codes.
   */
  uint8_t errorCode() const;
  /** \return error data for last error. */
  uint32_t errorData() const;
  /** \return error line for last error. Tmp function for debug. */
  uint32_t errorLine() const;
  /**
   * Check for busy with CMD13.
   *
   * \return true if busy else false.
   */
  bool isBusy();
  /** \return the SD clock frequency in kHz. */
  uint32_t kHzSdClk();
  /**
   * Read a 512 byte sector from an SD card.
   *
   * \param[in] sector Logical sector to be read.
   * \param[out] dst Pointer to the location that will receive the data.
   * \return true for success or false for failure.
   */
  bool readSector(uint32_t sector, uint8_t* dst);
  /**
   * Read multiple 512 byte sectors from an SD card.
   *
   * \param[in] sector Logical sector to be read.
   * \param[in] ns Number of sectors to be read.
   * \param[out] dst Pointer to the location that will receive the data.
   * \return true for success or false for failure.
   */
  bool readSectors(uint32_t sector, uint8_t* dst, size_t ns);
  /**
   * Read a card's CID register. The CID contains card identification
   * information such as Manufacturer ID, Product name, Product serial
   * number and Manufacturing date.
   *
   * \param[out] cid pointer to area for returned data.
   *
   * \return true for success or false for failure.
   */
  bool readCID(cid_t* cid);
  /**
   * Read a card's CSD register. The CSD contains Card-Specific Data that
   * provides information regarding access to the card's contents.
   *
   * \param[out] csd pointer to area for returned data.
   *
   * \return true for success or false for failure.
   */
  bool readCSD(csd_t* csd);
  /** Read one data sector in a multiple sector read sequence
   *
   * \param[out] dst Pointer to the location for the data to be read.
   *
   * \return true for success or false for failure.
   */
  bool readData(uint8_t* dst);
  /** Read OCR register.
   *
   * \param[out] ocr Value of OCR register.
   * \return true for success or false for failure.
   */
  bool readOCR(uint32_t* ocr);
  /** Read SCR register.
   *
   * \param[out] scr Value of SCR register.
   * \return true for success or false for failure.
   */
  bool readSCR(scr_t* scr);
  /** Return the 64 byte SD Status register.
   * \param[out] sds location for 64 status bytes.
   * \return true for success or false for failure.
   */
  bool readSDS(sds_t* sds);
  /** Start a read multiple sectors sequence.
   *
   * \param[in] sector Address of first sector in sequence.
   *
   * \note This function is used with readData() and readStop() for optimized
   * multiple sector reads.
   *
   * \return true for success or false for failure.
   */
  bool readStart(uint32_t sector);
  /** End a read multiple sectors sequence.
   *
   * \return true for success or false for failure.
   */
  bool readStop();
  /** \return SDIO card status. */
  uint32_t status();
  /**
   * Determine the size of an SD flash memory card.
   *
   * \return The number of 512 byte data sectors in the card
   *         or zero if an error occurs.
   */
  uint32_t sectorCount();
  /**
   *  Send CMD12 to stop read or write.
   *
   * \param[in] blocking If true, wait for command complete.
   *
   * \return true for success or false for failure.
   */
  bool stopTransmission(bool blocking);
  /** \return success if sync successful. Not for user apps. */
  bool syncDevice();
  /** Return the card type: SD V1, SD V2 or SDHC
   * \return 0 - SD V1, 1 - SD V2, or 3 - SDHC.
   */
  uint8_t type() const;
  /**
   * Writes a 512 byte sector to an SD card.
   *
   * \param[in] sector Logical sector to be written.
   * \param[in] src Pointer to the location of the data to be written.
   * \return true for success or false for failure.
   */
  bool writeSector(uint32_t sector, const uint8_t* src);
  /**
   * Write multiple 512 byte sectors to an SD card.
   *
   * \param[in] sector Logical sector to be written.
   * \param[in] ns Number of sectors to be written.
   * \param[in] src Pointer to the location of the data to be written.
   * \return true for success or false for failure.
   */
  bool writeSectors(uint32_t sector, const uint8_t* src, size_t ns);
  /** Write one data sector in a multiple sector write sequence.
   * \param[in] src Pointer to the location of the data to be written.
   * \return true for success or false for failure.
   */
  bool writeData(const uint8_t* src);
  /** Start a write multiple sectors sequence.
   *
   * \param[in] sector Address of first sector in sequence.
   *
   * \note This function is used with writeData() and writeStop()
   * for optimized multiple sector writes.
   *
   * \return true for success or false for failure.
   */
  bool writeStart(uint32_t sector);

  /** End a write multiple sectors sequence.
   *
   * \return true for success or false for failure.
   */
  bool writeStop();

 private:
  static const uint8_t IDLE_STATE = 0;
  static const uint8_t READ_STATE = 1;
  static const uint8_t WRITE_STATE = 2;
  uint32_t m_curSector;
  SdioConfig m_sdioConfig;
  uint8_t m_curState = IDLE_STATE;
};
#endif  // SdioCard_h
