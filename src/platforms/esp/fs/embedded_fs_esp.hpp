#pragma once

// IWYU pragma: private

/// @file platforms/esp/fs/embedded_fs_esp.hpp
/// @brief LittleFS fragment backing `fl::getEmbeddedFs()` on ESP.
///
/// LittleFS is an implementation detail and stops here — the public API
/// says "embedded storage" (FastLED #4007). ESP32 and ESP8266 cores both
/// ship `<LittleFS.h>`; where they do not, the guard below degrades to the
/// same null result the no-op fragment gives, so a build with an ESP
/// target but no LittleFS library still compiles.
///
/// A plain header, not a `.cpp.hpp`: it composes into whichever translation
/// unit includes it, which is how the backend stays inside
/// `fl.system.embedded_fs+.cpp.o` and gets dropped together with the entry
/// point when a sketch never asks for embedded storage. Same shape as the
/// platform fragments behind `platforms/delay.h`.

#include "fl/stl/compiler_control.h"
#include "fl/stl/has_include.h"
#include "fl/system/file_system.h"
#include "platforms/is_platform.h"

#if FL_HAS_INCLUDE(<LittleFS.h>) && FL_HAS_INCLUDE(<FS.h>)

// IWYU pragma: begin_keep
#include <FS.h>
#include <LittleFS.h>
// IWYU pragma: end_keep

#include "fl/stl/detail/file_handle.h"
#include "fl/stl/memory.h"
#include "fl/stl/string.h"

namespace fl {
namespace platforms {
namespace detail {

/// `filebuf` over a LittleFS `File`. The File API mirrors Arduino SD's, so
/// this parallels `SDFileHandle` in `fl/system/sd/`.
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
        // The ESP8266 core exposes a no-argument begin() with no
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

} // namespace detail

inline FsImplPtr makeEmbeddedFs(bool format_on_fail) FL_NO_EXCEPT {
    return fl::make_shared<detail::FsLittleFs>(format_on_fail);
}

} // namespace platforms
} // namespace fl

#else // ESP target without the LittleFS library on the include path

namespace fl {
namespace platforms {

inline FsImplPtr makeEmbeddedFs(bool format_on_fail) FL_NO_EXCEPT {
    FASTLED_UNUSED(format_on_fail);
    return FsImplPtr();
}

} // namespace platforms
} // namespace fl

#endif // FL_HAS_INCLUDE(<LittleFS.h>) && FL_HAS_INCLUDE(<FS.h>)
