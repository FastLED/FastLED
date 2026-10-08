#pragma once

#include "fl/stl/int.h"
#include "fl/stl/shared_ptr.h"  // filebuf_ptr
#include "fl/stl/detail/file_io.h"  // For fl::FILE* and fl::fopen/fclose/etc.
#include "fl/stl/span.h"
#include "fl/stl/string.h"
#include "fl/stl/noexcept.h"

// Unified file buffer abstraction (streambuf-style backend)
// This provides a platform-agnostic interface for file I/O operations.
// Platform-specific implementations subclass fl::filebuf.
// User-facing API is fl::ifstream / fl::ofstream (see fstream.h).

namespace fl {

struct CRGB; // Forward declaration for readRGB8

// Seek direction constants (match std::ios seekdir)
enum class seek_dir {
    beg = 0,  // Beginning of file
    cur = 1,  // Current position
    end = 2   // End of file
};

// Polymorphic file buffer backend (analogous to std::filebuf / std::streambuf).
// Platform implementations subclass this; consumers should prefer fl::ifstream.
class filebuf {
public:
    virtual ~filebuf() FL_NO_EXCEPT = default;

    // Core file operations (pure virtual)
    virtual bool is_open() const FL_NO_EXCEPT = 0;
    virtual void close() FL_NO_EXCEPT = 0;
    virtual fl::size_t read(char* buffer, fl::size_t count) FL_NO_EXCEPT = 0;
    virtual fl::size_t write(const char* data, fl::size_t count) FL_NO_EXCEPT = 0;
    virtual fl::size_t tell() FL_NO_EXCEPT = 0;
    virtual bool seek(fl::size_t pos, seek_dir dir) FL_NO_EXCEPT = 0;

    // Size and path (pure virtual)
    virtual fl::size_t size() const FL_NO_EXCEPT = 0;
    virtual const char* path() const FL_NO_EXCEPT = 0;

    // State queries (pure virtual)
    virtual bool is_eof() const FL_NO_EXCEPT = 0;
    virtual bool has_error() const FL_NO_EXCEPT = 0;
    virtual void clear_error() FL_NO_EXCEPT = 0;

    // Error reporting (pure virtual)
    virtual int error_code() const FL_NO_EXCEPT = 0;
    virtual const char* error_message() const FL_NO_EXCEPT = 0;

    // Default implementations (can be overridden)
    virtual bool available() const FL_NO_EXCEPT { return is_open() && !is_eof(); }
    virtual fl::size_t bytes_left() const FL_NO_EXCEPT;

    // Convenience: read into u8 buffer
    fl::size_t read(fl::u8* dst, fl::size_t n) FL_NO_EXCEPT {
        return read(reinterpret_cast<char*>(dst), n); // ok reinterpret cast
    }
    fl::size_t read(fl::span<fl::u8> dst) FL_NO_EXCEPT {
        return read(dst.data(), dst.size());
    }

    // Convenience: read RGB8 pixels (3 bytes per pixel)
    fl::size_t readRGB8(fl::span<CRGB> dst) FL_NO_EXCEPT {
        return read(reinterpret_cast<char*>(dst.data()), dst.size() * 3) / 3; // ok reinterpret cast
    }

    // Convenience: check if n bytes are available
    bool available(fl::size_t n) const FL_NO_EXCEPT { return bytes_left() >= n; }

    // Backward-compatibility methods (non-virtual, call through to new API)
    bool valid() const FL_NO_EXCEPT { return is_open(); }
    fl::size_t pos() const FL_NO_EXCEPT;
    bool seek(fl::size_t p) FL_NO_EXCEPT { return seek(p, seek_dir::beg); }
    fl::size_t bytesLeft() const FL_NO_EXCEPT { return bytes_left(); }
};

// Owning handle to a filebuf. Declared here, beside the type it points at,
// so anything using the interface gets the alias from the same include.
// Several headers redeclare this identical alias for their own closure;
// duplicate identical using-declarations are legal, so they still compile.
using filebuf_ptr = fl::shared_ptr<filebuf>;

namespace detail {

// ============================================================================
// POSIX File Buffer Implementation (uses fl::FILE* abstraction)
// ============================================================================

class posix_filebuf : public fl::filebuf {
private:
    fl::FILE* mFile;
    int mLastError;
    fl::string mPath;

    void captureError() FL_NO_EXCEPT;
    void clearErrorState() FL_NO_EXCEPT;

public:
    posix_filebuf() FL_NO_EXCEPT : mFile(nullptr), mLastError(0) {}

    explicit posix_filebuf(const char* path, const char* mode) FL_NO_EXCEPT;

    ~posix_filebuf() FL_NO_EXCEPT override;

    // Non-copyable
    posix_filebuf(const posix_filebuf&) FL_NO_EXCEPT = delete;
    posix_filebuf& operator=(const posix_filebuf&) = delete;

    // Moveable
    posix_filebuf(posix_filebuf&& other) FL_NO_EXCEPT;

    posix_filebuf& operator=(posix_filebuf&& other) FL_NO_EXCEPT;

    bool is_open() const FL_NO_EXCEPT override;

    void close() FL_NO_EXCEPT override;

    fl::size_t read(char* buffer, fl::size_t count) FL_NO_EXCEPT override;
    using filebuf::read; // Pull in u8 overload

    fl::size_t write(const char* data, fl::size_t count) FL_NO_EXCEPT override;

    fl::size_t tell() FL_NO_EXCEPT override;

    bool seek(fl::size_t pos, seek_dir dir) FL_NO_EXCEPT override;
    using filebuf::seek; // Pull in single-arg overload

    fl::size_t size() const FL_NO_EXCEPT override;

    const char* path() const FL_NO_EXCEPT override;

    bool is_eof() const FL_NO_EXCEPT override;

    bool has_error() const FL_NO_EXCEPT override;

    void clear_error() FL_NO_EXCEPT override;

    int error_code() const FL_NO_EXCEPT override;

    const char* error_message() const FL_NO_EXCEPT override;
};

} // namespace detail
} // namespace fl
