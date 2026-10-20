#pragma once

#include "fl/stl/vector.h"
#include "fl/stl/detail/rbtree.h"
#include "fl/stl/allocator.h"
#include "fl/stl/pair.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/move.h"
#include "fl/stl/strstream.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/utility.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

template <typename Key, typename Allocator> class set;  // IWYU pragma: keep

// VectorSet stores values in order of insertion.
template <typename Key, size N> class VectorSetFixed;
template <typename Key> class VectorSet;  // IWYU pragma: keep

template <typename Key, size N>
using FixedSet = VectorSetFixed<Key, N>;  // Backwards compatibility

// A simple unordered set implementation with a fixed size.
// The user is responsible for making sure that the inserts
// do not exceed the capacity of the set, otherwise they will
// fail. Because of this limitation, this set is not a drop in
// replacement for std::set.
template <typename Key, size N> class VectorSetFixed {
  public:
    typedef FixedVector<Key, N> VectorType;
    typedef typename VectorType::iterator iterator;
    typedef typename VectorType::const_iterator const_iterator;

    // Constructor
    constexpr VectorSetFixed() FL_NO_EXCEPT = default;

    iterator begin() FL_NO_EXCEPT { return data.begin(); }
    iterator end() FL_NO_EXCEPT { return data.end(); }
    const_iterator begin() const FL_NO_EXCEPT { return data.begin(); }
    const_iterator end() const FL_NO_EXCEPT { return data.end(); }

    iterator find(const Key &key) FL_NO_EXCEPT {
        for (auto it = begin(); it != end(); ++it) {
            if (*it == key) {
                return it;
            }
        }
        return end();
    }

    const_iterator find(const Key &key) const FL_NO_EXCEPT {
        for (auto it = begin(); it != end(); ++it) {
            if (*it == key) {
                return it;
            }
        }
        return end();
    }

    bool insert(const Key &key) FL_NO_EXCEPT {
        if (data.size() < N) {
            auto it = find(key);
            if (it == end()) {
                data.push_back(key);
                return true;
            }
        }
        return false;
    }

    // Move version of insert
    bool insert(Key &&key) FL_NO_EXCEPT {
        if (data.size() < N) {
            auto it = find(key);
            if (it == end()) {
                data.push_back(fl::move(key));
                return true;
            }
        }
        return false;
    }

    // Emplace - construct in place with perfect forwarding
    template<typename... Args>
    bool emplace(Args&&... args) FL_NO_EXCEPT {
        if (data.size() < N) {
            // Create a temporary to check if it already exists
            Key temp_key(fl::forward<Args>(args)...);
            auto it = find(temp_key);
            if (it == end()) {
                data.push_back(fl::move(temp_key));
                return true;
            }
        }
        return false;
    }

    bool erase(const Key &key) FL_NO_EXCEPT {
        auto it = find(key);
        if (it != end()) {
            data.erase(it);
            return true;
        }
        return false;
    }

    bool erase(iterator pos) FL_NO_EXCEPT {
        if (pos != end()) {
            data.erase(pos);
            return true;
        }
        return false;
    }

    bool next(const Key &key, Key *next_key,
              bool allow_rollover = false) const FL_NO_EXCEPT {
        const_iterator it = find(key);
        if (it != end()) {
            ++it;
            if (it != end()) {
                *next_key = *it;
                return true;
            } else if (allow_rollover && !empty()) {
                *next_key = *begin();
                return true;
            }
        }
        return false;
    }

    bool prev(const Key &key, Key *prev_key,
              bool allow_rollover = false) const FL_NO_EXCEPT {
        const_iterator it = find(key);
        if (it != end()) {
            if (it != begin()) {
                --it;
                *prev_key = *it;
                return true;
            } else if (allow_rollover && !empty()) {
                *prev_key = data[data.size() - 1];
                return true;
            }
        }
        return false;
    }

    // Get the current size of the set
    constexpr fl::size size() const FL_NO_EXCEPT { return data.size(); }

    constexpr bool empty() const FL_NO_EXCEPT { return data.empty(); }

    // Get the capacity of the set
    constexpr fl::size capacity() const FL_NO_EXCEPT { return N; }

    // Clear the set
    void clear() FL_NO_EXCEPT { data.clear(); }

    bool has(const Key &key) const FL_NO_EXCEPT { return find(key) != end(); }

    // Return the first element of the set
    const Key &front() const FL_NO_EXCEPT { return data.front(); }

    // Return the last element of the set
    const Key &back() const FL_NO_EXCEPT { return data.back(); }

    /// Equality comparison
    bool operator==(const VectorSetFixed& other) const FL_NO_EXCEPT {
        return data == other.data;
    }

    /// Inequality comparison
    bool operator!=(const VectorSetFixed& other) const FL_NO_EXCEPT {
        return data != other.data;
    }

    /// Lexicographic comparison
    bool operator<(const VectorSetFixed& other) const FL_NO_EXCEPT {
        return data < other.data;
    }

    /// Less-than-or-equal comparison
    bool operator<=(const VectorSetFixed& other) const FL_NO_EXCEPT {
        return data <= other.data;
    }

    /// Greater-than comparison
    bool operator>(const VectorSetFixed& other) const FL_NO_EXCEPT {
        return data > other.data;
    }

    /// Greater-than-or-equal comparison
    bool operator>=(const VectorSetFixed& other) const FL_NO_EXCEPT {
        return data >= other.data;
    }

  private:
    VectorType data;
};

template <typename Key> class VectorSet {
  public:
    typedef fl::vector<Key> VectorType;
    typedef typename VectorType::iterator iterator;
    typedef typename VectorType::const_iterator const_iterator;
    typedef typename VectorType::reverse_iterator reverse_iterator;

    // Constructor
    constexpr VectorSet() FL_NO_EXCEPT = default;

    // Copy constructor
    VectorSet(const VectorSet &other) FL_NO_EXCEPT : data(other.data) {}

    // Move constructor
    VectorSet(VectorSet &&other) FL_NO_EXCEPT
        : data(fl::move(other.data)) {}

    // Copy assignment operator
    VectorSet &operator=(const VectorSet &other) FL_NO_EXCEPT {
        if (this != &other) {
            data = other.data;
        }
        return *this;
    }

    // Move assignment operator
    VectorSet &operator=(VectorSet &&other) FL_NO_EXCEPT {
        if (this != &other) {
            data = fl::move(other.data);
        }
        return *this;
    }

    iterator begin() FL_NO_EXCEPT { return data.begin(); }
    iterator end() FL_NO_EXCEPT { return data.end(); }
    const_iterator begin() const FL_NO_EXCEPT { return data.begin(); }
    const_iterator end() const FL_NO_EXCEPT { return data.end(); }

    reverse_iterator rbegin() FL_NO_EXCEPT { return data.rbegin(); }
    reverse_iterator rend() FL_NO_EXCEPT { return data.rend(); }

    iterator find(const Key &key) FL_NO_EXCEPT {
        for (auto it = begin(); it != end(); ++it) {
            if (*it == key) {
                return it;
            }
        }
        return end();
    }

    const_iterator find(const Key &key) const FL_NO_EXCEPT {
        for (auto it = begin(); it != end(); ++it) {
            if (*it == key) {
                return it;
            }
        }
        return end();
    }

    bool insert(const Key &key) FL_NO_EXCEPT {
        auto it = find(key);
        if (it == end()) {
            data.push_back(key);
            return true;
        }
        return false;
    }

    // Move version of insert
    bool insert(Key &&key) FL_NO_EXCEPT {
        auto it = find(key);
        if (it == end()) {
            data.push_back(fl::move(key));
            return true;
        }
        return false;
    }

    // Emplace - construct in place with perfect forwarding
    template<typename... Args>
    bool emplace(Args&&... args) FL_NO_EXCEPT {
        // Create a temporary to check if it already exists
        Key temp_key(fl::forward<Args>(args)...);
        auto it = find(temp_key);
        if (it == end()) {
            data.push_back(fl::move(temp_key));
            return true;
        }
        return false;
    }

    bool erase(const Key &key) FL_NO_EXCEPT {
        auto it = find(key);
        if (it != end()) {
            data.erase(it);
            return true;
        }
        return false;
    }

    bool erase(iterator pos) FL_NO_EXCEPT {
        if (pos != end()) {
            data.erase(pos);
            return true;
        }
        return false;
    }

    // Get the current size of the set
    constexpr fl::size size() const FL_NO_EXCEPT { return data.size(); }

    constexpr bool empty() const FL_NO_EXCEPT { return data.empty(); }

    // Get the capacity of the set
    constexpr fl::size capacity() const FL_NO_EXCEPT { return data.capacity(); }

    // Clear the set
    void clear() FL_NO_EXCEPT { data.clear(); }

    bool has(const Key &key) const FL_NO_EXCEPT { return find(key) != end(); }

    // Return the first element of the set
    const Key &front() const FL_NO_EXCEPT { return data.front(); }

    // Return the last element of the set
    const Key &back() const FL_NO_EXCEPT { return data.back(); }

    /// Equality comparison
    bool operator==(const VectorSet& other) const FL_NO_EXCEPT {
        return data == other.data;
    }

    /// Inequality comparison
    bool operator!=(const VectorSet& other) const FL_NO_EXCEPT {
        return data != other.data;
    }

    /// Lexicographic comparison
    bool operator<(const VectorSet& other) const FL_NO_EXCEPT {
        return data < other.data;
    }

    /// Less-than-or-equal comparison
    bool operator<=(const VectorSet& other) const FL_NO_EXCEPT {
        return data <= other.data;
    }

    /// Greater-than comparison
    bool operator>(const VectorSet& other) const FL_NO_EXCEPT {
        return data > other.data;
    }

    /// Greater-than-or-equal comparison
    bool operator>=(const VectorSet& other) const FL_NO_EXCEPT {
        return data >= other.data;
    }

  private:
    VectorType data;
};

// fl::set<T, Allocator> - Ordered set implementation using SetRedBlackTree
// This is an ordered set that keeps elements sorted, similar to std::set
template <typename Key, typename Allocator = fl::allocator_slab<Key>> class set {
  private:
    using TreeType = fl::SetRedBlackTree<Key, fl::less<Key>, Allocator>;
    TreeType mTree;
    
  public:
    // Standard set typedefs
    using key_type = Key;
    using value_type = Key;
    using size_type = fl::size;
    using difference_type = ptrdiff_t;
    using reference = const Key&;
    using const_reference = const Key&;
    using pointer = const Key*;
    using const_pointer = const Key*;
    
    // Iterator types - we only provide const iterators since set elements are immutable
    using const_iterator = typename TreeType::const_iterator;
    using iterator = const_iterator;  // set only provides const iterators
    using const_reverse_iterator = typename TreeType::const_reverse_iterator;
    using reverse_iterator = const_reverse_iterator;  // set only provides const reverse iterators
    using allocator_type = Allocator;

    // Constructors
    set() FL_NO_EXCEPT = default;
    explicit set(const Allocator& alloc) FL_NO_EXCEPT : mTree(fl::less<Key>(), alloc) {}
    set(const set& other) FL_NO_EXCEPT = default;
    set(set&& other) FL_NO_EXCEPT = default;
    set& operator=(const set& other) FL_NO_EXCEPT = default;
    set& operator=(set&& other) FL_NO_EXCEPT = default;

    // Initializer list constructor
    set(fl::initializer_list<Key> init) FL_NO_EXCEPT {
        for (const auto& elem : init) {
            insert(elem);
        }
    }

    // Initializer list assignment
    set& operator=(fl::initializer_list<Key> init) FL_NO_EXCEPT {
        clear();
        for (const auto& elem : init) {
            insert(elem);
        }
        return *this;
    }
    
    // Iterators
    const_iterator begin() const FL_NO_EXCEPT { return mTree.begin(); }
    const_iterator end() const FL_NO_EXCEPT { return mTree.end(); }
    const_iterator cbegin() const FL_NO_EXCEPT { return mTree.cbegin(); }
    const_iterator cend() const FL_NO_EXCEPT { return mTree.cend(); }

    // Reverse iterators
    const_reverse_iterator rbegin() const FL_NO_EXCEPT { return mTree.rbegin(); }
    const_reverse_iterator rend() const FL_NO_EXCEPT { return mTree.rend(); }
    
    // Capacity
    bool empty() const FL_NO_EXCEPT { return mTree.empty(); }
    size_type size() const FL_NO_EXCEPT { return mTree.size(); }
    size_type max_size() const FL_NO_EXCEPT { return mTree.max_size(); }
    allocator_type get_allocator() const FL_NO_EXCEPT { return mTree.get_allocator(); }

    // Modifiers
    void clear() FL_NO_EXCEPT { mTree.clear(); }
    
    fl::pair<const_iterator, bool> insert(const Key& key) FL_NO_EXCEPT {
        return mTree.insert(key);
    }
    
    fl::pair<const_iterator, bool> insert(Key&& key) FL_NO_EXCEPT {
        return mTree.insert(fl::move(key));
    }
    
    template<typename... Args>
    fl::pair<const_iterator, bool> emplace(Args&&... args) FL_NO_EXCEPT {
        return mTree.emplace(fl::forward<Args>(args)...);
    }
    
    const_iterator erase(const_iterator pos) FL_NO_EXCEPT {
        return mTree.erase(pos);
    }
    
    size_type erase(const Key& key) FL_NO_EXCEPT {
        return mTree.erase(key);
    }
    
    void swap(set& other) FL_NO_EXCEPT {
        mTree.swap(other.mTree);
    }
    
    // Lookup
    size_type count(const Key& key) const FL_NO_EXCEPT {
        return mTree.count(key);
    }
    
    const_iterator find(const Key& key) const FL_NO_EXCEPT {
        return mTree.find(key);
    }
    
    bool contains(const Key& key) const FL_NO_EXCEPT {
        return mTree.contains(key);
    }
    
    bool has(const Key& key) const FL_NO_EXCEPT {
        return contains(key);
    }
    
    fl::pair<const_iterator, const_iterator> equal_range(const Key& key) const FL_NO_EXCEPT {
        return mTree.equal_range(key);
    }
    
    const_iterator lower_bound(const Key& key) const FL_NO_EXCEPT {
        return mTree.lower_bound(key);
    }
    
    const_iterator upper_bound(const Key& key) const FL_NO_EXCEPT {
        return mTree.upper_bound(key);
    }

    /// Equality comparison
    bool operator==(const set& other) const FL_NO_EXCEPT {
        if (size() != other.size()) return false;
        return fl::equal(begin(), end(), other.begin());
    }

    /// Inequality comparison
    bool operator!=(const set& other) const FL_NO_EXCEPT {
        return !(*this == other);
    }

    /// Lexicographic comparison
    bool operator<(const set& other) const FL_NO_EXCEPT {
        return fl::lexicographical_compare(begin(), end(), other.begin(), other.end());
    }

    /// Less-than-or-equal comparison
    bool operator<=(const set& other) const FL_NO_EXCEPT {
        return *this < other || *this == other;
    }

    /// Greater-than comparison
    bool operator>(const set& other) const FL_NO_EXCEPT {
        return other < *this;
    }

    /// Greater-than-or-equal comparison
    bool operator>=(const set& other) const FL_NO_EXCEPT {
        return other <= *this;
    }
};

// fl::set_inlined<T, N> - Inlined set implementation using proper allocator
// This is a using declaration that sets the proper allocator for fl::set
template <typename T, fl::size N>
using set_inlined = fl::set<T, fl::allocator_inlined_slab<T, N>>;

} // namespace fl
