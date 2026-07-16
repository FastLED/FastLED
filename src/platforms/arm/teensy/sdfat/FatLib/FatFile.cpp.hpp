// IWYU pragma: private
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
// Private implementation unit, included by sdfat/_build.cpp.hpp.
#define DBG_FILE "FatFile.cpp"
#include "../common/DebugMacros.h"  // ok relative include; ok include path  // IWYU pragma: keep
#include "FatLib.h"  // ok include path

namespace fl { namespace platforms { namespace teensy { namespace sdfat {
//------------------------------------------------------------------------------
// Add a cluster to a file.
bool FatFile::addCluster() {
#if USE_FAT_FILE_FLAG_CONTIGUOUS
  fl::u32 cc = mCurCluster;
  if (!mVol->allocateCluster(mCurCluster, &mCurCluster)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (cc == 0) {
    mFlags |= FILE_FLAG_CONTIGUOUS;
  } else if (mCurCluster != (cc + 1)) {
    mFlags &= ~FILE_FLAG_CONTIGUOUS;
  }
  mFlags |= FILE_FLAG_DIR_DIRTY;
  return true;

 fail:
  return false;
#else  // USE_FAT_FILE_FLAG_CONTIGUOUS
  mFlags |= FILE_FLAG_DIR_DIRTY;
  return mVol->allocateCluster(mCurCluster, &mCurCluster);
#endif  // USE_FAT_FILE_FLAG_CONTIGUOUS
}
//------------------------------------------------------------------------------
// Add a cluster to a directory file and zero the cluster.
// Return with first sector of cluster in the cache.
bool FatFile::addDirCluster() {
  fl::u32 sector;
  fl::u8* pc;

  if (isRootFixed()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // max folder size
  if (mCurPosition >= 512UL*4095) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (!addCluster()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  sector = mVol->clusterStartSector(mCurCluster);
  for (fl::u8 i = 0; i < mVol->sectorsPerCluster(); i++) {
    pc = mVol->dataCachePrepare(sector + i, FsCache::CACHE_RESERVE_FOR_WRITE);
    if (!pc) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    fl::memset(pc, 0, mVol->bytesPerSector());
  }
  // Set position to EOF to avoid inconsistent curCluster/curPosition.
  mCurPosition += mVol->bytesPerCluster();
  return true;

 fail:
  return false;
}
//------------------------------------------------------------------------------
// cache a file's directory entry
// return pointer to cached entry or null for failure
DirFat_t* FatFile::cacheDirEntry(fl::u8 action) {
  fl::u8* pc = mVol->dataCachePrepare(mDirSector, action);
  DirFat_t* dir = reinterpret_cast<DirFat_t*>(pc);  // ok reinterpret cast
  if (!dir) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  return dir + (mDirIndex & 0XF);

 fail:
  return nullptr;
}
//------------------------------------------------------------------------------
bool FatFile::close() {
  bool rtn = sync();
  mAttributes = FILE_ATTR_CLOSED;
  mFlags = 0;
  return rtn;
}
//------------------------------------------------------------------------------
bool FatFile::contiguousRange(fl::u32* bgnSector, fl::u32* endSector) {
  // error if no clusters
  if (!isFile() || mFirstCluster == 0) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  for (fl::u32 c = mFirstCluster; ; c++) {
    fl::u32 next;
    fl::i8 fg = mVol->fatGet(c, &next);
    if (fg < 0) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    // check for contiguous
    if (fg == 0 || next != (c + 1)) {
      // error if not end of chain
      if (fg) {
        DBG_FAIL_MACRO;
        goto fail;
      }
#if USE_FAT_FILE_FLAG_CONTIGUOUS
      mFlags |= FILE_FLAG_CONTIGUOUS;
#endif  // USE_FAT_FILE_FLAG_CONTIGUOUS
      if (bgnSector) {
        *bgnSector = mVol->clusterStartSector(mFirstCluster);
      }
      if (endSector) {
        *endSector = mVol->clusterStartSector(c)
                     + mVol->sectorsPerCluster() - 1;
      }
      return true;
    }
  }

 fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::createContiguous(const char* path, fl::u32 size) {
  if (!open(FatVolume::cwv(), path, O_CREAT | O_EXCL | O_RDWR)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (preAllocate(size)) {
    return true;
  }
  close();
 fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::createContiguous(FatFile* dirFile,
                               const char* path, fl::u32 size) {
  if (!open(dirFile, path, O_CREAT | O_EXCL | O_RDWR)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (preAllocate(size)) {
    return true;
  }
  close();
 fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::dirEntry(DirFat_t* dst) {
  DirFat_t* dir;
  // Make sure fields on device are correct.
  if (!sync()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // read entry
  dir = cacheDirEntry(FsCache::CACHE_FOR_READ);
  if (!dir) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // copy to caller's struct
  fl::memcpy(dst, dir, sizeof(DirFat_t));
  return true;

 fail:
  return false;
}
//------------------------------------------------------------------------------
fl::u32 FatFile::dirSize() {
  fl::i8 fg;
  if (!isDir()) {
    return 0;
  }
  if (isRootFixed()) {
    return FS_DIR_SIZE*mVol->rootDirEntryCount();
  }
  fl::u16 n = 0;
  fl::u32 c = isRoot32() ? mVol->rootDirStart() : mFirstCluster;
  do {
    fg = mVol->fatGet(c, &c);
    if (fg < 0 || n > 4095) {
      return 0;
    }
    n += mVol->sectorsPerCluster();
  } while (fg);
  return 512UL*n;
}
//------------------------------------------------------------------------------
int FatFile::fgets(char* str, int num, char* delim) {
  char ch;
  int n = 0;
  int r = -1;
  while ((n + 1) < num && (r = read(&ch, 1)) == 1) {
    // delete CR
    if (ch == '\r') {
      continue;
    }
    str[n++] = ch;
    if (!delim) {
      if (ch == '\n') {
        break;
      }
    } else {
      if (fl::strchr(delim, ch)) {
        break;
      }
    }
  }
  if (r < 0) {
    // read error
    return -1;
  }
  str[n] = '\0';
  return n;
}
//------------------------------------------------------------------------------
void FatFile::fgetpos(fspos_t* pos) const {
  pos->position = mCurPosition;
  pos->cluster = mCurCluster;
}
//------------------------------------------------------------------------------
fl::u32 FatFile::firstSector() const {
  return mFirstCluster ? mVol->clusterStartSector(mFirstCluster) : 0;
}
//------------------------------------------------------------------------------
void FatFile::fsetpos(const fspos_t* pos) {
  mCurPosition = pos->position;
  mCurCluster = pos->cluster;
}
//------------------------------------------------------------------------------
bool FatFile::getAccessDate(fl::u16* pdate) {
  DirFat_t dir;
  if (!dirEntry(&dir)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  *pdate = getLe16(dir.accessDate);
  return true;

 fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::getCreateDateTime(fl::u16* pdate, fl::u16* ptime) {
  DirFat_t dir;
  if (!dirEntry(&dir)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  *pdate = getLe16(dir.createDate);
  *ptime = getLe16(dir.createTime);
  return true;

 fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::getModifyDateTime(fl::u16* pdate, fl::u16* ptime) {
  DirFat_t dir;
  if (!dirEntry(&dir)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  *pdate = getLe16(dir.modifyDate);
  *ptime = getLe16(dir.modifyTime);
  return true;

 fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::isBusy() {
  return mVol->isBusy();
}
//------------------------------------------------------------------------------
bool FatFile::mkdir(FatFile* parent, const char* path, bool pFlag) {
  FatName_t fname;
  FatFile tmpDir;

  if (isOpen() || !parent->isDir()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (isDirSeparator(*path)) {
    while (isDirSeparator(*path)) {
      path++;
    }
    if (!tmpDir.openRoot(parent->mVol)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    parent = &tmpDir;
  }
  while (1) {
    if (!parsePathName(path, &fname, &path)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (!*path) {
      break;
    }
    if (!open(parent, &fname, O_RDONLY)) {
      if (!pFlag || !mkdir(parent, &fname)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    }
    tmpDir = *this;
    parent = &tmpDir;
    close();
  }
  return mkdir(parent, &fname);

 fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::mkdir(FatFile* parent, FatName_t* fname) {
  fl::u32 sector;
  DirFat_t dot;
  DirFat_t* dir;
  fl::u8* pc;

  if (!parent->isDir()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // create a normal file
  if (!open(parent, fname, O_CREAT | O_EXCL | O_RDWR)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // convert file to directory
  mFlags = FILE_FLAG_READ;
  mAttributes = FILE_ATTR_SUBDIR;

  // allocate and zero first cluster
  if (!addDirCluster()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  mFirstCluster = mCurCluster;
  // Set to start of dir
  rewind();
  // force entry to device
  if (!sync()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // cache entry - should already be in cache due to sync() call
  dir = cacheDirEntry(FsCache::CACHE_FOR_WRITE);
  if (!dir) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // change directory entry attribute
  dir->attributes = FAT_ATTRIB_DIRECTORY;

  // make entry for '.'
  fl::memcpy(&dot, dir, sizeof(dot));
  dot.name[0] = '.';
  for (fl::u8 i = 1; i < 11; i++) {
    dot.name[i] = ' ';
  }

  // cache sector for '.'  and '..'
  sector = mVol->clusterStartSector(mFirstCluster);
  pc = mVol->dataCachePrepare(sector, FsCache::CACHE_FOR_WRITE);
  dir = reinterpret_cast<DirFat_t*>(pc);  // ok reinterpret cast
  if (!dir) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // copy '.' to sector
  fl::memcpy(&dir[0], &dot, sizeof(dot));
  // make entry for '..'
  dot.name[1] = '.';
  setLe16(dot.firstClusterLow, parent->mFirstCluster & 0XFFFF);
  setLe16(dot.firstClusterHigh, parent->mFirstCluster >> 16);
  // copy '..' to sector
  fl::memcpy(&dir[1], &dot, sizeof(dot));
  // write first sector
  return mVol->cacheSync();

 fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::open(const char* path, oflag_t oflag) {
  return open(FatVolume::cwv(), path, oflag);
}
//------------------------------------------------------------------------------
bool FatFile::open(FatVolume* vol, const char* path, oflag_t oflag) {
  return vol && open(vol->vwd(), path, oflag);
}
//------------------------------------------------------------------------------
bool FatFile::open(FatFile* dirFile, const char* path, oflag_t oflag) {
  FatFile tmpDir;
  FatName_t fname;

  // error if already open
  if (isOpen() || !dirFile->isDir()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (isDirSeparator(*path)) {
    while (isDirSeparator(*path)) {
      path++;
    }
    if (*path == 0) {
      return openRoot(dirFile->mVol);
    }
    if (!tmpDir.openRoot(dirFile->mVol)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    dirFile = &tmpDir;
  }
  while (1) {
    if (!parsePathName(path, &fname, &path)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (*path == 0) {
      break;
    }
    if (!open(dirFile, &fname, O_RDONLY)) {
      DBG_WARN_MACRO;
      goto fail;
    }
    tmpDir = *this;
    dirFile = &tmpDir;
    close();
  }
  return open(dirFile, &fname, oflag);

 fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::open(FatFile* dirFile, fl::u16 index, oflag_t oflag) {
  if (index) {
    // Find start of LFN.
    DirLfn_t* ldir;
    fl::u8 n = index < 20 ? index : 20;
    for (fl::u8 i = 1; i <= n; i++) {
      ldir = reinterpret_cast<DirLfn_t*>(dirFile->cacheDir(index - i));  // ok reinterpret cast
      if (!ldir) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      if (ldir->attributes != FAT_ATTRIB_LONG_NAME) {
        break;
      }
      if (ldir->order & FAT_ORDER_LAST_LONG_ENTRY) {
        if (!dirFile->seekSet(32UL*(index - i))) {
          DBG_FAIL_MACRO;
          goto fail;
        }
        break;
      }
    }
  } else {
    dirFile->rewind();
  }
  if (!openNext(dirFile, oflag)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (dirIndex() != index) {
    close();
    DBG_FAIL_MACRO;
    goto fail;
  }
  return true;

 fail:
  return false;
}
//------------------------------------------------------------------------------
// open a cached directory entry.
bool FatFile::openCachedEntry(FatFile* dirFile, fl::u16 dirIndex,
                              oflag_t oflag, fl::u8 lfnOrd) {
  fl::u32 firstCluster;
  fl::memset(this, 0, sizeof(FatFile));
  // location of entry in cache
  mVol = dirFile->mVol;
  mDirIndex = dirIndex;
  mDirCluster = dirFile->mFirstCluster;
  DirFat_t* dir = reinterpret_cast<DirFat_t*>(mVol->cacheAddress());  // ok reinterpret cast
  dir += 0XF & dirIndex;

  // Must be file or subdirectory.
  if (!isFileOrSubdir(dir)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  mAttributes = dir->attributes & FILE_ATTR_COPY;
  if (isFileDir(dir)) {
    mAttributes |= FILE_ATTR_FILE;
  }
  mLfnOrd = lfnOrd;

  switch (oflag & O_ACCMODE) {
    case O_RDONLY:
      if (oflag & O_TRUNC) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      mFlags = FILE_FLAG_READ;
      break;

    case O_RDWR:
      mFlags = FILE_FLAG_READ | FILE_FLAG_WRITE;
      break;

    case O_WRONLY:
      mFlags = FILE_FLAG_WRITE;
      break;

    default:
      DBG_FAIL_MACRO;
      goto fail;
  }

  if (mFlags & FILE_FLAG_WRITE) {
    if (isSubDir() || isReadOnly()) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }
  mFlags |= (oflag & O_APPEND ? FILE_FLAG_APPEND : 0);

  mDirSector = mVol->cacheSectorNumber();

  // copy first cluster number for directory fields
  firstCluster = ((fl::u32)getLe16(dir->firstClusterHigh) << 16)
                 | getLe16(dir->firstClusterLow);

  if (oflag & O_TRUNC) {
    if (firstCluster && !mVol->freeChain(firstCluster)) {
      DBG_FAIL_MACRO;
      goto fail;
    }

    // need to update directory entry
    mFlags |= FILE_FLAG_DIR_DIRTY;
  } else {
    mFirstCluster = firstCluster;
    mFileSize = getLe32(dir->fileSize);
  }
  if ((oflag & O_AT_END) && !seekSet(mFileSize)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  return true;

 fail:
  mAttributes = FILE_ATTR_CLOSED;
  mFlags = 0;
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::openCluster(FatFile* file) {
  if (file->mDirCluster == 0) {
    return openRoot(file->mVol);
  }
  fl::memset(this, 0, sizeof(FatFile));
  mAttributes = FILE_ATTR_SUBDIR;
  mFlags = FILE_FLAG_READ;
  mVol = file->mVol;
  mFirstCluster = file->mDirCluster;
  return true;
}
//------------------------------------------------------------------------------
bool FatFile::openNext(FatFile* dirFile, oflag_t oflag) {
  fl::u8 checksum = 0;
  DirLfn_t* ldir;
  fl::u8 lfnOrd = 0;
  fl::u16 index;

  // Check for not open and valid directory..
  if (isOpen() || !dirFile->isDir() || (dirFile->curPosition() & 0X1F)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  while (1) {
    // read entry into cache
    index = dirFile->curPosition()/FS_DIR_SIZE;
    DirFat_t* dir = dirFile->readDirCache();
    if (!dir) {
      if (dirFile->getError()) {
        DBG_FAIL_MACRO;
      }
      goto fail;
    }
    // done if last entry
    if (dir->name[0] == FAT_NAME_FREE) {
      goto fail;
    }
    // skip empty slot or '.' or '..'
    if (dir->name[0] == '.' || dir->name[0] == FAT_NAME_DELETED) {
      lfnOrd = 0;
    } else if (isFileOrSubdir(dir)) {
      if (lfnOrd && checksum != lfnChecksum(dir->name)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      if (!openCachedEntry(dirFile, index, oflag, lfnOrd)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      return true;
    } else if (isLongName(dir)) {
      ldir = reinterpret_cast<DirLfn_t*>(dir);  // ok reinterpret cast
      if (ldir->order & FAT_ORDER_LAST_LONG_ENTRY) {
        lfnOrd = ldir->order & 0X1F;
        checksum = ldir->checksum;
      }
    } else {
      lfnOrd = 0;
    }
  }

 fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::openRoot(FatVolume* vol) {
  // error if file is already open
  if (isOpen()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  fl::memset(this, 0, sizeof(FatFile));

  mVol = vol;
  switch (vol->fatType()) {
#if FAT12_SUPPORT
  case 12:
#endif  // FAT12_SUPPORT
  case 16:
    mAttributes = FILE_ATTR_ROOT_FIXED;
    break;

  case 32:
    mAttributes = FILE_ATTR_ROOT32;
    break;

  default:
    DBG_FAIL_MACRO;
    goto fail;
  }
  // read only
  mFlags = FILE_FLAG_READ;
  return true;

 fail:
  return false;
}
//------------------------------------------------------------------------------
int FatFile::peek() {
  fl::u32 curPosition = mCurPosition;
  fl::u32 curCluster = mCurCluster;
  int c = read();
  mCurPosition = curPosition;
  mCurCluster = curCluster;
  return c;
}
//------------------------------------------------------------------------------
bool FatFile::preAllocate(fl::u32 length) {
  fl::u32 need;
  if (!length || !isWritable() || mFirstCluster) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  need = 1 + ((length - 1) >> mVol->bytesPerClusterShift());
  // allocate clusters
  if (!mVol->allocContiguous(need, &mFirstCluster)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  mFileSize = length;

#if USE_FAT_FILE_FLAG_CONTIGUOUS
  // Mark contiguous and insure sync() will update dir entry
  mFlags |= FILE_FLAG_PREALLOCATE | FILE_FLAG_CONTIGUOUS | FILE_FLAG_DIR_DIRTY;
#else  // USE_FAT_FILE_FLAG_CONTIGUOUS
  // insure sync() will update dir entry
  mFlags |= FILE_FLAG_DIR_DIRTY;
#endif  // USE_FAT_FILE_FLAG_CONTIGUOUS
  return sync();

 fail:
  return false;
}
//------------------------------------------------------------------------------
int FatFile::read(void* buf, size_t nbyte) {
  fl::i8 fg;
  fl::u8 sectorOfCluster = 0;
  fl::u8* dst = reinterpret_cast<fl::u8*>(buf);  // ok reinterpret cast
  fl::u16 offset;
  size_t toRead;
  fl::u32 sector;  // raw device sector number
  fl::u8* pc;
  // error if not open for read
  if (!isReadable()) {
    DBG_FAIL_MACRO;
    goto fail;
  }

  if (isFile()) {
    fl::u32 tmp32 = mFileSize - mCurPosition;
    if (nbyte >= tmp32) {
      nbyte = tmp32;
    }
  } else if (isRootFixed()) {
    fl::u16 tmp16 =
      FS_DIR_SIZE*mVol->mRootDirEntryCount - (fl::u16)mCurPosition;
    if (nbyte > tmp16) {
      nbyte = tmp16;
    }
  }
  toRead = nbyte;
  while (toRead) {
    size_t n;
    offset = mCurPosition & mVol->sectorMask();  // offset in sector
    if (isRootFixed()) {
      sector = mVol->rootDirStart()
               + (mCurPosition >> mVol->bytesPerSectorShift());
    } else {
      sectorOfCluster = mVol->sectorOfCluster(mCurPosition);
      if (offset == 0 && sectorOfCluster == 0) {
        // start of new cluster
        if (mCurPosition == 0) {
          // use first cluster in file
          mCurCluster = isRoot32() ? mVol->rootDirStart() : mFirstCluster;
#if USE_FAT_FILE_FLAG_CONTIGUOUS
        } else if (isFile() && isContiguous()) {
          mCurCluster++;
#endif  // USE_FAT_FILE_FLAG_CONTIGUOUS
        } else {
          // get next cluster from FAT
          fg = mVol->fatGet(mCurCluster, &mCurCluster);
          if (fg < 0) {
            DBG_FAIL_MACRO;
            goto fail;
          }
          if (fg == 0) {
            if (isDir()) {
              break;
            }
            DBG_FAIL_MACRO;
            goto fail;
          }
        }
      }
      sector = mVol->clusterStartSector(mCurCluster) + sectorOfCluster;
    }
    if (offset != 0 || toRead < mVol->bytesPerSector()
        || sector == mVol->cacheSectorNumber()) {
      // amount to be read from current sector
      n = mVol->bytesPerSector() - offset;
      if (n > toRead) {
        n = toRead;
      }
      // read sector to cache and copy data to caller
      pc = mVol->dataCachePrepare(sector, FsCache::CACHE_FOR_READ);
      if (!pc) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      fl::u8* src = pc + offset;
      fl::memcpy(dst, src, n);
#if USE_MULTI_SECTOR_IO
    } else if (toRead >= 2*mVol->bytesPerSector()) {
      fl::u32 ns = toRead >> mVol->bytesPerSectorShift();
      if (!isRootFixed()) {
        fl::u32 mb = mVol->sectorsPerCluster() - sectorOfCluster;
        if (mb < ns) {
          ns = mb;
        }
      }
      n = ns << mVol->bytesPerSectorShift();
      if (!mVol->cacheSafeRead(sector, dst, ns)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
#endif  // USE_MULTI_SECTOR_IO
    } else {
      // read single sector
      n = mVol->bytesPerSector();
      if (!mVol->cacheSafeRead(sector, dst)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    }
    dst += n;
    mCurPosition += n;
    toRead -= n;
  }
  return nbyte - toRead;

 fail:
  mError |= READ_ERROR;
  return -1;
}
//------------------------------------------------------------------------------
fl::i8 FatFile::readDir(DirFat_t* dir) {
  fl::i16 n;
  // if not a directory file or miss-positioned return an error
  if (!isDir() || (0X1F & mCurPosition)) {
    return -1;
  }

  while (1) {
    n = read(dir, sizeof(DirFat_t));
    if (n != sizeof(DirFat_t)) {
      return n == 0 ? 0 : -1;
    }
    // last entry if FAT_NAME_FREE
    if (dir->name[0] == FAT_NAME_FREE) {
      return 0;
    }
    // skip empty entries and entry for .  and ..
    if (dir->name[0] == FAT_NAME_DELETED || dir->name[0] == '.') {
      continue;
    }
    // return if normal file or subdirectory
    if (isFileOrSubdir(dir)) {
      return n;
    }
  }
}
//------------------------------------------------------------------------------
// Read next directory entry into the cache.
// Assumes file is correctly positioned.
DirFat_t* FatFile::readDirCache(bool skipReadOk) {
  DBG_HALT_IF(mCurPosition & 0X1F);
  fl::u8 i = (mCurPosition >> 5) & 0XF;

  if (i == 0 || !skipReadOk) {
    fl::i8 n = read(&n, 1);
    if  (n != 1) {
      if (n != 0) {
        DBG_FAIL_MACRO;
      }
      goto fail;
    }
    mCurPosition += FS_DIR_SIZE - 1;
  } else {
    mCurPosition += FS_DIR_SIZE;
  }
  // return pointer to entry
  return reinterpret_cast<DirFat_t*>(mVol->cacheAddress()) + i;  // ok reinterpret cast

 fail:
  return nullptr;
}
//------------------------------------------------------------------------------
bool FatFile::remove(const char* path) {
  FatFile file;
  if (!file.open(this, path, O_WRONLY)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  return file.remove();

 fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::rename(const char* newPath) {
  return rename(mVol->vwd(), newPath);
}
//------------------------------------------------------------------------------
bool FatFile::rename(FatFile* dirFile, const char* newPath) {
  DirFat_t entry;
  fl::u32 dirCluster = 0;
  FatFile file;
  FatFile oldFile;
  fl::u8* pc;
  DirFat_t* dir;

  // Must be an open file or subdirectory.
  if (!(isFile() || isSubDir())) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // Can't rename LFN in 8.3 mode.
  if (!USE_LONG_FILE_NAMES && isLFN()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // Can't move file to new volume.
  if (mVol != dirFile->mVol) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // sync() and cache directory entry
  sync();
  oldFile = *this;
  dir = cacheDirEntry(FsCache::CACHE_FOR_READ);
  if (!dir) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // save directory entry
  fl::memcpy(&entry, dir, sizeof(entry));
  // make directory entry for new path
  if (isFile()) {
    if (!file.open(dirFile, newPath, O_CREAT | O_EXCL | O_WRONLY)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  } else {
    // don't create missing path prefix components
    if (!file.mkdir(dirFile, newPath, false)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    // save cluster containing new dot dot
    dirCluster = file.mFirstCluster;
  }
  // change to new directory entry

  mDirSector = file.mDirSector;
  mDirIndex = file.mDirIndex;
  mLfnOrd = file.mLfnOrd;
  mDirCluster = file.mDirCluster;
  // mark closed to avoid possible destructor close call
  file.mAttributes = FILE_ATTR_CLOSED;
  file.mFlags = 0;

  // cache new directory entry
  dir = cacheDirEntry(FsCache::CACHE_FOR_WRITE);
  if (!dir) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // copy all but name and name flags to new directory entry
  fl::memcpy(&dir->createTimeMs, &entry.createTimeMs,
         sizeof(entry) - sizeof(dir->name) - 2);
  dir->attributes = entry.attributes;

  // update dot dot if directory
  if (dirCluster) {
    // get new dot dot
    fl::u32 sector = mVol->clusterStartSector(dirCluster);
    pc = mVol->dataCachePrepare(sector, FsCache::CACHE_FOR_READ);
    dir = reinterpret_cast<DirFat_t*>(pc);  // ok reinterpret cast
    if (!dir) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    fl::memcpy(&entry, &dir[1], sizeof(entry));

    // free unused cluster
    if (!mVol->freeChain(dirCluster)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    // store new dot dot
    sector = mVol->clusterStartSector(mFirstCluster);
    fl::u8* pc = mVol->dataCachePrepare(sector, FsCache::CACHE_FOR_WRITE);
    dir = reinterpret_cast<DirFat_t*>(pc);  // ok reinterpret cast
    if (!dir) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    fl::memcpy(&dir[1], &entry, sizeof(entry));
  }
  // Remove old directory entry;
  oldFile.mFirstCluster = 0;
  oldFile.mFlags = FILE_FLAG_WRITE;
  oldFile.mAttributes = FILE_ATTR_FILE;
  if (!oldFile.remove()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  return mVol->cacheSync();

 fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::rmdir() {
  // must be open subdirectory
  if (!isSubDir() || (!USE_LONG_FILE_NAMES && isLFN())) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  rewind();

  // make sure directory is empty
  while (1) {
    DirFat_t* dir = readDirCache(true);
    if (!dir) {
      // EOF if no error.
      if (!getError()) {
        break;
      }
      DBG_FAIL_MACRO;
      goto fail;
    }
    // done if past last used entry
    if (dir->name[0] == FAT_NAME_FREE) {
      break;
    }
    // skip empty slot, '.' or '..'
    if (dir->name[0] == FAT_NAME_DELETED || dir->name[0] == '.') {
      continue;
    }
    // error not empty
    if (isFileOrSubdir(dir)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }
  // convert empty directory to normal file for remove
  mAttributes = FILE_ATTR_FILE;
  mFlags |= FILE_FLAG_WRITE;
  return remove();

 fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::rmRfStar() {
  fl::u16 index;
  FatFile f;
  if (!isDir()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  rewind();
  while (1) {
    // remember position
    index = mCurPosition/FS_DIR_SIZE;

    DirFat_t* dir = readDirCache();
    if (!dir) {
      // At EOF if no error.
      if (!getError()) {
        break;
      }
      DBG_FAIL_MACRO;
      goto fail;
    }
    // done if past last entry
    if (dir->name[0] == FAT_NAME_FREE) {
      break;
    }

    // skip empty slot or '.' or '..'
    if (dir->name[0] == FAT_NAME_DELETED || dir->name[0] == '.') {
      continue;
    }

    // skip if part of long file name or volume label in root
    if (!isFileOrSubdir(dir)) {
      continue;
    }

    if (!f.open(this, index, O_RDONLY)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (f.isSubDir()) {
      // recursively delete
      if (!f.rmRfStar()) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    } else {
      // ignore read-only
      f.mFlags |= FILE_FLAG_WRITE;
      if (!f.remove()) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    }
    // position to next entry if required
    if (mCurPosition != (32UL*(index + 1))) {
      if (!seekSet(32UL*(index + 1))) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    }
  }
  // don't try to delete root
  if (!isRoot()) {
    if (!rmdir()) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }
  return true;

 fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::seekSet(fl::u32 pos) {
  fl::u32 nCur;
  fl::u32 nNew;
  fl::u32 tmp = mCurCluster;
  // error if file not open
  if (!isOpen()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // Optimize O_APPEND writes.
  if (pos == mCurPosition) {
    return true;
  }
  if (pos == 0) {
    // set position to start of file
    mCurCluster = 0;
    goto done;
  }
  if (isFile()) {
    if (pos > mFileSize) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  } else if (isRootFixed()) {
    if (pos <= FS_DIR_SIZE*mVol->rootDirEntryCount()) {
      goto done;
    }
    DBG_FAIL_MACRO;
    goto fail;
  }
  // calculate cluster index for new position
  nNew = (pos - 1) >> (mVol->bytesPerClusterShift());
#if USE_FAT_FILE_FLAG_CONTIGUOUS
  if (isContiguous()) {
    mCurCluster = mFirstCluster + nNew;
    goto done;
  }
#endif  // USE_FAT_FILE_FLAG_CONTIGUOUS
  // calculate cluster index for current position
  nCur = (mCurPosition - 1) >> (mVol->bytesPerClusterShift());

  if (nNew < nCur || mCurPosition == 0) {
    // must follow chain from first cluster
    mCurCluster = isRoot32() ? mVol->rootDirStart() : mFirstCluster;
  } else {
    // advance from curPosition
    nNew -= nCur;
  }
  while (nNew--) {
    if (mVol->fatGet(mCurCluster, &mCurCluster) <= 0) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }

 done:
  mCurPosition = pos;
  mFlags &= ~FILE_FLAG_PREALLOCATE;
  return true;

 fail:
  mCurCluster = tmp;
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::sync() {
  fl::u16 date, time;
  fl::u8 ms10;
  if (!isOpen()) {
    return true;
  }
  if (mFlags & FILE_FLAG_DIR_DIRTY) {
    DirFat_t* dir = cacheDirEntry(FsCache::CACHE_FOR_WRITE);
    // check for deleted by another open file object
    if (!dir || dir->name[0] == FAT_NAME_DELETED) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    // do not set filesize for dir files
    if (isFile()) {
      setLe32(dir->fileSize, mFileSize);
    }
    // update first cluster fields
    setLe16(dir->firstClusterLow, mFirstCluster & 0XFFFF);
    setLe16(dir->firstClusterHigh, mFirstCluster >> 16);

    // set modify time if user supplied a callback date/time function
    if (FsDateTime::callback) {
      FsDateTime::callback(&date, &time, &ms10);
      setLe16(dir->modifyDate, date);
      setLe16(dir->accessDate, date);
      setLe16(dir->modifyTime, time);
    }
    // clear directory dirty
    mFlags &= ~FILE_FLAG_DIR_DIRTY;
  }
  if (mVol->cacheSync()) {
    return true;
  }
  DBG_FAIL_MACRO;

 fail:
  mError |= WRITE_ERROR;
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::timestamp(fl::u8 flags, fl::u16 year, fl::u8 month,
                   fl::u8 day, fl::u8 hour, fl::u8 minute, fl::u8 second) {
  fl::u16 dirDate;
  fl::u16 dirTime;
  DirFat_t* dir;

  if (!isFile()
      || year < 1980
      || year > 2107
      || month < 1
      || month > 12
      || day < 1
      || day > 31
      || hour > 23
      || minute > 59
      || second > 59) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // update directory entry
  if (!sync()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  dir = cacheDirEntry(FsCache::CACHE_FOR_WRITE);
  if (!dir) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  dirDate = FS_DATE(year, month, day);
  dirTime = FS_TIME(hour, minute, second);
  if (flags & T_ACCESS) {
    setLe16(dir->accessDate, dirDate);
  }
  if (flags & T_CREATE) {
    setLe16(dir->createDate, dirDate);
    setLe16(dir->createTime, dirTime);
    // units of 10 ms
    dir->createTimeMs = second & 1 ? 100 : 0;
  }
  if (flags & T_WRITE) {
    setLe16(dir->modifyDate, dirDate);
    setLe16(dir->modifyTime, dirTime);
  }
  return mVol->cacheSync();

 fail:
  return false;
}
//------------------------------------------------------------------------------
bool FatFile::truncate() {
  fl::u32 toFree;
  // error if not a normal file or read-only
  if (!isWritable()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (mFirstCluster == 0) {
      return true;
  }
  if (mCurCluster) {
    toFree = 0;
    fl::i8 fg = mVol->fatGet(mCurCluster, &toFree);
    if (fg < 0) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (fg) {
      // current cluster is end of chain
      if (!mVol->fatPutEOC(mCurCluster)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    }
  } else {
    toFree = mFirstCluster;
    mFirstCluster = 0;
  }
  if (toFree) {
    if (!mVol->freeChain(toFree)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }
  mFileSize = mCurPosition;

  // need to update directory entry
  mFlags |= FILE_FLAG_DIR_DIRTY;
  return sync();

 fail:
  return false;
}
//------------------------------------------------------------------------------
size_t FatFile::write(const void* buf, size_t nbyte) {
  // convert void* to fl::u8*  -  must be before goto statements
  const fl::u8* src = reinterpret_cast<const fl::u8*>(buf);  // ok reinterpret cast
  fl::u8* pc;
  fl::u8 cacheOption;
  // number of bytes left to write  -  must be before goto statements
  size_t nToWrite = nbyte;
  size_t n;
  // error if not a normal file or is read-only
  if (!isWritable()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // seek to end of file if append flag
  if ((mFlags & FILE_FLAG_APPEND)) {
    if (!seekSet(mFileSize)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }
  // Don't exceed max fileSize.
  if (nbyte > (0XFFFFFFFF - mCurPosition)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  while (nToWrite) {
    fl::u8 sectorOfCluster = mVol->sectorOfCluster(mCurPosition);
    fl::u16 sectorOffset = mCurPosition & mVol->sectorMask();
    if (sectorOfCluster == 0 && sectorOffset == 0) {
      // start of new cluster
      if (mCurCluster != 0) {
#if USE_FAT_FILE_FLAG_CONTIGUOUS
        fl::i8 fg;
        if (isContiguous() && mFileSize > mCurPosition) {
          mCurCluster++;
          fg = 1;
        } else {
          fg = mVol->fatGet(mCurCluster, &mCurCluster);
          if (fg < 0) {
            DBG_FAIL_MACRO;
            goto fail;
          }
        }
#else  // USE_FAT_FILE_FLAG_CONTIGUOUS
        fl::i8 fg = mVol->fatGet(mCurCluster, &mCurCluster);
        if (fg < 0) {
          DBG_FAIL_MACRO;
          goto fail;
        }
#endif  // USE_FAT_FILE_FLAG_CONTIGUOUS
        if (fg == 0) {
          // add cluster if at end of chain
          if (!addCluster()) {
            DBG_FAIL_MACRO;
            goto fail;
          }
        }
      } else {
        if (mFirstCluster == 0) {
          // allocate first cluster of file
          if (!addCluster()) {
            DBG_FAIL_MACRO;
            goto fail;
          }
          mFirstCluster = mCurCluster;
        } else {
          mCurCluster = mFirstCluster;
        }
      }
    }
    // sector for data write
    fl::u32 sector = mVol->clusterStartSector(mCurCluster)
                      + sectorOfCluster;

    if (sectorOffset != 0 || nToWrite < mVol->bytesPerSector()) {
      // partial sector - must use cache
      // max space in sector
      n = mVol->bytesPerSector() - sectorOffset;
      // lesser of space and amount to write
      if (n > nToWrite) {
        n = nToWrite;
      }

      if (sectorOffset == 0 &&
         (mCurPosition >= mFileSize || mFlags & FILE_FLAG_PREALLOCATE)) {
        // start of new sector don't need to read into cache
        cacheOption = FsCache::CACHE_RESERVE_FOR_WRITE;
      } else {
        // rewrite part of sector
        cacheOption = FsCache::CACHE_FOR_WRITE;
      }
      pc = mVol->dataCachePrepare(sector, cacheOption);
      if (!pc) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      fl::u8* dst = pc + sectorOffset;
      fl::memcpy(dst, src, n);
      if (mVol->bytesPerSector() == (n + sectorOffset)) {
        // Force write if sector is full - improves large writes.
        if (!mVol->cacheSyncData()) {
          DBG_FAIL_MACRO;
          goto fail;
        }
      }
#if USE_MULTI_SECTOR_IO
    } else if (nToWrite >= 2*mVol->bytesPerSector()) {
      // use multiple sector write command
      fl::u32 maxSectors = mVol->sectorsPerCluster() - sectorOfCluster;
      fl::u32 nSector = nToWrite >> mVol->bytesPerSectorShift();
      if (nSector > maxSectors) {
        nSector = maxSectors;
      }
      n = nSector << mVol->bytesPerSectorShift();
      if (!mVol->cacheSafeWrite(sector, src, nSector)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
#endif  // USE_MULTI_SECTOR_IO
    } else {
      // use single sector write command
      n = mVol->bytesPerSector();
      if (!mVol->cacheSafeWrite(sector, src)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    }
    mCurPosition += n;
    src += n;
    nToWrite -= n;
  }
  if (mCurPosition > mFileSize) {
    // update fileSize and insure sync will update dir entry
    mFileSize = mCurPosition;
    mFlags |= FILE_FLAG_DIR_DIRTY;
  } else if (FsDateTime::callback) {
    // insure sync will update modified date and time
    mFlags |= FILE_FLAG_DIR_DIRTY;
  }
  return nbyte;

 fail:
  // return for write error
  mError |= WRITE_ERROR;
  return 0;
}
}  // namespace sdfat
}  // namespace teensy
}  // namespace platforms
}  // namespace fl

#ifdef DBG_FILE
#undef DBG_FILE
#endif
