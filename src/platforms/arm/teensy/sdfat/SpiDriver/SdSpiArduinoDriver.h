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
 * \brief SpiDriver classes for Arduino compatible systems.
 */
#ifndef SdSpiArduinoDriver_h
#define SdSpiArduinoDriver_h
#include "fl/stl/stdint.h"
//==============================================================================
#if SPI_DRIVER_SELECT == 0 && SD_HAS_CUSTOM_SPI
#define SD_USE_CUSTOM_SPI
#endif  // SPI_DRIVER_SELECT == 0 && SD_HAS_CUSTOM_SPI
namespace fl { namespace platforms { namespace teensy { namespace sdfat {
class SdSpiConfig;
/**
 * \class SdSpiArduinoDriver
 * \brief Optimized SPI class for access to SD and SDHC flash memory cards.
 */
class SdSpiArduinoDriver {
 public:
  /** Activate SPI hardware. */
  void activate();
  /** Initialize the SPI bus.
   *
   * \param[in] spiConfig SD card configuration.
   */
  void begin(SdSpiConfig spiConfig);
  /** Deactivate SPI hardware. */
  void deactivate();
  /** End use of SPI driver after begin() call. */
  void end();
  /** Receive a byte.
   *
   * \return The byte.
   */
  fl::u8 receive();
  /** Receive multiple bytes.
  *
  * \param[out] buf Buffer to receive the data.
  * \param[in] count Number of bytes to receive.
  *
  * \return Zero for no error or nonzero error code.
  */
  fl::u8 receive(fl::u8* buf, fl::size count);
  /** Send a byte.
   *
   * \param[in] data Byte to send
   */
  void send(fl::u8 data);
  /** Send multiple bytes.
   *
   * \param[in] buf Buffer for data to be sent.
   * \param[in] count Number of bytes to send.
   */
  void send(const fl::u8* buf, fl::size count);
  /** Save high speed SPISettings after SD initialization.
   *
   * \param[in] maxSck Maximum SCK frequency.
   */
  void setSckSpeed(fl::u32 maxSck) {
    mSpiSettings = SPISettings(maxSck, MSBFIRST, SPI_MODE0);
  }

 private:
  SPIClass *mSpi;
  SPISettings mSpiSettings;
};
// Keep the imported implementation's member spelling without changing it.
#define m_spi mSpi
#define m_spiSettings mSpiSettings
/** Typedef for use of SdSpiArduinoDriver */
typedef SdSpiArduinoDriver SdSpiDriver;
} } } }  // namespace fl::platforms::teensy::sdfat
#endif  // SdSpiArduinoDriver_h
