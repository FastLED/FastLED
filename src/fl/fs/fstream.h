#pragma once

#include "fl/stl/int.h"
#include "fl/stl/bit_cast.h"
#include "fl/fs/file_handle.h"
#include "fl/stl/shared_ptr.h"

// Platform-aware file stream implementation (analogous to std::ifstream / std::ofstream)
// - Host/Testing platforms (FASTLED_TESTING): Full file I/O implementation via posix_filebuf
// - Embedded platforms (ESP32, Uno, etc.): No-op stubs (no file system support)
//
// These stream classes accept any filebuf subclass via shared_ptr,
// enabling polymorphic dispatch to different backends (POSIX, SD card, WASM, memory).

#ifdef FASTLED_TESTING
#include "fl/stl/cstring.h"  // For fl::strerror
#include "fl/stl/noexcept.h"
#endif

namespace fl {

// Shared pointer to the polymorphic backend
using filebuf_ptr = fl::shared_ptr<filebuf>;

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
    enum class seekdir {
        beg = 0,  // Beginning of file
        cur = 1,  // Current position
        end = 2   // End of file
    };
}

// ============================================================================
// Input file stream (analogous to std::ifstream)
// ============================================================================

class ifstream {
private:
    filebuf_ptr mHandle;
    fl::size_t mLastRead;
    bool mGood;
    bool mEof;
    bool mFail;

    void updateState() FL_NO_EXCEPT {
        if (mHandle && mHandle->is_open()) {
            mEof = mHandle->is_eof();
            mFail = mHandle->has_error();
            mGood = !mEof && !mFail;
        } else {
            mGood = false;
            mEof = false;
            mFail = true;
        }
    }

public:
    ifstream() FL_NO_EXCEPT : mLastRead(0), mGood(false), mEof(false), mFail(true) {}

    explicit ifstream(const char* path, ios::openmode mode = ios::in) FL_NO_EXCEPT;

    explicit ifstream(filebuf_ptr handle) FL_NO_EXCEPT;

    ~ifstream() FL_NO_EXCEPT;

    // Non-copyable
    ifstream(const ifstream&) FL_NO_EXCEPT = delete;
    ifstream& operator=(const ifstream&) = delete;

    // Moveable
    ifstream(ifstream&& other) FL_NO_EXCEPT
        : mHandle(other.mHandle), mLastRead(other.mLastRead),
          mGood(other.mGood), mEof(other.mEof), mFail(other.mFail) {
        other.mHandle.reset();
        other.mLastRead = 0;
        other.mGood = false;
        other.mEof = false;
        other.mFail = true;
    }

    ifstream& operator=(ifstream&& other) FL_NO_EXCEPT {
        if (this != &other) {
            close();
            mHandle = other.mHandle;
            mLastRead = other.mLastRead;
            mGood = other.mGood;
            mEof = other.mEof;
            mFail = other.mFail;
            other.mHandle.reset();
            other.mLastRead = 0;
            other.mGood = false;
            other.mEof = false;
            other.mFail = true;
        }
        return *this;
    }

    void open(const char* path, ios::openmode mode = ios::in) FL_NO_EXCEPT;

    bool is_open() const FL_NO_EXCEPT {
        return mHandle && mHandle->is_open();
    }

    void close() FL_NO_EXCEPT;

    ifstream& read(char* buffer, fl::size_t count) FL_NO_EXCEPT;

    fl::size_t gcount() const FL_NO_EXCEPT {
        return mLastRead;
    }

    fl::size_t tellg() FL_NO_EXCEPT;

    ifstream& seekg(fl::size_t pos, ios::seekdir dir = ios::seekdir::beg) FL_NO_EXCEPT;

    bool good() const FL_NO_EXCEPT { return mGood; }
    bool eof() const FL_NO_EXCEPT { return mEof; }
    bool fail() const FL_NO_EXCEPT { return mFail; }

    // Get last error code (0 if no error)
    int error() const FL_NO_EXCEPT;

    // Get human-readable error message
    const char* error_message() const FL_NO_EXCEPT;

    // Manually clear error state (for retry scenarios)
    void clear_error() FL_NO_EXCEPT;

    // --- Enriched API (forwarded from filebuf) ---

    // Access the underlying buffer (like std::ifstream::rdbuf())
    filebuf_ptr rdbuf() const FL_NO_EXCEPT { return mHandle; }

    // Total file size in bytes
    fl::size_t size() const FL_NO_EXCEPT {
        return mHandle ? mHandle->size() : 0;
    }

    // Bytes remaining from current position to end
    fl::size_t bytes_left() const FL_NO_EXCEPT {
        return mHandle ? mHandle->bytes_left() : 0;
    }

    // File path (or description for non-file buffers)
    const char* path() const FL_NO_EXCEPT {
        return mHandle ? mHandle->path() : "";
    }

    // Check if data is available for reading
    bool available() const FL_NO_EXCEPT {
        return mHandle ? mHandle->available() : false;
    }

    // Check if at least n bytes are available
    bool available(fl::size_t n) const FL_NO_EXCEPT {
        return mHandle ? mHandle->available(n) : false;
    }

    // Convenience: read into u8 buffer
    fl::size_t read(fl::u8* dst, fl::size_t n) FL_NO_EXCEPT {
        read(fl::reinterpret_cast_<char*>(dst), n);
        return mLastRead;
    }

    // Convenience: read into u8 span
    fl::size_t read(fl::span<fl::u8> dst) FL_NO_EXCEPT {
        return read(dst.data(), dst.size());
    }

    // Convenience: read RGB8 pixels (3 bytes per pixel)
    fl::size_t readRGB8(fl::span<CRGB> dst) FL_NO_EXCEPT {
        read(fl::reinterpret_cast_<char*>(dst.data()), dst.size() * 3);
        return mLastRead / 3;
    }

    // Backward-compat aliases
    bool valid() const FL_NO_EXCEPT { return is_open(); }
    fl::size_t pos() const FL_NO_EXCEPT { return mHandle ? mHandle->pos() : 0; }
    fl::size_t bytesLeft() const FL_NO_EXCEPT { return bytes_left(); }
    bool seek(fl::size_t p) FL_NO_EXCEPT {
        seekg(p, ios::seekdir::beg);
        return good();
    }
    bool seek(fl::size_t p, seek_dir dir) FL_NO_EXCEPT {
        ios::seekdir d = (dir == seek_dir::beg) ? ios::seekdir::beg :
                         (dir == seek_dir::cur) ? ios::seekdir::cur : ios::seekdir::end;
        seekg(p, d);
        return good();
    }
};

// ============================================================================
// Output file stream (analogous to std::ofstream)
// ============================================================================

class ofstream {
private:
    filebuf_ptr mHandle;
    bool mGood;
    bool mEof;
    bool mFail;
    int mLocalError;

    void updateState() FL_NO_EXCEPT {
        if (mHandle && mHandle->is_open()) {
            mEof = mHandle->is_eof();
            mFail = mHandle->has_error() || (mLocalError != 0);
            mGood = !mEof && !mFail;
        } else {
            mGood = false;
            mEof = false;
            mFail = true;
        }
    }

public:
    ofstream() FL_NO_EXCEPT : mGood(false), mEof(false), mFail(true), mLocalError(0) {}

    explicit ofstream(const char* path, ios::openmode mode = ios::out) FL_NO_EXCEPT;

    explicit ofstream(filebuf_ptr handle) FL_NO_EXCEPT;

    ~ofstream() FL_NO_EXCEPT;

    // Non-copyable
    ofstream(const ofstream&) FL_NO_EXCEPT = delete;
    ofstream& operator=(const ofstream&) = delete;

    // Moveable
    ofstream(ofstream&& other) FL_NO_EXCEPT
        : mHandle(other.mHandle), mGood(other.mGood), mEof(other.mEof),
          mFail(other.mFail), mLocalError(other.mLocalError) {
        other.mHandle.reset();
        other.mGood = false;
        other.mEof = false;
        other.mFail = true;
        other.mLocalError = 0;
    }

    ofstream& operator=(ofstream&& other) FL_NO_EXCEPT {
        if (this != &other) {
            close();
            mHandle = other.mHandle;
            mGood = other.mGood;
            mEof = other.mEof;
            mFail = other.mFail;
            mLocalError = other.mLocalError;
            other.mHandle.reset();
            other.mGood = false;
            other.mEof = false;
            other.mFail = true;
            other.mLocalError = 0;
        }
        return *this;
    }

    void open(const char* path, ios::openmode mode = ios::out) FL_NO_EXCEPT;

    bool is_open() const FL_NO_EXCEPT {
        return mHandle && mHandle->is_open();
    }

    void close() FL_NO_EXCEPT;

    ofstream& write(const char* data, fl::size_t count) FL_NO_EXCEPT;

    bool good() const FL_NO_EXCEPT { return mGood; }
    bool eof() const FL_NO_EXCEPT { return mEof; }
    bool fail() const FL_NO_EXCEPT { return mFail; }

    // Get last error code (0 if no error)
    int error() const FL_NO_EXCEPT;

    // Get human-readable error message
    const char* error_message() const FL_NO_EXCEPT;

    // Manually clear error state (for retry scenarios)
    void clear_error() FL_NO_EXCEPT;

    // Access the underlying buffer
    filebuf_ptr rdbuf() const FL_NO_EXCEPT { return mHandle; }
};

// ============================================================================
// Generic file stream (supports both reading and writing)
// ============================================================================

class fstream {
private:
    filebuf_ptr mHandle;
    fl::size_t mLastRead;
    bool mGood;
    bool mEof;
    bool mFail;
    int mLocalError;

    void updateState() FL_NO_EXCEPT {
        if (mHandle && mHandle->is_open()) {
            mEof = mHandle->is_eof();
            mFail = mHandle->has_error() || (mLocalError != 0);
            mGood = !mEof && !mFail;
        } else {
            mGood = false;
            mEof = false;
            mFail = true;
        }
    }

public:
    fstream() FL_NO_EXCEPT : mLastRead(0), mGood(false), mEof(false), mFail(true), mLocalError(0) {}

    explicit fstream(const char* path, ios::openmode mode = ios::in | ios::out) FL_NO_EXCEPT;

    explicit fstream(filebuf_ptr handle) FL_NO_EXCEPT;

    ~fstream() FL_NO_EXCEPT;

    // Non-copyable
    fstream(const fstream&) FL_NO_EXCEPT = delete;
    fstream& operator=(const fstream&) = delete;

    // Moveable
    fstream(fstream&& other) FL_NO_EXCEPT
        : mHandle(other.mHandle), mLastRead(other.mLastRead),
          mGood(other.mGood), mEof(other.mEof), mFail(other.mFail),
          mLocalError(other.mLocalError) {
        other.mHandle.reset();
        other.mLastRead = 0;
        other.mGood = false;
        other.mEof = false;
        other.mFail = true;
        other.mLocalError = 0;
    }

    fstream& operator=(fstream&& other) FL_NO_EXCEPT {
        if (this != &other) {
            close();
            mHandle = other.mHandle;
            mLastRead = other.mLastRead;
            mGood = other.mGood;
            mEof = other.mEof;
            mFail = other.mFail;
            mLocalError = other.mLocalError;
            other.mHandle.reset();
            other.mLastRead = 0;
            other.mGood = false;
            other.mEof = false;
            other.mFail = true;
            other.mLocalError = 0;
        }
        return *this;
    }

    void open(const char* path, ios::openmode mode = ios::in | ios::out) FL_NO_EXCEPT;

    bool is_open() const FL_NO_EXCEPT {
        return mHandle && mHandle->is_open();
    }

    void close() FL_NO_EXCEPT;

    fstream& read(char* buffer, fl::size_t count) FL_NO_EXCEPT;

    fl::size_t gcount() const FL_NO_EXCEPT {
        return mLastRead;
    }

    fstream& write(const char* data, fl::size_t count) FL_NO_EXCEPT;

    fl::size_t tellg() FL_NO_EXCEPT;

    fstream& seekg(fl::size_t pos, ios::seekdir dir = ios::seekdir::beg) FL_NO_EXCEPT;

    bool good() const FL_NO_EXCEPT { return mGood; }
    bool eof() const FL_NO_EXCEPT { return mEof; }
    bool fail() const FL_NO_EXCEPT { return mFail; }

    // Get last error code (0 if no error)
    int error() const FL_NO_EXCEPT;

    // Get human-readable error message
    const char* error_message() const FL_NO_EXCEPT;

    // Manually clear error state (for retry scenarios)
    void clear_error() FL_NO_EXCEPT;

    // Access the underlying buffer
    filebuf_ptr rdbuf() const FL_NO_EXCEPT { return mHandle; }
};

} // namespace fl
