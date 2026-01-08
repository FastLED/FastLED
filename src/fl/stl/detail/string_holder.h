#pragma once

#include "fl/int.h"
#include "fl/stl/cstring.h"

namespace fl {

// StringHolder: Heap-allocated string storage with reference counting
// Used when string exceeds inline buffer size
class StringHolder {
  public:
    StringHolder(const char *str);
    StringHolder(size length);
    StringHolder(const char *str, size length);
    StringHolder(const StringHolder &other) = delete;
    StringHolder &operator=(const StringHolder &other) = delete;
    ~StringHolder();

    void grow(size newLength);
    bool hasCapacity(size newLength) const { return newLength + 1 <= mCapacity; }
    const char *data() const { return mData; }
    char *data() { return mData; }
    size length() const { return mLength; }
    size capacity() const { return mCapacity; }
    bool copy(const char *str, size len) {
        if ((len + 1) > mCapacity) {
            return false;
        }
        memcpy(mData, str, len);
        mData[len] = '\0';
        mLength = len;
        return true;
    }

  private:
    char* mData;
    size mLength = 0;
    size mCapacity = 0;
};

} // namespace fl
