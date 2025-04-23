#pragma once

#include "fl/hash.h"
#include "fl/template_magic.h"
#include "fl/vector.h"
#include "fl/pair.h"
#include "fl/warn.h"

namespace fl {


template<typename T>
struct EqualTo {
    bool operator()(const T &a, const T &b) const {
        return a == b;
    }
};

template<
    typename Key,
    typename T,
    typename Hash = Hash<Key>,
    typename KeyEqual = EqualTo<Key>>
class HashMap {
public:
    HashMap(size_t initial_capacity = 8, float max_load = 0.5f)
      : _buckets(next_power_of_two(initial_capacity))
      , _size(0)
      , _tombstones(0)
      , _max_load(max_load)
    {
        for (auto &e : _buckets) e.state = EntryState::Empty;
    }

    // insert or overwrite
    void insert(const Key &key, const T &value) {
        if (float(_size + _tombstones) / _buckets.size() > _max_load)
            rehash(_buckets.size() * 2);

        size_t idx;
        bool is_new;
        // fl::tie(idx, is_new) = find_slot(key);
        fl::pair<size_t,bool> p = find_slot(key);
        idx = p.first;
        is_new = p.second;
        if (is_new) {
            _buckets[idx].key   = key;
            _buckets[idx].value = value;
            _buckets[idx].state = EntryState::Occupied;
            ++_size;
        } else {
            _buckets[idx].value = value;
        }
    }

    // remove key; returns true if removed
    bool remove(const Key &key) {
        auto idx = find_index(key);
        if (idx == npos) return false;
        _buckets[idx].state = EntryState::Deleted;
        --_size; ++_tombstones;
        return true;
    }

    bool erase(const Key &key) {
        return remove(key);
    }

    void clear() {
        _buckets.assign(_buckets.size(), Entry{});
        for (auto &e : _buckets) e.state = EntryState::Empty;
        _size = _tombstones = 0;
    }

    // find pointer to value or nullptr
    T* find(const Key &key) {
        auto idx = find_index(key);
        return idx == npos ? nullptr : &_buckets[idx].value;
    }

    // access or default-construct
    T& operator[](const Key &key) {
        size_t idx;
        bool is_new;

        // std::tie(idx, is_new) = find_slot(key);
        fl::pair<size_t,bool> p = find_slot(key);
        idx = p.first;
        is_new = p.second;
        if (is_new) {
            _buckets[idx].key   = key;
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
        Key   key;
        T     value;
        EntryState state;
    };

    static size_t next_power_of_two(size_t n) {
        size_t p = 1;
        while (p < n) p <<= 1;
        return p;
    }

    // quadratic probe: idx = (h + i + i*i) & (cap-1)
    pair<size_t,bool> find_slot(const Key &key) const {
        size_t cap = _buckets.size();
        size_t h   = _hash(key) & (cap - 1);
        size_t first_tomb = npos;
        for (size_t i = 0; i < cap; ++i) {
            size_t idx = (h + i + i*i) & (cap - 1);
            auto &e = _buckets[idx];
            if (e.state == EntryState::Empty) {
                return { first_tomb != npos ? first_tomb : idx, true };
            }
            if (e.state == EntryState::Deleted) {
                if (first_tomb == npos) first_tomb = idx;
            }
            else if (_equal(e.key, key)) {
                return { idx, false };
            }
        }
        // throw std::overflow_error("HashMap is full");
        FASTLED_WARN("HashMap is full");  // Do something better here.
        return { npos, false };
    }

    size_t find_index(const Key &key) const {
        size_t cap = _buckets.size();
        size_t h   = _hash(key) & (cap - 1);
        for (size_t i = 0; i < cap; ++i) {
            size_t idx = (h + i + i*i) & (cap - 1);
            auto &e = _buckets[idx];
            if (e.state == EntryState::Empty) return npos;
            if (e.state == EntryState::Occupied && _equal(e.key, key))
                return idx;
        }
        return npos;
    }

    void rehash(size_t new_cap) {
        new_cap = next_power_of_two(new_cap);
        fl::HeapVector<Entry> old;
        old.swap(_buckets);
        _buckets.assign(new_cap, Entry{});
        for (auto &e : _buckets) e.state = EntryState::Empty;
        _size = _tombstones = 0;

        for (auto &e : old) {
            if (e.state == EntryState::Occupied)
                insert(e.key, e.value);
        }
    }


    fl::HeapVector<Entry> _buckets;
    size_t _size;
    size_t _tombstones;
    float  _max_load;
    Hash   _hash;
    KeyEqual _equal;
};

}  // namespace fl
