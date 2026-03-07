#pragma once

#include "fl/stl/stdint.h"

#include "fl/stl/assert.h"  // IWYU pragma: keep
#include "fl/stl/comparators.h"  // IWYU pragma: keep
#include "fl/insert_result.h"
#include "fl/stl/pair.h"
#include "fl/stl/type_traits.h"  // IWYU pragma: keep
#include "fl/stl/type_traits.h"  // IWYU pragma: keep
#include "fl/stl/vector.h"
#include "fl/stl/detail/rbtree.h"
#include "fl/stl/allocator.h"

namespace fl {

// A simple unordered map implementation with a fixed size.
// The user is responsible for making sure that the inserts
// do not exceed the capacity of the set, otherwise they will
// fail. Because of this limitation, this set is not a drop in
// replacement for std::map.
template <typename Key, typename Value, fl::size N> class FixedMap {
  public:
    using PairKV = fl::pair<Key, Value>;

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

    pair<bool, iterator> insert(const Key &key, const Value &value,
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

    // Move version of insert
    pair<bool, iterator> insert(Key &&key, Value &&value,
                                InsertResult *result = nullptr) {
        iterator it = find(key);
        if (it != end()) {
            if (result) {
                *result = InsertResult::kExists;
            }
            return {false, it};
        }
        if (data.size() < N) {
            data.push_back(PairKV(fl::move(key), fl::move(value)));
            if (result) {
                *result = InsertResult::kInserted;
            }
            return {true, data.end() - 1};
        }
        if (result) {
            *result = InsertResult::kMaxSize;
        }
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

    // Move version of update
    bool update(const Key &key, Value &&value,
                bool insert_if_missing = true) {
        iterator it = find(key);
        if (it != end()) {
            it->second = fl::move(value);
            return true;
        } else if (insert_if_missing) {
            return insert(key, fl::move(value)).first;
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
    constexpr fl::size size() const { return data.size(); }

    constexpr bool empty() const { return data.empty(); }

    // Get the capacity of the vector
    constexpr fl::size capacity() const { return N; }

    // Clear the vector
    void clear() { data.clear(); }

    bool has(const Key &it) const { return find(it) != end(); }

    bool contains(const Key &key) const { return has(key); }

    // Erase element by key
    fl::size erase(const Key &key) {
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
