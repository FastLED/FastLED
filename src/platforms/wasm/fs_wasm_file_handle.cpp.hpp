// IWYU pragma: private

#include "platforms/wasm/fs_wasm_file_handle.h"
#include "platforms/wasm/is_wasm.h"

#if defined(FL_IS_WASM) || defined(FASTLED_TESTING)

#include "fl/log/log.h"
#include "fl/math/math.h"
#include "fl/stl/cstring.h"
#include "fl/stl/memory.h"

namespace fl {

FileData::FileData(size_t capacity) FL_NO_EXCEPT : mCapacity(capacity) {
    mData.reserve(capacity);
}

FileData::FileData(const fl::vector<u8> &data, size_t len) FL_NO_EXCEPT
    : mData(data), mCapacity(len) {}

void FileData::append(const u8 *data, size_t len) FL_NO_EXCEPT {
    fl::unique_lock<fl::mutex> lock(mMutex);
    mData.insert(mData.end(), data, data + len);
    mCapacity = fl::max(mCapacity, mData.size());
}

size_t FileData::read(size_t pos, u8 *dst, size_t len) FL_NO_EXCEPT {
    fl::unique_lock<fl::mutex> lock(mMutex);
    if (pos >= mData.size()) {
        return 0;
    }
    size_t bytesAvailable = mData.size() - pos;
    size_t bytesToActuallyRead = fl::min(len, bytesAvailable);
    fl::memcpy(dst, mData.data() + pos, bytesToActuallyRead);
    return bytesToActuallyRead;
}

bool FileData::ready(size_t pos) FL_NO_EXCEPT {
    fl::unique_lock<fl::mutex> lock(mMutex);
    return mData.size() == mCapacity || pos < mData.size();
}

size_t FileData::bytesRead() const FL_NO_EXCEPT {
    fl::unique_lock<fl::mutex> lock(mMutex);
    return mData.size();
}

size_t FileData::capacity() const FL_NO_EXCEPT {
    fl::unique_lock<fl::mutex> lock(mMutex);
    return mCapacity;
}

WasmFileHandle::WasmFileHandle(const string &path, const FileDataPtr data) FL_NO_EXCEPT
    : mData(data), mPos(0), mPath(path) {}

WasmFileHandle::~WasmFileHandle() FL_NO_EXCEPT {}

bool WasmFileHandle::is_open() const FL_NO_EXCEPT { return mOpen; }

bool WasmFileHandle::available() const FL_NO_EXCEPT {
    if (!mOpen || mPos >= mData->capacity()) {
        return false;
    }
    if (!mData->ready(mPos)) {
        FL_WARN("File is not ready yet. This is a major error because "
                  "FastLED-wasm does not support async yet, the file "
                  "will fail to read.");
        return false;
    }
    return true;
}

fl::size_t WasmFileHandle::bytes_left() const FL_NO_EXCEPT {
    if (!available()) {
        return 0;
    }
    return mData->capacity() - mPos;
}

size_t WasmFileHandle::size() const FL_NO_EXCEPT { return mData->capacity(); }

size_t WasmFileHandle::read(char *dst, size_t bytesToRead) FL_NO_EXCEPT {
    if (!mOpen || mPos >= mData->capacity()) {
        return 0;
    }
    if (mPos + bytesToRead > mData->capacity()) {
        bytesToRead = mData->capacity() - mPos;
    }
    if (!mData->ready(mPos)) {
        FL_WARN("File is not ready yet. This is a major error because "
                  "FastLED-wasm does not support async yet, the file "
                  "will fail to read.");
        return 0;
    }
    size_t bytesRead =
        mData->read(mPos, reinterpret_cast<u8 *>(dst), bytesToRead); // ok reinterpret cast
    mPos += bytesRead;
    return bytesRead;
}

size_t WasmFileHandle::write(const char *data, size_t count) FL_NO_EXCEPT {
    (void)data;
    (void)count;
    return 0;
}

size_t WasmFileHandle::tell() FL_NO_EXCEPT { return mPos; }
const char *WasmFileHandle::path() const FL_NO_EXCEPT { return mPath.c_str(); }

bool WasmFileHandle::seek(size_t pos, fl::seek_dir dir) FL_NO_EXCEPT {
    if (!mOpen) {
        return false;
    }
    size_t target = pos;
    if (dir == fl::seek_dir::cur) {
        target = mPos + pos;
    } else if (dir == fl::seek_dir::end) {
        target = mData->capacity() + pos;
    }
    if (target > mData->capacity()) {
        return false;
    }
    mPos = target;
    return true;
}

void WasmFileHandle::close() FL_NO_EXCEPT { mOpen = false; }

bool WasmFileHandle::is_eof() const FL_NO_EXCEPT {
    return !mOpen || mPos >= mData->capacity();
}
bool WasmFileHandle::has_error() const FL_NO_EXCEPT { return false; }
void WasmFileHandle::clear_error() FL_NO_EXCEPT {}
int WasmFileHandle::error_code() const FL_NO_EXCEPT { return 0; }
const char *WasmFileHandle::error_message() const FL_NO_EXCEPT { return "No error"; }

} // namespace fl

#endif // FL_IS_WASM || FASTLED_TESTING
