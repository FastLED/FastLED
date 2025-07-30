#pragma once

/// std::string compatible string class.
// fl::string has inlined memory and copy on write semantics.


#include "fl/int.h"
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <string>
#endif

#include "fl/geometry.h"
#include "fl/math_macros.h"
#include "fl/namespace.h"
#include "fl/memory.h"
#include "fl/optional.h"
#include "fl/type_traits.h"
#include "fl/vector.h"
#include "fl/span.h"
#include "fl/force_inline.h"

#ifndef FASTLED_STR_INLINED_SIZE
#define FASTLED_STR_INLINED_SIZE 64
#endif

FASTLED_NAMESPACE_BEGIN
struct CRGB;
FASTLED_NAMESPACE_END;

namespace fl { // Mandatory namespace for this class since it has name
               // collisions.

class string;
using Str = fl::string;  // backwards compatibility
class Tile2x2_u8_wrap;
class JsonUiInternal;

// Forward declarations for JSON types
struct JsonValue;
class Json;

template <typename T> struct rect;
template <typename T> struct vec2;
template <typename T> struct vec3;
template <typename T> class Slice;
template <typename T, typename Allocator> class HeapVector;
template <typename T, fl::size N> class InlinedVector;
template <typename T, fl::size N> class FixedVector;
template <fl::size N> class StrN;

template <typename T> class WeakPtr;
template <typename T> class Ptr;

template <typename T> struct Hash;

template <typename T> struct EqualTo;

template <typename Key, typename Hash, typename KeyEqual> class HashSet;

template <fl::u32 N> class BitsetFixed;
class bitset_dynamic;
template <fl::u32 N> class BitsetInlined;

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


///////////////////////////////////////////////////////
// Implementation details.

FASTLED_SMART_PTR(StringHolder);

class StringFormatter {
  public:
    static void append(i32 val, StrN<FASTLED_STR_INLINED_SIZE> *dst);
    static void append(u32 val, StrN<FASTLED_STR_INLINED_SIZE> *dst);
    static void append(uint64_t val, StrN<FASTLED_STR_INLINED_SIZE> *dst);
    static void append(i16 val, StrN<FASTLED_STR_INLINED_SIZE> *dst);
    static void append(u16 val, StrN<FASTLED_STR_INLINED_SIZE> *dst);
    static bool isSpace(char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }
    static float parseFloat(const char *str, fl::size len);
    static int parseInt(const char *str, fl::size len);
    static int parseInt(const char *str);
    static bool isDigit(char c) { return c >= '0' && c <= '9'; }
    static void appendFloat(const float &val, StrN<FASTLED_STR_INLINED_SIZE> *dst);
    static void appendFloat(const float &val, StrN<FASTLED_STR_INLINED_SIZE> *dst, int precision);
};

class StringHolder {
  public:
    StringHolder(const char *str);
    StringHolder(fl::size length);
    StringHolder(const char *str, fl::size length);
    StringHolder(const StringHolder &other) = delete;
    StringHolder &operator=(const StringHolder &other) = delete;
    ~StringHolder();


    void grow(fl::size newLength);
    bool hasCapacity(fl::size newLength) const { return newLength <= mCapacity; }
    const char *data() const { return mData; }
    char *data() { return mData; }
    fl::size length() const { return mLength; }
    fl::size capacity() const { return mCapacity; }
    bool copy(const char *str, fl::size len) {
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
    fl::size mLength = 0;
    fl::size mCapacity = 0;
};

template <fl::size SIZE = FASTLED_STR_INLINED_SIZE> class StrN {
  protected:
    fl::size mLength = 0;
    char mInlineData[SIZE] = {0};
    StringHolderPtr mHeapData;

  public:
    // Static constants (like std::string)
    static constexpr fl::size npos = static_cast<fl::size>(-1);

    // Constructors
    StrN() = default;

    // cppcheck-suppress-begin [operatorEqVarError]
    template <fl::size M> StrN(const StrN<M> &other) { copy(other); }
    StrN(const char *str) {
        fl::size len = strlen(str);
        mLength = len;         // Length is without null terminator
        if (len + 1 <= SIZE) { // Check capacity including null
            memcpy(mInlineData, str, len + 1); // Copy including null
            mHeapData.reset();
        } else {
            mHeapData = fl::make_shared<StringHolder>(str);
        }
    }
    StrN(const StrN &other) { copy(other); }
    void copy(const char *str) {
        fl::size len = strlen(str);
        mLength = len;
        if (len + 1 <= SIZE) {
            memcpy(mInlineData, str, len + 1);
            mHeapData.reset();
        } else {
            if (mHeapData && mHeapData.use_count() <= 1) {
                // We are the sole owners of this data so we can modify it
                mHeapData->copy(str, len);
                return;
            }
            mHeapData.reset();
            mHeapData = fl::make_shared<StringHolder>(str);
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
    template <fl::size M> StrN &operator=(const StrN<M> &other) {
        copy(other);
        return *this;
    }
    // cppcheck-suppress-end

    void assign(const char* str, fl::size len) {
        mLength = len;
        if (len + 1 <= SIZE) {
            memcpy(mInlineData, str, len + 1);
            mHeapData.reset();
        } else {
            mHeapData = fl::make_shared<StringHolder>(str, len);
            mHeapData->copy(str, len);
            mHeapData->data()[len] = '\0';
        }
    }

    bool operator==(const StrN &other) const {
        return strcmp(c_str(), other.c_str()) == 0;
    }

    bool operator!=(const StrN &other) const {
        return strcmp(c_str(), other.c_str()) != 0;
    }

    void copy(const char *str, fl::size len) {
        mLength = len;
        if (len + 1 <= SIZE) {
            fl::memcopy(mInlineData, str, len);  // Copy only len characters, not len+1
            mInlineData[len] = '\0';        // Add null terminator manually
            mHeapData.reset();
        } else {
            mHeapData = fl::make_shared<StringHolder>(str, len);
        }
    }

    template <fl::size M> void copy(const StrN<M> &other) {
        fl::size len = other.size();
        if (len + 1 <= SIZE) {
            memcpy(mInlineData, other.c_str(), len + 1);
            mHeapData.reset();
        } else {
            if (other.mHeapData) {
                mHeapData = other.mHeapData;
            } else {
                mHeapData = fl::make_shared<StringHolder>(other.c_str());
            }
        }
        mLength = len;
    }

    fl::size capacity() const { return mHeapData ? mHeapData->capacity() : SIZE; }

    fl::size write(const u8 *data, fl::size n) {
        const char *str = fl::bit_cast_ptr<const char>(static_cast<const void*>(data));
        return write(str, n);
    }

    fl::size write(const char *str, fl::size n) {
        fl::size newLen = mLength + n;
        if (mHeapData && mHeapData.use_count() <= 1) {
            if (!mHeapData->hasCapacity(newLen)) {
                fl::size grow_length = MAX(3, newLen * 3 / 2);
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
        StringHolderPtr newData = fl::make_shared<StringHolder>(newLen);
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

    fl::size write(char c) { return write(&c, 1); }

    fl::size write(u8 c) {
        const char *str = fl::bit_cast_ptr<const char>(static_cast<const void*>(&c));
        return write(str, 1);
    }

    fl::size write(const u16 &n) {
        StrN<FASTLED_STR_INLINED_SIZE> dst;
        StringFormatter::append(n, &dst); // Inlined size should suffice
        return write(dst.c_str(), dst.size());
    }

    fl::size write(const u32 &val) {
        StrN<FASTLED_STR_INLINED_SIZE> dst;
        StringFormatter::append(val, &dst); // Inlined size should suffice
        return write(dst.c_str(), dst.size());
    }

    fl::size write(const uint64_t &val) {
        StrN<FASTLED_STR_INLINED_SIZE> dst;
        StringFormatter::append(val, &dst); // Inlined size should suffice
        return write(dst.c_str(), dst.size());
    }

    fl::size write(const i32 &val) {
        StrN<FASTLED_STR_INLINED_SIZE> dst;
        StringFormatter::append(val, &dst); // Inlined size should suffice
        return write(dst.c_str(), dst.size());
    }

    fl::size write(const i8 val) {
        StrN<FASTLED_STR_INLINED_SIZE> dst;
        StringFormatter::append(i16(val),
                                &dst); // Inlined size should suffice
        return write(dst.c_str(), dst.size());
    }

    // Destructor
    ~StrN() {}

    // Accessors
    fl::size size() const { return mLength; }
    fl::size length() const { return size(); }
    const char *c_str() const {
        return mHeapData ? mHeapData->data() : mInlineData;
    }

    char *c_str_mutable() {
        return mHeapData ? mHeapData->data() : mInlineData;
    }

    char &operator[](fl::size index) {
        if (index >= mLength) {
            static char dummy = '\0';
            return dummy;
        }
        return c_str_mutable()[index];
    }

    char operator[](fl::size index) const {
        if (index >= mLength) {
            static char dummy = '\0';
            return dummy;
        }
        return c_str()[index];
    }

    bool empty() const { return mLength == 0; }

    // Iterator support for range-based for loops
    char* begin() { return c_str_mutable(); }
    char* end() { return c_str_mutable() + mLength; }
    const char* begin() const { return c_str(); }
    const char* end() const { return c_str() + mLength; }
    const char* cbegin() const { return c_str(); }
    const char* cend() const { return c_str() + mLength; }

    // Append method

    bool operator<(const StrN &other) const {
        return strcmp(c_str(), other.c_str()) < 0;
    }

    template <fl::size M> bool operator<(const StrN<M> &other) const {
        return strcmp(c_str(), other.c_str()) < 0;
    }

    void reserve(fl::size newCapacity) {
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
        if (mHeapData && mHeapData.use_count() <= 1 &&
            mHeapData->hasCapacity(newCapacity)) {
            return;
        }

        // Need to allocate new storage
        StringHolderPtr newData = fl::make_shared<StringHolder>(newCapacity);
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



    // Find single character
    fl::size find(const char &value) const {
        for (fl::size i = 0; i < mLength; ++i) {
            if (c_str()[i] == value) {
                return i;
            }
        }
        return npos;
    }

    // Find substring (string literal support)
    fl::size find(const char* substr) const {
        if (!substr) {
            return npos;
        }
        auto begin = c_str();
        const char* found = strstr(begin, substr);
        if (found) {
            return found - begin;
        }
        return npos;
    }

    // Find another string
    template<fl::size M>
    fl::size find(const StrN<M>& other) const {
        return find(other.c_str());
    }

    // Find single character starting from position (like std::string)
    fl::size find(const char &value, fl::size start_pos) const {
        if (start_pos >= mLength) {
            return npos;
        }
        for (fl::size i = start_pos; i < mLength; ++i) {
            if (c_str()[i] == value) {
                return i;
            }
        }
        return npos;
    }

    // Find substring starting from position (like std::string)
    fl::size find(const char* substr, fl::size start_pos) const {
        if (!substr || start_pos >= mLength) {
            return npos;
        }
        auto begin = c_str() + start_pos;
        const char* found = strstr(begin, substr);
        if (found) {
            return found - c_str();
        }
        return npos;
    }

    // Find another string starting from position (like std::string)
    template<fl::size M>
    fl::size find(const StrN<M>& other, fl::size start_pos) const {
        return find(other.c_str(), start_pos);
    }

    // Contains methods for C++23 compatibility
    bool contains(const char* substr) const {
        return find(substr) != npos;
    }

    bool contains(char c) const {
        return find(c) != npos;
    }

    template<fl::size M>
    bool contains(const StrN<M>& other) const {
        return find(other.c_str()) != npos;
    }

    // Starts with methods for C++20 compatibility
    bool starts_with(const char* prefix) const {
        if (!prefix) {
            return true;  // Empty prefix matches any string
        }
        fl::size prefix_len = strlen(prefix);
        if (prefix_len > mLength) {
            return false;
        }
        return strncmp(c_str(), prefix, prefix_len) == 0;
    }

    bool starts_with(char c) const {
        return mLength > 0 && c_str()[0] == c;
    }

    template<fl::size M>
    bool starts_with(const StrN<M>& prefix) const {
        return starts_with(prefix.c_str());
    }

    // Ends with methods for C++20 compatibility
    bool ends_with(const char* suffix) const {
        if (!suffix) {
            return true;  // Empty suffix matches any string
        }
        fl::size suffix_len = strlen(suffix);
        if (suffix_len > mLength) {
            return false;
        }
        return strncmp(c_str() + mLength - suffix_len, suffix, suffix_len) == 0;
    }

    bool ends_with(char c) const {
        return mLength > 0 && c_str()[mLength - 1] == c;
    }

    template<fl::size M>
    bool ends_with(const StrN<M>& suffix) const {
        return ends_with(suffix.c_str());
    }

    StrN substring(fl::size start, fl::size end) const {
        // short cut, it's the same string
        if (start == 0 && end == mLength) {
            return *this;
        }
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

     StrN substr(fl::size start, fl::size length) const {
        // Standard substr(pos, length) behavior - convert to substring(start, end)
        fl::size end = start + length;
        if (end > mLength) {
            end = mLength;
        }
        return substring(start, end);
    }

    StrN substr(fl::size start) const {
        auto end = mLength;
        return substring(start, end);
    }

    void push_back(char c) {
        write(c);
    }

    // Push ASCII character without numeric conversion for display
    void push_ascii(char c) {
        write(c);
    }

    void pop_back() {
        if (mLength > 0) {
            mLength--;
            c_str_mutable()[mLength] = '\0';
        }
    }

    StrN trim() const {
        StrN out;
        fl::size start = 0;
        fl::size end = mLength;
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

class string : public StrN<FASTLED_STR_INLINED_SIZE> {
  public:
    // Standard string npos constant for compatibility
    static const fl::size npos = static_cast<fl::size>(-1);

    static int strcmp(const string& a, const string& b);
    
    string() : StrN<FASTLED_STR_INLINED_SIZE>() {}
    string(const char *str) : StrN<FASTLED_STR_INLINED_SIZE>(str) {}
    string(const char *str, fl::size len) : StrN<FASTLED_STR_INLINED_SIZE>() {
        copy(str, len);
    }
    string(fl::size len, char c) : StrN<FASTLED_STR_INLINED_SIZE>() {
        resize(len, c);
    }
    string(const string &other) : StrN<FASTLED_STR_INLINED_SIZE>(other) {}
    template <fl::size M>
    string(const StrN<M> &other) : StrN<FASTLED_STR_INLINED_SIZE>(other) {}
    string &operator=(const string &other) {
        copy(other);
        return *this;
    }
    
    string &operator=(const char *str) {
        copy(str, strlen(str));
        return *this;
    }

#ifdef __EMSCRIPTEN__
    string(const std::string &str) {
        copy(str.c_str(), str.size());
    }
    string &operator=(const std::string &str) {
        copy(str.c_str(), str.size());
        return *this;
    }
    // append
    string &append(const std::string &str) {
        write(str.c_str(), str.size());
        return *this;
    }
#endif




    bool operator>(const string &other) const {
        return strcmp(c_str(), other.c_str()) > 0;
    }

    bool operator>=(const string &other) const {
        return strcmp(c_str(), other.c_str()) >= 0;
    }

    bool operator<(const string &other) const {
        return strcmp(c_str(), other.c_str()) < 0;
    }

    bool operator<=(const string &other) const {
        return strcmp(c_str(), other.c_str()) <= 0;
    }

    bool operator==(const string &other) const {
        return strcmp(c_str(), other.c_str()) == 0;
    }

    bool operator!=(const string &other) const {
        return strcmp(c_str(), other.c_str()) != 0;
    }

    string &operator+=(const string &other) {
        append(other.c_str(), other.size());
        return *this;
    }

    template <typename T> string &operator+=(const T &val) {
        append(val);
        return *this;
    }

    template <fl::u32 N>
    string &append(const BitsetFixed<N> &bs) {
        bs.to_string(this);
        return *this;
    }

    string &append(const bitset_dynamic &bs) {
        bs.to_string(this);
        return *this;
    }

    template <fl::u32 N>
    string &append(const BitsetInlined<N> &bs) {
        bs.to_string(this);
        return *this;
    }

    char front() const {
        if (empty()) {
            return '\0';
        }
        return c_str()[0];
    }
    char back() const {
        if (empty()) {
            return '\0';
        }
        return c_str()[size() - 1];
    }

    // Push ASCII character without numeric conversion for display
    void push_ascii(char c) {
        write(c);
    }

    // Generic integral append: only enabled if T is an integral type. This is
    // needed because on some platforms type(int) is not one of the integral
    // types like i8, i16, i32, int64_t etc. In such a has just case
    // the value to i32 and then append it.
    template <typename T, typename = fl::enable_if_t<fl::is_integral<T>::value>>
    string &append(const T &val) {
        write(i32(val));
        return *this;
    }

    template <typename T> string &append(const fl::span<T> &slice) {
        append("[");
        for (fl::size i = 0; i < slice.size(); ++i) {
            if (i > 0) {
                append(", ");
            }
            append(slice[i]);
        }
        append("]");
        return *this;
    }

    template <typename T> string &append(const fl::HeapVector<T> &vec) {
        fl::span<const T> slice(vec.data(), vec.size());
        append(slice);
        return *this;
    }

    template <typename T, fl::size N>
    string &append(const fl::InlinedVector<T, N> &vec) {
        fl::span<const T> slice(vec.data(), vec.size());
        append(slice);
        return *this;
    }

    string &append(const char *str) {
        write(str, strlen(str));
        return *this;
    }
    string &append(const char *str, fl::size len) {
        write(str, len);
        return *this;
    }
    string &append(long long val) {
        write(i32(val));
        return *this;
    }
    // string& append(char c) { write(&c, 1); return *this; }
    string &append(const i8 &c) {
        const char *str = fl::bit_cast_ptr<const char>(static_cast<const void*>(&c));
        write(str, 1);
        return *this;
    }
    string &append(const u8 &c) {
        write(static_cast<u32>(c));
        return *this;
    }
    string &append(const u16 &val) {
        write(val);
        return *this;
    }
    string &append(const i16 &val) {
        write(i32(val));
        return *this;
    }
    string &append(const u32 &val) {
        write(val);
        return *this;
    }
    string &append(const uint64_t &val) {
        write(val);
        return *this;
    }
    string &append(const i32 &c) {
        write(c);
        return *this;
    }

    string &append(const bool &val) {
        if (val) {
            return append("true");
        } else {
            return append("false");
        }
    }

    template <typename T> string &append(const rect<T> &rect) {
        append(rect.mMin.x);
        append(",");
        append(rect.mMin.y);
        append(",");
        append(rect.mMax.x);
        append(",");
        append(rect.mMax.y);
        return *this;
    }

    template <typename T> string &append(const vec2<T> &pt) {
        append("(");
        append(pt.x);
        append(",");
        append(pt.y);
        append(")");
        return *this;
    }

    template <typename T> string &append(const vec3<T> &pt) {
        append("(");
        append(pt.x);
        append(",");
        append(pt.y);
        append(",");
        append(pt.z);
        append(")");
        return *this;
    }
    
    
    template <typename T> string &append(const WeakPtr<T> &val) {
        fl::shared_ptr<T> ptr = val.lock();
        append(ptr);
        return *this;
    }

    template <typename T> string &append(const fl::shared_ptr<T>& val) {
        // append(val->toString());
        if (!val) {
            append("shared_ptr(null)");
        } else {
            T* ptr = val.get();
            append("shared_ptr(");
            append(*ptr);
            append(")");
        }
        return *this;
    }

    string &append(const JsonUiInternal& val);
    
    // JSON type append methods - implementations in str.cpp
    string &append(const JsonValue& val);
    string &append(const Json& val);

    template <typename T, fl::size N>
    string &append(const fl::FixedVector<T, N> &vec) {
        fl::span<const T> slice(vec.data(), vec.size());
        append(slice);
        return *this;
    }

    string &append(const CRGB &c);

    string &append(const float &_val) {
        // round to nearest hundredth
        StringFormatter::appendFloat(_val, this);
        return *this;
    }

    string &append(const float &_val, int precision) {
        StringFormatter::appendFloat(_val, this, precision);
        return *this;
    }

    string &append(const double &val) { return append(float(val)); }

    string &append(const StrN &str) {
        write(str.c_str(), str.size());
        return *this;
    }

    string &append(const FFTBins &str);

    string &append(const XYMap &map);

    string &append(const Tile2x2_u8_wrap &tile);

    template <typename Key, typename Hash, typename KeyEqual>
    string &append(const HashSet<Key, Hash, KeyEqual> &set) {
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

    // Support for fl::optional<T> types
    template <typename T>
    string &append(const fl::optional<T> &opt) {
        if (opt.has_value()) {
            append(*opt);
        } else {
            append("nullopt");
        }
        return *this;
    }

    const char *data() const { return c_str(); }

    void swap(string &other);

    // Resize methods to match std::string interface
    void resize(fl::size count) {
        resize(count, char());
    }

    void resize(fl::size count, char ch) {
        if (count < mLength) {
            // Truncate the string
            mLength = count;
            c_str_mutable()[mLength] = '\0';
        } else if (count > mLength) {
            // Extend the string with the specified character
            fl::size additional_chars = count - mLength;
            reserve(count); // Ensure enough capacity
            char* data_ptr = c_str_mutable();
            for (fl::size i = 0; i < additional_chars; ++i) {
                data_ptr[mLength + i] = ch;
            }
            mLength = count;
            data_ptr[mLength] = '\0';
        }
        // If count == mLength, do nothing
    }

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

// to_string template function for converting values to fl::string
// This provides std::to_string equivalent functionality using fl::string
// Delegates to fl::string::append which handles all type conversions

template<typename T>
inline string to_string(T value) {
    string result;
    result.append(value);
    return result;
}

// Specialized to_string for float with precision
inline string to_string(float value, int precision) {
    string result;
    result.append(value, precision);
    return result;
}

// Free operator+ functions for string concatenation
// These allow expressions like: fl::string val = "string" + fl::to_string(5)

// String literal + fl::string
inline string operator+(const char* lhs, const string& rhs) {
    string result(lhs);
    result += rhs;
    return result;
}

// fl::string + string literal
inline string operator+(const string& lhs, const char* rhs) {
    string result(lhs);
    result += rhs;
    return result;
}

// fl::string + fl::string
inline string operator+(const string& lhs, const string& rhs) {
    string result(lhs);
    result += rhs;
    return result;
}

// String literal + any type that can be converted to string
template<typename T>
inline string operator+(const char* lhs, const T& rhs) {
    string result(lhs);
    result += rhs;
    return result;
}

// Any type that can be converted to string + string literal
template<typename T>
inline string operator+(const T& lhs, const char* rhs) {
    string result;
    result.append(lhs);
    result += rhs;
    return result;
}

// fl::string + any type that can be converted to string
template<typename T>
inline string operator+(const string& lhs, const T& rhs) {
    string result(lhs);
    result += rhs;
    return result;
}

// Any type that can be converted to string + fl::string
template<typename T>
inline string operator+(const T& lhs, const string& rhs) {
    string result;
    result.append(lhs);
    result += rhs;
    return result;
}

} // namespace fl
