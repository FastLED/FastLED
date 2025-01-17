#pragma once

#include <stdint.h>
#include <stddef.h>

#include "fl/namespace.h"
#include "fl/vector.h"
#include "fl/template_magic.h"
#include "fl/insert_result.h"
#include "fl/pair.h"
#include "fl/assert.h"

namespace fl {

template<typename T>
struct DefaultLess {
    bool operator()(const T& a, const T& b) const {
        return a < b;
    }
};


// A simple unordered map implementation with a fixed size.
// The user is responsible for making sure that the inserts
// do not exceed the capacity of the set, otherwise they will
// fail. Because of this limitation, this set is not a drop in
// replacement for std::map.
template<typename Key, typename Value, size_t N>
class FixedMap {
public:
    using PairKV = fl::Pair<Key, Value>;

    typedef FixedVector<PairKV, N> VectorType;
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

    template<typename Less>
    iterator lowest(Less less_than = Less()) {
        iterator lowest = end();
        for (iterator it = begin(); it != end(); ++it) {
            if (lowest == end() || less_than(it->first, lowest->first)) {
                lowest = it;
            }
        }
        return lowest;
    }

    template<typename Less>
    const_iterator lowest(Less less_than = Less()) const {
        const_iterator lowest = end();
        for (const_iterator it = begin(); it != end(); ++it) {
            if (lowest == end() || less_than(it->first, lowest->first)) {
                lowest = it;
            }
        }
        return lowest;
    }

    template<typename Less>
    iterator highest(Less less_than = Less()) {
        iterator highest = end();
        for (iterator it = begin(); it != end(); ++it) {
            if (highest == end() || less_than(highest->first, it->first)) {
                highest = it;
            }
        }
        return highest;
    }

    template<typename Less>
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

    Pair<bool, iterator> insert(const Key& key, const Value& value, InsertResult* result = nullptr) {
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
        //return false;
        return {false, end()};
    }

    bool update(const Key& key, const Value& value, bool insert_if_missing = true) {
        iterator it = find(key);
        if (it != end()) {
            it->second = value;
            return true;
        } else if (insert_if_missing) {
            return insert(key, value).first;
        }
        return false;
    }

    Value& operator[](const Key& key) {
        iterator it = find(key);
        if (it != end()) {
            return it->second;
        }
        data.push_back(PairKV(key, Value()));
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

    bool contains(const Key& key) const {
        return has(key);
    }
private:
    VectorType data;
};


template <typename Key, typename Value, typename Less = fl::DefaultLess<Key>>
class SortedHeapMap {
private:
    struct Pair {
        Key first;
        Value second;
        
        Pair(const Key& k = Key(), const Value& v = Value()) 
            : first(k), second(v) {}
    };

    struct PairLess {
        Less less;
        bool operator()(const Pair& a, const Pair& b) const {
            return less(a.first, b.first);
        }
    };

    SortedHeapVector<Pair, PairLess> data;

public:

    typedef typename SortedHeapVector<Pair, PairLess>::iterator iterator;
    typedef typename SortedHeapVector<Pair, PairLess>::const_iterator const_iterator;
    typedef Pair value_type;

    SortedHeapMap(Less less = Less()) 
        : data(PairLess{less}) {
    }

    void setMaxSize(size_t n) {
        data.setMaxSize(n);
    }

    void reserve(size_t n) {
        data.reserve(n);
    }

    bool insert(const Key& key, const Value& value, InsertResult* result = nullptr) {
        return data.insert(Pair(key, value), result);
    }

    void update(const Key& key, const Value& value) {
        if (!insert(key, value)) {
            iterator it = find(key);
            it->second = value;
        }
    }

    void swap(SortedHeapMap& other) {
        data.swap(other.data);
    }

    Value& at(const Key& key) {
        iterator it = find(key);
        return it->second;
    }

    bool has(const Key& key) const {
        return data.has(Pair(key));
    }

    bool contains(const Key& key) const {
        return has(key);
    }

    bool operator==(const SortedHeapMap& other) const {
        if (size() != other.size()) {
            return false;
        }
        for (const_iterator it = begin(), other_it = other.begin();
             it != end(); ++it, ++other_it) {
            if (it->first != other_it->first || it->second != other_it->second) {
                return false;
            }
        }
        return true;
    }

    bool operator!=(const SortedHeapMap& other) const {
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


    Value& operator[](const Key& key) {
        iterator it = find(key);
        if (it != end()) {
            return it->second;
        }
        Pair pair(key, Value());
        bool ok = data.insert(pair);
        FASTLED_ASSERT(ok, "Failed to insert into SortedHeapMap");
        return data.find(pair)->second;  // TODO: optimize.
    }
};

}  // namespace fl
