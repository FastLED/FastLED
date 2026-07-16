#pragma once

// Upstream-derived from the USE_SDFAT branch of
// src/platforms/fs_sdcard_arduino.hpp.

// IWYU pragma: begin_keep
#include <SPI.h>
#include "platforms/arm/teensy/sdfat/SdFat.h"
// IWYU pragma: end_keep

#include "fl/stl/memory.h"
#include "fl/system/file_system.h"

namespace fl { namespace platforms { namespace teensy {

struct SdFatState {
    sdfat::SdFat sd;
    bool begun = false;
};

// Defined in fs_sdfat_teensy.cpp.hpp so this private header never exposes
// the singleton implementation or its storage.
SdFatState &sdfat_state() FL_NO_EXCEPT;

class SdFatFileHandle : public filebuf {
  private:
    sdfat::SdFile mFile;
    fl::string mPath;

  public:
    SdFatFileHandle(sdfat::SdFile file, const char *path)
        : mFile(fl::move(file)), mPath(path) {}

    ~SdFatFileHandle() override {
        close();
    }

    bool is_open() const FL_NO_EXCEPT override {
        return mFile.isOpen();
    }

    bool available() const FL_NO_EXCEPT override {
        auto *self = const_cast<SdFatFileHandle *>(this);
        return self->mFile.available() > 0;
    }

    fl::size_t size() const FL_NO_EXCEPT override {
        return mFile.fileSize();
    }

    fl::size_t read(char *dst, fl::size_t bytes) FL_NO_EXCEPT override {
        return mFile.read(reinterpret_cast<fl::u8 *>(dst), bytes); // okay reinterpret cast
    }
    using filebuf::read;

    fl::size_t write(const char *data, fl::size_t count) FL_NO_EXCEPT override {
        FASTLED_UNUSED(data);
        FASTLED_UNUSED(count);
        return 0;
    }

    fl::size_t tell() FL_NO_EXCEPT override {
        return mFile.curPosition();
    }

    bool seek(fl::size_t pos, fl::seek_dir dir) FL_NO_EXCEPT override {
        if (dir == fl::seek_dir::beg) {
            return mFile.seekSet(pos);
        }
        if (dir == fl::seek_dir::cur) {
            return mFile.seekCur(pos);
        }
        return mFile.seekEnd(pos);
    }
    using filebuf::seek;

    const char *path() const FL_NO_EXCEPT override {
        return mPath.c_str();
    }

    void close() FL_NO_EXCEPT override {
        if (mFile.isOpen()) {
            mFile.close();
        }
    }

    bool is_eof() const FL_NO_EXCEPT override {
        auto *self = const_cast<SdFatFileHandle *>(this);
        return self->mFile.available() <= 0;
    }

    bool has_error() const override { return false; }
    void clear_error() override {}
    int error_code() const override { return 0; }
    const char *error_message() const override { return "No error"; }
};

class FsSdFatTeensy : public FsImpl {
  private:
    int mCsPin;
    bool mBegun = false;

  public:
    explicit FsSdFatTeensy(int cs_pin) : mCsPin(cs_pin) {}

    bool begin() FL_NO_EXCEPT override {
        SdFatState &sd_state = sdfat_state();
        sd_state.begun = sd_state.sd.begin(mCsPin, SPI_HALF_SPEED);
        mBegun = sd_state.begun;
        return mBegun;
    }

    void end() FL_NO_EXCEPT override {
        mBegun = false;
    }

    filebuf_ptr openRead(const char *path) FL_NO_EXCEPT override {
        if (!mBegun || !sdfat_state().begun) {
            return filebuf_ptr();
        }

        sdfat::SdFile file;
        if (!file.open(path, O_READ)) {
            return filebuf_ptr();
        }
        return fl::make_shared<SdFatFileHandle>(fl::move(file), path);
    }
};

inline FsImplPtr make_private_sdcard_filesystem(int cs_pin) FL_NO_EXCEPT {
    return fl::make_shared<FsSdFatTeensy>(cs_pin);
}

} } } // namespace fl::platforms::teensy

namespace fl {

inline FsImplPtr make_sdcard_filesystem(int cs_pin) FL_NO_EXCEPT {
    return platforms::teensy::make_private_sdcard_filesystem(cs_pin);
}

} // namespace fl
