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
#include "FsLib.h"
#if FILE_COPY_CONSTRUCTOR_SELECT
//------------------------------------------------------------------------------
FsBaseFile::FsBaseFile(const FsBaseFile& from) { copy(&from); }
//------------------------------------------------------------------------------
FsBaseFile& FsBaseFile::operator=(const FsBaseFile& from) {
  copy(&from);
  return *this;
}
#endif  // FILE_COPY_CONSTRUCTOR_SELECT
//------------------------------------------------------------------------------
void FsBaseFile::copy(const FsBaseFile* from) {
  if (from != this) {
    m_fFile = nullptr;
    m_xFile = nullptr;
    if (from->m_fFile) {
      m_fFile = new (m_fileMem) FatFile;
      m_fFile->copy(from->m_fFile);
    } else if (from->m_xFile) {
      m_xFile = new (m_fileMem) ExFatFile;
      m_xFile->copy(from->m_xFile);
    }
  }
}
//------------------------------------------------------------------------------
void FsBaseFile::move(FsBaseFile* from) {
  if (from != this) {
    copy(from);
    from->m_fFile = nullptr;
    from->m_xFile = nullptr;
  }
}
//------------------------------------------------------------------------------
bool FsBaseFile::close() {
  bool rtn = m_fFile ? m_fFile->close() : m_xFile ? m_xFile->close() : true;
  m_fFile = nullptr;
  m_xFile = nullptr;
  return rtn;
}
//------------------------------------------------------------------------------
bool FsBaseFile::mkdir(FsBaseFile* dir, const char* path, bool pFlag) {
  close();
  if (dir->m_fFile) {
    m_fFile = new (m_fileMem) FatFile;
    if (m_fFile->mkdir(dir->m_fFile, path, pFlag)) {
      return true;
    }
    m_fFile = nullptr;
  } else if (dir->m_xFile) {
    m_xFile = new (m_fileMem) ExFatFile;
    if (m_xFile->mkdir(dir->m_xFile, path, pFlag)) {
      return true;
    }
    m_xFile = nullptr;
  }
  return false;
}
//------------------------------------------------------------------------------
bool FsBaseFile::open(FsVolume* vol, const char* path, oflag_t oflag) {
  if (!vol) {
    return false;
  }
  close();
  if (vol->m_fVol) {
    m_fFile = new (m_fileMem) FatFile;
    if (m_fFile && m_fFile->open(vol->m_fVol, path, oflag)) {
      return true;
    }
    m_fFile = nullptr;
  } else if (vol->m_xVol) {
    m_xFile = new (m_fileMem) ExFatFile;
    if (m_xFile && m_xFile->open(vol->m_xVol, path, oflag)) {
      return true;
    }
    m_xFile = nullptr;
  }
  return false;
}
//------------------------------------------------------------------------------
bool FsBaseFile::open(FsBaseFile* dir, const char* path, oflag_t oflag) {
  close();
  if (dir->m_fFile) {
    m_fFile = new (m_fileMem) FatFile;
    if (m_fFile->open(dir->m_fFile, path, oflag)) {
      return true;
    }
    m_fFile = nullptr;
  } else if (dir->m_xFile) {
    m_xFile = new (m_fileMem) ExFatFile;
    if (m_xFile->open(dir->m_xFile, path, oflag)) {
      return true;
    }
    m_xFile = nullptr;
  }
  return false;
}
//------------------------------------------------------------------------------
bool FsBaseFile::open(FsBaseFile* dir, uint32_t index, oflag_t oflag) {
  close();
  if (dir->m_fFile) {
    m_fFile = new (m_fileMem) FatFile;
    if (m_fFile->open(dir->m_fFile, index, oflag)) {
      return true;
    }
    m_fFile = nullptr;
  } else if (dir->m_xFile) {
    m_xFile = new (m_fileMem) ExFatFile;
    if (m_xFile->open(dir->m_xFile, index, oflag)) {
      return true;
    }
    m_xFile = nullptr;
  }
  return false;
}
//------------------------------------------------------------------------------
bool FsBaseFile::openCwd() {
  close();
  if (FsVolume::m_cwv && FsVolume::m_cwv->m_fVol) {
    m_fFile = new (m_fileMem) FatFile;
    if (m_fFile->openCwd()) {
      return true;
    }
    m_fFile = nullptr;
  } else if (FsVolume::m_cwv && FsVolume::m_cwv->m_xVol) {
    m_xFile = new (m_fileMem) ExFatFile;
    if (m_xFile->openCwd()) {
      return true;
    }
    m_xFile = nullptr;
  }
  return false;
}
//------------------------------------------------------------------------------
bool FsBaseFile::openNext(FsBaseFile* dir, oflag_t oflag) {
  close();
  if (dir->m_fFile) {
    m_fFile = new (m_fileMem) FatFile;
    if (m_fFile->openNext(dir->m_fFile, oflag)) {
      return true;
    }
    m_fFile = nullptr;
  } else if (dir->m_xFile) {
    m_xFile = new (m_fileMem) ExFatFile;
    if (m_xFile->openNext(dir->m_xFile, oflag)) {
      return true;
    }
    m_xFile = nullptr;
  }
  return false;
}
//------------------------------------------------------------------------------
bool FsBaseFile::openRoot(FsVolume* vol) {
  if (!vol) {
    return false;
  }
  close();
  if (vol->m_fVol) {
    m_fFile = new (m_fileMem) FatFile;
    if (m_fFile && m_fFile->openRoot(vol->m_fVol)) {
      return true;
    }
    m_fFile = nullptr;
  } else if (vol->m_xVol) {
    m_xFile = new (m_fileMem) ExFatFile;
    if (m_xFile && m_xFile->openRoot(vol->m_xVol)) {
      return true;
    }
    m_xFile = nullptr;
  }
  return false;
}
//------------------------------------------------------------------------------
bool FsBaseFile::remove() {
  if (m_fFile) {
    if (m_fFile->remove()) {
      m_fFile = nullptr;
      return true;
    }
  } else if (m_xFile) {
    if (m_xFile->remove()) {
      m_xFile = nullptr;
      return true;
    }
  }
  return false;
}
//------------------------------------------------------------------------------
bool FsBaseFile::rmdir() {
  if (m_fFile) {
    if (m_fFile->rmdir()) {
      m_fFile = nullptr;
      return true;
    }
  } else if (m_xFile) {
    if (m_xFile->rmdir()) {
      m_xFile = nullptr;
      return true;
    }
  }
  return false;
}
