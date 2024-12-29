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
#ifndef FatFormatter_h
#define FatFormatter_h
#include "../common/FsBlockDevice.h"
#include "../common/SysCall.h"
/**
 * \class FatFormatter
 * \brief Format a FAT volume.
 */
class FatFormatter {
 public:
  /** Constructor. */
  FatFormatter() = default;
  /**
   * Format a FAT volume.
   *
   * \param[in] dev Block device for volume.
   * \param[in] secBuffer buffer for writing to volume.
   * \param[in] pr Print device for progress output.
   *
   * \return true for success or false for failure.
   */
  bool format(FsBlockDevice* dev, uint8_t* secBuffer, print_t* pr = nullptr);

 private:
  bool initFatDir(uint8_t fatType, uint32_t sectorCount);
  void initPbs();
  bool makeFat16();
  bool makeFat32();
  bool writeMbr();
  uint32_t m_capacityMB;
  uint32_t m_dataStart;
  uint32_t m_fatSize;
  uint32_t m_fatStart;
  uint32_t m_relativeSectors;
  uint32_t m_sectorCount;
  uint32_t m_totalSectors;
  FsBlockDevice* m_dev;
  print_t* m_pr;
  uint8_t* m_secBuf;
  uint16_t m_reservedSectorCount;
  uint8_t m_partType;
  uint8_t m_sectorsPerCluster;
};
#endif  // FatFormatter_h
