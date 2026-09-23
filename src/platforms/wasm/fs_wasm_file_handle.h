// IWYU pragma: private
#pragma once

#include "fl/fs/file_handle.h"
#include "fl/stl/mutex.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"

namespace fl {

FASTLED_SHARED_PTR(FileData);
FASTLED_SHARED_PTR(WasmFileHandle);

class FileData {
  public:
    explicit FileData(size_t capacity) FL_NO_EXCEPT;
    FileData(const fl::vector<u8> &data, size_t len) FL_NO_EXCEPT;
    FileData() FL_NO_EXCEPT = default;

    void append(const u8 *data, size_t len) FL_NO_EXCEPT;
    size_t read(size_t pos, u8 *dst, size_t len) FL_NO_EXCEPT;
    bool ready(size_t pos) FL_NO_EXCEPT;
    size_t bytesRead() const FL_NO_EXCEPT;
    size_t capacity() const FL_NO_EXCEPT;

  private:
    fl::vector<u8> mData;
    size_t mCapacity = 0;
    mutable fl::mutex mMutex;
};

class WasmFileHandle : public fl::filebuf {
  private:
    FileDataPtr mData;
    size_t mPos;
    string mPath;
    bool mOpen = true;

  public:
    WasmFileHandle(const string &path, const FileDataPtr data) FL_NO_EXCEPT;
    ~WasmFileHandle() FL_NO_EXCEPT override;

    bool is_open() const FL_NO_EXCEPT override;
    bool available() const FL_NO_EXCEPT override;
    fl::size_t bytes_left() const FL_NO_EXCEPT override;
    size_t size() const FL_NO_EXCEPT override;
    size_t read(char *dst, size_t bytesToRead) FL_NO_EXCEPT override;
    using filebuf::read;
    size_t write(const char *data, size_t count) FL_NO_EXCEPT override;
    size_t tell() FL_NO_EXCEPT override;
    const char *path() const FL_NO_EXCEPT override;
    bool seek(size_t pos, fl::seek_dir dir) FL_NO_EXCEPT override;
    using filebuf::seek;
    void close() FL_NO_EXCEPT override;
    bool is_eof() const FL_NO_EXCEPT override;
    bool has_error() const FL_NO_EXCEPT override;
    void clear_error() FL_NO_EXCEPT override;
    int error_code() const FL_NO_EXCEPT override;
    const char *error_message() const FL_NO_EXCEPT override;
};

} // namespace fl
