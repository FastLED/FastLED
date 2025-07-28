#pragma once

// fs card arduino implementation.

#include "fs.h"

#include <SPI.h>
#ifdef USE_SDFAT
#include <SdFat.h>
#else
#include <SD.h>
#endif



#include "fl/namespace.h"
#include "fl/memory.h"
#include "fl/file_system.h"
#include "fl/type_traits.h"

namespace fl {

// Forward declare the smart pointer type for FsArduino
class FsArduino;
FASTLED_SMART_PTR(FsArduino);

#ifdef USE_SDFAT
class SdFatFileHandle : public FileHandle {
private:
    SdFile _file;
    Str _path;  // Changed to Str for proper memory management

public:
    SdFatFileHandle(SdFile file, const char* path) : _file(fl::move(file)), _path(path) {}
    ~SdFatFileHandle() override { 
        if (_file.isOpen()) {
            _file.close(); 
        }
    }

    bool available() const override {
        // SdFat's available() is not const, so we need const_cast
        auto f = const_cast<SdFatFileHandle*>(this);
        return f->_file.available() > 0;
    }
    fl::size size() const override { 
        return _file.fileSize(); 
    }
    fl::size read(uint8_t *dst, fl::size bytesToRead) override { 
        return _file.read(dst, bytesToRead); 
    }
    fl::size pos() const override { 
        return _file.curPosition(); 
    }
    const char* path() const override { 
        return _path.c_str(); 
    }
    bool seek(fl::size pos) override { 
        return _file.seekSet(pos); 
    }
    void close() override { 
        if (_file.isOpen()) {
            _file.close(); 
        }
    }
    bool valid() const override { 
        return _file.isOpen(); 
    }
};
#else
class SDFileHandle : public FileHandle {
private:
    File _file;
    Str _path;  // Changed to Str for proper memory management

public:
    SDFileHandle(File file, const char* path) : _file(file), _path(path) {}
    ~SDFileHandle() override { 
        if (_file) {
            _file.close(); 
        }
    }

    bool available() const override {
        // Arduino's available() is not const, so we need const_cast
        auto f = const_cast<File&>(_file);
        return f.available() > 0;
    }
    fl::size size() const override { 
        // Arduino's size() is not const, so we need const_cast
        auto f = const_cast<File&>(_file);
        return f.size(); 
    }
    fl::size read(uint8_t *dst, fl::size bytesToRead) override { 
        return _file.read(dst, bytesToRead); 
    }
    fl::size pos() const override { 
        // Arduino's position() is not const, so we need const_cast
        auto f = const_cast<File&>(_file);
        return f.position(); 
    }
    const char* path() const override { 
        return _path.c_str(); 
    }
    bool seek(fl::size pos) override { 
        return _file.seek(pos); 
    }
    void close() override { 
        if (_file) {
            _file.close(); 
        }
    }
    bool valid() const override { 
        // Arduino's operator bool() is not const, so we need const_cast
        auto f = const_cast<File&>(_file);
        return f; 
    }
};
#endif

class FsArduino : public FsImpl {
private:
    int _cs_pin;
#ifdef USE_SDFAT
    SdFat _sd;
#endif

public:
    FsArduino(int cs_pin) : _cs_pin(cs_pin) {}

    bool begin() override {
#ifdef USE_SDFAT
        // Use the CS pin provided in constructor
        return _sd.begin(_cs_pin, SPI_HALF_SPEED);
#else
        return SD.begin(_cs_pin);
#endif
    }

    void end() override {
        // SD library doesn't have an end() method, but we can ensure files are closed
        // Note: This is a limitation of the Arduino SD library
    }

    FileHandlePtr openRead(const char *name) override {
#ifdef USE_SDFAT
        SdFile file;
        // Open file for reading
        if (!file.open(name, O_READ)) {
            return FileHandlePtr();
        }
        return fl::make_shared<SdFatFileHandle>(fl::move(file), name);
#else
        File file = SD.open(name, FILE_READ);
        if (!file) {
            return FileHandlePtr();
        }
        return fl::make_shared<SDFileHandle>(fl::move(file), name);
#endif
    }

    void close(FileHandlePtr file) override {
        // The close operation is now handled in the FileHandle wrapper classes
        // This method ensures the file is properly closed
        if (file) {
            file->close();
        }
    }
};

// Implementation of the factory function to create SD card filesystem
inline FsImplPtr make_sdcard_filesystem(int cs_pin) {
    return fl::make_shared<FsArduino>(cs_pin);
}

} // namespace fl
