#pragma once

#include "fl/assert.h"
#include "fl/hash.h"
#include "fl/pair.h"
#include "fl/template_magic.h"
#include "fl/vector.h"
#include "fl/warn.h"

namespace fl {

template <typename T> struct EqualTo {
    bool operator()(const T &a, const T &b) const { return a == b; }
};

template <typename Key, typename T, typename Hash = Hash<Key>,
          typename KeyEqual = EqualTo<Key>>
class HashMap {
  public:
    HashMap(size_t initial_capacity = 8, float max_load = 0.5f)
        : _buckets(next_power_of_two(initial_capacity)), _size(0),
          _tombstones(0), _max_load(max_load) {
        for (auto &e : _buckets)
            e.state = EntryState::Empty;
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

    // insert or overwrite
    void insert(const Key &key, const T &value) {
        if (float(_size + _tombstones) / _buckets.size() > _max_load) {
            rehash(_buckets.size() * 2);
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

        // std::tie(idx, is_new) = find_slot(key);
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
        for (size_t i = 0; i < cap; ++i) {
            const size_t idx = (h + i) & mask;
            auto &e = _buckets[idx];

            if (e.state == EntryState::Empty) {
                // if we saw a tombstone earlier, reuse it
                return {first_tomb != npos ? first_tomb : idx, true};
            }
            if (e.state == EntryState::Deleted) {
                if (first_tomb == npos)
                    first_tomb = idx;
            } else if (_equal(e.key, key)) {
                return {idx, false};
            }
        }

        return {npos, false};
    }

    size_t find_index(const Key &key) const {
        const size_t cap = _buckets.size();
        const size_t mask = cap - 1;
        const size_t h = _hash(key) & mask;

        for (size_t i = 0; i < cap; ++i) {
            const size_t idx = (h + i) & mask;
            auto &e = _buckets[idx];

            if (e.state == EntryState::Empty) {
                // once we hit an empty slot, key’s not in table
                return npos;
            }
            if (e.state == EntryState::Occupied && _equal(e.key, key)) {
                return idx;
            }
            // otherwise keep probing
        }
        return npos;
    }

    void rehash(size_t new_cap) {
        new_cap = next_power_of_two(new_cap);
        fl::HeapVector<Entry> old;
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

    fl::HeapVector<Entry> _buckets;
    size_t _size;
    size_t _tombstones;
    float _max_load;
    Hash _hash;
    KeyEqual _equal;
};

} // namespace fl
