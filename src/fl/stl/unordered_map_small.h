#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/assert.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/pair.h"
#include "fl/stl/vector.h"
#include "fl/stl/allocator.h"
#include "fl/stl/bitset_dynamic.h"
#include "fl/stl/memory_resource.h"
#include "fl/stl/move.h"
#include "fl/stl/noexcept.h"
#include "platforms/assert_defs.h"

namespace fl {

// Lightweight equal_to functor — avoids pulling in hash infrastructure.
template <typename T> struct SmallMapEqualTo {
    bool operator()(const T& a, const T& b) const FL_NOEXCEPT { return a == b; }
};

// Small unordered map using linear scan with equality comparison.
//
// No hashing — flat vector of key-value pairs with O(n) lookup.
// Uses an occupied bitmap so erase is O(1) (just clears a bit) and
// iterators remain stable across erase. Freed slots are reused by insert.
//
// Optimal for small collections (< ~32 elements) where hash overhead
// isn't worth it. Minimal code size, excellent cache locality.
template <typename Key, typename Value,
          typename Equal = fl::SmallMapEqualTo<Key>>
class unordered_map_small {
  public:
    enum insert_result { inserted = 0, exists = 1, at_capacity = 2 };

    using key_type = Key;
    using mapped_type = Value;
    using value_type = fl::pair<Key, Value>;
    using size_type = fl::size;
    using key_equal = Equal;
    using vector_type = fl::vector<value_type>;

    // Forward iterator that skips unoccupied slots.
    struct iterator {
        using reference = value_type&;
        using pointer = value_type*;
        using iterator_category = fl::forward_iterator_tag;

        iterator() FL_NOEXCEPT : mMap(nullptr), mIdx(0) {}
        iterator(unordered_map_small* map, size_type idx) FL_NOEXCEPT
            : mMap(map), mIdx(idx) { advance_to_occupied(); }

        reference operator*() const FL_NOEXCEPT { return mMap->mData[mIdx]; }
        pointer operator->() const FL_NOEXCEPT { return &mMap->mData[mIdx]; }

        iterator& operator++() FL_NOEXCEPT { ++mIdx; advance_to_occupied(); return *this; }
        iterator operator++(int) FL_NOEXCEPT { iterator t = *this; ++(*this); return t; }

        bool operator==(const iterator& o) const FL_NOEXCEPT { return mIdx == o.mIdx; }
        bool operator!=(const iterator& o) const FL_NOEXCEPT { return mIdx != o.mIdx; }

      private:
        void advance_to_occupied() FL_NOEXCEPT {
            if (!mMap) return;
            size_type cap = mMap->mData.size();
            while (mIdx < cap && !mMap->mOccupied.test(mIdx)) ++mIdx;
        }
        unordered_map_small* mMap;
        size_type mIdx;
        friend class unordered_map_small;
    };

    struct const_iterator {
        using reference = const value_type&;
        using pointer = const value_type*;
        using iterator_category = fl::forward_iterator_tag;

        const_iterator() FL_NOEXCEPT : mMap(nullptr), mIdx(0) {}
        const_iterator(const unordered_map_small* map, size_type idx) FL_NOEXCEPT
            : mMap(map), mIdx(idx) { advance_to_occupied(); }
        const_iterator(const iterator& it) FL_NOEXCEPT
            : mMap(it.mMap), mIdx(it.mIdx) {}

        reference operator*() const FL_NOEXCEPT { return mMap->mData[mIdx]; }
        pointer operator->() const FL_NOEXCEPT { return &mMap->mData[mIdx]; }

        const_iterator& operator++() FL_NOEXCEPT { ++mIdx; advance_to_occupied(); return *this; }
        const_iterator operator++(int) FL_NOEXCEPT { const_iterator t = *this; ++(*this); return t; }

        bool operator==(const const_iterator& o) const FL_NOEXCEPT { return mIdx == o.mIdx; }
        bool operator!=(const const_iterator& o) const FL_NOEXCEPT { return mIdx != o.mIdx; }

      private:
        void advance_to_occupied() FL_NOEXCEPT {
            if (!mMap) return;
            size_type cap = mMap->mData.size();
            while (mIdx < cap && !mMap->mOccupied.test(mIdx)) ++mIdx;
        }
        const unordered_map_small* mMap;
        size_type mIdx;
        friend class unordered_map_small;
    };

  private:
    vector_type mData;
    bitset_dynamic mOccupied;
    size_type mSize = 0;
    Equal mEqual;
    memory_resource* mResource = nullptr;

    // Linear scan for key among occupied slots. Returns slot index or npos().
    size_type find_index(const Key& key) const FL_NOEXCEPT {
        for (size_type i = 0; i < mData.size(); ++i) {
            if (mOccupied.test(i) && mEqual(mData[i].first, key)) {
                return i;
            }
        }
        return npos();
    }

    // Find first free (unoccupied) slot, or npos() if none.
    size_type find_free_slot() const FL_NOEXCEPT {
        for (size_type i = 0; i < mData.size(); ++i) {
            if (!mOccupied.test(i)) return i;
        }
        return npos();
    }

    static size_type npos() FL_NOEXCEPT { return static_cast<size_type>(-1); }

    // Place a value at a slot index (must already be allocated in mData).
    void place_at(size_type idx, const value_type& kv) FL_NOEXCEPT {
        mData[idx] = kv;
        mOccupied.set(idx);
        ++mSize;
    }

    void place_at(size_type idx, value_type&& kv) FL_NOEXCEPT {
        mData[idx] = fl::move(kv);
        mOccupied.set(idx);
        ++mSize;
    }

    // Append a new slot at the end.
    size_type append(const value_type& kv) FL_NOEXCEPT {
        size_type idx = mData.size();
        mData.push_back(kv);
        mOccupied.resize(idx + 1);
        mOccupied.set(idx);
        ++mSize;
        return idx;
    }

    size_type append(value_type&& kv) FL_NOEXCEPT {
        size_type idx = mData.size();
        mData.push_back(fl::move(kv));
        mOccupied.resize(idx + 1);
        mOccupied.set(idx);
        ++mSize;
        return idx;
    }

    // Insert into a free slot or append. Returns slot index.
    size_type do_insert(const value_type& kv) FL_NOEXCEPT {
        size_type free = find_free_slot();
        if (free != npos()) {
            place_at(free, kv);
            return free;
        }
        return append(kv);
    }

    size_type do_insert(value_type&& kv) FL_NOEXCEPT {
        size_type free = find_free_slot();
        if (free != npos()) {
            place_at(free, fl::move(kv));
            return free;
        }
        return append(fl::move(kv));
    }

  public:
    // Constructors
    unordered_map_small() = default;

    explicit unordered_map_small(memory_resource* resource) FL_NOEXCEPT
        : mData(resource), mResource(resource) {}

    explicit unordered_map_small(const Equal& eq,
                                 memory_resource* resource = nullptr) FL_NOEXCEPT
        : mData(resource ? resource : default_memory_resource()),
          mEqual(eq),
          mResource(resource) {}

    unordered_map_small(const unordered_map_small& other) FL_NOEXCEPT
        : mData(other.mData), mOccupied(other.mOccupied),
          mSize(other.mSize), mEqual(other.mEqual),
          mResource(other.mResource) {}

    unordered_map_small& operator=(const unordered_map_small& other) FL_NOEXCEPT {
        if (this != &other) {
            mData = other.mData;
            mOccupied = other.mOccupied;
            mSize = other.mSize;
            mEqual = other.mEqual;
            mResource = other.mResource;
        }
        return *this;
    }

    unordered_map_small(unordered_map_small&& other) FL_NOEXCEPT
        : mData(fl::move(other.mData)), mOccupied(fl::move(other.mOccupied)),
          mSize(other.mSize), mEqual(fl::move(other.mEqual)),
          mResource(other.mResource) {
        other.mSize = 0;
    }

    unordered_map_small& operator=(unordered_map_small&& other) FL_NOEXCEPT {
        if (this != &other) {
            mData = fl::move(other.mData);
            mOccupied = fl::move(other.mOccupied);
            mSize = other.mSize;
            mEqual = fl::move(other.mEqual);
            mResource = other.mResource;
            other.mSize = 0;
        }
        return *this;
    }

    // Iterators
    iterator begin() FL_NOEXCEPT { return iterator(this, 0); }
    iterator end() FL_NOEXCEPT { return iterator(this, mData.size()); }
    const_iterator begin() const FL_NOEXCEPT { return const_iterator(this, 0); }
    const_iterator end() const FL_NOEXCEPT { return const_iterator(this, mData.size()); }
    const_iterator cbegin() const FL_NOEXCEPT { return const_iterator(this, 0); }
    const_iterator cend() const FL_NOEXCEPT { return const_iterator(this, mData.size()); }

    // Capacity
    size_type size() const FL_NOEXCEPT { return mSize; }
    bool empty() const FL_NOEXCEPT { return mSize == 0; }
    size_type capacity() const FL_NOEXCEPT { return mData.capacity(); }
    size_type max_size() const FL_NOEXCEPT { return mData.max_size(); }



    // Element access
    Value& operator[](const Key& key) FL_NOEXCEPT {
        size_type idx = find_index(key);
        if (idx != npos()) return mData[idx].second;
        // Insert default-constructed value
        value_type kv(key, Value());
        idx = do_insert(fl::move(kv));
        FASTLED_ASSERT(idx != npos(), "unordered_map_small::operator[]: insert failed");
        return mData[idx].second;
    }

    Value& at(const Key& key) FL_NOEXCEPT {
        size_type idx = find_index(key);
        FASTLED_ASSERT(idx != npos(), "Key not found in unordered_map_small");
        return mData[idx].second;
    }

    const Value& at(const Key& key) const FL_NOEXCEPT {
        size_type idx = find_index(key);
        FASTLED_ASSERT(idx != npos(), "Key not found in unordered_map_small");
        return mData[idx].second;
    }

    // Lookup
    iterator find(const Key& key) FL_NOEXCEPT {
        size_type idx = find_index(key);
        return idx != npos() ? iterator(this, idx) : end();
    }

    const_iterator find(const Key& key) const FL_NOEXCEPT {
        size_type idx = find_index(key);
        return idx != npos() ? const_iterator(this, idx) : end();
    }

    size_type count(const Key& key) const FL_NOEXCEPT {
        return find_index(key) != npos() ? 1 : 0;
    }

    bool contains(const Key& key) const FL_NOEXCEPT {
        return find_index(key) != npos();
    }

    bool has(const Key& key) const FL_NOEXCEPT { return contains(key); }

    // Insertion — does NOT overwrite if key exists
    fl::pair<iterator, bool> insert(const value_type& kv) FL_NOEXCEPT {
        size_type idx = find_index(kv.first);
        if (idx != npos()) {
            return fl::pair<iterator, bool>(iterator(this, idx), false);
        }
        idx = do_insert(kv);
        return fl::pair<iterator, bool>(iterator(this, idx), true);
    }

    fl::pair<iterator, bool> insert(value_type&& kv) FL_NOEXCEPT {
        size_type idx = find_index(kv.first);
        if (idx != npos()) {
            return fl::pair<iterator, bool>(iterator(this, idx), false);
        }
        idx = do_insert(fl::move(kv));
        return fl::pair<iterator, bool>(iterator(this, idx), true);
    }

    bool insert(const Key& key, const Value& value, insert_result* result = nullptr) FL_NOEXCEPT {
        size_type idx = find_index(key);
        if (idx != npos()) {
            if (result) *result = exists;
            return false;
        }
        do_insert(value_type(key, value));
        if (result) *result = inserted;
        return true;
    }

    bool insert(Key&& key, Value&& value, insert_result* result = nullptr) FL_NOEXCEPT {
        Key key_copy = key;
        size_type idx = find_index(key_copy);
        if (idx != npos()) {
            if (result) *result = exists;
            return false;
        }
        do_insert(value_type(fl::move(key), fl::move(value)));
        if (result) *result = inserted;
        return true;
    }

    // Emplace
    template <typename... Args>
    fl::pair<iterator, bool> emplace(Args&&... args) FL_NOEXCEPT {
        value_type kv(fl::forward<Args>(args)...);
        return insert(fl::move(kv));
    }

    // Erase — clears occupied bit, iterator stable
    iterator erase(iterator pos) FL_NOEXCEPT {
        if (pos.mMap != this || pos.mIdx >= mData.size()) return end();
        if (!mOccupied.test(pos.mIdx)) return end(); // already erased
        mOccupied.reset(pos.mIdx);
        --mSize;
        // Advance to next occupied slot
        iterator next(this, pos.mIdx + 1);
        return next;
    }

    iterator erase(const_iterator pos) FL_NOEXCEPT {
        iterator it(this, pos.mIdx);
        return erase(it);
    }

    size_type erase(const Key& key) FL_NOEXCEPT {
        size_type idx = find_index(key);
        if (idx != npos()) {
            mOccupied.reset(idx);
            --mSize;
            return 1;
        }
        return 0;
    }

    // Clear
    void clear() FL_NOEXCEPT {
        mData.clear();
        mOccupied = bitset_dynamic();
        mSize = 0;
    }

    // Swap
    void swap(unordered_map_small& other) FL_NOEXCEPT {
        mData.swap(other.mData);
        fl::swap(mOccupied, other.mOccupied);
        fl::swap(mSize, other.mSize);
        fl::swap(mEqual, other.mEqual);
        fl::swap(mResource, other.mResource);
    }

    key_equal key_eq() const FL_NOEXCEPT { return mEqual; }
    memory_resource* get_memory_resource() const FL_NOEXCEPT { return mResource; }

    void reserve(size_type n) FL_NOEXCEPT {
        mData.reserve(n);
        if (n > mOccupied.size()) mOccupied.resize(n);
    }

    // Remove unoccupied gaps, compacting the vector.
    void compact() FL_NOEXCEPT {
        size_type write = 0;
        for (size_type read = 0; read < mData.size(); ++read) {
            if (mOccupied.test(read)) {
                if (write != read) {
                    mData[write] = fl::move(mData[read]);
                }
                ++write;
            }
        }
        // Trim vector to alive count
        while (mData.size() > write) mData.pop_back();
        // Rebuild bitmap — all slots occupied
        mOccupied = bitset_dynamic(write);
        for (size_type i = 0; i < write; ++i) {
            mOccupied.set(i);
        }
    }

    // ---- FastLED-specific methods (matching flat_map API) ----

    Value get(const Key& key, const Value& defaultValue) const FL_NOEXCEPT {
        size_type idx = find_index(key);
        return idx != npos() ? mData[idx].second : defaultValue;
    }

    bool get(const Key& key, Value* out_value) const FL_NOEXCEPT {
        size_type idx = find_index(key);
        if (idx != npos()) {
            *out_value = mData[idx].second;
            return true;
        }
        return false;
    }

    fl::pair<iterator, bool> insert_or_update(const Key& key, const Value& value) FL_NOEXCEPT {
        size_type idx = find_index(key);
        if (idx != npos()) {
            mData[idx].second = value;
            return fl::pair<iterator, bool>(iterator(this, idx), false);
        }
        idx = do_insert(value_type(key, value));
        return fl::pair<iterator, bool>(iterator(this, idx), true);
    }

    bool update(const Key& key, const Value& value) FL_NOEXCEPT {
        size_type idx = find_index(key);
        if (idx != npos()) {
            mData[idx].second = value;
            return true;
        }
        do_insert(value_type(key, value));
        return true;
    }

    bool update(const Key& key, Value&& value) FL_NOEXCEPT {
        size_type idx = find_index(key);
        if (idx != npos()) {
            mData[idx].second = fl::move(value);
            return true;
        }
        do_insert(value_type(key, fl::move(value)));
        return true;
    }

    // Comparison (order-independent)
    bool operator==(const unordered_map_small& other) const FL_NOEXCEPT {
        if (mSize != other.mSize) return false;
        for (size_type i = 0; i < mData.size(); ++i) {
            if (!mOccupied.test(i)) continue;
            size_type oidx = other.find_index(mData[i].first);
            if (oidx == npos() || !(other.mData[oidx].second == mData[i].second)) {
                return false;
            }
        }
        return true;
    }

    bool operator!=(const unordered_map_small& other) const FL_NOEXCEPT {
        return !(*this == other);
    }
};

template <typename Key, typename Value, typename Equal>
void swap(unordered_map_small<Key, Value, Equal>& lhs,
          unordered_map_small<Key, Value, Equal>& rhs) FL_NOEXCEPT {
    lhs.swap(rhs);
}

}  // namespace fl
