#pragma once

#ifdef FASTLED_TESTING
// Test filesystem implementation for stub platform
// Maps SD card operations to real hard drive paths for testing

#include "fl/file_system.h"
#include "fl/log.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/memory.h"
#include <algorithm>  // For std::replace in path conversion
#include <fstream>    // For file I/O operations
#include <cstdio>     // For file operations
#ifdef _WIN32
  #include <direct.h>
  #include <io.h>
#else
  #include <unistd.h>
  #include <sys/stat.h>
#endif

namespace fl {

class StubFileHandle : public FileHandle {
private:
    std::ifstream mFile;  // okay std namespace
    std::string mPath;  // okay std namespace
    std::size_t mSize;  // okay std namespace
    std::size_t mPos;  // okay std namespace

public:
    StubFileHandle(const std::string& path) : mPath(path), mPos(0) {  // okay std namespace
        mFile.open(path, std::ios::binary | std::ios::ate);  // okay std namespace
        if (mFile.is_open()) {
            mSize = mFile.tellg();
            mFile.seekg(0, std::ios::beg);  // okay std namespace
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
        mFile.seekg(pos, std::ios::beg);  // okay std namespace
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
    std::string mRootPath;  // okay std namespace

public:
    StubFileSystem() = default;
    ~StubFileSystem() override = default;

    void setRootPath(const std::string& path) {  // okay std namespace
        mRootPath = path;
        // Ensure the path ends with a directory separator
        if (!mRootPath.empty() && mRootPath.back() != '/' && mRootPath.back() != '\\') {
            mRootPath.append("/");
        }
    }

    // Static test utility functions for file/directory management
    // These are only available on the stub/test platform
    static bool createDirectory(const std::string& path) {  // okay std namespace
#ifdef _WIN32
        return _mkdir(path.c_str()) == 0 || errno == EEXIST;
#else
        return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
    }

    static bool removeDirectory(const std::string& path) {  // okay std namespace
#ifdef _WIN32
        return _rmdir(path.c_str()) == 0;
#else
        return rmdir(path.c_str()) == 0;
#endif
    }

    static bool removeFile(const std::string& path) {  // okay std namespace
        return ::remove(path.c_str()) == 0;
    }

    static bool createTextFile(const std::string& path, const std::string& content) {  // okay std namespace
        std::ofstream ofs(path);  // okay std namespace
        if (!ofs.is_open()) {
            return false;
        }
        ofs << content;
        return ofs.good();
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
        std::string full_path = mRootPath;  // okay std namespace
        full_path.append(path);

        // Convert forward slashes to platform-appropriate separators
#ifdef _WIN32
        std::replace(full_path.begin(), full_path.end(), '/', '\\');  // okay std namespace
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