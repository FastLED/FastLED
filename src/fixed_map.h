#pragma once

#include <stdint.h>
#include <stddef.h>

#include "namespace.h"
#include "fixed_vector.h"

FASTLED_NAMESPACE_BEGIN

template<typename Key, typename Value, size_t N>
class FixedMap {
public:
    struct Pair {
        Key key;
        Value value;
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
        for (size_t i = 0; i < data.size(); ++i) {
            if (data[i].key == key) {
                return data.begin() + i;
            }
        }
        return end();
    }

    const_iterator find(const Key& key) const {
        for (size_t i = 0; i < data.size(); ++i) {
            if (data[i].key == key) {
                return data.begin() + i;
            }
        }
        return end();
    }

    // We differ from the std standard here so that we don't allow
    // dereferencing the end iterator.
    bool get(const Key& key, Value* value) const {
        const_iterator it = find(key);
        if (it != end()) {
            *value = it->value;
            return true;
        }
        return false;
    }

    bool insert(const Key& key, const Value& value) {
        iterator it = find(key);
        if (it == end()) {
            if (data.size() < N) {
                data.push_back({key, value});
                return true;
            }
        }
        return false;
    }

    bool update(const Key& key, const Value& value, bool insert_if_missing = true) {
        Pair* pair = find(key);
        if (pair != end()) {
            pair->value = value;
            return true;
        } else if (insert_if_missing) {
            return insert(key, value);
        }
        return false;
    }

    bool next(const Key& key, Key* next_key, bool allow_rollover = false) const {
        const_iterator it = find(key);
        if (it != end()) {
            ++it;
            if (it != end()) {
                *next_key = it->key;
                return true;
            } else if (allow_rollover && !empty()) {
                *next_key = begin()->key;
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
                *prev_key = it->key;
                return true;
            } else if (allow_rollover && !empty()) {
                *prev_key = (--end())->key;
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
