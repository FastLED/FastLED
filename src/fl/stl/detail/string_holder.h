#pragma once

#include "fl/stl/int.h"
#include "fl/stl/cstring.h"
#include "fl/stl/noexcept.h"

namespace fl {

// StringHolder: Heap-allocated string storage with reference counting
// Used when string exceeds inline buffer size
class StringHolder {
  public:
    StringHolder(const char *str);
    StringHolder(size length);
    StringHolder(const char *str, size length);
    StringHolder(const StringHolder &other) FL_NOEXCEPT = delete;
    StringHolder &operator=(const StringHolder &other) FL_NOEXCEPT = delete;
    ~StringHolder() FL_NOEXCEPT;

    void grow(size newLength);
    bool hasCapacity(size newLength) const FL_NOEXCEPT { return newLength + 1 <= mCapacity; }
    const char *data() const FL_NOEXCEPT { return mData; }
    char *data() FL_NOEXCEPT { return mData; }
    size length() const FL_NOEXCEPT { return mLength; }
    size capacity() const FL_NOEXCEPT { return mCapacity; }
    bool copy(const char *str, size len) FL_NOEXCEPT {
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
