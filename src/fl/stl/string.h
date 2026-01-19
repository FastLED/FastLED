#pragma once

/// std::string compatible string class.
// fl::string has inlined memory and copy on write semantics.


#include "fl/int.h"
#include "fl/stl/cstring.h"
#include "fl/stl/detail/string_holder.h"
#include "fl/stl/cctype.h"
#include "fl/stl/charconv.h"
#include "fl/stl/not_null.h"

#ifdef __EMSCRIPTEN__
#include <string>
#endif

#include "fl/geometry.h"
#include "fl/math_macros.h"
#include "fl/stl/shared_ptr.h"         // For FASTLED_SHARED_PTR macro
#include "fl/stl/optional.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/vector.h"
#include "fl/stl/span.h"
#include "fl/stl/variant.h"
#include "fl/force_inline.h"
#include "fl/deprecated.h"
#include "fl/compiler_control.h"
#include "fl/string_view.h"

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
template <typename T, typename Allocator> class vector;
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

// Define shared_ptr type for StringHolder (must come after class definition)
// Don't use FASTLED_SHARED_PTR macro as it creates a forward declaration
using StringHolderPtr = fl::shared_ptr<fl::StringHolder>;
using NotNullStringHolderPtr = fl::not_null<StringHolderPtr>;

template <fl::size SIZE = FASTLED_STR_INLINED_SIZE> class StrN {
  protected:
    fl::size mLength = 0;

    // Storage variants: inline buffer, heap pointer, or non-owning references
    struct InlinedBuffer {
        char data[SIZE] = {0};
    };

    // ConstLiteral: Non-owning pointer to constant null-terminated string data.
    // Used for string literals that have static storage duration.
    // The string data must outlive the StrN object.
    struct ConstLiteral {
        const char* data;
        constexpr ConstLiteral() : data(nullptr) {}
        constexpr explicit ConstLiteral(const char* str) : data(str) {}
    };

    // ConstView: Non-owning pointer + length to constant string data.
    // Used for string_view-like references that don't need null termination.
    // The string data must outlive the StrN object.
    struct ConstView {
        const char* data;
        fl::size length;
        constexpr ConstView() : data(nullptr), length(0) {}
        constexpr ConstView(const char* str, fl::size len) : data(str), length(len) {}
    };

    fl::variant<InlinedBuffer, NotNullStringHolderPtr, ConstLiteral, ConstView> mStorage;

  public:
    // Static constants (like std::string)
    static constexpr fl::size npos = static_cast<fl::size>(-1);

    // ======= STATIC FACTORY METHODS FOR NON-OWNING STRINGS =======
    // These create strings that reference constant data without copying.
    // The source data must outlive the string object.
    // When the string is modified, data is automatically copied (copy-on-write).

    // Create a string that references a constant null-terminated string literal.
    // Usage: auto s = StrN<>::from_literal("hello");
    // The literal must have static storage duration (string literal).
    // No memory is allocated until the string is modified.
    static StrN from_literal(const char* literal) {
        StrN result;
        if (literal) {
            result.mLength = fl::strlen(literal);
            result.mStorage = ConstLiteral(literal);
        }
        return result;
    }

    // Create a string that references a constant string view.
    // Usage: auto s = StrN<>::from_view(ptr, len);
    // The source data must outlive the string object.
    // No memory is allocated until the string is modified.
    static StrN from_view(const char* data, fl::size len) {
        StrN result;
        if (data && len > 0) {
            result.mLength = len;
            result.mStorage = ConstView(data, len);
        }
        return result;
    }

    // Create a string that references a string_view without copying.
    // The string_view's data must outlive the string object.
    static StrN from_view(const string_view& sv) {
        return from_view(sv.data(), sv.size());
    }

    // ======= STORAGE TYPE QUERY METHODS =======
    // Check if the string is using constant literal storage (no copy made).
    bool is_literal() const { return mStorage.template is<ConstLiteral>(); }

    // Check if the string is using constant view storage (no copy made).
    bool is_view() const { return mStorage.template is<ConstView>(); }

    // Check if the string owns its data (inline buffer or heap allocated).
    bool is_owning() const {
        return mStorage.template is<InlinedBuffer>() ||
               mStorage.template is<NotNullStringHolderPtr>();
    }

    // Check if the string is referencing external data (literal or view).
    bool is_referencing() const { return is_literal() || is_view(); }

    // ======= CONSTRUCTORS =======
    StrN() : mLength(0), mStorage(InlinedBuffer{}) {}

    // cppcheck-suppress-begin [operatorEqVarError]
    template <fl::size M> StrN(const StrN<M> &other) : mLength(0), mStorage(InlinedBuffer{}) { copy(other); }
    StrN(const char *str) : mLength(0), mStorage(InlinedBuffer{}) {
        fl::size len = fl::strlen(str);
        mLength = len;         // Length is without null terminator
        if (len + 1 <= SIZE) { // Check capacity including null
            fl::memcpy(inlineData(), str, len + 1); // Copy including null
            // mStorage is already InlinedBuffer from initializer
        } else {
            mStorage = NotNullStringHolderPtr(fl::make_shared<StringHolder>(str));
        }
    }
    StrN(const StrN &other) : mLength(0), mStorage(InlinedBuffer{}) { copy(other); }

    // Move constructor (C++11 compatibility)
    // Steals resources from other, leaving it in a valid but empty state
    StrN(StrN&& other) noexcept
        : mLength(other.mLength), mStorage(fl::move(other.mStorage)) {
        // Leave other in a valid empty state
        other.mLength = 0;
        other.mStorage = InlinedBuffer{};
    }

    // Constructor from string_view
    // Copies the string_view data into the string
    StrN(const string_view& sv) : mLength(0), mStorage(InlinedBuffer{}) {
        if (sv.empty()) {
            return;
        }
        fl::size len = sv.size();
        mLength = len;
        if (len + 1 <= SIZE) {
            fl::memcpy(inlineData(), sv.data(), len);
            inlineData()[len] = '\0';
        } else {
            mStorage = NotNullStringHolderPtr(fl::make_shared<StringHolder>(sv.data(), len));
        }
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
            mStorage = fl::move(other.mStorage);

            // Leave other in a valid empty state
            other.mLength = 0;
            other.mStorage = InlinedBuffer{};
        }
        return *this;
    }

    void copy(const char *str) {
        fl::size len = fl::strlen(str);
        mLength = len;
        if (len + 1 <= SIZE) {
            // Ensure we're using inline storage
            if (!mStorage.template is<InlinedBuffer>()) {
                mStorage = InlinedBuffer{};
            }
            fl::memcpy(inlineData(), str, len + 1);
        } else {
            if (hasHeapData() && heapData().get().use_count() <= 1) {
                // We are the sole owners of this data so we can modify it
                heapData()->copy(str, len);
                return;
            }
            mStorage = NotNullStringHolderPtr(fl::make_shared<StringHolder>(str));
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
            // Must set storage BEFORE calling inlineData() to avoid creating temporary empty buffer
            if (!mStorage.template is<InlinedBuffer>()) {
                mStorage = InlinedBuffer{};
            }
            fl::memcpy(inlineData(), str, len + 1);
        } else {
            // StringHolder constructor already copies data and null-terminates
            mStorage = NotNullStringHolderPtr(fl::make_shared<StringHolder>(str, len));
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
            // Must set storage BEFORE calling inlineData()
            if (!mStorage.template is<InlinedBuffer>()) {
                mStorage = InlinedBuffer{};
            }
            char* data = inlineData();  // Cache the pointer
            for (fl::size i = 0; i < count; ++i) {
                data[i] = c;
            }
            data[count] = '\0';
        } else {
            // Need heap allocation
            mStorage = NotNullStringHolderPtr(fl::make_shared<StringHolder>(count));
            NotNullStringHolderPtr& ptr = heapData();
            { // ptr is not_null, always valid
                for (fl::size i = 0; i < count; ++i) {
                    ptr->data()[i] = c;
                }
                ptr->data()[count] = '\0';
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
            // Must set storage BEFORE calling inlineData()
            if (!mStorage.template is<InlinedBuffer>()) {
                mStorage = InlinedBuffer{};
            }
            fl::size i = 0;
            auto* data = inlineData();
            for (auto it = first; it != last; ++it, ++i) {
                data[i] = *it;
            }
            data[len] = '\0';
        } else {
            // Need heap allocation
            mStorage = NotNullStringHolderPtr(fl::make_shared<StringHolder>(len));
            NotNullStringHolderPtr& ptr = heapData();
            { // ptr is not_null, always valid
                fl::size i = 0;
                for (auto it = first; it != last; ++it, ++i) {
                    ptr->data()[i] = *it;
                }
                ptr->data()[len] = '\0';
            }
        }
        return *this;
    }

    bool operator==(const StrN &other) const {
        return fl::strcmp(c_str(), other.c_str()) == 0;
    }

    bool operator!=(const StrN &other) const {
        return fl::strcmp(c_str(), other.c_str()) != 0;
    }

    void copy(const char *str, fl::size len) {
        mLength = len;
        if (len + 1 <= SIZE) {
            // Must set storage BEFORE calling inlineData() to avoid creating temporary empty buffer
            if (!mStorage.template is<InlinedBuffer>()) {
                mStorage = InlinedBuffer{};
            }
            fl::memcpy(inlineData(), str, len);  // Copy only len characters, not len+1
            inlineData()[len] = '\0';        // Add null terminator manually
        } else {
            heapData() = fl::make_shared<StringHolder>(str, len);
        }
    }

    template <fl::size M> void copy(const StrN<M> &other) {
        fl::size len = other.size();
        if (len + 1 <= SIZE) {
            // Ensure we're using inline storage before copying
            if (!mStorage.template is<InlinedBuffer>()) {
                mStorage = InlinedBuffer{};
            }
            fl::memcpy(inlineData(), other.c_str(), len + 1);
        } else {
            if (other.hasHeapData()) {
                heapData() = other.heapData();
            } else {
                heapData() = fl::make_shared<StringHolder>(other.c_str());
            }
        }
        mLength = len;
    }

    fl::size capacity() const {
        if (hasHeapData()) {
            return heapData()->capacity();
        } else if (isNonOwning()) {
            // Non-owning storage has no capacity for modification
            // Return 0 to indicate no capacity available
            return 0;
        }
        return SIZE;
    }

    fl::size write(const fl::u8 *data, fl::size n) {
        const char *str = fl::bit_cast_ptr<const char>(static_cast<const void*>(data));
        return write(str, n);
    }

    fl::size write(const char *str, fl::size n) {
        fl::size newLen = mLength + n;

        // Handle non-owning storage (ConstLiteral or ConstView)
        // Must materialize into owned storage before modification
        if (isNonOwning()) {
            const char* existingData = constData();
            fl::size existingLen = mLength;
            if (newLen + 1 <= SIZE) {
                // Result fits in inline buffer
                InlinedBuffer buf;
                if (existingLen > 0 && existingData) {
                    fl::memcpy(buf.data, existingData, existingLen);
                }
                fl::memcpy(buf.data + existingLen, str, n);
                buf.data[newLen] = '\0';
                mStorage = buf;
                mLength = newLen;
            } else {
                // Need heap allocation
                NotNullStringHolderPtr newData = NotNullStringHolderPtr(fl::make_shared<StringHolder>(newLen));
                if (existingLen > 0 && existingData) {
                    fl::memcpy(newData->data(), existingData, existingLen);
                }
                fl::memcpy(newData->data() + existingLen, str, n);
                newData->data()[newLen] = '\0';
                mStorage = newData;
                mLength = newLen;
            }
            return mLength;
        }

        if (hasHeapData() && heapData().get().use_count() <= 1) {
            NotNullStringHolderPtr& heap = heapData();
            if (!heap->hasCapacity(newLen)) {
                fl::size grow_length = FL_MAX(3, newLen * 3 / 2);
                heap->grow(grow_length); // Grow by 50%
            }
            fl::memcpy(heap->data() + mLength, str, n);
            mLength = newLen;
            heap->data()[mLength] = '\0';
            return mLength;
        } else if (hasHeapData()) {
            // Copy-on-write: shared heap data needs to be copied before modification
            NotNullStringHolderPtr newData = NotNullStringHolderPtr(fl::make_shared<StringHolder>(newLen));
            { // newData is not_null, always valid
                // Copy existing heap data
                const NotNullStringHolderPtr& heap = heapData();
                fl::memcpy(newData->data(), heap->data(), mLength);
                // Append new data
                fl::memcpy(newData->data() + mLength, str, n);
                newData->data()[newLen] = '\0';
                // Replace shared pointer with new unshared copy
                mStorage = newData;
                mLength = newLen;
            }
            return mLength;
        }
        if (newLen + 1 <= SIZE) {
            // GCC -O2 false positive: offset [-83, -19] out of bounds [0, 88]
            // The bounds check above protects this memcpy, but aggressive inlining
            // causes GCC's optimizer to lose track of the invariant (mLength < SIZE).
            // See: https://github.com/FastLED/FastLED/issues/2150
            FL_DISABLE_WARNING_PUSH
            FL_DISABLE_WARNING(array-bounds)
            fl::memcpy(inlineData() + mLength, str, n);
            FL_DISABLE_WARNING_POP
            mLength = newLen;
            inlineData()[mLength] = '\0';
            return mLength;
        }
        // Need to transition from inline to heap storage
        // Copy current inline data to heap buffer
        NotNullStringHolderPtr newData = NotNullStringHolderPtr(fl::make_shared<StringHolder>(newLen));
        { // newData is not_null, always valid
            // Copy existing inline data (we know we're inline at this point)
            fl::memcpy(newData->data(), inlineData(), mLength);
            fl::memcpy(newData->data() + mLength, str, n);
            newData->data()[newLen] = '\0';
            // Now transition storage to heap
            mStorage = newData;
            mLength = newLen;
        }
        return mLength;
    }

    fl::size write(char c) { return write(&c, 1); }

    fl::size write(fl::u8 c) {
        const char *str = fl::bit_cast_ptr<const char>(static_cast<const void*>(&c));
        return write(str, 1);
    }

    fl::size write(const fl::u16 &n) {
        char buf[64] = {0};
        int len = fl::utoa32(static_cast<fl::u32>(n), buf, 10);
        return write(buf, len);
    }

    fl::size write(const fl::u32 &val) {
        char buf[64] = {0};
        int len = fl::utoa32(val, buf, 10);
        return write(buf, len);
    }

    fl::size write(const uint64_t &val) {
        char buf[64] = {0};
        int len = fl::utoa64(val, buf, 10);
        return write(buf, len);
    }

    fl::size write(const int64_t &val) {
        char buf[64] = {0};
        int len = fl::itoa64(val, buf, 10);
        return write(buf, len);
    }

    fl::size write(const fl::i32 &val) {
        char buf[64] = {0};
        int len = fl::itoa(val, buf, 10);
        return write(buf, len);
    }

    fl::size write(const fl::i8 val) {
        char buf[64] = {0};
        int len = fl::itoa(static_cast<fl::i32>(val), buf, 10);
        return write(buf, len);
    }

    // Generic write for multi-byte integer types not covered by explicit overloads
    template <typename T>
    typename fl::enable_if<fl::is_multi_byte_integer<T>::value, fl::size>::type
    write(const T &val) {
        // Cast to appropriate target type based on size/signedness
        using target_t = typename int_cast_detail::cast_target<T>::type;
        char buf[64] = {0};
        int len;
        if (fl::is_signed<target_t>::value) {
            len = fl::itoa(static_cast<fl::i32>(val), buf, 10);
        } else {
            len = fl::utoa32(static_cast<fl::u32>(val), buf, 10);
        }
        return write(buf, len);
    }

    // Destructor
    ~StrN() {}

    // Accessors
    fl::size size() const { return mLength; }
    fl::size length() const { return size(); }
    // c_str() returns a null-terminated string.
    // For ConstView, the data may not be null-terminated at position mLength,
    // so we must materialize to ensure null-termination.
    // For ConstLiteral, the data is guaranteed to be null-terminated.
    const char *c_str() const {
        if (mStorage.template is<ConstView>()) {
            // ConstView data may not be null-terminated - need to check
            const ConstView& view = mStorage.template get<ConstView>();
            if (view.data && view.length > 0 && view.data[view.length] != '\0') {
                // Need to materialize - const_cast is safe here because
                // we're converting to owned storage
                const_cast<StrN*>(this)->materialize();
            }
        }
        return constData();
    }

    char *c_str_mutable() {
        // Must materialize non-owning storage before providing mutable access
        if (isNonOwning()) {
            materialize();
        }
        return hasHeapData() ? heapData()->data() : inlineData();
    }

    char &operator[](fl::size index) {
        if (index >= mLength) {
            static char dummy = '\0';  // okay static in header
            return dummy;
        }
        return c_str_mutable()[index];
    }

    char operator[](fl::size index) const {
        if (index >= mLength) {
            static char dummy = '\0';  // okay static in header
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
            static char dummy = '\0';  // okay static in header
            return dummy;
        }
        return c_str_mutable()[pos];
    }

    const char& at(fl::size pos) const {
        if (pos >= mLength) {
            // Out of bounds access
            // In std::string, this would throw std::out_of_range
            // For fl::string (embedded-friendly), we return a dummy reference
            static char dummy = '\0';  // okay static in header
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
        return fl::strcmp(c_str(), other.c_str()) < 0;
    }

    template <fl::size M> bool operator<(const StrN<M> &other) const {
        return fl::strcmp(c_str(), other.c_str()) < 0;
    }

    bool operator>(const StrN &other) const {
        return fl::strcmp(c_str(), other.c_str()) > 0;
    }

    template <fl::size M> bool operator>(const StrN<M> &other) const {
        return fl::strcmp(c_str(), other.c_str()) > 0;
    }

    bool operator<=(const StrN &other) const {
        return fl::strcmp(c_str(), other.c_str()) <= 0;
    }

    template <fl::size M> bool operator<=(const StrN<M> &other) const {
        return fl::strcmp(c_str(), other.c_str()) <= 0;
    }

    bool operator>=(const StrN &other) const {
        return fl::strcmp(c_str(), other.c_str()) >= 0;
    }

    template <fl::size M> bool operator>=(const StrN<M> &other) const {
        return fl::strcmp(c_str(), other.c_str()) >= 0;
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
        if (hasHeapData()) {
            const NotNullStringHolderPtr& heap = heapData();
            if (heap.get().use_count() <= 1 && heap->hasCapacity(newCapacity)) {
                return;
            }
        }

        // Need to allocate new storage
        NotNullStringHolderPtr newData = NotNullStringHolderPtr(fl::make_shared<StringHolder>(newCapacity));
        { // newData is not_null, always valid
            // Copy existing content
            fl::memcpy(newData->data(), c_str(), mLength);
            newData->data()[mLength] = '\0';
            heapData() = newData;
        }
    }

    void clear(bool freeMemory = false) {
        mLength = 0;
        // For non-owning storage, always transition to owned empty buffer
        if (isNonOwning() || (freeMemory && hasHeapData())) {
            mStorage = InlinedBuffer{};
        } else {
            // Ensure null termination for c_str()
            c_str_mutable()[0] = '\0';
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
        return rfind(s, pos, fl::strlen(s));
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
        return find_first_of(s, pos, fl::strlen(s));
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
        return find_last_of(s, pos, fl::strlen(s));
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
        return find_first_not_of(s, pos, fl::strlen(s));
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
        return find_last_not_of(s, pos, fl::strlen(s));
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
        fl::size prefix_len = fl::strlen(prefix);
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
        fl::size suffix_len = fl::strlen(suffix);
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
        if (hasHeapData() && heapData().get().use_count() > 1) {
            const NotNullStringHolderPtr& heap = heapData();
            NotNullStringHolderPtr newData = NotNullStringHolderPtr(fl::make_shared<StringHolder>(newLen));
            { // newData is not_null, always valid
                // Copy data before insertion point
                if (pos > 0) {
                    fl::memcpy(newData->data(), heap->data(), pos);
                }
                // Insert new characters
                for (fl::size i = 0; i < count; ++i) {
                    newData->data()[pos + i] = ch;
                }
                // Copy data after insertion point
                if (pos < mLength) {
                    fl::memcpy(newData->data() + pos + count, heap->data() + pos, mLength - pos);
                }
                newData->data()[newLen] = '\0';
                heapData() = newData;
                mLength = newLen;
            }
            return *this;
        }

        // Check if result fits in inline buffer
        if (newLen + 1 <= SIZE && !hasHeapData()) {
            char* data = inlineData();  // Cache the pointer
            // Shift existing data right
            if (pos < mLength) {
                fl::memmove(data + pos + count, data + pos, mLength - pos);
            }
            // Insert new characters
            for (fl::size i = 0; i < count; ++i) {
                data[pos + i] = ch;
            }
            mLength = newLen;
            data[mLength] = '\0';
            return *this;
        }

        // Need heap allocation or have unshared heap
        bool canInsertInPlace = hasHeapData();
        if (canInsertInPlace) {
            NotNullStringHolderPtr& heap = heapData();
            canInsertInPlace = heap.get().use_count() <= 1 && heap->hasCapacity(newLen);
            if (canInsertInPlace) {
                // Can insert in place
                char* data = heap->data();
                if (pos < mLength) {
                    fl::memmove(data + pos + count, data + pos, mLength - pos);
                }
                for (fl::size i = 0; i < count; ++i) {
                    data[pos + i] = ch;
                }
                mLength = newLen;
                data[mLength] = '\0';
            }
        }
        if (!canInsertInPlace) {
            // Need new allocation
            NotNullStringHolderPtr newData = NotNullStringHolderPtr(fl::make_shared<StringHolder>(newLen));
            { // newData is not_null, always valid
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
                heapData() = newData;
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
        return insert(pos, s, fl::strlen(s));
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
        if (hasHeapData() && heapData().get().use_count() > 1) {
            const NotNullStringHolderPtr& heap = heapData();
            NotNullStringHolderPtr newData = NotNullStringHolderPtr(fl::make_shared<StringHolder>(newLen));
            { // newData is not_null, always valid
                if (pos > 0) {
                    fl::memcpy(newData->data(), heap->data(), pos);
                }
                fl::memcpy(newData->data() + pos, s, count);
                if (pos < mLength) {
                    fl::memcpy(newData->data() + pos + count, heap->data() + pos, mLength - pos);
                }
                newData->data()[newLen] = '\0';
                heapData() = newData;
                mLength = newLen;
            }
            return *this;
        }

        // Check if result fits in inline buffer
        if (newLen + 1 <= SIZE && !hasHeapData()) {
            char* data = inlineData();  // Cache the pointer
            if (pos < mLength) {
                fl::memmove(data + pos + count, data + pos, mLength - pos);
            }
            fl::memcpy(data + pos, s, count);
            mLength = newLen;
            data[mLength] = '\0';
            return *this;
        }

        // Need heap allocation or have unshared heap
        bool canInsertInPlace = hasHeapData();
        if (canInsertInPlace) {
            NotNullStringHolderPtr& heap = heapData();
            canInsertInPlace = heap.get().use_count() <= 1 && heap->hasCapacity(newLen);
            if (canInsertInPlace) {
                char* data = heap->data();
                if (pos < mLength) {
                    fl::memmove(data + pos + count, data + pos, mLength - pos);
                }
                fl::memcpy(data + pos, s, count);
                mLength = newLen;
                data[mLength] = '\0';
            }
        }
        if (!canInsertInPlace) {
            NotNullStringHolderPtr newData = NotNullStringHolderPtr(fl::make_shared<StringHolder>(newLen));
            { // newData is not_null, always valid
                const char* src = c_str();
                if (pos > 0) {
                    fl::memcpy(newData->data(), src, pos);
                }
                fl::memcpy(newData->data() + pos, s, count);
                if (pos < mLength) {
                    fl::memcpy(newData->data() + pos + count, src + pos, mLength - pos);
                }
                newData->data()[newLen] = '\0';
                heapData() = newData;
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
        if (hasHeapData() && heapData().get().use_count() > 1) {
            const NotNullStringHolderPtr& heap = heapData();
            NotNullStringHolderPtr newData = NotNullStringHolderPtr(fl::make_shared<StringHolder>(mLength - actualCount));
            { // newData is not_null, always valid
                // Copy data before erase point
                if (pos > 0) {
                    fl::memcpy(newData->data(), heap->data(), pos);
                }
                // Copy data after erase range
                fl::size remainingLen = mLength - pos - actualCount;
                if (remainingLen > 0) {
                    fl::memcpy(newData->data() + pos, heap->data() + pos + actualCount, remainingLen);
                }
                mLength = mLength - actualCount;
                newData->data()[mLength] = '\0';
                heapData() = newData;
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
    // Note: Iterator-based erase methods
    // SFINAE disambiguation: Only enable when argument is actually a pointer type
    // This prevents ambiguity with erase(0) which should call the fl::size overload

    // Erase character at iterator position
    // Returns iterator to the character following the erased character
    template<typename T>
    typename fl::enable_if<fl::is_pointer<T>::value && fl::is_same<typename fl::remove_cv<typename fl::remove_pointer<T>::type>::type, char>::value, char*>::type
    erase(T it_pos) {
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
    template<typename T>
    typename fl::enable_if<fl::is_pointer<T>::value && fl::is_same<typename fl::remove_cv<typename fl::remove_pointer<T>::type>::type, char>::value, char*>::type
    erase(T first, T last) {
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
        if (hasHeapData() && heapData().get().use_count() > 1) {
            const NotNullStringHolderPtr& heap = heapData();
            NotNullStringHolderPtr newData = NotNullStringHolderPtr(fl::make_shared<StringHolder>(newLen));
            { // newData is not_null, always valid
                // Copy data before replacement point
                if (pos > 0) {
                    fl::memcpy(newData->data(), heap->data(), pos);
                }
                // Copy replacement data
                fl::memcpy(newData->data() + pos, s, count2);
                // Copy data after replacement range
                fl::size remainingLen = mLength - pos - actualCount;
                if (remainingLen > 0) {
                    fl::memcpy(newData->data() + pos + count2,
                           heap->data() + pos + actualCount,
                           remainingLen);
                }
                newData->data()[newLen] = '\0';
                heapData() = newData;
                mLength = newLen;
            }
            return *this;
        }

        // Check if result fits in inline buffer
        if (newLen + 1 <= SIZE && !hasHeapData()) {
            char* data = inlineData();  // Cache the pointer
            // Shift data if necessary
            if (count2 != actualCount) {
                fl::size remainingLen = mLength - pos - actualCount;
                if (remainingLen > 0) {
                    fl::memmove(data + pos + count2,
                            data + pos + actualCount,
                            remainingLen);
                }
            }
            // Copy replacement data
            fl::memcpy(data + pos, s, count2);
            mLength = newLen;
            data[mLength] = '\0';
            return *this;
        }

        // Need heap allocation or have unshared heap
        bool canReplaceInPlace = hasHeapData();
        if (canReplaceInPlace) {
            NotNullStringHolderPtr& heap = heapData();
            canReplaceInPlace = heap.get().use_count() <= 1 && heap->hasCapacity(newLen);
            if (canReplaceInPlace) {
                // Can replace in place
                char* data = heap->data();
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
            }
        }
        if (!canReplaceInPlace) {
            // Need new allocation
            NotNullStringHolderPtr newData = NotNullStringHolderPtr(fl::make_shared<StringHolder>(newLen));
            { // newData is not_null, always valid
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
                heapData() = newData;
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
        return replace(pos, count, s, fl::strlen(s));
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
        if (hasHeapData() && heapData().get().use_count() > 1) {
            const NotNullStringHolderPtr& heap = heapData();
            NotNullStringHolderPtr newData = NotNullStringHolderPtr(fl::make_shared<StringHolder>(newLen));
            { // newData is not_null, always valid
                // Copy data before replacement point
                if (pos > 0) {
                    fl::memcpy(newData->data(), heap->data(), pos);
                }
                // Fill with replacement character
                for (fl::size i = 0; i < count2; ++i) {
                    newData->data()[pos + i] = ch;
                }
                // Copy data after replacement range
                fl::size remainingLen = mLength - pos - actualCount;
                if (remainingLen > 0) {
                    fl::memcpy(newData->data() + pos + count2,
                           heap->data() + pos + actualCount,
                           remainingLen);
                }
                newData->data()[newLen] = '\0';
                heapData() = newData;
                mLength = newLen;
            }
            return *this;
        }

        // Check if result fits in inline buffer
        if (newLen + 1 <= SIZE && !hasHeapData()) {
            // Shift data if necessary
            char* data = inlineData();  // Cache the pointer
            if (count2 != actualCount) {
                fl::size remainingLen = mLength - pos - actualCount;
                if (remainingLen > 0) {
                    fl::memmove(data + pos + count2,
                            data + pos + actualCount,
                            remainingLen);
                }
            }
            // Fill with replacement character
            for (fl::size i = 0; i < count2; ++i) {
                data[pos + i] = ch;
            }
            mLength = newLen;
            data[mLength] = '\0';
            return *this;
        }

        // Need heap allocation or have unshared heap
        bool canReplaceInPlace = hasHeapData();
        if (canReplaceInPlace) {
            NotNullStringHolderPtr& heap = heapData();
            canReplaceInPlace = heap.get().use_count() <= 1 && heap->hasCapacity(newLen);
            if (canReplaceInPlace) {
                // Can replace in place
                char* data = heap->data();
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
            }
        }
        if (!canReplaceInPlace) {
            // Need new allocation
            NotNullStringHolderPtr newData = NotNullStringHolderPtr(fl::make_shared<StringHolder>(newLen));
            { // newData is not_null, always valid
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
                heapData() = newData;
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
        while (start < mLength && fl::isspace(c_str()[start])) {
            start++;
        }
        while (end > start && fl::isspace(c_str()[end - 1])) {
            end--;
        }
        return substring(start, end);
    }

    float toFloat() const {
        return fl::parseFloat(c_str(), mLength);
    }

    // Three-way comparison operations (std::string compatibility)
    // Returns <0 if this<other, 0 if equal, >0 if this>other
    // This provides lexicographical comparison like strcmp

    // Compare with another string
    template<fl::size M>
    int compare(const StrN<M>& str) const {
        return fl::strcmp(c_str(), str.c_str());
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
        return fl::strcmp(c_str(), s);
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
        fl::size sLen = fl::strlen(s);
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
        if (hasHeapData()) {
            NotNullStringHolderPtr& heap = heapData();
            // Check if we're the sole owner (use_count == 1)
            if (heap.get().use_count() > 1) {
                // Shared data - can't shrink without affecting others
                return;
            }

            // Check if current capacity is larger than needed
            if (heap->capacity() <= mLength + 1) {
                // Already tight - no need to shrink
                return;
            }

            // Check if string now fits in inline buffer
            if (mLength + 1 <= SIZE) {
                // Copy to inline buffer and release heap
                fl::memcpy(inlineData(), heap->data(), mLength + 1);
                mStorage = InlinedBuffer{};
                return;
            }

            // Reallocate heap to exact size needed
            NotNullStringHolderPtr newData = NotNullStringHolderPtr(fl::make_shared<StringHolder>(mLength));
            { // newData is not_null, always valid
                fl::memcpy(newData->data(), heap->data(), mLength + 1);
                heapData() = newData;
            }
        }
        // If using inline buffer, already at minimum capacity
    }

  private:
    // Helper accessors for variant storage
    char* inlineData() {
        // Auto-transition to inline storage if not already there
        // This prevents nullptr returns that cause segfaults
        if (!mStorage.template is<InlinedBuffer>()) {
            mStorage = InlinedBuffer{};
        }
        return mStorage.template get<InlinedBuffer>().data;
    }

    const char* inlineData() const {
        // For const version, we MUST already be holding InlinedBuffer
        // If not, this is a programming error (logic bug in calling code)
        // We return the data directly - no transitions allowed in const methods
        if (!mStorage.template is<InlinedBuffer>()) {
            // This should never happen in correct code
            // Return empty string literal instead of nullptr to avoid immediate crash
            return "";
        }
        return mStorage.template get<InlinedBuffer>().data;
    }

    bool hasHeapData() const {
        return mStorage.template is<NotNullStringHolderPtr>();
    }

    bool hasConstLiteral() const {
        return mStorage.template is<ConstLiteral>();
    }

    bool hasConstView() const {
        return mStorage.template is<ConstView>();
    }

    // Check if storage is non-owning (literal or view)
    bool isNonOwning() const {
        return hasConstLiteral() || hasConstView();
    }

    // Get const data pointer for any storage type
    const char* constData() const {
        if (mStorage.template is<InlinedBuffer>()) {
            return mStorage.template get<InlinedBuffer>().data;
        } else if (mStorage.template is<NotNullStringHolderPtr>()) {
            return mStorage.template get<NotNullStringHolderPtr>()->data();
        } else if (mStorage.template is<ConstLiteral>()) {
            return mStorage.template get<ConstLiteral>().data;
        } else if (mStorage.template is<ConstView>()) {
            return mStorage.template get<ConstView>().data;
        }
        return "";
    }

    // Materialize non-owning storage into owned storage before modification.
    // This implements copy-on-write semantics for literals and views.
    // After calling this, the string will be using either InlinedBuffer or heap storage.
    void materialize() {
        if (mStorage.template is<ConstLiteral>()) {
            const char* data = mStorage.template get<ConstLiteral>().data;
            if (!data) {
                mLength = 0;
                mStorage = InlinedBuffer{};
                return;
            }
            fl::size len = mLength;
            if (len + 1 <= SIZE) {
                InlinedBuffer buf;
                fl::memcpy(buf.data, data, len);
                buf.data[len] = '\0';
                mStorage = buf;
            } else {
                mStorage = NotNullStringHolderPtr(fl::make_shared<StringHolder>(data, len));
            }
        } else if (mStorage.template is<ConstView>()) {
            const ConstView& view = mStorage.template get<ConstView>();
            if (!view.data) {
                mLength = 0;
                mStorage = InlinedBuffer{};
                return;
            }
            fl::size len = view.length;
            mLength = len;
            if (len + 1 <= SIZE) {
                InlinedBuffer buf;
                fl::memcpy(buf.data, view.data, len);
                buf.data[len] = '\0';
                mStorage = buf;
            } else {
                mStorage = NotNullStringHolderPtr(fl::make_shared<StringHolder>(view.data, len));
            }
        }
        // InlinedBuffer and NotNullStringHolderPtr are already owned - no action needed
    }

    NotNullStringHolderPtr& heapData() {
        if (!mStorage.template is<NotNullStringHolderPtr>()) {
            // Create a new heap-allocated StringHolder with empty content
            mStorage = NotNullStringHolderPtr(fl::make_shared<StringHolder>(0));
        }
        return mStorage.template get<NotNullStringHolderPtr>();
    }

    const NotNullStringHolderPtr& heapData() const {
        // For const access, we cannot transition the variant
        // Callers must use hasHeapData() to check first
        // If not holding NotNullStringHolderPtr, this will crash (intentional for fast failure)
        return mStorage.template get<NotNullStringHolderPtr>();
    }
};

class string : public StrN<FASTLED_STR_INLINED_SIZE> {
  public:
    // Standard string npos constant for compatibility
    static constexpr fl::size npos = static_cast<fl::size>(-1);

    // ======= STATIC FACTORY METHODS FOR NON-OWNING STRINGS =======
    // Create a string that references a constant null-terminated string literal.
    // No memory is allocated until the string is modified.
    static string from_literal(const char* literal) {
        string result;
        if (literal) {
            result.mLength = fl::strlen(literal);
            result.mStorage = ConstLiteral(literal);
        }
        return result;
    }

    // Create a string that references a constant string view.
    // No memory is allocated until the string is modified.
    static string from_view(const char* data, fl::size len) {
        string result;
        if (data && len > 0) {
            result.mLength = len;
            result.mStorage = ConstView(data, len);
        }
        return result;
    }

    // Create a string that references a string_view without copying.
    static string from_view(const string_view& sv) {
        return from_view(sv.data(), sv.size());
    }

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
    // Constructor from string_view
    string(const string_view& sv) : StrN<FASTLED_STR_INLINED_SIZE>(sv) {}
    string &operator=(const string &other) {
        copy(other);
        return *this;
    }
    
    string &operator=(const char *str) {
        copy(str, fl::strlen(str));
        return *this;
    }

    // Bring base class assign methods into scope so they're not hidden
    using StrN<FASTLED_STR_INLINED_SIZE>::assign;

    // Assignment from string_view
    // Note: This takes string_view by value to avoid ambiguity with StrN assignment.
    // Passing string_view by value is efficient since it's just a pointer+size.
    string &assign(string_view sv) {
        if (sv.empty()) {
            clear();
        } else {
            copy(sv.data(), sv.size());
        }
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
        return fl::strcmp(c_str(), other.c_str()) > 0;
    }

    bool operator>=(const string &other) const {
        return fl::strcmp(c_str(), other.c_str()) >= 0;
    }

    bool operator<(const string &other) const {
        return fl::strcmp(c_str(), other.c_str()) < 0;
    }

    bool operator<=(const string &other) const {
        return fl::strcmp(c_str(), other.c_str()) <= 0;
    }

    bool operator==(const string &other) const {
        return fl::strcmp(c_str(), other.c_str()) == 0;
    }

    bool operator!=(const string &other) const {
        return fl::strcmp(c_str(), other.c_str()) != 0;
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

    //-------------------------------------------------------------------------
    // Generic integer append using SFINAE and type traits
    //-------------------------------------------------------------------------
    // Handles all multi-byte integer types (excludes char types)
    // Converts to appropriate target type based on size/signedness
    // This is needed because on some platforms type(int) is not one of the standard
    // integral types like i8, i16, i32, int64_t etc.
    template <typename T>
    typename fl::enable_if<fl::is_multi_byte_integer<T>::value, string&>::type
    append(const T &val) {
        // Cast to appropriate type for display (handles platform differences)
        using target_t = typename int_cast_detail::cast_target<T>::type;
        write(static_cast<target_t>(val));
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

    template <typename T> string &append(const fl::vector<T> &vec) {
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
        write(str, fl::strlen(str));
        return *this;
    }
    string &append(const char *str, fl::size len) {
        write(str, len);
        return *this;
    }

    // Explicit overload for char to avoid ambiguity with template overloads
    string &append(char c) {
        write(&c, 1);
        return *this;
    }

    // i8 append - displays as number (-128 to 127), not ASCII character
    // This matches the behavior of write(i8) which formats as number
    string &append(const i8 &val) {
        write(fl::i16(val));  // Promote to i16 for numeric display
        return *this;
    }

    // u8 append - displays as number (0-255), not ASCII character
    // This overload is needed to avoid ambiguity with the generic integer template
    // when appending CRGB members (r, g, b which are u8 types)
    string &append(const u8 &val) {
        write(fl::u16(val));  // Promote to u16 for numeric display
        return *this;
    }

    string &append(const bool &val) {
        if (val) {
            return append("true");
        } else {
            return append("false");
        }
    }

    // 64-bit integer append methods
    string &append(const int64_t &val) {
        char buf[64] = {0};
        int len = fl::itoa64(val, buf, 10);
        write(buf, len);
        return *this;
    }

    string &append(const uint64_t &val) {
        char buf[64] = {0};
        int len = fl::utoa64(val, buf, 10);
        write(buf, len);
        return *this;
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
        char buf[64] = {0};
        fl::ftoa(_val, buf, 2);
        write(buf, fl::strlen(buf));
        return *this;
    }

    string &append(const float &_val, int precision) {
        char buf[64] = {0};
        fl::ftoa(_val, buf, precision);
        write(buf, fl::strlen(buf));
        return *this;
    }

    string &append(const double &val) { return append(float(val)); }

    // Hexadecimal formatting methods
    string &appendHex(i32 val) {
        char buf[64] = {0};
        int len = fl::itoa(val, buf, 16);
        write(buf, len);
        return *this;
    }
    string &appendHex(u32 val) {
        char buf[64] = {0};
        int len = fl::utoa32(val, buf, 16);
        write(buf, len);
        return *this;
    }
    string &appendHex(int64_t val) {
        char buf[64] = {0};
        int len = fl::utoa64(static_cast<uint64_t>(val), buf, 16);
        write(buf, len);
        return *this;
    }
    string &appendHex(uint64_t val) {
        char buf[64] = {0};
        int len = fl::utoa64(val, buf, 16);
        write(buf, len);
        return *this;
    }
    string &appendHex(i16 val) {
        char buf[64] = {0};
        int len = fl::itoa(static_cast<i32>(val), buf, 16);
        write(buf, len);
        return *this;
    }
    string &appendHex(u16 val) {
        char buf[64] = {0};
        int len = fl::utoa32(static_cast<u32>(val), buf, 16);
        write(buf, len);
        return *this;
    }
    string &appendHex(i8 val) {
        char buf[64] = {0};
        int len = fl::itoa(static_cast<i32>(val), buf, 16);
        write(buf, len);
        return *this;
    }
    string &appendHex(u8 val) {
        char buf[64] = {0};
        int len = fl::utoa32(static_cast<u32>(val), buf, 16);
        write(buf, len);
        return *this;
    }

    // Octal formatting methods
    string &appendOct(i32 val) {
        char buf[64] = {0};
        int len = fl::itoa(val, buf, 8);
        write(buf, len);
        return *this;
    }
    string &appendOct(u32 val) {
        char buf[64] = {0};
        int len = fl::utoa32(val, buf, 8);
        write(buf, len);
        return *this;
    }
    string &appendOct(int64_t val) {
        char buf[64] = {0};
        int len = fl::utoa64(static_cast<uint64_t>(val), buf, 8);
        write(buf, len);
        return *this;
    }
    string &appendOct(uint64_t val) {
        char buf[64] = {0};
        int len = fl::utoa64(val, buf, 8);
        write(buf, len);
        return *this;
    }
    string &appendOct(i16 val) {
        char buf[64] = {0};
        int len = fl::itoa(static_cast<i32>(val), buf, 8);
        write(buf, len);
        return *this;
    }
    string &appendOct(u16 val) {
        char buf[64] = {0};
        int len = fl::utoa32(static_cast<u32>(val), buf, 8);
        write(buf, len);
        return *this;
    }
    string &appendOct(i8 val) {
        char buf[64] = {0};
        int len = fl::itoa(static_cast<i32>(val), buf, 8);
        write(buf, len);
        return *this;
    }
    string &appendOct(u8 val) {
        char buf[64] = {0};
        int len = fl::utoa32(static_cast<u32>(val), buf, 8);
        write(buf, len);
        return *this;
    }

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

///////////////////////////////////////////////////////////////////////////////
// Template Implementation for charconv.h
//
// This must be defined HERE (after fl::string class definition) rather than
// in charconv.h to avoid circular dependency. charconv.h is included by
// string.h before the string class is defined, so template implementations
// that return fl::string must be placed here.
///////////////////////////////////////////////////////////////////////////////

// Implementation of to_hex template function (declared in charconv.h)
template<typename T>
fl::string to_hex(T value, bool uppercase, bool pad_to_width) {
    // Determine width classification at compile time
    constexpr auto width = detail::get_hex_int_width<sizeof(T)>();

    // Handle signed types
    bool is_negative = false;
    uint64_t unsigned_value;

    // Check if type is signed and value is negative
    if (static_cast<int64_t>(value) < 0 && sizeof(T) <= 8) {
        // Only handle negative for types that fit in int64_t
        is_negative = true;
        // Convert to positive value for hex conversion
        unsigned_value = static_cast<uint64_t>(-static_cast<int64_t>(value));
    } else {
        unsigned_value = static_cast<uint64_t>(value);
    }

    return detail::hex(unsigned_value, width, is_negative, uppercase, pad_to_width);
}

} // namespace fl
