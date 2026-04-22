#pragma once


#include "fl/math/math.h"
#include "fl/math/geometry.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/initializer_list.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

template <typename T, fl::size INLINED_SIZE> class FixedVector;

template <typename T> class vector;

template <typename T, fl::size INLINED_SIZE> class VectorN;

// Backward compat alias
template <typename T, fl::size INLINED_SIZE> using InlinedVector = VectorN<T, INLINED_SIZE>;

template <typename T, fl::size N> class array;

// Special value to indicate dynamic extent (runtime-determined size)
constexpr fl::size dynamic_extent = fl::size(-1);

// Forward declaration for static extent specialization
template <typename T, fl::size Extent = dynamic_extent> class span;

// ======= SFINAE TRAITS FOR GENERIC CONTAINER SUPPORT =======
// Detect if a type has .data() method
template <typename T>
class has_data_method {
  private:
    typedef fl::u8 yes;
    typedef fl::u16 no;

    template <typename C>
    static yes test(decltype(&C::data)) FL_NOEXCEPT;

    template <typename>
    static no test(...) FL_NOEXCEPT;

  public:
    static constexpr bool value = sizeof(test<T>(nullptr)) == sizeof(yes);
};

// Detect if a type has .size() method
template <typename T>
class has_size_method {
  private:
    typedef fl::u8 yes;
    typedef fl::u16 no;

    template <typename C>
    static yes test(decltype(&C::size)) FL_NOEXCEPT;

    template <typename>
    static no test(...) FL_NOEXCEPT;

  public:
    static constexpr bool value = sizeof(test<T>(nullptr)) == sizeof(yes);
};

// Combined check for containers with both .data() and .size()
template <typename T>
struct has_data_and_size {
    static constexpr bool value = has_data_method<T>::value && has_size_method<T>::value;
};

// span<int> is equivalent to int* with a length. It is used to pass around
// arrays of integers with a length, without needing to pass around a separate
// length parameter.
// It works just like an array of objects, but it also knows its length.
//
// This is the dynamic extent specialization (Extent == dynamic_extent).
template <typename T> class span<T, dynamic_extent> {
  public:
    // ======= STANDARD CONTAINER TYPE ALIASES =======
    using element_type = T;
    using value_type = typename fl::remove_cv<T>::type;
    using size_type = fl::size;
    using difference_type = fl::i32;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using iterator = T*;
    using const_iterator = const T*;
    using reverse_iterator = T*;
    using const_reverse_iterator = const T*;

    static constexpr fl::size extent = dynamic_extent;

    // ======= CONSTRUCTORS =======
    span() FL_NOEXCEPT : mData(nullptr), mSize(0) {}
    span(T *data, fl::size size) FL_NOEXCEPT : mData(data), mSize(size) {}

    // ======= INITIALIZER LIST CONSTRUCTOR DELETED =======
    // P2447R6 (C++26) adds span(initializer_list<T>) but the backing array is
    // always a temporary, so storing a span from a braced-init-list is UB unless
    // consumed in the same expression.  Deleted to prevent dangling references.
    span(fl::initializer_list<value_type> init) FL_NOEXCEPT = delete;

    // ======= CONTAINER CONSTRUCTORS =======
    // Simple constructors that work for all cases
    span(const fl::vector<T> &vector) FL_NOEXCEPT
        : mData(vector.data()), mSize(vector.size()) {}

    template <fl::size INLINED_SIZE>
    span(const FixedVector<T, INLINED_SIZE> &vector) FL_NOEXCEPT
        : mData(vector.data()), mSize(vector.size()) {}

    template <fl::size INLINED_SIZE>
    span(const VectorN<T, INLINED_SIZE> &vector) FL_NOEXCEPT
        : mData(vector.data()), mSize(vector.size()) {}

    // Additional constructors for const conversion (U -> const U)
    template<typename U>
    span(const fl::vector<U> &vector) FL_NOEXCEPT
        : mData(vector.data()), mSize(vector.size()) {}

    template<typename U, fl::size INLINED_SIZE>
    span(const FixedVector<U, INLINED_SIZE> &vector) FL_NOEXCEPT
        : mData(vector.data()), mSize(vector.size()) {}

    template<typename U, fl::size INLINED_SIZE>
    span(const VectorN<U, INLINED_SIZE> &vector) FL_NOEXCEPT
        : mData(vector.data()), mSize(vector.size()) {}

    // ======= NON-CONST CONTAINER CONVERSIONS =======
    // Non-const versions for mutable spans
    span(fl::vector<T> &vector) FL_NOEXCEPT
        : mData(vector.data()), mSize(vector.size()) {}

    template <fl::size INLINED_SIZE>
    span(FixedVector<T, INLINED_SIZE> &vector) FL_NOEXCEPT
        : mData(vector.data()), mSize(vector.size()) {}

    template <fl::size INLINED_SIZE>
    span(VectorN<T, INLINED_SIZE> &vector) FL_NOEXCEPT
        : mData(vector.data()), mSize(vector.size()) {}

    // ======= GENERIC CONTAINER CONSTRUCTOR =======
    // Generic constructor for any container with .data() and .size()
    // Uses SFINAE to only enable for appropriate containers
    // This excludes span itself and already-specialized containers
    template<typename Container>
    span(Container& c, typename fl::enable_if<
        has_data_and_size<Container>::value &&
        !fl::is_same<typename fl::decay<Container>::type, span<T, dynamic_extent>>::value &&
        !fl::is_base_of<span<T, dynamic_extent>, Container>::value
    >::type* = nullptr)
 FL_NOEXCEPT : mData(c.data()), mSize(c.size()) {}

    // Const version for const containers
    template<typename Container>
    span(const Container& c, typename fl::enable_if<
        has_data_and_size<Container>::value &&
        !fl::is_same<typename fl::decay<Container>::type, span<T, dynamic_extent>>::value &&
        !fl::is_base_of<span<T, dynamic_extent>, Container>::value
    >::type* = nullptr)
 FL_NOEXCEPT : mData(c.data()), mSize(c.size()) {}

    // ======= FL::ARRAY CONVERSIONS =======
    // fl::array<T> -> span<T>
    template <fl::size N>
    span(const array<T, N> &arr) FL_NOEXCEPT
        : mData(arr.data()), mSize(N) {}

    template <fl::size N>
    span(array<T, N> &arr) FL_NOEXCEPT
        : mData(arr.data()), mSize(N) {}

    // fl::array<U> -> span<T> (for type conversions like U -> const U)
    template <typename U, fl::size N>
    span(const array<U, N> &arr) FL_NOEXCEPT
        : mData(arr.data()), mSize(N) {}

    template <typename U, fl::size N>
    span(array<U, N> &arr) FL_NOEXCEPT
        : mData(arr.data()), mSize(N) {}

    // ======= C-STYLE ARRAY CONVERSIONS =======
    // T[] -> span<T>
    template <fl::size ARRAYSIZE>
    span(T (&array)[ARRAYSIZE]) FL_NOEXCEPT
        : mData(array), mSize(ARRAYSIZE) {}

    // U[] -> span<T> (for type conversions like U -> const U)
    template <typename U, fl::size ARRAYSIZE>
    span(U (&array)[ARRAYSIZE]) FL_NOEXCEPT
        : mData(array), mSize(ARRAYSIZE) {}

    // const U[] -> span<T> (for const arrays)
    template <typename U, fl::size ARRAYSIZE>
    span(const U (&array)[ARRAYSIZE]) FL_NOEXCEPT
        : mData(array), mSize(ARRAYSIZE) {}

    // ======= ITERATOR CONVERSIONS =======
    template <typename Iterator>
    span(Iterator begin, Iterator end) FL_NOEXCEPT
        : mData(&(*begin)), mSize(end - begin) {}

    span(const span &other) FL_NOEXCEPT : mData(other.mData), mSize(other.mSize) {}

    span &operator=(const span &other) FL_NOEXCEPT {
        mData = other.mData;
        mSize = other.mSize;
        return *this;
    }

    // Automatic promotion to const span<const T, dynamic_extent>
    operator span<const T, dynamic_extent>() const FL_NOEXCEPT { return span<const T, dynamic_extent>(mData, mSize); }

    T &operator[](fl::size index) FL_NOEXCEPT {
        // No bounds checking in embedded environment
        return mData[index];
    }

    const T &operator[](fl::size index) const FL_NOEXCEPT {
        // No bounds checking in embedded environment
        return mData[index];
    }

    // ======= ITERATORS =======
    iterator begin() FL_NOEXCEPT { return mData; }
    const_iterator begin() const FL_NOEXCEPT { return mData; }
    const_iterator cbegin() const FL_NOEXCEPT { return mData; }

    iterator end() FL_NOEXCEPT { return mData + mSize; }
    const_iterator end() const FL_NOEXCEPT { return mData + mSize; }
    const_iterator cend() const FL_NOEXCEPT { return mData + mSize; }

    reverse_iterator rbegin() FL_NOEXCEPT { return mData + mSize - 1; }
    const_reverse_iterator rbegin() const FL_NOEXCEPT { return mData + mSize - 1; }
    const_reverse_iterator crbegin() const FL_NOEXCEPT { return mData + mSize - 1; }

    reverse_iterator rend() FL_NOEXCEPT { return mData - 1; }
    const_reverse_iterator rend() const FL_NOEXCEPT { return mData - 1; }
    const_reverse_iterator crend() const FL_NOEXCEPT { return mData - 1; }

    // ======= SIZE AND ACCESS =======
    fl::size length() const FL_NOEXCEPT { return mSize; }
    fl::size size() const FL_NOEXCEPT { return mSize; }
    fl::size size_bytes() const FL_NOEXCEPT { return mSize * sizeof(T); }

    const T *data() const FL_NOEXCEPT { return mData; }
    T *data() FL_NOEXCEPT { return mData; }

    // ======= SUBVIEWS =======
    // Runtime-sized subviews (existing slice methods)
    span<T, dynamic_extent> slice(fl::size start, fl::size end) const FL_NOEXCEPT {
        // No bounds checking in embedded environment
        return span<T, dynamic_extent>(mData + start, end - start);
    }

    span<T, dynamic_extent> slice(fl::size start) const FL_NOEXCEPT {
        // No bounds checking in embedded environment
        return span<T, dynamic_extent>(mData + start, mSize - start);
    }

    // std::span-compatible subspan (runtime version)
    span<T, dynamic_extent> subspan(fl::size offset, fl::size count = dynamic_extent) const FL_NOEXCEPT {
        if (count == dynamic_extent) {
            return span<T, dynamic_extent>(mData + offset, mSize - offset);
        }
        return span<T, dynamic_extent>(mData + offset, count);
    }

    // Compile-time sized first N elements - returns static extent span
    template<fl::size N>
    span<T, N> first() const FL_NOEXCEPT {
        return span<T, N>(mData, N);
    }

    // Runtime-sized first count elements
    span<T, dynamic_extent> first(fl::size count) const FL_NOEXCEPT {
        return span<T, dynamic_extent>(mData, count);
    }

    // Compile-time sized last N elements - returns static extent span
    template<fl::size N>
    span<T, N> last() const FL_NOEXCEPT {
        return span<T, N>(mData + mSize - N, N);
    }

    // Runtime-sized last count elements
    span<T, dynamic_extent> last(fl::size count) const FL_NOEXCEPT {
        return span<T, dynamic_extent>(mData + mSize - count, count);
    }

    // Compile-time sized subspan - returns static extent span
    template<fl::size Offset, fl::size Count = dynamic_extent>
    span<T, Count> subspan() const FL_NOEXCEPT {
        if (Count == dynamic_extent) {
            return span<T, dynamic_extent>(mData + Offset, mSize - Offset);
        }
        return span<T, Count>(mData + Offset, Count);
    }

    // Find the first occurrence of a value in the slice
    // Returns the index of the first occurrence if found, or fl::size(-1) if not
    // found
    fl::size find(const T &value) const FL_NOEXCEPT {
        for (fl::size i = 0; i < mSize; ++i) {
            if (mData[i] == value) {
                return i;
            }
        }
        return fl::size(-1);
    }

    bool pop_front() FL_NOEXCEPT {
        if (mSize == 0) {
            return false;
        }
        ++mData;
        --mSize;
        return true;
    }

    bool pop_back() FL_NOEXCEPT {
        if (mSize == 0) {
            return false;
        }
        --mSize;
        return true;
    }

    T &front() FL_NOEXCEPT { return *mData; }

    const T &front() const FL_NOEXCEPT { return *mData; }

    T &back() FL_NOEXCEPT { return *(mData + mSize - 1); }

    const T &back() const FL_NOEXCEPT { return *(mData + mSize - 1); }

    bool empty() const FL_NOEXCEPT { return mSize == 0; }

    // ======= COMPARISON OPERATORS =======
    // Lexicographical comparison - compares element by element
    bool operator==(const span<T, dynamic_extent>& other) const FL_NOEXCEPT {
        if (mSize != other.mSize) return false;
        for (fl::size i = 0; i < mSize; ++i) {
            if (!(mData[i] == other.mData[i])) return false;
        }
        return true;
    }

    bool operator!=(const span<T, dynamic_extent>& other) const FL_NOEXCEPT {
        return !(*this == other);
    }

    bool operator<(const span<T, dynamic_extent>& other) const FL_NOEXCEPT {
        fl::size min_size = mSize < other.mSize ? mSize : other.mSize;
        for (fl::size i = 0; i < min_size; ++i) {
            if (mData[i] < other.mData[i]) return true;
            if (other.mData[i] < mData[i]) return false;
        }
        return mSize < other.mSize;
    }

    bool operator<=(const span<T, dynamic_extent>& other) const FL_NOEXCEPT {
        return !(other < *this);
    }

    bool operator>(const span<T, dynamic_extent>& other) const FL_NOEXCEPT {
        return other < *this;
    }

    bool operator>=(const span<T, dynamic_extent>& other) const FL_NOEXCEPT {
        return !(*this < other);
    }

  private:
    T *mData;
    fl::size mSize;
};

// ======= STATIC EXTENT SPECIALIZATION =======
// span with compile-time known size (static extent)
// This specialization stores size at compile-time, reducing runtime overhead
template <typename T, fl::size Extent> class span {
  public:
    // ======= STANDARD CONTAINER TYPE ALIASES =======
    using element_type = T;
    using value_type = typename fl::remove_cv<T>::type;
    using size_type = fl::size;
    using difference_type = fl::i32;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using iterator = T*;
    using const_iterator = const T*;
    using reverse_iterator = T*;
    using const_reverse_iterator = const T*;

    static constexpr fl::size extent = Extent;

    // ======= CONSTRUCTORS =======
    span() FL_NOEXCEPT : mData(nullptr) {}
    span(T *data, fl::size size) FL_NOEXCEPT : mData(data) {
        // In debug builds, could assert size == Extent
        (void)size; // Suppress unused parameter warning
    }

    // Constructor from C-array with matching size
    // Matches std::span: when the argument is an array of exactly Extent
    // elements, size is known at compile time and no runtime length is
    // required.  See std::span<T, N>::span(std::type_identity_t<T>(&)[N]).
    span(T (&array)[Extent]) FL_NOEXCEPT : mData(array) {}

    // Copy constructor
    span(const span &other) FL_NOEXCEPT : mData(other.mData) {}

    span &operator=(const span &other) FL_NOEXCEPT {
        mData = other.mData;
        return *this;
    }

    // Automatic promotion to const span<const T, Extent>
    operator span<const T, Extent>() const FL_NOEXCEPT { return span<const T, Extent>(mData, Extent); }

    // Conversion to dynamic extent
    operator span<T, dynamic_extent>() const FL_NOEXCEPT { return span<T, dynamic_extent>(mData, Extent); }

    // ======= ELEMENT ACCESS =======
    T &operator[](fl::size index) FL_NOEXCEPT {
        return mData[index];
    }

    const T &operator[](fl::size index) const FL_NOEXCEPT {
        return mData[index];
    }

    // ======= ITERATORS =======
    iterator begin() FL_NOEXCEPT { return mData; }
    const_iterator begin() const FL_NOEXCEPT { return mData; }
    const_iterator cbegin() const FL_NOEXCEPT { return mData; }

    iterator end() FL_NOEXCEPT { return mData + Extent; }
    const_iterator end() const FL_NOEXCEPT { return mData + Extent; }
    const_iterator cend() const FL_NOEXCEPT { return mData + Extent; }

    reverse_iterator rbegin() FL_NOEXCEPT { return mData + Extent - 1; }
    const_reverse_iterator rbegin() const FL_NOEXCEPT { return mData + Extent - 1; }
    const_reverse_iterator crbegin() const FL_NOEXCEPT { return mData + Extent - 1; }

    reverse_iterator rend() FL_NOEXCEPT { return mData - 1; }
    const_reverse_iterator rend() const FL_NOEXCEPT { return mData - 1; }
    const_reverse_iterator crend() const FL_NOEXCEPT { return mData - 1; }

    // ======= SIZE AND ACCESS =======
    constexpr fl::size length() const FL_NOEXCEPT { return Extent; }
    constexpr fl::size size() const FL_NOEXCEPT { return Extent; }
    constexpr fl::size size_bytes() const FL_NOEXCEPT { return Extent * sizeof(T); }

    const T *data() const FL_NOEXCEPT { return mData; }
    T *data() FL_NOEXCEPT { return mData; }

    // ======= SUBVIEWS =======
    // Compile-time sized first N elements
    template<fl::size N>
    span<T, N> first() const FL_NOEXCEPT {
        return span<T, N>(mData);
    }

    // Runtime-sized first count elements
    span<T, dynamic_extent> first(fl::size count) const FL_NOEXCEPT {
        return span<T, dynamic_extent>(mData, count);
    }

    // Compile-time sized last N elements
    template<fl::size N>
    span<T, N> last() const FL_NOEXCEPT {
        return span<T, N>(mData + Extent - N);
    }

    // Runtime-sized last count elements
    span<T, dynamic_extent> last(fl::size count) const FL_NOEXCEPT {
        return span<T, dynamic_extent>(mData + Extent - count, count);
    }

    // Compile-time sized subspan
    template<fl::size Offset, fl::size Count = dynamic_extent>
    span<T, Count> subspan() const FL_NOEXCEPT {
        if (Count == dynamic_extent) {
            return span<T, Extent - Offset>(mData + Offset);
        }
        return span<T, Count>(mData + Offset);
    }

    // Runtime subspan
    span<T, dynamic_extent> subspan(fl::size offset, fl::size count = dynamic_extent) const FL_NOEXCEPT {
        if (count == dynamic_extent) {
            return span<T, dynamic_extent>(mData + offset, Extent - offset);
        }
        return span<T, dynamic_extent>(mData + offset, count);
    }

    T &front() FL_NOEXCEPT { return *mData; }
    const T &front() const FL_NOEXCEPT { return *mData; }

    T &back() FL_NOEXCEPT { return *(mData + Extent - 1); }
    const T &back() const FL_NOEXCEPT { return *(mData + Extent - 1); }

    constexpr bool empty() const FL_NOEXCEPT { return Extent == 0; }

    // ======= COMPARISON OPERATORS =======
    bool operator==(const span<T, Extent>& other) const FL_NOEXCEPT {
        for (fl::size i = 0; i < Extent; ++i) {
            if (!(mData[i] == other.mData[i])) return false;
        }
        return true;
    }

    bool operator!=(const span<T, Extent>& other) const FL_NOEXCEPT {
        return !(*this == other);
    }

    bool operator<(const span<T, Extent>& other) const FL_NOEXCEPT {
        for (fl::size i = 0; i < Extent; ++i) {
            if (mData[i] < other.mData[i]) return true;
            if (other.mData[i] < mData[i]) return false;
        }
        return false; // Equal when all elements are equal
    }

    bool operator<=(const span<T, Extent>& other) const FL_NOEXCEPT {
        return !(other < *this);
    }

    bool operator>(const span<T, Extent>& other) const FL_NOEXCEPT {
        return other < *this;
    }

    bool operator>=(const span<T, Extent>& other) const FL_NOEXCEPT {
        return !(*this < other);
    }

  private:
    T *mData;
    // Note: No mSize member - size is known at compile-time via Extent
};

// ======= BYTE VIEW CONVERSION FUNCTIONS =======
// Convert span to read-only byte view
template<typename T, fl::size Extent>
span<const fl::u8, (Extent == dynamic_extent) FL_NOEXCEPT ? dynamic_extent : (Extent * sizeof(T))>
as_bytes(const span<T, Extent>& s) {
    if (Extent == dynamic_extent) {
        return span<const fl::u8, dynamic_extent>(
            fl::bit_cast<const fl::u8*>(s.data()),
            s.size_bytes()
        );
    } else {
        return span<const fl::u8, Extent * sizeof(T)>(
            fl::bit_cast<const fl::u8*>(s.data())
        );
    }
}

// Convert span to writable byte view
template<typename T, fl::size Extent>
span<fl::u8, (Extent == dynamic_extent) FL_NOEXCEPT ? dynamic_extent : (Extent * sizeof(T))>
as_writable_bytes(span<T, Extent>& s) {
    if (Extent == dynamic_extent) {
        return span<fl::u8, dynamic_extent>(
            fl::bit_cast<fl::u8*>(s.data()),
            s.size_bytes()
        );
    } else {
        return span<fl::u8, Extent * sizeof(T)>(
            fl::bit_cast<fl::u8*>(s.data())
        );
    }
}

template <typename T> class MatrixSlice {
  public:
    // represents a window into a matrix
    // bottom-left and top-right corners are passed as plain ints
    MatrixSlice() FL_NOEXCEPT = default;
    MatrixSlice(T *data, i32 dataWidth, i32 dataHeight,
                i32 bottomLeftX, i32 bottomLeftY, i32 topRightX,
                i32 topRightY)
 FL_NOEXCEPT : mData(data), mDataWidth(dataWidth), mDataHeight(dataHeight),
          mBottomLeft{bottomLeftX, bottomLeftY},
          mTopRight{topRightX, topRightY} {}

    MatrixSlice(const MatrixSlice &other) FL_NOEXCEPT = default;
    MatrixSlice &operator=(const MatrixSlice &other) FL_NOEXCEPT = default;

    // outputs a vec2 but takes x,y as inputs
    vec2<i32> getParentCoord(i32 x_local, i32 y_local) const FL_NOEXCEPT {
        return {x_local + mBottomLeft.x, y_local + mBottomLeft.y};
    }

    vec2<i32> getLocalCoord(i32 x_world, i32 y_world) const FL_NOEXCEPT {
        // clamp to [mBottomLeft, mTopRight]
        i32 x_clamped = fl::clamp(x_world, mBottomLeft.x, mTopRight.x);
        i32 y_clamped = fl::clamp(y_world, mBottomLeft.y, mTopRight.y);
        // convert to local
        return {x_clamped - mBottomLeft.x, y_clamped - mBottomLeft.y};
    }

    // element access via (x,y)
    T &operator()(i32 x, i32 y) FL_NOEXCEPT { return at(x, y); }

    // Add access like slice[y][x]
    T *operator[](i32 row) FL_NOEXCEPT {
        i32 parentRow = row + mBottomLeft.y;
        return mData + parentRow * mDataWidth + mBottomLeft.x;
    }

    T &at(i32 x, i32 y) FL_NOEXCEPT {
        auto parent = getParentCoord(x, y);
        return mData[parent.x + parent.y * mDataWidth];
    }

    const T &at(i32 x, i32 y) const FL_NOEXCEPT {
        auto parent = getParentCoord(x, y);
        return mData[parent.x + parent.y * mDataWidth];
    }

  private:
    T *mData = nullptr;
    i32 mDataWidth = 0;
    i32 mDataHeight = 0;
    vec2<i32> mBottomLeft;
    vec2<i32> mTopRight;
};

} // namespace fl
