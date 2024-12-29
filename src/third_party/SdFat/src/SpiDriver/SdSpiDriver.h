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
 * \brief SpiDriver classes
 */
#ifndef SdSpiDriver_h
#define SdSpiDriver_h
#include "../common/SysCall.h"
/**
 * Initialize SD chip select pin.
 *
 * \param[in] pin SD card chip select pin.
 */
void sdCsInit(SdCsPin_t pin);
/**
 * Initialize SD chip select pin.
 *
 * \param[in] pin SD card chip select pin.
 * \param[in] level SD card chip select level.
 */
void sdCsWrite(SdCsPin_t pin, bool level);
//------------------------------------------------------------------------------
/** SPI bus is share with other devices. */
const uint8_t SHARED_SPI = 0;
#if ENABLE_DEDICATED_SPI
/** The SD is the only device on the SPI bus. */
const uint8_t DEDICATED_SPI = 1;
/**
 * \param[in] opt option field of SdSpiConfig.
 * \return true for dedicated SPI.
 */
inline bool spiOptionDedicated(uint8_t opt) { return opt & DEDICATED_SPI; }
#else   // ENABLE_DEDICATED_SPI
/**
 * \param[in] opt option field of SdSpiConfig.
 * \return true for dedicated SPI.
 */
inline bool spiOptionDedicated(uint8_t opt) {
  (void)opt;
  return false;
}
#endif  // ENABLE_DEDICATED_SPI
/** The user will call begin. Useful for custom SPI configurations.       */
const uint8_t USER_SPI_BEGIN = 2;
//------------------------------------------------------------------------------
/** SPISettings for SCK frequency in Hz. */
#define SD_SCK_HZ(maxSpeed) (maxSpeed)
/** SPISettings for SCK frequency in MHz. */
#define SD_SCK_MHZ(maxMhz) (1000000UL * (maxMhz))
// SPI divisor constants - obsolete.
/** Set SCK to max rate. */
#define SPI_FULL_SPEED SD_SCK_MHZ(50)
/** Set SCK rate to 16 MHz for Due */
#define SPI_DIV3_SPEED SD_SCK_MHZ(16)
/** Set SCK rate to 4 MHz for AVR. */
#define SPI_HALF_SPEED SD_SCK_MHZ(4)
/** Set SCK rate to 8 MHz for Due */
#define SPI_DIV6_SPEED SD_SCK_MHZ(8)
/** Set SCK rate to 2 MHz for AVR. */
#define SPI_QUARTER_SPEED SD_SCK_MHZ(2)
/** Set SCK rate to 1 MHz for AVR. */
#define SPI_EIGHTH_SPEED SD_SCK_MHZ(1)
/** Set SCK rate to 500 kHz for AVR. */
#define SPI_SIXTEENTH_SPEED SD_SCK_HZ(500000)
//------------------------------------------------------------------------------
#if SPI_DRIVER_SELECT < 2
#include "SPI.h"
/** Port type for Arduino SPI hardware driver. */
typedef SPIClass SpiPort_t;
#elif SPI_DRIVER_SELECT == 2
class SdSpiSoftDriver;
/** Port type for software SPI driver. */
typedef SdSpiSoftDriver SpiPort_t;
#elif SPI_DRIVER_SELECT == 3
class SdSpiBaseClass;
/** Port type for extrernal SPI driver. */
typedef SdSpiBaseClass SpiPort_t;
#else   // SPI_DRIVER_SELECT
typedef void* SpiPort_t;
#endif  // SPI_DRIVER_SELECT
//------------------------------------------------------------------------------
/**
 * \class SdSpiConfig
 * \brief SPI card configuration.
 */
class SdSpiConfig {
 public:
  /** SdSpiConfig constructor.
   *
   * \param[in] cs Chip select pin.
   * \param[in] opt Options.
   * \param[in] maxSpeed Maximum SCK frequency.
   * \param[in] port The SPI port to use.
   */
  SdSpiConfig(SdCsPin_t cs, uint8_t opt, uint32_t maxSpeed, SpiPort_t* port)
      : csPin(cs), options(opt), maxSck(maxSpeed), spiPort(port) {}

  /** SdSpiConfig constructor.
   *
   * \param[in] cs Chip select pin.
   * \param[in] opt Options.
   * \param[in] maxSpeed Maximum SCK frequency.
   */
  SdSpiConfig(SdCsPin_t cs, uint8_t opt, uint32_t maxSpeed)
      : csPin(cs), options(opt), maxSck(maxSpeed) {}
  /** SdSpiConfig constructor.
   *
   * \param[in] cs Chip select pin.
   * \param[in] opt Options.
   */
  SdSpiConfig(SdCsPin_t cs, uint8_t opt) : csPin(cs), options(opt) {}
  /** SdSpiConfig constructor.
   *
   * \param[in] cs Chip select pin.
   */
  explicit SdSpiConfig(SdCsPin_t cs) : csPin(cs) {}

  /** Chip select pin. */
  const SdCsPin_t csPin;
  /** Options */
  const uint8_t options = SHARED_SPI;
  /** Max SCK frequency */
  const uint32_t maxSck = SD_SCK_MHZ(50);
  /** SPI port */
  SpiPort_t* spiPort = nullptr;
};
#if SPI_DRIVER_SELECT < 2
#include "SdSpiArduinoDriver.h"
#elif SPI_DRIVER_SELECT == 2
#include "SdSpiSoftDriver.h"
#elif SPI_DRIVER_SELECT == 3
#include "SdSpiBaseClass.h"
typedef SdSpiBaseClass SdSpiDriver;
#else  // SPI_DRIVER_SELECT
#error Invalid SPI_DRIVER_SELECT
#endif  // SPI_DRIVER_SELECT
#endif  // SdSpiDriver_h
