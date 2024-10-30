#pragma once

#include "fs.h"

#include <SPI.h>
#ifdef USE_SDFAT
#include <SdFat.h>
#else
#include <SD.h>
#endif

#include "namespace.h"
#include "ref.h"

FASTLED_NAMESPACE_BEGIN

#ifdef USE_SDFAT
class SdFatFileHandle : public FileHandle {
private:
    SdFile _file;
    const char* _path;

public:
    SdFatFileHandle(SdFile file, const char* path) : _file(std::move(file)), _path(path) {}
    ~SdFatFileHandle() override { _file.close(); }

    bool available() const override {
        // return _file.available();
        auto f = const_cast<SdFatFileHandle*>(this);
        return f->_file.available();
    }
    size_t size() const override { return _file.fileSize(); }
    size_t read(uint8_t *dst, size_t bytesToRead) override { return _file.read(dst, bytesToRead); }
    size_t pos() const override { return _file.curPosition(); }
    const char* path() const override { return _path; }
    void seek(size_t pos) override { _file.seekSet(pos); }
    void close() override { _file.close(); }
};
#else
class SDFileHandle : public FileHandle {
private:
    File _file;
    const char* _path;

public:
    SDFileHandle(File file, const char* path) : _file(file), _path(path) {}
    ~SDFileHandle() override { _file.close(); }

    bool available() const override {
        // return _file.available();
        auto f = const_cast<File&>(_file);
        return f.available();
    }
    size_t size() const override { return _file.size(); }
    size_t read(uint8_t *dst, size_t bytesToRead) override { return _file.read(dst, bytesToRead); }
    size_t pos() const override { return _file.position(); }
    const char* path() const override { return _path; }
    void seek(size_t pos) override { _file.seek(pos); }
    void close() override { _file.close(); }
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
        return _sd.begin(chipSelect, SPI_HALF_SPEED);
#else
        return SD.begin(_cs_pin);
#endif
    }

    void end() override {
        // SD library doesn't have an end() method
    }

    FileHandleRef openRead(const char *name) override {
#ifdef USE_SDFAT
        SdFile file;
        if (!file.open(name, oflag)) {
            return Ref<FileHandle>::TakeOwnership(nullptr);
        }
        return Ref<FileHandle>::TakeOwnership(new SdFatFileHandle(std::move(file), name));
#else

        #ifdef ESP32
        File file = SD.open(name);
        #else
        File file = SD.open(name);
        #endif
        if (!file) {
            return Ref<FileHandle>::TakeOwnership(nullptr);
        }
        return Ref<FileHandle>::TakeOwnership(new SDFileHandle(std::move(file), name));
#endif
    }

    void close(FileHandleRef file) override {
        // The close operation is now handled in the FileHandle wrapper classes
        // This method is no longer necessary, but we keep it for compatibility
        if (file) {
            file->close();
        }
    }
};

inline FsImplRef make_filesystem(int cs_pin) {
    return FsImplRef::Null();
}


FASTLED_NAMESPACE_END
