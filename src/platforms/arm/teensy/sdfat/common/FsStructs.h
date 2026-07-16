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
// IWYU pragma: private
#ifndef FsStructs_h
#define FsStructs_h
#include "fl/stl/int.h"  // IWYU pragma: keep
#include "SdFatConfig.h"  // ok include path
namespace fl { namespace platforms { namespace teensy { namespace sdfat {
//-----------------------------------------------------------------------------
void lbaToMbrChs(fl::u8* chs, fl::u32 capacityMB, fl::u32 lba);
//-----------------------------------------------------------------------------
#if !defined(USE_SIMPLE_LITTLE_ENDIAN) || USE_SIMPLE_LITTLE_ENDIAN
// assumes CPU is little-endian and handles alignment issues.
inline fl::u16 getLe16(const fl::u8* src) {
  return *reinterpret_cast<const fl::u16*>(src);  // okay reinterpret cast
}
inline fl::u32 getLe32(const fl::u8* src) {
  return *reinterpret_cast<const fl::u32*>(src);  // okay reinterpret cast
}
inline fl::u64 getLe64(const fl::u8* src) {
  return *reinterpret_cast<const fl::u64*>(src);  // okay reinterpret cast
}
inline void setLe16(fl::u8* dst, fl::u16 src) {
  *reinterpret_cast<fl::u16*>(dst) = src;  // okay reinterpret cast
}
inline void setLe32(fl::u8* dst, fl::u32 src) {
  *reinterpret_cast<fl::u32*>(dst) = src;  // okay reinterpret cast
}
inline void setLe64(fl::u8* dst, fl::u64 src) {
  *reinterpret_cast<fl::u64*>(dst) = src;  // okay reinterpret cast
}
#else  // USE_SIMPLE_LITTLE_ENDIAN
inline fl::u16 getLe16(const fl::u8* src) {
  return static_cast<fl::u16>(src[0]) << 0 |
         static_cast<fl::u16>(src[1]) << 8;
}
inline fl::u32 getLe32(const fl::u8* src) {
  return static_cast<fl::u32>(src[0]) <<  0 |
         static_cast<fl::u32>(src[1]) <<  8 |
         static_cast<fl::u32>(src[2]) << 16 |
         static_cast<fl::u32>(src[3]) << 24;
}
inline fl::u64 getLe64(const fl::u8* src) {
  return static_cast<fl::u64>(src[0]) <<  0 |
         static_cast<fl::u64>(src[1]) <<  8 |
         static_cast<fl::u64>(src[2]) << 16 |
         static_cast<fl::u64>(src[3]) << 24 |
         static_cast<fl::u64>(src[4]) << 32 |
         static_cast<fl::u64>(src[5]) << 40 |
         static_cast<fl::u64>(src[6]) << 48 |
         static_cast<fl::u64>(src[7]) << 56;
}
inline void setLe16(fl::u8* dst, fl::u16 src) {
  dst[0] = src >>  0;
  dst[1] = src >>  8;
}
inline void setLe32(fl::u8* dst, fl::u32 src) {
  dst[0] = src >>  0;
  dst[1] = src >>  8;
  dst[2] = src >> 16;
  dst[3] = src >> 24;
}
inline void setLe64(fl::u8* dst, fl::u64 src) {
  dst[0] = src >>  0;
  dst[1] = src >>  8;
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
const fl::size FS_DIR_SIZE = 32;
//------------------------------------------------------------------------------
// Reserved characters for exFAT names and FAT LFN.
inline bool lfnReservedChar(fl::u8 c) {
  return c < 0X20 || c == '"' || c == '*' || c == '/' || c == ':'
    || c == '<' || c == '>' || c == '?' || c == '\\'|| c == '|';
}
//------------------------------------------------------------------------------
// Reserved characters for FAT short 8.3 names.
inline bool sfnReservedChar(fl::u8 c) {
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
const fl::u16 MBR_SIGNATURE = 0xAA55;
const fl::u16 PBR_SIGNATURE = 0xAA55;

typedef struct mbrPartition {
  fl::u8 boot;
  fl::u8 beginCHS[3];
  fl::u8 type;
  fl::u8 endCHS[3];
  fl::u8 relativeSectors[4];
  fl::u8 totalSectors[4];
} MbrPart_t;
//------------------------------------------------------------------------------
typedef struct masterBootRecordSector {
  fl::u8   bootCode[446];
  MbrPart_t part[4];
  fl::u8   signature[2];
} MbrSector_t;
//------------------------------------------------------------------------------
typedef struct partitionBootSector {
  fl::u8  jmpInstruction[3];
  char     oemName[8];
  fl::u8  bpb[109];
  fl::u8  bootCode[390];
  fl::u8  signature[2];
} pbs_t;
//------------------------------------------------------------------------------
typedef struct {
  fl::u8 type;
  fl::u8 data[31];
} DirGeneric_t;
//==============================================================================
typedef struct {
  fl::u64 position;
  fl::u32 cluster;
} fspos_t;
//=============================================================================
const fl::u8 EXTENDED_BOOT_SIGNATURE = 0X29;
typedef struct biosParameterBlockFat16 {
  fl::u8  bytesPerSector[2];
  fl::u8  sectorsPerCluster;
  fl::u8  reservedSectorCount[2];
  fl::u8  fatCount;
  fl::u8  rootDirEntryCount[2];
  fl::u8  totalSectors16[2];
  fl::u8  mediaType;
  fl::u8  sectorsPerFat16[2];
  fl::u8  sectorsPerTrtack[2];
  fl::u8  headCount[2];
  fl::u8  hidddenSectors[4];
  fl::u8  totalSectors32[4];

  fl::u8  physicalDriveNumber;
  fl::u8  extReserved;
  fl::u8  extSignature;
  fl::u8  volumeSerialNumber[4];
  fl::u8  volumeLabel[11];
  fl::u8  volumeType[8];
} BpbFat16_t;
//-----------------------------------------------------------------------------
typedef struct biosParameterBlockFat32 {
  fl::u8  bytesPerSector[2];
  fl::u8  sectorsPerCluster;
  fl::u8  reservedSectorCount[2];
  fl::u8  fatCount;
  fl::u8  rootDirEntryCount[2];
  fl::u8  totalSectors16[2];
  fl::u8  mediaType;
  fl::u8  sectorsPerFat16[2];
  fl::u8  sectorsPerTrtack[2];
  fl::u8  headCount[2];
  fl::u8  hidddenSectors[4];
  fl::u8  totalSectors32[4];

  fl::u8  sectorsPerFat32[4];
  fl::u8  fat32Flags[2];
  fl::u8  fat32Version[2];
  fl::u8  fat32RootCluster[4];
  fl::u8  fat32FSInfoSector[2];
  fl::u8  fat32BackBootSector[2];
  fl::u8  fat32Reserved[12];

  fl::u8  physicalDriveNumber;
  fl::u8  extReserved;
  fl::u8  extSignature;
  fl::u8  volumeSerialNumber[4];
  fl::u8  volumeLabel[11];
  fl::u8  volumeType[8];
} BpbFat32_t;
//-----------------------------------------------------------------------------
typedef struct partitionBootSectorFat {
  fl::u8  jmpInstruction[3];
  char     oemName[8];
  union {
    fl::u8 bpb[109];
    BpbFat16_t bpb16;
    BpbFat32_t bpb32;
  } bpb;
  fl::u8  bootCode[390];
  fl::u8  signature[2];
} PbsFat_t;
//-----------------------------------------------------------------------------
const fl::u32 FSINFO_LEAD_SIGNATURE = 0X41615252;
const fl::u32 FSINFO_STRUCT_SIGNATURE = 0x61417272;
const fl::u32 FSINFO_TRAIL_SIGNATURE = 0xAA550000;
typedef struct FsInfoSector {
  fl::u8 leadSignature[4];
  fl::u8 reserved1[480];
  fl::u8 structSignature[4];
  fl::u8 freeCount[4];
  fl::u8 nextFree[4];
  fl::u8 reserved2[12];
  fl::u8 trailSignature[4];
} FsInfo_t;
//-----------------------------------------------------------------------------
/** name[0] value for entry that is free after being "deleted" */
const fl::u8 FAT_NAME_DELETED = 0XE5;
/** name[0] value for entry that is free and no allocated entries follow */
const fl::u8 FAT_NAME_FREE = 0X00;
const fl::u8 FAT_ATTRIB_READ_ONLY = 0x01;
const fl::u8 FAT_ATTRIB_HIDDEN    = 0x02;
const fl::u8 FAT_ATTRIB_SYSTEM    = 0x04;
const fl::u8 FAT_ATTRIB_LABEL     = 0x08;
const fl::u8 FAT_ATTRIB_DIRECTORY = 0x10;
const fl::u8 FAT_ATTRIB_ARCHIVE   = 0x20;
const fl::u8 FAT_ATTRIB_LONG_NAME = 0X0F;
/** Filename base-name is all lower case */
const fl::u8 FAT_CASE_LC_BASE = 0X08;
/** Filename extension is all lower case.*/
const fl::u8 FAT_CASE_LC_EXT = 0X10;

typedef struct {
  fl::u8  name[11];
  fl::u8  attributes;
  fl::u8  caseFlags;
  fl::u8  createTimeMs;
  fl::u8  createTime[2];
  fl::u8  createDate[2];
  fl::u8  accessDate[2];
  fl::u8  firstClusterHigh[2];
  fl::u8  modifyTime[2];
  fl::u8  modifyDate[2];
  fl::u8  firstClusterLow[2];
  fl::u8  fileSize[4];
} DirFat_t;

static inline bool isFileDir(const DirFat_t* dir) {
  return (dir->attributes & (FAT_ATTRIB_DIRECTORY | FAT_ATTRIB_LABEL)) == 0;
}
static inline bool isFileOrSubdir(const DirFat_t* dir) {
  return (dir->attributes & FAT_ATTRIB_LABEL) == 0;
}
static inline fl::u8 isLongName(const DirFat_t* dir) {
  return dir->attributes == FAT_ATTRIB_LONG_NAME;
}
static inline bool isSubdir(const DirFat_t* dir) {
  return (dir->attributes & (FAT_ATTRIB_DIRECTORY | FAT_ATTRIB_LABEL))
          == FAT_ATTRIB_DIRECTORY;
}
//-----------------------------------------------------------------------------
/**
 * Order mask that indicates the entry is the last long dir entry in a
 * set of long dir entries. All valid sets of long dir entries must
 * begin with an entry having this mask.
 */
const fl::u8 FAT_ORDER_LAST_LONG_ENTRY = 0X40;
/** Max long file name length */

const fl::u8 FAT_MAX_LFN_LENGTH = 255;
typedef struct {
  fl::u8  order;
  fl::u8  unicode1[10];
  fl::u8  attributes;
  fl::u8  mustBeZero1;
  fl::u8  checksum;
  fl::u8  unicode2[12];
  fl::u8  mustBeZero2[2];
  fl::u8  unicode3[4];
} DirLfn_t;
//=============================================================================
inline fl::u32 exFatChecksum(fl::u32 sum, fl::u8 data) {
  return (sum << 31) + (sum >> 1) + data;
}
//-----------------------------------------------------------------------------
typedef struct biosParameterBlockExFat {
  fl::u8 mustBeZero[53];
  fl::u8 partitionOffset[8];
  fl::u8 volumeLength[8];
  fl::u8 fatOffset[4];
  fl::u8 fatLength[4];
  fl::u8 clusterHeapOffset[4];
  fl::u8 clusterCount[4];
  fl::u8 rootDirectoryCluster[4];
  fl::u8 volumeSerialNumber[4];
  fl::u8 fileSystemRevision[2];
  fl::u8 volumeFlags[2];
  fl::u8 bytesPerSectorShift;
  fl::u8 sectorsPerClusterShift;
  fl::u8 numberOfFats;
  fl::u8 driveSelect;
  fl::u8 percentInUse;
  fl::u8 reserved[7];
} BpbExFat_t;
//-----------------------------------------------------------------------------
typedef struct ExFatBootSector {
  fl::u8  jmpInstruction[3];
  char     oemName[8];
  BpbExFat_t  bpb;
  fl::u8  bootCode[390];
  fl::u8  signature[2];
} ExFatPbs_t;
//-----------------------------------------------------------------------------
const fl::u32 EXFAT_EOC = 0XFFFFFFFF;

const fl::u8 EXFAT_TYPE_BITMAP = 0X81;
typedef struct {
  fl::u8  type;
  fl::u8  flags;
  fl::u8  reserved[18];
  fl::u8  firstCluster[4];
  fl::u8  size[8];
} DirBitmap_t;
//-----------------------------------------------------------------------------
const fl::u8 EXFAT_TYPE_UPCASE = 0X82;
typedef struct {
  fl::u8  type;
  fl::u8  reserved1[3];
  fl::u8  checksum[4];
  fl::u8  reserved2[12];
  fl::u8  firstCluster[4];
  fl::u8  size[8];
} DirUpcase_t;
//-----------------------------------------------------------------------------
const fl::u8 EXFAT_TYPE_LABEL = 0X83;
typedef struct {
  fl::u8  type;
  fl::u8  labelLength;
  fl::u8  unicode[22];
  fl::u8  reserved[8];
} DirLabel_t;
//-----------------------------------------------------------------------------
const fl::u8 EXFAT_TYPE_FILE        = 0X85;
const fl::u8 EXFAT_ATTRIB_READ_ONLY = 0x01;
const fl::u8 EXFAT_ATTRIB_HIDDEN    = 0x02;
const fl::u8 EXFAT_ATTRIB_SYSTEM    = 0x04;
const fl::u8 EXFAT_ATTRIB_RESERVED  = 0x08;
const fl::u8 EXFAT_ATTRIB_DIRECTORY = 0x10;
const fl::u8 EXFAT_ATTRIB_ARCHIVE   = 0x20;

typedef struct {
  fl::u8  type;
  fl::u8  setCount;
  fl::u8  setChecksum[2];
  fl::u8  attributes[2];
  fl::u8  reserved1[2];
  fl::u8  createTime[2];
  fl::u8  createDate[2];
  fl::u8  modifyTime[2];
  fl::u8  modifyDate[2];
  fl::u8  accessTime[2];
  fl::u8  accessDate[2];
  fl::u8  createTimeMs;
  fl::u8  modifyTimeMs;
  fl::u8  createTimezone;
  fl::u8  modifyTimezone;
  fl::u8  accessTimezone;
  fl::u8  reserved2[7];
} DirFile_t;

const fl::u8 EXFAT_TYPE_STREAM     = 0XC0;
const fl::u8 EXFAT_FLAG_ALWAYS1    = 0x01;
const fl::u8 EXFAT_FLAG_CONTIGUOUS = 0x02;
typedef struct {
  fl::u8  type;
  fl::u8  flags;
  fl::u8  reserved1;
  fl::u8  nameLength;
  fl::u8  nameHash[2];
  fl::u8  reserved2[2];
  fl::u8  validLength[8];
  fl::u8  reserved3[4];
  fl::u8  firstCluster[4];
  fl::u8  dataLength[8];
} DirStream_t;

const fl::u8 EXFAT_TYPE_NAME = 0XC1;
const fl::u8 EXFAT_MAX_NAME_LENGTH = 255;
typedef struct {
  fl::u8  type;
  fl::u8  mustBeZero;
  fl::u8  unicode[30];
} DirName_t;

//-----------------------------------------------------------------------------
// WIP GPT support for now just assume 16 bytes...
typedef struct {
  fl::u8 data[16];
} Guid_t;

typedef struct {
  fl::u8  signature[8];
  fl::u8  revision[4];
  fl::u8  headerSize[4];
  fl::u8  crc32[4];
  fl::u8  reserved[4];
  fl::u8  currentLBA[8];
  fl::u8  backupLBA[8];
  fl::u8  firstLBA[8];
  fl::u8  lastLBA[8];
  fl::u8  diskGUID[16];
  fl::u8  startLBAArray[8];
  fl::u8  numberPartitions[4];
  fl::u8  sizePartitionEntry[4];
  fl::u8  crc32PartitionEntries[4];
  fl::u8  unused[420]; // should be 0;
} GPTPartitionHeader_t;

typedef struct {
  fl::u8  partitionTypeGUID[16];
  fl::u8  uniqueGUID[16];
  fl::u8  firstLBA[8];
  fl::u8  lastLBA[8];
  fl::u8  attributeFlags[8];
  fl::u16 name[36];
} GPTPartitionEntryItem_t;

typedef struct {
  GPTPartitionEntryItem_t items[4];
} GPTPartitionEntrySector_t;


} } } }  // namespace fl::platforms::teensy::sdfat
#endif  // FsStructs_h
