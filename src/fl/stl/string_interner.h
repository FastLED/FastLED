#pragma once

// String interner: stores unique strings as heap-allocated fl::string instances
// for efficient comparison and deduplication. Interned strings are compared by
// pointer rather than character-by-character, and use reference-counted
// StringHolder for efficient copying.
//
// All interned strings are heap-allocated to ensure stable pointers.
// Copying interned strings is cheap (shared_ptr increment).
// Outstanding strings survive interner.clear() due to reference counting.

#include "fl/int.h"
#include "fl/stl/vector.h"

namespace fl {

class string;
class string_view;

// StringInterner: Manages a pool of unique strings as fl::string instances.
// Interned strings are heap-allocated and reference-counted, making copies cheap.
// Thread-safety: NOT thread-safe. Synchronize externally if needed.
class StringInterner {
  public:
    StringInterner();
    ~StringInterner();

    // Intern a string (copies the data) - returns a fl::string with heap allocation.
    // If the string already exists, returns the existing fl::string (cheap copy).
    // The returned fl::string is reference-counted via shared_ptr.
    // All interned strings use heap allocation to ensure stable pointers.
    fl::string intern(const fl::string& str);

    // Get an interned string by index (O(1) lookup)
    // Returns empty fl::string if index is out of bounds
    fl::string get(fl::size index) const;

    // Check if a string is already interned
    bool contains(const char* str) const;
    bool contains(const string_view& sv) const;

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
    fl::vector<fl::string> mEntries;  // All interned strings (heap-allocated)
};

// Global string interner singleton (optional convenience)
// Uses fl::Singleton to ensure the destructor is never called, avoiding
// static destruction order issues and unnecessary cleanup in embedded systems.
// Usage: fl::global_interner().intern("my string")
StringInterner& global_interner();

// Convenience function for global interning
// Returns heap-allocated fl::string with stable pointer (cheap to copy via shared_ptr)
// NOTE: Input is always deep-copied to ensure memory safety
fl::string intern(const fl::string& str);

} // namespace fl
