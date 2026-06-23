// Implementation file for fl::fstream, fl::ifstream, fl::ofstream
// This file contains out-of-line definitions to reduce header compilation overhead

#include "fl/stl/fstream.h"
#include "fl/stl/memory.h"
#include "fl/stl/noexcept.h"

namespace fl {

// ============================================================================
// ifstream Implementation
// ============================================================================

ifstream::ifstream(const char* path, ios::openmode mode)
    : mLastRead(0), mGood(false), mEof(false), mFail(true) {
    open(path, mode);
}

ifstream::ifstream(filebuf_ptr handle)
    : mHandle(handle), mLastRead(0), mGood(false), mEof(false), mFail(true) {
    updateState();
}

ifstream::~ifstream() FL_NOEXCEPT {
    close();
}

void ifstream::open(const char* path, ios::openmode mode) {
    close();

    // Build fopen mode string
    const char* fmode = (mode & ios::binary) ? "rb" : "r";

    mHandle = fl::make_shared<detail::posix_filebuf>(path, fmode);

    if (mHandle->is_open()) {
        if (mode & ios::ate) {
            mHandle->seek(0, seek_dir::end);
        }
        updateState();
    } else {
        updateState();
    }
}

void ifstream::close() {
    if (mHandle && mHandle->is_open()) {
        mHandle->close();
        // After successful close: keep good() = true to match std::ofstream behavior
        // This allows fs_stub.hpp's createTextFile to work correctly
        if (!mHandle->has_error()) {
            mGood = true;
            mEof = false;
            mFail = false;
        } else {
            updateState();
        }
    }
}

ifstream& ifstream::read(char* buffer, fl::size_t count) {
    mLastRead = 0;
    if (mHandle && mHandle->is_open()) {
        mLastRead = mHandle->read(buffer, count);
        updateState();
    }
    return *this;
}

fl::size_t ifstream::tellg() {
    if (!mHandle) {
        return 0;
    }
    return mHandle->tell();
}

ifstream& ifstream::seekg(fl::size_t pos, ios::seekdir dir) {
    if (mHandle && mHandle->is_open()) {
        seek_dir seek_direction =
            (dir == ios::seekdir::beg) ? seek_dir::beg :
            (dir == ios::seekdir::cur) ? seek_dir::cur : seek_dir::end;
        mHandle->seek(pos, seek_direction);
        updateState();
    }
    return *this;
}

int ifstream::error() const {
    if (!mHandle) {
        return fl::io::err_bad_file;
    }
    return mHandle->error_code();
}

const char* ifstream::error_message() const {
    if (!mHandle) {
        return "No handle";
    }
    return mHandle->error_message();
}

void ifstream::clear_error() {
    if (mHandle) {
        mHandle->clear_error();
    }
    mFail = false;
    updateState();
}

// ============================================================================
// ofstream Implementation
// ============================================================================

ofstream::ofstream(const char* path, ios::openmode mode)
    : mGood(false), mEof(false), mFail(true), mLocalError(0) {
    open(path, mode);
}

ofstream::ofstream(filebuf_ptr handle)
    : mHandle(handle), mGood(false), mEof(false), mFail(true), mLocalError(0) {
    updateState();
}

ofstream::~ofstream() FL_NOEXCEPT {
    close();
}

void ofstream::open(const char* path, ios::openmode mode) {
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

    mHandle = fl::make_shared<detail::posix_filebuf>(path, fmode);

    if (mHandle->is_open()) {
        if (mode & ios::ate) {
            mHandle->seek(0, seek_dir::end);
        }
        updateState();
    } else {
        updateState();
    }
}

void ofstream::close() {
    if (mHandle && mHandle->is_open()) {
        mHandle->close();
        // After successful close: keep good() = true to match std::ofstream behavior
        // This allows fs_stub.hpp's createTextFile to work correctly
        if (!mHandle->has_error()) {
            mGood = true;
            mEof = false;
            mFail = false;
        } else {
            updateState();
        }
    }
}

ofstream& ofstream::write(const char* data, fl::size_t count) {
    if (mHandle && mHandle->is_open()) {
        fl::size_t written = mHandle->write(data, count);
        if (written != count) {
            mFail = true;
            mGood = false;
        }
        updateState();
    } else {
        // Writing to closed stream - set bad file error
        mLocalError = fl::io::err_bad_file;
        mFail = true;
        mGood = false;
    }
    return *this;
}

int ofstream::error() const {
    if (mLocalError != 0) {
        return mLocalError;
    }
    if (!mHandle) {
        return fl::io::err_bad_file;
    }
    return mHandle->error_code();
}

const char* ofstream::error_message() const {
    if (mLocalError != 0) {
#ifdef FASTLED_TESTING
        return fl::strerror(mLocalError);
#else
        return "Write to closed stream";
#endif
    }
    if (!mHandle) {
        return "No handle";
    }
    return mHandle->error_message();
}

void ofstream::clear_error() {
    mLocalError = 0;
    if (mHandle) {
        mHandle->clear_error();
    }
    mFail = false;
    updateState();
}

// ============================================================================
// fstream Implementation
// ============================================================================

fstream::fstream(const char* path, ios::openmode mode)
    : mLastRead(0), mGood(false), mEof(false), mFail(true), mLocalError(0) {
    open(path, mode);
}

fstream::fstream(filebuf_ptr handle)
    : mHandle(handle), mLastRead(0), mGood(false), mEof(false), mFail(true), mLocalError(0) {
    updateState();
}

fstream::~fstream() FL_NOEXCEPT {
    close();
}

void fstream::open(const char* path, ios::openmode mode) {
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

    mHandle = fl::make_shared<detail::posix_filebuf>(path, fmode);

    if (mHandle->is_open()) {
        if (mode & ios::ate) {
            mHandle->seek(0, seek_dir::end);
        }
        updateState();
    } else {
        updateState();
    }
}

void fstream::close() {
    if (mHandle && mHandle->is_open()) {
        mHandle->close();
        // After successful close: keep good() = true to match std::ofstream behavior
        // This allows fs_stub.hpp's createTextFile to work correctly
        if (!mHandle->has_error()) {
            mGood = true;
            mEof = false;
            mFail = false;
        } else {
            updateState();
        }
    }
}

fstream& fstream::read(char* buffer, fl::size_t count) {
    mLastRead = 0;
    if (mHandle && mHandle->is_open()) {
        mLastRead = mHandle->read(buffer, count);
        updateState();
    }
    return *this;
}

fstream& fstream::write(const char* data, fl::size_t count) {
    if (mHandle && mHandle->is_open()) {
        fl::size_t written = mHandle->write(data, count);
        if (written != count) {
            mFail = true;
            mGood = false;
        }
        updateState();
    } else {
        // Writing to closed stream - set bad file error
        mLocalError = fl::io::err_bad_file;
        mFail = true;
        mGood = false;
    }
    return *this;
}

fl::size_t fstream::tellg() {
    if (!mHandle) {
        return 0;
    }
    return mHandle->tell();
}

fstream& fstream::seekg(fl::size_t pos, ios::seekdir dir) {
    if (mHandle && mHandle->is_open()) {
        seek_dir seek_direction =
            (dir == ios::seekdir::beg) ? seek_dir::beg :
            (dir == ios::seekdir::cur) ? seek_dir::cur : seek_dir::end;
        mHandle->seek(pos, seek_direction);
        updateState();
    }
    return *this;
}

int fstream::error() const {
    if (mLocalError != 0) {
        return mLocalError;
    }
    if (!mHandle) {
        return fl::io::err_bad_file;
    }
    return mHandle->error_code();
}

const char* fstream::error_message() const {
    if (mLocalError != 0) {
#ifdef FASTLED_TESTING
        return fl::strerror(mLocalError);
#else
        return "Write to closed stream";
#endif
    }
    if (!mHandle) {
        return "No handle";
    }
    return mHandle->error_message();
}

void fstream::clear_error() {
    mLocalError = 0;
    if (mHandle) {
        mHandle->clear_error();
    }
    mFail = false;
    updateState();
}

} // namespace fl
