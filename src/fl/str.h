#pragma once

/// std::string compatible string class.
// fl::string has inlined memory and copy on write semantics.


#include "fl/int.h"
#include "fl/cstring.h"

// Forward declarations for string comparison functions
extern "C" {
    fl::size strlen(const char* str);
    int strcmp(const char* str1, const char* str2);
}

#ifdef __EMSCRIPTEN__
#include <string>
#endif

#include "fl/geometry.h"
#include "fl/math_macros.h"
#include "fl/ptr.h"         // For FASTLED_SHARED_PTR macro
#include "ftl/optional.h"
#include "ftl/type_traits.h"
#include "ftl/vector.h"
#include "ftl/span.h"
#include "fl/force_inline.h"
#include "fl/deprecated.h"

#ifndef FASTLED_STR_INLINED_SIZE
#define FASTLED_STR_INLINED_SIZE 64
#endif


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
template <typename T, fl::size Extent> class span;
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
struct CRGB;

} // namespace fl

namespace fl {

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

FASTLED_SHARED_PTR(StringHolder);

class StringFormatter {
  public:
    // Decimal formatting (base 10)
    static void append(i32 val, StrN<FASTLED_STR_INLINED_SIZE> *dst);
    static void append(u32 val, StrN<FASTLED_STR_INLINED_SIZE> *dst);
    static void append(uint64_t val, StrN<FASTLED_STR_INLINED_SIZE> *dst);
    static void append(i16 val, StrN<FASTLED_STR_INLINED_SIZE> *dst);
    static void append(u16 val, StrN<FASTLED_STR_INLINED_SIZE> *dst);

    // Hexadecimal formatting (base 16)
    static void appendHex(i32 val, StrN<FASTLED_STR_INLINED_SIZE> *dst);
    static void appendHex(u32 val, StrN<FASTLED_STR_INLINED_SIZE> *dst);
    static void appendHex(uint64_t val, StrN<FASTLED_STR_INLINED_SIZE> *dst);
    static void appendHex(i16 val, StrN<FASTLED_STR_INLINED_SIZE> *dst);
    static void appendHex(u16 val, StrN<FASTLED_STR_INLINED_SIZE> *dst);

    // Octal formatting (base 8)
    static void appendOct(i32 val, StrN<FASTLED_STR_INLINED_SIZE> *dst);
    static void appendOct(u32 val, StrN<FASTLED_STR_INLINED_SIZE> *dst);
    static void appendOct(uint64_t val, StrN<FASTLED_STR_INLINED_SIZE> *dst);
    static void appendOct(i16 val, StrN<FASTLED_STR_INLINED_SIZE> *dst);
    static void appendOct(u16 val, StrN<FASTLED_STR_INLINED_SIZE> *dst);

    // Utility methods
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
        fl::memcpy(mData, str, len);
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
            fl::memcpy(mInlineData, str, len + 1); // Copy including null
            mHeapData.reset();
        } else {
            mHeapData = fl::make_shared<StringHolder>(str);
        }
    }
    StrN(const StrN &other) { copy(other); }

    // Move constructor (C++11 compatibility)
    // Steals resources from other, leaving it in a valid but empty state
    StrN(StrN&& other) noexcept
        : mLength(other.mLength), mHeapData(fl::move(other.mHeapData)) {
        // If other was using inline data, copy it
        if (!other.mHeapData) {
            fl::memcpy(mInlineData, other.mInlineData, SIZE);
        }
        // Leave other in a valid empty state
        other.mLength = 0;
        other.mInlineData[0] = '\0';
    }

    // Iterator range constructor (std::string compatibility)
    // Constructs string from iterator range [first, last)
    // This enables construction from any iterator pair including:
    // - Character arrays: StrN(arr, arr + N)
    // - Other strings: StrN(other.begin(), other.end())
    // - Partial ranges: StrN(str.begin() + 2, str.begin() + 7)
    template <typename InputIt>
    StrN(InputIt first, InputIt last) : StrN() {
        // Delegate to assign() which has all the logic
        assign(first, last);
    }

    // Move assignment operator (C++11 compatibility)
    // Steals resources from other, leaving it in a valid but empty state
    StrN& operator=(StrN&& other) noexcept {
        if (this != &other) {
            // Steal other's resources
            mLength = other.mLength;
            mHeapData = fl::move(other.mHeapData);

            // If other was using inline data, copy it
            if (!other.mHeapData) {
                fl::memcpy(mInlineData, other.mInlineData, SIZE);
            }

            // Leave other in a valid empty state
            other.mLength = 0;
            other.mInlineData[0] = '\0';
        }
        return *this;
    }

    void copy(const char *str) {
        fl::size len = strlen(str);
        mLength = len;
        if (len + 1 <= SIZE) {
            fl::memcpy(mInlineData, str, len + 1);
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

    // Assignment operations (std::string compatibility)
    // Assign from raw buffer with explicit length
    void assign(const char* str, fl::size len) {
        mLength = len;
        if (len + 1 <= SIZE) {
            fl::memcpy(mInlineData, str, len + 1);
            mHeapData.reset();
        } else {
            mHeapData = fl::make_shared<StringHolder>(str, len);
            mHeapData->copy(str, len);
            mHeapData->data()[len] = '\0';
        }
    }

    // Assign from another string (copy assignment)
    template <fl::size M>
    StrN& assign(const StrN<M>& str) {
        copy(str);
        return *this;
    }

    // Assign from substring [pos, pos+count) of str
    template <fl::size M>
    StrN& assign(const StrN<M>& str, fl::size pos, fl::size count = npos) {
        if (pos >= str.size()) {
            // Position out of bounds - assign empty string
            clear();
            return *this;
        }

        // Calculate actual count to assign
        fl::size actualCount = count;
        if (actualCount == npos || pos + actualCount > str.size()) {
            actualCount = str.size() - pos;
        }

        // Use existing copy logic
        copy(str.c_str() + pos, actualCount);
        return *this;
    }

    // Assign count copies of character c
    StrN& assign(fl::size count, char c) {
        if (count == 0) {
            clear();
            return *this;
        }

        mLength = count;

        // Check if result fits in inline buffer
        if (count + 1 <= SIZE) {
            for (fl::size i = 0; i < count; ++i) {
                mInlineData[i] = c;
            }
            mInlineData[count] = '\0';
            mHeapData.reset();
        } else {
            // Need heap allocation
            mHeapData = fl::make_shared<StringHolder>(count);
            if (mHeapData) {
                for (fl::size i = 0; i < count; ++i) {
                    mHeapData->data()[i] = c;
                }
                mHeapData->data()[count] = '\0';
            }
        }
        return *this;
    }

    // Assign from moved string (move assignment via assign)
    // This provides an alternative to operator=(StrN&&) for explicit move semantics
    StrN& assign(StrN&& str) noexcept {
        // Delegate to move assignment operator
        *this = fl::move(str);
        return *this;
    }

    // Assign from iterator range [first, last)
    // This enables construction from any iterator pair
    template <typename InputIt>
    StrN& assign(InputIt first, InputIt last) {
        // Clear existing content
        clear();

        // Calculate length by iterating
        fl::size len = 0;
        for (auto it = first; it != last; ++it) {
            ++len;
        }

        if (len == 0) {
            return *this;
        }

        // Reserve space
        mLength = len;

        // Check if result fits in inline buffer
        if (len + 1 <= SIZE) {
            fl::size i = 0;
            for (auto it = first; it != last; ++it, ++i) {
                mInlineData[i] = *it;
            }
            mInlineData[len] = '\0';
            mHeapData.reset();
        } else {
            // Need heap allocation
            mHeapData = fl::make_shared<StringHolder>(len);
            if (mHeapData) {
                fl::size i = 0;
                for (auto it = first; it != last; ++it, ++i) {
                    mHeapData->data()[i] = *it;
                }
                mHeapData->data()[len] = '\0';
            }
        }
        return *this;
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
            fl::memcpy(mInlineData, str, len);  // Copy only len characters, not len+1
            mInlineData[len] = '\0';        // Add null terminator manually
            mHeapData.reset();
        } else {
            mHeapData = fl::make_shared<StringHolder>(str, len);
        }
    }

    template <fl::size M> void copy(const StrN<M> &other) {
        fl::size len = other.size();
        if (len + 1 <= SIZE) {
            fl::memcpy(mInlineData, other.c_str(), len + 1);
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
                fl::size grow_length = FL_MAX(3, newLen * 3 / 2);
                mHeapData->grow(grow_length); // Grow by 50%
            }
            fl::memcpy(mHeapData->data() + mLength, str, n);
            mLength = newLen;
            mHeapData->data()[mLength] = '\0';
            return mLength;
        }
        if (newLen + 1 <= SIZE) {
            fl::memcpy(mInlineData + mLength, str, n);
            mLength = newLen;
            mInlineData[mLength] = '\0';
            return mLength;
        }
        mHeapData.reset();
        StringHolderPtr newData = fl::make_shared<StringHolder>(newLen);
        if (newData) {
            fl::memcpy(newData->data(), c_str(), mLength);
            fl::memcpy(newData->data() + mLength, str, n);
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

    // Bounds-checked element access (std::string compatibility)
    // Unlike operator[], at() provides more explicit error handling for out-of-bounds access
    char& at(fl::size pos) {
        if (pos >= mLength) {
            // Out of bounds access
            // In std::string, this would throw std::out_of_range
            // For fl::string (embedded-friendly), we return a dummy reference
            // Could also use FL_WARN here for debug builds
            static char dummy = '\0';
            return dummy;
        }
        return c_str_mutable()[pos];
    }

    const char& at(fl::size pos) const {
        if (pos >= mLength) {
            // Out of bounds access
            // In std::string, this would throw std::out_of_range
            // For fl::string (embedded-friendly), we return a dummy reference
            static char dummy = '\0';
            return dummy;
        }
        return c_str()[pos];
    }

    bool empty() const { return mLength == 0; }

    // Iterator support for range-based for loops
    char* begin() { return c_str_mutable(); }
    char* end() { return c_str_mutable() + mLength; }
    const char* begin() const { return c_str(); }
    const char* end() const { return c_str() + mLength; }
    const char* cbegin() const { return c_str(); }
    const char* cend() const { return c_str() + mLength; }

    // Reverse iterator support (std::string compatibility)
    // Note: These return raw pointers pointing to elements in reverse order
    // rbegin() points to the last element, rend() points to one-before-first
    // Use with caution: decrement to move forward in reverse iteration
    char* rbegin() {
        return empty() ? nullptr : (c_str_mutable() + mLength - 1);
    }
    char* rend() {
        return empty() ? nullptr : (c_str_mutable() - 1);
    }
    const char* rbegin() const {
        return empty() ? nullptr : (c_str() + mLength - 1);
    }
    const char* rend() const {
        return empty() ? nullptr : (c_str() - 1);
    }
    const char* crbegin() const {
        return rbegin();
    }
    const char* crend() const {
        return rend();
    }

    // Comparison operators (std::string compatibility)
    // Provide lexicographical comparison for StrN instances
    // These work with StrN instances of the same or different template sizes

    bool operator<(const StrN &other) const {
        return strcmp(c_str(), other.c_str()) < 0;
    }

    template <fl::size M> bool operator<(const StrN<M> &other) const {
        return strcmp(c_str(), other.c_str()) < 0;
    }

    bool operator>(const StrN &other) const {
        return strcmp(c_str(), other.c_str()) > 0;
    }

    template <fl::size M> bool operator>(const StrN<M> &other) const {
        return strcmp(c_str(), other.c_str()) > 0;
    }

    bool operator<=(const StrN &other) const {
        return strcmp(c_str(), other.c_str()) <= 0;
    }

    template <fl::size M> bool operator<=(const StrN<M> &other) const {
        return strcmp(c_str(), other.c_str()) <= 0;
    }

    bool operator>=(const StrN &other) const {
        return strcmp(c_str(), other.c_str()) >= 0;
    }

    template <fl::size M> bool operator>=(const StrN<M> &other) const {
        return strcmp(c_str(), other.c_str()) >= 0;
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
            fl::memcpy(newData->data(), c_str(), mLength);
            newData->data()[mLength] = '\0';
            mHeapData = newData;
        }
    }

    void clear(bool freeMemory = false) {
        mLength = 0;
        // Ensure null termination for c_str()
        c_str_mutable()[0] = '\0';
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
        const char* found = fl::strstr(begin, substr);
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
        const char* found = fl::strstr(begin, substr);
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

    // Reverse find operations (std::string compatibility)
    // Find last occurrence of character c starting at or before pos
    fl::size rfind(char c, fl::size pos = npos) const {
        if (mLength == 0) {
            return npos;
        }
        // If pos is npos or beyond end, start from last character
        fl::size searchPos = (pos >= mLength || pos == npos) ? (mLength - 1) : pos;

        const char* str = c_str();
        // Search backwards from searchPos
        for (fl::size i = searchPos + 1; i > 0; --i) {
            if (str[i - 1] == c) {
                return i - 1;
            }
        }
        return npos;
    }

    // Find last occurrence of substring starting at or before pos
    fl::size rfind(const char* s, fl::size pos, fl::size count) const {
        if (!s || count == 0) {
            // Empty string matches at pos (or end if pos > mLength)
            if (count == 0) {
                return (pos > mLength) ? mLength : pos;
            }
            return npos;
        }
        if (count > mLength) {
            return npos;
        }

        // Calculate the maximum starting position for a match
        // The match must start at a position where start + count <= mLength
        // And the match must start at or before position pos
        fl::size maxStart = mLength - count;  // Last possible starting position
        fl::size searchStart = (pos >= mLength || pos == npos) ? maxStart : pos;

        // The match at position searchStart must end at searchStart + count - 1
        // So if searchStart + count > mLength, we need to reduce searchStart
        if (searchStart + count > mLength) {
            searchStart = maxStart;
        }

        const char* str = c_str();
        // Search backwards from searchStart
        for (fl::size i = searchStart + 1; i > 0; --i) {
            fl::size idx = i - 1;
            if (idx + count > mLength) {
                continue;
            }
            bool match = true;
            for (fl::size j = 0; j < count; ++j) {
                if (str[idx + j] != s[j]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return idx;
            }
        }
        return npos;
    }

    // Find last occurrence of null-terminated string starting at or before pos
    fl::size rfind(const char* s, fl::size pos = npos) const {
        if (!s) {
            return npos;
        }
        return rfind(s, pos, strlen(s));
    }

    // Find last occurrence of another string starting at or before pos
    template<fl::size M>
    fl::size rfind(const StrN<M>& str, fl::size pos = npos) const {
        return rfind(str.c_str(), pos, str.size());
    }

    // Find first of operations (std::string compatibility)
    // Find first occurrence of any character from the set starting at pos
    fl::size find_first_of(char c, fl::size pos = 0) const {
        // Single character: delegate to find()
        return find(c, pos);
    }

    // Find first occurrence of any character from the set [s, s+count) starting at pos
    fl::size find_first_of(const char* s, fl::size pos, fl::size count) const {
        if (!s || count == 0) {
            return npos;
        }
        if (pos >= mLength) {
            return npos;
        }

        const char* str = c_str();
        // Search forward from pos
        for (fl::size i = pos; i < mLength; ++i) {
            // Check if character at position i matches any character in the set
            for (fl::size j = 0; j < count; ++j) {
                if (str[i] == s[j]) {
                    return i;
                }
            }
        }
        return npos;
    }

    // Find first occurrence of any character from null-terminated string s starting at pos
    fl::size find_first_of(const char* s, fl::size pos = 0) const {
        if (!s) {
            return npos;
        }
        return find_first_of(s, pos, strlen(s));
    }

    // Find first occurrence of any character from string str starting at pos
    template<fl::size M>
    fl::size find_first_of(const StrN<M>& str, fl::size pos = 0) const {
        return find_first_of(str.c_str(), pos, str.size());
    }

    // Find last of operations (std::string compatibility)
    // Find last occurrence of any character from the set starting at or before pos
    fl::size find_last_of(char c, fl::size pos = npos) const {
        // Single character: delegate to rfind()
        return rfind(c, pos);
    }

    // Find last occurrence of any character from the set [s, s+count) starting at or before pos
    fl::size find_last_of(const char* s, fl::size pos, fl::size count) const {
        if (!s || count == 0) {
            return npos;
        }
        if (mLength == 0) {
            return npos;
        }

        // If pos is npos or beyond end, start from last character
        fl::size searchPos = (pos >= mLength || pos == npos) ? (mLength - 1) : pos;

        const char* str = c_str();
        // Search backward from searchPos
        for (fl::size i = searchPos + 1; i > 0; --i) {
            // Check if character at position i-1 matches any character in the set
            for (fl::size j = 0; j < count; ++j) {
                if (str[i - 1] == s[j]) {
                    return i - 1;
                }
            }
        }
        return npos;
    }

    // Find last occurrence of any character from null-terminated string s starting at or before pos
    fl::size find_last_of(const char* s, fl::size pos = npos) const {
        if (!s) {
            return npos;
        }
        return find_last_of(s, pos, strlen(s));
    }

    // Find last occurrence of any character from string str starting at or before pos
    template<fl::size M>
    fl::size find_last_of(const StrN<M>& str, fl::size pos = npos) const {
        return find_last_of(str.c_str(), pos, str.size());
    }

    // Find first not of operations (std::string compatibility)
    // Find first character NOT matching c starting at pos
    fl::size find_first_not_of(char c, fl::size pos = 0) const {
        if (pos >= mLength) {
            return npos;
        }

        const char* str = c_str();
        // Search forward from pos for first character that doesn't match c
        for (fl::size i = pos; i < mLength; ++i) {
            if (str[i] != c) {
                return i;
            }
        }
        return npos;
    }

    // Find first character NOT in the set [s, s+count) starting at pos
    fl::size find_first_not_of(const char* s, fl::size pos, fl::size count) const {
        if (!s) {
            // Null pointer: every character is "not in the set"
            return (pos < mLength) ? pos : npos;
        }
        if (count == 0) {
            // Empty set: every character is "not in the set"
            return (pos < mLength) ? pos : npos;
        }
        if (pos >= mLength) {
            return npos;
        }

        const char* str = c_str();
        // Search forward from pos
        for (fl::size i = pos; i < mLength; ++i) {
            bool found_in_set = false;
            // Check if character at position i matches any character in the set
            for (fl::size j = 0; j < count; ++j) {
                if (str[i] == s[j]) {
                    found_in_set = true;
                    break;
                }
            }
            // If character is NOT in the set, return its position
            if (!found_in_set) {
                return i;
            }
        }
        return npos;
    }

    // Find first character NOT in null-terminated string s starting at pos
    fl::size find_first_not_of(const char* s, fl::size pos = 0) const {
        if (!s) {
            // Null pointer: every character is "not in the set"
            return (pos < mLength) ? pos : npos;
        }
        return find_first_not_of(s, pos, strlen(s));
    }

    // Find first character NOT in string str starting at pos
    template<fl::size M>
    fl::size find_first_not_of(const StrN<M>& str, fl::size pos = 0) const {
        return find_first_not_of(str.c_str(), pos, str.size());
    }

    // Find last not of operations (std::string compatibility)
    // Find last character NOT matching c starting at or before pos
    fl::size find_last_not_of(char c, fl::size pos = npos) const {
        if (mLength == 0) {
            return npos;
        }

        // If pos is npos or beyond end, start from last character
        fl::size searchPos = (pos >= mLength || pos == npos) ? (mLength - 1) : pos;

        const char* str = c_str();
        // Search backward from searchPos
        for (fl::size i = searchPos + 1; i > 0; --i) {
            if (str[i - 1] != c) {
                return i - 1;
            }
        }
        return npos;
    }

    // Find last character NOT in the set [s, s+count) starting at or before pos
    fl::size find_last_not_of(const char* s, fl::size pos, fl::size count) const {
        if (!s) {
            // Null pointer: every character is "not in the set"
            if (mLength == 0) {
                return npos;
            }
            fl::size searchPos = (pos >= mLength || pos == npos) ? (mLength - 1) : pos;
            return searchPos;
        }
        if (count == 0) {
            // Empty set: every character is "not in the set"
            if (mLength == 0) {
                return npos;
            }
            fl::size searchPos = (pos >= mLength || pos == npos) ? (mLength - 1) : pos;
            return searchPos;
        }
        if (mLength == 0) {
            return npos;
        }

        // If pos is npos or beyond end, start from last character
        fl::size searchPos = (pos >= mLength || pos == npos) ? (mLength - 1) : pos;

        const char* str = c_str();
        // Search backward from searchPos
        for (fl::size i = searchPos + 1; i > 0; --i) {
            bool found_in_set = false;
            // Check if character at position i-1 matches any character in the set
            for (fl::size j = 0; j < count; ++j) {
                if (str[i - 1] == s[j]) {
                    found_in_set = true;
                    break;
                }
            }
            // If character is NOT in the set, return its position
            if (!found_in_set) {
                return i - 1;
            }
        }
        return npos;
    }

    // Find last character NOT in null-terminated string s starting at or before pos
    fl::size find_last_not_of(const char* s, fl::size pos = npos) const {
        if (!s) {
            // Null pointer: every character is "not in the set"
            if (mLength == 0) {
                return npos;
            }
            fl::size searchPos = (pos >= mLength || pos == npos) ? (mLength - 1) : pos;
            return searchPos;
        }
        return find_last_not_of(s, pos, strlen(s));
    }

    // Find last character NOT in string str starting at or before pos
    template<fl::size M>
    fl::size find_last_not_of(const StrN<M>& str, fl::size pos = npos) const {
        return find_last_not_of(str.c_str(), pos, str.size());
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
        return fl::strncmp(c_str(), prefix, prefix_len) == 0;
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
        return fl::strncmp(c_str() + mLength - suffix_len, suffix, suffix_len) == 0;
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

    // Insert operations (std::string compatibility)
    // Insert count copies of character ch at position pos
    StrN& insert(fl::size pos, fl::size count, char ch) {
        if (pos > mLength) {
            pos = mLength;  // Clamp to valid range
        }
        if (count == 0) {
            return *this;
        }

        fl::size newLen = mLength + count;

        // Handle COW: if shared, make a copy
        if (mHeapData && mHeapData.use_count() > 1) {
            StringHolderPtr newData = fl::make_shared<StringHolder>(newLen);
            if (newData) {
                // Copy data before insertion point
                if (pos > 0) {
                    fl::memcpy(newData->data(), mHeapData->data(), pos);
                }
                // Insert new characters
                for (fl::size i = 0; i < count; ++i) {
                    newData->data()[pos + i] = ch;
                }
                // Copy data after insertion point
                if (pos < mLength) {
                    fl::memcpy(newData->data() + pos + count, mHeapData->data() + pos, mLength - pos);
                }
                newData->data()[newLen] = '\0';
                mHeapData = newData;
                mLength = newLen;
            }
            return *this;
        }

        // Check if result fits in inline buffer
        if (newLen + 1 <= SIZE && !mHeapData) {
            // Shift existing data right
            if (pos < mLength) {
                fl::memmove(mInlineData + pos + count, mInlineData + pos, mLength - pos);
            }
            // Insert new characters
            for (fl::size i = 0; i < count; ++i) {
                mInlineData[pos + i] = ch;
            }
            mLength = newLen;
            mInlineData[mLength] = '\0';
            return *this;
        }

        // Need heap allocation or have unshared heap
        if (mHeapData && mHeapData.use_count() <= 1 && mHeapData->hasCapacity(newLen)) {
            // Can insert in place
            char* data = mHeapData->data();
            if (pos < mLength) {
                fl::memmove(data + pos + count, data + pos, mLength - pos);
            }
            for (fl::size i = 0; i < count; ++i) {
                data[pos + i] = ch;
            }
            mLength = newLen;
            data[mLength] = '\0';
        } else {
            // Need new allocation
            StringHolderPtr newData = fl::make_shared<StringHolder>(newLen);
            if (newData) {
                const char* src = c_str();
                // Copy data before insertion point
                if (pos > 0) {
                    fl::memcpy(newData->data(), src, pos);
                }
                // Insert new characters
                for (fl::size i = 0; i < count; ++i) {
                    newData->data()[pos + i] = ch;
                }
                // Copy data after insertion point
                if (pos < mLength) {
                    fl::memcpy(newData->data() + pos + count, src + pos, mLength - pos);
                }
                newData->data()[newLen] = '\0';
                mHeapData = newData;
                mLength = newLen;
            }
        }
        return *this;
    }

    // Insert null-terminated string at position pos
    StrN& insert(fl::size pos, const char* s) {
        if (!s) {
            return *this;
        }
        return insert(pos, s, strlen(s));
    }

    // Insert first count characters of s at position pos
    StrN& insert(fl::size pos, const char* s, fl::size count) {
        if (!s || count == 0) {
            return *this;
        }
        if (pos > mLength) {
            pos = mLength;
        }

        fl::size newLen = mLength + count;

        // Handle COW: if shared, make a copy
        if (mHeapData && mHeapData.use_count() > 1) {
            StringHolderPtr newData = fl::make_shared<StringHolder>(newLen);
            if (newData) {
                if (pos > 0) {
                    fl::memcpy(newData->data(), mHeapData->data(), pos);
                }
                fl::memcpy(newData->data() + pos, s, count);
                if (pos < mLength) {
                    fl::memcpy(newData->data() + pos + count, mHeapData->data() + pos, mLength - pos);
                }
                newData->data()[newLen] = '\0';
                mHeapData = newData;
                mLength = newLen;
            }
            return *this;
        }

        // Check if result fits in inline buffer
        if (newLen + 1 <= SIZE && !mHeapData) {
            if (pos < mLength) {
                fl::memmove(mInlineData + pos + count, mInlineData + pos, mLength - pos);
            }
            fl::memcpy(mInlineData + pos, s, count);
            mLength = newLen;
            mInlineData[mLength] = '\0';
            return *this;
        }

        // Need heap allocation or have unshared heap
        if (mHeapData && mHeapData.use_count() <= 1 && mHeapData->hasCapacity(newLen)) {
            char* data = mHeapData->data();
            if (pos < mLength) {
                fl::memmove(data + pos + count, data + pos, mLength - pos);
            }
            fl::memcpy(data + pos, s, count);
            mLength = newLen;
            data[mLength] = '\0';
        } else {
            StringHolderPtr newData = fl::make_shared<StringHolder>(newLen);
            if (newData) {
                const char* src = c_str();
                if (pos > 0) {
                    fl::memcpy(newData->data(), src, pos);
                }
                fl::memcpy(newData->data() + pos, s, count);
                if (pos < mLength) {
                    fl::memcpy(newData->data() + pos + count, src + pos, mLength - pos);
                }
                newData->data()[newLen] = '\0';
                mHeapData = newData;
                mLength = newLen;
            }
        }
        return *this;
    }

    // Insert string str at position pos
    template <fl::size M>
    StrN& insert(fl::size pos, const StrN<M>& str) {
        return insert(pos, str.c_str(), str.size());
    }

    // Insert substring of str at position pos
    template <fl::size M>
    StrN& insert(fl::size pos, const StrN<M>& str, fl::size pos2, fl::size count = npos) {
        if (pos2 >= str.size()) {
            return *this;
        }
        fl::size actualCount = count;
        if (actualCount == npos || pos2 + actualCount > str.size()) {
            actualCount = str.size() - pos2;
        }
        return insert(pos, str.c_str() + pos2, actualCount);
    }

    // Iterator-based insert operations
    // Note: Disabled for now to avoid ambiguity with index-based insert
    // These can be added later with better type disambiguation
    // char* insert(char* it_pos, char ch);
    // char* insert(char* it_pos, fl::size count, char ch);

    // Erase operations (std::string compatibility)
    // Erase count characters starting at pos (default: erase everything from pos to end)
    StrN& erase(fl::size pos = 0, fl::size count = npos) {
        if (pos >= mLength) {
            // Position out of bounds - no-op
            return *this;
        }

        // Calculate actual number of characters to erase
        fl::size actualCount = count;
        if (actualCount == npos || pos + actualCount > mLength) {
            actualCount = mLength - pos;
        }

        if (actualCount == 0) {
            return *this;
        }

        // Handle COW: if shared, make a copy first
        if (mHeapData && mHeapData.use_count() > 1) {
            StringHolderPtr newData = fl::make_shared<StringHolder>(mLength - actualCount);
            if (newData) {
                // Copy data before erase point
                if (pos > 0) {
                    fl::memcpy(newData->data(), mHeapData->data(), pos);
                }
                // Copy data after erase range
                fl::size remainingLen = mLength - pos - actualCount;
                if (remainingLen > 0) {
                    fl::memcpy(newData->data() + pos, mHeapData->data() + pos + actualCount, remainingLen);
                }
                mLength = mLength - actualCount;
                newData->data()[mLength] = '\0';
                mHeapData = newData;
            }
            return *this;
        }

        // Shift data left to fill gap
        fl::size remainingLen = mLength - pos - actualCount;
        if (remainingLen > 0) {
            char* data = c_str_mutable();
            fl::memmove(data + pos, data + pos + actualCount, remainingLen);
        }

        mLength -= actualCount;
        c_str_mutable()[mLength] = '\0';

        return *this;
    }

    // Iterator-based erase operations
    // Note: These are commented out to avoid ambiguity with index-based erase
    // The issue is that erase(0) is ambiguous: 0 can be fl::size or char* (nullptr)
    // These can be added later with better type disambiguation (SFINAE/enable_if)

    /*
    // Erase character at iterator position
    // Returns iterator to the character following the erased character
    char* erase(char* it_pos) {
        if (!it_pos) {
            return end();
        }

        const char* str_begin = c_str();
        const char* str_end = str_begin + mLength;

        // Check if iterator is within valid range
        if (it_pos < str_begin || it_pos >= str_end) {
            return end();
        }

        fl::size pos = it_pos - str_begin;
        erase(pos, 1);

        // Return iterator to next element (which is now at the same position)
        return begin() + pos;
    }

    // Erase range [first, last)
    // Returns iterator to the character following the last erased character
    char* erase(char* first, char* last) {
        if (!first || !last || first >= last) {
            return end();
        }

        const char* str_begin = c_str();
        const char* str_end = str_begin + mLength;

        // Clamp iterators to valid range
        if (first < str_begin) {
            first = begin();
        }
        if (last > str_end) {
            last = end();
        }
        if (first >= str_end) {
            return end();
        }

        fl::size pos = first - str_begin;
        fl::size count = last - first;

        erase(pos, count);

        // Return iterator to next element
        return begin() + pos;
    }
    */

    // Replace operations (std::string compatibility)
    // Replace count characters starting at pos with string str
    template <fl::size M>
    StrN& replace(fl::size pos, fl::size count, const StrN<M>& str) {
        return replace(pos, count, str.c_str(), str.size());
    }

    // Replace count characters starting at pos with substring of str
    template <fl::size M>
    StrN& replace(fl::size pos, fl::size count, const StrN<M>& str,
                  fl::size pos2, fl::size count2 = npos) {
        if (pos2 >= str.size()) {
            return erase(pos, count);
        }
        fl::size actualCount2 = count2;
        if (actualCount2 == npos || pos2 + actualCount2 > str.size()) {
            actualCount2 = str.size() - pos2;
        }
        return replace(pos, count, str.c_str() + pos2, actualCount2);
    }

    // Replace count characters starting at pos with first count2 chars of s
    StrN& replace(fl::size pos, fl::size count, const char* s, fl::size count2) {
        if (pos > mLength) {
            // Position out of bounds - no-op
            return *this;
        }
        if (!s) {
            return erase(pos, count);
        }

        // Calculate actual number of characters to replace
        fl::size actualCount = count;
        if (actualCount == npos || pos + actualCount > mLength) {
            actualCount = mLength - pos;
        }

        // Calculate final length
        fl::size newLen = mLength - actualCount + count2;

        // Handle COW: if shared, make a copy with new size
        if (mHeapData && mHeapData.use_count() > 1) {
            StringHolderPtr newData = fl::make_shared<StringHolder>(newLen);
            if (newData) {
                // Copy data before replacement point
                if (pos > 0) {
                    fl::memcpy(newData->data(), mHeapData->data(), pos);
                }
                // Copy replacement data
                fl::memcpy(newData->data() + pos, s, count2);
                // Copy data after replacement range
                fl::size remainingLen = mLength - pos - actualCount;
                if (remainingLen > 0) {
                    fl::memcpy(newData->data() + pos + count2,
                           mHeapData->data() + pos + actualCount,
                           remainingLen);
                }
                newData->data()[newLen] = '\0';
                mHeapData = newData;
                mLength = newLen;
            }
            return *this;
        }

        // Check if result fits in inline buffer
        if (newLen + 1 <= SIZE && !mHeapData) {
            // Shift data if necessary
            if (count2 != actualCount) {
                fl::size remainingLen = mLength - pos - actualCount;
                if (remainingLen > 0) {
                    fl::memmove(mInlineData + pos + count2,
                            mInlineData + pos + actualCount,
                            remainingLen);
                }
            }
            // Copy replacement data
            fl::memcpy(mInlineData + pos, s, count2);
            mLength = newLen;
            mInlineData[mLength] = '\0';
            return *this;
        }

        // Need heap allocation or have unshared heap
        if (mHeapData && mHeapData.use_count() <= 1 && mHeapData->hasCapacity(newLen)) {
            // Can replace in place
            char* data = mHeapData->data();
            if (count2 != actualCount) {
                fl::size remainingLen = mLength - pos - actualCount;
                if (remainingLen > 0) {
                    fl::memmove(data + pos + count2,
                            data + pos + actualCount,
                            remainingLen);
                }
            }
            fl::memcpy(data + pos, s, count2);
            mLength = newLen;
            data[mLength] = '\0';
        } else {
            // Need new allocation
            StringHolderPtr newData = fl::make_shared<StringHolder>(newLen);
            if (newData) {
                const char* src = c_str();
                // Copy data before replacement point
                if (pos > 0) {
                    fl::memcpy(newData->data(), src, pos);
                }
                // Copy replacement data
                fl::memcpy(newData->data() + pos, s, count2);
                // Copy data after replacement range
                fl::size remainingLen = mLength - pos - actualCount;
                if (remainingLen > 0) {
                    fl::memcpy(newData->data() + pos + count2,
                           src + pos + actualCount,
                           remainingLen);
                }
                newData->data()[newLen] = '\0';
                mHeapData = newData;
                mLength = newLen;
            }
        }
        return *this;
    }

    // Replace count characters starting at pos with null-terminated s
    StrN& replace(fl::size pos, fl::size count, const char* s) {
        if (!s) {
            return erase(pos, count);
        }
        return replace(pos, count, s, strlen(s));
    }

    // Replace count characters starting at pos with count2 copies of ch
    StrN& replace(fl::size pos, fl::size count, fl::size count2, char ch) {
        if (pos > mLength) {
            return *this;
        }

        // Calculate actual number of characters to replace
        fl::size actualCount = count;
        if (actualCount == npos || pos + actualCount > mLength) {
            actualCount = mLength - pos;
        }

        // Calculate final length
        fl::size newLen = mLength - actualCount + count2;

        // Handle COW: if shared, make a copy with new size
        if (mHeapData && mHeapData.use_count() > 1) {
            StringHolderPtr newData = fl::make_shared<StringHolder>(newLen);
            if (newData) {
                // Copy data before replacement point
                if (pos > 0) {
                    fl::memcpy(newData->data(), mHeapData->data(), pos);
                }
                // Fill with replacement character
                for (fl::size i = 0; i < count2; ++i) {
                    newData->data()[pos + i] = ch;
                }
                // Copy data after replacement range
                fl::size remainingLen = mLength - pos - actualCount;
                if (remainingLen > 0) {
                    fl::memcpy(newData->data() + pos + count2,
                           mHeapData->data() + pos + actualCount,
                           remainingLen);
                }
                newData->data()[newLen] = '\0';
                mHeapData = newData;
                mLength = newLen;
            }
            return *this;
        }

        // Check if result fits in inline buffer
        if (newLen + 1 <= SIZE && !mHeapData) {
            // Shift data if necessary
            if (count2 != actualCount) {
                fl::size remainingLen = mLength - pos - actualCount;
                if (remainingLen > 0) {
                    fl::memmove(mInlineData + pos + count2,
                            mInlineData + pos + actualCount,
                            remainingLen);
                }
            }
            // Fill with replacement character
            for (fl::size i = 0; i < count2; ++i) {
                mInlineData[pos + i] = ch;
            }
            mLength = newLen;
            mInlineData[mLength] = '\0';
            return *this;
        }

        // Need heap allocation or have unshared heap
        if (mHeapData && mHeapData.use_count() <= 1 && mHeapData->hasCapacity(newLen)) {
            // Can replace in place
            char* data = mHeapData->data();
            if (count2 != actualCount) {
                fl::size remainingLen = mLength - pos - actualCount;
                if (remainingLen > 0) {
                    fl::memmove(data + pos + count2,
                            data + pos + actualCount,
                            remainingLen);
                }
            }
            for (fl::size i = 0; i < count2; ++i) {
                data[pos + i] = ch;
            }
            mLength = newLen;
            data[mLength] = '\0';
        } else {
            // Need new allocation
            StringHolderPtr newData = fl::make_shared<StringHolder>(newLen);
            if (newData) {
                const char* src = c_str();
                // Copy data before replacement point
                if (pos > 0) {
                    fl::memcpy(newData->data(), src, pos);
                }
                // Fill with replacement character
                for (fl::size i = 0; i < count2; ++i) {
                    newData->data()[pos + i] = ch;
                }
                // Copy data after replacement range
                fl::size remainingLen = mLength - pos - actualCount;
                if (remainingLen > 0) {
                    fl::memcpy(newData->data() + pos + count2,
                           src + pos + actualCount,
                           remainingLen);
                }
                newData->data()[newLen] = '\0';
                mHeapData = newData;
                mLength = newLen;
            }
        }
        return *this;
    }

    // Iterator-based replace operations
    // Note: Disabled for now to avoid ambiguity issues (same as insert/erase)
    // These can be added later with better type disambiguation
    /*
    // Replace range [first, last) with string str
    template <fl::size M>
    StrN& replace(char* first, char* last, const StrN<M>& str);

    // Replace range [first, last) with first count chars of s
    StrN& replace(char* first, char* last, const char* s, fl::size count);

    // Replace range [first, last) with null-terminated s
    StrN& replace(char* first, char* last, const char* s);

    // Replace range [first, last) with count copies of ch
    StrN& replace(char* first, char* last, fl::size count, char ch);
    */

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

    // Three-way comparison operations (std::string compatibility)
    // Returns <0 if this<other, 0 if equal, >0 if this>other
    // This provides lexicographical comparison like strcmp

    // Compare with another string
    template<fl::size M>
    int compare(const StrN<M>& str) const {
        return strcmp(c_str(), str.c_str());
    }

    // Compare substring [pos1, pos1+count1) with str
    template<fl::size M>
    int compare(fl::size pos1, fl::size count1, const StrN<M>& str) const {
        if (pos1 > mLength) {
            // Position out of bounds - comparing empty string with str
            // Empty string is less than non-empty string
            return str.empty() ? 0 : -1;
        }

        // Calculate actual count to compare from this string
        fl::size actualCount1 = count1;
        if (actualCount1 == npos || pos1 + actualCount1 > mLength) {
            actualCount1 = mLength - pos1;
        }

        // Use strncmp to compare actualCount1 characters from this string with all of str
        fl::size minLen = (actualCount1 < str.size()) ? actualCount1 : str.size();
        int result = fl::strncmp(c_str() + pos1, str.c_str(), minLen);

        if (result != 0) {
            return result;
        }

        // If compared portions are equal, the shorter one is "less than"
        if (actualCount1 < str.size()) {
            return -1;  // this substring is shorter
        } else if (actualCount1 > str.size()) {
            return 1;   // this substring is longer
        }
        return 0;  // Equal
    }

    // Compare substring [pos1, pos1+count1) with substring [pos2, pos2+count2) of str
    template<fl::size M>
    int compare(fl::size pos1, fl::size count1, const StrN<M>& str,
                fl::size pos2, fl::size count2 = npos) const {
        if (pos1 > mLength || pos2 >= str.size()) {
            // Out of bounds - treat as comparing empty substrings
            if (pos1 > mLength && pos2 >= str.size()) {
                return 0;  // Both empty
            } else if (pos1 > mLength) {
                return -1;  // This is empty, other is not
            } else {
                return 1;   // Other is empty, this is not
            }
        }

        // Calculate actual counts
        fl::size actualCount1 = count1;
        if (actualCount1 == npos || pos1 + actualCount1 > mLength) {
            actualCount1 = mLength - pos1;
        }

        fl::size actualCount2 = count2;
        if (actualCount2 == npos || pos2 + actualCount2 > str.size()) {
            actualCount2 = str.size() - pos2;
        }

        // Compare the substrings
        fl::size minLen = (actualCount1 < actualCount2) ? actualCount1 : actualCount2;
        int result = fl::strncmp(c_str() + pos1, str.c_str() + pos2, minLen);

        if (result != 0) {
            return result;
        }

        // If compared portions are equal, the shorter one is "less than"
        if (actualCount1 < actualCount2) {
            return -1;
        } else if (actualCount1 > actualCount2) {
            return 1;
        }
        return 0;
    }

    // Compare with C-string
    int compare(const char* s) const {
        if (!s) {
            // Null pointer: non-empty string is greater than null
            return mLength > 0 ? 1 : 0;
        }
        return strcmp(c_str(), s);
    }

    // Compare substring [pos1, pos1+count1) with C-string s
    int compare(fl::size pos1, fl::size count1, const char* s) const {
        if (!s) {
            // Null pointer comparison
            if (pos1 >= mLength) {
                return 0;  // Both empty
            }
            fl::size actualCount1 = count1;
            if (actualCount1 == npos || pos1 + actualCount1 > mLength) {
                actualCount1 = mLength - pos1;
            }
            return (actualCount1 > 0) ? 1 : 0;
        }

        if (pos1 > mLength) {
            // Position out of bounds - comparing empty with s
            return (s[0] == '\0') ? 0 : -1;
        }

        // Calculate actual count
        fl::size actualCount1 = count1;
        if (actualCount1 == npos || pos1 + actualCount1 > mLength) {
            actualCount1 = mLength - pos1;
        }

        // Compare actualCount1 characters from this string with all of s
        fl::size sLen = strlen(s);
        fl::size minLen = (actualCount1 < sLen) ? actualCount1 : sLen;
        int result = fl::strncmp(c_str() + pos1, s, minLen);

        if (result != 0) {
            return result;
        }

        // If compared portions are equal, the shorter one is "less than"
        if (actualCount1 < sLen) {
            return -1;
        } else if (actualCount1 > sLen) {
            return 1;
        }
        return 0;
    }

    // Compare substring [pos1, pos1+count1) with first count2 characters of C-string s
    int compare(fl::size pos1, fl::size count1, const char* s, fl::size count2) const {
        if (!s) {
            // Null pointer comparison
            if (pos1 >= mLength) {
                return (count2 == 0) ? 0 : -1;
            }
            fl::size actualCount1 = count1;
            if (actualCount1 == npos || pos1 + actualCount1 > mLength) {
                actualCount1 = mLength - pos1;
            }
            return (actualCount1 > 0) ? 1 : ((count2 == 0) ? 0 : -1);
        }

        if (pos1 > mLength) {
            // Position out of bounds - comparing empty with s
            return (count2 == 0) ? 0 : -1;
        }

        // Calculate actual count from this string
        fl::size actualCount1 = count1;
        if (actualCount1 == npos || pos1 + actualCount1 > mLength) {
            actualCount1 = mLength - pos1;
        }

        // Compare the substrings
        fl::size minLen = (actualCount1 < count2) ? actualCount1 : count2;
        int result = fl::strncmp(c_str() + pos1, s, minLen);

        if (result != 0) {
            return result;
        }

        // If compared portions are equal, the shorter one is "less than"
        if (actualCount1 < count2) {
            return -1;
        } else if (actualCount1 > count2) {
            return 1;
        }
        return 0;
    }

    // Copy substring to external buffer (std::string compatibility)
    // Copies substring starting at pos with length count to dest
    // Returns the number of characters copied
    // Note: Unlike c_str(), this does NOT null-terminate the destination buffer
    fl::size copy(char* dest, fl::size count, fl::size pos = 0) const {
        if (!dest) {
            return 0;
        }
        if (pos >= mLength) {
            // Position out of bounds - no characters to copy
            return 0;
        }

        // Calculate actual number of characters to copy
        fl::size actualCount = count;
        if (actualCount > mLength - pos) {
            actualCount = mLength - pos;
        }

        // Copy characters to destination buffer
        if (actualCount > 0) {
            fl::memcpy(dest, c_str() + pos, actualCount);
        }

        return actualCount;
    }

    // Maximum possible string size (std::string compatibility)
    // Returns theoretical maximum size that a string could have
    // This is less than npos to leave room for npos as a sentinel value
    fl::size max_size() const {
        // Return conservative maximum to avoid overflow
        // For 32-bit: ~2GB, for 64-bit: limited by available memory
        return (npos / 2) - 1;
    }

    // Reduce capacity to fit current size (std::string compatibility)
    // This is a non-binding request to reduce memory usage
    // Useful after many erases or when downsizing a string
    void shrink_to_fit() {
        // If using heap data
        if (mHeapData) {
            // Check if we're the sole owner (use_count == 1)
            if (mHeapData.use_count() > 1) {
                // Shared data - can't shrink without affecting others
                return;
            }

            // Check if current capacity is larger than needed
            if (mHeapData->capacity() <= mLength + 1) {
                // Already tight - no need to shrink
                return;
            }

            // Check if string now fits in inline buffer
            if (mLength + 1 <= SIZE) {
                // Copy to inline buffer and release heap
                fl::memcpy(mInlineData, mHeapData->data(), mLength + 1);
                mHeapData.reset();
                return;
            }

            // Reallocate heap to exact size needed
            StringHolderPtr newData = fl::make_shared<StringHolder>(mLength);
            if (newData) {
                fl::memcpy(newData->data(), mHeapData->data(), mLength + 1);
                mHeapData = newData;
            }
        }
        // If using inline buffer, already at minimum capacity
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
    string(const std::string &str) {  // okay std namespace
        copy(str.c_str(), str.size());
    }
    string &operator=(const std::string &str) {  // okay std namespace
        copy(str.c_str(), str.size());
        return *this;
    }
    // append
    string &append(const std::string &str) {  // okay std namespace
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

    // Deprecated: Use append(const CRGB&) instead
    // This version outputs "CRGB(...)" for backwards compatibility
    FL_DEPRECATED("Use append(const CRGB&) instead - outputs 'CRGB(...)' format")
    string &appendCRGB(const CRGB &c);

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
