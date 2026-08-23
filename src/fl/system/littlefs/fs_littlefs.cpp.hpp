/// @file fl/system/littlefs/fs_littlefs.cpp.hpp
/// @brief Arduino LittleFS implementation of `FsImpl`.
///
/// LittleFS is the on-chip flash filesystem shipped by the ESP32, ESP8266
/// and RP2040 Arduino cores. Unlike SD it needs no CS pin or SPI bus — it
/// lives in a flash partition, which is why mounting fails for reasons SD
/// never sees: no filesystem partition in the partition table, or an
/// unformatted one.
///
/// This file includes external Arduino headers, so it is only ever pulled
/// in by `src/fl/build/fl.system.littlefs+.cpp` — never through FastLED's
/// header graph. The linker drops the whole TU when a sketch does not
/// reference `make_littlefs_filesystem()`.

#include "fl/stl/compiler_control.h"
#include "fl/stl/has_include.h"
#include "fl/system/littlefs/fs_littlefs.h"
#include "platforms/is_platform.h"

#if FL_HAS_INCLUDE(<LittleFS.h>) && FL_HAS_INCLUDE(<FS.h>)

// IWYU pragma: begin_keep
#include <FS.h>
#include <LittleFS.h>
// IWYU pragma: end_keep

#include "fl/stl/detail/file_handle.h"
#include "fl/stl/memory.h"
#include "fl/stl/string.h"

#define FASTLED_HAS_LITTLEFS_IMPL 1

namespace fl {

namespace {

/// `filebuf` over a LittleFS `File`. The File API mirrors Arduino SD's,
/// so this parallels `SDFileHandle` in `fl/system/sd/`.
class LittleFsFileHandle : public filebuf {
  private:
    File _file;
    string _path;

  public:
    LittleFsFileHandle(File file, const char *path)
        : _file(file), _path(path) {}

    ~LittleFsFileHandle() FL_NO_EXCEPT override {
        if (_file) {
            _file.close();
        }
    }

    bool is_open() const FL_NO_EXCEPT override {
        auto f = const_cast<File &>(_file);
        return static_cast<bool>(f);
    }

    bool available() const FL_NO_EXCEPT override {
        auto f = const_cast<File &>(_file);
        return f.available() > 0;
    }

    fl::size_t size() const FL_NO_EXCEPT override {
        auto f = const_cast<File &>(_file);
        return f.size();
    }

    fl::size_t read(char *dst, fl::size_t bytesToRead) FL_NO_EXCEPT override {
        return _file.read(reinterpret_cast<u8 *>(dst), bytesToRead); // ok reinterpret cast
    }
    using filebuf::read;

    fl::size_t write(const char *data, fl::size_t count) FL_NO_EXCEPT override {
        // Read-only, matching the SD backend. Sketches needing to write
        // should drive the platform LittleFS API directly.
        FASTLED_UNUSED(data);
        FASTLED_UNUSED(count);
        return 0;
    }

    fl::size_t tell() FL_NO_EXCEPT override {
        auto f = const_cast<File &>(_file);
        return f.position();
    }

    const char *path() const FL_NO_EXCEPT override { return _path.c_str(); }

    bool seek(fl::size_t pos, fl::seek_dir dir) FL_NO_EXCEPT override {
        auto f = const_cast<File &>(_file);
        if (dir == fl::seek_dir::beg) {
            return _file.seek(pos);
        }
        if (dir == fl::seek_dir::cur) {
            return _file.seek(f.position() + pos);
        }
        return _file.seek(f.size() + pos);
    }
    using filebuf::seek;

    void close() FL_NO_EXCEPT override {
        if (_file) {
            _file.close();
        }
    }

    bool is_eof() const FL_NO_EXCEPT override {
        auto f = const_cast<File &>(_file);
        return f.available() <= 0;
    }

    bool has_error() const override { return false; }
    void clear_error() override {}
    int error_code() const override { return 0; }
    const char *error_message() const override { return "No error"; }
};

class FsLittleFs : public FsImpl {
  private:
    bool _format_on_fail;

  public:
    explicit FsLittleFs(bool format_on_fail)
        : _format_on_fail(format_on_fail) {}

    bool begin() FL_NO_EXCEPT override {
#if defined(FL_IS_ESP32)
        // ESP32's core takes (formatOnFail, basePath, maxOpenFiles,
        // partitionLabel); the remaining parameters default sensibly.
        return LittleFS.begin(_format_on_fail);
#else
        // ESP8266 / RP2040 cores expose a no-argument begin() with no
        // format-on-fail hook, so an unformatted filesystem simply fails
        // to mount rather than being reformatted behind the user's back.
        FASTLED_UNUSED(_format_on_fail);
        return LittleFS.begin();
#endif
    }

    void end() FL_NO_EXCEPT override { LittleFS.end(); }

    filebuf_ptr openRead(const char *name) FL_NO_EXCEPT override {
        File file = LittleFS.open(name, "r");
        if (!file) {
            return filebuf_ptr();
        }
        return fl::make_shared<LittleFsFileHandle>(fl::move(file), name);
    }
};

} // namespace

FsImplPtr make_littlefs_filesystem(bool format_on_fail) FL_NO_EXCEPT {
    return fl::make_shared<FsLittleFs>(format_on_fail);
}

} // namespace fl

#else // no LittleFS on this platform

#define FASTLED_HAS_LITTLEFS_IMPL 0

namespace fl {

// Fallback so a sketch calling make_littlefs_filesystem() on a platform
// without LittleFS gets a null filesystem — and therefore a clean
// `begin()` failure — instead of a link error.
//
// Deliberately a strong definition, unlike the SD fallback. The two
// branches of this file are mutually exclusive, so exactly one
// definition of this symbol exists in any build and there is nothing for
// a weak symbol to defer to. It also has to be strong to be exported
// from the shared library on Windows, which the host test build links
// against.
FsImplPtr make_littlefs_filesystem(bool format_on_fail) FL_NO_EXCEPT {
    FASTLED_UNUSED(format_on_fail);
    return FsImplPtr();
}

} // namespace fl

#endif // FL_HAS_INCLUDE(<LittleFS.h>) && FL_HAS_INCLUDE(<FS.h>)
