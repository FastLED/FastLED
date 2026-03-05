#pragma once

#include "fl/int.h"
#include "fl/stl/detail/file_io.h"  // For fl::FILE* and fl::fopen/fclose/etc.
#include "fl/stl/span.h"
#include "fl/stl/string.h"

// Unified file handle abstraction
// This provides a platform-agnostic interface for file I/O operations.
// Platform-specific implementations subclass fl::FileHandle.

namespace fl {

struct CRGB; // Forward declaration for readRGB8

// Seek direction constants (match std::ios seekdir)
enum class seek_dir {
    beg = 0,  // Beginning of file
    cur = 1,  // Current position
    end = 2   // End of file
};

// Unified abstract interface for file operations.
// Replaces both the old ad-hoc FileHandle and file_handle_base.
class FileHandle {
public:
    virtual ~FileHandle() = default;

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

// Backward-compat alias for code using the old name
using file_handle_base = fl::FileHandle;

// ============================================================================
// POSIX File Handle Implementation (uses fl::FILE* abstraction)
// ============================================================================

class posix_file_handle : public fl::FileHandle {
private:
    fl::FILE* mFile;
    int mLastError;
    fl::string mPath;

    void captureError();
    void clearErrorState();

public:
    posix_file_handle() : mFile(nullptr), mLastError(0) {}

    explicit posix_file_handle(const char* path, const char* mode);

    ~posix_file_handle() override;

    // Non-copyable
    posix_file_handle(const posix_file_handle&) = delete;
    posix_file_handle& operator=(const posix_file_handle&) = delete;

    // Moveable
    posix_file_handle(posix_file_handle&& other) noexcept;

    posix_file_handle& operator=(posix_file_handle&& other) noexcept;

    bool is_open() const override;

    void close() override;

    fl::size_t read(char* buffer, fl::size_t count) override;
    using FileHandle::read; // Pull in u8 overload

    fl::size_t write(const char* data, fl::size_t count) override;

    fl::size_t tell() override;

    bool seek(fl::size_t pos, seek_dir dir) override;
    using FileHandle::seek; // Pull in single-arg overload

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
