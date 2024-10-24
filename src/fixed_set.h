#pragma once

#include <stdint.h>
#include <stddef.h>

#include "namespace.h"
#include "fixed_vector.h"

FASTLED_NAMESPACE_BEGIN

// A simple unordered set implementation with a fixed size.
// The user is responsible for making sure that the inserts
// do not exceed the capacity of the set, otherwise they will
// fail. Because of this limitation, this set is not a drop in
// replacement for std::set.
template<typename Key, size_t N>
class FixedSet {
public:
    typedef FixedVector<Key, N> VectorType;
    typedef typename VectorType::iterator iterator;
    typedef typename VectorType::const_iterator const_iterator;

    // Constructor
    constexpr FixedSet() = default;

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
            if (*it == key) {
                return it;
            }
        }
        return end();
    }

    const_iterator find(const Key& key) const {
        for (auto it = begin(); it != end(); ++it) {
            if (*it == key) {
                return it;
            }
        }
        return end();
    }

    bool insert(const Key& key) {
        if (data.size() < N) {
            auto it = find(key);
            if (it == end()) {
                data.push_back(key);
                return true;
            }
        }
        return false;
    }

    bool erase(const Key& key) {
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

    bool next(const Key& key, Key* next_key, bool allow_rollover = false) const {
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

    bool prev(const Key& key, Key* prev_key, bool allow_rollover = false) const {
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
    constexpr size_t size() const {
        return data.size();
    }

    constexpr bool empty() const {
        return data.empty();
    }

    // Get the capacity of the set
    constexpr size_t capacity() const {
        return N;
    }

    // Clear the set
    void clear() {
        data.clear();
    }

    bool has(const Key& key) const {
        return find(key) != end();
    }

    // Return the first element of the set
    const Key& front() const {
        return data.front();
    }

    // Return the last element of the set
    const Key& back() const {
        return data.back();
    }


private:
    VectorType data;
};

FASTLED_NAMESPACE_END
