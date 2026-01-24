#pragma once

#include "fl/int.h"

// Platform-aware file stream implementation
// - Host/Testing platforms (FASTLED_TESTING): Full file I/O implementation
// - Embedded platforms (ESP32, Uno, etc.): No-op stubs (no file system support)

// Platform-specific includes for file I/O (only on host platforms)
#ifdef FASTLED_TESTING
#include <cstdio>  // For FILE*, fopen, fclose, fread, fwrite, fseek, ftell
#include <cerrno>  // For errno
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

#ifdef FASTLED_TESTING

// ============================================================================
// HOST/TESTING PLATFORM IMPLEMENTATION (Full file I/O support)
// ============================================================================

// Input file stream
class ifstream {
private:
    FILE* mFile;
    fl::size_t mLastRead;
    bool mGood;
    bool mEof;
    bool mFail;
    int mLastError;  // Last captured errno value (0 = no error)

    void captureError() {
        mLastError = errno;
        mFail = true;
        mGood = false;
    }

    void clearError() {
        mLastError = 0;
    }

    void updateState() {
        if (mFile) {
            mEof = feof(mFile) != 0;
            bool hasError = ferror(mFile) != 0;

            // If ferror detected error but we haven't captured errno yet
            if (hasError && mLastError == 0) {
                mLastError = errno;  // Capture current errno
            }

            mFail = hasError || (mLastError != 0);
            mGood = !mEof && !mFail;
        } else {
            mGood = false;
            mEof = false;
            mFail = true;
            // Note: mLastError preserved from operation that failed
        }
    }

public:
    ifstream() : mFile(nullptr), mLastRead(0), mGood(false), mEof(false), mFail(true), mLastError(0) {}

    explicit ifstream(const char* path, ios::openmode mode = ios::in)
        : mFile(nullptr), mLastRead(0), mGood(false), mEof(false), mFail(true), mLastError(0) {
        open(path, mode);
    }

    ~ifstream() {
        close();
    }

    // Non-copyable
    ifstream(const ifstream&) = delete;
    ifstream& operator=(const ifstream&) = delete;

    void open(const char* path, ios::openmode mode = ios::in) {
        close();

        // Build fopen mode string
        const char* fmode = (mode & ios::binary) ? "rb" : "r";

        errno = 0;  // Clear errno before operation
        mFile = fopen(path, fmode);

        if (mFile) {
            clearError();  // Reset error state on success

            if (mode & ios::ate) {
                if (fseek(mFile, 0, 2 /* SEEK_END */) != 0) {
                    captureError();  // Capture fseek failure
                }
            }
            updateState();
        } else {
            captureError();  // Capture fopen failure (ENOENT, EACCES, etc.)
        }
    }

    bool is_open() const {
        return mFile != nullptr;
    }

    void close() {
        if (mFile) {
            errno = 0;
            int result = fclose(mFile);
            mFile = nullptr;
            if (result == 0) {
                clearError();  // Successful close
                // After successful close: keep good() = true to match std::ofstream behavior
                // This allows fs_stub.hpp's createTextFile to work correctly
                mGood = true;
                mEof = false;
                mFail = false;
            } else {
                captureError();  // Capture flush failure (ENOSPC, EIO)
                // After failed close: update full state
                updateState();
            }
        }
    }

    ifstream& read(char* buffer, fl::size_t count) {
        mLastRead = 0;
        if (mFile) {
            errno = 0;
            mLastRead = fread(buffer, 1, count, mFile);

            // fread can return short at EOF - not an error
            if (feof(mFile) || mLastRead > 0) {
                clearError();  // Successful read or EOF (not error)
            }
            updateState();
        }
        return *this;
    }

    fl::size_t gcount() const {
        return mLastRead;
    }

    fl::size_t tellg() {
        if (!mFile) {
            mLastError = EBADF;  // Bad file descriptor
            return 0;
        }
        errno = 0;
        long pos = ftell(mFile);
        if (pos < 0) {
            captureError();  // Capture ftell failure
            return 0;
        }
        clearError();  // Successful position query
        return static_cast<fl::size_t>(pos);
    }

    ifstream& seekg(fl::size_t pos, ios::seekdir dir = ios::beg) {
        if (mFile) {
            errno = 0;
            if (fseek(mFile, static_cast<long>(pos), dir) != 0) {
                captureError();  // Capture seek failure
            } else {
                clearError();  // Reset error on success
            }
            updateState();
        }
        return *this;
    }

    bool good() const { return mGood; }
    bool eof() const { return mEof; }
    bool fail() const { return mFail; }

    // Get last error code (0 if no error)
    int error() const {
        return mLastError;
    }

    // Get human-readable error message
    const char* error_message() const {
        if (mLastError == 0) {
            return "No error";
        }
        return fl::strerror(mLastError);
    }

    // Manually clear error state (for retry scenarios)
    void clear_error() {
        mLastError = 0;
        mFail = false;
        if (mFile) {
            clearerr(mFile);  // Clear C stdio error indicators
        }
        updateState();
    }
};

// Output file stream (host/testing platform implementation)
class ofstream {
private:
    FILE* mFile;
    bool mGood;
    bool mEof;
    bool mFail;
    int mLastError;  // Last captured errno value (0 = no error)

    void captureError() {
        mLastError = errno;
        mFail = true;
        mGood = false;
    }

    void clearError() {
        mLastError = 0;
    }

    void updateState() {
        if (mFile) {
            mEof = feof(mFile) != 0;
            bool hasError = ferror(mFile) != 0;

            // If ferror detected error but we haven't captured errno yet
            if (hasError && mLastError == 0) {
                mLastError = errno;  // Capture current errno
            }

            mFail = hasError || (mLastError != 0);
            mGood = !mEof && !mFail;
        } else {
            mGood = false;
            mEof = false;
            mFail = true;
            // Note: mLastError preserved from operation that failed
        }
    }

public:
    ofstream() : mFile(nullptr), mGood(false), mEof(false), mFail(true), mLastError(0) {}

    explicit ofstream(const char* path, ios::openmode mode = ios::out)
        : mFile(nullptr), mGood(false), mEof(false), mFail(true), mLastError(0) {
        open(path, mode);
    }

    ~ofstream() {
        close();
    }

    // Non-copyable
    ofstream(const ofstream&) = delete;
    ofstream& operator=(const ofstream&) = delete;

    void open(const char* path, ios::openmode mode = ios::out) {
        close();

        // Build fopen mode string
        const char* fmode;
        if (mode & ios::app) {
            fmode = (mode & ios::binary) ? "ab" : "a";
        } else if (mode & ios::trunc) {
            fmode = (mode & ios::binary) ? "wb" : "w";
        } else {
            // Default for 'out' is truncate
            fmode = (mode & ios::binary) ? "wb" : "w";
        }

        errno = 0;  // Clear errno before operation
        mFile = fopen(path, fmode);

        if (mFile) {
            clearError();  // Reset error state on success

            if (mode & ios::ate) {
                if (fseek(mFile, 0, SEEK_END) != 0) {
                    captureError();  // Capture fseek failure
                }
            }
            updateState();
        } else {
            captureError();  // Capture fopen failure (ENOENT, EACCES, etc.)
        }
    }

    bool is_open() const {
        return mFile != nullptr;
    }

    void close() {
        if (mFile) {
            errno = 0;
            int result = fclose(mFile);
            mFile = nullptr;
            if (result == 0) {
                clearError();  // Successful close
                // After successful close: keep good() = true to match std::ofstream behavior
                // This allows fs_stub.hpp's createTextFile to work correctly
                mGood = true;
                mEof = false;
                mFail = false;
            } else {
                captureError();  // Capture flush failure (ENOSPC, EIO)
                // After failed close: update full state
                updateState();
            }
        }
    }

    ofstream& write(const char* data, fl::size_t count) {
        if (mFile) {
            errno = 0;
            fl::size_t written = fwrite(data, 1, count, mFile);
            if (written != count) {
                captureError();  // Capture short write (ENOSPC, EIO)
            } else {
                // Flush to ensure data is written before any subsequent reads
                fflush(mFile);
                clearError();  // Reset error on full write
            }
            updateState();
        } else {
            // Attempting to write to closed stream
            mLastError = EBADF;  // Bad file descriptor
            mFail = true;
            mGood = false;
        }
        return *this;
    }

    bool good() const { return mGood; }
    bool eof() const { return mEof; }
    bool fail() const { return mFail; }

    // Get last error code (0 if no error)
    int error() const {
        return mLastError;
    }

    // Get human-readable error message
    const char* error_message() const {
        if (mLastError == 0) {
            return "No error";
        }
        return fl::strerror(mLastError);
    }

    // Manually clear error state (for retry scenarios)
    void clear_error() {
        mLastError = 0;
        mFail = false;
        if (mFile) {
            clearerr(mFile);  // Clear C stdio error indicators
        }
        updateState();
    }
};

// Generic file stream (supports both reading and writing, host/testing platform)
class fstream {
private:
    FILE* mFile;
    fl::size_t mLastRead;
    bool mGood;
    bool mEof;
    bool mFail;
    int mLastError;  // Last captured errno value (0 = no error)

    void captureError() {
        mLastError = errno;
        mFail = true;
        mGood = false;
    }

    void clearError() {
        mLastError = 0;
    }

    void updateState() {
        if (mFile) {
            mEof = feof(mFile) != 0;
            bool hasError = ferror(mFile) != 0;

            // If ferror detected error but we haven't captured errno yet
            if (hasError && mLastError == 0) {
                mLastError = errno;  // Capture current errno
            }

            mFail = hasError || (mLastError != 0);
            mGood = !mEof && !mFail;
        } else {
            mGood = false;
            mEof = false;
            mFail = true;
            // Note: mLastError preserved from operation that failed
        }
    }

public:
    fstream() : mFile(nullptr), mLastRead(0), mGood(false), mEof(false), mFail(true), mLastError(0) {}

    explicit fstream(const char* path, ios::openmode mode = ios::in | ios::out)
        : mFile(nullptr), mLastRead(0), mGood(false), mEof(false), mFail(true), mLastError(0) {
        open(path, mode);
    }

    ~fstream() {
        close();
    }

    // Non-copyable
    fstream(const fstream&) = delete;
    fstream& operator=(const fstream&) = delete;

    void open(const char* path, ios::openmode mode = ios::in | ios::out) {
        close();

        // Build fopen mode string for read+write
        const char* fmode;
        if (mode & ios::app) {
            fmode = (mode & ios::binary) ? "a+b" : "a+";
        } else if (mode & ios::trunc) {
            fmode = (mode & ios::binary) ? "w+b" : "w+";
        } else {
            fmode = (mode & ios::binary) ? "r+b" : "r+";
        }

        errno = 0;  // Clear errno before operation
        mFile = fopen(path, fmode);

        if (mFile) {
            clearError();  // Reset error state on success

            if (mode & ios::ate) {
                if (fseek(mFile, 0, SEEK_END) != 0) {
                    captureError();  // Capture fseek failure
                }
            }
            updateState();
        } else {
            captureError();  // Capture fopen failure (ENOENT, EACCES, etc.)
        }
    }

    bool is_open() const {
        return mFile != nullptr;
    }

    void close() {
        if (mFile) {
            errno = 0;
            int result = fclose(mFile);
            mFile = nullptr;
            if (result == 0) {
                clearError();  // Successful close
                // After successful close: keep good() = true to match std::ofstream behavior
                // This allows fs_stub.hpp's createTextFile to work correctly
                mGood = true;
                mEof = false;
                mFail = false;
            } else {
                captureError();  // Capture flush failure (ENOSPC, EIO)
                // After failed close: update full state
                updateState();
            }
        }
    }

    fstream& read(char* buffer, fl::size_t count) {
        mLastRead = 0;
        if (mFile) {
            errno = 0;
            mLastRead = fread(buffer, 1, count, mFile);

            // fread can return short at EOF - not an error
            if (feof(mFile) || mLastRead > 0) {
                clearError();  // Successful read or EOF (not error)
            }
            updateState();
        }
        return *this;
    }

    fl::size_t gcount() const {
        return mLastRead;
    }

    fstream& write(const char* data, fl::size_t count) {
        if (mFile) {
            errno = 0;
            fl::size_t written = fwrite(data, 1, count, mFile);
            if (written != count) {
                captureError();  // Capture short write (ENOSPC, EIO)
            } else {
                // Flush to ensure data is written before any subsequent reads
                fflush(mFile);
                clearError();  // Reset error on full write
            }
            updateState();
        } else {
            // Attempting to write to closed stream
            mLastError = EBADF;  // Bad file descriptor
            mFail = true;
            mGood = false;
        }
        return *this;
    }

    fl::size_t tellg() {
        if (!mFile) {
            mLastError = EBADF;  // Bad file descriptor
            return 0;
        }
        errno = 0;
        long pos = ftell(mFile);
        if (pos < 0) {
            captureError();  // Capture ftell failure
            return 0;
        }
        clearError();  // Successful position query
        return static_cast<fl::size_t>(pos);
    }

    fstream& seekg(fl::size_t pos, ios::seekdir dir = ios::beg) {
        if (mFile) {
            errno = 0;
            if (fseek(mFile, static_cast<long>(pos), dir) != 0) {
                captureError();  // Capture seek failure
            } else {
                clearError();  // Reset error on success
            }
            updateState();
        }
        return *this;
    }

    bool good() const { return mGood; }
    bool eof() const { return mEof; }
    bool fail() const { return mFail; }

    // Get last error code (0 if no error)
    int error() const {
        return mLastError;
    }

    // Get human-readable error message
    const char* error_message() const {
        if (mLastError == 0) {
            return "No error";
        }
        return fl::strerror(mLastError);
    }

    // Manually clear error state (for retry scenarios)
    void clear_error() {
        mLastError = 0;
        mFail = false;
        if (mFile) {
            clearerr(mFile);  // Clear C stdio error indicators
        }
        updateState();
    }
};

#else // !FASTLED_TESTING

// ============================================================================
// EMBEDDED PLATFORM IMPLEMENTATION (No file system support)
// ============================================================================

// No-op file stream stubs for embedded platforms without file systems
// These classes provide the same API but all operations fail gracefully

// Input file stream (no-op stub)
class ifstream {
private:
    fl::size_t mLastRead;
    bool mGood;
    bool mEof;
    bool mFail;
    int mLastError;

public:
    ifstream() : mLastRead(0), mGood(false), mEof(false), mFail(true), mLastError(0) {}

    explicit ifstream(const char* /*path*/, ios::openmode /*mode*/ = ios::in)
        : mLastRead(0), mGood(false), mEof(false), mFail(true), mLastError(0) {}

    ~ifstream() {}

    // Non-copyable
    ifstream(const ifstream&) = delete;
    ifstream& operator=(const ifstream&) = delete;

    void open(const char* /*path*/, ios::openmode /*mode*/ = ios::in) {
        mFail = true;
        mGood = false;
    }

    bool is_open() const { return false; }
    void close() {}

    ifstream& read(char* /*buffer*/, fl::size_t /*count*/) {
        mLastRead = 0;
        return *this;
    }

    fl::size_t gcount() const { return mLastRead; }
    fl::size_t tellg() { return 0; }

    ifstream& seekg(fl::size_t /*pos*/, ios::seekdir /*dir*/ = ios::beg) {
        return *this;
    }

    bool good() const { return mGood; }
    bool eof() const { return mEof; }
    bool fail() const { return mFail; }

    int error() const { return mLastError; }
    const char* error_message() const { return "File I/O not supported on this platform"; }
    void clear_error() {}
};

// Output file stream (no-op stub)
class ofstream {
private:
    bool mGood;
    bool mEof;
    bool mFail;
    int mLastError;

public:
    ofstream() : mGood(false), mEof(false), mFail(true), mLastError(0) {}

    explicit ofstream(const char* /*path*/, ios::openmode /*mode*/ = ios::out)
        : mGood(false), mEof(false), mFail(true), mLastError(0) {}

    ~ofstream() {}

    // Non-copyable
    ofstream(const ofstream&) = delete;
    ofstream& operator=(const ofstream&) = delete;

    void open(const char* /*path*/, ios::openmode /*mode*/ = ios::out) {
        mFail = true;
        mGood = false;
    }

    bool is_open() const { return false; }
    void close() {}

    ofstream& write(const char* /*data*/, fl::size_t /*count*/) {
        return *this;
    }

    bool good() const { return mGood; }
    bool eof() const { return mEof; }
    bool fail() const { return mFail; }

    int error() const { return mLastError; }
    const char* error_message() const { return "File I/O not supported on this platform"; }
    void clear_error() {}
};

// Generic file stream (no-op stub)
class fstream {
private:
    fl::size_t mLastRead;
    bool mGood;
    bool mEof;
    bool mFail;
    int mLastError;

public:
    fstream() : mLastRead(0), mGood(false), mEof(false), mFail(true), mLastError(0) {}

    explicit fstream(const char* /*path*/, ios::openmode /*mode*/ = ios::in | ios::out)
        : mLastRead(0), mGood(false), mEof(false), mFail(true), mLastError(0) {}

    ~fstream() {}

    // Non-copyable
    fstream(const fstream&) = delete;
    fstream& operator=(const fstream&) = delete;

    void open(const char* /*path*/, ios::openmode /*mode*/ = ios::in | ios::out) {
        mFail = true;
        mGood = false;
    }

    bool is_open() const { return false; }
    void close() {}

    fstream& read(char* /*buffer*/, fl::size_t /*count*/) {
        mLastRead = 0;
        return *this;
    }

    fl::size_t gcount() const { return mLastRead; }

    fstream& write(const char* /*data*/, fl::size_t /*count*/) {
        return *this;
    }

    fl::size_t tellg() { return 0; }

    fstream& seekg(fl::size_t /*pos*/, ios::seekdir /*dir*/ = ios::beg) {
        return *this;
    }

    bool good() const { return mGood; }
    bool eof() const { return mEof; }
    bool fail() const { return mFail; }

    int error() const { return mLastError; }
    const char* error_message() const { return "File I/O not supported on this platform"; }
    void clear_error() {}
};

#endif // FASTLED_TESTING

} // namespace fl
