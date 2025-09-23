#pragma once

#ifdef FASTLED_TESTING
// Test filesystem implementation for stub platform
// Maps SD card operations to real hard drive paths for testing

#include "fl/file_system.h"
#include "fl/namespace.h"
#include "fl/memory.h"
#include <fstream>
#include <cerrno>
#include <cstdio>
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
    std::ifstream file_;
    std::string path_;
    std::size_t size_;
    std::size_t pos_;

public:
    StubFileHandle(const std::string& path) : path_(path), pos_(0) {
        file_.open(path, std::ios::binary | std::ios::ate);
        if (file_.is_open()) {
            size_ = file_.tellg();
            file_.seekg(0, std::ios::beg);
        } else {
            size_ = 0;
        }
    }

    ~StubFileHandle() override {
        if (file_.is_open()) {
            file_.close();
        }
    }

    bool available() const override {
        return file_.is_open() && pos_ < size_;
    }

    fl::size size() const override {
        return size_;
    }

    fl::size read(fl::u8 *dst, fl::size bytesToRead) override {
        if (!file_.is_open() || pos_ >= size_) {
            return 0;
        }

        fl::size bytesAvailable = size_ - pos_;
        fl::size bytesToActuallyRead = (bytesToRead < bytesAvailable) ? bytesToRead : bytesAvailable;

        file_.read(reinterpret_cast<char*>(dst), bytesToActuallyRead);
        fl::size bytesActuallyRead = file_.gcount();
        pos_ += bytesActuallyRead;

        return bytesActuallyRead;
    }

    fl::size pos() const override {
        return pos_;
    }

    const char* path() const override {
        return path_.c_str();
    }

    bool seek(fl::size pos) override {
        if (!file_.is_open() || pos > size_) {
            return false;
        }
        file_.seekg(pos, std::ios::beg);
        pos_ = pos;
        return true;
    }

    void close() override {
        if (file_.is_open()) {
            file_.close();
        }
    }

    bool valid() const override {
        return file_.is_open();
    }
};

class StubFileSystem : public FsImpl {
private:
    std::string root_path_;

public:
    StubFileSystem() = default;
    ~StubFileSystem() override = default;

    void setRootPath(const std::string& path) {
        root_path_ = path;
        // Ensure the path ends with a directory separator
        if (!root_path_.empty() && root_path_.back() != '/' && root_path_.back() != '\\') {
            root_path_.append("/");
        }
    }

    // Static test utility functions for file/directory management
    // These are only available on the stub/test platform
    static bool createDirectory(const std::string& path) {
#ifdef _WIN32
        return _mkdir(path.c_str()) == 0 || errno == EEXIST;
#else
        return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
    }

    static bool removeDirectory(const std::string& path) {
#ifdef _WIN32
        return _rmdir(path.c_str()) == 0;
#else
        return rmdir(path.c_str()) == 0;
#endif
    }

    static bool removeFile(const std::string& path) {
        return ::remove(path.c_str()) == 0;
    }

    static bool createTextFile(const std::string& path, const std::string& content) {
        std::ofstream ofs(path);
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
        std::string full_path = root_path_;
        full_path.append(path);

        // Convert forward slashes to platform-appropriate separators
#ifdef _WIN32
        std::replace(full_path.begin(), full_path.end(), '/', '\\');
#endif

        // Check if file exists
#ifdef _WIN32
        if (_access(full_path.c_str(), 0) != 0) {
#else
        if (access(full_path.c_str(), F_OK) != 0) {
#endif
            FASTLED_WARN("Test file not found: " << full_path.c_str());
            return FileHandlePtr();
        }

        auto handle = fl::make_shared<StubFileHandle>(full_path);
        if (!handle->valid()) {
            FASTLED_WARN("Failed to open test file: " << full_path.c_str());
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