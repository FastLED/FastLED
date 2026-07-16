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
 * \brief configuration definitions
 */
#ifndef SdFatConfig_h
#define SdFatConfig_h
// IWYU pragma: private

#include "fl/stl/int.h"  // IWYU pragma: export
#include "Arduino.h"

//
// To try UTF-8 encoded filenames.
// #define USE_UTF8_LONG_NAMES 1
//
// For minimum flash size use these settings:
// #define USE_FAT_FILE_FLAG_CONTIGUOUS 0
// #define ENABLE_DEDICATED_SPI 0
// #define USE_LONG_FILE_NAMES 0
// #define SDFAT_FILE_TYPE 1
//
// Based on SdFat 2.1.2 as shipped with Teensyduino 1.60. FastLED keeps this
// private copy limited to FAT32 reads over SPI.
//------------------------------------------------------------------------------
/** For Debug - must be one */
#define ENABLE_ARDUINO_FEATURES 1
/** For Debug - must be one */
#define ENABLE_ARDUINO_SERIAL 1
/** For Debug - must be one */
#define ENABLE_ARDUINO_STRING 1
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
/**
 * File types for SdFat, File, SdFile, SdBaseFile, fstream,
 * ifstream, and ofstream.
 *
 * Set SDFAT_FILE_TYPE to:
 *
 * 1 for FAT16/FAT32, 2 for exFAT, 3 for FAT16/FAT32 and exFAT.
 */
#define SDFAT_FILE_TYPE 1
//------------------------------------------------------------------------------
/**
 * Set USE_FAT_FILE_FLAG_CONTIGUOUS nonzero to optimize access to
 * contiguous files.  A small amount of flash is flash is used.
 */
#define USE_FAT_FILE_FLAG_CONTIGUOUS 0
//------------------------------------------------------------------------------
/**
 * Set ENABLE_DEDICATED_SPI non-zero to enable dedicated use of the SPI bus.
 * Selecting dedicated SPI in SdSpiConfig() will produce better
 * performance by using very large multi-block transfers to and
 * from the SD card.
 *
 * Enabling dedicated SPI will cost extra flash and RAM.
 */
#define ENABLE_DEDICATED_SPI 1
//------------------------------------------------------------------------------
// Driver options
/**
 * If the symbol SPI_DRIVER_SELECT is:
 *
 * 0 - An optimized custom SPI driver is used if it exists
 *     else the standard library driver is used.
 *
 * 1 - The standard library driver is always used.
 *
 * 2 - An external SPI driver of SoftSpiDriver template class is always used.
 *
 * 3 - An external SPI driver derived from SdSpiBaseClass is always used.
 */
#define SPI_DRIVER_SELECT 0
/**
 * If USE_SPI_ARRAY_TRANSFER is non-zero and the standard SPI library is
 * use, the array transfer function, transfer(buf, size), will be used.
 * This option will allocate up to a 512 byte temporary buffer for send.
 * This may be faster for some boards.  Do not use this with AVR boards.
 */
#define USE_SPI_ARRAY_TRANSFER 0
/**
 * SD maximum initialization clock rate.
 */
#define SD_MAX_INIT_RATE_KHZ 400
/**
 * Set USE_BLOCK_DEVICE_INTERFACE nonzero to use a generic block device.
 * This allow use of an external FsBlockDevice driver that is derived from
 * the FsBlockDeviceInterface like this:
 *
 * class UsbMscDriver : public FsBlockDeviceInterface {
 *   ... code for USB mass storage class driver.
 * };
 *
 * UsbMscDriver usbMsc;
 * FsVolume key;
 * ...
 *
 *   // Init USB MSC driver.
 *   if (!usbMsc.begin()) {
 *     ... handle driver init failure.
 *   }
 *   // Init FAT/exFAT volume.
 *   if (!key.begin(&usbMsc)) {
 *     ... handle FAT/exFAT failure.
 *   }
 */
#define USE_BLOCK_DEVICE_INTERFACE 1
 /**
 * SD_CHIP_SELECT_MODE defines how the functions
 * void sdCsInit(SdCsPin_t pin) {pinMode(pin, OUTPUT);}
 * and
 * void sdCsWrite(SdCsPin_t pin, bool level) {digitalWrite(pin, level);}
 * are defined.
 *
 * 0 - Internal definition is a strong symbol and can't be replaced.
 *
 * 1 - Internal definition is a weak symbol and can be replaced.
 *
 * 2 - No internal definition and must be defined in the application.
 */
#define SD_CHIP_SELECT_MODE 0
/** Type for card chip select pin. */
using SdCsPin_t = fl::u8;
//------------------------------------------------------------------------------
/**
 * Set USE_LONG_FILE_NAMES nonzero to use long file names (LFN) in FAT16/FAT32.
 * exFAT always uses long file names.
 *
 * Long File Name are limited to a maximum length of 255 characters.
 *
 * This implementation allows 7-bit characters in the range
 * 0X20 to 0X7E except the following characters are not allowed:
 *
 *  < (less than)
 *  > (greater than)
 *  : (colon)
 *  " (double quote)
 *  / (forward slash)
 *  \ (backslash)
 *  | (vertical bar or pipe)
 *  ? (question mark)
 *  * (asterisk)
 *
 */
#define USE_LONG_FILE_NAMES 1
/**
 * Set USE_UTF8_LONG_NAMES nonzero to use UTF-8 file names. Use of UTF-8 names
 * will require significantly more flash memory and a small amount of extra
 * RAM.
 *
 * UTF-8 filenames allow encoding of 1,112,064 code points in Unicode using
 * one to four one-byte (8-bit) code units.
 *
 * As of Version 13.0, the Unicode Standard defines 143,859 characters.
 *
 * getName() will return UTF-8 strings and printName() will write UTF-8 strings.
 */
#define USE_UTF8_LONG_NAMES 0

#if USE_UTF8_LONG_NAMES && !USE_LONG_FILE_NAMES
#error "USE_UTF8_LONG_NAMES requires USE_LONG_FILE_NAMES to be non-zero."
#endif  // USE_UTF8_LONG_NAMES && !USE_LONG_FILE_NAMES
//------------------------------------------------------------------------------
/**
 * Set MAINTAIN_FREE_CLUSTER_COUNT nonzero to keep the count of free clusters
 * updated.  This will increase the speed of the freeClusterCount() call
 * after the first call.  Extra flash will be required.
 */
#define MAINTAIN_FREE_CLUSTER_COUNT 0
//------------------------------------------------------------------------------
/**
 * Set the default file time stamp when a RTC callback is not used.
 * A valid date and time is required by the FAT/exFAT standard.
 *
 * The default below is YYYY-01-01 00:00:00 midnight where YYYY is
 * the compile year from the __DATE__ macro.  This is easy to recognize
 * as a placeholder for a correct date/time.
 *
 * The full compile date is:
 * FS_DATE(compileYear(), compileMonth(), compileDay())
 *
 * The full compile time is:
 * FS_TIME(compileHour(), compileMinute(), compileSecond())
 */
#define FS_DEFAULT_DATE FS_DATE(compileYear(), 1, 1)
/** 00:00:00 midnight */
#define FS_DEFAULT_TIME FS_TIME(0, 0, 0)
//------------------------------------------------------------------------------
/**
 * If CHECK_FLASH_PROGRAMMING is zero, overlap of single sector flash
 * programming and other operations will be allowed for faster write
 * performance.
 *
 * Some cards will not sleep in low power mode unless CHECK_FLASH_PROGRAMMING
 * is non-zero.
 */
#define CHECK_FLASH_PROGRAMMING 0
//------------------------------------------------------------------------------
/**
 * To enable SD card CRC checking for SPI, set USE_SD_CRC nonzero.
 *
 * Set USE_SD_CRC to 1 to use a smaller CRC-CCITT function.  This function
 * is slower for AVR but may be fast for ARM and other processors.
 *
 * Set USE_SD_CRC to 2 to used a larger table driven CRC-CCITT function.  This
 * function is faster for AVR but may be slower for ARM and other processors.
 */
#define USE_SD_CRC 0
//------------------------------------------------------------------------------
/** If the symbol USE_FCNTL_H is nonzero, open flags for access modes O_RDONLY,
 * O_WRONLY, O_RDWR and the open modifiers O_APPEND, O_CREAT, O_EXCL, O_SYNC
 * will be defined by including the system file fcntl.h.
 */
#define USE_FCNTL_H 1
//------------------------------------------------------------------------------
/**
 * Set INCLUDE_SDIOS nonzero to include sdios.h in SdFat.h.
 * sdios.h provides C++ style IO Streams.
 */
#define INCLUDE_SDIOS 0
//------------------------------------------------------------------------------
/**
 * Set FAT12_SUPPORT nonzero to enable use if FAT12 volumes.
 * FAT12 has not been well tested and requires additional flash.
 */
#define FAT12_SUPPORT 0
//------------------------------------------------------------------------------
/**
 * Set DESTRUCTOR_CLOSES_FILE nonzero to close a file in its destructor.
 *
 * Causes use of lots of heap in ARM.
 */
#define DESTRUCTOR_CLOSES_FILE 0
//------------------------------------------------------------------------------
/**
 * Call flush for endl if ENDL_CALLS_FLUSH is nonzero
 *
 * The standard for iostreams is to call flush.  This is very costly for
 * SdFat.  Each call to flush causes 2048 bytes of I/O to the SD.
 *
 * SdFat has a single 512 byte buffer for SD I/O so it must write the current
 * data sector to the SD, read the directory sector from the SD, update the
 * directory entry, write the directory sector to the SD and read the data
 * sector back into the buffer.
 *
 * The SD flash memory controller is not designed for this many rewrites
 * so performance may be reduced by more than a factor of 100.
 *
 * If ENDL_CALLS_FLUSH is zero, you must call flush and/or close to force
 * all data to be written to the SD.
 */
#define ENDL_CALLS_FLUSH 0
//------------------------------------------------------------------------------
/**
 * Set USE_SIMPLE_LITTLE_ENDIAN nonzero for little endian processors
 * with no memory alignment restrictions.
 */
#define USE_SIMPLE_LITTLE_ENDIAN 1
//------------------------------------------------------------------------------
/**
 * Set USE_SEPARATE_FAT_CACHE nonzero to use a second 512 byte cache
 * for FAT16/FAT32 table entries.  This improves performance for large
 * writes that are not a multiple of 512 bytes.
 */
#define USE_SEPARATE_FAT_CACHE 1
//------------------------------------------------------------------------------
/**
 * Set USE_EXFAT_BITMAP_CACHE nonzero to use a second 512 byte cache
 * for exFAT bitmap entries.  This improves performance for large
 * writes that are not a multiple of 512 bytes.
 */
#define USE_EXFAT_BITMAP_CACHE 1

//------------------------------------------------------------------------------
/**
 * Set USE_MULTI_SECTOR_IO nonzero to use multi-sector SD read/write.
 *
 * Don't use mult-sector read/write on small AVR boards.
 */
#define USE_MULTI_SECTOR_IO 1
#define HAS_SDIO_CLASS 0
//------------------------------------------------------------------------------
/**
 * Determine the default SPI configuration.
 */
#define SD_HAS_CUSTOM_SPI 1

#endif  // SdFatConfig_h
