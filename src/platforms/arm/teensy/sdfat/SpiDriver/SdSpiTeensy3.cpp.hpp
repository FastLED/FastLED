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
// Private implementation unit, included by sdfat/_build.cpp.hpp.
#include "platforms/arm/teensy/sdfat/SpiDriver/SdSpiDriver.h"
#include "fl/stl/cstring.h"
#if defined(SD_USE_CUSTOM_SPI) &&  defined(__arm__) && defined(CORE_TEENSY)
#define USE_TRANSFER_TX_RX 1
namespace fl { namespace platforms { namespace teensy { namespace sdfat {

//------------------------------------------------------------------------------
void SdSpiArduinoDriver::activate() {
  m_spi->beginTransaction(m_spiSettings);
}
//------------------------------------------------------------------------------
void SdSpiArduinoDriver::begin(SdSpiConfig spiConfig) {
  if (spiConfig.spiPort) {
    m_spi = spiConfig.spiPort;
#if defined(SDFAT_SDCARD_SPI) && defined(SDFAT_SDCARD_SS_PIN)
  } else if (spiConfig.csPin == SDFAT_SDCARD_SS_PIN) {
    m_spi = &SDFAT_SDCARD_SPI;
    m_spi->setMISO(SDFAT_SDCARD_MISO_PIN);
    m_spi->setMOSI(SDFAT_SDCARD_MOSI_PIN);
    m_spi->setSCK(SDFAT_SDCARD_SCK_PIN);
#endif  // defined(SDFAT_SDCARD_SPI) && defined(SDFAT_SDCARD_SS_PIN)
  } else {
    m_spi = &SPI;
  }
  m_spi->begin();
}
//------------------------------------------------------------------------------
void SdSpiArduinoDriver::deactivate() {
  m_spi->endTransaction();
}
//------------------------------------------------------------------------------
void SdSpiArduinoDriver::end() {
  m_spi->end();
}
//------------------------------------------------------------------------------
fl::u8 SdSpiArduinoDriver::receive() {
  return m_spi->transfer(0XFF);
}
//------------------------------------------------------------------------------
fl::u8 SdSpiArduinoDriver::receive(fl::u8* buf, size_t count) {
#ifdef USE_TRANSFER_TX_RX
  m_spi->setTransferWriteFill(0xff);
  m_spi->transfer(nullptr, buf, count);
#else
  fl::memset(buf, 0XFF, count);
  m_spi->transfer(buf, count);
#endif
  return 0;
}
//------------------------------------------------------------------------------
void SdSpiArduinoDriver::send(fl::u8 data) {
  m_spi->transfer(data);
}
//------------------------------------------------------------------------------
void SdSpiArduinoDriver::send(const fl::u8* buf, size_t count) {
#ifdef USE_TRANSFER_TX_RX
  m_spi->transfer(buf, nullptr, count);
#else
  fl::u32 tmp[128];
  if (0 < count && count <= 512) {
    fl::memcpy(tmp, buf, count);
    m_spi->transfer(tmp, count);
    return;
  }

#endif
}
#endif  // defined(SD_USE_CUSTOM_SPI) && defined(__arm__) &&defined(CORE_TEENSY)
#if defined(SD_USE_CUSTOM_SPI) && defined(__arm__) && defined(CORE_TEENSY)
} } } }  // namespace fl::platforms::teensy::sdfat
#endif
