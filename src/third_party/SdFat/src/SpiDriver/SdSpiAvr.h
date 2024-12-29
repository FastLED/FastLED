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
#ifndef SdSpiAvr_h
#define SdSpiAvr_h
// Use of in-line for AVR to save flash.
#define nop asm volatile("nop\n\t")
//------------------------------------------------------------------------------
inline void SdSpiArduinoDriver::activate() {
  SPI.beginTransaction(m_spiSettings);
}
//------------------------------------------------------------------------------
inline void SdSpiArduinoDriver::begin(SdSpiConfig spiConfig) {
  (void)spiConfig;
  SPI.begin();
}
//------------------------------------------------------------------------------
inline void SdSpiArduinoDriver::deactivate() { SPI.endTransaction(); }
//------------------------------------------------------------------------------
inline void SdSpiArduinoDriver::end() { SPI.end(); }
//------------------------------------------------------------------------------
inline uint8_t SdSpiArduinoDriver::receive() { return SPI.transfer(0XFF); }
//------------------------------------------------------------------------------
inline uint8_t SdSpiArduinoDriver::receive(uint8_t* buf, size_t count) {
  if (count == 0) {
    return 0;
  }
#ifdef SPSR
  SPDR = 0XFF;
  while (--count) {
    // nops optimize loop for 16MHz CPU 8 MHz SPI
    nop;
    nop;
    while (!(SPSR & _BV(SPIF))) {
    }
    uint8_t in = SPDR;
    SPDR = 0XFF;
    *buf++ = in;
  }
  while (!(SPSR & _BV(SPIF))) {
  }
  *buf = SPDR;
#elif defined(SPI_RXCIF_bm)
  SPI0.DATA = 0XFF;
  while (--count) {
    // nops optimize loop for ATmega4809 16MHz CPU 8 MHz SPI
    nop;
    nop;
    nop;
    nop;
    while (!(SPI0.INTFLAGS & SPI_RXCIF_bm)) {
    }
    uint8_t in = SPI0.DATA;
    SPI0.DATA = 0XFF;
    *buf++ = in;
  }
  while (!(SPI0.INTFLAGS & SPI_RXCIF_bm)) {
  }
  *buf = SPI0.DATA;
#else  // SPSR
#error Unsupported AVR CPU - edit SdFatConfig.h to use standard SPI library.
#endif  // SPSR
  return 0;
}
//------------------------------------------------------------------------------
inline void SdSpiArduinoDriver::send(uint8_t data) { SPI.transfer(data); }
//------------------------------------------------------------------------------
inline void SdSpiArduinoDriver::send(const uint8_t* buf, size_t count) {
  if (count == 0) {
    return;
  }
#ifdef SPSR
  SPDR = *buf++;
  while (--count) {
    uint8_t b = *buf++;
    // nops optimize loop for 16MHz CPU 8 MHz SPI
    nop;
    nop;
    while (!(SPSR & (1 << SPIF))) {
    }
    SPDR = b;
  }
  while (!(SPSR & (1 << SPIF))) {
  }
#elif defined(SPI_RXCIF_bm)
  SPI0.DATA = *buf++;
  while (--count) {
    uint8_t b = *buf++;
    // nops optimize loop for ATmega4809 16MHz CPU 8 MHz SPI
    nop;
    nop;
    nop;
    while (!(SPI0.INTFLAGS & SPI_RXCIF_bm)) {
    }
    SPI0.DATA = b;
  }
  while (!(SPI0.INTFLAGS & SPI_RXCIF_bm)) {
  }
#else  // SPSR
#error Unsupported AVR CPU - edit SdFatConfig.h to use standard SPI library.
#endif  // SPSR
}
#endif  // SdSpiAvr_h
