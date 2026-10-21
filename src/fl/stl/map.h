#pragma once

#include "fl/stl/stdint.h"

#include "fl/stl/assert.h"  // IWYU pragma: keep
#include "fl/stl/comparators.h"  // IWYU pragma: keep
#include "fl/stl/pair.h"
#include "fl/stl/type_traits.h"  // IWYU pragma: keep
#include "fl/stl/type_traits.h"  // IWYU pragma: keep
#include "fl/stl/vector.h"
#include "fl/stl/detail/rbtree.h"
#include "fl/stl/allocator.h"
#include "fl/stl/noexcept.h"

namespace fl {

// A simple unordered map implementation with a fixed size.
// The user is responsible for making sure that the inserts
// do not exceed the capacity of the set, otherwise they will
// fail. Because of this limitation, this set is not a drop in
// replacement for std::map.
template <typename Key, typename Value, fl::size N> class unsorted_map_fixed {
  public:
    enum insert_result { inserted = 0, exists = 1, at_capacity = 2 };

    using PairKV = fl::pair<Key, Value>;

    typedef FixedVector<PairKV, N> VectorType;
    typedef typename VectorType::iterator iterator;
    typedef typename VectorType::const_iterator const_iterator;

    // Constructor
    constexpr unsorted_map_fixed() FL_NO_EXCEPT = default;

    iterator begin() FL_NO_EXCEPT { return data.begin(); }
    iterator end() FL_NO_EXCEPT { return data.end(); }
    const_iterator begin() const FL_NO_EXCEPT { return data.begin(); }
    const_iterator end() const FL_NO_EXCEPT { return data.end(); }

    iterator find(const Key &key) FL_NO_EXCEPT {
        for (auto it = begin(); it != end(); ++it) {
            if (it->first == key) {
                return it;
            }
        }
        return end();
    }

    const_iterator find(const Key &key) const FL_NO_EXCEPT {
        for (auto it = begin(); it != end(); ++it) {
            if (it->first == key) {
                return it;
            }
        }
        return end();
    }

    template <typename Less> iterator lowest(Less less_than = Less()) FL_NO_EXCEPT {
        iterator lowest = end();
        for (iterator it = begin(); it != end(); ++it) {
            if (lowest == end() || less_than(it->first, lowest->first)) {
                lowest = it;
            }
        }
        return lowest;
    }

    template <typename Less>
    const_iterator lowest(Less less_than = Less()) const FL_NO_EXCEPT {
        const_iterator lowest = end();
        for (const_iterator it = begin(); it != end(); ++it) {
            if (lowest == end() || less_than(it->first, lowest->first)) {
                lowest = it;
            }
        }
        return lowest;
    }

    template <typename Less> iterator highest(Less less_than = Less()) FL_NO_EXCEPT {
        iterator highest = end();
        for (iterator it = begin(); it != end(); ++it) {
            if (highest == end() || less_than(highest->first, it->first)) {
                highest = it;
            }
        }
        return highest;
    }

    template <typename Less>
    const_iterator highest(Less less_than = Less()) const FL_NO_EXCEPT {
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
    bool get(const Key &key, Value *value) const FL_NO_EXCEPT {
        const_iterator it = find(key);
        if (it != end()) {
            *value = it->second;
            return true;
        }
        return false;
    }

    Value get(const Key &key, bool *has = nullptr) const FL_NO_EXCEPT {
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

    pair<bool, iterator> insert(const Key &key, const Value &value,
                                insert_result *result = nullptr) FL_NO_EXCEPT {
        iterator it = find(key);
        if (it != end()) {
            if (result) {
                *result = exists;
            }
            return {false, it};
        }
        if (data.size() < N) {
            data.push_back(PairKV(key, value));
            if (result) {
                *result = inserted;
            }
            return {true, data.end() - 1};
        }
        if (result) {
            *result = at_capacity;
        }
        return {false, end()};
    }

    pair<bool, iterator> insert(Key &&key, Value &&value,
                                insert_result *result = nullptr) FL_NO_EXCEPT {
        iterator it = find(key);
        if (it != end()) {
            if (result) {
                *result = exists;
            }
            return {false, it};
        }
        if (data.size() < N) {
            data.push_back(PairKV(fl::move(key), fl::move(value)));
            if (result) {
                *result = inserted;
            }
            return {true, data.end() - 1};
        }
        if (result) {
            *result = at_capacity;
        }
        return {false, end()};
    }

    bool update(const Key &key, const Value &value,
                bool insert_if_missing = true) FL_NO_EXCEPT {
        iterator it = find(key);
        if (it != end()) {
            it->second = value;
            return true;
        } else if (insert_if_missing) {
            return insert(key, value).first;
        }
        return false;
    }

    // Move version of update
    bool update(const Key &key, Value &&value,
                bool insert_if_missing = true) FL_NO_EXCEPT {
        iterator it = find(key);
        if (it != end()) {
            it->second = fl::move(value);
            return true;
        } else if (insert_if_missing) {
            return insert(key, fl::move(value)).first;
        }
        return false;
    }

    Value &operator[](const Key &key) FL_NO_EXCEPT {
        iterator it = find(key);
        if (it != end()) {
            return it->second;
        }
        data.push_back(PairKV(key, Value()));
        return data.back().second;
    }

    const Value &operator[](const Key &key) const FL_NO_EXCEPT {
        const_iterator it = find(key);
        if (it != end()) {
            return it->second;
        }
        static Value default_value;
        return default_value;
    }

    bool next(const Key &key, Key *next_key,
              bool allow_rollover = false) const FL_NO_EXCEPT {
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
              bool allow_rollover = false) const FL_NO_EXCEPT {
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
    constexpr fl::size size() const FL_NO_EXCEPT { return data.size(); }

    constexpr bool empty() const FL_NO_EXCEPT { return data.empty(); }

    // Get the capacity of the vector
    constexpr fl::size capacity() const FL_NO_EXCEPT { return N; }

    // Clear the vector
    void clear() FL_NO_EXCEPT { data.clear(); }

    bool has(const Key &it) const FL_NO_EXCEPT { return find(it) != end(); }

    bool contains(const Key &key) const FL_NO_EXCEPT { return has(key); }

    // Erase element by key
    fl::size erase(const Key &key) FL_NO_EXCEPT {
        iterator it = find(key);
        if (it != end()) {
            data.erase(it);
            return 1;
        }
        return 0;
    }

  private:
    VectorType data;
};


} // namespace fl

// Drop-in replacement for std::map
namespace fl {

// Default map uses slab allocator for better performance
// In fl:: namespace, "map" refers to the container (map data structure)
// The Arduino map() function exists only in global namespace (not in fl::)
template <typename Key, typename T, typename Compare = fl::less<Key>>
using map = MapRedBlackTree<Key, T, Compare, fl::allocator_slab<char>>;

// Legacy alias for backward compatibility
template <typename Key, typename T, typename Compare = fl::less<Key>>
using fl_map = map<Key, T, Compare>;

} // namespace fl
