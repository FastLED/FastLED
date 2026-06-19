#pragma once

#include "fl/stl/stdint.h"

#include "fl/stl/deque_basic.h"  // detail::deque_grow_map_capacity (#3270)
#include "fl/stl/move.h"
#include "fl/stl/iterator.h"
#include "fl/stl/memory_resource.h"
#include "fl/stl/new.h"  // IWYU pragma: keep
#include "fl/stl/noexcept.h"
#include "fl/system/sketch_macros.h"  // FL_PLATFORM_HAS_LARGE_MEMORY

namespace fl {

// Per-type chunk-size trait. Specialize to override the default chunk size
// for a particular T (e.g. when the typical deque<T> sliding window has a
// known fixed extent that should match the chunk size). See FastLED #3270.
template <typename T>
struct deque_traits {
    static constexpr fl::size chunk_size =
#if FL_PLATFORM_HAS_LARGE_MEMORY
        64u;
#else
        16u;
#endif
};

template <typename T>
class deque {
private:
    // Map-of-fixed-size-chunks layout. Existing elements are NEVER moved on
    // grow; only the chunk-pointer map is reallocated when one side runs out
    // of room. push_back / push_front allocate a new chunk at the active
    // end as needed; intermediate chunks (between front and back) are
    // always allocated. See FastLED #3270.
    static constexpr fl::size kChunkSize = deque_traits<T>::chunk_size;

    T** mMap = nullptr;            // array of T* chunk pointers, length mMapCapacity
    fl::size mMapCapacity = 0;     // number of chunk slots in mMap
    fl::size mFrontMapIdx = 0;     // index in mMap of the chunk holding the front element
    fl::size mFrontOffset = 0;     // offset within front chunk of the front element [0, kChunkSize)
    fl::size mSize = 0;            // total elements
    memory_resource* mResource = default_memory_resource();

    T* allocate_chunk() FL_NO_EXCEPT {
        return static_cast<T*>(mResource->allocate(kChunkSize * sizeof(T)));
    }

    void deallocate_chunk(T* chunk) FL_NO_EXCEPT {
        if (chunk) {
            mResource->deallocate(chunk, kChunkSize * sizeof(T));
        }
    }

    T** allocate_map(fl::size capacity) FL_NO_EXCEPT {
        T** m = static_cast<T**>(mResource->allocate(capacity * sizeof(T*)));
        if (m) {
            for (fl::size i = 0; i < capacity; ++i) {
                m[i] = nullptr;
            }
        }
        return m;
    }

    void deallocate_map(T** map, fl::size capacity) FL_NO_EXCEPT {
        if (map) {
            mResource->deallocate(map, capacity * sizeof(T*));
        }
    }

    // Translate logical index -> (chunk_idx in mMap, offset within chunk).
    // Caller validates `logical_idx < mSize` (or == mSize for end-position).
    void locate(fl::size logical_idx, fl::size& chunk_idx, fl::size& offset) const FL_NO_EXCEPT {
        fl::size global = mFrontOffset + logical_idx;
        chunk_idx = mFrontMapIdx + global / kChunkSize;
        offset = global % kChunkSize;
    }

    // Number of currently-used chunk slots [mFrontMapIdx .. back_chunk_idx].
    fl::size used_chunks() const FL_NO_EXCEPT {
        if (mSize == 0) return 0;
        fl::size last_global = mFrontOffset + mSize - 1;
        fl::size last_chunk = mFrontMapIdx + last_global / kChunkSize;
        return last_chunk - mFrontMapIdx + 1;
    }

    // Grow the chunk-pointer map. After return:
    //   - mMap has at least `extra_front` empty slots before mFrontMapIdx
    //   - mMap has at least `extra_back` empty slots after the back chunk
    // Live chunk pointers in [mFrontMapIdx, mFrontMapIdx+used) are preserved
    // (chunks themselves are never moved). Orphaned chunks outside that range
    // (left behind by prior pop_front / pop_back when a chunk emptied without
    // being released) are FREED here -- otherwise the old `mMap` is
    // deallocated below and those chunk pointers are lost forever, leaking
    // their backing storage. See FastLED #3286.
    // mFrontMapIdx may shift to a new position within the new map.
    void grow_map(fl::size extra_front, fl::size extra_back) FL_NO_EXCEPT {
        fl::size used = used_chunks();
        fl::size needed = used + extra_front + extra_back;
        fl::size new_cap = detail::deque_grow_map_capacity(mMapCapacity, needed);
        // Center the existing chunks within the new map, biased to leave the
        // requested extra room on each side.
        fl::size new_front = extra_front + (new_cap - needed) / 2;
        T** new_map = allocate_map(new_cap);
        if (!new_map) {
            // Allocation failure: match the silent-fail behavior of the prior
            // vector-style ensure_capacity. Callers must defensively check
            // capacity / size after push_*.
            return;
        }
        // Free any chunk pointers in the OLD map that live outside the
        // [mFrontMapIdx, mFrontMapIdx+used) live range; those are orphans
        // and would otherwise leak when the old map is deallocated.
        fl::size live_end = mFrontMapIdx + used;
        for (fl::size i = 0; i < mMapCapacity; ++i) {
            if (i < mFrontMapIdx || i >= live_end) {
                deallocate_chunk(mMap[i]);
                mMap[i] = nullptr;
            }
        }
        for (fl::size i = 0; i < used; ++i) {
            new_map[new_front + i] = mMap[mFrontMapIdx + i];
        }
        deallocate_map(mMap, mMapCapacity);
        mMap = new_map;
        mMapCapacity = new_cap;
        mFrontMapIdx = new_front;
    }

    // Ensure the chunk that will hold the next push_back exists. May grow
    // the map if the back chunk lies past mMapCapacity.
    void ensure_back_room() FL_NO_EXCEPT {
        fl::size global = mFrontOffset + mSize;
        fl::size need_chunk_idx = mFrontMapIdx + global / kChunkSize;
        if (need_chunk_idx >= mMapCapacity) {
            grow_map(0, 1);
            global = mFrontOffset + mSize;
            need_chunk_idx = mFrontMapIdx + global / kChunkSize;
            if (need_chunk_idx >= mMapCapacity) return;  // grow failed
        }
        if (mMap[need_chunk_idx] == nullptr) {
            mMap[need_chunk_idx] = allocate_chunk();
        }
    }

    // Ensure the chunk that will hold the next push_front exists. May grow
    // the map if push_front would step before mMap[0].
    void ensure_front_room() FL_NO_EXCEPT {
        if (mFrontOffset > 0) {
            // Stays within current front chunk; that chunk is guaranteed
            // allocated when mSize > 0, and allocated below for empty deque.
            if (mMapCapacity == 0) {
                grow_map(1, 0);
                if (mMapCapacity == 0) return;
            }
            if (mMap[mFrontMapIdx] == nullptr) {
                mMap[mFrontMapIdx] = allocate_chunk();
            }
            return;
        }
        // mFrontOffset == 0: need the previous chunk.
        if (mFrontMapIdx == 0) {
            grow_map(1, 0);
            if (mFrontMapIdx == 0) return;  // grow failed
        }
        if (mMap[mFrontMapIdx - 1] == nullptr) {
            mMap[mFrontMapIdx - 1] = allocate_chunk();
        }
    }

public:
    // Iterator implementation (RandomAccessIterator). The iterator carries
    // only `(deque*, logical_index)` and resolves to a chunk pointer at
    // dereference time. Pointer stability for T* taken at insertion time is
    // provided by chunks themselves never being moved; iterator stability
    // is preserved across push at either end (logical_index of pre-existing
    // elements doesn't change for push_back, and is adjusted internally by
    // mFrontOffset / mFrontMapIdx for push_front).
    class iterator {
    public:
        typedef T value_type;
        typedef T& reference;
        typedef T* pointer;
        typedef fl::size difference_type;
        typedef fl::random_access_iterator_tag iterator_category;

    private:
        deque* mDeque;
        fl::size mIndex;

        friend class deque;

    public:
        iterator(deque* dq, fl::size index) : mDeque(dq), mIndex(index) {}

        T& operator*() const {
            return (*mDeque)[mIndex];
        }

        T* operator->() const {
            return &(*mDeque)[mIndex];
        }

        iterator& operator++() {
            ++mIndex;
            return *this;
        }

        iterator operator++(int) {
            iterator temp = *this;
            ++mIndex;
            return temp;
        }

        iterator& operator--() {
            --mIndex;
            return *this;
        }

        iterator operator--(int) {
            iterator temp = *this;
            --mIndex;
            return temp;
        }

        iterator& operator+=(fl::size n) {
            mIndex += n;
            return *this;
        }

        iterator operator+(fl::size n) const {
            iterator temp = *this;
            return temp += n;
        }

        iterator& operator-=(fl::size n) {
            mIndex -= n;
            return *this;
        }

        iterator operator-(fl::size n) const {
            iterator temp = *this;
            return temp -= n;
        }

        fl::size operator-(const iterator& other) const {
            return mIndex - other.mIndex;
        }

        T& operator[](fl::size n) const {
            return (*mDeque)[mIndex + n];
        }

        bool operator==(const iterator& other) const {
            return mDeque == other.mDeque && mIndex == other.mIndex;
        }

        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }

        bool operator<(const iterator& other) const {
            return mIndex < other.mIndex;
        }

        bool operator<=(const iterator& other) const {
            return mIndex <= other.mIndex;
        }

        bool operator>(const iterator& other) const {
            return mIndex > other.mIndex;
        }

        bool operator>=(const iterator& other) const {
            return mIndex >= other.mIndex;
        }
    };

    class const_iterator {
    public:
        typedef T value_type;
        typedef const T& reference;
        typedef const T* pointer;
        typedef fl::size difference_type;
        typedef fl::random_access_iterator_tag iterator_category;

    private:
        const deque* mDeque;
        fl::size mIndex;

        friend class deque;

    public:
        const_iterator(const deque* dq, fl::size index) : mDeque(dq), mIndex(index) {}

        // Implicit conversion from iterator to const_iterator
        const_iterator(const iterator& it) : mDeque(it.mDeque), mIndex(it.mIndex) {}

        const T& operator*() const {
            return (*mDeque)[mIndex];
        }

        const T* operator->() const {
            return &(*mDeque)[mIndex];
        }

        const_iterator& operator++() {
            ++mIndex;
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator temp = *this;
            ++mIndex;
            return temp;
        }

        const_iterator& operator--() {
            --mIndex;
            return *this;
        }

        const_iterator operator--(int) {
            const_iterator temp = *this;
            --mIndex;
            return temp;
        }

        const_iterator& operator+=(fl::size n) {
            mIndex += n;
            return *this;
        }

        const_iterator operator+(fl::size n) const {
            const_iterator temp = *this;
            return temp += n;
        }

        const_iterator& operator-=(fl::size n) {
            mIndex -= n;
            return *this;
        }

        const_iterator operator-(fl::size n) const {
            const_iterator temp = *this;
            return temp -= n;
        }

        fl::size operator-(const const_iterator& other) const {
            return mIndex - other.mIndex;
        }

        const T& operator[](fl::size n) const {
            return (*mDeque)[mIndex + n];
        }

        bool operator==(const const_iterator& other) const {
            return mDeque == other.mDeque && mIndex == other.mIndex;
        }

        bool operator!=(const const_iterator& other) const {
            return !(*this == other);
        }

        bool operator<(const const_iterator& other) const {
            return mIndex < other.mIndex;
        }

        bool operator<=(const const_iterator& other) const {
            return mIndex <= other.mIndex;
        }

        bool operator>(const const_iterator& other) const {
            return mIndex > other.mIndex;
        }

        bool operator>=(const const_iterator& other) const {
            return mIndex >= other.mIndex;
        }
    };

    typedef fl::reverse_iterator<iterator> reverse_iterator;
    typedef fl::reverse_iterator<const_iterator> const_reverse_iterator;

    // Constructors
    deque() FL_NO_EXCEPT {}

    explicit deque(memory_resource* resource) FL_NO_EXCEPT : mResource(resource) {}

    explicit deque(fl::size count, const T& value = T()) : deque() {
        resize(count, value);
    }

    deque(const deque& other) FL_NO_EXCEPT : deque() {
        *this = other;
    }

    deque(deque&& other) FL_NO_EXCEPT : deque() {
        *this = fl::move(other);
    }

    deque(fl::initializer_list<T> init) : deque() {
        for (const auto& value : init) {
            push_back(value);
        }
    }

    ~deque() FL_NO_EXCEPT {
        clear();
        // Deallocate any remaining chunk pointers (clear() releases element
        // storage but may leave the map intact for reuse on a still-live deque;
        // here we tear it down for good).
        for (fl::size i = 0; i < mMapCapacity; ++i) {
            deallocate_chunk(mMap[i]);
        }
        deallocate_map(mMap, mMapCapacity);
    }

    deque& operator=(const deque& other) FL_NO_EXCEPT {
        if (this != &other) {
            clear();
            for (fl::size i = 0; i < other.size(); ++i) {
                push_back(other[i]);
            }
        }
        return *this;
    }

    deque& operator=(deque&& other) FL_NO_EXCEPT {
        if (this != &other) {
            // Tear down current state.
            clear();
            for (fl::size i = 0; i < mMapCapacity; ++i) {
                deallocate_chunk(mMap[i]);
            }
            deallocate_map(mMap, mMapCapacity);

            mMap = other.mMap;
            mMapCapacity = other.mMapCapacity;
            mFrontMapIdx = other.mFrontMapIdx;
            mFrontOffset = other.mFrontOffset;
            mSize = other.mSize;
            mResource = other.mResource;

            other.mMap = nullptr;
            other.mMapCapacity = 0;
            other.mFrontMapIdx = 0;
            other.mFrontOffset = 0;
            other.mSize = 0;
        }
        return *this;
    }

    // Element access
    T& operator[](fl::size index) {
        fl::size c, o; locate(index, c, o);
        return mMap[c][o];
    }

    const T& operator[](fl::size index) const {
        fl::size c, o; locate(index, c, o);
        return mMap[c][o];
    }

    T& at(fl::size index) {
        if (index >= mSize) {
            // Bounds error: return front in embedded context (matching the
            // pre-#3270 deque's documented fallback).
            return front();
        }
        fl::size c, o; locate(index, c, o);
        return mMap[c][o];
    }

    const T& at(fl::size index) const {
        if (index >= mSize) {
            return front();
        }
        fl::size c, o; locate(index, c, o);
        return mMap[c][o];
    }

    T& front() {
        return mMap[mFrontMapIdx][mFrontOffset];
    }

    const T& front() const {
        return mMap[mFrontMapIdx][mFrontOffset];
    }

    T& back() {
        fl::size c, o; locate(mSize - 1, c, o);
        return mMap[c][o];
    }

    const T& back() const {
        fl::size c, o; locate(mSize - 1, c, o);
        return mMap[c][o];
    }

    iterator begin() FL_NO_EXCEPT { return iterator(this, 0); }
    const_iterator begin() const FL_NO_EXCEPT { return const_iterator(this, 0); }
    iterator end() FL_NO_EXCEPT { return iterator(this, mSize); }
    const_iterator end() const FL_NO_EXCEPT { return const_iterator(this, mSize); }

    reverse_iterator rbegin() FL_NO_EXCEPT { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const FL_NO_EXCEPT { return const_reverse_iterator(end()); }
    reverse_iterator rend() FL_NO_EXCEPT { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const FL_NO_EXCEPT { return const_reverse_iterator(begin()); }

    const_iterator cbegin() const FL_NO_EXCEPT { return const_iterator(this, 0); }
    const_iterator cend() const FL_NO_EXCEPT { return const_iterator(this, mSize); }
    const_reverse_iterator crbegin() const FL_NO_EXCEPT { return const_reverse_iterator(cend()); }
    const_reverse_iterator crend() const FL_NO_EXCEPT { return const_reverse_iterator(cbegin()); }

    bool empty() const FL_NO_EXCEPT { return mSize == 0; }
    fl::size size() const FL_NO_EXCEPT { return mSize; }

    // Total addressable storage in the currently-allocated chunks. Returns
    // a chunk-size-quantized value (multiple of kChunkSize). The old
    // vector-style deque reported exact element capacity; chunked deques
    // report whole-chunk capacity. See FastLED #3270 migration plan.
    fl::size capacity() const {
        fl::size allocated = 0;
        for (fl::size i = 0; i < mMapCapacity; ++i) {
            if (mMap[i] != nullptr) ++allocated;
        }
        return allocated * kChunkSize;
    }

    fl::size max_size() const {
        return static_cast<fl::size>(-1) / sizeof(T);
    }

    // Ensure capacity for at least `n` elements from the current front.
    // Chunked: pre-allocates ceil(n / kChunkSize) chunks at the back.
    void reserve(fl::size n) FL_NO_EXCEPT {
        if (n <= capacity()) return;
        // Allocate enough chunks behind the current front-offset to hold n total.
        fl::size needed_chunks = (mFrontOffset + n + kChunkSize - 1) / kChunkSize;
        fl::size cur_chunks = used_chunks();
        if (cur_chunks < needed_chunks) {
            fl::size extra = needed_chunks - cur_chunks;
            // Ensure mMap has room for `extra` more chunks past current back.
            fl::size last_used_idx = (mSize == 0) ? mFrontMapIdx : (mFrontMapIdx + used_chunks() - 1);
            if (last_used_idx + extra >= mMapCapacity) {
                grow_map(0, extra);
            }
            // Allocate chunks at the back to cover [front .. front+n).
            fl::size first_new = (mSize == 0) ? mFrontMapIdx : (mFrontMapIdx + cur_chunks);
            for (fl::size i = 0; i < extra; ++i) {
                if (first_new + i >= mMapCapacity) break;
                if (mMap[first_new + i] == nullptr) {
                    mMap[first_new + i] = allocate_chunk();
                }
            }
        }
    }

    // Release chunks that hold no live elements. After return, only the
    // chunks spanning [front .. back] remain allocated. Capacity is
    // quantized to kChunkSize and equals ceil(size / kChunkSize) * kChunkSize.
    void shrink_to_fit() {
        if (mSize == 0) {
            // Release every chunk and the map itself.
            for (fl::size i = 0; i < mMapCapacity; ++i) {
                deallocate_chunk(mMap[i]);
                mMap[i] = nullptr;
            }
            deallocate_map(mMap, mMapCapacity);
            mMap = nullptr;
            mMapCapacity = 0;
            mFrontMapIdx = 0;
            mFrontOffset = 0;
            return;
        }
        // Release chunks outside [front .. back].
        fl::size last_used = mFrontMapIdx + used_chunks() - 1;
        for (fl::size i = 0; i < mMapCapacity; ++i) {
            if (i < mFrontMapIdx || i > last_used) {
                deallocate_chunk(mMap[i]);
                mMap[i] = nullptr;
            }
        }
    }

    memory_resource* get_memory_resource() const FL_NO_EXCEPT { return mResource; }

    void clear() {
        for (fl::size i = 0; i < mSize; ++i) {
            fl::size c, o; locate(i, c, o);
            mMap[c][o].~T();
        }
        mSize = 0;
        // Reset front to the start of its current chunk so subsequent
        // pushes don't waste leading-chunk space. Chunks themselves stay
        // allocated for reuse; callers wanting deallocation should call
        // shrink_to_fit() afterward.
        mFrontOffset = 0;
    }

    void push_back(const T& value) {
        ensure_back_room();
        fl::size global = mFrontOffset + mSize;
        fl::size c = mFrontMapIdx + global / kChunkSize;
        fl::size o = global % kChunkSize;
        new (&mMap[c][o]) T(value);
        ++mSize;
    }

    void push_back(T&& value) {
        ensure_back_room();
        fl::size global = mFrontOffset + mSize;
        fl::size c = mFrontMapIdx + global / kChunkSize;
        fl::size o = global % kChunkSize;
        new (&mMap[c][o]) T(fl::move(value));
        ++mSize;
    }

    void push_front(const T& value) {
        ensure_front_room();
        if (mFrontOffset == 0) {
            --mFrontMapIdx;
            mFrontOffset = kChunkSize - 1;
        } else {
            --mFrontOffset;
        }
        new (&mMap[mFrontMapIdx][mFrontOffset]) T(value);
        ++mSize;
    }

    void push_front(T&& value) {
        ensure_front_room();
        if (mFrontOffset == 0) {
            --mFrontMapIdx;
            mFrontOffset = kChunkSize - 1;
        } else {
            --mFrontOffset;
        }
        new (&mMap[mFrontMapIdx][mFrontOffset]) T(fl::move(value));
        ++mSize;
    }

    void pop_back() {
        if (mSize == 0) return;
        fl::size c, o; locate(mSize - 1, c, o);
        mMap[c][o].~T();
        --mSize;
    }

    void pop_front() {
        if (mSize == 0) return;
        mMap[mFrontMapIdx][mFrontOffset].~T();
        ++mFrontOffset;
        if (mFrontOffset == kChunkSize) {
            mFrontOffset = 0;
            ++mFrontMapIdx;
        }
        --mSize;
    }

    void resize(fl::size new_size) {
        resize(new_size, T());
    }

    void resize(fl::size new_size, const T& value) {
        while (mSize < new_size) push_back(value);
        while (mSize > new_size) pop_back();
    }

    void swap(deque& other) {
        if (this != &other) {
            T** t_map = mMap;
            fl::size t_cap = mMapCapacity;
            fl::size t_idx = mFrontMapIdx;
            fl::size t_off = mFrontOffset;
            fl::size t_size = mSize;
            memory_resource* t_res = mResource;

            mMap = other.mMap;
            mMapCapacity = other.mMapCapacity;
            mFrontMapIdx = other.mFrontMapIdx;
            mFrontOffset = other.mFrontOffset;
            mSize = other.mSize;
            mResource = other.mResource;

            other.mMap = t_map;
            other.mMapCapacity = t_cap;
            other.mFrontMapIdx = t_idx;
            other.mFrontOffset = t_off;
            other.mSize = t_size;
            other.mResource = t_res;
        }
    }

    // Insert / emplace / erase use a higher-level strategy than the
    // vector-style deque: instead of manual ~T() + placement-new bookkeeping
    // across un-allocated chunk slots, they (1) extend the deque by pushing
    // a copy of the current back (or the new value when the deque is
    // empty), which routes through push_back's chunk-allocation path, then
    // (2) shift via operator[] assignment. Erase is symmetric: shift via
    // assignment then pop_back. Requires CopyConstructible + CopyAssignable
    // T, the same contract as std::deque::insert.

    iterator insert(const_iterator pos, const T& value) {
        fl::size idx = pos.mIndex;
        if (idx == mSize) {
            push_back(value);
            return iterator(this, idx);
        }
        // Push a copy of the current back to extend by one; chunk alloc
        // is handled inside push_back.
        push_back((*this)[mSize - 1]);
        for (fl::size i = mSize - 1; i > idx + 1; --i) {
            (*this)[i - 1] = fl::move((*this)[i - 2]);
        }
        (*this)[idx] = value;
        return iterator(this, idx);
    }

    iterator insert(const_iterator pos, T&& value) {
        fl::size idx = pos.mIndex;
        if (idx == mSize) {
            push_back(fl::move(value));
            return iterator(this, idx);
        }
        // Move-extend at the back so move-only T works. The moved-from
        // slot at old logical mSize-1 is overwritten by the shift loop.
        push_back(fl::move((*this)[mSize - 1]));
        for (fl::size i = mSize - 1; i > idx + 1; --i) {
            (*this)[i - 1] = fl::move((*this)[i - 2]);
        }
        (*this)[idx] = fl::move(value);
        return iterator(this, idx);
    }

    iterator insert(const_iterator pos, fl::size count, const T& value) {
        fl::size idx = pos.mIndex;
        if (count == 0) return iterator(this, idx);
        fl::size old_size = mSize;
        // Extend by `count` slots. First push uses `value` if the deque
        // is empty, else copies the current back; subsequent pushes copy
        // whatever's at the back now. All these copies will be overwritten
        // by the shift + fill below.
        for (fl::size k = 0; k < count; ++k) {
            if (mSize == 0) push_back(value);
            else push_back((*this)[mSize - 1]);
        }
        // Shift elements [idx .. old_size) right by `count` slots.
        for (fl::size i = old_size; i > idx; --i) {
            (*this)[i - 1 + count] = fl::move((*this)[i - 1]);
        }
        for (fl::size i = 0; i < count; ++i) {
            (*this)[idx + i] = value;
        }
        return iterator(this, idx);
    }

    iterator erase(const_iterator pos) {
        if (pos == end()) return end();
        fl::size idx = pos.mIndex;
        for (fl::size i = idx; i + 1 < mSize; ++i) {
            (*this)[i] = fl::move((*this)[i + 1]);
        }
        pop_back();
        return iterator(this, idx);
    }

    iterator erase(const_iterator first, const_iterator last) {
        fl::size start_idx = first.mIndex;
        if (first == last) return iterator(this, start_idx);
        fl::size count = last.mIndex - first.mIndex;
        for (fl::size i = start_idx; i + count < mSize; ++i) {
            (*this)[i] = fl::move((*this)[i + count]);
        }
        for (fl::size k = 0; k < count; ++k) {
            pop_back();
        }
        return iterator(this, start_idx);
    }

    template<typename... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        fl::size idx = pos.mIndex;
        if (idx == mSize) {
            emplace_back(fl::forward<Args>(args)...);
            return iterator(this, idx);
        }
        // Move-extend at the back so move-only T works. The moved-from
        // slot at old logical mSize-1 is overwritten by the shift loop.
        push_back(fl::move((*this)[mSize - 1]));
        for (fl::size i = mSize - 1; i > idx + 1; --i) {
            (*this)[i - 1] = fl::move((*this)[i - 2]);
        }
        (*this)[idx] = T(fl::forward<Args>(args)...);
        return iterator(this, idx);
    }

    template<typename... Args>
    T& emplace_back(Args&&... args) {
        ensure_back_room();
        fl::size global = mFrontOffset + mSize;
        fl::size c = mFrontMapIdx + global / kChunkSize;
        fl::size o = global % kChunkSize;
        new (&mMap[c][o]) T(fl::forward<Args>(args)...);
        ++mSize;
        return mMap[c][o];
    }

    template<typename... Args>
    T& emplace_front(Args&&... args) {
        ensure_front_room();
        if (mFrontOffset == 0) {
            --mFrontMapIdx;
            mFrontOffset = kChunkSize - 1;
        } else {
            --mFrontOffset;
        }
        new (&mMap[mFrontMapIdx][mFrontOffset]) T(fl::forward<Args>(args)...);
        ++mSize;
        return mMap[mFrontMapIdx][mFrontOffset];
    }

    void assign(fl::size count, const T& value) {
        clear();
        for (fl::size i = 0; i < count; ++i) {
            push_back(value);
        }
    }

    bool operator==(const deque& other) const {
        if (mSize != other.mSize) return false;
        for (fl::size i = 0; i < mSize; ++i) {
            if ((*this)[i] != other[i]) return false;
        }
        return true;
    }

    bool operator!=(const deque& other) const FL_NO_EXCEPT { return !(*this == other); }

    bool operator<(const deque& other) const {
        fl::size min_size = mSize < other.mSize ? mSize : other.mSize;
        for (fl::size i = 0; i < min_size; ++i) {
            if ((*this)[i] < other[i]) return true;
            if ((*this)[i] > other[i]) return false;
        }
        return mSize < other.mSize;
    }

    bool operator<=(const deque& other) const FL_NO_EXCEPT { return *this < other || *this == other; }
    bool operator>(const deque& other) const FL_NO_EXCEPT { return other < *this; }
    bool operator>=(const deque& other) const FL_NO_EXCEPT { return *this > other || *this == other; }
};

typedef deque<int> deque_int;
typedef deque<float> deque_float;
typedef deque<double> deque_double;

} // namespace fl
