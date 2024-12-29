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
#include "FsLib.h"
FsVolume* FsVolume::m_cwv = nullptr;
//------------------------------------------------------------------------------
bool FsVolume::begin(FsBlockDevice* blockDev, bool setCwv, uint8_t part,
                     uint32_t volStart) {
  m_fVol = nullptr;
  m_xVol = new (m_volMem) ExFatVolume;
  if (m_xVol && m_xVol->begin(blockDev, false, part, volStart)) {
    goto done;
  }
  m_xVol = nullptr;
  m_fVol = new (m_volMem) FatVolume;
  if (m_fVol && m_fVol->begin(blockDev, false, part, volStart)) {
    goto done;
  }
  m_fVol = nullptr;
  return false;

done:
  if (setCwv || !m_cwv) {
    m_cwv = this;
  }
  return true;
}
//------------------------------------------------------------------------------
bool FsVolume::ls(print_t* pr, const char* path, uint8_t flags) {
  FsBaseFile dir;
  return dir.open(this, path, O_RDONLY) && dir.ls(pr, flags);
}
//------------------------------------------------------------------------------
FsFile FsVolume::open(const char* path, oflag_t oflag) {
  FsFile tmpFile;
  tmpFile.open(this, path, oflag);
  return tmpFile;
}
#if ENABLE_ARDUINO_STRING
//------------------------------------------------------------------------------
FsFile FsVolume::open(const String& path, oflag_t oflag) {
  return open(path.c_str(), oflag);
}
#endif  // ENABLE_ARDUINO_STRING
