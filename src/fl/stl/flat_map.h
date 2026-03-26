#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/assert.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/assert.h"  // IWYU pragma: keep
#include "fl/stl/comparators.h"
#include "fl/stl/pair.h"
#include "fl/stl/vector.h"
#include "fl/stl/allocator.h"
#include "fl/stl/memory_resource.h"
#include "platforms/assert_defs.h"
#include "fl/stl/noexcept.h"

namespace fl {

// A cache-friendly sorted map using contiguous storage.
//
// flat_map stores key-value pairs in a sorted vector for optimal memory layout.
// This provides excellent cache locality and iteration performance, but slower
// insertions and deletions (O(n)) compared to tree-based maps. Use flat_map when:
// - You have frequent lookups and iteration (better cache locality)
// - You don't do many insertions/deletions
// - Memory layout matters for performance
//
// The container maintains sorted order by key and uses binary search for lookups.
template <typename Key, typename Value,
          typename Less = fl::less<Key>>
class flat_map {
  public:
    enum insert_result { inserted = 0, exists = 1, at_capacity = 2 };

    using key_type = Key;
    using mapped_type = Value;
    using value_type = fl::pair<Key, Value>;
    using size_type = fl::size;
    using difference_type = ptrdiff_t;
    using key_compare = Less;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;

    using vector_type = fl::vector<value_type>;
    using iterator = typename vector_type::iterator;
    using const_iterator = typename vector_type::const_iterator;
    using reverse_iterator = typename vector_type::reverse_iterator;
    using const_reverse_iterator = typename vector_type::const_reverse_iterator;

  private:
    vector_type mData;
    Less mLess;

  public:
    // Constructors
    flat_map() FL_NOEXCEPT = default;

    explicit flat_map(memory_resource* resource) FL_NOEXCEPT
        : mData(resource) {}

    explicit flat_map(const Less& less) FL_NOEXCEPT
        : mLess(less) {}

    flat_map(const Less& less, memory_resource* resource) FL_NOEXCEPT
        : mData(resource), mLess(less) {}

    flat_map(const flat_map& other) FL_NOEXCEPT = default;
    flat_map& operator=(const flat_map& other) FL_NOEXCEPT = default;

    flat_map(flat_map&& other) FL_NOEXCEPT
        : mData(fl::move(other.mData)), mLess(fl::move(other.mLess)) {}

    flat_map& operator=(flat_map&& other) FL_NOEXCEPT {
        if (this != &other) {
            mData = fl::move(other.mData);
            mLess = fl::move(other.mLess);
        }
        return *this;
    }

    // Iterators
    iterator begin() FL_NOEXCEPT { return mData.begin(); }
    iterator end() FL_NOEXCEPT { return mData.end(); }
    const_iterator begin() const FL_NOEXCEPT { return mData.begin(); }
    const_iterator end() const FL_NOEXCEPT { return mData.end(); }
    const_iterator cbegin() const FL_NOEXCEPT { return mData.begin(); }
    const_iterator cend() const FL_NOEXCEPT { return mData.end(); }

    reverse_iterator rbegin() FL_NOEXCEPT { return mData.rbegin(); }
    reverse_iterator rend() FL_NOEXCEPT { return mData.rend(); }
    const_reverse_iterator rbegin() const FL_NOEXCEPT { return mData.rbegin(); }
    const_reverse_iterator rend() const FL_NOEXCEPT { return mData.rend(); }

    // Capacity
    size_type size() const FL_NOEXCEPT { return mData.size(); }
    bool empty() const FL_NOEXCEPT { return mData.empty(); }
    size_type capacity() const FL_NOEXCEPT { return mData.capacity(); }
    size_type max_size() const FL_NOEXCEPT { return mData.max_size(); }
    bool full() const FL_NOEXCEPT { return size() >= capacity(); }

    // Element access - check keys are equal (not less than in either direction)
    Value& operator[](const Key& key) FL_NOEXCEPT {
        auto it = lower_bound(key);
        if (it != end() && !mLess(key, it->first) && !mLess(it->first, key)) {
            return it->second;
        }
        // Key not found, insert it with default value
        bool success = mData.insert(it, value_type(key, Value()));
        FASTLED_ASSERT(success, "Insert failed in flat_map::operator[]");
        // Find the newly inserted element
        it = find(key);
        return it->second;
    }

    Value& at(const Key& key) FL_NOEXCEPT {
        auto it = lower_bound(key);
        if (it != end() && !mLess(key, it->first) && !mLess(it->first, key)) {
            return it->second;
        }
        // Key not found - could throw, but FastLED style is to assert
        FASTLED_ASSERT(false, "Key not found in flat_map");
        return mData.front().second;  // unreachable
    }

    const Value& at(const Key& key) const FL_NOEXCEPT {
        auto it = lower_bound(key);
        if (it != end() && !mLess(key, it->first) && !mLess(it->first, key)) {
            return it->second;
        }
        FASTLED_ASSERT(false, "Key not found in flat_map");
        return mData.front().second;  // unreachable
    }

    // Lookup
    iterator find(const Key& key) FL_NOEXCEPT {
        auto it = lower_bound(key);
        if (it != end() && !mLess(key, it->first) && !mLess(it->first, key)) {
            return it;
        }
        return end();
    }

    const_iterator find(const Key& key) const FL_NOEXCEPT {
        auto it = lower_bound(key);
        if (it != end() && !mLess(key, it->first) && !mLess(it->first, key)) {
            return it;
        }
        return end();
    }

    size_type count(const Key& key) const FL_NOEXCEPT {
        return find(key) != end() ? 1 : 0;
    }

    bool contains(const Key& key) const FL_NOEXCEPT {
        return find(key) != end();
    }

    // Alias for contains (FastLED compatibility)
    bool has(const Key& key) const FL_NOEXCEPT {
        return contains(key);
    }

    // Bounds - binary search using pointer arithmetic
    iterator lower_bound(const Key& key) FL_NOEXCEPT {
        // Binary search: find first element where !(element < key)
        iterator first = mData.begin();
        size_type count = mData.size();

        while (count > 0) {
            size_type step = count / 2;
            iterator it = first + step;
            if (mLess(it->first, key)) {
                first = it + 1;
                count -= step + 1;
            } else {
                count = step;
            }
        }
        return first;
    }

    const_iterator lower_bound(const Key& key) const FL_NOEXCEPT {
        // Binary search: find first element where !(element < key)
        const_iterator first = mData.begin();
        size_type count = mData.size();

        while (count > 0) {
            size_type step = count / 2;
            const_iterator it = first + step;
            if (mLess(it->first, key)) {
                first = it + 1;
                count -= step + 1;
            } else {
                count = step;
            }
        }
        return first;
    }

    iterator upper_bound(const Key& key) FL_NOEXCEPT {
        // Binary search: find first element where key < element
        iterator first = mData.begin();
        size_type count = mData.size();

        while (count > 0) {
            size_type step = count / 2;
            iterator it = first + step;
            if (!mLess(key, it->first)) {
                first = it + 1;
                count -= step + 1;
            } else {
                count = step;
            }
        }
        return first;
    }

    const_iterator upper_bound(const Key& key) const FL_NOEXCEPT {
        // Binary search: find first element where key < element
        const_iterator first = mData.begin();
        size_type count = mData.size();

        while (count > 0) {
            size_type step = count / 2;
            const_iterator it = first + step;
            if (!mLess(key, it->first)) {
                first = it + 1;
                count -= step + 1;
            } else {
                count = step;
            }
        }
        return first;
    }

    fl::pair<iterator, iterator> equal_range(const Key& key) FL_NOEXCEPT {
        auto lower = lower_bound(key);
        auto upper = upper_bound(key);
        return fl::pair<iterator, iterator>(lower, upper);
    }

    fl::pair<const_iterator, const_iterator> equal_range(const Key& key) const FL_NOEXCEPT {
        auto lower = lower_bound(key);
        auto upper = upper_bound(key);
        return fl::pair<const_iterator, const_iterator>(lower, upper);
    }

    // Insertion
    fl::pair<iterator, bool> insert(const value_type& value) FL_NOEXCEPT {
        auto it = lower_bound(value.first);
        if (it != end() && !mLess(value.first, it->first) && !mLess(it->first, value.first)) {
            return fl::pair<iterator, bool>(it, false);  // Already exists
        }
        bool success = mData.insert(it, value);
        if (success) {
            // After insert, find the newly inserted element
            it = find(value.first);
            return fl::pair<iterator, bool>(it, true);
        }
        return fl::pair<iterator, bool>(end(), false);
    }

    fl::pair<iterator, bool> insert(value_type&& value) FL_NOEXCEPT {
        auto key = value.first;
        auto it = lower_bound(key);
        if (it != end() && !mLess(key, it->first) && !mLess(it->first, key)) {
            return fl::pair<iterator, bool>(it, false);  // Already exists
        }
        bool success = mData.insert(it, fl::move(value));
        if (success) {
            // After insert, find the newly inserted element
            it = find(key);
            return fl::pair<iterator, bool>(it, true);
        }
        return fl::pair<iterator, bool>(end(), false);
    }

    // Insert with hint (optimization hint, may be ignored)
    iterator insert(const_iterator, const value_type& value) FL_NOEXCEPT {
        return insert(value).first;
    }

    iterator insert(const_iterator, value_type&& value) FL_NOEXCEPT {
        return insert(fl::move(value)).first;
    }

    bool insert(const Key& key, const Value& value, insert_result* result = nullptr) FL_NOEXCEPT {
        auto it = lower_bound(key);
        if (it != end() && !mLess(key, it->first) && !mLess(it->first, key)) {
            if (result) *result = exists;
            return false;
        }
        bool success = mData.insert(it, value_type(key, value));
        if (success) {
            if (result) *result = inserted;
            return true;
        }
        if (result) *result = at_capacity;
        return false;
    }

    bool insert(Key&& key, Value&& value, insert_result* result = nullptr) FL_NOEXCEPT {
        auto key_copy = key;
        auto it = lower_bound(key_copy);
        if (it != end() && !mLess(key_copy, it->first) && !mLess(it->first, key_copy)) {
            if (result) *result = exists;
            return false;
        }
        bool success = mData.insert(it, value_type(fl::move(key), fl::move(value)));
        if (success) {
            if (result) *result = inserted;
            return true;
        }
        if (result) *result = at_capacity;
        return false;
    }

    // Emplace
    template <typename... Args>
    fl::pair<iterator, bool> emplace(Args&&... args) FL_NOEXCEPT {
        value_type value(fl::forward<Args>(args)...);
        return insert(value);
    }

    template <typename... Args>
    iterator emplace_hint(const_iterator hint, Args&&... args) FL_NOEXCEPT {
        value_type value(fl::forward<Args>(args)...);
        return insert(hint, fl::move(value));
    }

    // Deletion
    iterator erase(iterator pos) FL_NOEXCEPT {
        // Vector erase() returns iterator in some versions, bool in others
        // To be safe, erase and return the next element
        iterator next = pos + 1;
        mData.erase(pos);
        return next;
    }

    iterator erase(const_iterator pos) FL_NOEXCEPT {
        // Convert const_iterator to iterator and erase
        iterator it = const_cast<iterator>(pos);
        iterator next = it + 1;
        mData.erase(it);
        return next;
    }

    iterator erase(const_iterator first, const_iterator last) FL_NOEXCEPT {
        // Erase range [first, last) by repeatedly erasing the element at first
        fl::size count = last - first;
        iterator pos = const_cast<iterator>(first);

        for (fl::size i = 0; i < count && pos != end(); ++i) {
            // Erase the element at pos. After erase, pos points to the next element
            // (because all elements shift left)
            mData.erase(pos);
            // Don't increment pos - it already points to the next element after erase
        }
        return pos;
    }

    size_type erase(const Key& key) FL_NOEXCEPT {
        auto it = find(key);
        if (it != end()) {
            erase(it);
            return 1;
        }
        return 0;
    }

    // Clear
    void clear() FL_NOEXCEPT {
        mData.clear();
    }

    // Swap
    void swap(flat_map& other) FL_NOEXCEPT {
        mData.swap(other.mData);
        fl::swap(mLess, other.mLess);
    }

    // Comparison
    key_compare key_comp() const FL_NOEXCEPT {
        return mLess;
    }

    // Element access
    value_type& front() FL_NOEXCEPT {
        FASTLED_ASSERT(!empty(), "flat_map::front() on empty map");
        return mData.front();
    }

    const value_type& front() const FL_NOEXCEPT {
        FASTLED_ASSERT(!empty(), "flat_map::front() on empty map");
        return mData.front();
    }

    value_type& back() FL_NOEXCEPT {
        FASTLED_ASSERT(!empty(), "flat_map::back() on empty map");
        return mData.back();
    }

    const value_type& back() const FL_NOEXCEPT {
        FASTLED_ASSERT(!empty(), "flat_map::back() on empty map");
        return mData.back();
    }

    // FastLED-specific methods

    // Get value with default return if not found
    Value get(const Key& key, const Value& defaultValue) const FL_NOEXCEPT {
        auto it = find(key);
        if (it != end()) {
            return it->second;
        }
        return defaultValue;
    }

    // Get value with out parameter
    bool get(const Key& key, Value* out_value) const FL_NOEXCEPT {
        auto it = find(key);
        if (it != end()) {
            *out_value = it->second;
            return true;
        }
        return false;
    }

    // Insert or update (returns whether it was inserted)
    fl::pair<iterator, bool> insert_or_update(const Key& key, const Value& value) FL_NOEXCEPT {
        iterator it = find(key);
        if (it != end()) {
            it->second = value;
            return fl::pair<iterator, bool>(it, false);  // Updated, not inserted
        }
        return insert(value_type(key, value));
    }

    // FastLED-style update: insert if missing, update if exists
    bool update(const Key& key, const Value& value) FL_NOEXCEPT {
        iterator it = find(key);
        if (it != end()) {
            it->second = value;
            return true;  // Updated
        }
        // Key doesn't exist, insert it
        auto result = insert(value_type(key, value));
        return result.second;  // Return whether insertion succeeded
    }

    // Move version of update
    bool update(const Key& key, Value&& value) FL_NOEXCEPT {
        iterator it = find(key);
        if (it != end()) {
            it->second = fl::move(value);
            return true;  // Updated
        }
        // Key doesn't exist, insert it
        auto result = insert(value_type(key, fl::move(value)));
        return result.second;  // Return whether insertion succeeded
    }

    // Navigate to next element by key (FastLED compatibility)
    bool next(const Key& key, Key* next_key, bool allow_rollover = false) const FL_NOEXCEPT {
        auto it = find(key);
        if (it != end()) {
            ++it;
            if (it != end()) {
                *next_key = it->first;
                return true;
            } else if (allow_rollover && !empty()) {
                *next_key = begin()->first;
                return true;
            }
        }
        return false;
    }

    // Navigate to previous element by key (FastLED compatibility)
    bool prev(const Key& key, Key* prev_key, bool allow_rollover = false) const FL_NOEXCEPT {
        auto it = find(key);
        if (it != end()) {
            if (it != begin()) {
                --it;
                *prev_key = it->first;
                return true;
            } else if (allow_rollover && !empty()) {
                *prev_key = mData.back().first;
                return true;
            }
        }
        return false;
    }

    // Allows pre-allocating space
    void reserve(size_type n) FL_NOEXCEPT {
        mData.reserve(n);
    }

    // Shrink capacity to fit size
    void shrink_to_fit() FL_NOEXCEPT {
        mData.shrink_to_fit();
    }

    memory_resource* get_memory_resource() const FL_NOEXCEPT { return mData.get_resource(); }
};

// Comparison operators
template <typename Key, typename Value, typename Less>
bool operator==(const flat_map<Key, Value, Less>& lhs,
                const flat_map<Key, Value, Less>& rhs) {
    return lhs.size() == rhs.size() &&
           fl::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template <typename Key, typename Value, typename Less>
bool operator!=(const flat_map<Key, Value, Less>& lhs,
                const flat_map<Key, Value, Less>& rhs) {
    return !(lhs == rhs);
}

template <typename Key, typename Value, typename Less>
bool operator<(const flat_map<Key, Value, Less>& lhs,
               const flat_map<Key, Value, Less>& rhs) {
    return fl::lexicographical_compare(lhs.begin(), lhs.end(),
                                       rhs.begin(), rhs.end());
}

template <typename Key, typename Value, typename Less>
bool operator<=(const flat_map<Key, Value, Less>& lhs,
                const flat_map<Key, Value, Less>& rhs) {
    return !(rhs < lhs);
}

template <typename Key, typename Value, typename Less>
bool operator>(const flat_map<Key, Value, Less>& lhs,
               const flat_map<Key, Value, Less>& rhs) {
    return rhs < lhs;
}

template <typename Key, typename Value, typename Less>
bool operator>=(const flat_map<Key, Value, Less>& lhs,
                const flat_map<Key, Value, Less>& rhs) {
    return !(lhs < rhs);
}

// Swap
template <typename Key, typename Value, typename Less>
void swap(flat_map<Key, Value, Less>& lhs,
          flat_map<Key, Value, Less>& rhs) FL_NOEXCEPT {
    lhs.swap(rhs);
}

}  // namespace fl
