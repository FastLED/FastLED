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
#include "fl/stl/initializer_list.h"
#include "fl/int.h"


namespace fl {

template <typename Key, typename Allocator> class set;

// VectorSet stores values in order of insertion.
template <typename Key, size N> class VectorSetFixed;
template <typename Key, typename Allocator> class VectorSet;

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
    constexpr VectorSetFixed() = default;

    iterator begin() { return data.begin(); }
    iterator end() { return data.end(); }
    const_iterator begin() const { return data.begin(); }
    const_iterator end() const { return data.end(); }

    iterator find(const Key &key) {
        for (auto it = begin(); it != end(); ++it) {
            if (*it == key) {
                return it;
            }
        }
        return end();
    }

    const_iterator find(const Key &key) const {
        for (auto it = begin(); it != end(); ++it) {
            if (*it == key) {
                return it;
            }
        }
        return end();
    }

    bool insert(const Key &key) {
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
    bool insert(Key &&key) {
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
    bool emplace(Args&&... args) {
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

    bool erase(const Key &key) {
        auto it = find(key);
        if (it != end()) {
            data.erase(it);
            return true;
        }
        return false;
    }

    bool erase(iterator pos) {
        if (pos != end()) {
            data.erase(pos);
            return true;
        }
        return false;
    }

    bool next(const Key &key, Key *next_key,
              bool allow_rollover = false) const {
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
              bool allow_rollover = false) const {
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
    constexpr fl::size size() const { return data.size(); }

    constexpr bool empty() const { return data.empty(); }

    // Get the capacity of the set
    constexpr fl::size capacity() const { return N; }

    // Clear the set
    void clear() { data.clear(); }

    bool has(const Key &key) const { return find(key) != end(); }

    // Return the first element of the set
    const Key &front() const { return data.front(); }

    // Return the last element of the set
    const Key &back() const { return data.back(); }

  private:
    VectorType data;
};

template <typename Key, typename Allocator = fl::allocator<Key>> class VectorSet {
  public:
    typedef fl::vector<Key, Allocator> VectorType;
    typedef typename VectorType::iterator iterator;
    typedef typename VectorType::const_iterator const_iterator;

    // Constructor
    constexpr VectorSet() = default;

    // Copy constructor
    VectorSet(const VectorSet<Key, Allocator> &other) : data(other.data) {}

    // Move constructor
    VectorSet(VectorSet<Key, Allocator> &&other) noexcept
        : data(fl::move(other.data)) {}

    // Copy assignment operator
    VectorSet &operator=(const VectorSet<Key, Allocator> &other) {
        if (this != &other) {
            data = other.data;
        }
        return *this;
    }

    // Move assignment operator
    VectorSet &operator=(VectorSet<Key, Allocator> &&other) noexcept {
        if (this != &other) {
            data = fl::move(other.data);
        }
        return *this;
    }

    iterator begin() { return data.begin(); }
    iterator end() { return data.end(); }
    const_iterator begin() const { return data.begin(); }
    const_iterator end() const { return data.end(); }

    iterator find(const Key &key) {
        for (auto it = begin(); it != end(); ++it) {
            if (*it == key) {
                return it;
            }
        }
        return end();
    }

    const_iterator find(const Key &key) const {
        for (auto it = begin(); it != end(); ++it) {
            if (*it == key) {
                return it;
            }
        }
        return end();
    }

    bool insert(const Key &key) {
        auto it = find(key);
        if (it == end()) {
            data.push_back(key);
            return true;
        }
        return false;
    }

    // Move version of insert
    bool insert(Key &&key) {
        auto it = find(key);
        if (it == end()) {
            data.push_back(fl::move(key));
            return true;
        }
        return false;
    }

    // Emplace - construct in place with perfect forwarding
    template<typename... Args>
    bool emplace(Args&&... args) {
        // Create a temporary to check if it already exists
        Key temp_key(fl::forward<Args>(args)...);
        auto it = find(temp_key);
        if (it == end()) {
            data.push_back(fl::move(temp_key));
            return true;
        }
        return false;
    }

    bool erase(const Key &key) {
        auto it = find(key);
        if (it != end()) {
            data.erase(it);
            return true;
        }
        return false;
    }

    bool erase(iterator pos) {
        if (pos != end()) {
            data.erase(pos);
            return true;
        }
        return false;
    }

    // Get the current size of the set
    constexpr fl::size size() const { return data.size(); }

    constexpr bool empty() const { return data.empty(); }

    // Get the capacity of the set
    constexpr fl::size capacity() const { return data.capacity(); }

    // Clear the set
    void clear() { data.clear(); }

    bool has(const Key &key) const { return find(key) != end(); }

    // Return the first element of the set
    const Key &front() const { return data.front(); }

    // Return the last element of the set
    const Key &back() const { return data.back(); }

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
    
    // Constructors
    set() = default;
    set(const set& other) = default;
    set(set&& other) = default;
    set& operator=(const set& other) = default;
    set& operator=(set&& other) = default;

    // Initializer list constructor
    set(fl::initializer_list<Key> init) {
        for (const auto& elem : init) {
            insert(elem);
        }
    }

    // Initializer list assignment
    set& operator=(fl::initializer_list<Key> init) {
        clear();
        for (const auto& elem : init) {
            insert(elem);
        }
        return *this;
    }
    
    // Iterators
    const_iterator begin() const { return mTree.begin(); }
    const_iterator end() const { return mTree.end(); }
    const_iterator cbegin() const { return mTree.cbegin(); }
    const_iterator cend() const { return mTree.cend(); }
    
    // Capacity
    bool empty() const { return mTree.empty(); }
    size_type size() const { return mTree.size(); }
    size_type max_size() const { return mTree.max_size(); }
    
    // Modifiers
    void clear() { mTree.clear(); }
    
    fl::pair<const_iterator, bool> insert(const Key& key) {
        return mTree.insert(key);
    }
    
    fl::pair<const_iterator, bool> insert(Key&& key) {
        return mTree.insert(fl::move(key));
    }
    
    template<typename... Args>
    fl::pair<const_iterator, bool> emplace(Args&&... args) {
        return mTree.emplace(fl::forward<Args>(args)...);
    }
    
    const_iterator erase(const_iterator pos) {
        return mTree.erase(pos);
    }
    
    size_type erase(const Key& key) {
        return mTree.erase(key);
    }
    
    void swap(set& other) {
        mTree.swap(other.mTree);
    }
    
    // Lookup
    size_type count(const Key& key) const {
        return mTree.count(key);
    }
    
    const_iterator find(const Key& key) const {
        return mTree.find(key);
    }
    
    bool contains(const Key& key) const {
        return mTree.contains(key);
    }
    
    bool has(const Key& key) const {
        return contains(key);
    }
    
    fl::pair<const_iterator, const_iterator> equal_range(const Key& key) const {
        return mTree.equal_range(key);
    }
    
    const_iterator lower_bound(const Key& key) const {
        return mTree.lower_bound(key);
    }
    
    const_iterator upper_bound(const Key& key) const {
        return mTree.upper_bound(key);
    }
};

// fl::set_inlined<T, N> - Inlined set implementation using proper allocator
// This is a using declaration that sets the proper allocator for fl::set
template <typename T, fl::size N>
using set_inlined = fl::set<T, fl::allocator_inlined_slab<T, N>>;

} // namespace fl
