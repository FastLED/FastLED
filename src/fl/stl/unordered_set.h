#pragma once


#include "fl/stl/unordered_map.h"
#include "fl/hash.h"
#include "fl/stl/move.h"
#include "fl/stl/type_traits.h"
#include "fl/int.h"

namespace fl {


template <typename Key, typename Hash = Hash<Key>,
          typename KeyEqual = EqualTo<Key>>
class unordered_set {
  public:
    // Iterator support - wrap the underlying map's iterators
    class iterator {
    private:
        using map_iterator = typename fl::unordered_map<Key, bool, Hash, KeyEqual>::iterator;
        map_iterator mIt;
    
    public:
        using value_type = Key;
        using reference = const Key&;
        using pointer = const Key*;
        
        iterator() = default;
        explicit iterator(map_iterator it) : mIt(it) {}
        
        reference operator*() const { return mIt->first; }
        pointer operator->() const { return &(mIt->first); }
        
        iterator& operator++() { ++mIt; return *this; }
        iterator operator++(int) { iterator tmp = *this; ++mIt; return tmp; }
        
        bool operator==(const iterator& other) const { return mIt == other.mIt; }
        bool operator!=(const iterator& other) const { return mIt != other.mIt; }
        
        friend class unordered_set;
    };
    
    class const_iterator {
    private:
        using map_const_iterator = typename fl::unordered_map<Key, bool, Hash, KeyEqual>::const_iterator;
        map_const_iterator mIt;
    
    public:
        using value_type = Key;
        using reference = const Key&;
        using pointer = const Key*;
        
        const_iterator() = default;
        explicit const_iterator(map_const_iterator it) : mIt(it) {}
        const_iterator(const iterator& other) : mIt(static_cast<map_const_iterator>(other.mIt)) {}
        
        reference operator*() const { return mIt->first; }
        pointer operator->() const { return &(mIt->first); }
        
        const_iterator& operator++() { ++mIt; return *this; }
        const_iterator operator++(int) { const_iterator tmp = *this; ++mIt; return tmp; }
        
        bool operator==(const const_iterator& other) const { return mIt == other.mIt; }
        bool operator!=(const const_iterator& other) const { return mIt != other.mIt; }
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
        return data.insert(key, true).second;
    }

    // Move version of insert
    bool insert(Key &&key) {
        return data.insert(fl::move(key), true).second;
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
    fl::unordered_map<Key, bool, Hash, KeyEqual> data;
};

} // namespace fl
