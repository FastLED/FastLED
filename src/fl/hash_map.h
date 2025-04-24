#pragma once

#include "fl/assert.h"
#include "fl/clamp.h"
#include "fl/hash.h"
#include "fl/map_range.h"
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


// -- HashMap class -------------------------------------------------------------
// Begin HashMap class

template <typename Key, typename T, typename Hash = Hash<Key>,
          typename KeyEqual = EqualTo<Key>, int INLINED_COUNT = 8>
class HashMap {
  public:
    HashMap(size_t initial_capacity = 8, float max_load = 0.5f)
        : _buckets(next_power_of_two(initial_capacity)), _size(0),
          _tombstones(0) {
        for (auto &e : _buckets)
            e.state = EntryState::Empty;
        setLoadFactor(max_load);
    }

    void setLoadFactor(float f) {
        f = fl::clamp(f, 0.f, 1.f);
        mLoadFactor = fl::map_range<float, uint8_t>(f, 0.f, 1.f, 0, 255);
    }

    // Iterator support.
    struct iterator {
        using value_type = pair<Key, T>;

        iterator(HashMap *m, size_t idx) : _map(m), _idx(idx) {
            advance_to_occupied();
        }

        value_type operator*() const {
            auto &e = _map->_buckets[_idx];
            return {e.key, e.value};
        }

        iterator &operator++() {
            ++_idx;
            advance_to_occupied();
            return *this;
        }

        bool operator==(const iterator &o) const {
            return _map == o._map && _idx == o._idx;
        }
        bool operator!=(const iterator &o) const { return !(*this == o); }

      private:
        HashMap *_map;
        size_t _idx;

        void advance_to_occupied() {
            size_t cap = _map->_buckets.size();
            while (_idx < cap &&
                   _map->_buckets[_idx].state != EntryState::Occupied)
                ++_idx;
        }
    };

    struct const_iterator {
        using value_type = pair<Key, T>;

        const_iterator(const HashMap *m, size_t idx) : _map(m), _idx(idx) {
            advance_to_occupied();
        }

        value_type operator*() const {
            auto &e = _map->_buckets[_idx];
            return {e.key, e.value};
        }

        const_iterator &operator++() {
            ++_idx;
            advance_to_occupied();
            return *this;
        }

        bool operator==(const const_iterator &o) const {
            return _map == o._map && _idx == o._idx;
        }
        bool operator!=(const const_iterator &o) const { return !(*this == o); }

      private:
        const HashMap *_map;
        size_t _idx;

        void advance_to_occupied() {
            size_t cap = _map->_buckets.size();
            while (_idx < cap &&
                   _map->_buckets[_idx].state != EntryState::Occupied)
                ++_idx;
        }
    };

    iterator begin() { return iterator(this, 0); }
    iterator end() { return iterator(this, _buckets.size()); }
    const_iterator begin() const { return const_iterator(this, 0); }
    const_iterator end() const { return const_iterator(this, _buckets.size()); }

    // returns true if (size + tombs)/capacity > _max_load/256
    bool needs_rehash() const {
        // (size + tombstones) << 8   : multiply numerator by 256
        // _buckets.size() * _max_load : denominator * threshold
        uint32_t lhs = (_size + _tombstones) << 8;
        uint32_t rhs = (_buckets.size() * mLoadFactor);
        return lhs > rhs;
    }

    // insert or overwrite
    void insert(const Key &key, const T &value) {
        const bool _needs_rehash = needs_rehash();
        if (_needs_rehash) {
            const size_t new_size = _buckets.size() * 2;
            rehash(new_size);
        }

        size_t idx;
        bool is_new;
        fl::pair<size_t, bool> p = find_slot(key);
        idx = p.first;
        is_new = p.second;
        if (is_new) {
            _buckets[idx].key = key;
            _buckets[idx].value = value;
            _buckets[idx].state = EntryState::Occupied;
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
        _buckets[idx].state = EntryState::Deleted;
        --_size;
        ++_tombstones;
        return true;
    }

    bool erase(const Key &key) { return remove(key); }

    void clear() {
        _buckets.assign(_buckets.size(), Entry{});
        for (auto &e : _buckets)
            e.state = EntryState::Empty;
        _size = _tombstones = 0;
    }

    // find pointer to value or nullptr
    T *find(const Key &key) {
        auto idx = find_index(key);
        return idx == npos ? nullptr : &_buckets[idx].value;
    }

    const T *find(const Key &key) const {
        auto idx = find_index(key);
        return idx == npos ? nullptr : &_buckets[idx].value;
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
            _buckets[idx].state = EntryState::Occupied;
            ++_size;
        }
        return _buckets[idx].value;
    }

    size_t size() const { return _size; }
    bool empty() const { return _size == 0; }

  private:
    static constexpr size_t npos = size_t(-1);

    enum class EntryState : uint8_t { Empty, Occupied, Deleted };

    struct Entry {
        Key key;
        T value;
        EntryState state;
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
                auto &e = _buckets[idx];

                if (e.state == EntryState::Empty)
                    return {first_tomb != npos ? first_tomb : idx, true};
                if (e.state == EntryState::Deleted) {
                    if (first_tomb == npos)
                        first_tomb = idx;
                } else if (_equal(e.key, key)) {
                    return {idx, false};
                }
            }
        } else {
            // quadratic probing up to 8 tries
            size_t i = 0;
            for (; i < 8; ++i) {
                const size_t idx = (h + i + i * i) & mask;
                auto &e = _buckets[idx];

                if (e.state == EntryState::Empty)
                    return {first_tomb != npos ? first_tomb : idx, true};
                if (e.state == EntryState::Deleted) {
                    if (first_tomb == npos)
                        first_tomb = idx;
                } else if (_equal(e.key, key)) {
                    return {idx, false};
                }
            }
            // fallback to linear for the rest
            for (; i < cap; ++i) {
                const size_t idx = (h + i) & mask;
                auto &e = _buckets[idx];

                if (e.state == EntryState::Empty)
                    return {first_tomb != npos ? first_tomb : idx, true};
                if (e.state == EntryState::Deleted) {
                    if (first_tomb == npos)
                        first_tomb = idx;
                } else if (_equal(e.key, key)) {
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
                auto &e = _buckets[idx];
                if (e.state == EntryState::Empty)
                    return npos;
                if (e.state == EntryState::Occupied && _equal(e.key, key))
                    return idx;
            }
        } else {
            // quadratic probing up to 8 tries
            size_t i = 0;
            for (; i < kQuadraticProbingTries; ++i) {
                const size_t idx = (h + i + i * i) & mask;
                auto &e = _buckets[idx];
                if (e.state == EntryState::Empty)
                    return npos;
                if (e.state == EntryState::Occupied && _equal(e.key, key))
                    return idx;
            }
            // fallback to linear for the rest
            for (; i < cap; ++i) {
                const size_t idx = (h + i) & mask;
                auto &e = _buckets[idx];
                if (e.state == EntryState::Empty)
                    return npos;
                if (e.state == EntryState::Occupied && _equal(e.key, key))
                    return idx;
            }
        }

        return npos;
    }

    void rehash(size_t new_cap) {
        new_cap = next_power_of_two(new_cap);
        fl::vector_inlined<Entry, INLINED_COUNT> old;
        _buckets.swap(old);
        _buckets.clear();
        _buckets.assign(new_cap, Entry{});
        for (auto &e : _buckets)
            e.state = EntryState::Empty;
        _size = _tombstones = 0;

        for (auto &e : old) {
            if (e.state == EntryState::Occupied)
                insert(e.key, e.value);
        }
    }

    fl::vector_inlined<Entry, INLINED_COUNT> _buckets;
    size_t _size;
    size_t _tombstones;
    uint8_t mLoadFactor;
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
