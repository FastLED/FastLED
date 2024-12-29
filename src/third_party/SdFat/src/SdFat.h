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
#ifndef SdFat_h
#define SdFat_h
/**
 * \file
 * \brief main SdFs include file.
 */
#include "ExFatLib/ExFatLib.h"
#include "FatLib/FatLib.h"
#include "FsLib/FsLib.h"
#include "SdCard/SdCard.h"
#include "common/SysCall.h"
#if INCLUDE_SDIOS
#include "sdios.h"
#endif  // INCLUDE_SDIOS
//------------------------------------------------------------------------------
/** SdFat version for cpp use. */
#define SD_FAT_VERSION 20203
/** SdFat version as string. */
#define SD_FAT_VERSION_STR "2.2.3"
//==============================================================================
/**
 * \class SdBase
 * \brief base SD file system template class.
 */
template <class Vol, class Fmt>
class SdBase : public Vol {
 public:
  //----------------------------------------------------------------------------
  /** Initialize SD card and file system.
   *
   * \param[in] csPin SD card chip select pin.
   * \return true for success or false for failure.
   */
  bool begin(SdCsPin_t csPin = SS) {
#ifdef BUILTIN_SDCARD
    if (csPin == BUILTIN_SDCARD) {
      return begin(SdioConfig(FIFO_SDIO));
    }
#endif  // BUILTIN_SDCARD
    return begin(SdSpiConfig(csPin, SHARED_SPI));
  }
  //----------------------------------------------------------------------------
  /** Initialize SD card and file system.
   *
   * \param[in] csPin SD card chip select pin.
   * \param[in] maxSck Maximum SCK frequency.
   * \return true for success or false for failure.
   */
  bool begin(SdCsPin_t csPin, uint32_t maxSck) {
    return begin(SdSpiConfig(csPin, SHARED_SPI, maxSck));
  }
  //----------------------------------------------------------------------------
  /** Initialize SD card and file system for SPI mode.
   *
   * \param[in] spiConfig SPI configuration.
   * \return true for success or false for failure.
   */
  bool begin(SdSpiConfig spiConfig) {
    return cardBegin(spiConfig) && volumeBegin();
  }
  //---------------------------------------------------------------------------
  /** Initialize SD card and file system for SDIO mode.
   *
   * \param[in] sdioConfig SDIO configuration.
   * \return true for success or false for failure.
   */
  bool begin(SdioConfig sdioConfig) {
    return cardBegin(sdioConfig) && volumeBegin();
  }
  //----------------------------------------------------------------------------
  /** \return Pointer to SD card object. */
  SdCard* card() { return m_card; }
  //----------------------------------------------------------------------------
  /** Initialize SD card in SPI mode.
   *
   * \param[in] spiConfig SPI configuration.
   * \return true for success or false for failure.
   */
  bool cardBegin(SdSpiConfig spiConfig) {
    m_card = m_cardFactory.newCard(spiConfig);
    return m_card && !m_card->errorCode();
  }
  //----------------------------------------------------------------------------
  /** Initialize SD card in SDIO mode.
   *
   * \param[in] sdioConfig SDIO configuration.
   * \return true for success or false for failure.
   */
  bool cardBegin(SdioConfig sdioConfig) {
    m_card = m_cardFactory.newCard(sdioConfig);
    return m_card && !m_card->errorCode();
  }
  //----------------------------------------------------------------------------
  /** End use of card. */
  void end() {
    Vol::end();
    if (m_card) {
      m_card->end();
    }
  }
  //----------------------------------------------------------------------------
  /** %Print error info and halt.
   *
   * \param[in] pr Print destination.
   */
  void errorHalt(print_t* pr) {
    if (sdErrorCode()) {
      pr->print(F("SdError: 0X"));
      pr->print(sdErrorCode(), HEX);
      pr->print(F(",0X"));
      pr->println(sdErrorData(), HEX);
    } else if (!Vol::fatType()) {
      pr->println(F("Check SD format."));
    }
    while (true) {
    }
  }
  //----------------------------------------------------------------------------
  /** %Print error info and halt.
   *
   * \param[in] pr Print destination.
   * \param[in] msg Message to print.
   */
  void errorHalt(print_t* pr, const char* msg) {
    pr->print(F("error: "));
    pr->println(msg);
    errorHalt(pr);
  }
  //----------------------------------------------------------------------------
  /** %Print msg and halt.
   *
   * \param[in] pr Print destination.
   * \param[in] msg Message to print.
   */
  void errorHalt(print_t* pr, const __FlashStringHelper* msg) {
    pr->print(F("error: "));
    pr->println(msg);
    errorHalt(pr);
  }
  //----------------------------------------------------------------------------
  /** Format SD card
   *
   * \param[in] pr Print destination.
   * \return true for success else false.
   */
  bool format(print_t* pr = nullptr) {
    Fmt fmt;
    uint8_t* mem = Vol::end();
    if (!mem) {
      return false;
    }
    bool switchSpi = hasDedicatedSpi() && !isDedicatedSpi();
    if (switchSpi && !setDedicatedSpi(true)) {
      return false;
    }
    bool rtn = fmt.format(card(), mem, pr);
    if (switchSpi && !setDedicatedSpi(false)) {
      return false;
    }
    return rtn;
  }
  //----------------------------------------------------------------------------
  /** \return the free cluster count. */
  uint32_t freeClusterCount() {
    bool switchSpi = hasDedicatedSpi() && !isDedicatedSpi();
    if (switchSpi && !setDedicatedSpi(true)) {
      return 0;
    }
    uint32_t rtn = Vol::freeClusterCount();
    if (switchSpi && !setDedicatedSpi(false)) {
      return 0;
    }
    return rtn;
  }
  //----------------------------------------------------------------------------
  /** \return true if can be in dedicated SPI state */
  bool hasDedicatedSpi() { return m_card ? m_card->hasDedicatedSpi() : false; }
  //----------------------------------------------------------------------------
  /** %Print error info and halt.
   *
   * \param[in] pr Print destination.
   */
  void initErrorHalt(print_t* pr) {
    initErrorPrint(pr);
    while (true) {
    }
  }
  //----------------------------------------------------------------------------
  /** %Print error info and halt.
   *
   * \param[in] pr Print destination.
   * \param[in] msg Message to print.
   */
  void initErrorHalt(print_t* pr, const char* msg) {
    pr->println(msg);
    initErrorHalt(pr);
  }
  //----------------------------------------------------------------------------
  /** %Print error info and halt.
   *
   * \param[in] pr Print destination.
   * \param[in] msg Message to print.
   */
  void initErrorHalt(print_t* pr, const __FlashStringHelper* msg) {
    pr->println(msg);
    initErrorHalt(pr);
  }
  //----------------------------------------------------------------------------
  /** Print error details after begin() fails.
   *
   * \param[in] pr Print destination.
   */
  void initErrorPrint(print_t* pr) {
    pr->println(F("begin() failed"));
    if (sdErrorCode()) {
      pr->println(F("Do not reformat the SD."));
      if (sdErrorCode() == SD_CARD_ERROR_CMD0) {
        pr->println(F("No card, wrong chip select pin, or wiring error?"));
      }
    }
    errorPrint(pr);
  }
  //----------------------------------------------------------------------------
  /** \return true if in dedicated SPI state. */
  bool isDedicatedSpi() { return m_card ? m_card->isDedicatedSpi() : false; }
  //----------------------------------------------------------------------------
  /** %Print volume FAT/exFAT type.
   *
   * \param[in] pr Print destination.
   */
  void printFatType(print_t* pr) {
    if (Vol::fatType() == FAT_TYPE_EXFAT) {
      pr->print(F("exFAT"));
    } else {
      pr->print(F("FAT"));
      pr->print(Vol::fatType());
    }
  }
  //----------------------------------------------------------------------------
  /** %Print SD errorCode and errorData.
   *
   * \param[in] pr Print destination.
   */
  void errorPrint(print_t* pr) {
    if (sdErrorCode()) {
      pr->print(F("SdError: 0X"));
      pr->print(sdErrorCode(), HEX);
      pr->print(F(",0X"));
      pr->println(sdErrorData(), HEX);
    } else if (!Vol::fatType()) {
      pr->println(F("Check SD format."));
    }
  }
  //----------------------------------------------------------------------------
  /** %Print msg, any SD error code.
   *
   * \param[in] pr Print destination.
   * \param[in] msg Message to print.
   */
  void errorPrint(print_t* pr, char const* msg) {
    pr->print(F("error: "));
    pr->println(msg);
    errorPrint(pr);
  }

  /** %Print msg, any SD error code.
   *
   * \param[in] pr Print destination.
   * \param[in] msg Message to print.
   */
  void errorPrint(print_t* pr, const __FlashStringHelper* msg) {
    pr->print(F("error: "));
    pr->println(msg);
    errorPrint(pr);
  }
  //----------------------------------------------------------------------------
  /** %Print error info and return.
   *
   * \param[in] pr Print destination.
   */
  void printSdError(print_t* pr) {
    if (sdErrorCode()) {
      if (sdErrorCode() == SD_CARD_ERROR_CMD0) {
        pr->println(F("No card, wrong chip select pin, or wiring error?"));
      }
      pr->print(F("SD error: "));
      printSdErrorSymbol(pr, sdErrorCode());
      pr->print(F(" = 0x"));
      pr->print(sdErrorCode(), HEX);
      pr->print(F(",0x"));
      pr->println(sdErrorData(), HEX);
    } else if (!Vol::fatType()) {
      pr->println(F("Check SD format."));
    }
  }
  //----------------------------------------------------------------------------
  /** \return SD card error code. */
  uint8_t sdErrorCode() {
    if (m_card) {
      return m_card->errorCode();
    }
    return SD_CARD_ERROR_INVALID_CARD_CONFIG;
  }
  //----------------------------------------------------------------------------
  /** \return SD card error data. */
  uint8_t sdErrorData() { return m_card ? m_card->errorData() : 0; }
  //----------------------------------------------------------------------------
  /** Set SPI sharing state
   * \param[in] value desired state.
   * \return true for success else false;
   */
  bool setDedicatedSpi(bool value) {
    if (m_card) {
      return m_card->setDedicatedSpi(value);
    }
    return false;
  }
  //----------------------------------------------------------------------------
  /** \return pointer to base volume */
  Vol* vol() { return reinterpret_cast<Vol*>(this); }
  //----------------------------------------------------------------------------
  /** Initialize file system after call to cardBegin.
   *
   * \return true for success or false for failure.
   */
  bool volumeBegin() {
    return Vol::begin(m_card) || Vol::begin(m_card, true, 0);
  }
#if ENABLE_ARDUINO_SERIAL
  /** Print error details after begin() fails. */
  void initErrorPrint() { initErrorPrint(&Serial); }
  //----------------------------------------------------------------------------
  /** %Print msg to Serial and halt.
   *
   * \param[in] msg Message to print.
   */
  void errorHalt(const __FlashStringHelper* msg) { errorHalt(&Serial, msg); }
  //----------------------------------------------------------------------------
  /** %Print error info to Serial and halt. */
  void errorHalt() { errorHalt(&Serial); }
  //----------------------------------------------------------------------------
  /** %Print error info and halt.
   *
   * \param[in] msg Message to print.
   */
  void errorHalt(const char* msg) { errorHalt(&Serial, msg); }
  //----------------------------------------------------------------------------
  /** %Print error info and halt. */
  void initErrorHalt() { initErrorHalt(&Serial); }
  //----------------------------------------------------------------------------
  /** %Print msg, any SD error code.
   *
   * \param[in] msg Message to print.
   */
  void errorPrint(const char* msg) { errorPrint(&Serial, msg); }
  /** %Print msg, any SD error code.
   *
   * \param[in] msg Message to print.
   */
  void errorPrint(const __FlashStringHelper* msg) { errorPrint(&Serial, msg); }
  //----------------------------------------------------------------------------
  /** %Print error info and halt.
   *
   * \param[in] msg Message to print.
   */
  void initErrorHalt(const char* msg) { initErrorHalt(&Serial, msg); }
  //----------------------------------------------------------------------------
  /** %Print error info and halt.
   *
   * \param[in] msg Message to print.
   */
  void initErrorHalt(const __FlashStringHelper* msg) {
    initErrorHalt(&Serial, msg);
  }
#endif  // ENABLE_ARDUINO_SERIAL
  //----------------------------------------------------------------------------
 private:
  SdCard* m_card = nullptr;
  SdCardFactory m_cardFactory;
};
//------------------------------------------------------------------------------
/**
 * \class SdFat32
 * \brief SD file system class for FAT volumes.
 */
class SdFat32 : public SdBase<FatVolume, FatFormatter> {
 public:
};
//------------------------------------------------------------------------------
/**
 * \class SdExFat
 * \brief SD file system class for exFAT volumes.
 */
class SdExFat : public SdBase<ExFatVolume, ExFatFormatter> {
 public:
};
//------------------------------------------------------------------------------
/**
 * \class SdFs
 * \brief SD file system class for FAT16, FAT32, and exFAT volumes.
 */
class SdFs : public SdBase<FsVolume, FsFormatter> {
 public:
};
//------------------------------------------------------------------------------
#if SDFAT_FILE_TYPE == 1 || defined(DOXYGEN)
/** Select type for SdFat. */
typedef SdFat32 SdFat;
/** Select type for SdBaseFile. */
typedef FatFile SdBaseFile;
#elif SDFAT_FILE_TYPE == 2
typedef SdExFat SdFat;
typedef ExFatFile SdBaseFile;
#elif SDFAT_FILE_TYPE == 3
typedef SdFs SdFat;
typedef FsBaseFile SdBaseFile;
#else  // SDFAT_FILE_TYPE
#error Invalid SDFAT_FILE_TYPE
#endif  // SDFAT_FILE_TYPE
//
// Only define File if FS.h is not included.
// Line with test for __has_include must not have operators or parentheses.
#if defined __has_include
#if __has_include(<FS.h>)
#define HAS_INCLUDE_FS_H
#endif  // __has_include(<FS.h>)
#endif  // defined __has_include
#ifndef HAS_INCLUDE_FS_H
#if SDFAT_FILE_TYPE == 1 || defined(DOXYGEN)
/** Select type for File. */
typedef File32 File;
#elif SDFAT_FILE_TYPE == 2
typedef ExFile File;
#elif SDFAT_FILE_TYPE == 3
typedef FsFile File;
#endif  // SDFAT_FILE_TYPE
#elif !defined(DISABLE_FS_H_WARNING)
#warning File not defined because __has_include(FS.h)
#endif  // HAS_INCLUDE_FS_H
/**
 * \class SdFile
 * \brief FAT16/FAT32 file with Print.
 */
class SdFile : public PrintFile<SdBaseFile> {
 public:
  SdFile() {}
  /** Create an open SdFile.
   * \param[in] path path for file.
   * \param[in] oflag open flags.
   */
  SdFile(const char* path, oflag_t oflag) { open(path, oflag); }
  /** Set the date/time callback function
   *
   * \param[in] dateTime The user's call back function.  The callback
   * function is of the form:
   *
   * \code
   * void dateTime(uint16_t* date, uint16_t* time) {
   *   uint16_t year;
   *   uint8_t month, day, hour, minute, second;
   *
   *   // User gets date and time from GPS or real-time clock here
   *
   *   // return date using FS_DATE macro to format fields
   *   *date = FS_DATE(year, month, day);
   *
   *   // return time using FS_TIME macro to format fields
   *   *time = FS_TIME(hour, minute, second);
   * }
   * \endcode
   *
   * Sets the function that is called when a file is created or when
   * a file's directory entry is modified by sync(). All timestamps,
   * access, creation, and modify, are set when a file is created.
   * sync() maintains the last access date and last modify date/time.
   *
   */
  static void dateTimeCallback(void (*dateTime)(uint16_t* date,
                                                uint16_t* time)) {
    FsDateTime::setCallback(dateTime);
  }
  /**  Cancel the date/time callback function. */
  static void dateTimeCallbackCancel() { FsDateTime::clearCallback(); }
};
#endif  // SdFat_h
