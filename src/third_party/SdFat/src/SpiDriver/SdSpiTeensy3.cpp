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
#include "SdSpiDriver.h"
#if defined(SD_USE_CUSTOM_SPI) && defined(__arm__) && defined(CORE_TEENSY)
#define USE_BLOCK_TRANSFER 1
//------------------------------------------------------------------------------
void SdSpiArduinoDriver::activate() { m_spi->beginTransaction(m_spiSettings); }
//------------------------------------------------------------------------------
void SdSpiArduinoDriver::begin(SdSpiConfig spiConfig) {
  if (spiConfig.spiPort) {
    m_spi = spiConfig.spiPort;
#if defined(SDCARD_SPI) && defined(SDCARD_SS_PIN)
  } else if (spiConfig.csPin == SDCARD_SS_PIN) {
    m_spi = &SDCARD_SPI;
    m_spi->setMISO(SDCARD_MISO_PIN);
    m_spi->setMOSI(SDCARD_MOSI_PIN);
    m_spi->setSCK(SDCARD_SCK_PIN);
#endif  // defined(SDCARD_SPI) && defined(SDCARD_SS_PIN)
  } else {
    m_spi = &SPI;
  }
  m_spi->begin();
}
//------------------------------------------------------------------------------
void SdSpiArduinoDriver::deactivate() { m_spi->endTransaction(); }
//------------------------------------------------------------------------------
void SdSpiArduinoDriver::end() { m_spi->end(); }
//------------------------------------------------------------------------------
uint8_t SdSpiArduinoDriver::receive() { return m_spi->transfer(0XFF); }
//------------------------------------------------------------------------------
uint8_t SdSpiArduinoDriver::receive(uint8_t* buf, size_t count) {
#if USE_BLOCK_TRANSFER
  memset(buf, 0XFF, count);
  m_spi->transfer(buf, count);
#else   // USE_BLOCK_TRANSFER
  for (size_t i = 0; i < count; i++) {
    buf[i] = m_spi->transfer(0XFF);
  }
#endif  // USE_BLOCK_TRANSFER
  return 0;
}
//------------------------------------------------------------------------------
void SdSpiArduinoDriver::send(uint8_t data) { m_spi->transfer(data); }
//------------------------------------------------------------------------------
void SdSpiArduinoDriver::send(const uint8_t* buf, size_t count) {
#if USE_BLOCK_TRANSFER
  uint32_t tmp[128];
  if (0 < count && count <= 512) {
    memcpy(tmp, buf, count);
    m_spi->transfer(tmp, count);
    return;
  }
#endif  // USE_BLOCK_TRANSFER
  for (size_t i = 0; i < count; i++) {
    m_spi->transfer(buf[i]);
  }
}
#endif  // defined(SD_USE_CUSTOM_SPI) && defined(__arm__) &&defined(CORE_TEENSY)
