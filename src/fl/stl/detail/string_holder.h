#pragma once

#include "fl/stl/int.h"
#include "fl/stl/cstring.h"
#include "fl/stl/noexcept.h"

namespace fl {

// StringHolder: Heap-allocated string storage with reference counting
// Used when string exceeds inline buffer size
class StringHolder {
  public:
    StringHolder(const char *str) FL_NO_EXCEPT;
    StringHolder(size length) FL_NO_EXCEPT;
    StringHolder(const char *str, size length) FL_NO_EXCEPT;
    StringHolder(const StringHolder &other) FL_NO_EXCEPT = delete;
    StringHolder &operator=(const StringHolder &other) FL_NO_EXCEPT = delete;
    ~StringHolder() FL_NO_EXCEPT;

    void grow(size newLength) FL_NO_EXCEPT;
    bool hasCapacity(size newLength) const FL_NO_EXCEPT { return newLength + 1 <= mCapacity; }
    const char *data() const FL_NO_EXCEPT { return mData; }
    char *data() FL_NO_EXCEPT { return mData; }
    size length() const FL_NO_EXCEPT { return mLength; }
    size capacity() const FL_NO_EXCEPT { return mCapacity; }
    bool copy(const char *str, size len) FL_NO_EXCEPT {
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
