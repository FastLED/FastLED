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
#if defined(SD_USE_CUSTOM_SPI) && (defined(ESP8266) || defined(ESP32))
#define ESP_UNALIGN_OK 1
//------------------------------------------------------------------------------
void SdSpiArduinoDriver::activate() { m_spi->beginTransaction(m_spiSettings); }
//------------------------------------------------------------------------------
void SdSpiArduinoDriver::begin(SdSpiConfig spiConfig) {
  if (spiConfig.spiPort) {
    m_spi = spiConfig.spiPort;
#if defined(SDCARD_SPI) && defined(SDCARD_SS_PIN)
  } else if (spiConfig.csPin == SDCARD_SS_PIN) {
    m_spi = &SDCARD_SPI;
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
#if ESP_UNALIGN_OK
  m_spi->transferBytes(nullptr, buf, count);
#else   // ESP_UNALIGN_OK
  // Adjust to 32-bit alignment.
  while ((reinterpret_cast<uintptr_t>(buf) & 0X3) && count) {
    *buf++ = m_spi->transfer(0xff);
    count--;
  }
  // Do multiple of four byte transfers.
  size_t n4 = 4 * (count / 4);
  if (n4) {
    m_spi->transferBytes(nullptr, buf, n4);
  }
  // Transfer up to three remaining bytes.
  for (buf += n4, count -= n4; count; count--) {
    *buf++ = m_spi->transfer(0xff);
  }
#endif  // ESP_UNALIGN_OK
  return 0;
}
//------------------------------------------------------------------------------
void SdSpiArduinoDriver::send(uint8_t data) { m_spi->transfer(data); }
//------------------------------------------------------------------------------
void SdSpiArduinoDriver::send(const uint8_t* buf, size_t count) {
#if !ESP_UNALIGN_OK
  // Adjust to 32-bit alignment.
  while ((reinterpret_cast<uintptr_t>(buf) & 0X3) && count) {
    SPI.transfer(*buf++);
    count--;
  }
#endif  // #if ESP_UNALIGN_OK

  m_spi->transferBytes(const_cast<uint8_t*>(buf), nullptr, count);
}
#endif  // defined(SD_USE_CUSTOM_SPI) && (defined(ESP8266) || defined(ESP32))
