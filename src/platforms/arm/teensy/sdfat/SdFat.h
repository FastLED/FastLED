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
 * \brief Private FAT32-only root header derived from SdFat 2.1.2.
 *
 * This is the FastLED Teensy subset. It intentionally exposes only the SPI
 * card and FAT32 volume needed by the read-only filesystem adapter.
 */
#ifndef SdFat_h
#define SdFat_h

/** This copy of SdFat has special modifications for Teensy. */
#define SD_FAT_TEENSY_MODIFIED 1

// IWYU pragma: begin_keep
#include "common/SysCall.h"
#include "common/ArduinoFiles.h"
#include "SdCard/SdSpiCard.h"
#include "FatLib/FatVolume.h"
#include "fl/stl/int.h"
// IWYU pragma: end_keep

/** SdFat version for cpp use. */
#define SD_FAT_VERSION 20102
/** SdFat version as string. */
#define SD_FAT_VERSION_STR "2.1.2"

/** FAT32 SdFat facade backed by the private SPI card implementation. */
namespace fl { namespace platforms { namespace teensy { namespace sdfat {

class SdFat32 : public FatVolume {
 public:
  bool begin(SdCsPin_t csPin = SS) {
    return begin(SdSpiConfig(csPin, SHARED_SPI));
  }

  bool begin(SdCsPin_t csPin, fl::u32 maxSck) {
    return begin(SdSpiConfig(csPin, SHARED_SPI, maxSck));
  }

  bool begin(SdSpiConfig spiConfig) {
    return mCard.begin(spiConfig) && FatVolume::begin(&mCard);
  }

  void end() {
    FatVolume::end();
    mCard.end();
  }

  SdSpiCard* card() { return &mCard; }

 private:
  SdSpiCard mCard;
};

/** SdFat is the FAT32-only public type in this private subset. */
typedef SdFat32 SdFat;
/** SdBaseFile is the FAT32 base file type. */
typedef FatFile SdBaseFile;
/** File is the FAT32 stream file type. */
typedef File32 File;

/** FAT32 file with Arduino Print support. */
class SdFile : public PrintFile<SdBaseFile> {
 public:
  SdFile() {}

  explicit SdFile(const char* path, oflag_t oflag) {
    open(path, oflag);
  }

  static void dateTimeCallback(
      void (*dateTime)(fl::u16* date, fl::u16* time)) {
    FsDateTime::setCallback(dateTime);
  }

  static void dateTimeCallbackCancel() {
    FsDateTime::clearCallback();
  }
};

} } } } // namespace fl::platforms::teensy::sdfat

#endif  // SdFat_h
