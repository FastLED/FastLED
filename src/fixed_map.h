#pragma once

#include <stdint.h>
#include <stddef.h>

#include "namespace.h"
#include "fixed_vector.h"
#include "template_magic.h"

FASTLED_NAMESPACE_BEGIN

// A simple unordered map implementation with a fixed size.
// The user is responsible for making sure that the inserts
// do not exceed the capacity of the set, otherwise they will
// fail. Because of this limitation, this set is not a drop in
// replacement for std::map.
template<typename Key, typename Value, size_t N>
class FixedMap {
public:
    struct Pair {
        Key first = Key();
        Value second = Value();
        Pair(const Key& k, const Value& v) : first(k), second(v) {}
    };

    typedef FixedVector<Pair, N> VectorType;
    typedef typename VectorType::iterator iterator;
    typedef typename VectorType::const_iterator const_iterator;


    // Constructor
    constexpr FixedMap() = default;

    iterator begin() {
        return data.begin();
    }
    iterator end() {
        return data.end();
    }
    const_iterator begin() const {
        return data.begin();
    }
    const_iterator end() const {
        return data.end();
    }

    iterator find(const Key& key) {
        for (auto it = begin(); it != end(); ++it) {
            if (it->first == key) {
                return it;
            }
        }
        return end();
    }

    const_iterator find(const Key& key) const {
        for (auto it = begin(); it != end(); ++it) {
            if (it->first == key) {
                return it;
            }
        }
        return end();
    }

    template<typename LessThan>
    iterator lowest(LessThan less_than = LessThan()) {
        iterator lowest = end();
        for (iterator it = begin(); it != end(); ++it) {
            if (lowest == end() || less_than(it->first, lowest->first)) {
                lowest = it;
            }
        }
        return lowest;
    }

    template<typename LessThan>
    const_iterator lowest(LessThan less_than = LessThan()) const {
        const_iterator lowest = end();
        for (const_iterator it = begin(); it != end(); ++it) {
            if (lowest == end() || less_than(it->first, lowest->first)) {
                lowest = it;
            }
        }
        return lowest;
    }

    template<typename LessThan>
    iterator highest(LessThan less_than = LessThan()) {
        iterator highest = end();
        for (iterator it = begin(); it != end(); ++it) {
            if (highest == end() || less_than(highest->first, it->first)) {
                highest = it;
            }
        }
        return highest;
    }

    template<typename LessThan>
    const_iterator highest(LessThan less_than = LessThan()) const {
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
    bool get(const Key& key, Value* value) const {
        const_iterator it = find(key);
        if (it != end()) {
            *value = it->second;
            return true;
        }
        return false;
    }

    Value get(const Key& key, bool* has=nullptr) const {
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

    bool insert(const Key& key, const Value& value) {
        iterator it = find(key);
        if (it == end()) {
            if (data.size() < N) {
                data.push_back(Pair(key, value));
                return true;
            }
        }
        return false;
    }

    bool update(const Key& key, const Value& value, bool insert_if_missing = true) {
        iterator it = find(key);
        if (it != end()) {
            it->second = value;
            return true;
        } else if (insert_if_missing) {
            return insert(key, value);
        }
        return false;
    }

    Value& operator[](const Key& key) {
        iterator it = find(key);
        if (it != end()) {
            return it->second;
        }
        data.push_back(Pair(key, Value()));
        return data.back().second;
    }

    const Value& operator[](const Key& key) const {
        const_iterator it = find(key);
        if (it != end()) {
            return it->second;
        }
        static Value default_value;
        return default_value;
    }

    bool next(const Key& key, Key* next_key, bool allow_rollover = false) const {
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

    bool prev(const Key& key, Key* prev_key, bool allow_rollover = false) const {
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
    constexpr size_t size() const {
        return data.size();
    }

    constexpr bool empty() const {
        return data.empty();
    }

    // Get the capacity of the vector
    constexpr size_t capacity() const {
        return N;
    }

    // Clear the vector
    void clear() {
        data.clear();
    }

    bool has(const Key& it) const {
        return find(it) != end();
    }
private:
    VectorType data;
};

template <typename Key, typename Value, typename LessThan>
class SortedHeapMap {
private:
    struct Pair {
        Key first;
        Value second;
        
        Pair(const Key& k = Key(), const Value& v = Value()) 
            : first(k), second(v) {}
    };

    struct PairLess {
        LessThan less;
        bool operator()(const Pair& a, const Pair& b) const {
            return less(a.first, b.first);
        }
    };

    SortedHeapVector<Pair, PairLess> data;

public:

    typedef typename SortedHeapVector<Pair, PairLess>::iterator iterator;
    typedef typename SortedHeapVector<Pair, PairLess>::const_iterator const_iterator;

    SortedHeapMap(size_t capacity, LessThan less = LessThan()) 
        : data(capacity, PairLess{less}) {}

    bool insert(const Key& key, const Value& value) {
        return data.insert(Pair(key, value));
    }

    bool has(const Key& key) const {
        return data.has(Pair(key));
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

    iterator find(const Key& key) {
        return data.find(Pair(key));
    }
    const_iterator find(const Key& key) const {
        return data.find(Pair(key));
    }

    bool erase(const Key& key) {
        return data.erase(Pair(key));
    }
    bool erase(iterator it) {
        return data.erase(it);
    }

    iterator lower_bound(const Key& key) {
        return data.lower_bound(Pair(key));
    }

    const_iterator lower_bound(const Key& key) const {
        return data.lower_bound(Pair(key));
    }

    iterator upper_bound(const Key& key) {
        iterator it = lower_bound(key);
        if (it != end() && it->first == key) {
            ++it;
        }
        return it;
    }

    const_iterator upper_bound(const Key& key) const {
        const_iterator it = lower_bound(key);
        if (it != end() && it->first == key) {
            ++it;
        }
        return it;
    }

    Pair& front() { return data.front(); }
    const Pair& front() const { return data.front(); }
    Pair& back() { return data.back(); }
    const Pair& back() const { return data.back(); }


};

FASTLED_NAMESPACE_END
