#pragma once

//#include <stddef.h>
#include "fl/stdint.h"

#include "fl/namespace.h"
#include "fl/vector.h"
#include "fl/hash_map.h"

namespace fl {

// A simple unordered set implementation with a fixed size.
// The user is responsible for making sure that the inserts
// do not exceed the capacity of the set, otherwise they will
// fail. Because of this limitation, this set is not a drop in
// replacement for std::set.
template <typename Key, sz N> class FixedSet {
  public:
    typedef FixedVector<Key, N> VectorType;
    typedef typename VectorType::iterator iterator;
    typedef typename VectorType::const_iterator const_iterator;

    // Constructor
    constexpr FixedSet() = default;

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
    constexpr fl::sz size() const { return data.size(); }

    constexpr bool empty() const { return data.empty(); }

    // Get the capacity of the set
    constexpr fl::sz capacity() const { return N; }

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

template <typename Key>
class unordered_set {
  public:
    // Iterator support - wrap the underlying map's iterators
    class iterator {
    private:
        using map_iterator = typename fl::unordered_map<Key, bool>::iterator;
        map_iterator it_;
    
    public:
        using value_type = Key;
        using reference = const Key&;
        using pointer = const Key*;
        
        iterator() = default;
        explicit iterator(map_iterator it) : it_(it) {}
        
        reference operator*() const { return it_->first; }
        pointer operator->() const { return &(it_->first); }
        
        iterator& operator++() { ++it_; return *this; }
        iterator operator++(int) { iterator tmp = *this; ++it_; return tmp; }
        
        bool operator==(const iterator& other) const { return it_ == other.it_; }
        bool operator!=(const iterator& other) const { return it_ != other.it_; }
        
        friend class unordered_set;
    };
    
    class const_iterator {
    private:
        using map_const_iterator = typename fl::unordered_map<Key, bool>::const_iterator;
        map_const_iterator it_;
    
    public:
        using value_type = Key;
        using reference = const Key&;
        using pointer = const Key*;
        
        const_iterator() = default;
        explicit const_iterator(map_const_iterator it) : it_(it) {}
        const_iterator(const iterator& other) : it_(static_cast<map_const_iterator>(other.it_)) {}
        
        reference operator*() const { return it_->first; }
        pointer operator->() const { return &(it_->first); }
        
        const_iterator& operator++() { ++it_; return *this; }
        const_iterator operator++(int) { const_iterator tmp = *this; ++it_; return tmp; }
        
        bool operator==(const const_iterator& other) const { return it_ == other.it_; }
        bool operator!=(const const_iterator& other) const { return it_ != other.it_; }
    };

    unordered_set() = default;
    ~unordered_set() = default;

    // Iterator methods
    iterator begin() { return iterator(data.begin()); }
    iterator end() { return iterator(data.end()); }
    const_iterator begin() const { return const_iterator(data.begin()); }
    const_iterator end() const { return const_iterator(data.end()); }
    const_iterator cbegin() const { return const_iterator(data.cbegin()); }
    const_iterator cend() const { return const_iterator(data.cend()); }

    // Insert operations
    bool insert(const Key &key) {
        return data.insert(key, true);
    }
    
    // Move version of insert
    bool insert(Key &&key) {
        return data.insert(fl::move(key), true);
    }
    
    // Emplace - construct in place
    template<typename... Args>
    bool emplace(Args&&... args) {
        Key key(fl::forward<Args>(args)...);
        return insert(fl::move(key));
    }

    // Find operations
    iterator find(const Key &key) {
        auto map_it = data.find(key);
        return iterator(map_it);
    }
    
    const_iterator find(const Key &key) const {
        auto map_it = data.find(key);
        return const_iterator(map_it);
    }
    
    // Count - returns 0 or 1 for sets
    fl::sz count(const Key &key) const {
        return has(key) ? 1 : 0;
    }

    // Erase operations
    bool erase(const Key &key) {
        return data.erase(key);
    }
    
    iterator erase(iterator pos) {
        // Extract the key from the iterator before erasing
        Key key = *pos;
        data.erase(key);
        // Return iterator to next element
        return ++pos;
    }
    
    iterator erase(const_iterator pos) {
        // Extract the key from the const_iterator before erasing
        Key key = *pos;
        data.erase(key);
        // Find the next element after erasing
        return find(key);
    }

    // Capacity operations
    fl::sz size() const { return data.size(); }
    fl::sz capacity() const { return data.capacity(); }
    bool empty() const { return data.empty(); }
    
    // Modifiers
    void clear() { data.clear(); }
    
    // Lookup operations
    bool has(const Key &key) const { return data.has(key); }
    bool contains(const Key &key) const { return has(key); } // C++20 style

  private:
    fl::unordered_map<Key, bool> data;
};

} // namespace fl
