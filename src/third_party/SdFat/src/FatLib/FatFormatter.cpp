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
// Set nonzero to use calculated CHS in MBR.  Should not be required.
#define USE_LBA_TO_CHS 1

// Constants for file system structure optimized for flash.
uint16_t const BU16 = 128;
uint16_t const BU32 = 8192;
// Assume 512 byte sectors.
const uint16_t BYTES_PER_SECTOR = 512;
const uint16_t SECTORS_PER_MB = 0X100000 / BYTES_PER_SECTOR;
const uint16_t FAT16_ROOT_ENTRY_COUNT = 512;
const uint16_t FAT16_ROOT_SECTOR_COUNT =
    32 * FAT16_ROOT_ENTRY_COUNT / BYTES_PER_SECTOR;
//------------------------------------------------------------------------------
#define PRINT_FORMAT_PROGRESS 1
#if !PRINT_FORMAT_PROGRESS
#define writeMsg(str)
#elif defined(__AVR__)
#define writeMsg(str) \
  if (m_pr) m_pr->print(F(str))
#else  // PRINT_FORMAT_PROGRESS
#define writeMsg(str) \
  if (m_pr) m_pr->write(str)
#endif  // PRINT_FORMAT_PROGRESS
//------------------------------------------------------------------------------
bool FatFormatter::format(FsBlockDevice* dev, uint8_t* secBuf, print_t* pr) {
  bool rtn;
  m_dev = dev;
  m_secBuf = secBuf;
  m_pr = pr;
  m_sectorCount = m_dev->sectorCount();
  m_capacityMB = (m_sectorCount + SECTORS_PER_MB - 1) / SECTORS_PER_MB;

  if (m_capacityMB <= 6) {
    writeMsg("Card is too small.\r\n");
    return false;
  } else if (m_capacityMB <= 16) {
    m_sectorsPerCluster = 2;
  } else if (m_capacityMB <= 32) {
    m_sectorsPerCluster = 4;
  } else if (m_capacityMB <= 64) {
    m_sectorsPerCluster = 8;
  } else if (m_capacityMB <= 128) {
    m_sectorsPerCluster = 16;
  } else if (m_capacityMB <= 1024) {
    m_sectorsPerCluster = 32;
  } else if (m_capacityMB <= 32768) {
    m_sectorsPerCluster = 64;
  } else {
    // SDXC cards
    m_sectorsPerCluster = 128;
  }
  rtn = m_sectorCount < 0X400000 ? makeFat16() : makeFat32();
  if (rtn) {
    writeMsg("Format Done\r\n");
  } else {
    writeMsg("Format Failed\r\n");
  }
  return rtn;
}
//------------------------------------------------------------------------------
bool FatFormatter::initFatDir(uint8_t fatType, uint32_t sectorCount) {
  size_t n;
  memset(m_secBuf, 0, BYTES_PER_SECTOR);
  writeMsg("Writing FAT ");
  for (uint32_t i = 1; i < sectorCount; i++) {
    if (!m_dev->writeSector(m_fatStart + i, m_secBuf)) {
      return false;
    }
    if ((i % (sectorCount / 32)) == 0) {
      writeMsg(".");
    }
  }
  writeMsg("\r\n");
  // Allocate reserved clusters and root for FAT32.
  m_secBuf[0] = 0XF8;
  n = fatType == 16 ? 4 : 12;
  for (size_t i = 1; i < n; i++) {
    m_secBuf[i] = 0XFF;
  }
  return m_dev->writeSector(m_fatStart, m_secBuf) &&
         m_dev->writeSector(m_fatStart + m_fatSize, m_secBuf);
}
//------------------------------------------------------------------------------
void FatFormatter::initPbs() {
  PbsFat_t* pbs = reinterpret_cast<PbsFat_t*>(m_secBuf);
  memset(m_secBuf, 0, BYTES_PER_SECTOR);
  pbs->jmpInstruction[0] = 0XEB;
  pbs->jmpInstruction[1] = 0X76;
  pbs->jmpInstruction[2] = 0X90;
  for (uint8_t i = 0; i < sizeof(pbs->oemName); i++) {
    pbs->oemName[i] = ' ';
  }
  setLe16(pbs->bpb.bpb16.bytesPerSector, BYTES_PER_SECTOR);
  pbs->bpb.bpb16.sectorsPerCluster = m_sectorsPerCluster;
  setLe16(pbs->bpb.bpb16.reservedSectorCount, m_reservedSectorCount);
  pbs->bpb.bpb16.fatCount = 2;
  // skip rootDirEntryCount
  // skip totalSectors16
  pbs->bpb.bpb16.mediaType = 0XF8;
  // skip sectorsPerFat16
  // skip sectorsPerTrack
  // skip headCount
  setLe32(pbs->bpb.bpb16.hidddenSectors, m_relativeSectors);
  setLe32(pbs->bpb.bpb16.totalSectors32, m_totalSectors);
  // skip rest of bpb
  setLe16(pbs->signature, PBR_SIGNATURE);
}
//------------------------------------------------------------------------------
bool FatFormatter::makeFat16() {
  uint32_t nc;
  PbsFat_t* pbs = reinterpret_cast<PbsFat_t*>(m_secBuf);

  for (m_dataStart = 2 * BU16;; m_dataStart += BU16) {
    nc = (m_sectorCount - m_dataStart) / m_sectorsPerCluster;
    m_fatSize = (nc + 2 + (BYTES_PER_SECTOR / 2) - 1) / (BYTES_PER_SECTOR / 2);
    uint32_t r = BU16 + 1 + 2 * m_fatSize + FAT16_ROOT_SECTOR_COUNT;
    if (m_dataStart >= r) {
      m_relativeSectors = m_dataStart - r + BU16;
      break;
    }
  }
  // check valid cluster count for FAT16 volume
  if (nc < 4085 || nc >= 65525) {
    writeMsg("Bad cluster count\r\n");
    return false;
  }
  m_reservedSectorCount = 1;
  m_fatStart = m_relativeSectors + m_reservedSectorCount;
  m_totalSectors =
      nc * m_sectorsPerCluster + 2 * m_fatSize + m_reservedSectorCount + 32;
  if (m_totalSectors < 65536) {
    m_partType = 0X04;
  } else {
    m_partType = 0X06;
  }
  // write MBR
  if (!writeMbr()) {
    return false;
  }
  initPbs();
  setLe16(pbs->bpb.bpb16.rootDirEntryCount, FAT16_ROOT_ENTRY_COUNT);
  setLe16(pbs->bpb.bpb16.sectorsPerFat16, m_fatSize);
  pbs->bpb.bpb16.physicalDriveNumber = 0X80;
  pbs->bpb.bpb16.extSignature = EXTENDED_BOOT_SIGNATURE;
  setLe32(pbs->bpb.bpb16.volumeSerialNumber, 1234567);
  for (size_t i = 0; i < sizeof(pbs->bpb.bpb16.volumeLabel); i++) {
    pbs->bpb.bpb16.volumeLabel[i] = ' ';
  }
  pbs->bpb.bpb16.volumeType[0] = 'F';
  pbs->bpb.bpb16.volumeType[1] = 'A';
  pbs->bpb.bpb16.volumeType[2] = 'T';
  pbs->bpb.bpb16.volumeType[3] = '1';
  pbs->bpb.bpb16.volumeType[4] = '6';
  if (!m_dev->writeSector(m_relativeSectors, m_secBuf)) {
    return false;
  }
  return initFatDir(16, m_dataStart - m_fatStart);
}
//------------------------------------------------------------------------------
bool FatFormatter::makeFat32() {
  uint32_t nc;
  PbsFat_t* pbs = reinterpret_cast<PbsFat_t*>(m_secBuf);
  FsInfo_t* fsi = reinterpret_cast<FsInfo_t*>(m_secBuf);

  m_relativeSectors = BU32;
  for (m_dataStart = 2 * BU32;; m_dataStart += BU32) {
    nc = (m_sectorCount - m_dataStart) / m_sectorsPerCluster;
    m_fatSize = (nc + 2 + (BYTES_PER_SECTOR / 4) - 1) / (BYTES_PER_SECTOR / 4);
    uint32_t r = m_relativeSectors + 9 + 2 * m_fatSize;
    if (m_dataStart >= r) {
      break;
    }
  }
  // error if too few clusters in FAT32 volume
  if (nc < 65525) {
    writeMsg("Bad cluster count\r\n");
    return false;
  }
  m_reservedSectorCount = m_dataStart - m_relativeSectors - 2 * m_fatSize;
  m_fatStart = m_relativeSectors + m_reservedSectorCount;
  m_totalSectors = nc * m_sectorsPerCluster + m_dataStart - m_relativeSectors;
  // type depends on address of end sector
  // max CHS has lba = 16450560 = 1024*255*63
  if ((m_relativeSectors + m_totalSectors) <= 16450560) {
    // FAT32 with CHS and LBA
    m_partType = 0X0B;
  } else {
    // FAT32 with only LBA
    m_partType = 0X0C;
  }
  if (!writeMbr()) {
    return false;
  }
  initPbs();
  setLe32(pbs->bpb.bpb32.sectorsPerFat32, m_fatSize);
  setLe32(pbs->bpb.bpb32.fat32RootCluster, 2);
  setLe16(pbs->bpb.bpb32.fat32FSInfoSector, 1);
  setLe16(pbs->bpb.bpb32.fat32BackBootSector, 6);
  pbs->bpb.bpb32.physicalDriveNumber = 0X80;
  pbs->bpb.bpb32.extSignature = EXTENDED_BOOT_SIGNATURE;
  setLe32(pbs->bpb.bpb32.volumeSerialNumber, 1234567);
  for (size_t i = 0; i < sizeof(pbs->bpb.bpb32.volumeLabel); i++) {
    pbs->bpb.bpb32.volumeLabel[i] = ' ';
  }
  pbs->bpb.bpb32.volumeType[0] = 'F';
  pbs->bpb.bpb32.volumeType[1] = 'A';
  pbs->bpb.bpb32.volumeType[2] = 'T';
  pbs->bpb.bpb32.volumeType[3] = '3';
  pbs->bpb.bpb32.volumeType[4] = '2';
  if (!m_dev->writeSector(m_relativeSectors, m_secBuf) ||
      !m_dev->writeSector(m_relativeSectors + 6, m_secBuf)) {
    return false;
  }
  // write extra boot area and backup
  memset(m_secBuf, 0, BYTES_PER_SECTOR);
  setLe32(fsi->trailSignature, FSINFO_TRAIL_SIGNATURE);
  if (!m_dev->writeSector(m_relativeSectors + 2, m_secBuf) ||
      !m_dev->writeSector(m_relativeSectors + 8, m_secBuf)) {
    return false;
  }
  // write FSINFO sector and backup
  setLe32(fsi->leadSignature, FSINFO_LEAD_SIGNATURE);
  setLe32(fsi->structSignature, FSINFO_STRUCT_SIGNATURE);
  setLe32(fsi->freeCount, 0XFFFFFFFF);
  setLe32(fsi->nextFree, 0XFFFFFFFF);
  if (!m_dev->writeSector(m_relativeSectors + 1, m_secBuf) ||
      !m_dev->writeSector(m_relativeSectors + 7, m_secBuf)) {
    return false;
  }
  return initFatDir(32, 2 * m_fatSize + m_sectorsPerCluster);
}
//------------------------------------------------------------------------------
bool FatFormatter::writeMbr() {
  memset(m_secBuf, 0, BYTES_PER_SECTOR);
  MbrSector_t* mbr = reinterpret_cast<MbrSector_t*>(m_secBuf);

#if USE_LBA_TO_CHS
  lbaToMbrChs(mbr->part->beginCHS, m_capacityMB, m_relativeSectors);
  lbaToMbrChs(mbr->part->endCHS, m_capacityMB,
              m_relativeSectors + m_totalSectors - 1);
#else   // USE_LBA_TO_CHS
  mbr->part->beginCHS[0] = 1;
  mbr->part->beginCHS[1] = 1;
  mbr->part->beginCHS[2] = 0;
  mbr->part->endCHS[0] = 0XFE;
  mbr->part->endCHS[1] = 0XFF;
  mbr->part->endCHS[2] = 0XFF;
#endif  // USE_LBA_TO_CHS

  mbr->part->type = m_partType;
  setLe32(mbr->part->relativeSectors, m_relativeSectors);
  setLe32(mbr->part->totalSectors, m_totalSectors);
  setLe16(mbr->signature, MBR_SIGNATURE);
  return m_dev->writeSector(0, m_secBuf);
}
