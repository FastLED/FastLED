#include "fl/stl/string_interner.h"
#include "fl/singleton.h"
#include "fl/int.h"
#include "fl/stl/cstring.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "fl/string_view.h"

namespace fl {

// StringInterner member function implementations

StringInterner::StringInterner() {}

StringInterner::~StringInterner() {
    clear();
}

fl::string StringInterner::intern(const char* str) {
    if (!str) return fl::string();
    return intern(str, fl::strlen(str));
}

fl::string StringInterner::intern(const char* str, fl::size len) {
    if (!str || len == 0) return fl::string();

    // Linear search - simple and works reliably for small N
    // String interners typically have <1000 entries, so O(N) is acceptable
    for (const fl::string& entry : mEntries) {
        if (entry.size() == len &&
            fl::memcmp(entry.c_str(), str, len) == 0) {
            return entry;  // Return existing (cheap copy via shared_ptr)
        }
    }

    // Use interned() to force heap allocation for stable pointers
    // This ensures all copies share the same StringHolder via shared_ptr
    fl::string interned = fl::string::interned(str, len);

    // Add to vector
    mEntries.push_back(interned);

    return interned;
}

fl::string StringInterner::intern(const string_view& sv) {
    return intern(sv.data(), sv.size());
}

fl::string StringInterner::get(fl::size index) const {
    if (index >= mEntries.size()) return fl::string();
    return mEntries[index];  // Cheap shared_ptr copy
}

bool StringInterner::contains(const char* str) const {
    if (!str) return false;
    return contains(string_view(str));
}

bool StringInterner::contains(const string_view& sv) const {
    for (const fl::string& entry : mEntries) {
        if (entry.size() == sv.size() &&
            fl::memcmp(entry.c_str(), sv.data(), sv.size()) == 0) {
            return true;
        }
    }
    return false;
}

fl::size StringInterner::size() const {
    return mEntries.size();
}

bool StringInterner::empty() const {
    return mEntries.empty();
}

void StringInterner::clear() {
    mEntries.clear();
}

void StringInterner::reserve(fl::size count) {
    mEntries.reserve(count);
}

// Global string interner singleton implementation
StringInterner& global_interner() {
    return Singleton<StringInterner>::instance();
}

// Convenience functions for global interning
fl::string intern(const char* str) {
    return global_interner().intern(str);
}

fl::string intern(const string_view& sv) {
    return global_interner().intern(sv);
}

} // namespace fl
