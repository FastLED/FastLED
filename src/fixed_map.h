#pragma once

#include <stdint.h>
#include <stddef.h>

#include "namespace.h"
#include "fixed_vector.h"

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

FASTLED_NAMESPACE_END
