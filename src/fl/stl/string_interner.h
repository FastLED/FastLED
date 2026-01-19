#pragma once

// String interner: stores unique strings and returns handles for efficient
// comparison and deduplication. Interned strings are compared by pointer/index
// rather than character-by-character.
//
// Supports two types of interned strings:
// 1. Copied strings (default): The interner allocates memory and copies the string.
// 2. Persistent strings: The interner uses the pointer directly without copying.
//    Use this for string literals or other static/global data that outlives the interner.

#include "fl/stl/stdint.h"
#include "fl/stl/cstring.h"
#include "fl/stl/vector.h"
#include "fl/stl/unordered_map.h"
#include "fl/hash.h"
#include "fl/string_view.h"
#include "fl/int.h"
#include "fl/singleton.h"

namespace fl {

// Forward declarations
class StringInterner;

// InternedString: A lightweight handle to an interned string.
// Comparing two InternedStrings from the same interner is O(1) pointer comparison.
// The InternedString does NOT own the string data - the StringInterner does.
class InternedString {
  public:
    // Default constructor creates an invalid/empty interned string
    constexpr InternedString() : mData(nullptr), mSize(0), mId(0), mPersistent(false) {}

    // Check if this is a valid interned string
    constexpr bool valid() const { return mData != nullptr; }
    constexpr bool empty() const { return mSize == 0; }

    // Get the string data
    constexpr const char* data() const { return mData; }
    constexpr const char* c_str() const { return mData ? mData : ""; }
    constexpr fl::size size() const { return mSize; }
    constexpr fl::size length() const { return mSize; }

    // Get unique ID (can be used for efficient hashing/comparison)
    constexpr fl::u32 id() const { return mId; }

    // Check if this string is from persistent (non-owned) memory
    constexpr bool persistent() const { return mPersistent; }

    // Convert to string_view (zero-copy)
    string_view view() const { return string_view(mData, mSize); }

    // Comparison operators - O(1) when from the same interner
    bool operator==(const InternedString& other) const {
        // Fast path: same pointer means same string (from same interner)
        if (mData == other.mData) return true;
        // Slow path: different interners or one is invalid
        if (!mData || !other.mData) return false;
        if (mSize != other.mSize) return false;
        return fl::strcmp(mData, other.mData) == 0;
    }

    bool operator!=(const InternedString& other) const {
        return !(*this == other);
    }

    // Comparison with string_view
    bool operator==(const string_view& other) const {
        if (!mData) return other.empty();
        if (mSize != other.size()) return false;
        return fl::memcmp(mData, other.data(), mSize) == 0;
    }

    bool operator!=(const string_view& other) const {
        return !(*this == other);
    }

    // Comparison with C-string
    bool operator==(const char* other) const {
        if (!other) return !mData || mSize == 0;
        if (!mData) return other[0] == '\0';
        return fl::strcmp(mData, other) == 0;
    }

    bool operator!=(const char* other) const {
        return !(*this == other);
    }

    // Less-than for use in ordered containers (lexicographic, not ID-based)
    bool operator<(const InternedString& other) const {
        if (mData == other.mData) return false;
        if (!mData) return other.mData != nullptr;
        if (!other.mData) return false;
        return fl::strcmp(mData, other.mData) < 0;
    }

  private:
    friend class StringInterner;

    // Private constructor - only StringInterner can create InternedStrings
    constexpr InternedString(const char* data, fl::size size, fl::u32 id, bool persistent)
        : mData(data), mSize(size), mId(id), mPersistent(persistent) {}

    const char* mData;
    fl::size mSize;
    fl::u32 mId;  // Unique ID for this string within the interner
    bool mPersistent;  // True if data points to persistent memory (not owned by interner)
};

// Hash specialization for InternedString - uses the unique ID for fast hashing
template<>
struct Hash<InternedString> {
    fl::u32 operator()(const InternedString& key) const noexcept {
        // Use the ID for fast hashing since it's unique per string
        return fast_hash32(key.id());
    }
};

// StringInterner: Manages a pool of unique strings.
// Thread-safety: NOT thread-safe. Synchronize externally if needed.
class StringInterner {
  public:
    StringInterner() : mNextId(1) {}  // Start IDs at 1, 0 is reserved for invalid

    ~StringInterner() {
        clear();
    }

    // Intern a string (copies the data) - returns an InternedString handle.
    // If the string already exists, returns the existing handle.
    // The returned InternedString is valid as long as the interner exists.
    InternedString intern(const char* str) {
        if (!str) return InternedString();
        return intern(str, fl::strlen(str));
    }

    InternedString intern(const char* str, fl::size len) {
        if (!str || len == 0) return InternedString();

        // Create a temporary view for lookup
        string_view view(str, len);

        // Check if already interned
        auto it = mStringToEntry.find(view);
        if (it != mStringToEntry.end()) {
            const Entry& entry = mEntries[it->second];
            return InternedString(entry.data, entry.size, entry.id, entry.persistent);
        }

        // Not found - intern new string (copy the data)
        return internNew(str, len, false);
    }

    InternedString intern(const string_view& sv) {
        return intern(sv.data(), sv.size());
    }

    template <fl::size SIZE>
    InternedString intern(const StrN<SIZE>& str) {
        return intern(str.c_str(), str.size());
    }

    // Intern a persistent string (does NOT copy the data).
    // Use this for string literals or other static/global data that outlives the interner.
    // WARNING: The caller must ensure the string data remains valid for the lifetime of the interner.
    InternedString internPersistent(const char* str) {
        if (!str) return InternedString();
        return internPersistent(str, fl::strlen(str));
    }

    InternedString internPersistent(const char* str, fl::size len) {
        if (!str || len == 0) return InternedString();

        // Create a temporary view for lookup
        string_view view(str, len);

        // Check if already interned
        auto it = mStringToEntry.find(view);
        if (it != mStringToEntry.end()) {
            const Entry& entry = mEntries[it->second];
            return InternedString(entry.data, entry.size, entry.id, entry.persistent);
        }

        // Not found - intern new string (persistent - no copy)
        return internNew(str, len, true);
    }

    InternedString internPersistent(const string_view& sv) {
        return internPersistent(sv.data(), sv.size());
    }

    // Get an interned string by ID (O(1) lookup)
    // Returns invalid InternedString if ID not found
    InternedString get(fl::u32 id) const {
        if (id == 0 || id > mEntries.size()) return InternedString();
        const Entry& entry = mEntries[id - 1];  // IDs are 1-based
        return InternedString(entry.data, entry.size, entry.id, entry.persistent);
    }

    // Check if a string is already interned
    bool contains(const char* str) const {
        if (!str) return false;
        return contains(string_view(str));
    }

    bool contains(const string_view& sv) const {
        return mStringToEntry.find(sv) != mStringToEntry.end();
    }

    // Get the number of unique interned strings
    fl::size size() const { return mEntries.size(); }

    // Check if the interner is empty
    bool empty() const { return mEntries.empty(); }

    // Clear all interned strings
    void clear() {
        // Free all allocated (non-persistent) string data
        for (fl::size i = 0; i < mEntries.size(); ++i) {
            if (mEntries[i].data && !mEntries[i].persistent) {
                char* ptr = const_cast<char*>(mEntries[i].data);
                delete[] ptr;
            }
        }
        mEntries.clear();
        mStringToEntry.clear();
        mNextId = 1;
    }

    // Reserve capacity for expected number of strings
    void reserve(fl::size count) {
        mEntries.reserve(count);
        mStringToEntry.reserve(count);
    }

  private:
    // Internal entry storing the string data
    struct Entry {
        const char* data;
        fl::size size;
        fl::u32 id;
        bool persistent;  // True if data is not owned by the interner

        Entry() : data(nullptr), size(0), id(0), persistent(false) {}
        Entry(const char* d, fl::size s, fl::u32 i, bool p) : data(d), size(s), id(i), persistent(p) {}
    };

    // Hash functor for string_view in the lookup map
    struct StringViewHash {
        fl::u32 operator()(const string_view& sv) const noexcept {
            return MurmurHash3_x86_32(sv.data(), sv.size());
        }
    };

    // Equality functor for string_view in the lookup map
    struct StringViewEqual {
        bool operator()(const string_view& a, const string_view& b) const {
            if (a.size() != b.size()) return false;
            return fl::memcmp(a.data(), b.data(), a.size()) == 0;
        }
    };

    InternedString internNew(const char* str, fl::size len, bool persistent) {
        const char* data;

        if (persistent) {
            // Use the pointer directly - caller guarantees lifetime
            data = str;
        } else {
            // Allocate and copy the string data
            char* allocated = new char[len + 1];
            fl::memcpy(allocated, str, len);
            allocated[len] = '\0';
            data = allocated;
        }

        // Create entry
        fl::u32 id = mNextId++;
        Entry entry(data, len, id, persistent);
        mEntries.push_back(entry);

        // Add to lookup map (use the stored data pointer for the view)
        string_view stored_view(data, len);
        mStringToEntry.insert(stored_view, mEntries.size() - 1);

        return InternedString(data, len, id, persistent);
    }

    fl::vector<Entry> mEntries;  // All interned strings
    fl::unordered_map<string_view, fl::size, StringViewHash, StringViewEqual> mStringToEntry;
    fl::u32 mNextId;
};

// Global string interner singleton (optional convenience)
// Uses fl::Singleton to ensure the destructor is never called, avoiding
// static destruction order issues and unnecessary cleanup in embedded systems.
// Usage: fl::global_interner().intern("my string")
inline StringInterner& global_interner() {
    return Singleton<StringInterner>::instance();
}

// Convenience function for global interning (copies data)
inline InternedString intern(const char* str) {
    return global_interner().intern(str);
}

inline InternedString intern(const string_view& sv) {
    return global_interner().intern(sv);
}

// Convenience function for global interning of persistent strings (no copy)
inline InternedString internPersistent(const char* str) {
    return global_interner().internPersistent(str);
}

inline InternedString internPersistent(const string_view& sv) {
    return global_interner().internPersistent(sv);
}

} // namespace fl
