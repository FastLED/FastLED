#pragma once

/*
unordered_map that is optimized for embedded devices. The unordered_map
will store upto N elements inline, and will spill over to a heap.
This unordered_map will try not to grow by detecting during rehash that
the number of tombstones is greater than the number of elements.
This will keep the memory from growing during multiple inserts
and removals.
*/

#include "fl/stl/assert.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/bitset.h"
#include "fl/clamp.h"
#include "fl/hash.h"
#include "fl/stl/initializer_list.h"
#include "fl/map_range.h"
#include "fl/stl/optional.h"
#include "fl/stl/pair.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/vector.h"
#include "fl/warn.h"
#include "fl/align.h"
#include "fl/compiler_control.h"
#include "fl/math_macros.h"

#include "fl/compiler_control.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_SHORTEN_64_TO_32

namespace fl {

// // begin using declarations for stl compatibility
// use fl::equal_to;
// use fl::unordered_map;
// // end using declarations for stl compatibility

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_NULL_DEREFERENCE
template <typename T> struct EqualTo {
    bool operator()(const T &a, const T &b) const { return a == b; }
};
FL_DISABLE_WARNING_POP

// -- unordered_map class
// ------------------------------------------------------------- Begin unordered_map
// class

#ifndef FASTLED_HASHMAP_INLINED_COUNT
#define FASTLED_HASHMAP_INLINED_COUNT 8
#endif

template <typename Key, typename T, typename Hash = Hash<Key>,
          typename KeyEqual = EqualTo<Key>,
          int INLINED_COUNT = FASTLED_HASHMAP_INLINED_COUNT>
class FL_ALIGN unordered_map {
  public:
    unordered_map() : unordered_map(FASTLED_HASHMAP_INLINED_COUNT, 0.7f) {}
    unordered_map(fl::size initial_capacity) : unordered_map(initial_capacity, 0.7f) {}

    unordered_map(fl::size initial_capacity, float max_load)
        : _buckets(next_power_of_two(initial_capacity)), _size(0),
          _tombstones(0), _occupied(next_power_of_two(initial_capacity)),
          _deleted(next_power_of_two(initial_capacity)) {
        setLoadFactor(max_load);
    }

    // Copy constructor
    unordered_map(const unordered_map& other)
        : _buckets(other._buckets.size()), _size(0), _tombstones(0),
          mLoadFactor(other.mLoadFactor), _occupied(other._buckets.size()),
          _deleted(other._buckets.size()), _hash(other._hash), _equal(other._equal) {
        // Copy all occupied entries using insert to properly hash them
        for (fl::size i = 0; i < other._buckets.size(); ++i) {
            if (other.is_occupied(i)) {
                insert(other._buckets[i].key, other._buckets[i].value);
            }
        }
    }

    // Move constructor
    unordered_map(unordered_map&& other)
        : _buckets(fl::move(other._buckets)), _size(other._size),
          _tombstones(other._tombstones), mLoadFactor(other.mLoadFactor),
          _occupied(fl::move(other._occupied)), _deleted(fl::move(other._deleted)),
          _hash(fl::move(other._hash)), _equal(fl::move(other._equal)) {
        // Reset other to valid empty state
        other._size = 0;
        other._tombstones = 0;
    }

    // Range constructor
    template<typename InputIt>
    unordered_map(InputIt first, InputIt last)
        : unordered_map(FASTLED_HASHMAP_INLINED_COUNT, 0.7f) {
        insert(first, last);
    }

    // Initializer list constructor
    unordered_map(fl::initializer_list<pair<Key, T>> init)
        : unordered_map(FASTLED_HASHMAP_INLINED_COUNT, 0.7f) {
        insert(init);
    }

    // Constructor with hash/equal/allocator parameters
    unordered_map(fl::size n, const Hash& hf, const KeyEqual& eq)
        : _buckets(next_power_of_two(n)), _size(0), _tombstones(0),
          _occupied(next_power_of_two(n)), _deleted(next_power_of_two(n)),
          _hash(hf), _equal(eq) {
        setLoadFactor(0.7f);
    }

    // Copy assignment operator
    unordered_map& operator=(const unordered_map& other) {
        if (this != &other) {
            // Clear current content
            clear();

            // Resize if necessary
            if (_buckets.size() != other._buckets.size()) {
                _buckets.clear();
                _buckets.assign(other._buckets.size(), Entry{});
                _occupied.reset();
                _occupied.resize(other._buckets.size());
                _deleted.reset();
                _deleted.resize(other._buckets.size());
            }

            // Copy all occupied entries
            for (fl::size i = 0; i < other._buckets.size(); ++i) {
                if (other.is_occupied(i)) {
                    _buckets[i] = other._buckets[i];
                    mark_occupied(i);
                    ++_size;
                }
            }

            mLoadFactor = other.mLoadFactor;
            _hash = other._hash;
            _equal = other._equal;
        }
        return *this;
    }

    // Move assignment operator
    unordered_map& operator=(unordered_map&& other) {
        if (this != &other) {
            _buckets = fl::move(other._buckets);
            _size = other._size;
            _tombstones = other._tombstones;
            mLoadFactor = other.mLoadFactor;
            _occupied = fl::move(other._occupied);
            _deleted = fl::move(other._deleted);
            _hash = fl::move(other._hash);
            _equal = fl::move(other._equal);

            // Reset other to valid empty state
            other._size = 0;
            other._tombstones = 0;
        }
        return *this;
    }

    // Initializer list assignment operator
    unordered_map& operator=(fl::initializer_list<pair<Key, T>> init) {
        clear();
        insert(init);
        return *this;
    }

    void setLoadFactor(float f) {
        f = fl::clamp(f, 0.f, 1.f);
        mLoadFactor = fl::map_range<float, u8>(f, 0.f, 1.f, 0, 255);
    }

    // Iterator support.
    struct iterator {
        // Standard iterator typedefs
        // using difference_type = std::ptrdiff_t;
        using value_type = pair<const Key, T>;  // Keep const Key as per standard
        using pointer = value_type *;
        using reference = value_type &;
        // using iterator_category = std::forward_iterator_tag;

        iterator() : _map(nullptr), _idx(0) {}
        iterator(unordered_map *m, fl::size idx) : _map(m), _idx(idx) {
            advance_to_occupied();
        }

        value_type operator*() const {
            auto &e = _map->_buckets[_idx];
            return {e.key, e.value};
        }

        pointer operator->() {
            // Use fl::bit_cast since pair<const Key, T> and pair<Key, T> are different types
            // but have the same memory layout, then destroy/reconstruct to avoid assignment issues
            using mutable_value_type = pair<Key, T>;
            auto& mutable_cached = *fl::bit_cast<mutable_value_type*>(&_cached_value);
            mutable_cached.~mutable_value_type();
            new (&mutable_cached) mutable_value_type(operator*());
            return &_cached_value;
        }
        
        pointer operator->() const {
            // Use fl::bit_cast since pair<const Key, T> and pair<Key, T> are different types
            // but have the same memory layout, then destroy/reconstruct to avoid assignment issues
            using mutable_value_type = pair<Key, T>;
            auto& mutable_cached = *fl::bit_cast<mutable_value_type*>(&_cached_value);
            mutable_cached.~mutable_value_type();
            new (&mutable_cached) mutable_value_type(operator*());
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
            fl::size cap = _map->_buckets.size();
            while (_idx < cap && !_map->is_occupied(_idx))
                ++_idx;
        }

      private:
        unordered_map *_map;
        fl::size _idx;
        mutable value_type _cached_value;
        friend class unordered_map;
    };

    struct const_iterator {
        // Standard iterator typedefs
        // using difference_type = std::ptrdiff_t;
        using value_type = pair<const Key, T>;
        using pointer = const value_type *;
        using reference = const value_type &;
        // using iterator_category = std::forward_iterator_tag;

        const_iterator() : _map(nullptr), _idx(0) {}
        const_iterator(const unordered_map *m, fl::size idx) : _map(m), _idx(idx) {
            advance_to_occupied();
        }
        const_iterator(const iterator &it) : _map(it._map), _idx(it._idx) {}

        value_type operator*() const {
            auto &e = _map->_buckets[_idx];
            return {e.key, e.value};
        }

        pointer operator->() const {
            // Use fl::bit_cast since pair<const Key, T> and pair<Key, T> are different types
            // but have the same memory layout, then destroy/reconstruct to avoid assignment issues
            using mutable_value_type = pair<Key, T>;
            auto& mutable_cached = *fl::bit_cast<mutable_value_type*>(&_cached_value);
            mutable_cached.~mutable_value_type();
            new (&mutable_cached) mutable_value_type(operator*());
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
            fl::size cap = _map->_buckets.size();
            while (_idx < cap && !_map->is_occupied(_idx))
                ++_idx;
        }

        friend class unordered_map;

      private:
        const unordered_map *_map;
        fl::size _idx;
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

    static bool NeedsRehash(fl::size size, fl::size bucket_size, fl::size tombstones,
                            u8 load_factor) {
        // (size + tombstones) << 8   : multiply numerator by 256
        // capacity * max_load : denominator * threshold
        u32 lhs = (size + tombstones) << 8;
        u32 rhs = (bucket_size * load_factor);
        return lhs > rhs;
    }

    // returns true if (size + tombs)/capacity > _max_load/256
    bool needs_rehash() const {
        return NeedsRehash(_size, _buckets.size(), _tombstones, mLoadFactor);
    }

    // insert or overwrite - returns pair<iterator, bool>
    // iterator points to element, bool is true if inserted, false if updated
    pair<iterator, bool> insert(const Key &key, const T &value) {
        const bool will_rehash = needs_rehash();
        if (will_rehash) {
            // if half the buckets are tombstones, rehash inline to prevent
            // memory spill over into the heap.
            if (_tombstones > _size) {
                rehash_inline_no_resize();
            } else {
                rehash_internal(_buckets.size() * 2);
            }
        }
        fl::size idx;
        bool is_new;
        fl::pair<fl::size, bool> p = find_slot(key);
        idx = p.first;
        is_new = p.second;
        if (is_new) {
            _buckets[idx].key = key;
            _buckets[idx].value = value;
            mark_occupied(idx);
            ++_size;
        } else {
            FASTLED_ASSERT(idx != npos(), "unordered_map::insert: invalid index at "
                                            << idx << " which is " << npos());
            _buckets[idx].value = value;
        }
        return {iterator(this, idx), is_new};
    }

    // Move version of insert - returns pair<iterator, bool>
    // iterator points to element, bool is true if inserted, false if updated
    pair<iterator, bool> insert(Key &&key, T &&value) {
        const bool will_rehash = needs_rehash();
        if (will_rehash) {
            // if half the buckets are tombstones, rehash inline to prevent
            // memory spill over into the heap.
            if (_tombstones > _size) {
                rehash_inline_no_resize();
            } else {
                rehash_internal(_buckets.size() * 2);
            }
        }
        fl::size idx;
        bool is_new;
        fl::pair<fl::size, bool> p = find_slot(key);
        idx = p.first;
        is_new = p.second;
        if (is_new) {
            _buckets[idx].key = fl::move(key);
            _buckets[idx].value = fl::move(value);
            mark_occupied(idx);
            ++_size;
        } else {
            FASTLED_ASSERT(idx != npos(), "unordered_map::insert: invalid index at "
                                            << idx << " which is " << npos());
            _buckets[idx].value = fl::move(value);
        }
        return {iterator(this, idx), is_new};
    }

    // Pair-based insert (const) - std::unordered_map compatible
    // Insert a pair<const Key, T> or pair<Key, T>
    pair<iterator, bool> insert(const pair<Key, T> &kv) {
        return insert(kv.first, kv.second);
    }

    // Pair-based insert (move) - std::unordered_map compatible
    // Insert a pair<const Key, T> or pair<Key, T> with move semantics
    pair<iterator, bool> insert(pair<Key, T> &&kv) {
        return insert(fl::move(kv.first), fl::move(kv.second));
    }

    // Range insert - insert elements from iterator range [first, last)
    // std::unordered_map compatible
    template<typename InputIt>
    void insert(InputIt first, InputIt last) {
        for (; first != last; ++first) {
            insert(*first);  // Uses pair-based insert
        }
    }

    // Initializer list insert - std::unordered_map compatible
    // Insert elements from an initializer list like: m.insert({{1, "a"}, {2, "b"}})
    void insert(fl::initializer_list<pair<Key, T>> init) {
        for (const auto& kv : init) {
            insert(kv);  // Uses pair-based insert
        }
    }

    // insert_or_assign() - C++17: insert new element or update existing
    // Returns pair<iterator, bool> where bool is true if inserted, false if updated
    pair<iterator, bool> insert_or_assign(const Key &key, T &&value) {
        const bool will_rehash = needs_rehash();
        if (will_rehash) {
            if (_tombstones > _size) {
                rehash_inline_no_resize();
            } else {
                rehash_internal(_buckets.size() * 2);
            }
        }
        fl::size idx;
        bool is_new;
        fl::pair<fl::size, bool> p = find_slot(key);
        idx = p.first;
        is_new = p.second;
        if (is_new) {
            _buckets[idx].key = key;
            _buckets[idx].value = fl::move(value);
            mark_occupied(idx);
            ++_size;
        } else {
            FASTLED_ASSERT(idx != npos(), "unordered_map::insert_or_assign: invalid index");
            _buckets[idx].value = fl::move(value);
        }
        return {iterator(this, idx), is_new};
    }

    // insert_or_assign() with move key - C++17
    pair<iterator, bool> insert_or_assign(Key &&key, T &&value) {
        const bool will_rehash = needs_rehash();
        if (will_rehash) {
            if (_tombstones > _size) {
                rehash_inline_no_resize();
            } else {
                rehash_internal(_buckets.size() * 2);
            }
        }
        fl::size idx;
        bool is_new;
        fl::pair<fl::size, bool> p = find_slot(key);
        idx = p.first;
        is_new = p.second;
        if (is_new) {
            _buckets[idx].key = fl::move(key);
            _buckets[idx].value = fl::move(value);
            mark_occupied(idx);
            ++_size;
        } else {
            FASTLED_ASSERT(idx != npos(), "unordered_map::insert_or_assign: invalid index");
            _buckets[idx].value = fl::move(value);
        }
        return {iterator(this, idx), is_new};
    }

    // emplace() - construct element in-place from arguments
    // For pairs, accepts (key, value) arguments to construct pair<Key, T>
    template<typename... Args>
    pair<iterator, bool> emplace(Args&&... args) {
        // Construct a pair from the arguments
        pair<Key, T> kv(fl::forward<Args>(args)...);
        // Use move-based insert since we just constructed this pair
        return insert(fl::move(kv));
    }

    // emplace_hint() - construct element in-place with hint
    // Hint is ignored for hash maps (no spatial locality benefits)
    template<typename... Args>
    iterator emplace_hint(const_iterator hint, Args&&... args) {
        (void)hint;  // Hint is ignored in hash maps
        return emplace(fl::forward<Args>(args)...).first;
    }

    // try_emplace() - C++17: emplace only if key doesn't exist
    // Advantage: doesn't move key if element already exists
    template<typename... Args>
    pair<iterator, bool> try_emplace(const Key& k, Args&&... args) {
        const bool will_rehash = needs_rehash();
        if (will_rehash) {
            if (_tombstones > _size) {
                rehash_inline_no_resize();
            } else {
                rehash_internal(_buckets.size() * 2);
            }
        }
        fl::size idx;
        bool is_new;
        fl::pair<fl::size, bool> p = find_slot(k);
        idx = p.first;
        is_new = p.second;
        if (is_new) {
            // Key doesn't exist, construct in place
            _buckets[idx].key = k;
            _buckets[idx].value = T(fl::forward<Args>(args)...);
            mark_occupied(idx);
            ++_size;
        }
        // If not new, key already exists - don't construct value, just return existing iterator
        return {iterator(this, idx), is_new};
    }

    // try_emplace() with move key - C++17
    template<typename... Args>
    pair<iterator, bool> try_emplace(Key&& k, Args&&... args) {
        const bool will_rehash = needs_rehash();
        if (will_rehash) {
            if (_tombstones > _size) {
                rehash_inline_no_resize();
            } else {
                rehash_internal(_buckets.size() * 2);
            }
        }
        fl::size idx;
        bool is_new;
        fl::pair<fl::size, bool> p = find_slot(k);
        idx = p.first;
        is_new = p.second;
        if (is_new) {
            // Key doesn't exist, construct in place with moved key
            _buckets[idx].key = fl::move(k);
            _buckets[idx].value = T(fl::forward<Args>(args)...);
            mark_occupied(idx);
            ++_size;
        }
        // If not new, key already exists - don't move key or construct value
        return {iterator(this, idx), is_new};
    }

    // remove key; returns true if removed
    bool remove(const Key &key) {
        auto idx = find_index(key);
        if (idx == npos())
            return false;
        mark_deleted(idx);
        --_size;
        ++_tombstones;
        return true;
    }

    bool erase(const Key &key) { return remove(key); }

    // Iterator-based erase - more efficient when you already have the iterator position
    iterator erase(iterator it) {
        if (it == end() || it._map != this) {
            return end(); // Invalid iterator
        }

        // Mark the current position as deleted
        mark_deleted(it._idx);
        --_size;
        ++_tombstones;

        // Advance to next valid element and return iterator to it
        ++it._idx;
        it.advance_to_occupied();
        return it;
    }

    // Range erase - erase elements in range [first, last)
    iterator erase(const_iterator first, const_iterator last) {
        if (first._map != this || last._map != this) {
            return end(); // Invalid iterators
        }

        // Erase each element in the range
        // We need to convert const_iterator to iterator for the return value
        iterator current(this, first._idx);
        current.advance_to_occupied();

        while (current != end() && current._idx < last._idx) {
            fl::size idx_to_erase = current._idx;
            ++current;
            current.advance_to_occupied();

            // Erase the element at idx_to_erase
            mark_deleted(idx_to_erase);
            --_size;
            ++_tombstones;
        }

        return current;
    }

    void clear() {
        _buckets.assign(_buckets.size(), Entry{});
        _occupied.reset();
        _deleted.reset();
        _size = _tombstones = 0;
    }

    // swap() - swap contents with another unordered_map
    void swap(unordered_map& other) {
        // Swap all member variables
        _buckets.swap(other._buckets);
        fl::swap(_size, other._size);
        fl::swap(_tombstones, other._tombstones);
        fl::swap(mLoadFactor, other.mLoadFactor);
        fl::swap(_occupied, other._occupied);
        fl::swap(_deleted, other._deleted);
        fl::swap(_hash, other._hash);
        fl::swap(_equal, other._equal);
    }

    // find pointer to value or nullptr
    T *find_value(const Key &key) {
        auto idx = find_index(key);
        return idx == npos() ? nullptr : &_buckets[idx].value;
    }

    const T *find_value(const Key &key) const {
        auto idx = find_index(key);
        return idx == npos() ? nullptr : &_buckets[idx].value;
    }

    iterator find(const Key &key) {
        auto idx = find_index(key);
        return idx == npos() ? end() : iterator(this, idx);
    }

    const_iterator find(const Key &key) const {
        auto idx = find_index(key);
        return idx == npos() ? end() : const_iterator(this, idx);
    }

    bool contains(const Key &key) const {
        auto idx = find_index(key);
        return idx != npos();
    }

    // at() - bounds-checked access, asserts if key not found
    T &at(const Key &key) {
        T* value = find_value(key);
        FASTLED_ASSERT(value != nullptr, "unordered_map::at: key not found");
        return *value;
    }

    const T &at(const Key &key) const {
        const T* value = find_value(key);
        FASTLED_ASSERT(value != nullptr, "unordered_map::at: key not found");
        return *value;
    }

    // count() - returns 0 or 1 (unordered_map has unique keys)
    fl::size count(const Key &key) const {
        return contains(key) ? 1 : 0;
    }

    // equal_range() - returns pair of iterators [it, it+1) or [end, end)
    pair<iterator, iterator> equal_range(const Key &key) {
        iterator it = find(key);
        if (it == end()) {
            return {end(), end()};
        }
        iterator next = it;
        ++next;
        return {it, next};
    }

    pair<const_iterator, const_iterator> equal_range(const Key &key) const {
        const_iterator it = find(key);
        if (it == end()) {
            return {end(), end()};
        }
        const_iterator next = it;
        ++next;
        return {it, next};
    }

    // access or default-construct
    T &operator[](const Key &key) {
        fl::size idx;
        bool is_new;

        fl::pair<fl::size, bool> p = find_slot(key);
        idx = p.first;
        is_new = p.second;
        
        // Check if find_slot failed to find a valid slot (unordered_map is full)
        if (idx == npos()) {
            // Need to resize to make room
            if (needs_rehash()) {
                // if half the buckets are tombstones, rehash inline to prevent
                // memory growth. Otherwise, double the size.
                if (_tombstones >= _buckets.size() / 2) {
                    rehash_inline_no_resize();
                } else {
                    rehash_internal(_buckets.size() * 2);
                }
            } else {
                // Force a rehash with double size if needs_rehash() doesn't detect the issue
                rehash_internal(_buckets.size() * 2);
            }
            
            // Try find_slot again after resize
            p = find_slot(key);
            idx = p.first;
            is_new = p.second;
            
            // If still npos() after resize, allocation failed catastrophically
            if (idx == npos()) {
                // This should never happen after a successful resize
                static T default_value{};
                FASTLED_ASSERT(false, "unordered_map::operator[]: Failed to allocate after rehash");
                return default_value;
            }
        }
        
        if (is_new) {
            _buckets[idx].key = key;
            _buckets[idx].value = T{};
            mark_occupied(idx);
            ++_size;
        }
        return _buckets[idx].value;
    }

    fl::size size() const { return _size; }
    bool empty() const { return _size == 0; }
    fl::size capacity() const { return _buckets.size(); }

    // max_size() - theoretical maximum size
    fl::size max_size() const {
        // Return the maximum value that fl::size can hold, divided by the size of Entry
        // to be conservative about memory limits
        return static_cast<fl::size>(-1) / sizeof(Entry);
    }

    // hash_function() - return the hash functor
    Hash hash_function() const { return _hash; }

    // key_eq() - return the key equality predicate
    KeyEqual key_eq() const { return _equal; }

    // Hash Policy Interface

    // load_factor() - current load factor (size / bucket_count)
    float load_factor() const {
        if (_buckets.size() == 0) {
            return 0.0f;
        }
        return static_cast<float>(_size) / static_cast<float>(_buckets.size());
    }

    // max_load_factor() - get maximum load factor
    float max_load_factor() const {
        // mLoadFactor is stored as u8 (0-255) representing 0.0-1.0
        return static_cast<float>(mLoadFactor) / 255.0f;
    }

    // max_load_factor(float ml) - set maximum load factor
    void max_load_factor(float ml) {
        setLoadFactor(ml);
    }

    // bucket_count() - number of buckets
    fl::size bucket_count() const {
        return _buckets.size();
    }

    // rehash(size_type n) - change number of buckets to at least n
    void rehash(fl::size n) {
        // Ensure n is at least as large as the current number of elements
        fl::size min_buckets = _size;
        if (n < min_buckets) {
            n = min_buckets;
        }

        // Only rehash if we need more buckets than we currently have
        if (n > _buckets.size()) {
            rehash_internal(n);
        }
        // If n <= current bucket count, do nothing (std::unordered_map behavior)
    }

    // reserve(size_type n) - reserve capacity for at least n elements
    void reserve(fl::size n) {
        // Calculate required buckets to hold n elements without exceeding load factor
        float max_lf = max_load_factor();
        if (max_lf <= 0.0f) {
            max_lf = 0.7f; // Default if somehow zero
        }
        // Required buckets = ceil(n / max_load_factor)
        fl::size required_buckets = static_cast<fl::size>(
            static_cast<float>(n) / max_lf + 0.999999f // Add almost 1 to simulate ceil
        );
        if (required_buckets > _buckets.size()) {
            rehash(required_buckets);
        }
    }

  private:
    static fl::size npos() {
        return static_cast<fl::size>(-1);
    }

    // Helper methods to check entry state
    bool is_occupied(fl::size idx) const { return _occupied.test(idx); }

    bool is_deleted(fl::size idx) const { return _deleted.test(idx); }

    bool is_empty(fl::size idx) const {
        return !is_occupied(idx) && !is_deleted(idx);
    }

    void mark_occupied(fl::size idx) {
        _occupied.set(idx);
        _deleted.reset(idx);
    }

    void mark_deleted(fl::size idx) {
        _occupied.reset(idx);
        _deleted.set(idx);
    }

    void mark_empty(fl::size idx) {
        _occupied.reset(idx);
        _deleted.reset(idx);
    }

    struct alignas(fl::max_align<Key, T>::value) Entry {
        Key key;
        T value;
        void swap(Entry &other) {
            fl::swap(key, other.key);
            fl::swap(value, other.value);
        }
    };

    static fl::size next_power_of_two(fl::size n) {
        fl::size p = 1;
        while (p < n)
            p <<= 1;
        return p;
    }

    pair<fl::size, bool> find_slot(const Key &key) const {
        const fl::size cap = _buckets.size();
        const fl::size mask = cap - 1;
        const fl::size h = _hash(key) & mask;
        fl::size first_tomb = npos();

        if (cap <= 8) {
            // linear probing
            for (fl::size i = 0; i < cap; ++i) {
                const fl::size idx = (h + i) & mask;

                if (is_empty(idx))
                    return {first_tomb != npos() ? first_tomb : idx, true};
                if (is_deleted(idx)) {
                    if (first_tomb == npos())
                        first_tomb = idx;
                } else if (is_occupied(idx) && _equal(_buckets[idx].key, key)) {
                    return {idx, false};
                }
            }
        } else {
            // quadratic probing up to 8 tries
            fl::size i = 0;
            for (; i < 8; ++i) {
                const fl::size idx = (h + i + i * i) & mask;

                if (is_empty(idx))
                    return {first_tomb != npos() ? first_tomb : idx, true};
                if (is_deleted(idx)) {
                    if (first_tomb == npos())
                        first_tomb = idx;
                } else if (is_occupied(idx) && _equal(_buckets[idx].key, key)) {
                    return {idx, false};
                }
            }
            // fallback to linear for the rest
            for (; i < cap; ++i) {
                const fl::size idx = (h + i) & mask;

                if (is_empty(idx))
                    return {first_tomb != npos() ? first_tomb : idx, true};
                if (is_deleted(idx)) {
                    if (first_tomb == npos())
                        first_tomb = idx;
                } else if (is_occupied(idx) && _equal(_buckets[idx].key, key)) {
                    return {idx, false};
                }
            }
        }

        return {npos(), false};
    }

    enum {
        kLinearProbingOnlySize = 8,
        kQuadraticProbingTries = 8,
    };

    fl::size find_index(const Key &key) const {
        const fl::size cap = _buckets.size();
        const fl::size mask = cap - 1;
        const fl::size h = _hash(key) & mask;

        if (cap <= kLinearProbingOnlySize) {
            // linear probing
            for (fl::size i = 0; i < cap; ++i) {
                const fl::size idx = (h + i) & mask;
                if (is_empty(idx))
                    return npos();
                if (is_occupied(idx) && _equal(_buckets[idx].key, key))
                    return idx;
            }
        } else {
            // quadratic probing up to 8 tries
            fl::size i = 0;
            for (; i < kQuadraticProbingTries; ++i) {
                const fl::size idx = (h + i + i * i) & mask;
                if (is_empty(idx))
                    return npos();
                if (is_occupied(idx) && _equal(_buckets[idx].key, key))
                    return idx;
            }
            // fallback to linear for the rest
            for (; i < cap; ++i) {
                const fl::size idx = (h + i) & mask;
                if (is_empty(idx))
                    return npos();
                if (is_occupied(idx) && _equal(_buckets[idx].key, key))
                    return idx;
            }
        }

        return npos();
    }

    fl::size find_unoccupied_index_using_bitset(
        const Key &key, const fl::bitset<1024> &occupied_set) const {
        const fl::size cap = _buckets.size();
        const fl::size mask = cap - 1;
        const fl::size h = _hash(key) & mask;

        if (cap <= kLinearProbingOnlySize) {
            // linear probing
            for (fl::size i = 0; i < cap; ++i) {
                const fl::size idx = (h + i) & mask;
                bool occupied = occupied_set.test(idx);
                if (occupied) {
                    continue;
                }
                return idx;
            }
        } else {
            // quadratic probing up to 8 tries
            fl::size i = 0;
            for (; i < kQuadraticProbingTries; ++i) {
                const fl::size idx = (h + i + i * i) & mask;
                bool occupied = occupied_set.test(idx);
                if (occupied) {
                    continue;
                }
                return idx;
            }
            // fallback to linear for the rest
            for (; i < cap; ++i) {
                const fl::size idx = (h + i) & mask;
                bool occupied = occupied_set.test(idx);
                if (occupied) {
                    continue;
                }
                return idx;
            }
        }
        return npos();
    }

    void rehash_internal(fl::size new_cap) {
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

        for (fl::size i = 0; i < old.size(); i++) {
            if (old_occupied.test(i))
                insert(fl::move(old[i].key), fl::move(old[i].value));
        }
    }

    // Rehash the inline buckets without resizing
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_NULL_DEREFERENCE
    void rehash_inline_no_resize() {
        // filter out tombstones and compact
        fl::size cap = _buckets.size();
        fl::size pos = 0;
        // compact live elements to the left.
        for (fl::size i = 0; i < cap; ++i) {
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
        for (fl::size i = 0; i < _size; ++i) {
            const bool already_finished = occupied.test(i);
            if (already_finished) {
                continue;
            }
            auto &e = _buckets[i];
            // FASTLED_ASSERT(e.state == EntryState::Occupied,
            //                "unordered_map::rehash_inline_no_resize: invalid
            //                state");

            fl::size idx = find_unoccupied_index_using_bitset(e.key, occupied);
            if (idx == npos()) {
                // no more space
                FASTLED_ASSERT(
                    false, "unordered_map::rehash_inline_no_resize: invalid index at "
                               << idx << " which is " << npos());
                return;
            }
            // if idx < pos then we are moving the entry to a new location
            FASTLED_ASSERT(!tmp,
                           "unordered_map::rehash_inline_no_resize: invalid tmp");
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
                fl::size new_idx =
                    find_unoccupied_index_using_bitset(key, occupied);
                if (new_idx == npos()) {
                    // no more space
                    FASTLED_ASSERT(
                        false,
                        "unordered_map::rehash_inline_no_resize: invalid index at "
                            << new_idx << " which is " << npos());
                    return;
                }
                occupied.set(new_idx);
                if (new_idx < _size) {
                    // we have to swap the entry at new_idx with tmp
                    fl::optional<Entry> tmp2 = _buckets[new_idx];
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
                "unordered_map::rehash_inline_no_resize: invalid occupied at " << i);
            FASTLED_ASSERT(
                tmp.empty(), "unordered_map::rehash_inline_no_resize: invalid tmp at " << i);
        }
        // Reset tombstones count since we've cleared all deleted entries
        _tombstones = 0;
    }
    FL_DISABLE_WARNING_POP

    fl::vector_inlined<Entry, INLINED_COUNT> _buckets;
    fl::size _size;
    fl::size _tombstones;
    u8 mLoadFactor;
    fl::bitset<1024> _occupied;
    fl::bitset<1024> _deleted;
    Hash _hash;
    KeyEqual _equal;
};

// begin using declarations for stl compatibility
template <typename T> using equal_to = EqualTo<T>;

// Legacy alias for backwards compatibility - use fl::unordered_map instead
template <typename Key, typename T, typename Hash = Hash<Key>,
          typename KeyEqual = equal_to<Key>>
using hash_map = unordered_map<Key, T, Hash, KeyEqual>;
// end using declarations for stl compatibility

} // namespace fl

FL_DISABLE_WARNING_POP
