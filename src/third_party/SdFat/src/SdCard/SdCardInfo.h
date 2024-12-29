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
 * \brief Definitions for SD cards.
 */
#ifndef SdCardInfo_h
#define SdCardInfo_h
#include <stdint.h>

#include "../common/SysCall.h"
// Based on the document:
//
// SD Specifications
// Part 1
// Physical Layer
// Simplified Specification
// Version 8.00
// Sep 23, 2020
//
// https://www.sdcard.org/downloads/pls/
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
// SD registers are big endian.
#error bit fields in structures assume little endian processor.
#endif  // __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
//------------------------------------------------------------------------------
// SD card errors
// See the SD Specification for command info.
/** Define error codes and brief description.  */
#define SD_ERROR_CODE_LIST                                          \
  SD_CARD_ERROR(NONE, "No error")                                   \
  SD_CARD_ERROR(CMD0, "Card reset failed")                          \
  SD_CARD_ERROR(CMD2, "SDIO read CID")                              \
  SD_CARD_ERROR(CMD3, "SDIO publish RCA")                           \
  SD_CARD_ERROR(CMD6, "Switch card function")                       \
  SD_CARD_ERROR(CMD7, "SDIO card select")                           \
  SD_CARD_ERROR(CMD8, "Send and check interface settings")          \
  SD_CARD_ERROR(CMD9, "Read CSD data")                              \
  SD_CARD_ERROR(CMD10, "Read CID data")                             \
  SD_CARD_ERROR(CMD12, "Stop multiple block transmission")          \
  SD_CARD_ERROR(CMD13, "Read card status")                          \
  SD_CARD_ERROR(CMD17, "Read single block")                         \
  SD_CARD_ERROR(CMD18, "Read multiple blocks")                      \
  SD_CARD_ERROR(CMD24, "Write single block")                        \
  SD_CARD_ERROR(CMD25, "Write multiple blocks")                     \
  SD_CARD_ERROR(CMD32, "Set first erase block")                     \
  SD_CARD_ERROR(CMD33, "Set last erase block")                      \
  SD_CARD_ERROR(CMD38, "Erase selected blocks")                     \
  SD_CARD_ERROR(CMD58, "Read OCR register")                         \
  SD_CARD_ERROR(CMD59, "Set CRC mode")                              \
  SD_CARD_ERROR(ACMD6, "Set SDIO bus width")                        \
  SD_CARD_ERROR(ACMD13, "Read extended status")                     \
  SD_CARD_ERROR(ACMD23, "Set pre-erased count")                     \
  SD_CARD_ERROR(ACMD41, "Activate card initialization")             \
  SD_CARD_ERROR(ACMD51, "Read SCR data")                            \
  SD_CARD_ERROR(READ_TOKEN, "Bad read data token")                  \
  SD_CARD_ERROR(READ_CRC, "Read CRC error")                         \
  SD_CARD_ERROR(READ_FIFO, "SDIO fifo read timeout")                \
  SD_CARD_ERROR(READ_REG, "Read CID or CSD failed.")                \
  SD_CARD_ERROR(READ_START, "Bad readStart argument")               \
  SD_CARD_ERROR(READ_TIMEOUT, "Read data timeout")                  \
  SD_CARD_ERROR(STOP_TRAN, "Multiple block stop failed")            \
  SD_CARD_ERROR(TRANSFER_COMPLETE, "SDIO transfer complete")        \
  SD_CARD_ERROR(WRITE_DATA, "Write data not accepted")              \
  SD_CARD_ERROR(WRITE_FIFO, "SDIO fifo write timeout")              \
  SD_CARD_ERROR(WRITE_START, "Bad writeStart argument")             \
  SD_CARD_ERROR(WRITE_PROGRAMMING, "Flash programming")             \
  SD_CARD_ERROR(WRITE_TIMEOUT, "Write timeout")                     \
  SD_CARD_ERROR(DMA, "DMA transfer failed")                         \
  SD_CARD_ERROR(ERASE, "Card did not accept erase commands")        \
  SD_CARD_ERROR(ERASE_SINGLE_SECTOR, "Card does not support erase") \
  SD_CARD_ERROR(ERASE_TIMEOUT, "Erase command timeout")             \
  SD_CARD_ERROR(INIT_NOT_CALLED, "Card has not been initialized")   \
  SD_CARD_ERROR(INVALID_CARD_CONFIG, "Invalid card config")         \
  SD_CARD_ERROR(FUNCTION_NOT_SUPPORTED, "Unsupported SDIO command")

enum {
/** Macro for generation of error codes using an enum. */
#define SD_CARD_ERROR(e, m) SD_CARD_ERROR_##e,
  SD_ERROR_CODE_LIST
#undef SD_CARD_ERROR
      SD_CARD_ERROR_UNKNOWN
};
/** Print the enum symbol for an error code.
 * \param[in] pr Print stream.
 * \param[in] code enum value for error.
 */
void printSdErrorSymbol(print_t* pr, uint8_t code);
/** Print text for an error code.
 * \param[in] pr Print stream.
 * \param[in] code enum value for error.
 */
void printSdErrorText(print_t* pr, uint8_t code);
//------------------------------------------------------------------------------
// card types
/** Standard capacity V1 SD card */
const uint8_t SD_CARD_TYPE_SD1 = 1;
/** Standard capacity V2 SD card */
const uint8_t SD_CARD_TYPE_SD2 = 2;
/** High Capacity SD card */
const uint8_t SD_CARD_TYPE_SDHC = 3;
//------------------------------------------------------------------------------
// SD operation timeouts
/** command timeout ms */
const uint16_t SD_CMD_TIMEOUT = 300;
/** erase timeout ms */
const uint16_t SD_ERASE_TIMEOUT = 10000;
/** init timeout ms */
const uint16_t SD_INIT_TIMEOUT = 2000;
/** read timeout ms */
const uint16_t SD_READ_TIMEOUT = 300;
/** write time out ms */
const uint16_t SD_WRITE_TIMEOUT = 600;
//------------------------------------------------------------------------------
// SD card commands
/** GO_IDLE_STATE - init card in spi mode if CS low */
const uint8_t CMD0 = 0X00;
/** ALL_SEND_CID - Asks any card to send the CID. */
const uint8_t CMD2 = 0X02;
/** SEND_RELATIVE_ADDR - Ask the card to publish a new RCA. */
const uint8_t CMD3 = 0X03;
/** SWITCH_FUNC - Switch Function Command */
const uint8_t CMD6 = 0X06;
/** SELECT/DESELECT_CARD - toggles between the stand-by and transfer states. */
const uint8_t CMD7 = 0X07;
/** SEND_IF_COND - verify SD Memory Card interface operating condition.*/
const uint8_t CMD8 = 0X08;
/** SEND_CSD - read the Card Specific Data (CSD register) */
const uint8_t CMD9 = 0X09;
/** SEND_CID - read the card identification information (CID register) */
const uint8_t CMD10 = 0X0A;
/** VOLTAGE_SWITCH -Switch to 1.8V bus signaling level. */
const uint8_t CMD11 = 0X0B;
/** STOP_TRANSMISSION - end multiple sector read sequence */
const uint8_t CMD12 = 0X0C;
/** SEND_STATUS - read the card status register */
const uint8_t CMD13 = 0X0D;
/** READ_SINGLE_SECTOR - read a single data sector from the card */
const uint8_t CMD17 = 0X11;
/** READ_MULTIPLE_SECTOR - read multiple data sectors from the card */
const uint8_t CMD18 = 0X12;
/** WRITE_SECTOR - write a single data sector to the card */
const uint8_t CMD24 = 0X18;
/** WRITE_MULTIPLE_SECTOR - write sectors of data until a STOP_TRANSMISSION */
const uint8_t CMD25 = 0X19;
/** ERASE_WR_BLK_START - sets the address of the first sector to be erased */
const uint8_t CMD32 = 0X20;
/** ERASE_WR_BLK_END - sets the address of the last sector of the continuous
    range to be erased*/
const uint8_t CMD33 = 0X21;
/** ERASE - erase all previously selected sectors */
const uint8_t CMD38 = 0X26;
/** APP_CMD - escape for application specific command */
const uint8_t CMD55 = 0X37;
/** READ_OCR - read the OCR register of a card */
const uint8_t CMD58 = 0X3A;
/** CRC_ON_OFF - enable or disable CRC checking */
const uint8_t CMD59 = 0X3B;
/** SET_BUS_WIDTH - Defines the data bus width for data transfer. */
const uint8_t ACMD6 = 0X06;
/** SD_STATUS - Send the SD Status. */
const uint8_t ACMD13 = 0X0D;
/** SET_WR_BLK_ERASE_COUNT - Set the number of write sectors to be
     pre-erased before writing */
const uint8_t ACMD23 = 0X17;
/** SD_SEND_OP_COMD - Sends host capacity support information and
    activates the card's initialization process */
const uint8_t ACMD41 = 0X29;
/** Reads the SD Configuration Register (SCR). */
const uint8_t ACMD51 = 0X33;
//==============================================================================
// CARD_STATUS
/** The command's argument was out of the allowed range for this card. */
const uint32_t CARD_STATUS_OUT_OF_RANGE = 1UL << 31;
/** A misaligned address which did not match the sector length. */
const uint32_t CARD_STATUS_ADDRESS_ERROR = 1UL << 30;
/** The transferred sector length is not allowed for this card. */
const uint32_t CARD_STATUS_SECTOR_LEN_ERROR = 1UL << 29;
/** An error in the sequence of erase commands occurred. */
const uint32_t CARD_STATUS_ERASE_SEQ_ERROR = 1UL << 28;
/** An invalid selection of write-sectors for erase occurred. */
const uint32_t CARD_STATUS_ERASE_PARAM = 1UL << 27;
/** Set when the host attempts to write to a protected sector. */
const uint32_t CARD_STATUS_WP_VIOLATION = 1UL << 26;
/** When set, signals that the card is locked by the host. */
const uint32_t CARD_STATUS_CARD_IS_LOCKED = 1UL << 25;
/** Set when a sequence or password error has been detected. */
const uint32_t CARD_STATUS_LOCK_UNLOCK_FAILED = 1UL << 24;
/** The CRC check of the previous command failed. */
const uint32_t CARD_STATUS_COM_CRC_ERROR = 1UL << 23;
/** Command not legal for the card state. */
const uint32_t CARD_STATUS_ILLEGAL_COMMAND = 1UL << 22;
/** Card internal ECC was applied but failed to correct the data. */
const uint32_t CARD_STATUS_CARD_ECC_FAILED = 1UL << 21;
/** Internal card controller error */
const uint32_t CARD_STATUS_CC_ERROR = 1UL << 20;
/** A general or an unknown error occurred during the operation. */
const uint32_t CARD_STATUS_ERROR = 1UL << 19;
// bits 19, 18, and 17 reserved.
/** Permanent WP set or attempt to change read only values of  CSD. */
const uint32_t CARD_STATUS_CSD_OVERWRITE = 1UL << 16;
/** partial address space was erased due to write protect. */
const uint32_t CARD_STATUS_WP_ERASE_SKIP = 1UL << 15;
/** The command has been executed without using the internal ECC. */
const uint32_t CARD_STATUS_CARD_ECC_DISABLED = 1UL << 14;
/** out of erase sequence command was received. */
const uint32_t CARD_STATUS_ERASE_RESET = 1UL << 13;
/** The state of the card when receiving the command.
 * 0 = idle
 * 1 = ready
 * 2 = ident
 * 3 = stby
 * 4 = tran
 * 5 = data
 * 6 = rcv
 * 7 = prg
 * 8 = dis
 * 9-14 = reserved
 * 15 = reserved for I/O mode
 */
const uint32_t CARD_STATUS_CURRENT_STATE = 0XF << 9;
/** Shift for current state. */
const uint32_t CARD_STATUS_CURRENT_STATE_SHIFT = 9;
/** Corresponds to buffer empty signaling on the bus. */
const uint32_t CARD_STATUS_READY_FOR_DATA = 1UL << 8;
// bit 7 reserved.
/** Extension Functions may set this bit to get host to deal with events. */
const uint32_t CARD_STATUS_FX_EVENT = 1UL << 6;
/** The card will expect ACMD, or the command has been interpreted as ACMD */
const uint32_t CARD_STATUS_APP_CMD = 1UL << 5;
// bit 4 reserved.
/** Error in the sequence of the authentication process. */
const uint32_t CARD_STATUS_AKE_SEQ_ERROR = 1UL << 3;
// bits 2,1, and 0 reserved for manufacturer test mode.
//==============================================================================
/** status for card in the ready state */
const uint8_t R1_READY_STATE = 0X00;
/** status for card in the idle state */
const uint8_t R1_IDLE_STATE = 0X01;
/** status bit for illegal command */
const uint8_t R1_ILLEGAL_COMMAND = 0X04;
/** start data token for read or write single sector*/
const uint8_t DATA_START_SECTOR = 0XFE;
/** stop token for write multiple sectors*/
const uint8_t STOP_TRAN_TOKEN = 0XFD;
/** start data token for write multiple sectors*/
const uint8_t WRITE_MULTIPLE_TOKEN = 0XFC;
/** mask for data response tokens after a write sector operation */
const uint8_t DATA_RES_MASK = 0X1F;
/** write data accepted token */
const uint8_t DATA_RES_ACCEPTED = 0X05;
//==============================================================================
/**
 * \class cid_t
 * \brief Card Identification (CID) register.
 */
struct cid_t {
  // byte 0
  /** Manufacturer ID */
  uint8_t mid;
  // byte 1-2
  /** OEM/Application ID. */
  char oid[2];
  // byte 3-7
  /** Product name. */
  char pnm[5];
  // byte 8
  /** Product revision - n.m two 4-bit nibbles. */
  uint8_t prv;
  // byte 9-12
  /** Product serial 32-bit number Big Endian format. */
  uint8_t psn8[4];
  // byte 13-14
  /** Manufacturing date big endian - four nibbles RYYM Reserved Year Month. */
  uint8_t mdt[2];
  // byte 15
  /** CRC7 bits 1-7 checksum, bit 0 always 1 */
  uint8_t crc;
  // Extract big endian fields.
  /** \return major revision number. */
  int prvN() const { return prv >> 4; }
  /** \return minor revision number. */
  int prvM() const { return prv & 0XF; }
  /** \return Manufacturing Year. */
  int mdtYear() const { return 2000 + ((mdt[0] & 0XF) << 4) + (mdt[1] >> 4); }
  /** \return Manufacturing Month. */
  int mdtMonth() const { return mdt[1] & 0XF; }
  /** \return Product Serial Number. */
  uint32_t psn() const {
    return (uint32_t)psn8[0] << 24 | (uint32_t)psn8[1] << 16 |
           (uint32_t)psn8[2] << 8 | (uint32_t)psn8[3];
  }
} __attribute__((packed));
//==============================================================================
/**
 * \class csd_t
 * \brief Union of old and new style CSD register.
 */
struct csd_t {
  /** union of all CSD versions */
  uint8_t csd[16];
  // Extract big endian fields.
  /** \return Capacity in sectors */
  uint32_t capacity() const {
    uint32_t c_size;
    uint8_t ver = csd[0] >> 6;
    if (ver == 0) {
      c_size = (uint32_t)(csd[6] & 3) << 10;
      c_size |= (uint32_t)csd[7] << 2 | csd[8] >> 6;
      uint8_t c_size_mult = (csd[9] & 3) << 1 | csd[10] >> 7;
      uint8_t read_bl_len = csd[5] & 15;
      return (c_size + 1) << (c_size_mult + read_bl_len + 2 - 9);
    } else if (ver == 1) {
      c_size = (uint32_t)(csd[7] & 63) << 16;
      c_size |= (uint32_t)csd[8] << 8;
      c_size |= csd[9];
      return (c_size + 1) << 10;
    } else {
      return 0;
    }
  }
  /** \return true if erase granularity is single block. */
  bool eraseSingleBlock() const { return csd[10] & 0X40; }
  /** \return erase size in 512 byte blocks if eraseSingleBlock is false. */
  int eraseSize() const { return ((csd[10] & 0X3F) << 1 | csd[11] >> 7) + 1; }
  /** \return true if the contents is copied or true if original. */
  bool copy() const { return csd[14] & 0X40; }
  /** \return true if the entire card is permanently write protected. */
  bool permWriteProtect() const { return csd[14] & 0X20; }
  /** \return true if the entire card is temporarily write protected. */
  bool tempWriteProtect() const { return csd[14] & 0X10; }
};
//==============================================================================
/**
 * \class scr_t
 * \brief SCR register.
 */
struct scr_t {
  /** Bytes 0-3 SD Association, bytes 4-7 reserved for manufacturer. */
  uint8_t scr[8];
  /** \return SCR_STRUCTURE field  - must be zero.*/
  uint8_t srcStructure() const { return scr[0] >> 4; }
  /** \return SD_SPEC field 0 - v1.0 or V1.01, 1 - 1.10, 2 - V2.00 or greater */
  uint8_t sdSpec() const { return scr[0] & 0XF; }
  /** \return false if all zero, true if all one. */
  bool dataAfterErase() const { return scr[1] & 0X80; }
  /** \return CPRM Security Version. */
  uint8_t sdSecurity() const { return (scr[1] >> 4) & 0X7; }
  /** \return 0101b.  */
  uint8_t sdBusWidths() const { return scr[1] & 0XF; }
  /** \return true if V3.0 or greater. */
  bool sdSpec3() const { return scr[2] & 0X80; }
  /** \return if true and sdSpecX is zero V4.xx. */
  bool sdSpec4() const { return scr[2] & 0X4; }
  /** \return nonzero for version 5 or greater if sdSpec == 2,
              sdSpec3 == true. Version is return plus four.*/
  uint8_t sdSpecX() const { return (scr[2] & 0X3) << 2 | scr[3] >> 6; }
  /** \return bit map for support CMD58/59, CMD48/49, CMD23, and CMD20 */
  uint8_t cmdSupport() const { return scr[3] & 0XF; }
  /** \return SD spec version */
  int16_t sdSpecVer() const {
    if (sdSpec() > 2) {
      return -1;
    } else if (sdSpec() < 2) {
      return sdSpec() ? 110 : 101;
    } else if (!sdSpec3()) {
      return 200;
    } else if (!sdSpec4() && !sdSpecX()) {
      return 300;
    }
    return 400 + 100 * sdSpecX();
  }
};
//==============================================================================
/**
 * \class sds_t
 * \brief SD Status.
 */
// fields are big endian
struct sds_t {
  /** byte 0, bit 7-6 width, bit 5 secured mode, bits 4-0 reserved. */
  uint8_t busWidthSecureMode;
  /** byte 1 reserved */
  uint8_t reserved1;
  /** byte 2-3  zero for SD rd/wr memory card. */
  uint8_t sdCardType[2];
  /** byte 4-7  size of protected area big endian */
  uint8_t sizeOfProtectedArea[4];
  /** byte 8 speed class. */
  uint8_t speed;
  /** byte 9 performance move */
  uint8_t performanceMove;
  /** byte 10 AU size code. */
  uint8_t auSize;
  /** byte 11-12 erase size big endian */
  uint8_t eraseSize[2];
  /** byte 13 erase timeout and erase offset */
  uint8_t eraseTimeoutOffset;
  /** byte 14 */
  uint8_t uhsClassAuSize;
  /** byte 15 */
  uint8_t videoSpeedClass;
  /** byte 16-17 */
  uint8_t vscAuSize[2];
  /** byte 18-21 */
  uint8_t susAddr[3];
  /** byte 21 */
  uint8_t appPerfClass;
  /** byte 22 */
  uint8_t perfEnhance;
  /** byte 23 */
  uint8_t discardFule;
  /** byte 24 */
  uint8_t reservedManufacturer[40];

  /** \return appClass. */
  int appClass() { return appPerfClass; }
  /** \return AU size in KB. or zero for error. */
  uint32_t auSizeKB() {
    // 0XF mask and uint16_t array helps compiler optimize size on Uno.
    uint8_t val = (auSize >> 4) & 0XF;
    static const uint16_t au[] = {0,    16,    32,    64,    128,
                                  256,  512,   1024,  2048,  4096,
                                  8192, 12288, 16384, 24576, 32768};
    return val < 0XF ? au[val] : 65536UL;
  }
  /** \return current bus width or -1 for error. */
  uint8_t busWidth() const {
    uint8_t w = busWidthSecureMode >> 6;
    return w == 2 ? 4 : w == 0 ? 1 : -1;
  }
  /** \return true is discard operation is supported else true. */
  bool discard() const { return discardFule & 2; }
  /** \return eraseSize in AUs. */
  uint16_t eraseSizeAU() const {
    return (uint16_t)eraseSize[0] << 8 | (uint16_t)eraseSize[1];
  }
  /** \return eraseTimeout seconds. */
  uint8_t eraseTimeout() const { return eraseTimeoutOffset >> 2; }
  /** \return eraseOffset seconds. */
  uint8_t eraseOffset() const { return eraseTimeoutOffset & 3; }
  /** \return true if full user logical erase is supported else false. */
  bool fule() const { return discardFule & 1; }
  /** \return true for secure mode else false. */
  bool secureMode() const { return busWidthSecureMode & 0X20; }
  /** \return speed class or -1 for error. */
  int speedClass() const {
    return speed < 4 ? 2 * speed : speed == 4 ? 10 : -1;
  }
  /** \return UHS Speed Grade. */
  int uhsClass() const { return uhsClassAuSize >> 4; }
  /** \return Video Speed */
  int videoClass() { return videoSpeedClass; }
};
#endif  // SdCardInfo_h
