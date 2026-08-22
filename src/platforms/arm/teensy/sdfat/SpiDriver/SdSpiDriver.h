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
 * \brief SpiDriver classes
 */
// IWYU pragma: private
#ifndef SdSpiDriver_h
#define SdSpiDriver_h
#include "platforms/arm/teensy/sdfat/common/SysCall.h"
#include "fl/stl/int.h"
#include "platforms/arm/teensy/is_teensy.h"  // ok platform headers

// Transport selection for the Arduino-shaped hardware driver.
//
// On Teensy 4 (IMXRT1062) SdFat's SPI port is FastLED's own `LpspiBus` rather
// than the framework's `SPIClass`. This is not a preference: the Teensyduino
// `SPI` library is only *compiled* when the library finder selects it from an
// unconditional sketch-level `#include <SPI.h>`, so `SPIClass::begin`,
// `SPIClass::transfer` and the global `SPI` object resolve as headers but
// never link (FastLED #3970). The library also cannot be vendored -- it is
// GPLv2/LGPLv2.1 and FastLED is MIT. `LpspiBus` (#3802) already owns the
// registers, so the port type is simply retargeted at it.
//
// The Kinetis Teensys (3.x and LC) get the same treatment through
// `DspiBus`, which owns SPI0's DSPI registers directly (FastLED #3972). That
// also retires the `SPISettings::ctar_clock_table` / `ctar_div_table`
// dependency: the frequency-to-CTAR mapping is computed from the K20/K64/K66
// reference manual instead of transcribed from the GPL/LGPL tables.
//
// Define `SD_SPI_USE_ARDUINO_SPI` to force the Arduino path back on as an
// escape hatch. Nothing links in that configuration unless the sketch also
// carries an unconditional `#include <SPI.h>`.
#if !defined(SD_SPI_USE_ARDUINO_SPI)
#if defined(FL_IS_TEENSY_4X)
#define FL_SDFAT_HAS_LPSPI_BUS
#elif defined(FL_IS_TEENSY_3X) || defined(FL_IS_TEENSY_LC)
#define FL_SDFAT_HAS_DSPI_BUS
#endif
#endif

#if SPI_DRIVER_SELECT < 2
#if defined(FL_SDFAT_HAS_LPSPI_BUS)
#include "platforms/arm/teensy/teensy4_common/lpspi/lpspi_bus.h"  // ok platform headers
#elif defined(FL_SDFAT_HAS_DSPI_BUS)
#include "platforms/arm/teensy/kinetis_spi/dspi_bus.h"  // ok platform headers
#else
#include "SPI.h"
#endif
#endif

// SpiPort_t / SpiPortSettings_t for SPI_DRIVER_SELECT < 2 are declared in
// SdSpiArduinoDriver.h below, which is the only consumer and already opens
// this namespace -- the include has to stay above any namespace here.
#if SPI_DRIVER_SELECT == 0 && SD_HAS_CUSTOM_SPI
#define SD_USE_CUSTOM_SPI
#include "platforms/arm/teensy/sdfat/SpiDriver/SdSpiArduinoDriver.h"
#else  // SPI_DRIVER_SELECT == 0 && SD_HAS_CUSTOM_SPI
#error Invalid SPI_DRIVER_SELECT
#endif  // SPI_DRIVER_SELECT == 0 && SD_HAS_CUSTOM_SPI

namespace fl { namespace platforms { namespace teensy { namespace sdfat {
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
const fl::u8 SHARED_SPI = 0;
#if ENABLE_DEDICATED_SPI
/** The SD is the only device on the SPI bus. */
const fl::u8 DEDICATED_SPI = 1;
/**
 * \param[in] opt option field of SdSpiConfig.
 * \return true for dedicated SPI.
 */
inline bool spiOptionDedicated(fl::u8 opt) {return opt & DEDICATED_SPI;}
#else  // ENABLE_DEDICATED_SPI
/**
 * \param[in] opt option field of SdSpiConfig.
 * \return true for dedicated SPI.
 */
inline bool spiOptionDedicated(fl::u8 opt) {(void)opt; return false;}
#endif  // ENABLE_DEDICATED_SPI
//------------------------------------------------------------------------------
/** SPISettings for SCK frequency in Hz. */
#define SD_SCK_HZ(maxSpeed) (maxSpeed)
/** SPISettings for SCK frequency in MHz. */
#define SD_SCK_MHZ(maxMhz) (1000000UL*(maxMhz))
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
#if SPI_DRIVER_SELECT == 2
class SdSpiSoftDriver;
/** Port type for software SPI driver. */
typedef SdSpiSoftDriver SpiPort_t;
#elif SPI_DRIVER_SELECT == 3
class SdSpiBaseClass;
/** Port type for extrernal SPI driver. */
typedef SdSpiBaseClass SpiPort_t;
#elif SPI_DRIVER_SELECT >= 4
typedef void*  SpiPort_t;
#endif  // SPI_DRIVER_SELECT (< 2 is handled above, next to its include)
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
  SdSpiConfig(SdCsPin_t cs, fl::u8 opt, fl::u32 maxSpeed, SpiPort_t* port) :
    csPin(cs), options(opt), maxSck(maxSpeed), spiPort(port) {}

  /** SdSpiConfig constructor.
   *
   * \param[in] cs Chip select pin.
   * \param[in] opt Options.
   * \param[in] maxSpeed Maximum SCK frequency.
   */
  SdSpiConfig(SdCsPin_t cs, fl::u8 opt, fl::u32 maxSpeed) :
    csPin(cs), options(opt), maxSck(maxSpeed) {}
  /** SdSpiConfig constructor.
   *
   * \param[in] cs Chip select pin.
   * \param[in] opt Options.
   */
  SdSpiConfig(SdCsPin_t cs, fl::u8 opt) : csPin(cs), options(opt) {}
  /** SdSpiConfig constructor.
   *
   * \param[in] cs Chip select pin.
   */
  explicit SdSpiConfig(SdCsPin_t cs) : csPin(cs) {}

  /** Chip select pin. */
  const SdCsPin_t csPin;
  /** Options */
  const fl::u8 options = SHARED_SPI;
  /** Max SCK frequency */
  const fl::u32 maxSck = SD_SCK_MHZ(50);
  /** SPI port */
  SpiPort_t* spiPort = nullptr;
};
} } } }  // namespace fl::platforms::teensy::sdfat

#endif  // SdSpiDriver_h
