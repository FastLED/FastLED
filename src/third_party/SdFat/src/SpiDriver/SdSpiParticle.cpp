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
#if defined(SD_USE_CUSTOM_SPI) && defined(PLATFORM_ID)
static volatile bool SPI_DMA_TransferCompleted = false;
//-----------------------------------------------------------------------------
static void SD_SPI_DMA_TransferComplete_Callback() {
  SPI_DMA_TransferCompleted = true;
}
//------------------------------------------------------------------------------
void SdSpiArduinoDriver::activate() { m_spi->beginTransaction(m_spiSettings); }
//------------------------------------------------------------------------------
void SdSpiArduinoDriver::begin(SdSpiConfig spiConfig) {
  if (spiConfig.spiPort) {
    m_spi = spiConfig.spiPort;
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
  SPI_DMA_TransferCompleted = false;
  m_spi->transfer(nullptr, buf, count, SD_SPI_DMA_TransferComplete_Callback);
  while (!SPI_DMA_TransferCompleted) {
  }
  return 0;
}
//------------------------------------------------------------------------------
void SdSpiArduinoDriver::send(uint8_t data) { m_spi->transfer(data); }
//------------------------------------------------------------------------------
void SdSpiArduinoDriver::send(const uint8_t* buf, size_t count) {
  SPI_DMA_TransferCompleted = false;

  m_spi->transfer(const_cast<uint8_t*>(buf), nullptr, count,
                  SD_SPI_DMA_TransferComplete_Callback);

  while (!SPI_DMA_TransferCompleted) {
  }
}
#endif  // defined(SD_USE_CUSTOM_SPI) && defined(PLATFORM_ID)
