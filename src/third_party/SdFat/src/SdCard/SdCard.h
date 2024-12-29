/**
 * Copyright (c) 2011-2024 Bill Greiman
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
 * \brief Top level include for SPI and SDIO cards.
 */
#ifndef SdCard_h
#define SdCard_h
#include "SdSpiCard.h"
#include "SdioCard.h"
#if HAS_SDIO_CLASS
/** Type for both SPI and SDIO cards. */
typedef SdCardInterface SdCard;
#else   // HAS_SDIO_CLASS
/** Type for SPI card. */
typedef SdSpiCard SdCard;
#endif  // HAS_SDIO_CLASS
/** Determine card configuration type.
 *
 * \param[in] cfg Card configuration.
 * \return true if SPI.
 */
inline bool isSpi(SdSpiConfig cfg) {
  (void)cfg;
  return true;
}
/** Determine card configuration type.
 *
 * \param[in] cfg Card configuration.
 * \return true if SPI.
 */
inline bool isSpi(SdioConfig cfg) {
  (void)cfg;
  return false;
}
/**
 * \class SdCardFactory
 * \brief Setup a SPI card or SDIO card.
 */
class SdCardFactory {
 public:
  /** Initialize SPI card.
   *
   * \param[in] config SPI configuration.
   * \return generic card pointer or nullptr if failure.
   */
  SdCard* newCard(SdSpiConfig config) {
    m_spiCard.begin(config);
    return &m_spiCard;
  }
  /** Initialize SDIO card.
   *
   * \param[in] config SDIO configuration.
   * \return generic card pointer or nullptr if SDIO is not supported.
   */
  SdCard* newCard(SdioConfig config) {
#if HAS_SDIO_CLASS
    m_sdioCard.begin(config);
    return &m_sdioCard;
#else   // HAS_SDIO_CLASS
    (void)config;
    return nullptr;
#endif  // HAS_SDIO_CLASS
  }

 private:
#if HAS_SDIO_CLASS
  SdioCard m_sdioCard;
#endif  // HAS_SDIO_CLASS
  SdSpiCard m_spiCard;
};
#endif  // SdCard_h
