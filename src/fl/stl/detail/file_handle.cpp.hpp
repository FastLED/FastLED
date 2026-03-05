// Implementation file for fl::detail::posix_file_handle
// This file contains out-of-line definitions to reduce header compilation overhead

#include "fl/stl/detail/file_handle.h"
#include "fl/stl/move.h"

namespace fl {

// ============================================================================
// FileHandle default implementations
// ============================================================================

fl::size_t FileHandle::bytes_left() const {
    fl::size_t s = size();
    // tell() is non-const in the interface, but bytes_left needs it.
    // Use const_cast for this read-only query - tell() doesn't mutate logical state.
    fl::size_t t = const_cast<FileHandle*>(this)->tell();
    return (t <= s) ? (s - t) : 0;
}

fl::size_t FileHandle::pos() const {
    return const_cast<FileHandle*>(this)->tell();
}

namespace detail {

// ============================================================================
// posix_file_handle Implementation
// ============================================================================

posix_file_handle::posix_file_handle(const char* path, const char* mode)
    : mFile(nullptr), mLastError(0), mPath(path ? path : "") {
    mFile = fl::fopen(path, mode);
    if (!mFile) {
        captureError();
    }
}

posix_file_handle::~posix_file_handle() {
    close();
}

posix_file_handle::posix_file_handle(posix_file_handle&& other) noexcept
    : mFile(other.mFile), mLastError(other.mLastError),
      mPath(fl::move(other.mPath)) {
    other.mFile = nullptr;
    other.mLastError = 0;
}

posix_file_handle& posix_file_handle::operator=(posix_file_handle&& other) noexcept {
    if (this != &other) {
        close();
        mFile = other.mFile;
        mLastError = other.mLastError;
        mPath = fl::move(other.mPath);
        other.mFile = nullptr;
        other.mLastError = 0;
    }
    return *this;
}

bool posix_file_handle::is_open() const {
    return mFile != nullptr;
}

void posix_file_handle::close() {
    if (mFile) {
        int result = fl::fclose(mFile);
        mFile = nullptr;
        if (result != 0) {
            captureError();
        } else {
            mLastError = 0;
        }
    }
}

fl::size_t posix_file_handle::read(char* buffer, fl::size_t count) {
    if (!mFile) {
        return 0;
    }
    fl::size_t bytes_read = fl::fread(buffer, 1, count, mFile);

    // fread can return short at EOF - not an error
    if (fl::feof(mFile) || bytes_read > 0) {
        mLastError = 0;
    } else if (fl::ferror(mFile)) {
        captureError();
    }

    return bytes_read;
}

fl::size_t posix_file_handle::write(const char* data, fl::size_t count) {
    if (!mFile) {
        mLastError = fl::io::err_bad_file;
        return 0;
    }
    fl::size_t bytes_written = fl::fwrite(data, 1, count, mFile);
    if (bytes_written != count) {
        captureError();
    } else {
        fl::fflush(mFile);
        mLastError = 0;
    }
    return bytes_written;
}

fl::size_t posix_file_handle::tell() {
    if (!mFile) {
        mLastError = fl::io::err_bad_file;
        return 0;
    }
    long pos = fl::ftell(mFile);
    if (pos < 0) {
        captureError();
        return 0;
    }
    mLastError = 0;
    return static_cast<fl::size_t>(pos);
}

bool posix_file_handle::seek(fl::size_t pos, seek_dir dir) {
    if (!mFile) {
        mLastError = fl::io::err_bad_file;
        return false;
    }
    int whence = (dir == seek_dir::beg) ? fl::io::seek_set :
                 (dir == seek_dir::cur) ? fl::io::seek_cur : fl::io::seek_end;
    int result = fl::fseek(mFile, static_cast<long>(pos), whence);
    if (result != 0) {
        captureError();
        return false;
    }
    mLastError = 0;
    return true;
}

fl::size_t posix_file_handle::size() const {
    if (!mFile) {
        return 0;
    }
    // Save current position, seek to end, get size, restore position
    long cur_pos = fl::ftell(mFile);
    if (cur_pos < 0) {
        return 0;
    }
    fl::fseek(mFile, 0, fl::io::seek_end);
    long end_pos = fl::ftell(mFile);
    fl::fseek(mFile, cur_pos, fl::io::seek_set);
    if (end_pos < 0) {
        return 0;
    }
    return static_cast<fl::size_t>(end_pos);
}

const char* posix_file_handle::path() const {
    return mPath.c_str();
}

bool posix_file_handle::is_eof() const {
    return mFile ? (fl::feof(mFile) != 0) : false;
}

bool posix_file_handle::has_error() const {
    return mLastError != 0 || (mFile && fl::ferror(mFile) != 0);
}

void posix_file_handle::clear_error() {
    clearErrorState();
}

int posix_file_handle::error_code() const {
    return mLastError;
}

const char* posix_file_handle::error_message() const {
    if (mLastError == 0) {
        return "No error";
    }
    return fl::strerror(mLastError);
}

void posix_file_handle::captureError() {
    mLastError = fl::get_errno();
}

void posix_file_handle::clearErrorState() {
    mLastError = 0;
    if (mFile) {
        fl::clearerr(mFile);
    }
}

} // namespace detail
} // namespace fl
