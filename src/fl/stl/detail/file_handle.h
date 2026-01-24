#pragma once

#include "fl/int.h"
#include "fl/stl/detail/file_io.h"  // For fl::FILE* and fl::fopen/fclose/etc.

// Low-level file handle abstraction for fl::fstream implementations
// This provides a platform-agnostic interface for basic file I/O operations,
// using fl::FILE* abstraction that platforms can implement

namespace fl {
namespace detail {

// Seek direction constants (match std::ios seekdir)
enum class seek_dir {
    beg = 0,  // Beginning of file
    cur = 1,  // Current position
    end = 2   // End of file
};

// Abstract interface for low-level file operations
// Platform-specific implementations will subclass this
class file_handle_base {
public:
    virtual ~file_handle_base() = default;

    // File operations
    virtual bool is_open() const = 0;
    virtual void close() = 0;
    virtual fl::size_t read(char* buffer, fl::size_t count) = 0;
    virtual fl::size_t write(const char* data, fl::size_t count) = 0;
    virtual fl::size_t tell() = 0;
    virtual bool seek(fl::size_t pos, seek_dir dir) = 0;

    // State queries
    virtual bool is_eof() const = 0;
    virtual bool has_error() const = 0;
    virtual void clear_error() = 0;

    // Error reporting
    virtual int error_code() const = 0;
    virtual const char* error_message() const = 0;
};

// ============================================================================
// File Handle Implementation (uses fl::FILE* abstraction)
// ============================================================================
// This implementation works on all platforms that provide fl::FILE*
// (POSIX/testing platforms via standard FILE*, embedded platforms can provide custom implementations)

class posix_file_handle : public file_handle_base {
private:
    fl::FILE* mFile;
    int mLastError;

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

    fl::size_t write(const char* data, fl::size_t count) override;

    fl::size_t tell() override;

    bool seek(fl::size_t pos, seek_dir dir) override;

    bool is_eof() const override;

    bool has_error() const override;

    void clear_error() override;

    int error_code() const override;

    const char* error_message() const override;
};

} // namespace detail
} // namespace fl
