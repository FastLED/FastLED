#pragma once

/// @file basic_string.h
/// @brief Concrete type-erased string class operating on a caller-
/// provided buffer (or `fl::span<char>`).
/// All string logic lives here, compiled once. `fl::string` is the
/// default convenience wrapper that co-locates a 64-byte inline
/// buffer + heap-overflow with the basic_string state.

#include "fl/stl/int.h"
#include "fl/stl/cstring.h"
#include "fl/stl/detail/string_holder.h"
#include "fl/stl/cctype.h"
#include "fl/stl/charconv.h"
#include "fl/stl/not_null.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/variant.h"
#include "fl/stl/iterator.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/move.h"
#include "fl/math/math.h"
#include "fl/stl/noexcept.h"

namespace fl {

// Forward declarations
class string_view;
template <typename T, fl::size Extent> class span;

// Define shared_ptr type for StringHolder
using StringHolderPtr = fl::shared_ptr<fl::StringHolder>;
using NotNullStringHolderPtr = fl::not_null<StringHolderPtr>;

/// Concrete type-erased string class. Holds all string logic
/// (write, append, find, replace, resize, hashing, …) and is
/// directly constructible from a caller-provided buffer
/// (`char* + len`) or `fl::span<char>`. `fl::string` is a thin
/// wrapper that co-locates a default-sized inline buffer with the
/// `basic_string` state and adds composite-type formatters
/// (vec2, vector, span, …). See `agents/docs/string-architecture.md`.
class basic_string {
  public:
    // ======= NESTED TYPES =======

    // Non-owning pointer to constant null-terminated string data.
    struct ConstLiteral {
        const char* data;
        constexpr ConstLiteral() FL_NOEXCEPT : data(nullptr) {}
        constexpr explicit ConstLiteral(const char* str) FL_NOEXCEPT : data(str) {}
    };

    // Non-owning pointer + length to constant string data.
    struct ConstView {
        const char* data;
        fl::size length;
        constexpr ConstView() FL_NOEXCEPT : data(nullptr), length(0) {}
        constexpr ConstView(const char* str, fl::size len) FL_NOEXCEPT : data(str), length(len) {}
    };

    class iterator {
    private:
        char* ptr;
    public:
        typedef char value_type;
        typedef char& reference;
        typedef char* pointer;
        typedef fl::ptrdiff_t difference_type;
        typedef fl::random_access_iterator_tag iterator_category;

        iterator() FL_NOEXCEPT : ptr(nullptr) {}
        explicit iterator(char* p) FL_NOEXCEPT : ptr(p) {}

        reference operator*() const FL_NOEXCEPT { return *ptr; }
        pointer operator->() const FL_NOEXCEPT { return ptr; }
        iterator& operator++() FL_NOEXCEPT { ++ptr; return *this; }
        iterator operator++(int) FL_NOEXCEPT { iterator tmp = *this; ++ptr; return tmp; }
        iterator& operator--() FL_NOEXCEPT { --ptr; return *this; }
        iterator operator--(int) FL_NOEXCEPT { iterator tmp = *this; --ptr; return tmp; }

        iterator operator+(difference_type n) const FL_NOEXCEPT { return iterator(ptr + n); }
        iterator operator-(difference_type n) const FL_NOEXCEPT { return iterator(ptr - n); }
        iterator& operator+=(difference_type n) FL_NOEXCEPT { ptr += n; return *this; }
        iterator& operator-=(difference_type n) FL_NOEXCEPT { ptr -= n; return *this; }
        difference_type operator-(const iterator& other) const FL_NOEXCEPT { return ptr - other.ptr; }
        reference operator[](difference_type n) const FL_NOEXCEPT { return ptr[n]; }

        bool operator==(const iterator& other) const FL_NOEXCEPT { return ptr == other.ptr; }
        bool operator!=(const iterator& other) const FL_NOEXCEPT { return ptr != other.ptr; }
        bool operator<(const iterator& other) const FL_NOEXCEPT { return ptr < other.ptr; }
        bool operator>(const iterator& other) const FL_NOEXCEPT { return ptr > other.ptr; }
        bool operator<=(const iterator& other) const FL_NOEXCEPT { return ptr <= other.ptr; }
        bool operator>=(const iterator& other) const FL_NOEXCEPT { return ptr >= other.ptr; }

        operator char*() const FL_NOEXCEPT { return ptr; }
    };

    class const_iterator {
    private:
        const char* ptr;
    public:
        typedef char value_type;
        typedef const char& reference;
        typedef const char* pointer;
        typedef fl::ptrdiff_t difference_type;
        typedef fl::random_access_iterator_tag iterator_category;

        const_iterator() FL_NOEXCEPT : ptr(nullptr) {}
        explicit const_iterator(const char* p) FL_NOEXCEPT : ptr(p) {}
        const_iterator(const iterator& it) FL_NOEXCEPT : ptr(it.operator char*()) {}

        reference operator*() const FL_NOEXCEPT { return *ptr; }
        pointer operator->() const FL_NOEXCEPT { return ptr; }
        const_iterator& operator++() FL_NOEXCEPT { ++ptr; return *this; }
        const_iterator operator++(int) FL_NOEXCEPT { const_iterator tmp = *this; ++ptr; return tmp; }
        const_iterator& operator--() FL_NOEXCEPT { --ptr; return *this; }
        const_iterator operator--(int) FL_NOEXCEPT { const_iterator tmp = *this; --ptr; return tmp; }

        const_iterator operator+(difference_type n) const FL_NOEXCEPT { return const_iterator(ptr + n); }
        const_iterator operator-(difference_type n) const FL_NOEXCEPT { return const_iterator(ptr - n); }
        const_iterator& operator+=(difference_type n) FL_NOEXCEPT { ptr += n; return *this; }
        const_iterator& operator-=(difference_type n) FL_NOEXCEPT { ptr -= n; return *this; }
        difference_type operator-(const const_iterator& other) const FL_NOEXCEPT { return ptr - other.ptr; }
        reference operator[](difference_type n) const FL_NOEXCEPT { return ptr[n]; }

        bool operator==(const const_iterator& other) const FL_NOEXCEPT { return ptr == other.ptr; }
        bool operator!=(const const_iterator& other) const FL_NOEXCEPT { return ptr != other.ptr; }
        bool operator<(const const_iterator& other) const FL_NOEXCEPT { return ptr < other.ptr; }
        bool operator>(const const_iterator& other) const FL_NOEXCEPT { return ptr > other.ptr; }
        bool operator<=(const const_iterator& other) const FL_NOEXCEPT { return ptr <= other.ptr; }
        bool operator>=(const const_iterator& other) const FL_NOEXCEPT { return ptr >= other.ptr; }

        operator const char*() const FL_NOEXCEPT { return ptr; }
    };

    typedef fl::reverse_iterator<iterator> reverse_iterator;
    typedef fl::reverse_iterator<const_iterator> const_reverse_iterator;

    // ======= CONSTANTS =======
    static constexpr fl::size npos = static_cast<fl::size>(-1);

    // ======= STORAGE TYPE QUERIES =======
    bool is_literal() const FL_NOEXCEPT { return mStorage.is<ConstLiteral>(); }
    bool is_view() const FL_NOEXCEPT { return mStorage.is<ConstView>(); }
    bool is_owning() const FL_NOEXCEPT {
        return isInline() || mStorage.is<NotNullStringHolderPtr>();
    }
    bool is_referencing() const FL_NOEXCEPT { return is_literal() || is_view(); }

    // ======= ACCESSORS =======
    fl::size size() const FL_NOEXCEPT { return mLength; }
    fl::size length() const FL_NOEXCEPT { return mLength; }
    bool empty() const FL_NOEXCEPT { return mLength == 0; }
    const char* c_str() const FL_NOEXCEPT;
    const char* data() const FL_NOEXCEPT { return c_str(); }
    char* c_str_mutable() FL_NOEXCEPT;
    fl::size capacity() const FL_NOEXCEPT;

    // ======= ELEMENT ACCESS =======
    char operator[](fl::size index) const FL_NOEXCEPT;
    char& operator[](fl::size index) FL_NOEXCEPT;
    char& at(fl::size pos) FL_NOEXCEPT;
    const char& at(fl::size pos) const FL_NOEXCEPT;
    char front() const FL_NOEXCEPT;
    char back() const FL_NOEXCEPT;
    char charAt(fl::size index) const FL_NOEXCEPT;

    // ======= ITERATORS =======
    iterator begin() FL_NOEXCEPT { return iterator(c_str_mutable()); }
    iterator end() FL_NOEXCEPT { return iterator(c_str_mutable() + mLength); }
    const_iterator begin() const FL_NOEXCEPT { return const_iterator(c_str()); }
    const_iterator end() const FL_NOEXCEPT { return const_iterator(c_str() + mLength); }
    const_iterator cbegin() const FL_NOEXCEPT { return const_iterator(c_str()); }
    const_iterator cend() const FL_NOEXCEPT { return const_iterator(c_str() + mLength); }
    reverse_iterator rbegin() FL_NOEXCEPT { return reverse_iterator(end()); }
    reverse_iterator rend() FL_NOEXCEPT { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin() const FL_NOEXCEPT { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const FL_NOEXCEPT { return const_reverse_iterator(begin()); }
    const_reverse_iterator crbegin() const FL_NOEXCEPT { return const_reverse_iterator(end()); }
    const_reverse_iterator crend() const FL_NOEXCEPT { return const_reverse_iterator(begin()); }

    // ======= COMPARISON OPERATORS =======
    bool operator==(const basic_string& other) const FL_NOEXCEPT;
    bool operator!=(const basic_string& other) const FL_NOEXCEPT;
    bool operator<(const basic_string& other) const FL_NOEXCEPT;
    bool operator>(const basic_string& other) const FL_NOEXCEPT;
    bool operator<=(const basic_string& other) const FL_NOEXCEPT;
    bool operator>=(const basic_string& other) const FL_NOEXCEPT;

    bool operator==(const char* other) const FL_NOEXCEPT;
    bool operator!=(const char* other) const FL_NOEXCEPT;

    // ======= WRITE OPERATIONS =======
    fl::size write(const fl::u8* data, fl::size n) FL_NOEXCEPT;
    fl::size write(const char* str, fl::size n) FL_NOEXCEPT;
    fl::size write(char c) FL_NOEXCEPT;
    fl::size write(fl::u8 c) FL_NOEXCEPT;
    fl::size write(const fl::u16& n) FL_NOEXCEPT;
    fl::size write(const fl::u32& val) FL_NOEXCEPT;
    fl::size write(const u64& val) FL_NOEXCEPT;
    fl::size write(const i64& val) FL_NOEXCEPT;
    fl::size write(const fl::i32& val) FL_NOEXCEPT;
    fl::size write(const fl::i8 val) FL_NOEXCEPT;

    // Generic write for multi-byte integer types
    template <typename T>
    typename fl::enable_if<fl::is_multi_byte_integer<T>::value, fl::size>::type
    write(const T& val) FL_NOEXCEPT {
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

    // ======= COPY OPERATIONS =======
    void copy(const char* str) FL_NOEXCEPT;
    void copy(const char* str, fl::size len) FL_NOEXCEPT;
    void copy(const basic_string& other) FL_NOEXCEPT;
    fl::size copy(char* dest, fl::size count, fl::size pos = 0) const FL_NOEXCEPT;

    // ======= ASSIGN OPERATIONS =======
    void assign(const char* str, fl::size len) FL_NOEXCEPT;
    basic_string& assign(const basic_string& str) FL_NOEXCEPT;
    basic_string& assign(const basic_string& str, fl::size pos, fl::size count = npos) FL_NOEXCEPT;
    basic_string& assign(fl::size count, char c) FL_NOEXCEPT;
    basic_string& assign(basic_string&& str) FL_NOEXCEPT;

    // Assign from iterator range
    template <typename InputIt>
    basic_string& assign(InputIt first, InputIt last) FL_NOEXCEPT {
        clear();
        fl::size len = 0;
        for (auto it = first; it != last; ++it) {
            ++len;
        }
        if (len == 0) {
            return *this;
        }
        mLength = len;
        if (len + 1 <= mInlineCapacity) {
            if (!isInline()) {
                mStorage.reset();
            }
            fl::size i = 0;
            for (auto it = first; it != last; ++it, ++i) {
                inlineBufferPtr()[i] = *it;
            }
            inlineBufferPtr()[len] = '\0';
        } else {
            mStorage = NotNullStringHolderPtr(fl::make_shared<StringHolder>(len));
            NotNullStringHolderPtr& ptr = heapData();
            fl::size i = 0;
            for (auto it = first; it != last; ++it, ++i) {
                ptr->data()[i] = *it;
            }
            ptr->data()[len] = '\0';
        }
        return *this;
    }

    // ======= FIND OPERATIONS =======
    fl::size find(const char& value) const FL_NOEXCEPT;
    fl::size find(const char* substr) const FL_NOEXCEPT;
    fl::size find(const basic_string& other) const FL_NOEXCEPT;
    fl::size find(const char& value, fl::size start_pos) const FL_NOEXCEPT;
    fl::size find(const char* substr, fl::size start_pos) const FL_NOEXCEPT;
    fl::size find(const basic_string& other, fl::size start_pos) const FL_NOEXCEPT;

    fl::size rfind(char c, fl::size pos = npos) const FL_NOEXCEPT;
    fl::size rfind(const char* s, fl::size pos, fl::size count) const FL_NOEXCEPT;
    fl::size rfind(const char* s, fl::size pos = npos) const FL_NOEXCEPT;
    fl::size rfind(const basic_string& str, fl::size pos = npos) const FL_NOEXCEPT;

    fl::size find_first_of(char c, fl::size pos = 0) const FL_NOEXCEPT;
    fl::size find_first_of(const char* s, fl::size pos, fl::size count) const FL_NOEXCEPT;
    fl::size find_first_of(const char* s, fl::size pos = 0) const FL_NOEXCEPT;
    fl::size find_first_of(const basic_string& str, fl::size pos = 0) const FL_NOEXCEPT;

    fl::size find_last_of(char c, fl::size pos = npos) const FL_NOEXCEPT;
    fl::size find_last_of(const char* s, fl::size pos, fl::size count) const FL_NOEXCEPT;
    fl::size find_last_of(const char* s, fl::size pos = npos) const FL_NOEXCEPT;
    fl::size find_last_of(const basic_string& str, fl::size pos = npos) const FL_NOEXCEPT;

    fl::size find_first_not_of(char c, fl::size pos = 0) const FL_NOEXCEPT;
    fl::size find_first_not_of(const char* s, fl::size pos, fl::size count) const FL_NOEXCEPT;
    fl::size find_first_not_of(const char* s, fl::size pos = 0) const FL_NOEXCEPT;
    fl::size find_first_not_of(const basic_string& str, fl::size pos = 0) const FL_NOEXCEPT;

    fl::size find_last_not_of(char c, fl::size pos = npos) const FL_NOEXCEPT;
    fl::size find_last_not_of(const char* s, fl::size pos, fl::size count) const FL_NOEXCEPT;
    fl::size find_last_not_of(const char* s, fl::size pos = npos) const FL_NOEXCEPT;
    fl::size find_last_not_of(const basic_string& str, fl::size pos = npos) const FL_NOEXCEPT;

    // ======= CONTAINS / STARTS_WITH / ENDS_WITH =======
    bool contains(const char* substr) const FL_NOEXCEPT;
    bool contains(char c) const FL_NOEXCEPT;
    bool contains(const basic_string& other) const FL_NOEXCEPT;

    bool starts_with(const char* prefix) const FL_NOEXCEPT;
    bool starts_with(char c) const FL_NOEXCEPT;
    bool starts_with(const basic_string& prefix) const FL_NOEXCEPT;

    bool ends_with(const char* suffix) const FL_NOEXCEPT;
    bool ends_with(char c) const FL_NOEXCEPT;
    bool ends_with(const basic_string& suffix) const FL_NOEXCEPT;

    // ======= STACK OPERATIONS =======
    void push_back(char c) FL_NOEXCEPT;
    void push_ascii(char c) FL_NOEXCEPT;
    void pop_back() FL_NOEXCEPT;

    // ======= APPEND OPERATIONS =======
    basic_string& append(const char* str) FL_NOEXCEPT;
    basic_string& append(const char* str, fl::size len) FL_NOEXCEPT;
    basic_string& append(char c) FL_NOEXCEPT;
    basic_string& append(const i8& val) FL_NOEXCEPT;
    basic_string& append(const u8& val) FL_NOEXCEPT;
    basic_string& append(const bool& val) FL_NOEXCEPT;
    basic_string& append(const i16& val) FL_NOEXCEPT;
    basic_string& append(const u16& val) FL_NOEXCEPT;
    basic_string& append(const i32& val) FL_NOEXCEPT;
    basic_string& append(const u32& val) FL_NOEXCEPT;
    basic_string& append(const i64& val) FL_NOEXCEPT;
    basic_string& append(const u64& val) FL_NOEXCEPT;
    basic_string& append(const float& val) FL_NOEXCEPT;
    basic_string& append(const float& val, int precision) FL_NOEXCEPT;
    basic_string& append(const double& val) FL_NOEXCEPT;
    basic_string& append(const basic_string& str) FL_NOEXCEPT;

    // SFINAE catch-all for integer types not covered by explicit overloads
    // (e.g. unsigned long on Windows, which is distinct from both u32 and u64).
    // Casts to the appropriate fl:: type via cast_target.
    template<typename T>
    typename fl::enable_if<fl::is_multi_byte_integer<T>::value
        && !fl::is_same<T, i8>::value  && !fl::is_same<T, u8>::value
        && !fl::is_same<T, i16>::value && !fl::is_same<T, u16>::value
        && !fl::is_same<T, i32>::value && !fl::is_same<T, u32>::value
        && !fl::is_same<T, i64>::value && !fl::is_same<T, u64>::value,
        basic_string&>::type
    append(T val) FL_NOEXCEPT {
        using target_t = typename int_cast_detail::cast_target<T>::type;
        return append(static_cast<target_t>(val));
    }

    // ======= HEX/OCT APPEND =======
    basic_string& appendHex(i32 val) FL_NOEXCEPT;
    basic_string& appendHex(u32 val) FL_NOEXCEPT;
    basic_string& appendHex(i64 val) FL_NOEXCEPT;
    basic_string& appendHex(u64 val) FL_NOEXCEPT;
    basic_string& appendHex(i16 val) FL_NOEXCEPT;
    basic_string& appendHex(u16 val) FL_NOEXCEPT;
    basic_string& appendHex(i8 val) FL_NOEXCEPT;
    basic_string& appendHex(u8 val) FL_NOEXCEPT;
    basic_string& appendOct(i32 val) FL_NOEXCEPT;
    basic_string& appendOct(u32 val) FL_NOEXCEPT;
    basic_string& appendOct(i64 val) FL_NOEXCEPT;
    basic_string& appendOct(u64 val) FL_NOEXCEPT;
    basic_string& appendOct(i16 val) FL_NOEXCEPT;
    basic_string& appendOct(u16 val) FL_NOEXCEPT;
    basic_string& appendOct(i8 val) FL_NOEXCEPT;
    basic_string& appendOct(u8 val) FL_NOEXCEPT;

    // ======= INSERT =======
    basic_string& insert(fl::size pos, fl::size count, char ch) FL_NOEXCEPT;
    basic_string& insert(fl::size pos, const char* s) FL_NOEXCEPT;
    basic_string& insert(fl::size pos, const char* s, fl::size count) FL_NOEXCEPT;
    basic_string& insert(fl::size pos, const basic_string& str) FL_NOEXCEPT;
    basic_string& insert(fl::size pos, const basic_string& str, fl::size pos2, fl::size count = npos) FL_NOEXCEPT;

    // ======= ERASE =======
    basic_string& erase(fl::size pos = 0, fl::size count = npos) FL_NOEXCEPT;

    // Iterator-based erase (SFINAE for pointer types)
    template<typename T>
    typename fl::enable_if<fl::is_pointer<T>::value && fl::is_same<typename fl::remove_cv<typename fl::remove_pointer<T>::type>::type, char>::value, char*>::type
    erase(T it_pos) FL_NOEXCEPT {
        if (!it_pos) return end();
        const char* str_begin = c_str();
        const char* str_end = str_begin + mLength;
        if (it_pos < str_begin || it_pos >= str_end) return end();
        fl::size pos = it_pos - str_begin;
        erase(pos, 1);
        return begin() + pos;
    }

    template<typename T>
    typename fl::enable_if<fl::is_pointer<T>::value && fl::is_same<typename fl::remove_cv<typename fl::remove_pointer<T>::type>::type, char>::value, char*>::type
    erase(T first, T last) FL_NOEXCEPT {
        if (!first || !last || first >= last) return end();
        const char* str_begin = c_str();
        const char* str_end = str_begin + mLength;
        if (first < str_begin) first = begin();
        if (last > str_end) last = end();
        if (first >= str_end) return end();
        fl::size pos = first - str_begin;
        fl::size count = last - first;
        erase(pos, count);
        return begin() + pos;
    }

    // ======= REPLACE =======
    basic_string& replace(fl::size pos, fl::size count, const basic_string& str) FL_NOEXCEPT;
    basic_string& replace(fl::size pos, fl::size count, const basic_string& str,
                     fl::size pos2, fl::size count2 = npos) FL_NOEXCEPT;
    basic_string& replace(fl::size pos, fl::size count, const char* s, fl::size count2) FL_NOEXCEPT;
    basic_string& replace(fl::size pos, fl::size count, const char* s) FL_NOEXCEPT;
    basic_string& replace(fl::size pos, fl::size count, fl::size count2, char ch) FL_NOEXCEPT;

    // ======= COMPARE =======
    int compare(const basic_string& str) const FL_NOEXCEPT;
    int compare(fl::size pos1, fl::size count1, const basic_string& str) const FL_NOEXCEPT;
    int compare(fl::size pos1, fl::size count1, const basic_string& str,
                fl::size pos2, fl::size count2 = npos) const FL_NOEXCEPT;
    int compare(const char* s) const FL_NOEXCEPT;
    int compare(fl::size pos1, fl::size count1, const char* s) const FL_NOEXCEPT;
    int compare(fl::size pos1, fl::size count1, const char* s, fl::size count2) const FL_NOEXCEPT;

    // ======= MEMORY MANAGEMENT =======
    void reserve(fl::size newCapacity) FL_NOEXCEPT;
    void clear(bool freeMemory = false) FL_NOEXCEPT;
    void shrink_to_fit() FL_NOEXCEPT;
    fl::size max_size() const FL_NOEXCEPT { return (npos / 2) - 1; }
    void resize(fl::size count) FL_NOEXCEPT;
    void resize(fl::size count, char ch) FL_NOEXCEPT;

    // ======= OTHER =======
    float toFloat() const FL_NOEXCEPT;

    // ======= DESTRUCTOR =======
    // Body in basic_string.cpp.hpp per the project's header-thin
    // convention (declarations in `*.h`, definitions in `*.cpp.hpp`).
    ~basic_string() FL_NOEXCEPT;

    // ======= PUBLIC CONSTRUCTION =======
    // Construct a basic_string backed by a caller-provided buffer.
    // The span / (ptr, len) pair points at a block of `char` the
    // caller owns; the resulting basic_string is non-owning until
    // the first write that exceeds the buffer's capacity, which
    // promotes to heap-backed storage via `mStorage`.
    //
    // Lifetime contract: the buffer must outlive the basic_string
    // AND the basic_string must not be trivially relocated (bitwise
    // copied to a different address) — `mInlineOffset` is computed
    // from `this` at construction time. For member-stored use,
    // either co-locate the buffer with the basic_string (the
    // `fl::string` pattern) or arrange the buffer at a fixed
    // offset from `this` (e.g. as a class field).
    basic_string(char* inlineBuffer, fl::size inlineCapacity) FL_NOEXCEPT
        : mInlineOffset(static_cast<fl::size>(
              inlineBuffer -
              static_cast<char*>(static_cast<void*>(this))))
        , mInlineCapacity(inlineCapacity)
        , mLength(0)
        , mStorage() // empty variant = inline mode
    {}

    basic_string(fl::span<char, static_cast<fl::size>(-1)> storage) FL_NOEXCEPT;

  protected:
    // Deleted copy/move — wrappers (fl::string) handle these via
    // `copy(const basic_string&)` / `moveFrom(basic_string&&)`.
    basic_string(const basic_string&) FL_NOEXCEPT = delete;
    basic_string(basic_string&&) FL_NOEXCEPT = delete;
    basic_string& operator=(const basic_string&) FL_NOEXCEPT = delete;
    basic_string& operator=(basic_string&&) FL_NOEXCEPT = delete;

    // ======= CONTENT POPULATION (for wrapper constructors) =======
    void moveFrom(basic_string&& other) FL_NOEXCEPT;
    void moveAssign(basic_string&& other) FL_NOEXCEPT;
    void swapWith(basic_string& other) FL_NOEXCEPT;

    // Factory helpers
    void setLiteral(const char* literal) FL_NOEXCEPT;
    void setView(const char* data, fl::size len) FL_NOEXCEPT;
    void setSharedHolder(const fl::shared_ptr<StringHolder>& holder) FL_NOEXCEPT;

    // ======= DATA MEMBERS =======
    // Store offset from `this` to the inline buffer, not a raw pointer.
    // This survives trivial relocation (bitwise copy by containers) because
    // the offset is relative to `this` which updates with the object.
    fl::size mInlineOffset;
    fl::size mInlineCapacity;
    fl::size mLength;
    fl::variant<NotNullStringHolderPtr, ConstLiteral, ConstView> mStorage;

    // ======= HELPER METHODS =======
    // Compute inline buffer pointer from offset (survives trivial relocation)
    char* inlineBufferPtr() FL_NOEXCEPT {
        return static_cast<char*>(static_cast<void*>(this)) + mInlineOffset;
    }
    const char* inlineBufferPtr() const FL_NOEXCEPT {
        return static_cast<const char*>(static_cast<const void*>(this)) + mInlineOffset;
    }

    bool isInline() const FL_NOEXCEPT { return mStorage.empty(); }
    bool hasHeapData() const FL_NOEXCEPT { return mStorage.is<NotNullStringHolderPtr>(); }
    bool hasConstLiteral() const FL_NOEXCEPT { return mStorage.is<ConstLiteral>(); }
    bool hasConstView() const FL_NOEXCEPT { return mStorage.is<ConstView>(); }
    bool isNonOwning() const FL_NOEXCEPT { return hasConstLiteral() || hasConstView(); }

    const char* constData() const FL_NOEXCEPT;
    void materialize() FL_NOEXCEPT;

    NotNullStringHolderPtr& heapData() FL_NOEXCEPT;
    const NotNullStringHolderPtr& heapData() const FL_NOEXCEPT;
};

} // namespace fl
