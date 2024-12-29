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
#if defined(SD_USE_CUSTOM_SPI) && defined(ARDUINO_SAM_DUE)
/* Use SAM3X DMAC if nonzero */
#define USE_SAM3X_DMAC 1
/* Use extra Bus Matrix arbitration fix if nonzero */
#define USE_SAM3X_BUS_MATRIX_FIX 1
/* Time in ms for DMA receive timeout */
#define SAM3X_DMA_TIMEOUT 100
/* chip select register number */
#define SPI_CHIP_SEL 3
/* DMAC receive channel */
#define SPI_DMAC_RX_CH 1
/* DMAC transmit channel */
#define SPI_DMAC_TX_CH 0
/* DMAC Channel HW Interface Number for SPI TX. */
#define SPI_TX_IDX 1
/* DMAC Channel HW Interface Number for SPI RX. */
#define SPI_RX_IDX 2
//------------------------------------------------------------------------------
/* Disable DMA Controller. */
static void dmac_disable() { DMAC->DMAC_EN &= (~DMAC_EN_ENABLE); }
/* Enable DMA Controller. */
static void dmac_enable() { DMAC->DMAC_EN = DMAC_EN_ENABLE; }
/* Disable DMA Channel. */
static void dmac_channel_disable(uint32_t ul_num) {
  DMAC->DMAC_CHDR = DMAC_CHDR_DIS0 << ul_num;
}
/* Enable DMA Channel. */
static void dmac_channel_enable(uint32_t ul_num) {
  DMAC->DMAC_CHER = DMAC_CHER_ENA0 << ul_num;
}
/* Poll for transfer complete. */
static bool dmac_channel_transfer_done(uint32_t ul_num) {
  return (DMAC->DMAC_CHSR & (DMAC_CHSR_ENA0 << ul_num)) ? false : true;
}
//------------------------------------------------------------------------------
// start RX DMA
static void spiDmaRX(uint8_t* dst, uint16_t count) {
  dmac_channel_disable(SPI_DMAC_RX_CH);
  DMAC->DMAC_CH_NUM[SPI_DMAC_RX_CH].DMAC_SADDR = (uint32_t)&SPI0->SPI_RDR;
  DMAC->DMAC_CH_NUM[SPI_DMAC_RX_CH].DMAC_DADDR = (uint32_t)dst;
  DMAC->DMAC_CH_NUM[SPI_DMAC_RX_CH].DMAC_DSCR = 0;
  DMAC->DMAC_CH_NUM[SPI_DMAC_RX_CH].DMAC_CTRLA =
      count | DMAC_CTRLA_SRC_WIDTH_BYTE | DMAC_CTRLA_DST_WIDTH_BYTE;
  DMAC->DMAC_CH_NUM[SPI_DMAC_RX_CH].DMAC_CTRLB =
      DMAC_CTRLB_SRC_DSCR | DMAC_CTRLB_DST_DSCR | DMAC_CTRLB_FC_PER2MEM_DMA_FC |
      DMAC_CTRLB_SRC_INCR_FIXED | DMAC_CTRLB_DST_INCR_INCREMENTING;
  DMAC->DMAC_CH_NUM[SPI_DMAC_RX_CH].DMAC_CFG =
      DMAC_CFG_SRC_PER(SPI_RX_IDX) | DMAC_CFG_SRC_H2SEL | DMAC_CFG_SOD |
      DMAC_CFG_FIFOCFG_ASAP_CFG;
  dmac_channel_enable(SPI_DMAC_RX_CH);
}
//------------------------------------------------------------------------------
// start TX DMA
static void spiDmaTX(const uint8_t* src, uint16_t count) {
  static uint8_t ff = 0XFF;
  uint32_t src_incr = DMAC_CTRLB_SRC_INCR_INCREMENTING;
  if (!src) {
    src = &ff;
    src_incr = DMAC_CTRLB_SRC_INCR_FIXED;
  }
  dmac_channel_disable(SPI_DMAC_TX_CH);
  DMAC->DMAC_CH_NUM[SPI_DMAC_TX_CH].DMAC_SADDR = (uint32_t)src;
  DMAC->DMAC_CH_NUM[SPI_DMAC_TX_CH].DMAC_DADDR = (uint32_t)&SPI0->SPI_TDR;
  DMAC->DMAC_CH_NUM[SPI_DMAC_TX_CH].DMAC_DSCR = 0;
  DMAC->DMAC_CH_NUM[SPI_DMAC_TX_CH].DMAC_CTRLA =
      count | DMAC_CTRLA_SRC_WIDTH_BYTE | DMAC_CTRLA_DST_WIDTH_BYTE;

  DMAC->DMAC_CH_NUM[SPI_DMAC_TX_CH].DMAC_CTRLB =
      DMAC_CTRLB_SRC_DSCR | DMAC_CTRLB_DST_DSCR | DMAC_CTRLB_FC_MEM2PER_DMA_FC |
      src_incr | DMAC_CTRLB_DST_INCR_FIXED;

  DMAC->DMAC_CH_NUM[SPI_DMAC_TX_CH].DMAC_CFG =
      DMAC_CFG_DST_PER(SPI_TX_IDX) | DMAC_CFG_DST_H2SEL | DMAC_CFG_SOD |
      DMAC_CFG_FIFOCFG_ALAP_CFG;

  dmac_channel_enable(SPI_DMAC_TX_CH);
}
//------------------------------------------------------------------------------
//  initialize SPI controller
void SdSpiArduinoDriver::activate() {
  SPI.beginTransaction(m_spiSettings);

  Spi* pSpi = SPI0;
  // Save the divisor
  uint32_t scbr = pSpi->SPI_CSR[SPI_CHIP_SEL] & 0XFF00;
  // Disable SPI
  pSpi->SPI_CR = SPI_CR_SPIDIS;
  // reset SPI
  pSpi->SPI_CR = SPI_CR_SWRST;
  // no mode fault detection, set master mode
  pSpi->SPI_MR = SPI_PCS(SPI_CHIP_SEL) | SPI_MR_MODFDIS | SPI_MR_MSTR;
  // mode 0, 8-bit,
  pSpi->SPI_CSR[SPI_CHIP_SEL] = scbr | SPI_CSR_CSAAT | SPI_CSR_NCPHA;
  // enable SPI
  pSpi->SPI_CR |= SPI_CR_SPIEN;
}
//------------------------------------------------------------------------------
void SdSpiArduinoDriver::begin(SdSpiConfig spiConfig) {
  (void)spiConfig;
  SPI.begin();
#if USE_SAM3X_DMAC
  pmc_enable_periph_clk(ID_DMAC);
  dmac_disable();
  DMAC->DMAC_GCFG = DMAC_GCFG_ARB_CFG_FIXED;
  dmac_enable();
#if USE_SAM3X_BUS_MATRIX_FIX
  MATRIX->MATRIX_WPMR = 0x4d415400;
  MATRIX->MATRIX_MCFG[1] = 1;
  MATRIX->MATRIX_MCFG[2] = 1;
  MATRIX->MATRIX_SCFG[0] = 0x01000010;
  MATRIX->MATRIX_SCFG[1] = 0x01000010;
  MATRIX->MATRIX_SCFG[7] = 0x01000010;
#endif  // USE_SAM3X_BUS_MATRIX_FIX
#endif  // USE_SAM3X_DMAC
}
//------------------------------------------------------------------------------
void SdSpiArduinoDriver::deactivate() { SPI.endTransaction(); }
//------------------------------------------------------------------------------
void SdSpiArduinoDriver::end() { SPI.end(); }
//------------------------------------------------------------------------------
static inline uint8_t spiTransfer(uint8_t b) {
  Spi* pSpi = SPI0;

  pSpi->SPI_TDR = b;
  while ((pSpi->SPI_SR & SPI_SR_RDRF) == 0) {
  }
  b = pSpi->SPI_RDR;
  return b;
}
//------------------------------------------------------------------------------
uint8_t SdSpiArduinoDriver::receive() { return spiTransfer(0XFF); }
//------------------------------------------------------------------------------
uint8_t SdSpiArduinoDriver::receive(uint8_t* buf, size_t count) {
  Spi* pSpi = SPI0;
  int rtn = 0;
#if USE_SAM3X_DMAC
  // clear overrun error
  while (pSpi->SPI_SR & (SPI_SR_OVRES | SPI_SR_RDRF)) {
    pSpi->SPI_RDR;
  }
  spiDmaRX(buf, count);
  spiDmaTX(0, count);

  uint32_t m = millis();
  while (!dmac_channel_transfer_done(SPI_DMAC_RX_CH)) {
    if ((millis() - m) > SAM3X_DMA_TIMEOUT) {
      dmac_channel_disable(SPI_DMAC_RX_CH);
      dmac_channel_disable(SPI_DMAC_TX_CH);
      rtn = 2;
      break;
    }
  }
  if (pSpi->SPI_SR & SPI_SR_OVRES) {
    rtn |= 1;
  }
#else   // USE_SAM3X_DMAC
  for (size_t i = 0; i < count; i++) {
    pSpi->SPI_TDR = 0XFF;
    while ((pSpi->SPI_SR & SPI_SR_RDRF) == 0) {
    }
    buf[i] = pSpi->SPI_RDR;
  }
#endif  // USE_SAM3X_DMAC
  return rtn;
}
//------------------------------------------------------------------------------
void SdSpiArduinoDriver::send(uint8_t data) { spiTransfer(data); }
//------------------------------------------------------------------------------
void SdSpiArduinoDriver::send(const uint8_t* buf, size_t count) {
  Spi* pSpi = SPI0;
#if USE_SAM3X_DMAC
  spiDmaTX(buf, count);
  while (!dmac_channel_transfer_done(SPI_DMAC_TX_CH)) {
  }
#else   // #if USE_SAM3X_DMAC
  while ((pSpi->SPI_SR & SPI_SR_TXEMPTY) == 0) {
  }
  for (size_t i = 0; i < count; i++) {
    pSpi->SPI_TDR = buf[i];
    while ((pSpi->SPI_SR & SPI_SR_TDRE) == 0) {
    }
  }
#endif  // #if USE_SAM3X_DMAC
  while ((pSpi->SPI_SR & SPI_SR_TXEMPTY) == 0) {
  }
  // leave RDR empty
  while (pSpi->SPI_SR & (SPI_SR_OVRES | SPI_SR_RDRF)) {
    pSpi->SPI_RDR;
  }
}
#endif  // defined(SD_USE_CUSTOM_SPI) && defined(ARDUINO_SAM_DUE)
