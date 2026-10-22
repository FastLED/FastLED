#pragma once

// IWYU pragma: private
// Composed into fl.fs.embedded+.cpp, never the common platforms unity object.

#include "fl/fs/backend.h"
#include "fl/stl/string.h"
#include "fl/stl/memory.h"
#include "platforms/esp/fs/embedded_fs_mount.hpp"
// IWYU pragma: begin_keep
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
// IWYU pragma: end_keep

namespace fl {
namespace platforms {
namespace detail {

class NativeEmbeddedFile : public filebuf {
    int mFd;
    mutable int mError = 0;
    fl::size mPosition = 0;
    string mPath;
    EmbeddedMountPtr mMount;
public:
    NativeEmbeddedFile(int fd, string path,
                       EmbeddedMountPtr mount) FL_NO_EXCEPT
        : mFd(fd), mPath(fl::move(path)), mMount(fl::move(mount)) {}
    ~NativeEmbeddedFile() FL_NO_EXCEPT override { close(); }
    bool is_open() const FL_NO_EXCEPT override { return mFd >= 0; }
    void close() FL_NO_EXCEPT override {
        if (mFd >= 0) {
            if (::close(mFd) != 0) mError = errno;
            mFd = -1;
        }
        mMount.reset(); // Close the SDK descriptor before releasing its mount.
    }
    fl::size size() const FL_NO_EXCEPT override {
        struct stat info = {};
        if (!is_open()) { mError = EBADF; return 0; }
        if (::fstat(mFd, &info) != 0) { mError = errno; return 0; }
        if (info.st_size < 0) { mError = EIO; return 0; }
        return static_cast<fl::size>(info.st_size);
    }
    const char* path() const FL_NO_EXCEPT override { return mPath.c_str(); }
    fl::size tell() FL_NO_EXCEPT override { return is_open() ? mPosition : 0; }
    bool is_eof() const FL_NO_EXCEPT override { return !is_open() || mPosition >= size(); }
    bool has_error() const FL_NO_EXCEPT override { return mError != 0; }
    int error_code() const FL_NO_EXCEPT override { return mError; }
    const char* error_message() const FL_NO_EXCEPT override { return mError ? "Embedded storage I/O error" : "No error"; }
    void clear_error() FL_NO_EXCEPT override { mError = 0; }
    fl::size write(const char*, fl::size) FL_NO_EXCEPT override { return 0; }
    fl::size read(char* dst, fl::size count) FL_NO_EXCEPT override {
        if (!is_open() || (!dst && count)) { mError = EBADF; return 0; }
        if (!count) return 0;
        // Let VFS report EOF or errors. Cached filesystem metadata may lag
        // another writer, so size() must not suppress a valid SDK read.
        if (count > 0x7fffffffu) count = 0x7fffffffu;
        const auto bytes = ::read(mFd, dst, count);
        if (bytes < 0) { mError = errno; return 0; }
        mPosition += static_cast<fl::size>(bytes);
        mError = 0;
        return static_cast<fl::size>(bytes);
    }
    bool seek(fl::size position, seek_dir dir) FL_NO_EXCEPT override {
        if (!is_open() || position > 0x7fffffffu) { mError = EINVAL; return false; }
        const int origin = dir == seek_dir::beg ? SEEK_SET : dir == seek_dir::cur ? SEEK_CUR : SEEK_END;
        const auto result = ::lseek(mFd, static_cast<off_t>(position), origin);
        if (result < 0) { mError = errno; return false; }
        mPosition = static_cast<fl::size>(result);
        mError = 0;
        return true;
    }
    using filebuf::read;
    using filebuf::seek;
};

class NativeEmbeddedFs : public FsImpl {
    bool mFormat;
    EmbeddedMountPtr mMount;
public:
    explicit NativeEmbeddedFs(bool format) FL_NO_EXCEPT : mFormat(format) {}
    ~NativeEmbeddedFs() FL_NO_EXCEPT override { end(); }
    bool begin() FL_NO_EXCEPT override {
        if (mMount) return true;
        auto mount = acquireEmbeddedMount(mFormat);
        if (!mount) return false;
        // The SDK has no public base-path getter for borrowed volumes. Adopt
        // the standard route only; custom Arduino routes need their provider.
        struct stat root = {};
        if (::stat("/littlefs", &root) != 0 || !S_ISDIR(root.st_mode)) return false;
        mMount = fl::move(mount);
        return true;
    }
    void end() FL_NO_EXCEPT override { mMount.reset(); }
    filebuf_ptr openRead(const char* path) FL_NO_EXCEPT override {
        if (!mMount || !path || !*path) return filebuf_ptr();
        // Reject parent traversal so a read cannot leave this storage root.
        string logical{path};
        if (logical.size() != fl::strlen(path)) return filebuf_ptr();
        for (fl::size i = 0; i < logical.size(); ++i) {
            if ((i == 0 || logical[i-1] == '/') && logical[i] == '.' &&
                i+1 < logical.size() && logical[i+1] == '.' &&
                (i+2 == logical.size() || logical[i+2] == '/')) return filebuf_ptr();
        }
        string full{"/littlefs"};
        if (*path != '/') full += '/';
        full += logical;
        if (full.size() != 9u + (*path != '/' ? 1u : 0u) + logical.size()) return filebuf_ptr();
        const int fd = ::open(full.c_str(), O_RDONLY);
        if (fd < 0) return filebuf_ptr();
        struct stat info = {};
        if (::fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 0) {
            ::close(fd);
            return filebuf_ptr();
        }
        auto file = fl::make_shared<NativeEmbeddedFile>(fd, fl::move(logical), mMount);
        if (!file) ::close(fd);
        return file;
    }
};

} // namespace detail
inline FsImplPtr makeEmbeddedFs(bool format) FL_NO_EXCEPT {
    return fl::make_shared<detail::NativeEmbeddedFs>(format);
}
} // namespace platforms
} // namespace fl
