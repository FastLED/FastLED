#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/comparators.h"
#include "fl/stl/vector.h"
#include "fl/stl/allocator.h"
#include "fl/insert_result.h"

namespace fl {

// A cache-friendly sorted set using contiguous storage.
//
// flat_set stores keys in a sorted vector for optimal memory layout.
// This provides excellent cache locality and iteration performance, but slower
// insertions and deletions (O(n)) compared to tree-based sets. Use flat_set when:
// - You have frequent lookups and iteration (better cache locality)
// - You don't do many insertions/deletions
// - Memory layout matters for performance
//
// The container maintains sorted order and uses binary search for lookups.
template <typename Key,
          typename Less = fl::less<Key>,
          typename Allocator = fl::allocator<Key>>
class flat_set {
  public:
    using key_type = Key;
    using value_type = Key;
    using size_type = fl::size;
    using difference_type = ptrdiff_t;
    using key_compare = Less;
    using value_compare = Less;
    using allocator_type = Allocator;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;

    using vector_type = fl::vector<value_type, Allocator>;
    using iterator = typename vector_type::iterator;
    using const_iterator = typename vector_type::const_iterator;
    using reverse_iterator = typename vector_type::reverse_iterator;
    using const_reverse_iterator = typename vector_type::const_reverse_iterator;

  private:
    vector_type mData;
    Less mLess;

  public:
    // Constructors
    flat_set() = default;

    explicit flat_set(const Less& less, const Allocator& alloc = Allocator())
        : mData(alloc), mLess(less) {}

    explicit flat_set(const Allocator& alloc)
        : mData(alloc), mLess(Less()) {}

    flat_set(const flat_set& other) = default;
    flat_set& operator=(const flat_set& other) = default;

    flat_set(flat_set&& other) noexcept
        : mData(fl::move(other.mData)), mLess(fl::move(other.mLess)) {}

    flat_set& operator=(flat_set&& other) noexcept {
        if (this != &other) {
            mData = fl::move(other.mData);
            mLess = fl::move(other.mLess);
        }
        return *this;
    }

    // Iterators
    iterator begin() { return mData.begin(); }
    iterator end() { return mData.end(); }
    const_iterator begin() const { return mData.begin(); }
    const_iterator end() const { return mData.end(); }
    const_iterator cbegin() const { return mData.begin(); }
    const_iterator cend() const { return mData.end(); }

    reverse_iterator rbegin() { return mData.rbegin(); }
    reverse_iterator rend() { return mData.rend(); }
    const_reverse_iterator rbegin() const { return mData.rbegin(); }
    const_reverse_iterator rend() const { return mData.rend(); }

    // Capacity
    size_type size() const { return mData.size(); }
    bool empty() const { return mData.empty(); }
    size_type capacity() const { return mData.capacity(); }
    size_type max_size() const { return mData.max_size(); }

    allocator_type get_allocator() const { return mData.get_allocator(); }

    // Lookup
    iterator find(const Key& key) {
        auto it = lower_bound(key);
        if (it != end() && !mLess(key, *it) && !mLess(*it, key)) {
            return it;
        }
        return end();
    }

    const_iterator find(const Key& key) const {
        auto it = lower_bound(key);
        if (it != end() && !mLess(key, *it) && !mLess(*it, key)) {
            return it;
        }
        return end();
    }

    size_type count(const Key& key) const {
        return find(key) != end() ? 1 : 0;
    }

    bool contains(const Key& key) const {
        return find(key) != end();
    }

    // Bounds - binary search using pointer arithmetic
    iterator lower_bound(const Key& key) {
        // Binary search: find first element where !(element < key)
        iterator first = mData.begin();
        size_type count = mData.size();

        while (count > 0) {
            size_type step = count / 2;
            iterator it = first + step;
            if (mLess(*it, key)) {
                first = it + 1;
                count -= step + 1;
            } else {
                count = step;
            }
        }
        return first;
    }

    const_iterator lower_bound(const Key& key) const {
        // Binary search: find first element where !(element < key)
        const_iterator first = mData.begin();
        size_type count = mData.size();

        while (count > 0) {
            size_type step = count / 2;
            const_iterator it = first + step;
            if (mLess(*it, key)) {
                first = it + 1;
                count -= step + 1;
            } else {
                count = step;
            }
        }
        return first;
    }

    iterator upper_bound(const Key& key) {
        // Binary search: find first element where key < element
        iterator first = mData.begin();
        size_type count = mData.size();

        while (count > 0) {
            size_type step = count / 2;
            iterator it = first + step;
            if (!mLess(key, *it)) {
                first = it + 1;
                count -= step + 1;
            } else {
                count = step;
            }
        }
        return first;
    }

    const_iterator upper_bound(const Key& key) const {
        // Binary search: find first element where key < element
        const_iterator first = mData.begin();
        size_type count = mData.size();

        while (count > 0) {
            size_type step = count / 2;
            const_iterator it = first + step;
            if (!mLess(key, *it)) {
                first = it + 1;
                count -= step + 1;
            } else {
                count = step;
            }
        }
        return first;
    }

    fl::pair<iterator, iterator> equal_range(const Key& key) {
        auto lower = lower_bound(key);
        auto upper = upper_bound(key);
        return fl::pair<iterator, iterator>(lower, upper);
    }

    fl::pair<const_iterator, const_iterator> equal_range(const Key& key) const {
        auto lower = lower_bound(key);
        auto upper = upper_bound(key);
        return fl::pair<const_iterator, const_iterator>(lower, upper);
    }

    // Insertion
    fl::pair<iterator, bool> insert(const value_type& value) {
        auto it = lower_bound(value);
        if (it != end() && !mLess(value, *it) && !mLess(*it, value)) {
            return fl::pair<iterator, bool>(it, false);  // Already exists
        }
        bool success = mData.insert(it, value);
        if (success) {
            // After insert, find the newly inserted element
            it = find(value);
            return fl::pair<iterator, bool>(it, true);
        }
        return fl::pair<iterator, bool>(end(), false);
    }

    fl::pair<iterator, bool> insert(value_type&& value) {
        auto key = value;
        auto it = lower_bound(key);
        if (it != end() && !mLess(key, *it) && !mLess(*it, key)) {
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
    iterator insert(const_iterator hint, const value_type& value) {
        // TODO: use hint for performance if it's valid
        return insert(value).first;
    }

    iterator insert(const_iterator hint, value_type&& value) {
        return insert(fl::move(value)).first;
    }

    // Emplace
    template <typename... Args>
    fl::pair<iterator, bool> emplace(Args&&... args) {
        value_type value(fl::forward<Args>(args)...);
        return insert(value);
    }

    template <typename... Args>
    iterator emplace_hint(const_iterator hint, Args&&... args) {
        value_type value(fl::forward<Args>(args)...);
        return insert(hint, fl::move(value));
    }

    // Deletion
    iterator erase(iterator pos) {
        // Vector erase() returns iterator in some versions, bool in others
        // To be safe, erase and return the next element
        iterator next = pos + 1;
        mData.erase(pos);
        return next;
    }

    iterator erase(const_iterator pos) {
        // Convert const_iterator to iterator and erase
        iterator it = const_cast<iterator>(pos);
        iterator next = it + 1;
        mData.erase(it);
        return next;
    }

    iterator erase(const_iterator first, const_iterator last) {
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

    size_type erase(const Key& key) {
        auto it = find(key);
        if (it != end()) {
            erase(it);
            return 1;
        }
        return 0;
    }

    // Clear
    void clear() {
        mData.clear();
    }

    // Swap
    void swap(flat_set& other) noexcept {
        mData.swap(other.mData);
        fl::fl_swap(mLess, other.mLess);
    }

    // Comparison
    key_compare key_comp() const {
        return mLess;
    }

    value_compare value_comp() const {
        return mLess;
    }

    // Allows pre-allocating space (FastLED style)
    void reserve(size_type n) {
        mData.reserve(n);
    }
};

// Comparison operators
template <typename Key, typename Less, typename Allocator>
bool operator==(const flat_set<Key, Less, Allocator>& lhs,
                const flat_set<Key, Less, Allocator>& rhs) {
    return lhs.size() == rhs.size() &&
           fl::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template <typename Key, typename Less, typename Allocator>
bool operator!=(const flat_set<Key, Less, Allocator>& lhs,
                const flat_set<Key, Less, Allocator>& rhs) {
    return !(lhs == rhs);
}

template <typename Key, typename Less, typename Allocator>
bool operator<(const flat_set<Key, Less, Allocator>& lhs,
               const flat_set<Key, Less, Allocator>& rhs) {
    return fl::lexicographical_compare(lhs.begin(), lhs.end(),
                                       rhs.begin(), rhs.end());
}

template <typename Key, typename Less, typename Allocator>
bool operator<=(const flat_set<Key, Less, Allocator>& lhs,
                const flat_set<Key, Less, Allocator>& rhs) {
    return !(rhs < lhs);
}

template <typename Key, typename Less, typename Allocator>
bool operator>(const flat_set<Key, Less, Allocator>& lhs,
               const flat_set<Key, Less, Allocator>& rhs) {
    return rhs < lhs;
}

template <typename Key, typename Less, typename Allocator>
bool operator>=(const flat_set<Key, Less, Allocator>& lhs,
                const flat_set<Key, Less, Allocator>& rhs) {
    return !(lhs < rhs);
}

// Swap
template <typename Key, typename Compare, typename Allocator>
void fl_swap(flat_set<Key, Compare, Allocator>& lhs, flat_set<Key, Compare, Allocator>& rhs) {
    lhs.swap(rhs);
}

}  // namespace fl
```
