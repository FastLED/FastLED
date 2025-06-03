#pragma once

#include <stdint.h>
#include <string.h>

#include "fl/geometry.h"
#include "fl/math_macros.h"
#include "fl/namespace.h"
#include "fl/ptr.h"
#include "fl/template_magic.h"
#include "fl/vector.h"

#ifndef FASTLED_STR_INLINED_SIZE
#define FASTLED_STR_INLINED_SIZE 64
#endif

FASTLED_NAMESPACE_BEGIN
struct CRGB;
FASTLED_NAMESPACE_END;

namespace fl { // Mandatory namespace for this class since it has name
               // collisions.

class Str;
using string = fl::Str; // std-like string

template <typename T> struct rect;
template <typename T> struct vec2;
template <typename T> struct vec3;
template <typename T> class Slice;
template <typename T, typename Allocator> class HeapVector;
template <typename T, size_t N> class InlinedVector;
template <typename T, size_t N> class FixedVector;
template <size_t N> class StrN;

template <typename T> struct Hash;

template <typename T> struct EqualTo;

template <typename Key, typename Hash, typename KeyEqual> class HashSet;

class XYMap;

struct FFTBins;

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

FASTLED_SMART_PTR(StringHolder);

class StringFormatter {
  public:
    static void append(int32_t val, StrN<64> *dst);
    static bool isSpace(char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }
    static float parseFloat(const char *str, size_t len);
    static bool isDigit(char c) { return c >= '0' && c <= '9'; }
    static void appendFloat(const float &val, StrN<64> *dst);
};

class StringHolder : public fl::Referent {
  public:
    StringHolder(const char *str);
    StringHolder(size_t length);
    StringHolder(const char *str, size_t length);
    StringHolder(const StringHolder &other) = delete;
    StringHolder &operator=(const StringHolder &other) = delete;
    ~StringHolder();

    bool isShared() const { return ref_count() > 1; }
    void grow(size_t newLength);
    bool hasCapacity(size_t newLength) const { return newLength <= mCapacity; }
    const char *data() const { return mData; }
    char *data() { return mData; }
    size_t length() const { return mLength; }
    size_t capacity() const { return mCapacity; }
    bool copy(const char *str, size_t len) {
        if ((len + 1) > mCapacity) {
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
  protected:
    size_t mLength = 0;
    char mInlineData[SIZE] = {0};
    StringHolderPtr mHeapData;

  public:
    // Constructors
    StrN() = default;

    // cppcheck-suppress-begin [operatorEqVarError]
    template <size_t M> StrN(const StrN<M> &other) { copy(other); }
    StrN(const char *str) {
        size_t len = strlen(str);
        mLength = len;         // Length is without null terminator
        if (len + 1 <= SIZE) { // Check capacity including null
            memcpy(mInlineData, str, len + 1); // Copy including null
            mHeapData.reset();
        } else {
            mHeapData = StringHolderPtr::New(str);
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
            mHeapData = StringHolderPtr::New(str);
        }
    }

    template <int N> StrN(const char (&str)[N]) {
        copy(str, N - 1); // Subtract 1 to not count null terminator
    }
    template <int N> StrN &operator=(const char (&str)[N]) {
        assign(str, N);
        return *this;
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

    void copy(const char *str, size_t len) {
        mLength = len;
        if (len + 1 <= SIZE) {
            memcpy(mInlineData, str, len + 1);
            mHeapData.reset();
        } else {
            mHeapData = StringHolderPtr::New(str, len);
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
                mHeapData = StringHolderPtr::New(other.c_str());
            }
        }
        mLength = len;
    }

    size_t capacity() const { return mHeapData ? mHeapData->capacity() : SIZE; }

    size_t write(const uint8_t *data, size_t n) {
        const char *str = reinterpret_cast<const char *>(data);
        return write(str, n);
    }

    size_t write(const char *str, size_t n) {
        size_t newLen = mLength + n;
        if (mHeapData && !mHeapData->isShared()) {
            if (!mHeapData->hasCapacity(newLen)) {
                size_t grow_length = MAX(3, newLen * 3 / 2);
                mHeapData->grow(grow_length); // Grow by 50%
            }
            memcpy(mHeapData->data() + mLength, str, n);
            mLength = newLen;
            mHeapData->data()[mLength] = '\0';
            return mLength;
        }
        if (newLen + 1 <= SIZE) {
            memcpy(mInlineData + mLength, str, n);
            mLength = newLen;
            mInlineData[mLength] = '\0';
            return mLength;
        }
        mHeapData.reset();
        StringHolderPtr newData = StringHolderPtr::New(newLen);
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

    size_t write(const uint16_t &n) {
        StrN<64> dst;
        StringFormatter::append(n, &dst); // Inlined size should suffice
        return write(dst.c_str(), dst.size());
    }

    size_t write(const uint32_t &val) {
        StrN<64> dst;
        StringFormatter::append(val, &dst); // Inlined size should suffice
        return write(dst.c_str(), dst.size());
    }

    size_t write(const int32_t &val) {
        StrN<64> dst;
        StringFormatter::append(val, &dst); // Inlined size should suffice
        return write(dst.c_str(), dst.size());
    }

    size_t write(const int8_t val) {
        StrN<64> dst;
        StringFormatter::append(int16_t(val),
                                &dst); // Inlined size should suffice
        return write(dst.c_str(), dst.size());
    }

    // Destructor
    ~StrN() {}

    // Accessors
    size_t size() const { return mLength; }
    size_t length() const { return size(); }
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

    bool empty() const { return mLength == 0; }

    // Append method

    bool operator<(const StrN &other) const {
        return strcmp(c_str(), other.c_str()) < 0;
    }

    template <size_t M> bool operator<(const StrN<M> &other) const {
        return strcmp(c_str(), other.c_str()) < 0;
    }

    void reserve(size_t newCapacity) {
        // If capacity is less than current length, do nothing
        if (newCapacity <= mLength) {
            return;
        }

        // If new capacity fits in inline buffer, no need to allocate
        if (newCapacity + 1 <= SIZE) {
            return;
        }

        // If we already have unshared heap data with sufficient capacity, do
        // nothing
        if (mHeapData && !mHeapData->isShared() &&
            mHeapData->hasCapacity(newCapacity)) {
            return;
        }

        // Need to allocate new storage
        StringHolderPtr newData = StringHolderPtr::New(newCapacity);
        if (newData) {
            // Copy existing content
            memcpy(newData->data(), c_str(), mLength);
            newData->data()[mLength] = '\0';
            mHeapData = newData;
        }
    }

    void clear(bool freeMemory = false) {
        mLength = 0;
        if (freeMemory && mHeapData) {
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
    StringHolderPtr mData;
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

    bool operator>(const Str &other) const {
        return strcmp(c_str(), other.c_str()) > 0;
    }

    bool operator>=(const Str &other) const {
        return strcmp(c_str(), other.c_str()) >= 0;
    }

    bool operator<(const Str &other) const {
        return strcmp(c_str(), other.c_str()) < 0;
    }

    bool operator<=(const Str &other) const {
        return strcmp(c_str(), other.c_str()) <= 0;
    }

    bool operator==(const Str &other) const {
        return strcmp(c_str(), other.c_str()) == 0;
    }

    bool operator!=(const Str &other) const {
        return strcmp(c_str(), other.c_str()) != 0;
    }

    Str &operator+=(const Str &other) {
        append(other.c_str(), other.size());
        return *this;
    }

    template <typename T> Str &operator+=(const T &val) {
        append(val);
        return *this;
    }

    // Generic integral append: only enabled if T is an integral type. This is
    // needed because on some platforms type(int) is not one of the integral
    // types like int8_t, int16_t, int32_t, int64_t etc. In such a has just case
    // the value to int32_t and then append it.
    template <typename T, typename = fl::enable_if_t<fl::is_integral<T>::value>>
    Str &append(const T &val) {
        write(int32_t(val));
        return *this;
    }

    template <typename T> Str &append(const Slice<T> &slice) {
        append("[");
        for (size_t i = 0; i < slice.size(); ++i) {
            if (i > 0) {
                append(", ");
            }
            append(slice[i]);
        }
        append("]");
        return *this;
    }

    template <typename T> Str &append(const fl::HeapVector<T> &vec) {
        Slice<const T> slice(vec.data(), vec.size());
        append(slice);
        return *this;
    }

    template <typename T, size_t N>
    Str &append(const fl::InlinedVector<T, N> &vec) {
        Slice<const T> slice(vec.data(), vec.size());
        append(slice);
        return *this;
    }

    Str &append(const char *str) {
        write(str, strlen(str));
        return *this;
    }
    Str &append(const char *str, size_t len) {
        write(str, len);
        return *this;
    }
    // Str& append(char c) { write(&c, 1); return *this; }
    Str &append(const int8_t &c) {
        const char *str = reinterpret_cast<const char *>(&c);
        write(str, 1);
        return *this;
    }
    Str &append(const uint8_t &c) {
        write(uint16_t(c));
        return *this;
    }
    Str &append(const uint16_t &val) {
        write(val);
        return *this;
    }
    Str &append(const int16_t &val) {
        write(int32_t(val));
        return *this;
    }
    Str &append(const uint32_t &val) {
        write(val);
        return *this;
    }
    Str &append(const int32_t &c) {
        write(c);
        return *this;
    }

    Str &append(const bool &val) {
        if (val) {
            return append("true");
        } else {
            return append("false");
        }
    }

    template <typename T> Str &append(const rect<T> &rect) {
        append(rect.mMin.x);
        append(",");
        append(rect.mMin.y);
        append(",");
        append(rect.mMax.x);
        append(",");
        append(rect.mMax.y);
        return *this;
    }

    template <typename T> Str &append(const vec2<T> &pt) {
        append("(");
        append(pt.x);
        append(",");
        append(pt.y);
        append(")");
        return *this;
    }

    template <typename T> Str &append(const vec3<T> &pt) {
        append("(");
        append(pt.x);
        append(",");
        append(pt.y);
        append(",");
        append(pt.z);
        append(")");
        return *this;
    }
        

    template <typename T, size_t N>
    Str &append(const fl::FixedVector<T, N> &vec) {
        Slice<const T> slice(vec.data(), vec.size());
        append(slice);
        return *this;
    }

    Str &append(const CRGB &c);

    Str &append(const float &_val) {
        // round to nearest hundredth
        StringFormatter::appendFloat(_val, this);
        return *this;
    }

    Str &append(const double &val) { return append(float(val)); }

    Str &append(const StrN &str) {
        write(str.c_str(), str.size());
        return *this;
    }

    Str &append(const FFTBins &str);

    Str &append(const XYMap &map);

    template <typename Key, typename Hash, typename KeyEqual>
    Str &append(const HashSet<Key, Hash, KeyEqual> &set) {
        append("{");
        for (auto it = set.begin(); it != set.end(); ++it) {
            if (it != set.begin()) {
                append(", ");
            }
            auto p = *it;
            append(p.first);
        }
        append("}");
        return *this;
    }

    const char *data() const { return c_str(); }

    void swap(Str &other);

  private:
    enum {
        // Bake the size into the string class so we can issue a compile time
        // check
        // to make sure the user doesn't do something funny like try to change
        // the
        // size of the inlined string via an included defined instead of a build
        // define.
        kStrInlineSize = FASTLED_STR_INLINED_SIZE,
    };

    static void compileTimeAssertions();
};

} // namespace fl
