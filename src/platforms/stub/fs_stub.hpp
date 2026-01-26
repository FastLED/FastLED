#pragma once

#ifdef FASTLED_TESTING
// Test filesystem implementation for stub platform
// Maps SD card operations to real hard drive paths for testing

#include "fl/file_system.h"
#include "fl/log.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/memory.h"
#include "fl/stl/fstream.h"  // For file I/O operations
#include "fl/stl/string.h"   // For fl::string
#include "fl/stl/algorithm.h"  // For fl::replace in path conversion
#include <cstdio>     // For file operations (remove, etc.)
#include <errno.h>    // For errno
#ifdef _WIN32
  #include <direct.h>
  #include <io.h>
  #include <sys/stat.h>  // For _stat
#else
  #include <unistd.h>
  #include <sys/stat.h>
  #include <dirent.h>   // For directory iteration
#endif

namespace fl {

class StubFileHandle : public FileHandle {
private:
    fl::ifstream mFile;
    fl::string mPath;
    fl::size_t mSize;
    fl::size_t mPos;

public:
    StubFileHandle(const fl::string& path) : mPath(path), mPos(0) {
        mFile.open(path.c_str(), fl::ios::binary | fl::ios::ate);
        if (mFile.is_open()) {
            mSize = mFile.tellg();
            mFile.seekg(0, fl::ios::beg);
        } else {
            mSize = 0;
        }
    }

    ~StubFileHandle() override {
        if (mFile.is_open()) {
            mFile.close();
        }
    }

    bool available() const override {
        return mFile.is_open() && mPos < mSize;
    }

    fl::size size() const override {
        return mSize;
    }

    fl::size read(u8 *dst, fl::size bytesToRead) override {
        if (!mFile.is_open() || mPos >= mSize) {
            return 0;
        }

        fl::size bytesAvailable = mSize - mPos;
        fl::size bytesToActuallyRead = (bytesToRead < bytesAvailable) ? bytesToRead : bytesAvailable;

        mFile.read(fl::bit_cast<char*>(dst), bytesToActuallyRead);
        fl::size bytesActuallyRead = mFile.gcount();
        mPos += bytesActuallyRead;

        return bytesActuallyRead;
    }

    fl::size pos() const override {
        return mPos;
    }

    const char* path() const override {
        return mPath.c_str();
    }

    bool seek(fl::size pos) override {
        if (!mFile.is_open() || pos > mSize) {
            return false;
        }
        mFile.seekg(pos, fl::ios::beg);
        mPos = pos;
        return true;
    }

    void close() override {
        if (mFile.is_open()) {
            mFile.close();
        }
    }

    bool valid() const override {
        return mFile.is_open();
    }
};

class StubFileSystem : public FsImpl {
private:
    fl::string mRootPath;

public:
    StubFileSystem() = default;
    ~StubFileSystem() override = default;

    void setRootPath(const fl::string& path) {
        mRootPath = path;
        // Ensure the path ends with a directory separator
        if (!mRootPath.empty() && mRootPath.back() != '/' && mRootPath.back() != '\\') {
            mRootPath.append("/");
        }
    }

    // Static test utility functions for file/directory management
    // These are only available on the stub/test platform
    static bool createDirectory(const fl::string& path) {
#ifdef _WIN32
        return _mkdir(path.c_str()) == 0 || errno == EEXIST;
#else
        return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
    }

    static bool removeDirectory(const fl::string& path) {
#ifdef _WIN32
        return _rmdir(path.c_str()) == 0;
#else
        return rmdir(path.c_str()) == 0;
#endif
    }

    static bool removeFile(const fl::string& path) {
        // Silently succeed if file doesn't exist (cleanup idempotency)
        if (::remove(path.c_str()) == 0) {
            return true;
        }
        return errno == ENOENT; // Success if file didn't exist
    }

    static void forceRemoveDirectory(const fl::string& path) {
        // Synchronously and recursively remove directory and all contents
        // This ensures cleanup completes before proceeding
#ifdef _WIN32
        // Windows implementation using _findfirst/_findnext (avoids windows.h conflicts)
        fl::string search_path = path;
        search_path.append("\\*");

        struct _finddata_t find_data;
        intptr_t find_handle = _findfirst(search_path.c_str(), &find_data);

        if (find_handle != -1) {
            do {
                fl::string entry_name = find_data.name;
                if (entry_name != "." && entry_name != "..") {
                    fl::string full_path = path;
                    full_path.append("\\");
                    full_path.append(entry_name);

                    if (find_data.attrib & _A_SUBDIR) {
                        // Recursively remove subdirectory
                        forceRemoveDirectory(full_path);
                    } else {
                        // Remove file
                        ::remove(full_path.c_str());
                    }
                }
            } while (_findnext(find_handle, &find_data) == 0);
            _findclose(find_handle);
        }

        // Finally remove the directory itself
        _rmdir(path.c_str());
#else
        // Unix implementation using opendir/readdir
        DIR* dir = ::opendir(path.c_str());
        if (dir) {
            struct dirent* entry;
            while ((entry = ::readdir(dir)) != nullptr) {
                fl::string entry_name = entry->d_name;
                if (entry_name != "." && entry_name != "..") {
                    fl::string full_path = path;
                    full_path.append("/");
                    full_path.append(entry_name);

                    struct stat st;
                    if (::stat(full_path.c_str(), &st) == 0) {
                        if (S_ISDIR(st.st_mode)) {
                            // Recursively remove subdirectory
                            forceRemoveDirectory(full_path);
                        } else {
                            // Remove file
                            ::remove(full_path.c_str());
                        }
                    }
                }
            }
            ::closedir(dir);
        }

        // Finally remove the directory itself
        ::rmdir(path.c_str());
#endif
    }

    static bool createTextFile(const fl::string& path, const fl::string& content) {
        // Force remove any existing file first to ensure clean state
        removeFile(path.c_str());

        // Create new file with explicit truncate mode
        fl::ofstream ofs(path.c_str(), fl::ios::binary | fl::ios::trunc);
        if (!ofs.is_open()) {
            return false;
        }
        ofs.write(content.data(), content.size());  // Use write() instead of << for exact byte control
        bool success = ofs.good();  // Check status before close
        ofs.close();  // Explicitly close to flush buffers before reading
        return success;
    }

    bool begin() override {
        return true;
    }

    void end() override {
        // Nothing to do
    }

    void close(FileHandlePtr file) override {
        if (file) {
            file->close();
        }
    }

    FileHandlePtr openRead(const char* path) override {
        fl::string full_path = mRootPath;
        full_path.append(path);

        // Convert forward slashes to platform-appropriate separators
#ifdef _WIN32
        fl::replace(full_path.begin(), full_path.end(), '/', '\\');
#endif

        // Check if file exists
#ifdef _WIN32
        if (_access(full_path.c_str(), 0) != 0) {
#else
        if (access(full_path.c_str(), F_OK) != 0) {
#endif
            FL_WARN("Test file not found: " << full_path.c_str());
            return FileHandlePtr();
        }

        auto handle = fl::make_shared<StubFileHandle>(full_path);
        if (!handle->valid()) {
            FL_WARN("Failed to open test file: " << full_path.c_str());
            return FileHandlePtr();
        }

        return handle;
    }
};

// Function declarations - implementations are in fs_stub.cpp
FsImplPtr make_sdcard_filesystem(int cs_pin);
void setTestFileSystemRoot(const char* root_path);
const char* getTestFileSystemRoot();

} // namespace fl

#endif // FASTLED_TESTING