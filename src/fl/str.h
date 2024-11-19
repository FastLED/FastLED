#pragma once

#include <string.h>
#include <stdint.h>

#include "ref.h"
#include "template_magic.h"
#include "fixed_vector.h"
#include "namespace.h"

#ifndef FASTLED_STR_INLINED_SIZE
#define FASTLED_STR_INLINED_SIZE 64
#endif

namespace fl {  // Mandatory namespace for this class since it has name collisions.

template <size_t N> class StrN;

// A copy on write string class. Fast to copy from another
// Str object as read only pointers are shared. If the size
// of the string is below FASTLED_STR_INLINED_SIZE then the
// the entire string fits in the object and no heap allocation occurs.
// When the string is large enough it will overflow into heap
// allocated memory. Copy on write means that if the Str has
// a heap buffer that is shared with another Str object, then
// a copy is made and then modified in place.
// If write() or append() is called then the internal data structure
// will grow to accomodate the new data with extra space for future,
// like a vector.
class Str;

///////////////////////////////////////////////////////
// Implementation details.

FASTLED_SMART_REF(StringHolder);

class StringFormatter {
  public:
    static void append(int val, StrN<64> *dst);
    static bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
    static float parseFloat(const char *str, size_t len);
    static bool isDigit(char c) { return c >= '0' && c <= '9'; }
};

class StringHolder : public Referent {
  public:
    StringHolder(const char *str);
    StringHolder(size_t length);
    StringHolder(const char *str, size_t length);
    ~StringHolder();
    bool isShared() const { return ref_count() > 1; }
    void grow(size_t newLength);
    bool hasCapacity(size_t newLength) const { return newLength <= mCapacity; }
    const char *data() const { return mData; }
    char *data() { return mData; }
    size_t length() const { return mLength; }
    size_t capacity() const { return mCapacity; }
    bool copy(const char *str, size_t len) {
        if ((len+1) > mCapacity) {
            return false;
        }
        memcpy(mData, str, len);
        mData[len] = '\0';
        mLength = len;
        return true;
    }
    
    
  private:
    char *mData = nullptr;
    size_t mLength = 0;
    size_t mCapacity = 0;
};

template <size_t SIZE = 64> class StrN {
  private:
    size_t mLength = 0;
    char mInlineData[SIZE] = {0};
    StringHolderRef mHeapData;

  public:
    // Constructors
    StrN() = default;

    // cppcheck-suppress-begin [operatorEqVarError]
    template <size_t M> StrN(const StrN<M> &other) { copy(other); }
    StrN(const char *str) {
        size_t len = strlen(str);
        mLength = len;
        if (len + 1 <= SIZE) {
            memcpy(mInlineData, str, len + 1);
            mHeapData.reset();
        } else {
            mHeapData = StringHolderRef::New(str);
        }
    }
    StrN(const StrN &other) { copy(other); }
    void copy(const char *str) {
        size_t len = strlen(str);
        mLength = len;
        if (len + 1 <= SIZE) {
            memcpy(mInlineData, str, len + 1);
            mHeapData.reset();
        } else {
            if (mHeapData && !mHeapData->isShared()) {
                // We are the sole owners of this data so we can modify it
                mHeapData->copy(str, len);
                return;
            }
            mHeapData.reset();
            mHeapData = StringHolderRef::New(str);
        }
    }
    StrN &operator=(const StrN &other) {
        copy(other);
        return *this;
    }
    template <size_t M> StrN &operator=(const StrN<M> &other) {
        copy(other);
        return *this;
    }
    // cppcheck-suppress-end

    bool operator==(const StrN &other) const {
        return strcmp(c_str(), other.c_str()) == 0;
    }

    bool operator!=(const StrN &other) const {
        return strcmp(c_str(), other.c_str()) != 0;
    }

    void copy(const char* str, size_t len) {
        mLength = len;
        if (len + 1 <= SIZE) {
            memcpy(mInlineData, str, len + 1);
            mHeapData.reset();
        } else {
            mHeapData = StringHolderRef::New(str, len);
        }
    }

    template <size_t M> void copy(const StrN<M> &other) {
        size_t len = other.size();
        if (len + 1 <= SIZE) {
            memcpy(mInlineData, other.c_str(), len + 1);
            mHeapData.reset();
        } else {
            if (other.mHeapData) {
                mHeapData = other.mHeapData;
            } else {
                mHeapData = StringHolderRef::New(other.c_str());
            }
        }
        mLength = len;
    }

    size_t write(int n) {
        StrN<64> dst;
        StringFormatter::append(n, &dst); // Inlined size should suffice
        return write(dst.c_str(), dst.size());
    }

    size_t write(const uint8_t *data, size_t n) {
        const char *str = reinterpret_cast<const char *>(data);
        return write(str, n);
    }

    size_t write(const char *str, size_t n) {
        size_t newLen = mLength + n;
        if (newLen + 1 <= SIZE) {
            memcpy(mInlineData + mLength, str, n);
            mLength = newLen;
            mInlineData[mLength] = '\0';
            return mLength;
        }
        if (mHeapData && !mHeapData->isShared()) {
            if (!mHeapData->hasCapacity(newLen)) {
                mHeapData->grow(newLen * 3 / 2); // Grow by 50%
            }
            memcpy(mHeapData->data() + mLength, str, n);
            mLength = newLen;
            mHeapData->data()[mLength] = '\0';
            return mLength;
        }
        mHeapData.reset();
        StringHolderRef newData = StringHolderRef::New(newLen);
        if (newData) {
            memcpy(newData->data(), c_str(), mLength);
            memcpy(newData->data() + mLength, str, n);
            newData->data()[newLen] = '\0';
            mHeapData = newData;
            mLength = newLen;
        }
        mHeapData = newData;
        return mLength;
    }

    size_t write(char c) { return write(&c, 1); }

    size_t write(uint8_t c) {
        const char *str = reinterpret_cast<const char *>(&c);
        return write(str, 1);
    }

    // Destructor
    ~StrN() {}

    // Accessors
    size_t size() const { return mLength; }
    size_t length() const { return mLength; }
    const char *c_str() const {
        return mHeapData ? mHeapData->data() : mInlineData;
    }

    char *c_str_mutable() {
        return mHeapData ? mHeapData->data() : mInlineData;
    }

    char &operator[](size_t index) {
        if (index >= mLength) {
            static char dummy = '\0';
            return dummy;
        }
        return c_str_mutable()[index];
    }

    const char &operator[](size_t index) const {
        if (index >= mLength) {
            static char dummy = '\0';
            return dummy;
        }
        return c_str()[index];
    }

    // Append method
    void append(const char *str) { write(str, strlen(str)); }
    void append(char c) { write(&c, 1); }
    void append(const StrN &str) { write(str.c_str(), str.size()); }

    bool operator<(const StrN &other) const {
        return strcmp(c_str(), other.c_str()) < 0;
    }

    template<size_t M> bool operator<(const StrN<M> &other) const {
        return strcmp(c_str(), other.c_str()) < 0;
    }

    void clear() {
        mLength = 0;
        if (mHeapData) {
            mHeapData.reset();
        }
    }



    int16_t find(const char &value) const {
        for (size_t i = 0; i < mLength; ++i) {
            if (c_str()[i] == value) {
                return i;
            }
        }
        return -1;
    }

    StrN substring(size_t start, size_t end) const {
        if (start >= mLength) {
            return StrN();
        }
        if (end > mLength) {
            end = mLength;
        }
        if (start >= end) {
            return StrN();
        }
        StrN out;
        out.copy(c_str() + start, end - start);
        return out;
    }



    StrN trim() const {
        StrN out;
        size_t start = 0;
        size_t end = mLength;
        while (start < mLength && StringFormatter::isSpace(c_str()[start])) {
            start++;
        }
        while (end > start && StringFormatter::isSpace(c_str()[end - 1])) {
            end--;
        }
        return substring(start, end);
    }

    float toFloat() const {
        return StringFormatter::parseFloat(c_str(), mLength);
    }

  private:
    StringHolderRef mData;
};

class Str : public StrN<FASTLED_STR_INLINED_SIZE> {
  public:
    Str() : StrN<FASTLED_STR_INLINED_SIZE>() {}
    Str(const char *str) : StrN<FASTLED_STR_INLINED_SIZE>(str) {}
    Str(const Str &other) : StrN<FASTLED_STR_INLINED_SIZE>(other) {}
    template <size_t M>
    Str(const StrN<M> &other) : StrN<FASTLED_STR_INLINED_SIZE>(other) {}
    Str &operator=(const Str &other) {
        copy(other);
        return *this;
    }
};

// Make compatible with std::ostream and other ostream-like objects
FASTLED_DEFINE_OUTPUT_OPERATOR(Str) {
    os << obj.c_str();
    return os;
}

} // namespace fl