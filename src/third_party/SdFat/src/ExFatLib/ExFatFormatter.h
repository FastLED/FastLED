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
#ifndef ExFatFormatter_h
#define ExFatFormatter_h
#include "../common/FsBlockDevice.h"
/**
 * \class ExFatFormatter
 * \brief Format an exFAT volume.
 */
class ExFatFormatter {
 public:
  /** Constructor. */
  ExFatFormatter() = default;
  /**
   * Format an exFAT volume.
   *
   * \param[in] dev Block device for volume.
   * \param[in] secBuf buffer for writing to volume.
   * \param[in] pr Print device for progress output.
   *
   * \return true for success or false for failure.
   */
  bool format(FsBlockDevice* dev, uint8_t* secBuf, print_t* pr = nullptr);

 private:
  bool syncUpcase();
  bool writeUpcase(uint32_t sector);
  bool writeUpcaseByte(uint8_t b);
  bool writeUpcaseUnicode(uint16_t unicode);
  uint32_t m_upcaseSector;
  uint32_t m_upcaseChecksum;
  uint32_t m_upcaseSize;
  FsBlockDevice* m_dev;
  uint8_t* m_secBuf;
};
#endif  // ExFatFormatter_h
