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
#ifndef FsStructs_h
#define FsStructs_h
#include <stddef.h>
#include <stdint.h>
//------------------------------------------------------------------------------
// See:
// https://learn.microsoft.com/en-us/windows/win32/fileio/file-systems
// https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification
// https://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/fatgen103.doc
// https://github.com/MicrosoftDocs/win32/blob/docs/desktop-src/FileIO/exfat-specification.md
//------------------------------------------------------------------------------
void lbaToMbrChs(uint8_t* chs, uint32_t capacityMB, uint32_t lba);
//------------------------------------------------------------------------------
#if !defined(USE_SIMPLE_LITTLE_ENDIAN) || USE_SIMPLE_LITTLE_ENDIAN
// assumes CPU is little-endian and handles alignment issues.
inline uint16_t getLe16(const uint8_t* src) {
  return *reinterpret_cast<const uint16_t*>(src);
}
inline uint32_t getLe32(const uint8_t* src) {
  return *reinterpret_cast<const uint32_t*>(src);
}
inline uint64_t getLe64(const uint8_t* src) {
  return *reinterpret_cast<const uint64_t*>(src);
}
inline void setLe16(uint8_t* dst, uint16_t src) {
  *reinterpret_cast<uint16_t*>(dst) = src;
}
inline void setLe32(uint8_t* dst, uint32_t src) {
  *reinterpret_cast<uint32_t*>(dst) = src;
}
inline void setLe64(uint8_t* dst, uint64_t src) {
  *reinterpret_cast<uint64_t*>(dst) = src;
}
#else   // USE_SIMPLE_LITTLE_ENDIAN
inline uint16_t getLe16(const uint8_t* src) {
  return (uint16_t)src[0] << 0 | (uint16_t)src[1] << 8;
}
inline uint32_t getLe32(const uint8_t* src) {
  return (uint32_t)src[0] << 0 | (uint32_t)src[1] << 8 |
         (uint32_t)src[2] << 16 | (uint32_t)src[3] << 24;
}
inline uint64_t getLe64(const uint8_t* src) {
  return (uint64_t)src[0] << 0 | (uint64_t)src[1] << 8 |
         (uint64_t)src[2] << 16 | (uint64_t)src[3] << 24 |
         (uint64_t)src[4] << 32 | (uint64_t)src[5] << 40 |
         (uint64_t)src[6] << 48 | (uint64_t)src[7] << 56;
}
inline void setLe16(uint8_t* dst, uint16_t src) {
  dst[0] = src >> 0;
  dst[1] = src >> 8;
}
inline void setLe32(uint8_t* dst, uint32_t src) {
  dst[0] = src >> 0;
  dst[1] = src >> 8;
  dst[2] = src >> 16;
  dst[3] = src >> 24;
}
inline void setLe64(uint8_t* dst, uint64_t src) {
  dst[0] = src >> 0;
  dst[1] = src >> 8;
  dst[2] = src >> 16;
  dst[3] = src >> 24;
  dst[4] = src >> 32;
  dst[5] = src >> 40;
  dst[6] = src >> 48;
  dst[7] = src >> 56;
}
#endif  // USE_SIMPLE_LITTLE_ENDIAN
//------------------------------------------------------------------------------
// Size of FAT and exFAT directory structures.
const size_t FS_DIR_SIZE = 32;
//------------------------------------------------------------------------------
// Reserved characters for exFAT names and FAT LFN.
inline bool lfnReservedChar(uint8_t c) {
  return c < 0X20 || c == '"' || c == '*' || c == '/' || c == ':' || c == '<' ||
         c == '>' || c == '?' || c == '\\' || c == '|';
}
//------------------------------------------------------------------------------
// Reserved characters for FAT short 8.3 names.
inline bool sfnReservedChar(uint8_t c) {
  if (c == '"' || c == '|' || c == '[' || c == '\\' || c == ']') {
    return true;
  }
  //  *+,./ or :;<=>?
  if ((0X2A <= c && c <= 0X2F && c != 0X2D) || (0X3A <= c && c <= 0X3F)) {
    return true;
  }
  // Reserved if not in range (0X20, 0X7F).
  return !(0X20 < c && c < 0X7F);
}
//------------------------------------------------------------------------------
const uint16_t MBR_SIGNATURE = 0xAA55;
const uint16_t PBR_SIGNATURE = 0xAA55;

typedef struct mbrPartition {
  uint8_t boot;
  uint8_t beginCHS[3];
  uint8_t type;
  uint8_t endCHS[3];
  uint8_t relativeSectors[4];
  uint8_t totalSectors[4];
} MbrPart_t;
//------------------------------------------------------------------------------
typedef struct masterBootRecordSector {
  uint8_t bootCode[446];
  MbrPart_t part[4];
  uint8_t signature[2];
} MbrSector_t;
//------------------------------------------------------------------------------
typedef struct partitionBootSector {
  uint8_t jmpInstruction[3];
  char oemName[8];
  uint8_t bpb[109];
  uint8_t bootCode[390];
  uint8_t signature[2];
} pbs_t;
//------------------------------------------------------------------------------
typedef struct {
  uint8_t type;
  uint8_t data[31];
} DirGeneric_t;
//==============================================================================
typedef struct {
  uint64_t position;
  uint32_t cluster;
} fspos_t;
//==============================================================================
const uint8_t EXTENDED_BOOT_SIGNATURE = 0X29;
typedef struct biosParameterBlockFat16 {
  uint8_t bytesPerSector[2];
  uint8_t sectorsPerCluster;
  uint8_t reservedSectorCount[2];
  uint8_t fatCount;
  uint8_t rootDirEntryCount[2];
  uint8_t totalSectors16[2];
  uint8_t mediaType;
  uint8_t sectorsPerFat16[2];
  uint8_t sectorsPerTrtack[2];
  uint8_t headCount[2];
  uint8_t hidddenSectors[4];
  uint8_t totalSectors32[4];

  uint8_t physicalDriveNumber;
  uint8_t extReserved;
  uint8_t extSignature;
  uint8_t volumeSerialNumber[4];
  uint8_t volumeLabel[11];
  uint8_t volumeType[8];
} BpbFat16_t;
//------------------------------------------------------------------------------
typedef struct biosParameterBlockFat32 {
  uint8_t bytesPerSector[2];
  uint8_t sectorsPerCluster;
  uint8_t reservedSectorCount[2];
  uint8_t fatCount;
  uint8_t rootDirEntryCount[2];
  uint8_t totalSectors16[2];
  uint8_t mediaType;
  uint8_t sectorsPerFat16[2];
  uint8_t sectorsPerTrtack[2];
  uint8_t headCount[2];
  uint8_t hidddenSectors[4];
  uint8_t totalSectors32[4];

  uint8_t sectorsPerFat32[4];
  uint8_t fat32Flags[2];
  uint8_t fat32Version[2];
  uint8_t fat32RootCluster[4];
  uint8_t fat32FSInfoSector[2];
  uint8_t fat32BackBootSector[2];
  uint8_t fat32Reserved[12];

  uint8_t physicalDriveNumber;
  uint8_t extReserved;
  uint8_t extSignature;
  uint8_t volumeSerialNumber[4];
  uint8_t volumeLabel[11];
  uint8_t volumeType[8];
} BpbFat32_t;
//------------------------------------------------------------------------------
typedef struct partitionBootSectorFat {
  uint8_t jmpInstruction[3];
  char oemName[8];
  union {
    uint8_t bpb[109];
    BpbFat16_t bpb16;
    BpbFat32_t bpb32;
  } bpb;
  uint8_t bootCode[390];
  uint8_t signature[2];
} PbsFat_t;
//------------------------------------------------------------------------------
const uint32_t FSINFO_LEAD_SIGNATURE = 0X41615252;
const uint32_t FSINFO_STRUCT_SIGNATURE = 0x61417272;
const uint32_t FSINFO_TRAIL_SIGNATURE = 0xAA550000;
typedef struct FsInfoSector {
  uint8_t leadSignature[4];
  uint8_t reserved1[480];
  uint8_t structSignature[4];
  uint8_t freeCount[4];
  uint8_t nextFree[4];
  uint8_t reserved2[12];
  uint8_t trailSignature[4];
} FsInfo_t;
//==============================================================================
/** Attributes common to FAT and exFAT */
const uint8_t FS_ATTRIB_READ_ONLY = 0x01;
const uint8_t FS_ATTRIB_HIDDEN = 0x02;
const uint8_t FS_ATTRIB_SYSTEM = 0x04;
const uint8_t FS_ATTRIB_DIRECTORY = 0x10;
const uint8_t FS_ATTRIB_ARCHIVE = 0x20;
// Attributes that users can change.
const uint8_t FS_ATTRIB_USER_SETTABLE = FS_ATTRIB_READ_ONLY | FS_ATTRIB_HIDDEN |
                                        FS_ATTRIB_SYSTEM | FS_ATTRIB_ARCHIVE;
// Attributes to copy when a file is opened.
const uint8_t FS_ATTRIB_COPY = FS_ATTRIB_USER_SETTABLE | FS_ATTRIB_DIRECTORY;
//==============================================================================
/** name[0] value for entry that is free and no allocated entries follow */
const uint8_t FAT_NAME_FREE = 0X00;
/** name[0] value for entry that is free after being "deleted" */
const uint8_t FAT_NAME_DELETED = 0XE5;
// Directory attribute of volume label.
const uint8_t FAT_ATTRIB_LABEL = 0x08;
const uint8_t FAT_ATTRIB_LONG_NAME = 0X0F;
/** Filename base-name is all lower case */
const uint8_t FAT_CASE_LC_BASE = 0X08;
/** Filename extension is all lower case.*/
const uint8_t FAT_CASE_LC_EXT = 0X10;

typedef struct {
  uint8_t name[11];
  uint8_t attributes;
  uint8_t caseFlags;
  uint8_t createTimeMs;
  uint8_t createTime[2];
  uint8_t createDate[2];
  uint8_t accessDate[2];
  uint8_t firstClusterHigh[2];
  uint8_t modifyTime[2];
  uint8_t modifyDate[2];
  uint8_t firstClusterLow[2];
  uint8_t fileSize[4];
} DirFat_t;

static inline bool isFatFile(const DirFat_t* dir) {
  return (dir->attributes & (FS_ATTRIB_DIRECTORY | FAT_ATTRIB_LABEL)) == 0;
}
static inline bool isFatFileOrSubdir(const DirFat_t* dir) {
  return (dir->attributes & FAT_ATTRIB_LABEL) == 0;
}
static inline uint8_t isFatLongName(const DirFat_t* dir) {
  return dir->attributes == FAT_ATTRIB_LONG_NAME;
}
static inline bool isFatSubdir(const DirFat_t* dir) {
  return (dir->attributes & (FS_ATTRIB_DIRECTORY | FAT_ATTRIB_LABEL)) ==
         FS_ATTRIB_DIRECTORY;
}
//------------------------------------------------------------------------------
/**
 * Order mask that indicates the entry is the last long dir entry in a
 * set of long dir entries. All valid sets of long dir entries must
 * begin with an entry having this mask.
 */
const uint8_t FAT_ORDER_LAST_LONG_ENTRY = 0X40;
/** Max long file name length */

const uint8_t FAT_MAX_LFN_LENGTH = 255;
typedef struct {
  uint8_t order;
  uint8_t unicode1[10];
  uint8_t attributes;
  uint8_t mustBeZero1;
  uint8_t checksum;
  uint8_t unicode2[12];
  uint8_t mustBeZero2[2];
  uint8_t unicode3[4];
} DirLfn_t;
//==============================================================================
inline uint32_t exFatChecksum(uint32_t sum, uint8_t data) {
  return (sum << 31) + (sum >> 1) + data;
}
//------------------------------------------------------------------------------
typedef struct biosParameterBlockExFat {
  uint8_t mustBeZero[53];
  uint8_t partitionOffset[8];
  uint8_t volumeLength[8];
  uint8_t fatOffset[4];
  uint8_t fatLength[4];
  uint8_t clusterHeapOffset[4];
  uint8_t clusterCount[4];
  uint8_t rootDirectoryCluster[4];
  uint8_t volumeSerialNumber[4];
  uint8_t fileSystemRevision[2];
  uint8_t volumeFlags[2];
  uint8_t bytesPerSectorShift;
  uint8_t sectorsPerClusterShift;
  uint8_t numberOfFats;
  uint8_t driveSelect;
  uint8_t percentInUse;
  uint8_t reserved[7];
} BpbExFat_t;
//------------------------------------------------------------------------------
typedef struct ExFatBootSector {
  uint8_t jmpInstruction[3];
  char oemName[8];
  BpbExFat_t bpb;
  uint8_t bootCode[390];
  uint8_t signature[2];
} ExFatPbs_t;
//------------------------------------------------------------------------------
const uint32_t EXFAT_EOC = 0XFFFFFFFF;

const uint8_t EXFAT_TYPE_BITMAP = 0X81;
typedef struct {
  uint8_t type;
  uint8_t flags;
  uint8_t reserved[18];
  uint8_t firstCluster[4];
  uint8_t size[8];
} DirBitmap_t;
//------------------------------------------------------------------------------
const uint8_t EXFAT_TYPE_UPCASE = 0X82;
typedef struct {
  uint8_t type;
  uint8_t reserved1[3];
  uint8_t checksum[4];
  uint8_t reserved2[12];
  uint8_t firstCluster[4];
  uint8_t size[8];
} DirUpcase_t;
//------------------------------------------------------------------------------
const uint8_t EXFAT_TYPE_LABEL = 0X83;
typedef struct {
  uint8_t type;
  uint8_t labelLength;
  uint8_t unicode[22];
  uint8_t reserved[8];
} DirLabel_t;
//------------------------------------------------------------------------------
// Last entry in directory.
const uint8_t EXFAT_TYPE_END_DIR = 0X00;
// Entry is used if bit is set.
const uint8_t EXFAT_TYPE_USED = 0X80;
const uint8_t EXFAT_TYPE_FILE = 0X85;
// File attribute reserved since used for FAT volume label.
const uint8_t EXFAT_ATTRIB_RESERVED = 0x08;

typedef struct {
  uint8_t type;
  uint8_t setCount;
  uint8_t setChecksum[2];
  uint8_t attributes[2];
  uint8_t reserved1[2];
  uint8_t createTime[2];
  uint8_t createDate[2];
  uint8_t modifyTime[2];
  uint8_t modifyDate[2];
  uint8_t accessTime[2];
  uint8_t accessDate[2];
  uint8_t createTimeMs;
  uint8_t modifyTimeMs;
  uint8_t createTimezone;
  uint8_t modifyTimezone;
  uint8_t accessTimezone;
  uint8_t reserved2[7];
} DirFile_t;

const uint8_t EXFAT_TYPE_STREAM = 0XC0;
const uint8_t EXFAT_FLAG_ALWAYS1 = 0x01;
const uint8_t EXFAT_FLAG_CONTIGUOUS = 0x02;
typedef struct {
  uint8_t type;
  uint8_t flags;
  uint8_t reserved1;
  uint8_t nameLength;
  uint8_t nameHash[2];
  uint8_t reserved2[2];
  uint8_t validLength[8];
  uint8_t reserved3[4];
  uint8_t firstCluster[4];
  uint8_t dataLength[8];
} DirStream_t;

const uint8_t EXFAT_TYPE_NAME = 0XC1;
const uint8_t EXFAT_MAX_NAME_LENGTH = 255;
typedef struct {
  uint8_t type;
  uint8_t mustBeZero;
  uint8_t unicode[30];
} DirName_t;
#endif  // FsStructs_h
