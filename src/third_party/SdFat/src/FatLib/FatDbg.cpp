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
#include "FatLib.h"
#ifndef DOXYGEN_SHOULD_SKIP_THIS
//------------------------------------------------------------------------------
static uint16_t getLfnChar(DirLfn_t* ldir, uint8_t i) {
  if (i < 5) {
    return getLe16(ldir->unicode1 + 2 * i);
  } else if (i < 11) {
    return getLe16(ldir->unicode2 + 2 * (i - 5));
  } else if (i < 13) {
    return getLe16(ldir->unicode3 + 2 * (i - 11));
  }
  return 0;
}
//------------------------------------------------------------------------------
static void printHex(print_t* pr, uint8_t h) {
  if (h < 16) {
    pr->write('0');
  }
  pr->print(h, HEX);
}
//------------------------------------------------------------------------------
static void printHex(print_t* pr, uint8_t w, uint16_t h) {
  char buf[5];
  char* ptr = buf + sizeof(buf);
  *--ptr = 0;
  for (uint8_t i = 0; i < w; i++) {
    char c = h & 0XF;
    *--ptr = c < 10 ? c + '0' : c + 'A' - 10;
    h >>= 4;
  }
  pr->write(ptr);
}
//------------------------------------------------------------------------------
static void printHex(print_t* pr, uint16_t val) {
  bool space = true;
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t h = (val >> (12 - 4 * i)) & 15;
    if (h || i == 3) {
      space = false;
    }
    if (space) {
      pr->write(' ');
    } else {
      pr->print(h, HEX);
    }
  }
}
//------------------------------------------------------------------------------
static void printHex(print_t* pr, uint32_t val) {
  bool space = true;
  for (uint8_t i = 0; i < 8; i++) {
    uint8_t h = (val >> (28 - 4 * i)) & 15;
    if (h || i == 7) {
      space = false;
    }
    if (space) {
      pr->write(' ');
    } else {
      pr->print(h, HEX);
    }
  }
}
//------------------------------------------------------------------------------
template <typename Uint>
static void printHexLn(print_t* pr, Uint val) {
  printHex(pr, val);
  pr->println();
}
//------------------------------------------------------------------------------
static bool printFatDir(print_t* pr, DirFat_t* dir) {
  DirLfn_t* ldir = reinterpret_cast<DirLfn_t*>(dir);
  if (!dir->name[0]) {
    pr->println(F("Unused"));
    return false;
  } else if (dir->name[0] == FAT_NAME_DELETED) {
    pr->println(F("Deleted"));
  } else if (isFatFileOrSubdir(dir)) {
    pr->print(F("SFN: "));
    for (uint8_t i = 0; i < 11; i++) {
      printHex(pr, dir->name[i]);
      pr->write(' ');
    }
    pr->write(' ');
    pr->write(dir->name, 11);
    pr->println();
    pr->print(F("attributes: 0X"));
    printHexLn(pr, dir->attributes);
    pr->print(F("caseFlags: 0X"));
    printHexLn(pr, dir->caseFlags);
    uint32_t fc = ((uint32_t)getLe16(dir->firstClusterHigh) << 16) |
                  getLe16(dir->firstClusterLow);
    pr->print(F("firstCluster: "));
    pr->println(fc, HEX);
    pr->print(F("fileSize: "));
    pr->println(getLe32(dir->fileSize));
  } else if (isFatLongName(dir)) {
    pr->print(F("LFN: "));
    for (uint8_t i = 0; i < 13; i++) {
      uint16_t c = getLfnChar(ldir, i);
      if (15 < c && c < 128) {
        pr->print(static_cast<char>(c));
      } else {
        pr->print("0X");
        pr->print(c, HEX);
      }
      pr->print(' ');
    }
    pr->println();
    pr->print(F("order: 0X"));
    pr->println(ldir->order, HEX);
    pr->print(F("attributes: 0X"));
    pr->println(ldir->attributes, HEX);
    pr->print(F("checksum: 0X"));
    pr->println(ldir->checksum, HEX);
  } else {
    pr->println(F("Other"));
  }
  pr->println();
  return true;
}
//------------------------------------------------------------------------------
void FatFile::dmpFile(print_t* pr, uint32_t pos, size_t n) {
  char text[17];
  text[16] = 0;
  if (n >= 0XFFF0) {
    n = 0XFFF0;
  }
  if (!seekSet(pos)) {
    return;
  }
  for (size_t i = 0; i <= n; i++) {
    if ((i & 15) == 0) {
      if (i) {
        pr->write(' ');
        pr->write(text);
        if (i == n) {
          break;
        }
      }
      pr->write('\r');
      pr->write('\n');
      if (i >= n) {
        break;
      }
      printHex(pr, 4, i);
      pr->write(' ');
    }
    int16_t h = read();
    if (h < 0) {
      break;
    }
    pr->write(' ');
    printHex(pr, 2, h);
    text[i & 15] = ' ' <= h && h < 0X7F ? h : '.';
  }
  pr->write('\r');
  pr->write('\n');
}
//------------------------------------------------------------------------------
bool FatPartition::dmpDirSector(print_t* pr, uint32_t sector) {
  DirFat_t dir[16];
  if (!cacheSafeRead(sector, reinterpret_cast<uint8_t*>(dir))) {
    pr->println(F("dmpDir failed"));
    return false;
  }
  for (uint8_t i = 0; i < 16; i++) {
    if (!printFatDir(pr, dir + i)) {
      return false;
    }
  }
  return true;
}
//------------------------------------------------------------------------------
bool FatPartition::dmpRootDir(print_t* pr, uint32_t n) {
  uint32_t sector;
  if (fatType() == 16) {
    sector = rootDirStart();
  } else if (fatType() == 32) {
    sector = clusterStartSector(rootDirStart());
  } else {
    pr->println(F("dmpRootDir failed"));
    return false;
  }
  return dmpDirSector(pr, sector + n);
}
//------------------------------------------------------------------------------
void FatPartition::dmpSector(print_t* pr, uint32_t sector, uint8_t bits) {
  uint8_t data[FatPartition::m_bytesPerSector];
  if (!cacheSafeRead(sector, data)) {
    pr->println(F("dmpSector failed"));
    return;
  }
  for (uint16_t i = 0; i < m_bytesPerSector;) {
    if (i % 32 == 0) {
      if (i) {
        pr->println();
      }
      printHex(pr, i);
    }
    pr->write(' ');
    if (bits == 32) {
      printHex(pr, *reinterpret_cast<uint32_t*>(data + i));
      i += 4;
    } else if (bits == 16) {
      printHex(pr, *reinterpret_cast<uint16_t*>(data + i));
      i += 2;
    } else {
      printHex(pr, data[i++]);
    }
  }
  pr->println();
}
//------------------------------------------------------------------------------
void FatPartition::dmpFat(print_t* pr, uint32_t start, uint32_t count) {
  uint16_t nf = fatType() == 16 ? 256 : fatType() == 32 ? 128 : 0;
  if (nf == 0) {
    pr->println(F("Invalid fatType"));
    return;
  }
  pr->println(F("FAT:"));
  uint32_t sector = m_fatStartSector + start;
  uint32_t cluster = nf * start;
  for (uint32_t i = 0; i < count; i++) {
    uint8_t* pc = fatCachePrepare(sector + i, FsCache::CACHE_FOR_READ);
    if (!pc) {
      pr->println(F("cache read failed"));
      return;
    }
    for (size_t k = 0; k < nf; k++) {
      if (0 == cluster % 8) {
        if (k) {
          pr->println();
        }
        printHex(pr, cluster);
      }
      cluster++;
      pr->write(' ');
      uint32_t v = fatType() == 32 ? getLe32(pc + 4 * k) : getLe16(pc + 2 * k);
      printHex(pr, v);
    }
    pr->println();
  }
}
#endif  // DOXYGEN_SHOULD_SKIP_THIS
