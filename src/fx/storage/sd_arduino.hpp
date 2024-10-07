#pragma once

#include "fx/storage/sd.h"

#include <SPI.h>
#ifdef USE_SDFAT
#include <SdFat.h>
#else
#include <SD.h>
#endif

#include "namespace.h"
#include "ptr.h"

FASTLED_NAMESPACE_BEGIN

namespace storage {

class SdCardSpiArduino : public ISdCardSpi {
private:
    int _cs_pin;
#ifdef USE_SDFAT
    SdFat _sd;
#endif

public:
    SdCardSpiArduino(int cs_pin) : _cs_pin(cs_pin) {}

    bool begin(int chipSelect) override {
#ifdef USE_SDFAT
        return _sd.begin(chipSelect, SPI_HALF_SPEED);
#else
        return SD.begin(chipSelect);
#endif
    }

    void end() override {
        // SD library doesn't have an end() method
    }

    FileHandlePtr open(const char *name, uint8_t oflag) override {
        auto nullout = RefPtr<FileHandle>::FromHeap(nullptr);
#ifdef USE_SDFAT
        SdFile file;
        if (!file.open(name, oflag)) {
            return nullout
        }
        // Implement a wrapper class for SdFile that inherits from FileHandle
        // and return it here
#else
        File file = SD.open(name);
        if (!file) {
            return nullout;
        }
        // Implement a wrapper class for File that inherits from FileHandle
        // and return it here
#endif
        // For now, return nullptr as we haven't implemented the wrapper classes yet
        return nullout;
    }

    void close(FileHandle *file) override {
        // The close operation should be handled in the FileHandle wrapper classes
        // This method might not be necessary if we manage closing in the destructor
        // of our FileHandle wrapper classes
    }
};

ISdCardSpi *createSdCardSpi(int cs_pin) {
    return new SdCardSpiArduino(cs_pin);
}

} // namespace storage

FASTLED_NAMESPACE_END
