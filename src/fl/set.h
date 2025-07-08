#pragma once

//#include <stddef.h>
#include "fl/stdint.h"

#include "fl/namespace.h"
#include "fl/vector.h"
#include "fl/map.h"
#include "fl/allocator.h"


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
    typedef fl::HeapVector<Key, Allocator> VectorType;
    typedef typename VectorType::iterator iterator;
    typedef typename VectorType::const_iterator const_iterator;

    // Constructor
    constexpr VectorSet() = default;

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

// fl::set<T, Allocator> - Ordered set implementation using SortedHeapMap
// This is an ordered set that keeps elements sorted, similar to std::set
template <typename Key, typename Allocator = fl::allocator_slab<Key>> class set {
  private:
    // Use bool as the value type for the map to create a set
    // Rebind the allocator to work with Pair<Key, bool>
    using PairType = fl::Pair<Key, bool>;
    using ReboundAllocator = typename Allocator::template rebind<PairType>::other;
    using MapType = fl::SortedHeapMap<Key, bool>;
    
    MapType map_data;
    
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
    class const_iterator {
    private:
        typename MapType::const_iterator map_it;
        
    public:
        explicit const_iterator(typename MapType::const_iterator it) : map_it(it) {}
        
        const Key& operator*() const { return map_it->first; }
        const Key* operator->() const { return &(map_it->first); }
        
        const_iterator& operator++() { ++map_it; return *this; }
        const_iterator operator++(int) { const_iterator tmp = *this; ++map_it; return tmp; }
        
        bool operator==(const const_iterator& other) const { return map_it == other.map_it; }
        bool operator!=(const const_iterator& other) const { return map_it != other.map_it; }
    };
    
    using iterator = const_iterator;  // set only provides const iterators
    
    // Constructors
    set() = default;
    set(const set& other) = default;
    set(set&& other) = default;
    set& operator=(const set& other) = default;
    set& operator=(set&& other) = default;
    
    // Iterators
    const_iterator begin() const { return const_iterator(map_data.begin()); }
    const_iterator end() const { return const_iterator(map_data.end()); }
    const_iterator cbegin() const { return begin(); }
    const_iterator cend() const { return end(); }
    
    // Capacity
    bool empty() const { return map_data.empty(); }
    size_type size() const { return map_data.size(); }
    size_type max_size() const { return map_data.max_size(); }
    
    // Modifiers
    void clear() { map_data.clear(); }
    
    fl::Pair<const_iterator, bool> insert(const Key& key) {
        auto result = map_data.insert(fl::Pair<Key, bool>(key, true));
        return fl::Pair<const_iterator, bool>(const_iterator(result.first), result.second);
    }
    
    fl::Pair<const_iterator, bool> insert(Key&& key) {
        auto result = map_data.insert(fl::Pair<Key, bool>(fl::move(key), true));
        return fl::Pair<const_iterator, bool>(const_iterator(result.first), result.second);
    }
    
    template<typename... Args>
    fl::Pair<const_iterator, bool> emplace(Args&&... args) {
        auto result = map_data.emplace(fl::forward<Args>(args)..., true);
        return fl::Pair<const_iterator, bool>(const_iterator(result.first), result.second);
    }
    
    const_iterator erase(const_iterator pos) {
        auto map_it = map_data.erase(pos.map_it);
        return const_iterator(map_it);
    }
    
    size_type erase(const Key& key) {
        return map_data.erase(key);
    }
    
    void swap(set& other) {
        map_data.swap(other.map_data);
    }
    
    // Lookup
    size_type count(const Key& key) const {
        return map_data.count(key);
    }
    
    const_iterator find(const Key& key) const {
        return const_iterator(map_data.find(key));
    }
    
    bool contains(const Key& key) const {
        return map_data.contains(key);
    }
    
    bool has(const Key& key) const {
        return contains(key);
    }
    
    fl::Pair<const_iterator, const_iterator> equal_range(const Key& key) const {
        auto range = map_data.equal_range(key);
        return fl::Pair<const_iterator, const_iterator>(
            const_iterator(range.first), 
            const_iterator(range.second)
        );
    }
    
    const_iterator lower_bound(const Key& key) const {
        return const_iterator(map_data.lower_bound(key));
    }
    
    const_iterator upper_bound(const Key& key) const {
        return const_iterator(map_data.upper_bound(key));
    }
};

} // namespace fl
