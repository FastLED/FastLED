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
/**
 * \file
 * \brief SpiDriver classes for Arduino compatible systems.
 */
#ifndef SdSpiArduinoDriver_h
#define SdSpiArduinoDriver_h
//==============================================================================
#if SPI_DRIVER_SELECT == 0 && SD_HAS_CUSTOM_SPI
#define SD_USE_CUSTOM_SPI
#endif  // SPI_DRIVER_SELECT == 0 && SD_HAS_CUSTOM_SPI
/**
 * \class SdSpiArduinoDriver
 * \brief Optimized SPI class for access to SD and SDHC flash memory cards.
 */
class SdSpiArduinoDriver {
 public:
  /** Constructor. */
  SdSpiArduinoDriver() = default;
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
  uint8_t receive();
  /** Receive multiple bytes.
   *
   * \param[out] buf Buffer to receive the data.
   * \param[in] count Number of bytes to receive.
   *
   * \return Zero for no error or nonzero error code.
   */
  uint8_t receive(uint8_t* buf, size_t count);
  /** Send a byte.
   *
   * \param[in] data Byte to send
   */
  void send(uint8_t data);
  /** Send multiple bytes.
   *
   * \param[in] buf Buffer for data to be sent.
   * \param[in] count Number of bytes to send.
   */
  void send(const uint8_t* buf, size_t count);
  /** Save high speed SPISettings after SD initialization.
   *
   * \param[in] maxSck Maximum SCK frequency.
   */
  void setSckSpeed(uint32_t maxSck) {
    m_spiSettings = SPISettings(maxSck, MSBFIRST, SPI_MODE0);
  }

 private:
  SPIClass* m_spi = nullptr;
  SPISettings m_spiSettings;
};
/** Typedef for use of SdSpiArduinoDriver */
typedef SdSpiArduinoDriver SdSpiDriver;
//------------------------------------------------------------------------------
#ifndef SD_USE_CUSTOM_SPI
#include "SdSpiLibDriver.h"
#elif defined(__AVR__)
#include "SdSpiAvr.h"
#endif  // __AVR__
#endif  // SdSpiArduinoDriver_h
