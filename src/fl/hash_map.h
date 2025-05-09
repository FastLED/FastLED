#pragma once

/*
HashMap that is optimized for embedded devices. The hashmap
will store upto N elements inline, and will spill over to a heap.
This hashmap will try not to grow by detecting during rehash that
the number of tombstones is greater than the number of elements.
This will keep the memory from growing during multiple inserts
and removals.
*/

// #include <cstddef>
// #include <iterator>

#include "fl/assert.h"
#include "fl/bitset.h"
#include "fl/clamp.h"
#include "fl/hash.h"
#include "fl/map_range.h"
#include "fl/optional.h"
#include "fl/pair.h"
#include "fl/template_magic.h"
#include "fl/vector.h"
#include "fl/warn.h"

namespace fl {

// // begin using declarations for stl compatibility
// use fl::equal_to;
// use fl::hash_map;
// use fl::unordered_map;
// // end using declarations for stl compatibility

template <typename T> struct EqualTo {
    bool operator()(const T &a, const T &b) const { return a == b; }
};

// -- HashMap class
// ------------------------------------------------------------- Begin HashMap
// class

#ifndef FASTLED_HASHMAP_INLINED_COUNT
#define FASTLED_HASHMAP_INLINED_COUNT 8
#endif

template <typename Key, typename T, typename Hash = Hash<Key>,
          typename KeyEqual = EqualTo<Key>,
          int INLINED_COUNT = FASTLED_HASHMAP_INLINED_COUNT>
class HashMap {
  public:
    HashMap(size_t initial_capacity = FASTLED_HASHMAP_INLINED_COUNT,
            float max_load = 0.7f)
        : _buckets(next_power_of_two(initial_capacity)), _size(0),
          _tombstones(0), _occupied(next_power_of_two(initial_capacity)),
          _deleted(next_power_of_two(initial_capacity)) {
        setLoadFactor(max_load);
    }

    void setLoadFactor(float f) {
        f = fl::clamp(f, 0.f, 1.f);
        mLoadFactor = fl::map_range<float, uint8_t>(f, 0.f, 1.f, 0, 255);
    }

    // Iterator support.
    struct iterator {
        // Standard iterator typedefs
        // using difference_type = std::ptrdiff_t;
        using value_type = pair<const Key, T>;
        using pointer = value_type *;
        using reference = value_type &;
        // using iterator_category = std::forward_iterator_tag;

        iterator() : _map(nullptr), _idx(0) {}
        iterator(HashMap *m, size_t idx) : _map(m), _idx(idx) {
            advance_to_occupied();
        }

        value_type operator*() const {
            auto &e = _map->_buckets[_idx];
            return {e.key, e.value};
        }

        pointer operator->() {
            _cached_value = operator*();
            return &_cached_value;
        }

        iterator &operator++() {
            ++_idx;
            advance_to_occupied();
            return *this;
        }

        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const iterator &o) const {
            return _map == o._map && _idx == o._idx;
        }
        bool operator!=(const iterator &o) const { return !(*this == o); }

        void advance_to_occupied() {
            if (!_map)
                return;
            size_t cap = _map->_buckets.size();
            while (_idx < cap && !_map->is_occupied(_idx))
                ++_idx;
        }

      private:
        HashMap *_map;
        size_t _idx;
        mutable value_type _cached_value;
    };

    struct const_iterator {
        // Standard iterator typedefs
        // using difference_type = std::ptrdiff_t;
        using value_type = pair<const Key, T>;
        using pointer = const value_type *;
        using reference = const value_type &;
        // using iterator_category = std::forward_iterator_tag;

        const_iterator() : _map(nullptr), _idx(0) {}
        const_iterator(const HashMap *m, size_t idx) : _map(m), _idx(idx) {
            advance_to_occupied();
        }
        const_iterator(const iterator &it) : _map(it._map), _idx(it._idx) {}

        value_type operator*() const {
            auto &e = _map->_buckets[_idx];
            return {e.key, e.value};
        }

        pointer operator->() const {
            _cached_value = operator*();
            return &_cached_value;
        }

        const_iterator &operator++() {
            ++_idx;
            advance_to_occupied();
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const const_iterator &o) const {
            return _map == o._map && _idx == o._idx;
        }
        bool operator!=(const const_iterator &o) const { return !(*this == o); }

        void advance_to_occupied() {
            if (!_map)
                return;
            size_t cap = _map->_buckets.size();
            while (_idx < cap && !_map->is_occupied(_idx))
                ++_idx;
        }

        friend class HashMap;

      private:
        const HashMap *_map;
        size_t _idx;
        mutable value_type _cached_value;
    };

    iterator begin() { return iterator(this, 0); }
    iterator end() { return iterator(this, _buckets.size()); }
    const_iterator begin() const { return const_iterator(this, 0); }
    const_iterator end() const { return const_iterator(this, _buckets.size()); }
    const_iterator cbegin() const { return const_iterator(this, 0); }
    const_iterator cend() const {
        return const_iterator(this, _buckets.size());
    }

    static bool NeedsRehash(size_t size, size_t bucket_size, size_t tombstones,
                            uint8_t load_factor) {
        // (size + tombstones) << 8   : multiply numerator by 256
        // capacity * max_load : denominator * threshold
        uint32_t lhs = (size + tombstones) << 8;
        uint32_t rhs = (bucket_size * load_factor);
        return lhs > rhs;
    }

    // returns true if (size + tombs)/capacity > _max_load/256
    bool needs_rehash() const {
        return NeedsRehash(_size, _buckets.size(), _tombstones, mLoadFactor);
    }

    // insert or overwrite
    void insert(const Key &key, const T &value) {
        const bool will_rehash = needs_rehash();
        if (will_rehash) {
            // if half the buckets are tombstones, rehash inline to prevent
            // memory spill over into the heap.
            if (_tombstones > _size) {
                rehash_inline_no_resize();
            } else {
                rehash(_buckets.size() * 2);
            }
        }
        size_t idx;
        bool is_new;
        fl::pair<size_t, bool> p = find_slot(key);
        idx = p.first;
        is_new = p.second;
        if (is_new) {
            _buckets[idx].key = key;
            _buckets[idx].value = value;
            mark_occupied(idx);
            ++_size;
        } else {
            FASTLED_ASSERT(idx != npos, "HashMap::insert: invalid index at "
                                            << idx << " which is " << npos);
            _buckets[idx].value = value;
        }
    }

    // remove key; returns true if removed
    bool remove(const Key &key) {
        auto idx = find_index(key);
        if (idx == npos)
            return false;
        mark_deleted(idx);
        --_size;
        ++_tombstones;
        return true;
    }

    bool erase(const Key &key) { return remove(key); }

    void clear() {
        _buckets.assign(_buckets.size(), Entry{});
        _occupied.reset();
        _deleted.reset();
        _size = _tombstones = 0;
    }

    // find pointer to value or nullptr
    T *find_value(const Key &key) {
        auto idx = find_index(key);
        return idx == npos ? nullptr : &_buckets[idx].value;
    }

    const T *find_value(const Key &key) const {
        auto idx = find_index(key);
        return idx == npos ? nullptr : &_buckets[idx].value;
    }

    iterator find(const Key &key) {
        auto idx = find_index(key);
        return idx == npos ? end() : iterator(this, idx);
    }

    const_iterator find(const Key &key) const {
        auto idx = find_index(key);
        return idx == npos ? end() : const_iterator(this, idx);
    }

    // access or default-construct
    T &operator[](const Key &key) {
        size_t idx;
        bool is_new;

        fl::pair<size_t, bool> p = find_slot(key);
        idx = p.first;
        is_new = p.second;
        if (is_new) {
            _buckets[idx].key = key;
            _buckets[idx].value = T{};
            mark_occupied(idx);
            ++_size;
        }
        return _buckets[idx].value;
    }

    size_t size() const { return _size; }
    bool empty() const { return _size == 0; }
    size_t capacity() const { return _buckets.size(); }

  private:
    static constexpr size_t npos = size_t(-1);

    // Helper methods to check entry state
    bool is_occupied(size_t idx) const { return _occupied.test(idx); }

    bool is_deleted(size_t idx) const { return _deleted.test(idx); }

    bool is_empty(size_t idx) const {
        return !is_occupied(idx) && !is_deleted(idx);
    }

    void mark_occupied(size_t idx) {
        _occupied.set(idx);
        _deleted.reset(idx);
    }

    void mark_deleted(size_t idx) {
        _occupied.reset(idx);
        _deleted.set(idx);
    }

    void mark_empty(size_t idx) {
        _occupied.reset(idx);
        _deleted.reset(idx);
    }

    struct Entry {
        Key key;
        T value;
        void swap(Entry &other) {
            fl::swap(key, other.key);
            fl::swap(value, other.value);
        }
    };

    static size_t next_power_of_two(size_t n) {
        size_t p = 1;
        while (p < n)
            p <<= 1;
        return p;
    }

    pair<size_t, bool> find_slot(const Key &key) const {
        const size_t cap = _buckets.size();
        const size_t mask = cap - 1;
        const size_t h = _hash(key) & mask;
        size_t first_tomb = npos;

        if (cap <= 8) {
            // linear probing
            for (size_t i = 0; i < cap; ++i) {
                const size_t idx = (h + i) & mask;

                if (is_empty(idx))
                    return {first_tomb != npos ? first_tomb : idx, true};
                if (is_deleted(idx)) {
                    if (first_tomb == npos)
                        first_tomb = idx;
                } else if (is_occupied(idx) && _equal(_buckets[idx].key, key)) {
                    return {idx, false};
                }
            }
        } else {
            // quadratic probing up to 8 tries
            size_t i = 0;
            for (; i < 8; ++i) {
                const size_t idx = (h + i + i * i) & mask;

                if (is_empty(idx))
                    return {first_tomb != npos ? first_tomb : idx, true};
                if (is_deleted(idx)) {
                    if (first_tomb == npos)
                        first_tomb = idx;
                } else if (is_occupied(idx) && _equal(_buckets[idx].key, key)) {
                    return {idx, false};
                }
            }
            // fallback to linear for the rest
            for (; i < cap; ++i) {
                const size_t idx = (h + i) & mask;

                if (is_empty(idx))
                    return {first_tomb != npos ? first_tomb : idx, true};
                if (is_deleted(idx)) {
                    if (first_tomb == npos)
                        first_tomb = idx;
                } else if (is_occupied(idx) && _equal(_buckets[idx].key, key)) {
                    return {idx, false};
                }
            }
        }

        return {npos, false};
    }

    enum {
        kLinearProbingOnlySize = 8,
        kQuadraticProbingTries = 8,
    };

    size_t find_index(const Key &key) const {
        const size_t cap = _buckets.size();
        const size_t mask = cap - 1;
        const size_t h = _hash(key) & mask;

        if (cap <= kLinearProbingOnlySize) {
            // linear probing
            for (size_t i = 0; i < cap; ++i) {
                const size_t idx = (h + i) & mask;
                if (is_empty(idx))
                    return npos;
                if (is_occupied(idx) && _equal(_buckets[idx].key, key))
                    return idx;
            }
        } else {
            // quadratic probing up to 8 tries
            size_t i = 0;
            for (; i < kQuadraticProbingTries; ++i) {
                const size_t idx = (h + i + i * i) & mask;
                if (is_empty(idx))
                    return npos;
                if (is_occupied(idx) && _equal(_buckets[idx].key, key))
                    return idx;
            }
            // fallback to linear for the rest
            for (; i < cap; ++i) {
                const size_t idx = (h + i) & mask;
                if (is_empty(idx))
                    return npos;
                if (is_occupied(idx) && _equal(_buckets[idx].key, key))
                    return idx;
            }
        }

        return npos;
    }

    size_t find_unoccupied_index_using_bitset(
        const Key &key, const fl::bitset<1024> &occupied_set) const {
        const size_t cap = _buckets.size();
        const size_t mask = cap - 1;
        const size_t h = _hash(key) & mask;

        if (cap <= kLinearProbingOnlySize) {
            // linear probing
            for (size_t i = 0; i < cap; ++i) {
                const size_t idx = (h + i) & mask;
                bool occupied = occupied_set.test(idx);
                if (occupied) {
                    continue;
                }
                return idx;
            }
        } else {
            // quadratic probing up to 8 tries
            size_t i = 0;
            for (; i < kQuadraticProbingTries; ++i) {
                const size_t idx = (h + i + i * i) & mask;
                bool occupied = occupied_set.test(idx);
                if (occupied) {
                    continue;
                }
                return idx;
            }
            // fallback to linear for the rest
            for (; i < cap; ++i) {
                const size_t idx = (h + i) & mask;
                bool occupied = occupied_set.test(idx);
                if (occupied) {
                    continue;
                }
                return idx;
            }
        }
        return npos;
    }

    void rehash(size_t new_cap) {
        new_cap = next_power_of_two(new_cap);
        fl::vector_inlined<Entry, INLINED_COUNT> old;
        fl::bitset<1024> old_occupied = _occupied;

        _buckets.swap(old);
        _buckets.clear();
        _buckets.assign(new_cap, Entry{});

        _occupied.reset();
        _occupied.resize(new_cap);
        _deleted.reset();
        _deleted.resize(new_cap);

        _size = _tombstones = 0;

        for (size_t i = 0; i < old.size(); i++) {
            if (old_occupied.test(i))
                insert(old[i].key, old[i].value);
        }
    }

    void rehash_inline_no_resize() {
        // filter out tombstones and compact
        size_t cap = _buckets.size();
        size_t pos = 0;
        // compact live elements to the left.
        for (size_t i = 0; i < cap; ++i) {
            if (is_occupied(i)) {
                if (pos != i) {
                    _buckets[pos] = _buckets[i];
                    mark_empty(i);
                    mark_occupied(pos);
                }
                ++pos;
            } else if (is_deleted(i)) {
                mark_empty(i);
            }
        }

        fl::bitset<1024> occupied; // Preallocate a bitset of size 1024
        // swap the components, this will happen at most N times,
        // use the occupied bitset to track which entries are occupied
        // in the array rather than just copied in.
        fl::optional<Entry> tmp;
        for (size_t i = 0; i < _size; ++i) {
            const bool already_finished = occupied.test(i);
            if (already_finished) {
                continue;
            }
            auto &e = _buckets[i];
            // FASTLED_ASSERT(e.state == EntryState::Occupied,
            //                "HashMap::rehash_inline_no_resize: invalid
            //                state");

            size_t idx = find_unoccupied_index_using_bitset(e.key, occupied);
            if (idx == npos) {
                // no more space
                FASTLED_ASSERT(
                    false, "HashMap::rehash_inline_no_resize: invalid index at "
                               << idx << " which is " << npos);
                return;
            }
            // if idx < pos then we are moving the entry to a new location
            FASTLED_ASSERT(!tmp,
                           "HashMap::rehash_inline_no_resize: invalid tmp");
            if (idx >= _size) {
                // directly copy it now
                _buckets[idx] = e;
                continue;
            }
            tmp = e;
            occupied.set(idx);
            _buckets[idx] = *tmp.ptr();
            while (!tmp.empty()) {
                // we have to find a place for temp.
                // find new position for tmp.
                auto key = tmp.ptr()->key;
                size_t new_idx =
                    find_unoccupied_index_using_bitset(key, occupied);
                if (new_idx == npos) {
                    // no more space
                    FASTLED_ASSERT(
                        false,
                        "HashMap::rehash_inline_no_resize: invalid index at "
                            << new_idx << " which is " << npos);
                    return;
                }
                occupied.set(new_idx);
                if (new_idx < _size) {
                    // we have to swap the entry at new_idx with tmp
                    optional<Entry> tmp2 = _buckets[new_idx];
                    _buckets[new_idx] = *tmp.ptr();
                    tmp = tmp2;
                } else {
                    // we can just move tmp to new_idx
                    _buckets[new_idx] = *tmp.ptr();
                    tmp.reset();
                }
            }
            FASTLED_ASSERT(
                occupied.test(i),
                "HashMap::rehash_inline_no_resize: invalid occupied at " << i);
            FASTLED_ASSERT(
                !tmp, "HashMap::rehash_inline_no_resize: invalid tmp at " << i);
        }
    }

    fl::vector_inlined<Entry, INLINED_COUNT> _buckets;
    size_t _size;
    size_t _tombstones;
    uint8_t mLoadFactor;
    fl::bitset<1024> _occupied;
    fl::bitset<1024> _deleted;
    Hash _hash;
    KeyEqual _equal;
};

// begin using declarations for stl compatibility
template <typename T> using equal_to = EqualTo<T>;

template <typename Key, typename T, typename Hash = Hash<Key>,
          typename KeyEqual = equal_to<Key>>
using hash_map = HashMap<Key, T, Hash, KeyEqual>;

template <typename Key, typename T, typename Hash = Hash<Key>,
          typename KeyEqual = equal_to<Key>>
using unordered_map = HashMap<Key, T, Hash, KeyEqual>;
// end using declarations for stl compatibility

} // namespace fl
