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
/**
 * \file
 * \brief SdSpiCard class for V2 SD/SDHC cards
 */
#ifndef SdSpiCard_h
#define SdSpiCard_h
#include "fl/stl/cstddef.h"
#include "fl/stl/int.h"
#include "platforms/arm/teensy/sdfat/common/SysCall.h"  // IWYU pragma: keep
#include "platforms/arm/teensy/sdfat/SdCard/SdCardInfo.h"
#include "platforms/arm/teensy/sdfat/SdCard/SdCardInterface.h"
#include "platforms/arm/teensy/sdfat/SpiDriver/SdSpiDriver.h"  // IWYU pragma: keep
// Keep the imported SdFat implementation's legacy member spellings working.
#define m_errorCode mErrorCode
#define m_status mStatus
#define m_state mState
#define m_type mType
#define m_spiDriver mSpiDriver
#define m_spiDriverPtr mSpiDriverPtr
#define m_csPin mCsPin
#define m_spiActive mSpiActive
#define m_dedicatedSpi mDedicatedSpi
#define m_curSector mCurSector
namespace fl { namespace platforms { namespace teensy { namespace sdfat {

//==============================================================================
/**
 * \class SharedSpiCard
 * \brief Raw access to SD and SDHC flash memory cards via shared SPI port.
 */
#if HAS_SDIO_CLASS
class SharedSpiCard : public SdCardInterface {
#elif USE_BLOCK_DEVICE_INTERFACE
class SharedSpiCard : public FsBlockDeviceInterface {
#else  // HAS_SDIO_CLASS
class SharedSpiCard {
#endif  // HAS_SDIO_CLASS
 public:
  /** SD is in idle state */
  static const fl::u8 IDLE_STATE = 0;
  /** SD is in multi-sector read state. */
  static const fl::u8 READ_STATE = 1;
  /** SD is in multi-sector write state. */
  static const fl::u8 WRITE_STATE = 2;
  /** Construct an instance of SharedSpiCard. */
  SharedSpiCard() {}
  /** Initialize the SD card.
   * \param[in] spiConfig SPI card configuration.
   * \return true for success or false for failure.
   */
  bool begin(SdSpiConfig spiConfig);
  /** End use of card */
  void end() {
    spiEnd();
  }
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
  bool erase(fl::u32 firstSector, fl::u32 lastSector);
  /** Determine if card supports single sector erase.
   *
   * \return true is returned if single sector erase is supported.
   * false is returned if single sector erase is not supported.
   */
  bool eraseSingleSectorEnable();
  /**
   *  Set SD error code.
   *  \param[in] code value for error code.
   */
  void error(fl::u8 code) {
//    (void)code;
    mErrorCode = code;
  }
  /**
   * \return code for the last error. See SdCardInfo.h for a list of error codes.
   */
  fl::u8 errorCode() const {
    return mErrorCode;
  }
  /** \return error data for last error. */
  fl::u32 errorData() const {
    return mStatus;
  }
  /** \return false for shared class. */
  bool hasDedicatedSpi() {return false;}
  /**
   * Check for busy.  MISO low indicates the card is busy.
   *
   * \return true if busy else false.
   */
  bool isBusy();
  /** \return false, can't be in dedicated state. */
  bool isDedicatedSpi() {return false;}
  /**
   * Read a card's CID register. The CID contains card identification
   * information such as Manufacturer ID, Product name, Product serial
   * number and Manufacturing date.
   *
   * \param[out] cid pointer to area for returned data.
   *
   * \return true for success or false for failure.
   */
  bool readCID(cid_t* cid) {
    return readRegister(CMD10, cid);
  }
  /**
   * Read a card's CSD register. The CSD contains Card-Specific Data that
   * provides information regarding access to the card's contents.
   *
   * \param[out] csd pointer to area for returned data.
   *
   * \return true for success or false for failure.
   */
  bool readCSD(csd_t* csd) {
    return readRegister(CMD9, csd);
  }
  /** Read one data sector in a multiple sector read sequence
   *
   * \param[out] dst Pointer to the location for the data to be read.
   *
   * \return true for success or false for failure.
   */
  bool readData(fl::u8* dst);
  /** Read OCR register.
   *
   * \param[out] ocr Value of OCR register.
   * \return true for success or false for failure.
   */
  bool readOCR(fl::u32* ocr);
  /**
   * Read a 512 byte sector from an SD card.
   *
   * \param[in] sector Logical sector to be read.
   * \param[out] dst Pointer to the location that will receive the data.
   * \return true for success or false for failure.
   */
  bool readSector(fl::u32 sector, fl::u8* dst);
  /**
   * Read multiple 512 byte sectors from an SD card.
   *
   * \param[in] sector Logical sector to be read.
   * \param[in] ns Number of sectors to be read.
   * \param[out] dst Pointer to the location that will receive the data.
   * \return true for success or false for failure.
   */
  bool readSectors(fl::u32 sector, fl::u8* dst, fl::size_t ns);
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
  bool readSectorsCallback(fl::u32 sector, fl::u8* dst, fl::size_t ns,
   void (*callback)(fl::u32 sector, fl::u8 *buf, void *context), void *context);

  /** Start a read multiple sector sequence.
   *
   * \param[in] sector Address of first sector in sequence.
   *
   * \note This function is used with readData() and readStop() for optimized
   * multiple sector reads.  SPI chipSelect must be low for the entire sequence.
   *
   * \return true for success or false for failure.
   */
  bool readStart(fl::u32 sector);
  /** Return the 64 byte card status
   * \param[out] status location for 64 status bytes.
   * \return true for success or false for failure.
   */
  bool readStatus(fl::u8* status);
  /** End a read multiple sectors sequence.
   *
   * \return true for success or false for failure.
   */
  bool readStop();
  /** \return SD multi-sector read/write state */
  fl::u8 sdState() {return mState;}
  /**
   * Determine the size of an SD flash memory card.
   *
   * \return The number of 512 byte data sectors in the card
   *         or zero if an error occurs.
   */
  fl::u32 sectorCount();
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  // Use sectorCount(). cardSize() will be removed in the future.
  fl::u32 __attribute__((error("use sectorCount()"))) cardSize();
#endif  // DOXYGEN_SHOULD_SKIP_THIS
  /** Set SPI sharing state
   * \param[in] value desired state.
   * \return false for shared card
   */
  bool setDedicatedSpi(bool value) {
    (void)value;
    return false;
  }
  /** end a mult-sector transfer.
   *
   * \return true for success or false for failure.
   */
  bool stopTransfer();
  /** \return success if sync successful. Not for user apps. */
  bool syncDevice();
  /** Return the card type: SD V1, SD V2 or SDHC/SDXC
   * \return 0 - SD V1, 1 - SD V2, or 3 - SDHC/SDXC.
   */
  fl::u8 type() const {
    return mType;
  }
  /**
   * Write a 512 byte sector to an SD card.
   *
   * \param[in] sector Logical sector to be written.
   * \param[in] src Pointer to the location of the data to be written.
   * \return true for success or false for failure.
   */
  bool writeSector(fl::u32 sector, const fl::u8* src);
  /**
   * Write multiple 512 byte sectors to an SD card.
   *
   * \param[in] sector Logical sector to be written.
   * \param[in] ns Number of sectors to be written.
   * \param[in] src Pointer to the location of the data to be written.
   * \return true for success or false for failure.
   */
  bool writeSectors(fl::u32 sector, const fl::u8* src, fl::size_t ns);
  /**
   * Write multiple sectors to SD card with callback to prep data.
   *
   * \param[in] sector Logical sector to be written.
   * \param[in] ns Number of sectors to be written.
   * \param[in] callback Function to be called for each sector's data
   * \param[in] context to pass to callback function
   * \return true for success or false for failure.
   */
  bool writeSectorsCallback(fl::u32 sector, fl::size_t ns,
   const fl::u8 * (*callback)(fl::u32 sector, void *context), void *context);
  /** Write one data sector in a multiple sector write sequence.
   * \param[in] src Pointer to the location of the data to be written.
   * \return true for success or false for failure.
   */
  bool writeData(const fl::u8* src);
  /** Start a write multiple sectors sequence.
   *
   * \param[in] sector Address of first sector in sequence.
   *
   * \note This function is used with writeData() and writeStop()
   * for optimized multiple sector writes.
   *
   * \return true for success or false for failure.
   */
  bool writeStart(fl::u32 sector);

  /** End a write multiple sectors sequence.
   *
   * \return true for success or false for failure.
   */
  bool writeStop();

 private:
  // private functions
  fl::u8 cardAcmd(fl::u8 cmd, fl::u32 arg) {
    cardCommand(CMD55, 0);
    return cardCommand(cmd, arg);
  }
  fl::u8 cardCommand(fl::u8 cmd, fl::u32 arg);
  bool readData(fl::u8* dst, fl::size_t count);
  bool readRegister(fl::u8 cmd, void* buf);
  void spiSelect() {
    sdCsWrite(mCsPin, false);
  }
  void spiStart();
  void spiStop();
  void spiUnselect() {
    sdCsWrite(mCsPin, true);
  }
  void type(fl::u8 value) {
    mType = value;
  }
  bool waitReady(fl::u16 ms);
  bool writeData(fl::u8 token, const fl::u8* src);
#if SPI_DRIVER_SELECT < 2
  void spiActivate() {
    mSpiDriver.activate();
  }
  void spiBegin(SdSpiConfig spiConfig) {
    mSpiDriver.begin(spiConfig);
  }
  void spiDeactivate() {
    mSpiDriver.deactivate();
  }
  void spiEnd() {
    mSpiDriver.end();
  }
  fl::u8 spiReceive() {
    return mSpiDriver.receive();
  }
  fl::u8 spiReceive(fl::u8* buf, fl::size_t n) {
    return mSpiDriver.receive(buf, n);
  }
  void spiSend(fl::u8 data) {
    mSpiDriver.send(data);
  }
  void spiSend(const fl::u8* buf, fl::size_t n) {
    mSpiDriver.send(buf, n);
  }
  void spiSetSckSpeed(fl::u32 maxSck) {
    mSpiDriver.setSckSpeed(maxSck);
  }
  SdSpiDriver mSpiDriver;
#else  // SPI_DRIVER_SELECT < 2
  void spiActivate() {
    mSpiDriverPtr->activate();
  }
  void spiBegin(SdSpiConfig spiConfig) {
    mSpiDriverPtr->begin(spiConfig);
  }
  void spiDeactivate() {
    mSpiDriverPtr->deactivate();
  }
  void spiEnd() {
    mSpiDriverPtr->end();
  }
  fl::u8 spiReceive() {
    return mSpiDriverPtr->receive();
  }
  fl::u8 spiReceive(fl::u8* buf, fl::size_t n) {
    return mSpiDriverPtr->receive(buf, n);
  }
  void spiSend(fl::u8 data) {
    mSpiDriverPtr->send(data);
  }
  void spiSend(const fl::u8* buf, fl::size_t n) {
    mSpiDriverPtr->send(buf, n);
  }
  void spiSetSckSpeed(fl::u32 maxSck) {
    mSpiDriverPtr->setSckSpeed(maxSck);
  }
  SdSpiDriver* mSpiDriverPtr;
#endif  // SPI_DRIVER_SELECT < 2
  SdCsPin_t mCsPin;
  fl::u8 mErrorCode = SD_CARD_ERROR_INIT_NOT_CALLED;
  bool    mSpiActive;
  fl::u8 mState;
  fl::u8 mStatus;
  fl::u8 mType = 0;
};

//==============================================================================
/**
 * \class DedicatedSpiCard
 * \brief Raw access to SD and SDHC flash memory cards via dedicate SPI port.
 */
class DedicatedSpiCard : public SharedSpiCard {
 public:
  /** Construct an instance of DedicatedSpiCard. */
  DedicatedSpiCard() {}
  /** Initialize the SD card.
   * \param[in] spiConfig SPI card configuration.
   * \return true for success or false for failure.
   */
  bool begin(SdSpiConfig spiConfig);
  /** \return true, can be in dedicaded state. */
  bool hasDedicatedSpi() {return true;}
  /** \return true if in dedicated SPI state. */
  bool isDedicatedSpi() {return mDedicatedSpi;}
  /**
   * Read a 512 byte sector from an SD card.
   *
   * \param[in] sector Logical sector to be read.
   * \param[out] dst Pointer to the location that will receive the data.
   * \return true for success or false for failure.
   */
  bool readSector(fl::u32 sector, fl::u8* dst);
  /**
   * Read multiple 512 byte sectors from an SD card.
   *
   * \param[in] sector Logical sector to be read.
   * \param[in] ns Number of sectors to be read.
   * \param[out] dst Pointer to the location that will receive the data.
   * \return true for success or false for failure.
   */
  bool readSectors(fl::u32 sector, fl::u8* dst, fl::size_t ns);
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
  bool readSectorsCallback(fl::u32 sector, fl::u8* dst, fl::size_t ns,
   void (*callback)(fl::u32 sector, fl::u8 *buf, void *context), void *context);
  /** Set SPI sharing state
   * \param[in] value desired state.
   * \return true for success else false;
   */
  bool setDedicatedSpi(bool value);
  /**
   * Write a 512 byte sector to an SD card.
   *
   * \param[in] sector Logical sector to be written.
   * \param[in] src Pointer to the location of the data to be written.
   * \return true for success or false for failure.
   */
  bool writeSector(fl::u32 sector, const fl::u8* src);
  /**
   * Write multiple 512 byte sectors to an SD card.
   *
   * \param[in] sector Logical sector to be written.
   * \param[in] ns Number of sectors to be written.
   * \param[in] src Pointer to the location of the data to be written.
   * \return true for success or false for failure.
   */
  bool writeSectors(fl::u32 sector, const fl::u8* src, fl::size_t ns);
  /**
   * Write multiple sectors to SD card with callback to prep data.
   *
   * \param[in] sector Logical sector to be written.
   * \param[in] ns Number of sectors to be written.
   * \param[in] callback Function to be called for each sector's data
   * \param[in] context to pass to callback function
   * \return true for success or false for failure.
   */
  bool writeSectorsCallback(fl::u32 sector, fl::size_t ns,
   const fl::u8 * (*callback)(fl::u32 sector, void *context), void *context);

 private:
  fl::u32 mCurSector;
  bool mDedicatedSpi = false;
};
//==============================================================================
#if ENABLE_DEDICATED_SPI
/** typedef for dedicated SPI. */
typedef DedicatedSpiCard SdSpiCard;
#else
/** typedef for shared SPI. */
typedef SharedSpiCard SdSpiCard;
#endif
} } } }  // namespace fl::platforms::teensy::sdfat
#endif  // SdSpiCard_h
