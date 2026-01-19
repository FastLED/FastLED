#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/cstring.h"
#include "fl/stl/type_traits.h"

namespace fl {

// Forward declarations
template <fl::size SIZE> class StrN;
class string;

// string_view: A non-owning view into a contiguous sequence of characters
// Provides std::string_view-like functionality for embedded environments
// Does NOT allocate memory or manage ownership
// Similar to span<const char> but with string-specific operations
class string_view {
  public:
    // ======= STANDARD CONTAINER TYPE ALIASES =======
    using element_type = const char;
    using value_type = char;
    using size_type = fl::size;
    using difference_type = fl::i32;
    using pointer = const char*;
    using const_pointer = const char*;
    using reference = const char&;
    using const_reference = const char&;
    using iterator = const char*;
    using const_iterator = const char*;
    using reverse_iterator = const char*;
    using const_reverse_iterator = const char*;

    // Special value to indicate "not found" or "until end"
    // Using constexpr for C++11 implicit inline semantics - no ODR definition needed
    static constexpr fl::size npos = static_cast<fl::size>(-1);

    // ======= CONSTRUCTORS =======
    // Default constructor - empty view
    constexpr string_view() : mData(nullptr), mSize(0) {}

    // Constructor from null-terminated C string
    constexpr string_view(const char* str)
        : mData(str), mSize(str ? strlen(str) : 0) {}

    // Constructor from pointer and length
    constexpr string_view(const char* str, fl::size len)
        : mData(str), mSize(len) {}

    // Constructor from C-array
    template <fl::size N>
    constexpr string_view(const char (&arr)[N])
        : mData(arr), mSize(N - 1) {}  // Subtract 1 for null terminator

    // Constructor from fl::string
    template <fl::size SIZE>
    string_view(const StrN<SIZE>& str)
        : mData(str.c_str()), mSize(str.size()) {}

    // Copy constructor
    string_view(const string_view& other) = default;

    // Assignment operator
    string_view& operator=(const string_view& other) = default;

    // ======= ITERATORS =======
    constexpr iterator begin() const { return mData; }
    constexpr iterator end() const { return mData + mSize; }
    constexpr const_iterator cbegin() const { return mData; }
    constexpr const_iterator cend() const { return mData + mSize; }

    constexpr reverse_iterator rbegin() const { return mData + mSize - 1; }
    constexpr reverse_iterator rend() const { return mData - 1; }
    constexpr const_reverse_iterator crbegin() const { return mData + mSize - 1; }
    constexpr const_reverse_iterator crend() const { return mData - 1; }

    // ======= ELEMENT ACCESS =======
    constexpr const char& operator[](fl::size index) const {
        // No bounds checking in embedded environment
        return mData[index];
    }

    constexpr const char& at(fl::size index) const {
        // Basic bounds checking
        return (index < mSize) ? mData[index] : mData[0];
    }

    constexpr const char& front() const {
        return mData[0];
    }

    constexpr const char& back() const {
        return mData[mSize - 1];
    }

    constexpr const char* data() const {
        return mData;
    }

    // ======= CAPACITY =======
    constexpr fl::size size() const { return mSize; }
    constexpr fl::size length() const { return mSize; }
    constexpr fl::size max_size() const { return npos - 1; }
    constexpr bool empty() const { return mSize == 0; }

    // ======= MODIFIERS (modify the view, not the data) =======
    void remove_prefix(fl::size n) {
        if (n > mSize) n = mSize;
        mData += n;
        mSize -= n;
    }

    void remove_suffix(fl::size n) {
        if (n > mSize) n = mSize;
        mSize -= n;
    }

    void swap(string_view& other) {
        const char* tmp_data = mData;
        fl::size tmp_size = mSize;
        mData = other.mData;
        mSize = other.mSize;
        other.mData = tmp_data;
        other.mSize = tmp_size;
    }

    // ======= STRING OPERATIONS =======
    // Copy substring to buffer
    fl::size copy(char* dest, fl::size count, fl::size pos = 0) const {
        if (!dest || pos >= mSize) {
            return 0;
        }
        fl::size actual_count = count;
        if (actual_count > mSize - pos) {
            actual_count = mSize - pos;
        }
        if (actual_count > 0) {
            fl::memcpy(dest, mData + pos, actual_count);
        }
        return actual_count;
    }

    // Substring operations
    string_view substr(fl::size pos = 0, fl::size count = npos) const {
        if (pos >= mSize) {
            return string_view();
        }
        fl::size actual_count = count;
        if (actual_count == npos || pos + actual_count > mSize) {
            actual_count = mSize - pos;
        }
        return string_view(mData + pos, actual_count);
    }

    // ======= COMPARISON OPERATIONS =======
    int compare(string_view other) const {
        fl::size min_len = (mSize < other.mSize) ? mSize : other.mSize;
        for (fl::size i = 0; i < min_len; ++i) {
            if (mData[i] < other.mData[i]) return -1;
            if (mData[i] > other.mData[i]) return 1;
        }
        if (mSize < other.mSize) return -1;
        if (mSize > other.mSize) return 1;
        return 0;
    }

    int compare(fl::size pos1, fl::size count1, string_view other) const {
        return substr(pos1, count1).compare(other);
    }

    int compare(fl::size pos1, fl::size count1, string_view other,
                         fl::size pos2, fl::size count2) const {
        return substr(pos1, count1).compare(other.substr(pos2, count2));
    }

    int compare(const char* s) const {
        return compare(string_view(s));
    }

    int compare(fl::size pos1, fl::size count1, const char* s) const {
        return substr(pos1, count1).compare(string_view(s));
    }

    int compare(fl::size pos1, fl::size count1, const char* s, fl::size count2) const {
        return substr(pos1, count1).compare(string_view(s, count2));
    }

    // ======= SEARCH OPERATIONS =======
    // Find character
    fl::size find(char ch, fl::size pos = 0) const {
        if (pos >= mSize) return npos;
        for (fl::size i = pos; i < mSize; ++i) {
            if (mData[i] == ch) {
                return i;
            }
        }
        return npos;
    }

    // Find substring
    fl::size find(string_view sv, fl::size pos = 0) const {
        if (sv.empty()) return pos;
        if (pos >= mSize || sv.mSize > mSize - pos) return npos;

        for (fl::size i = pos; i <= mSize - sv.mSize; ++i) {
            bool match = true;
            for (fl::size j = 0; j < sv.mSize; ++j) {
                if (mData[i + j] != sv.mData[j]) {
                    match = false;
                    break;
                }
            }
            if (match) return i;
        }
        return npos;
    }

    fl::size find(const char* s, fl::size pos, fl::size count) const {
        return find(string_view(s, count), pos);
    }

    fl::size find(const char* s, fl::size pos = 0) const {
        return find(string_view(s), pos);
    }

    // Reverse find
    fl::size rfind(char ch, fl::size pos = npos) const {
        if (mSize == 0) return npos;
        fl::size search_pos = (pos >= mSize || pos == npos) ? (mSize - 1) : pos;
        for (fl::size i = search_pos + 1; i > 0; --i) {
            if (mData[i - 1] == ch) {
                return i - 1;
            }
        }
        return npos;
    }

    fl::size rfind(string_view sv, fl::size pos = npos) const {
        if (sv.empty()) return (pos > mSize) ? mSize : pos;
        if (sv.mSize > mSize) return npos;

        fl::size max_start = mSize - sv.mSize;
        fl::size search_start = (pos >= mSize || pos == npos) ? max_start : pos;
        if (search_start + sv.mSize > mSize) {
            search_start = max_start;
        }

        for (fl::size i = search_start + 1; i > 0; --i) {
            fl::size idx = i - 1;
            if (idx + sv.mSize > mSize) continue;
            bool match = true;
            for (fl::size j = 0; j < sv.mSize; ++j) {
                if (mData[idx + j] != sv.mData[j]) {
                    match = false;
                    break;
                }
            }
            if (match) return idx;
        }
        return npos;
    }

    fl::size rfind(const char* s, fl::size pos, fl::size count) const {
        return rfind(string_view(s, count), pos);
    }

    fl::size rfind(const char* s, fl::size pos = npos) const {
        return rfind(string_view(s), pos);
    }

    // Find first of any character in set
    fl::size find_first_of(string_view sv, fl::size pos = 0) const {
        if (pos >= mSize || sv.empty()) return npos;
        for (fl::size i = pos; i < mSize; ++i) {
            for (fl::size j = 0; j < sv.mSize; ++j) {
                if (mData[i] == sv.mData[j]) {
                    return i;
                }
            }
        }
        return npos;
    }

    fl::size find_first_of(char ch, fl::size pos = 0) const {
        return find(ch, pos);
    }

    fl::size find_first_of(const char* s, fl::size pos, fl::size count) const {
        return find_first_of(string_view(s, count), pos);
    }

    fl::size find_first_of(const char* s, fl::size pos = 0) const {
        return find_first_of(string_view(s), pos);
    }

    // Find last of any character in set
    fl::size find_last_of(string_view sv, fl::size pos = npos) const {
        if (mSize == 0 || sv.empty()) return npos;
        fl::size search_pos = (pos >= mSize || pos == npos) ? (mSize - 1) : pos;
        for (fl::size i = search_pos + 1; i > 0; --i) {
            for (fl::size j = 0; j < sv.mSize; ++j) {
                if (mData[i - 1] == sv.mData[j]) {
                    return i - 1;
                }
            }
        }
        return npos;
    }

    fl::size find_last_of(char ch, fl::size pos = npos) const {
        return rfind(ch, pos);
    }

    fl::size find_last_of(const char* s, fl::size pos, fl::size count) const {
        return find_last_of(string_view(s, count), pos);
    }

    fl::size find_last_of(const char* s, fl::size pos = npos) const {
        return find_last_of(string_view(s), pos);
    }

    // Find first not of any character in set
    fl::size find_first_not_of(string_view sv, fl::size pos = 0) const {
        if (pos >= mSize) return npos;
        for (fl::size i = pos; i < mSize; ++i) {
            bool found = false;
            for (fl::size j = 0; j < sv.mSize; ++j) {
                if (mData[i] == sv.mData[j]) {
                    found = true;
                    break;
                }
            }
            if (!found) return i;
        }
        return npos;
    }

    fl::size find_first_not_of(char ch, fl::size pos = 0) const {
        if (pos >= mSize) return npos;
        for (fl::size i = pos; i < mSize; ++i) {
            if (mData[i] != ch) return i;
        }
        return npos;
    }

    fl::size find_first_not_of(const char* s, fl::size pos, fl::size count) const {
        return find_first_not_of(string_view(s, count), pos);
    }

    fl::size find_first_not_of(const char* s, fl::size pos = 0) const {
        return find_first_not_of(string_view(s), pos);
    }

    // Find last not of any character in set
    fl::size find_last_not_of(string_view sv, fl::size pos = npos) const {
        if (mSize == 0) return npos;
        fl::size search_pos = (pos >= mSize || pos == npos) ? (mSize - 1) : pos;
        for (fl::size i = search_pos + 1; i > 0; --i) {
            bool found = false;
            for (fl::size j = 0; j < sv.mSize; ++j) {
                if (mData[i - 1] == sv.mData[j]) {
                    found = true;
                    break;
                }
            }
            if (!found) return i - 1;
        }
        return npos;
    }

    fl::size find_last_not_of(char ch, fl::size pos = npos) const {
        if (mSize == 0) return npos;
        fl::size search_pos = (pos >= mSize || pos == npos) ? (mSize - 1) : pos;
        for (fl::size i = search_pos + 1; i > 0; --i) {
            if (mData[i - 1] != ch) return i - 1;
        }
        return npos;
    }

    fl::size find_last_not_of(const char* s, fl::size pos, fl::size count) const {
        return find_last_not_of(string_view(s, count), pos);
    }

    fl::size find_last_not_of(const char* s, fl::size pos = npos) const {
        return find_last_not_of(string_view(s), pos);
    }

    // ======= C++20/C++23 CONVENIENCE METHODS =======
    bool starts_with(string_view sv) const {
        if (sv.mSize > mSize) return false;
        for (fl::size i = 0; i < sv.mSize; ++i) {
            if (mData[i] != sv.mData[i]) return false;
        }
        return true;
    }

    bool starts_with(char ch) const {
        return !empty() && mData[0] == ch;
    }

    bool starts_with(const char* s) const {
        return starts_with(string_view(s));
    }

    bool ends_with(string_view sv) const {
        if (sv.mSize > mSize) return false;
        for (fl::size i = 0; i < sv.mSize; ++i) {
            if (mData[mSize - sv.mSize + i] != sv.mData[i]) return false;
        }
        return true;
    }

    bool ends_with(char ch) const {
        return !empty() && mData[mSize - 1] == ch;
    }

    bool ends_with(const char* s) const {
        return ends_with(string_view(s));
    }

    bool contains(string_view sv) const {
        return find(sv) != npos;
    }

    bool contains(char ch) const {
        return find(ch) != npos;
    }

    bool contains(const char* s) const {
        return find(s) != npos;
    }

  private:
    const char* mData;
    fl::size mSize;
};

// ======= COMPARISON OPERATORS =======
inline bool operator==(string_view lhs, string_view rhs) {
    return lhs.compare(rhs) == 0;
}

inline bool operator!=(string_view lhs, string_view rhs) {
    return lhs.compare(rhs) != 0;
}

inline bool operator<(string_view lhs, string_view rhs) {
    return lhs.compare(rhs) < 0;
}

inline bool operator<=(string_view lhs, string_view rhs) {
    return lhs.compare(rhs) <= 0;
}

inline bool operator>(string_view lhs, string_view rhs) {
    return lhs.compare(rhs) > 0;
}

inline bool operator>=(string_view lhs, string_view rhs) {
    return lhs.compare(rhs) >= 0;
}

// ======= OUTPUT HELPER =======
// Inline hash function for string_view
inline fl::size hash_string_view(string_view sv) {
    // FNV-1a hash algorithm
    fl::size hash = 2166136261u;
    for (fl::size i = 0; i < sv.size(); ++i) {
        hash ^= static_cast<fl::size>(sv[i]);
        hash *= 16777619u;
    }
    return hash;
}

} // namespace fl
