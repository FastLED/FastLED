#pragma once

// IWYU pragma: private

/// @file platforms/arduino/fs/embedded_fs_arduino.hpp
/// @brief Optional Arduino LittleFS adapter for embedded storage.
///
/// On ESP32 an application's strong embedded-storage provider includes this
/// fragment and calls makeArduinoEmbeddedFs(). The application must mount
/// LittleFS first and keep it mounted until all adapters and files are done.
/// The adapter only borrows it: no begin(), formatting or end() on the owner.
/// The default ESP32 backend uses the SDK directly instead of this fragment.
/// ESP8266 retains the original opt-in Arduino mount lifecycle.

#include "fl/stl/compiler_control.h"
#include "fl/stl/has_include.h"
#include "fl/fs/fs.h"
#include "platforms/is_platform.h"

#if FL_HAS_INCLUDE(<LittleFS.h>) && FL_HAS_INCLUDE(<FS.h>)

// IWYU pragma: begin_keep
#include <FS.h>
#include <LittleFS.h>
// IWYU pragma: end_keep

#include "fl/fs/file_handle.h"
#include "fl/stl/memory.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/string.h"

namespace fl {
namespace platforms {
namespace detail {

#if defined(FL_IS_ESP32)
// ESP32 adapters borrow application-owned LittleFS. Arduino begin() cannot
// distinguish newly mounted from already mounted under an external race, so
// this adapter never mounts, formats or unmounts that global object.
struct ArduinoEmbeddedToken { void reset() FL_NO_EXCEPT {} };
class ArduinoEmbeddedLifetime {
    bool mReady = false;
public:
    bool begin(bool format) FL_NO_EXCEPT {
        (void)format;
        File root = LittleFS.open("/", "r");
        mReady = root && root.isDirectory();
        root.close();
        return mReady;
    }
    bool ready() const FL_NO_EXCEPT { return mReady; }
    void end() FL_NO_EXCEPT { mReady = false; }
    ArduinoEmbeddedToken fileLease() const FL_NO_EXCEPT { return ArduinoEmbeddedToken(); }
};
#else
// ESP8266 retains its original Arduino-owned mount lifecycle and opt-in.
struct ArduinoEmbeddedToken { void reset() FL_NO_EXCEPT {} };
class ArduinoEmbeddedLifetime {
    bool mReady = false;
public:
    bool begin(bool format) FL_NO_EXCEPT {
        (void)format;
        mReady = LittleFS.begin();
        return mReady;
    }
    bool ready() const FL_NO_EXCEPT { return mReady; }
    void end() FL_NO_EXCEPT { LittleFS.end(); mReady = false; }
    ArduinoEmbeddedToken fileLease() const FL_NO_EXCEPT { return ArduinoEmbeddedToken(); }
};
#endif

/// `filebuf` over a LittleFS `File`. The File API mirrors Arduino SD's, so
/// this parallels `SDFileHandle` in `fl/system/sd/`.
class LittleFsFileHandle : public filebuf {
  private:
    File _file;
    string _path;
    ArduinoEmbeddedToken _mount;

  public:
    LittleFsFileHandle(File file, const char *path, ArduinoEmbeddedToken mount) FL_NO_EXCEPT
        : _file(file), _path(path), _mount(fl::move(mount)) {}

    ~LittleFsFileHandle() FL_NO_EXCEPT override {
        close();
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
        if (_file) _file.close();
        _mount.reset();
    }

    bool is_eof() const FL_NO_EXCEPT override {
        auto f = const_cast<File &>(_file);
        return f.available() <= 0;
    }

    bool has_error() const FL_NO_EXCEPT override { return false; }
    void clear_error() FL_NO_EXCEPT override {}
    int error_code() const FL_NO_EXCEPT override { return 0; }
    const char *error_message() const FL_NO_EXCEPT override { return "No error"; }
};

class FsLittleFs : public FsImpl {
  private:
    bool _format_on_fail;
    ArduinoEmbeddedLifetime _lifetime;

  public:
    explicit FsLittleFs(bool format_on_fail) FL_NO_EXCEPT
        : _format_on_fail(format_on_fail) {}

    bool begin() FL_NO_EXCEPT override { return _lifetime.begin(_format_on_fail); }
    void end() FL_NO_EXCEPT override { _lifetime.end(); }

    filebuf_ptr openRead(const char *name) FL_NO_EXCEPT override {
        if (!_lifetime.ready() || !name) return filebuf_ptr();
        File file = LittleFS.open(name, "r");
        if (!file) {
            return filebuf_ptr();
        }
        return fl::make_shared<LittleFsFileHandle>(fl::move(file), name, _lifetime.fileLease());
    }
};

} // namespace detail

inline FsImplPtr makeArduinoEmbeddedFs(bool format_on_fail) FL_NO_EXCEPT {
    return fl::make_shared<detail::FsLittleFs>(format_on_fail);
}

} // namespace platforms
} // namespace fl

#else // ESP target without the LittleFS library on the include path

namespace fl {
namespace platforms {

inline FsImplPtr makeArduinoEmbeddedFs(bool format_on_fail) FL_NO_EXCEPT {
    FASTLED_UNUSED(format_on_fail);
    return FsImplPtr();
}

} // namespace platforms
} // namespace fl

#endif // FL_HAS_INCLUDE(<LittleFS.h>) && FL_HAS_INCLUDE(<FS.h>)
