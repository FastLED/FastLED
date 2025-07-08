#pragma once

//#include <stddef.h>
#include "fl/stdint.h"

#include "fl/namespace.h"
#include "fl/vector.h"
#include "fl/hash_map.h"
#include "fl/allocator.h"

namespace fl {


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
    fl::size count(const Key &key) const {
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
    fl::size size() const { return data.size(); }
    fl::size capacity() const { return data.capacity(); }
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
