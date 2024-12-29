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
#include "SdSpiCard.h"
//==============================================================================
class Timeout {
 public:
  Timeout() {}
  explicit Timeout(uint16_t ms) { set(ms); }
  uint16_t millis16() { return millis(); }
  void set(uint16_t ms) { m_endTime = ms + millis16(); }
  bool timedOut() { return (int16_t)(m_endTime - millis16()) < 0; }

 private:
  uint16_t m_endTime;
};
//==============================================================================
#if USE_SD_CRC
// CRC functions
//------------------------------------------------------------------------------
static uint8_t CRC7(const uint8_t* data, uint8_t n) {
  uint8_t crc = 0;
  for (uint8_t i = 0; i < n; i++) {
    uint8_t d = data[i];
    for (uint8_t j = 0; j < 8; j++) {
      crc <<= 1;
      if ((d & 0x80) ^ (crc & 0x80)) {
        crc ^= 0x09;
      }
      d <<= 1;
    }
  }
  return (crc << 1) | 1;
}
//------------------------------------------------------------------------------
#if USE_SD_CRC == 1
// Shift based CRC-CCITT
// uses the x^16,x^12,x^5,x^1 polynomial.
static uint16_t CRC_CCITT(const uint8_t* data, size_t n) {
  uint16_t crc = 0;
  for (size_t i = 0; i < n; i++) {
    crc = (uint8_t)(crc >> 8) | (crc << 8);
    crc ^= data[i];
    crc ^= (uint8_t)(crc & 0xff) >> 4;
    crc ^= crc << 12;
    crc ^= (crc & 0xff) << 5;
  }
  return crc;
}
#elif USE_SD_CRC > 1  // CRC_CCITT
//------------------------------------------------------------------------------
// Table based CRC-CCITT
// uses the x^16,x^12,x^5,x^1 polynomial.
#ifdef __AVR__
static const uint16_t crctab[] PROGMEM = {
#else   // __AVR__
static const uint16_t crctab[] = {
#endif  // __AVR__
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7, 0x8108,
    0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF, 0x1231, 0x0210,
    0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6, 0x9339, 0x8318, 0xB37B,
    0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE, 0x2462, 0x3443, 0x0420, 0x1401,
    0x64E6, 0x74C7, 0x44A4, 0x5485, 0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE,
    0xF5CF, 0xC5AC, 0xD58D, 0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6,
    0x5695, 0x46B4, 0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D,
    0xC7BC, 0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B, 0x5AF5,
    0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12, 0xDBFD, 0xCBDC,
    0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A, 0x6CA6, 0x7C87, 0x4CE4,
    0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41, 0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD,
    0xAD2A, 0xBD0B, 0x8D68, 0x9D49, 0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13,
    0x2E32, 0x1E51, 0x0E70, 0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A,
    0x9F59, 0x8F78, 0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E,
    0xE16F, 0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E, 0x02B1,
    0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256, 0xB5EA, 0xA5CB,
    0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D, 0x34E2, 0x24C3, 0x14A0,
    0x0481, 0x7466, 0x6447, 0x5424, 0x4405, 0xA7DB, 0xB7FA, 0x8799, 0x97B8,
    0xE75F, 0xF77E, 0xC71D, 0xD73C, 0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657,
    0x7676, 0x4615, 0x5634, 0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9,
    0xB98A, 0xA9AB, 0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882,
    0x28A3, 0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92, 0xFD2E,
    0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9, 0x7C26, 0x6C07,
    0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1, 0xEF1F, 0xFF3E, 0xCF5D,
    0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8, 0x6E17, 0x7E36, 0x4E55, 0x5E74,
    0x2E93, 0x3EB2, 0x0ED1, 0x1EF0};
static uint16_t CRC_CCITT(const uint8_t* data, size_t n) {
  uint16_t crc = 0;
  for (size_t i = 0; i < n; i++) {
#ifdef __AVR__
    crc = pgm_read_word(&crctab[(crc >> 8 ^ data[i]) & 0XFF]) ^ (crc << 8);
#else   // __AVR__
    crc = crctab[(crc >> 8 ^ data[i]) & 0XFF] ^ (crc << 8);
#endif  // __AVR__
  }
  return crc;
}
#endif  // CRC_CCITT
#endif  // USE_SD_CRC
//==============================================================================
// SharedSpiCard member functions
//------------------------------------------------------------------------------
bool SharedSpiCard::begin(SdSpiConfig spiConfig) {
  uint8_t cardType;
  uint32_t arg;
  Timeout timeout;
  // Restore state to creator.
  initSharedSpiCard();
  m_errorCode = SD_CARD_ERROR_NONE;
  m_csPin = spiConfig.csPin;
#if SPI_DRIVER_SELECT >= 2
  m_spiDriverPtr = spiConfig.spiPort;
  if (!m_spiDriverPtr) {
    error(SD_CARD_ERROR_INVALID_CARD_CONFIG);
    goto fail;
  }
#endif  // SPI_DRIVER_SELECT
  sdCsInit(m_csPin);
  spiUnselect();
  spiSetSckSpeed(1000UL * SD_MAX_INIT_RATE_KHZ);
  spiBegin(spiConfig);
  m_beginCalled = true;

  spiStart();

  // must supply min of 74 clock cycles with CS high.
  spiUnselect();
  for (uint8_t i = 0; i < 10; i++) {
    spiReceive();
  }
  spiSelect();
  timeout.set(SD_INIT_TIMEOUT);
  while (true) {
    // command to go idle in SPI mode
    if (cardCommand(CMD0, 0) == R1_IDLE_STATE) {
      break;
    }
    if (timeout.timedOut()) {
      error(SD_CARD_ERROR_CMD0);
      goto fail;
    }
  }
#if USE_SD_CRC
  if (cardCommand(CMD59, 1) != R1_IDLE_STATE) {
    error(SD_CARD_ERROR_CMD59);
    goto fail;
  }
#endif  // USE_SD_CRC
  // check SD version
  while (true) {
    if (cardCommand(CMD8, 0x1AA) & R1_ILLEGAL_COMMAND) {
      cardType = SD_CARD_TYPE_SD1;
      break;
    }
    // Skip first three bytes.
    for (uint8_t i = 0; i < 4; i++) {
      m_status = spiReceive();
    }
    if (m_status == 0XAA) {
      cardType = SD_CARD_TYPE_SD2;
      break;
    }
    if (timeout.timedOut()) {
      error(SD_CARD_ERROR_CMD8);
      goto fail;
    }
  }
  // initialize card and send host supports SDHC if SD2
  arg = cardType == SD_CARD_TYPE_SD2 ? 0X40000000 : 0;
  while (cardAcmd(ACMD41, arg) != R1_READY_STATE) {
    // check for timeout
    if (timeout.timedOut()) {
      error(SD_CARD_ERROR_ACMD41);
      goto fail;
    }
  }
  // if SD2 read OCR register to check for SDHC card
  if (cardType == SD_CARD_TYPE_SD2) {
    if (cardCommand(CMD58, 0)) {
      error(SD_CARD_ERROR_CMD58);
      goto fail;
    }
    if ((spiReceive() & 0XC0) == 0XC0) {
      cardType = SD_CARD_TYPE_SDHC;
    }
    // Discard rest of ocr - contains allowed voltage range.
    for (uint8_t i = 0; i < 3; i++) {
      spiReceive();
    }
  }
  spiStop();
  spiSetSckSpeed(spiConfig.maxSck);
  m_type = cardType;
  return true;

fail:
  spiStop();
  return false;
}
//------------------------------------------------------------------------------
bool SharedSpiCard::cardCMD6(uint32_t arg, uint8_t* status) {
  if (cardCommand(CMD6, arg)) {
    error(SD_CARD_ERROR_CMD6);
    goto fail;
  }
  if (!readData(status, 64)) {
    goto fail;
  }
  spiStop();
  return true;

fail:
  spiStop();
  return false;
}
//------------------------------------------------------------------------------
// send command and return error code.  Return zero for OK
uint8_t SharedSpiCard::cardCommand(uint8_t cmd, uint32_t arg) {
  if (!syncDevice()) {
    return 0XFF;
  }
  // select card
  if (!m_spiActive) {
    spiStart();
  }
  if (cmd != CMD0 && cmd != CMD12 && !waitReady(SD_CMD_TIMEOUT)) {
    return 0XFF;
  }
#if USE_SD_CRC
  // form message
  uint8_t buf[6];
  buf[0] = (uint8_t)0x40U | cmd;
  buf[1] = (uint8_t)(arg >> 24U);
  buf[2] = (uint8_t)(arg >> 16U);
  buf[3] = (uint8_t)(arg >> 8U);
  buf[4] = (uint8_t)arg;

  // add CRC
  buf[5] = CRC7(buf, 5);

  // send message
  spiSend(buf, 6);
#else   // USE_SD_CRC
  // send command
  spiSend(cmd | 0x40);

  // send argument
  uint8_t* pa = reinterpret_cast<uint8_t*>(&arg);
  for (int8_t i = 3; i >= 0; i--) {
    spiSend(pa[i]);
  }

  // send CRC - correct for CMD0 with arg zero or CMD8 with arg 0X1AA
  spiSend(cmd == CMD0 ? 0X95 : 0X87);
#endif  // USE_SD_CRC

  // discard first fill byte to avoid MISO pull-up problem.
  spiReceive();

  // there are 1-8 fill bytes before response.  fill bytes should be 0XFF.
  uint8_t n = 0;
  do {
    m_status = spiReceive();
  } while (m_status & 0X80 && ++n < 10);
  return m_status;
}
//------------------------------------------------------------------------------
void SharedSpiCard::end() {
  if (m_beginCalled) {
    syncDevice();
    spiEnd();
    m_beginCalled = false;
  }
}
//------------------------------------------------------------------------------
bool SharedSpiCard::erase(uint32_t firstSector, uint32_t lastSector) {
  csd_t csd;
  if (!readCSD(&csd)) {
    goto fail;
  }
  // check for single sector erase
  if (!csd.eraseSingleBlock()) {
    // erase size mask
    uint8_t m = csd.eraseSize() - 1;
    if ((firstSector & m) != 0 || ((lastSector + 1) & m) != 0) {
      // error card can't erase specified area
      error(SD_CARD_ERROR_ERASE_SINGLE_SECTOR);
      goto fail;
    }
  }
  if (type() != SD_CARD_TYPE_SDHC) {
    firstSector <<= 9;
    lastSector <<= 9;
  }
  if (cardCommand(CMD32, firstSector) || cardCommand(CMD33, lastSector) ||
      cardCommand(CMD38, 0)) {
    error(SD_CARD_ERROR_ERASE);
    goto fail;
  }
  if (!waitReady(SD_ERASE_TIMEOUT)) {
    error(SD_CARD_ERROR_ERASE_TIMEOUT);
    goto fail;
  }
  spiStop();
  return true;

fail:
  spiStop();
  return false;
}
//------------------------------------------------------------------------------
bool SharedSpiCard::eraseSingleSectorEnable() {
  csd_t csd;
  return readCSD(&csd) ? csd.eraseSingleBlock() : false;
}
//------------------------------------------------------------------------------
bool SharedSpiCard::isBusy() {
  if (m_state == READ_STATE) {
    return false;
  }
  bool spiActive = m_spiActive;
  if (!spiActive) {
    spiStart();
  }
  bool rtn = 0XFF != spiReceive();
  if (!spiActive) {
    spiStop();
  }
  return rtn;
}
//------------------------------------------------------------------------------
bool SharedSpiCard::readData(uint8_t* dst) { return readData(dst, 512); }
//------------------------------------------------------------------------------
bool SharedSpiCard::readData(uint8_t* dst, size_t count) {
#if USE_SD_CRC
  uint16_t crc;
#endif  // USE_SD_CRC

  // wait for start sector token
  Timeout timeout(SD_READ_TIMEOUT);
  while ((m_status = spiReceive()) == 0XFF) {
    if (timeout.timedOut()) {
      error(SD_CARD_ERROR_READ_TIMEOUT);
      goto fail;
    }
  }
  if (m_status != DATA_START_SECTOR) {
    error(SD_CARD_ERROR_READ_TOKEN);
    goto fail;
  }
  // transfer data
  if ((m_status = spiReceive(dst, count))) {
    error(SD_CARD_ERROR_DMA);
    goto fail;
  }

#if USE_SD_CRC
  // get crc
  crc = (spiReceive() << 8) | spiReceive();
  if (crc != CRC_CCITT(dst, count)) {
    error(SD_CARD_ERROR_READ_CRC);
    goto fail;
  }
#else   // USE_SD_CRC
  // discard crc
  spiReceive();
  spiReceive();
#endif  // USE_SD_CRC
  return true;

fail:
  spiStop();
  return false;
}
//------------------------------------------------------------------------------
bool SharedSpiCard::readOCR(uint32_t* ocr) {
  uint8_t* p = reinterpret_cast<uint8_t*>(ocr);
  if (cardCommand(CMD58, 0)) {
    error(SD_CARD_ERROR_CMD58);
    goto fail;
  }
  for (uint8_t i = 0; i < 4; i++) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    p[3 - i] = spiReceive();
#else   // __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    p[i] = spiReceive();
#endif  // __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  }
  spiStop();
  return true;

fail:
  spiStop();
  return false;
}
//------------------------------------------------------------------------------
/** read CID or CSR register */
bool SharedSpiCard::readRegister(uint8_t cmd, void* buf) {
  uint8_t* dst = reinterpret_cast<uint8_t*>(buf);
  if (cardCommand(cmd, 0)) {
    error(SD_CARD_ERROR_READ_REG);
    goto fail;
  }
  if (!readData(dst, 16)) {
    goto fail;
  }
  spiStop();
  return true;

fail:
  spiStop();
  return false;
}
//------------------------------------------------------------------------------
bool SharedSpiCard::readSCR(scr_t* scr) {
  uint8_t* dst = reinterpret_cast<uint8_t*>(scr);
  if (cardAcmd(ACMD51, 0)) {
    error(SD_CARD_ERROR_ACMD51);
    goto fail;
  }
  if (!readData(dst, sizeof(scr_t))) {
    goto fail;
  }
  spiStop();
  return true;

fail:
  spiStop();
  return false;
}
//------------------------------------------------------------------------------
bool SharedSpiCard::readSector(uint32_t sector, uint8_t* dst) {
  // use address if not SDHC card
  if (type() != SD_CARD_TYPE_SDHC) {
    sector <<= 9;
  }
  if (cardCommand(CMD17, sector)) {
    error(SD_CARD_ERROR_CMD17);
    goto fail;
  }
  if (!readData(dst, 512)) {
    goto fail;
  }
  spiStop();
  return true;

fail:
  spiStop();
  return false;
}
//------------------------------------------------------------------------------
bool SharedSpiCard::readSectors(uint32_t sector, uint8_t* dst, size_t ns) {
  if (!readStart(sector)) {
    goto fail;
  }
  for (size_t i = 0; i < ns; i++, dst += 512) {
    if (!readData(dst, 512)) {
      goto fail;
    }
  }
  return readStop();
fail:
  return false;
}
//------------------------------------------------------------------------------
bool SharedSpiCard::readStart(uint32_t sector) {
  if (type() != SD_CARD_TYPE_SDHC) {
    sector <<= 9;
  }
  if (cardCommand(CMD18, sector)) {
    error(SD_CARD_ERROR_CMD18);
    goto fail;
  }
  m_state = READ_STATE;
  return true;

fail:
  spiStop();
  return false;
}
//------------------------------------------------------------------------------
bool SharedSpiCard::readSDS(sds_t* sds) {
  uint8_t* dst = reinterpret_cast<uint8_t*>(sds);
  // retrun is R2 so read extra status byte.
  if (cardAcmd(ACMD13, 0) || spiReceive()) {
    error(SD_CARD_ERROR_ACMD13);
    goto fail;
  }
  if (!readData(dst, sizeof(sds_t))) {
    goto fail;
  }
  spiStop();
  return true;

fail:
  spiStop();
  return false;
}
//------------------------------------------------------------------------------
bool SharedSpiCard::readStop() {
  m_state = IDLE_STATE;
  if (cardCommand(CMD12, 0)) {
    error(SD_CARD_ERROR_CMD12);
    goto fail;
  }
  spiStop();
  return true;

fail:
  spiStop();
  return false;
}
//------------------------------------------------------------------------------
uint32_t SharedSpiCard::sectorCount() {
  csd_t csd;
  return readCSD(&csd) ? csd.capacity() : 0;
}
//------------------------------------------------------------------------------
void SharedSpiCard::spiStart() {
  SPI_ASSERT_NOT_ACTIVE;
  if (!m_spiActive) {
    spiActivate();
    m_spiActive = true;
    spiSelect();
    // Dummy byte to drive MISO busy status.
    spiSend(0XFF);
  }
}
//------------------------------------------------------------------------------
void SharedSpiCard::spiStop() {
  SPI_ASSERT_ACTIVE;
  if (m_spiActive) {
    spiUnselect();
    // Insure MISO goes to low Z.
    spiSend(0XFF);
    spiDeactivate();
    m_spiActive = false;
  }
}
//------------------------------------------------------------------------------
bool SharedSpiCard::syncDevice() {
  if (m_state == WRITE_STATE) {
    return writeStop();
  }
  if (m_state == READ_STATE) {
    return readStop();
  }
  return true;
}
//------------------------------------------------------------------------------
bool SharedSpiCard::waitReady(uint16_t ms) {
  Timeout timeout(ms);
  while (spiReceive() != 0XFF) {
    if (timeout.timedOut()) {
      return false;
    }
  }
  return true;
}
//------------------------------------------------------------------------------
bool SharedSpiCard::writeData(const uint8_t* src) {
  // wait for previous write to finish
  if (!waitReady(SD_WRITE_TIMEOUT)) {
    error(SD_CARD_ERROR_WRITE_TIMEOUT);
    goto fail;
  }
  if (!writeData(WRITE_MULTIPLE_TOKEN, src)) {
    goto fail;
  }
  return true;

fail:
  spiStop();
  return false;
}
//------------------------------------------------------------------------------
// send one sector of data for write sector or write multiple sectors
bool SharedSpiCard::writeData(uint8_t token, const uint8_t* src) {
#if USE_SD_CRC
  uint16_t crc = CRC_CCITT(src, 512);
#else   // USE_SD_CRC
  uint16_t crc = 0XFFFF;
#endif  // USE_SD_CRC
  spiSend(token);
  spiSend(src, 512);
  spiSend(crc >> 8);
  spiSend(crc & 0XFF);

  m_status = spiReceive();
  if ((m_status & DATA_RES_MASK) != DATA_RES_ACCEPTED) {
    error(SD_CARD_ERROR_WRITE_DATA);
    goto fail;
  }
  return true;

fail:
  spiStop();
  return false;
}
//------------------------------------------------------------------------------
bool SharedSpiCard::writeSector(uint32_t sector, const uint8_t* src) {
  // use address if not SDHC card
  if (type() != SD_CARD_TYPE_SDHC) {
    sector <<= 9;
  }
  if (cardCommand(CMD24, sector)) {
    error(SD_CARD_ERROR_CMD24);
    goto fail;
  }
  if (!writeData(DATA_START_SECTOR, src)) {
    goto fail;
  }

#if CHECK_FLASH_PROGRAMMING
  // wait for flash programming to complete
  if (!waitReady(SD_WRITE_TIMEOUT)) {
    error(SD_CARD_ERROR_WRITE_PROGRAMMING);
    goto fail;
  }
  // response is r2 so get and check two bytes for nonzero
  if (cardCommand(CMD13, 0) || spiReceive()) {
    error(SD_CARD_ERROR_CMD13);
    goto fail;
  }
#endif  // CHECK_FLASH_PROGRAMMING

  spiStop();
  return true;

fail:
  spiStop();
  return false;
}
//------------------------------------------------------------------------------
bool SharedSpiCard::writeSectors(uint32_t sector, const uint8_t* src,
                                 size_t ns) {
  if (!writeStart(sector)) {
    goto fail;
  }
  for (size_t i = 0; i < ns; i++, src += 512) {
    if (!writeData(src)) {
      goto fail;
    }
  }
  return writeStop();

fail:
  spiStop();
  return false;
}
//------------------------------------------------------------------------------
bool SharedSpiCard::writeStart(uint32_t sector) {
  // use address if not SDHC card
  if (type() != SD_CARD_TYPE_SDHC) {
    sector <<= 9;
  }
  if (cardCommand(CMD25, sector)) {
    error(SD_CARD_ERROR_CMD25);
    goto fail;
  }
  m_state = WRITE_STATE;
  return true;

fail:
  spiStop();
  return false;
}
//------------------------------------------------------------------------------
bool SharedSpiCard::writeStop() {
  if (!waitReady(SD_WRITE_TIMEOUT)) {
    goto fail;
  }
  spiSend(STOP_TRAN_TOKEN);
  spiStop();
  m_state = IDLE_STATE;
  return true;

fail:
  error(SD_CARD_ERROR_STOP_TRAN);
  spiStop();
  return false;
}
//==============================================================================
bool DedicatedSpiCard::begin(SdSpiConfig spiConfig) {
  if (!SharedSpiCard::begin(spiConfig)) {
    return false;
  }
  m_dedicatedSpi = spiOptionDedicated(spiConfig.options);
  return true;
}
//------------------------------------------------------------------------------
bool DedicatedSpiCard::readSector(uint32_t sector, uint8_t* dst) {
  return readSectors(sector, dst, 1);
}
//------------------------------------------------------------------------------
bool DedicatedSpiCard::readSectors(uint32_t sector, uint8_t* dst, size_t ns) {
  if (sdState() != READ_STATE || sector != m_curSector) {
    if (!readStart(sector)) {
      goto fail;
    }
    m_curSector = sector;
  }
  for (size_t i = 0; i < ns; i++, dst += 512) {
    if (!readData(dst)) {
      goto fail;
    }
  }
  m_curSector += ns;
  return m_dedicatedSpi ? true : readStop();

fail:
  return false;
}
//------------------------------------------------------------------------------
bool DedicatedSpiCard::setDedicatedSpi(bool value) {
  if (!syncDevice()) {
    return false;
  }
  m_dedicatedSpi = value;
  return true;
}
//------------------------------------------------------------------------------
bool DedicatedSpiCard::writeSector(uint32_t sector, const uint8_t* src) {
  if (m_dedicatedSpi) {
    return writeSectors(sector, src, 1);
  }
  return SharedSpiCard::writeSector(sector, src);
}
//------------------------------------------------------------------------------
bool DedicatedSpiCard::writeSectors(uint32_t sector, const uint8_t* src,
                                    size_t ns) {
  if (sdState() != WRITE_STATE || m_curSector != sector) {
    if (!writeStart(sector)) {
      goto fail;
    }
    m_curSector = sector;
  }
  for (size_t i = 0; i < ns; i++, src += 512) {
    if (!writeData(src)) {
      goto fail;
    }
  }
  m_curSector += ns;
  return m_dedicatedSpi ? true : writeStop();

fail:
  return false;
}
