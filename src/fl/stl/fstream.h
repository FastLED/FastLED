#pragma once

#include "fl/int.h"
#include "fl/stl/detail/file_handle.h"

// Platform-aware file stream implementation
// - Host/Testing platforms (FASTLED_TESTING): Full file I/O implementation via posix_file_handle
// - Embedded platforms (ESP32, Uno, etc.): No-op stubs (no file system support)
//
// This implementation uses an abstraction layer (detail::file_handle_base) to hide
// platform-specific file I/O details (FILE*, POSIX calls, etc.) and provide a
// clean, extensible interface for future platform implementations.

#ifdef FASTLED_TESTING
#include "fl/stl/cstring.h"  // For fl::strerror
#endif

namespace fl {

// File stream mode flags and seek directions
namespace ios {
    // Mode flags (bitmask)
    using openmode = unsigned int;
    static constexpr openmode binary = 0x01;  // Binary mode
    static constexpr openmode ate    = 0x02;  // Seek to end after opening
    static constexpr openmode in     = 0x04;  // Open for reading
    static constexpr openmode out    = 0x08;  // Open for writing
    static constexpr openmode trunc  = 0x10;  // Truncate file if exists
    static constexpr openmode app    = 0x20;  // Append mode

    // Seek direction
    enum seekdir {
        beg = 0,  // Beginning of file
        cur = 1,  // Current position
        end = 2   // End of file
    };
}

// ============================================================================
// IMPLEMENTATION (Uses detail::posix_file_handle abstraction)
// ============================================================================

// Input file stream
class ifstream {
private:
    detail::posix_file_handle mHandle;
    fl::size_t mLastRead;
    bool mGood;
    bool mEof;
    bool mFail;

    void updateState() {
        if (mHandle.is_open()) {
            mEof = mHandle.is_eof();
            mFail = mHandle.has_error();
            mGood = !mEof && !mFail;
        } else {
            mGood = false;
            mEof = false;
            mFail = true;
        }
    }

public:
    ifstream() : mLastRead(0), mGood(false), mEof(false), mFail(true) {}

    explicit ifstream(const char* path, ios::openmode mode = ios::in);

    ~ifstream();

    // Non-copyable
    ifstream(const ifstream&) = delete;
    ifstream& operator=(const ifstream&) = delete;

    void open(const char* path, ios::openmode mode = ios::in);

    bool is_open() const {
        return mHandle.is_open();
    }

    void close();

    ifstream& read(char* buffer, fl::size_t count);

    fl::size_t gcount() const {
        return mLastRead;
    }

    fl::size_t tellg();

    ifstream& seekg(fl::size_t pos, ios::seekdir dir = ios::beg);

    bool good() const { return mGood; }
    bool eof() const { return mEof; }
    bool fail() const { return mFail; }

    // Get last error code (0 if no error)
    int error() const;

    // Get human-readable error message
    const char* error_message() const;

    // Manually clear error state (for retry scenarios)
    void clear_error();
};

// Output file stream
class ofstream {
private:
    detail::posix_file_handle mHandle;
    bool mGood;
    bool mEof;
    bool mFail;
    int mLocalError;  // Track errors that occur at stream level (not just handle level)

    void updateState() {
        if (mHandle.is_open()) {
            mEof = mHandle.is_eof();
            mFail = mHandle.has_error() || (mLocalError != 0);
            mGood = !mEof && !mFail;
        } else {
            mGood = false;
            mEof = false;
            mFail = true;
        }
    }

public:
    ofstream() : mGood(false), mEof(false), mFail(true), mLocalError(0) {}

    explicit ofstream(const char* path, ios::openmode mode = ios::out);

    ~ofstream();

    // Non-copyable
    ofstream(const ofstream&) = delete;
    ofstream& operator=(const ofstream&) = delete;

    void open(const char* path, ios::openmode mode = ios::out);

    bool is_open() const {
        return mHandle.is_open();
    }

    void close();

    ofstream& write(const char* data, fl::size_t count);

    bool good() const { return mGood; }
    bool eof() const { return mEof; }
    bool fail() const { return mFail; }

    // Get last error code (0 if no error)
    int error() const;

    // Get human-readable error message
    const char* error_message() const;

    // Manually clear error state (for retry scenarios)
    void clear_error();
};

// Generic file stream (supports both reading and writing)
class fstream {
private:
    detail::posix_file_handle mHandle;
    fl::size_t mLastRead;
    bool mGood;
    bool mEof;
    bool mFail;
    int mLocalError;  // Track errors that occur at stream level (not just handle level)

    void updateState() {
        if (mHandle.is_open()) {
            mEof = mHandle.is_eof();
            mFail = mHandle.has_error() || (mLocalError != 0);
            mGood = !mEof && !mFail;
        } else {
            mGood = false;
            mEof = false;
            mFail = true;
        }
    }

public:
    fstream() : mLastRead(0), mGood(false), mEof(false), mFail(true), mLocalError(0) {}

    explicit fstream(const char* path, ios::openmode mode = ios::in | ios::out);

    ~fstream();

    // Non-copyable
    fstream(const fstream&) = delete;
    fstream& operator=(const fstream&) = delete;

    void open(const char* path, ios::openmode mode = ios::in | ios::out);

    bool is_open() const {
        return mHandle.is_open();
    }

    void close();

    fstream& read(char* buffer, fl::size_t count);

    fl::size_t gcount() const {
        return mLastRead;
    }

    fstream& write(const char* data, fl::size_t count);

    fl::size_t tellg();

    fstream& seekg(fl::size_t pos, ios::seekdir dir = ios::beg);

    bool good() const { return mGood; }
    bool eof() const { return mEof; }
    bool fail() const { return mFail; }

    // Get last error code (0 if no error)
    int error() const;

    // Get human-readable error message
    const char* error_message() const;

    // Manually clear error state (for retry scenarios)
    void clear_error();
};

} // namespace fl
