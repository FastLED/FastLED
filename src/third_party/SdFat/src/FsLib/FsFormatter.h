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
#ifndef FsFormatter_h
#define FsFormatter_h
#include "ExFatLib/ExFatLib.h"
#include "FatLib/FatLib.h"
/**
 * \class FsFormatter
 * \brief Format a exFAT/FAT volume.
 */
class FsFormatter {
 public:
  /** Constructor. */
  FsFormatter() = default;
  /**
   * Format a FAT volume.
   *
   * \param[in] dev Block device for volume.
   * \param[in] secBuffer buffer for writing to volume.
   * \param[in] pr Print device for progress output.
   *
   * \return true for success or false for failure.
   */
  bool format(FsBlockDevice* dev, uint8_t* secBuffer, print_t* pr = nullptr) {
    uint32_t sectorCount = dev->sectorCount();
    if (sectorCount == 0) {
      return false;
    }
    return sectorCount <= 67108864 ? m_fFmt.format(dev, secBuffer, pr)
                                   : m_xFmt.format(dev, secBuffer, pr);
  }

 private:
  FatFormatter m_fFmt;
  ExFatFormatter m_xFmt;
};
#endif  // FsFormatter_h
