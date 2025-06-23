#pragma once

#include <stddef.h>
#include "fl/stdint.h"

#include "fl/assert.h"
#include "fl/insert_result.h"
#include "fl/namespace.h"
#include "fl/pair.h"
#include "fl/template_magic.h"
#include "fl/vector.h"

namespace fl {

template <typename T> struct DefaultLess {
    bool operator()(const T &a, const T &b) const { return a < b; }
};

// A simple unordered map implementation with a fixed size.
// The user is responsible for making sure that the inserts
// do not exceed the capacity of the set, otherwise they will
// fail. Because of this limitation, this set is not a drop in
// replacement for std::map.
template <typename Key, typename Value, size_t N> class FixedMap {
  public:
    using PairKV = fl::Pair<Key, Value>;

    typedef FixedVector<PairKV, N> VectorType;
    typedef typename VectorType::iterator iterator;
    typedef typename VectorType::const_iterator const_iterator;

    // Constructor
    constexpr FixedMap() = default;

    iterator begin() { return data.begin(); }
    iterator end() { return data.end(); }
    const_iterator begin() const { return data.begin(); }
    const_iterator end() const { return data.end(); }

    iterator find(const Key &key) {
        for (auto it = begin(); it != end(); ++it) {
            if (it->first == key) {
                return it;
            }
        }
        return end();
    }

    const_iterator find(const Key &key) const {
        for (auto it = begin(); it != end(); ++it) {
            if (it->first == key) {
                return it;
            }
        }
        return end();
    }

    template <typename Less> iterator lowest(Less less_than = Less()) {
        iterator lowest = end();
        for (iterator it = begin(); it != end(); ++it) {
            if (lowest == end() || less_than(it->first, lowest->first)) {
                lowest = it;
            }
        }
        return lowest;
    }

    template <typename Less>
    const_iterator lowest(Less less_than = Less()) const {
        const_iterator lowest = end();
        for (const_iterator it = begin(); it != end(); ++it) {
            if (lowest == end() || less_than(it->first, lowest->first)) {
                lowest = it;
            }
        }
        return lowest;
    }

    template <typename Less> iterator highest(Less less_than = Less()) {
        iterator highest = end();
        for (iterator it = begin(); it != end(); ++it) {
            if (highest == end() || less_than(highest->first, it->first)) {
                highest = it;
            }
        }
        return highest;
    }

    template <typename Less>
    const_iterator highest(Less less_than = Less()) const {
        const_iterator highest = end();
        for (const_iterator it = begin(); it != end(); ++it) {
            if (highest == end() || less_than(highest->first, it->first)) {
                highest = it;
            }
        }
        return highest;
    }

    // We differ from the std standard here so that we don't allow
    // dereferencing the end iterator.
    bool get(const Key &key, Value *value) const {
        const_iterator it = find(key);
        if (it != end()) {
            *value = it->second;
            return true;
        }
        return false;
    }

    Value get(const Key &key, bool *has = nullptr) const {
        const_iterator it = find(key);
        if (it != end()) {
            if (has) {
                *has = true;
            }
            return it->second;
        }
        if (has) {
            *has = false;
        }
        return Value();
    }

    Pair<bool, iterator> insert(const Key &key, const Value &value,
                                InsertResult *result = nullptr) {
        iterator it = find(key);
        if (it != end()) {
            if (result) {
                *result = InsertResult::kExists;
            }
            // return false;
            return {false, it};
        }
        if (data.size() < N) {
            data.push_back(PairKV(key, value));
            if (result) {
                *result = InsertResult::kInserted;
            }
            // return true;
            return {true, data.end() - 1};
        }
        if (result) {
            *result = InsertResult::kMaxSize;
        }
        // return false;
        return {false, end()};
    }

    bool update(const Key &key, const Value &value,
                bool insert_if_missing = true) {
        iterator it = find(key);
        if (it != end()) {
            it->second = value;
            return true;
        } else if (insert_if_missing) {
            return insert(key, value).first;
        }
        return false;
    }

    Value &operator[](const Key &key) {
        iterator it = find(key);
        if (it != end()) {
            return it->second;
        }
        data.push_back(PairKV(key, Value()));
        return data.back().second;
    }

    const Value &operator[](const Key &key) const {
        const_iterator it = find(key);
        if (it != end()) {
            return it->second;
        }
        static Value default_value;
        return default_value;
    }

    bool next(const Key &key, Key *next_key,
              bool allow_rollover = false) const {
        const_iterator it = find(key);
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

    bool prev(const Key &key, Key *prev_key,
              bool allow_rollover = false) const {
        const_iterator it = find(key);
        if (it != end()) {
            if (it != begin()) {
                --it;
                *prev_key = it->first;
                return true;
            } else if (allow_rollover && !empty()) {
                *prev_key = data[data.size() - 1].first;
                return true;
            }
        }
        return false;
    }

    // Get the current size of the vector
    constexpr size_t size() const { return data.size(); }

    constexpr bool empty() const { return data.empty(); }

    // Get the capacity of the vector
    constexpr size_t capacity() const { return N; }

    // Clear the vector
    void clear() { data.clear(); }

    bool has(const Key &it) const { return find(it) != end(); }

    bool contains(const Key &key) const { return has(key); }

  private:
    VectorType data;
};

// Closest data structure to an std::map. Always sorted.
// O(n + log(n)) for insertions, O(log(n)) for searches, O(n) for iteration.
template <typename Key, typename Value, typename Less = fl::DefaultLess<Key>>
class SortedHeapMap {
  public:
    struct Pair {
        Key first;
        Value second;

        Pair(const Key &k = Key(), const Value &v = Value())
            : first(k), second(v) {}
    };

  private:

    struct PairLess {
        Less less;
        bool operator()(const Pair &a, const Pair &b) const {
            return less(a.first, b.first);
        }
    };

    SortedHeapVector<Pair, PairLess> data;

  public:
    typedef typename SortedHeapVector<Pair, PairLess>::iterator iterator;
    typedef typename SortedHeapVector<Pair, PairLess>::const_iterator
        const_iterator;
    typedef Pair value_type;

    SortedHeapMap(Less less = Less()) : data(PairLess{less}) {}

    void setMaxSize(size_t n) { data.setMaxSize(n); }

    void reserve(size_t n) { data.reserve(n); }

    bool insert(const Key &key, const Value &value,
                InsertResult *result = nullptr) {
        return data.insert(Pair(key, value), result);
    }

    void update(const Key &key, const Value &value) {
        if (!insert(key, value)) {
            iterator it = find(key);
            it->second = value;
        }
    }

    void swap(SortedHeapMap &other) { data.swap(other.data); }

    Value &at(const Key &key) {
        iterator it = find(key);
        return it->second;
    }

    // Matches FixedMap<>::get(...).
    bool get(const Key &key, Value *value) const {
        const_iterator it = find(key);
        if (it != end()) {
            *value = it->second;
            return true;
        }
        return false;
    }

    bool has(const Key &key) const { return data.has(Pair(key)); }

    bool contains(const Key &key) const { return has(key); }

    bool operator==(const SortedHeapMap &other) const {
        if (size() != other.size()) {
            return false;
        }
        for (const_iterator it = begin(), other_it = other.begin(); it != end();
             ++it, ++other_it) {
            if (it->first != other_it->first ||
                it->second != other_it->second) {
                return false;
            }
        }
        return true;
    }

    bool operator!=(const SortedHeapMap &other) const {
        return !(*this == other);
    }

    size_t size() const { return data.size(); }
    bool empty() const { return data.empty(); }
    bool full() const { return data.full(); }
    size_t capacity() const { return data.capacity(); }
    void clear() { data.clear(); }

    // begin, dend
    iterator begin() { return data.begin(); }
    iterator end() { return data.end(); }
    const_iterator begin() const { return data.begin(); }
    const_iterator end() const { return data.end(); }

    iterator find(const Key &key) { return data.find(Pair(key)); }
    const_iterator find(const Key &key) const { return data.find(Pair(key)); }

    bool erase(const Key &key) { return data.erase(Pair(key)); }
    bool erase(iterator it) { return data.erase(it); }

    iterator lower_bound(const Key &key) { return data.lower_bound(Pair(key)); }

    const_iterator lower_bound(const Key &key) const {
        return data.lower_bound(Pair(key));
    }

    iterator upper_bound(const Key &key) {
        iterator it = lower_bound(key);
        if (it != end() && it->first == key) {
            ++it;
        }
        return it;
    }

    const_iterator upper_bound(const Key &key) const {
        const_iterator it = lower_bound(key);
        if (it != end() && it->first == key) {
            ++it;
        }
        return it;
    }

    Pair &front() { return data.front(); }
    const Pair &front() const { return data.front(); }
    Pair &back() { return data.back(); }
    const Pair &back() const { return data.back(); }

    Value &operator[](const Key &key) {
        iterator it = find(key);
        if (it != end()) {
            return it->second;
        }
        Pair pair(key, Value());
        bool ok = data.insert(pair);
        FASTLED_ASSERT(ok, "Failed to insert into SortedHeapMap");
        return data.find(pair)->second; // TODO: optimize.
    }
};

// Generic fl::map template - drop-in replacement for std::map<Key, Value>
// Uses SortedHeapMap internally to provide std::map behavior and guarantees:
// - Keys are always sorted
// - O(log n) search complexity 
// - Unique keys only
// - Iterator invalidation behavior similar to std::map
template <typename Key, typename Value, typename Compare = fl::DefaultLess<Key>>
class map {
public:
    // Standard typedefs to match std::map interface
    typedef Key key_type;
    typedef Value mapped_type;
    typedef fl::Pair<const Key, Value> value_type;  // Note: const Key like std::map
    typedef Compare key_compare;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    
    // Use SortedHeapMap internally but expose std::map-compatible interface
private:
    typedef SortedHeapMap<Key, Value, Compare> internal_map_type;
    typedef typename internal_map_type::Pair internal_pair_type;
    
    internal_map_type impl_;

public:
    // Iterator adapters that safely convert between internal pair and std::map-style pair
    class iterator {
        friend class map;
        typedef typename internal_map_type::iterator internal_iterator;
        internal_iterator it_;
        iterator(internal_iterator it) : it_(it) {}
        
        // Helper to create std::map-style pair on demand
        mutable value_type cached_pair_;
        mutable bool cache_valid_ = false;
        
        void update_cache() const {
            if (!cache_valid_) {
                // Safe copy construction - avoid reinterpret_cast issues
                const_cast<Key&>(cached_pair_.first) = it_->first;
                cached_pair_.second = it_->second;
                cache_valid_ = true;
            }
        }
        
    public:
        typedef value_type& reference;
        typedef value_type* pointer;
        
        reference operator*() const { 
            update_cache();
            return cached_pair_; 
        }
        pointer operator->() const { 
            update_cache();
            return &cached_pair_; 
        }
        
        iterator& operator++() { ++it_; cache_valid_ = false; return *this; }
        iterator operator++(int) { iterator tmp = *this; ++it_; cache_valid_ = false; return tmp; }
        iterator& operator--() { --it_; cache_valid_ = false; return *this; }
        iterator operator--(int) { iterator tmp = *this; --it_; cache_valid_ = false; return tmp; }
        
        bool operator==(const iterator& other) const { return it_ == other.it_; }
        bool operator!=(const iterator& other) const { return it_ != other.it_; }
        
        // Allow access to underlying key/value for direct modification
        Key& key() const { 
            FASTLED_ASSERT(it_ != internal_iterator{}, "Cannot access key of end() iterator");
            return it_->first; 
        }
        Value& value() const { 
            FASTLED_ASSERT(it_ != internal_iterator{}, "Cannot access value of end() iterator");
            return it_->second; 
        }
    };
    
    class const_iterator {
        friend class map;
        typedef typename internal_map_type::const_iterator internal_const_iterator;
        internal_const_iterator it_;
        const_iterator(internal_const_iterator it) : it_(it) {}
        
        // Helper to create std::map-style pair on demand
        mutable value_type cached_pair_;
        mutable bool cache_valid_ = false;
        
        void update_cache() const {
            if (!cache_valid_) {
                // Safe copy construction
                const_cast<Key&>(cached_pair_.first) = it_->first;
                const_cast<Value&>(cached_pair_.second) = it_->second;
                cache_valid_ = true;
            }
        }
        
    public:
        typedef const value_type& reference;
        typedef const value_type* pointer;
        
        reference operator*() const { 
            update_cache();
            return cached_pair_; 
        }
        pointer operator->() const { 
            update_cache();
            return &cached_pair_; 
        }
        
        const_iterator& operator++() { ++it_; cache_valid_ = false; return *this; }
        const_iterator operator++(int) { const_iterator tmp = *this; ++it_; cache_valid_ = false; return tmp; }
        const_iterator& operator--() { --it_; cache_valid_ = false; return *this; }
        const_iterator operator--(int) { const_iterator tmp = *this; --it_; cache_valid_ = false; return tmp; }
        
        bool operator==(const const_iterator& other) const { return it_ == other.it_; }
        bool operator!=(const const_iterator& other) const { return it_ != other.it_; }
        
        // Allow access to underlying key/value  
        const Key& key() const { 
            FASTLED_ASSERT(it_ != internal_const_iterator{}, "Cannot access key of end() iterator");
            return it_->first; 
        }
        const Value& value() const { 
            FASTLED_ASSERT(it_ != internal_const_iterator{}, "Cannot access value of end() iterator");
            return it_->second; 
        }
    };

    // Constructors
    map() = default;
    explicit map(const Compare& comp) : impl_(comp) {}
    
    // Iterators
    iterator begin() { return iterator(impl_.begin()); }
    iterator end() { return iterator(impl_.end()); }
    const_iterator begin() const { return const_iterator(impl_.begin()); }
    const_iterator end() const { return const_iterator(impl_.end()); }
    const_iterator cbegin() const { return const_iterator(impl_.begin()); }
    const_iterator cend() const { return const_iterator(impl_.end()); }
    
    // Capacity
    bool empty() const { return impl_.empty(); }
    size_type size() const { return impl_.size(); }
    size_type max_size() const { return impl_.capacity(); }
    
    // Element access
    Value& operator[](const Key& key) { return impl_[key]; }
    Value& at(const Key& key) { return impl_.at(key); }
    const Value& at(const Key& key) const { 
        auto it = impl_.find(key);
        if (it == impl_.end()) {
            // std::map throws std::out_of_range, but we'll use assert for embedded
            FASTLED_ASSERT(false, "fl::map::at: key not found");
        }
        return it->second;
    }
    
    // Modifiers
    fl::Pair<iterator, bool> insert(const value_type& value) {
        InsertResult result;
        bool success = impl_.insert(value.first, value.second, &result);
        auto it = impl_.find(value.first);
        return fl::Pair<iterator, bool>(iterator(it), success);
    }
    
    fl::Pair<iterator, bool> insert(const Key& key, const Value& value) {
        InsertResult result;
        bool success = impl_.insert(key, value, &result);
        auto it = impl_.find(key);
        return fl::Pair<iterator, bool>(iterator(it), success);
    }
    
    template<typename InputIt>
    void insert(InputIt first, InputIt last) {
        for (auto it = first; it != last; ++it) {
            insert(*it);
        }
    }
    
    size_type erase(const Key& key) {
        return impl_.erase(key) ? 1 : 0;
    }
    
    iterator erase(iterator pos) {
        // Get key to erase and determine next key
        Key key_to_erase = pos.it_->first;
        auto next_internal_it = pos.it_;
        ++next_internal_it;
        
        // Store the next key if there is one
        Key next_key;
        bool has_next = (next_internal_it != impl_.end());
        if (has_next) {
            next_key = next_internal_it->first;
        }
        
        // Erase the key
        impl_.erase(key_to_erase);
        
        // Find and return iterator to next element (or end if no next element)
        if (has_next) {
            return iterator(impl_.find(next_key));
        } else {
            return iterator(impl_.end());
        }
    }
    
    iterator erase(iterator first, iterator last) {
        // Determine what key should be after the erased range
        Key after_range_key;
        bool has_after_range = (last.it_ != impl_.end());
        if (has_after_range) {
            after_range_key = last.it_->first;
        }
        
        // Collect keys to erase first to avoid iterator invalidation issues
        fl::vector<Key> keys_to_erase;
        for (auto it = first; it != last; ++it) {
            keys_to_erase.push_back(it.it_->first);
        }
        
        // Erase all the keys
        for (const auto& key : keys_to_erase) {
            impl_.erase(key);
        }
        
        // Return iterator to position after the range
        if (has_after_range) {
            return iterator(impl_.find(after_range_key));
        } else {
            return iterator(impl_.end());
        }
    }
    
    void clear() { impl_.clear(); }
    
    void swap(map& other) { impl_.swap(other.impl_); }
    
    // Lookup
    size_type count(const Key& key) const {
        return impl_.has(key) ? 1 : 0;
    }
    
    iterator find(const Key& key) {
        return iterator(impl_.find(key));
    }
    
    const_iterator find(const Key& key) const {
        return const_iterator(impl_.find(key));
    }
    
    fl::Pair<iterator, iterator> equal_range(const Key& key) {
        auto it = find(key);
        if (it != end()) {
            auto next_it = it;
            ++next_it;
            return fl::Pair<iterator, iterator>(it, next_it);
        }
        return fl::Pair<iterator, iterator>(it, it);
    }
    
    fl::Pair<const_iterator, const_iterator> equal_range(const Key& key) const {
        auto it = find(key);
        if (it != end()) {
            auto next_it = it;
            ++next_it;
            return fl::Pair<const_iterator, const_iterator>(it, next_it);
        }
        return fl::Pair<const_iterator, const_iterator>(it, it);
    }
    
    iterator lower_bound(const Key& key) {
        return iterator(impl_.lower_bound(key));
    }
    
    const_iterator lower_bound(const Key& key) const {
        return const_iterator(impl_.lower_bound(key));
    }
    
    iterator upper_bound(const Key& key) {
        return iterator(impl_.upper_bound(key));
    }
    
    const_iterator upper_bound(const Key& key) const {
        return const_iterator(impl_.upper_bound(key));
    }
    
    // Comparison operators
    bool operator==(const map& other) const { return impl_ == other.impl_; }
    bool operator!=(const map& other) const { return impl_ != other.impl_; }
    
    // Additional FastLED-specific methods (maintain compatibility with existing code)
    void setMaxSize(size_t n) { impl_.setMaxSize(n); }
    void reserve(size_t n) { impl_.reserve(n); }
    bool full() const { return impl_.full(); }
    size_type capacity() const { return impl_.capacity(); }
    
    // FastLED-style get method for compatibility
    bool get(const Key& key, Value* value) const { return impl_.get(key, value); }
    bool has(const Key& key) const { return impl_.has(key); }
    bool contains(const Key& key) const { return impl_.contains(key); }
};

} // namespace fl
