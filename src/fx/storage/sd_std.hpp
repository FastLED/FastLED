#pragma once

#include "namespace.h"
#include "ptr.h"

#if !__has_include(<SdFat.h>)
#error "SdFat.h is required for the SD card support"
#endif

#include <SdFat.h>

FASTLED_NAMESPACE_BEGIN

namespace storage
{

typedef SdFs MySdFat;
typedef FsFile MySdFile;
MySdFat sd;

#if HAS_SDIO_CLASS
#define SD_CONFIG SdioConfig(FIFO_SDIO)
#elif ENABLE_DEDICATED_SPI
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI)
#else  // HAS_SDIO_CLASS
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI)
#endif  // HAS_SDIO_CLASS



class SdFileHandle: public FileHandle {
  public:
    SdFileHandle(const char *name, uint8_t oflag) {
        mFile.open(name, oflag);
        mFile.getName(mName, sizeof(mName));
    }
    virtual ~SdFileHandle() {
        close();
    }
    virtual bool available() const { return mFile.isOpen(); }
    virtual size_t bytesLeft() const { return size() - pos(); }
    virtual size_t size() const { return mFile.fileSize(); }
    virtual size_t read(uint8_t *dst, size_t bytesToRead) {
        return mFile.read(dst, bytesToRead);
    }
    virtual size_t pos() const {
        return mFile.curPosition();
    }
    virtual const char* path() const {
        return mName;
    }
    virtual void seek(size_t pos) {
        mFile.seekSet(pos);
    }
    virtual void close() {
        if (mFile.isOpen()) {
            mFile.close();
        }
    }
    MySdFile mFile;
    char mName[256];
};


// Begin define of SdCardSpi

bool SdCardSpi::begin() {
    // Teensy specific implementation ignores the pins.
    //Serial.println("Initializing SD card...");
    // Initialize the SD.
    if (!sd.begin(SD_CONFIG)) {
        //sd.initErrorPrint(&Serial);
        return false;
    }
    return true;
}

void SdCardSpi::end() {
    sd.end();
}

FileHandlePtr SdCardSpi::open(const char *name, uint8_t oflag) {
    return FileHandlePtr(new SdFileHandle(name, oflag));
}

void SdCardSpi::close(FileHandle *file) {
    file->close();
    delete file;
}

} // namespace storage

FASTLED_NAMESPACE_END