#include "fl/stl/string_interner.h"
#include "fl/singleton.h"
#include "fl/int.h"
#include "fl/stl/string.h"
#include "fl/stl/detail/string_holder.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/unordered_map.h"
#include "fl/string_view.h"
#include "fl/stl/span.h"
#include "fl/stl/mutex.h"

namespace fl {

// Mutex for global interner only (instance interners are single-threaded)
static fl::mutex& global_interner_mutex() {
    static fl::mutex mtx;
    return mtx;
}

// StringInterner member function implementations

StringInterner::StringInterner() {}

StringInterner::~StringInterner() {
    clear();
}

fl::string StringInterner::intern(const string_view& sv) {
    if (sv.empty()) return fl::string();

    // SSO optimization: strings that fit in the inline buffer don't benefit from interning
    // because fl::string will store them inline (no heap allocation anyway).
    // Interning small strings would just add hash overhead with no memory savings.
    if (sv.size() <= FASTLED_STR_INLINED_SIZE) {
        return fl::string(sv);
    }

    // String is large enough to require heap allocation - check if already interned
    // Try to find existing entry - O(1) average via hash map
    auto it = mEntries.find(sv);
    if (it != mEntries.end()) {
        // Found existing - return fl::string sharing the StringHolder
        return fl::string(it->second);
    }

    // Not found - create new StringHolder (heap-allocated)
    auto holder = fl::make_shared<StringHolder>(sv.data(), sv.size());

    // Create string_view key that points into holder's data
    // This is safe because StringHolder data is heap-allocated and never moves
    string_view key(holder->data(), holder->length());

    // Insert into map - key points into value's data (self-referential)
    mEntries[key] = holder;

    // Return fl::string sharing the StringHolder
    return fl::string(holder);
}

fl::string StringInterner::intern(const fl::string& str) {
    // Convert to string_view and delegate
    return intern(string_view(str.c_str(), str.size()));
}

fl::string StringInterner::intern(const char* str) {
    if (!str) return fl::string();
    // Convert to string_view and delegate
    return intern(string_view(str));
}

fl::string StringInterner::intern(const fl::span<const char>& sp) {
    if (sp.empty()) return fl::string();
    // Convert to string_view and delegate
    return intern(string_view(sp.data(), sp.size()));
}

bool StringInterner::contains(const string_view& sv) const {
    return mEntries.find(sv) != mEntries.end();
}

bool StringInterner::contains(const char* str) const {
    if (!str) return false;
    return contains(string_view(str));
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
    // unordered_map doesn't have reserve(), but we can set the bucket count
    // This pre-allocates buckets to avoid rehashing
    if (count > 0) {
        mEntries.rehash(count);
    }
}

// Global string interner singleton implementation
StringInterner& global_interner() {
    return Singleton<StringInterner>::instance();
}

// Convenience functions for global interning (thread-safe via mutex)
fl::string intern(const string_view& sv) {
    fl::unique_lock<fl::mutex> lock(global_interner_mutex());
    return global_interner().intern(sv);
}

fl::string intern(const fl::string& str) {
    fl::unique_lock<fl::mutex> lock(global_interner_mutex());
    return global_interner().intern(str);
}

fl::string intern(const char* str) {
    fl::unique_lock<fl::mutex> lock(global_interner_mutex());
    return global_interner().intern(str);
}

fl::string intern(const fl::span<const char>& sp) {
    fl::unique_lock<fl::mutex> lock(global_interner_mutex());
    return global_interner().intern(sp);
}

} // namespace fl
