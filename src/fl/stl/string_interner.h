#pragma once

// String interner: stores unique strings as heap-allocated fl::string instances
// for efficient comparison and deduplication. Uses a hash map for O(1) average
// lookup time instead of linear search.
//
// Implementation: unordered_map<string_view, shared_ptr<StringHolder>>
// The string_view keys point into the StringHolder values' data (self-referential).
// This is safe because StringHolder data is heap-allocated and never moves.
//
// All interned strings are heap-allocated to ensure stable pointers.
// Copying interned strings is cheap (shared_ptr increment).
// Outstanding strings survive interner.clear() due to reference counting.

#include "fl/int.h"
#include "fl/stl/unordered_map.h"
#include "fl/stl/shared_ptr.h"

namespace fl {

class string;
class string_view;
class StringHolder;
template <typename T, fl::size Extent> class span;

// StringInterner: Manages a pool of unique strings as fl::string instances.
// Interned strings are heap-allocated and reference-counted, making copies cheap.
// Uses hash map for O(1) average lookup time (vs O(N) linear search).
// Thread-safety: NOT thread-safe. Synchronize externally if needed.
class StringInterner {
  public:
    StringInterner();
    ~StringInterner();

    // Intern a string_view (primary API) - returns a fl::string with heap allocation.
    // If the string already exists, returns the existing fl::string (cheap shared_ptr copy).
    // The returned fl::string is reference-counted via shared_ptr.
    // All interned strings use heap allocation to ensure stable pointers.
    // Performance: O(1) average time complexity via hash map lookup.
    fl::string intern(const string_view& sv);

    // Convenience overload: intern from fl::string
    fl::string intern(const fl::string& str);

    // Convenience overload: intern from C string
    fl::string intern(const char* str);

    // Convenience overload: intern from span<const char>
    fl::string intern(const fl::span<const char>& sp);

    // Check if a string is already interned
    // Performance: O(1) average time complexity via hash map lookup.
    bool contains(const string_view& sv) const;
    bool contains(const char* str) const;

    // Get the number of unique interned strings
    fl::size size() const;

    // Check if the interner is empty
    bool empty() const;

    // Clear all interned strings
    // Outstanding fl::string copies survive due to shared_ptr reference counting
    void clear();

    // Reserve capacity for expected number of strings
    void reserve(fl::size count);

  private:
    using StringHolderPtr = fl::shared_ptr<StringHolder>;
    fl::unordered_map<string_view, StringHolderPtr> mEntries;  // O(1) average lookup
};

// Global string interner singleton (optional convenience)
// Uses fl::Singleton to ensure the destructor is never called, avoiding
// static destruction order issues and unnecessary cleanup in embedded systems.
// Usage: fl::global_interner().intern("my string")
StringInterner& global_interner();

// Convenience functions for global interning
// Returns heap-allocated fl::string with stable pointer (cheap to copy via shared_ptr)
// Performance: O(1) average time complexity via hash map lookup.
fl::string intern(const string_view& sv);
fl::string intern(const fl::string& str);
fl::string intern(const char* str);
fl::string intern(const fl::span<const char>& sp);

} // namespace fl
