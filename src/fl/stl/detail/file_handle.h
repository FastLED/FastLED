#pragma once

#include "fl/stl/int.h"
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
    virtual ~filebuf() FL_NOEXCEPT = default;

    // Core file operations (pure virtual)
    virtual bool is_open() const = 0;
    virtual void close() = 0;
    virtual fl::size_t read(char* buffer, fl::size_t count) = 0;
    virtual fl::size_t write(const char* data, fl::size_t count) = 0;
    virtual fl::size_t tell() = 0;
    virtual bool seek(fl::size_t pos, seek_dir dir) = 0;

    // Size and path (pure virtual)
    virtual fl::size_t size() const = 0;
    virtual const char* path() const = 0;

    // State queries (pure virtual)
    virtual bool is_eof() const = 0;
    virtual bool has_error() const = 0;
    virtual void clear_error() = 0;

    // Error reporting (pure virtual)
    virtual int error_code() const = 0;
    virtual const char* error_message() const = 0;

    // Default implementations (can be overridden)
    virtual bool available() const { return is_open() && !is_eof(); }
    virtual fl::size_t bytes_left() const;

    // Convenience: read into u8 buffer
    fl::size_t read(fl::u8* dst, fl::size_t n) {
        return read(reinterpret_cast<char*>(dst), n); // ok reinterpret cast
    }
    fl::size_t read(fl::span<fl::u8> dst) {
        return read(dst.data(), dst.size());
    }

    // Convenience: read RGB8 pixels (3 bytes per pixel)
    fl::size_t readRGB8(fl::span<CRGB> dst) {
        return read(reinterpret_cast<char*>(dst.data()), dst.size() * 3) / 3; // ok reinterpret cast
    }

    // Convenience: check if n bytes are available
    bool available(fl::size_t n) const { return bytes_left() >= n; }

    // Backward-compatibility methods (non-virtual, call through to new API)
    bool valid() const { return is_open(); }
    fl::size_t pos() const;
    bool seek(fl::size_t p) { return seek(p, seek_dir::beg); }
    fl::size_t bytesLeft() const { return bytes_left(); }
};

namespace detail {

// ============================================================================
// POSIX File Buffer Implementation (uses fl::FILE* abstraction)
// ============================================================================

class posix_filebuf : public fl::filebuf {
private:
    fl::FILE* mFile;
    int mLastError;
    fl::string mPath;

    void captureError();
    void clearErrorState();

public:
    posix_filebuf() FL_NOEXCEPT : mFile(nullptr), mLastError(0) {}

    explicit posix_filebuf(const char* path, const char* mode);

    ~posix_filebuf() FL_NOEXCEPT override;

    // Non-copyable
    posix_filebuf(const posix_filebuf&) FL_NOEXCEPT = delete;
    posix_filebuf& operator=(const posix_filebuf&) = delete;

    // Moveable
    posix_filebuf(posix_filebuf&& other) FL_NOEXCEPT;

    posix_filebuf& operator=(posix_filebuf&& other) FL_NOEXCEPT;

    bool is_open() const override;

    void close() override;

    fl::size_t read(char* buffer, fl::size_t count) override;
    using filebuf::read; // Pull in u8 overload

    fl::size_t write(const char* data, fl::size_t count) override;

    fl::size_t tell() override;

    bool seek(fl::size_t pos, seek_dir dir) override;
    using filebuf::seek; // Pull in single-arg overload

    fl::size_t size() const override;

    const char* path() const override;

    bool is_eof() const override;

    bool has_error() const override;

    void clear_error() override;

    int error_code() const override;

    const char* error_message() const override;
};

} // namespace detail
} // namespace fl
