#pragma once

#include "fl/stl/int.h"
#include "fl/stl/move.h"    // for fl::move
#include "fl/stl/vector.h"  // for fl::vector_inlined
#include "fl/stl/memory_resource.h"
#include "fl/stl/noexcept.h"

namespace fl {

// Core circular buffer logic operating on caller-owned storage.
// Uses a boolean flag (mFull) to distinguish full from empty when head == tail.
template <typename T>
class circular_buffer_core {
  public:
    circular_buffer_core() FL_NO_EXCEPT : mData(nullptr), mCapacity(0), mHead(0), mTail(0), mFull(false) {}

    circular_buffer_core(T* data, fl::size capacity) FL_NO_EXCEPT
        : mData(data), mCapacity(capacity), mHead(0), mTail(0), mFull(false) {}

    void assign(T* data, fl::size capacity) FL_NO_EXCEPT {
        mData = data;
        mCapacity = capacity;
        mHead = 0;
        mTail = 0;
        mFull = false;
    }

    bool push_back(const T &value) FL_NO_EXCEPT {
        if (mCapacity == 0) return false;
        if (mFull) {
            // Overwrite oldest: advance tail
            mTail = increment(mTail);
        }
        mData[mHead] = value;
        mHead = increment(mHead);
        if (mHead == mTail) {
            mFull = true;
        }
        return true;
    }

    template<typename... Args>
    bool emplace_back(Args&&... args) FL_NO_EXCEPT {
        if (mCapacity == 0) return false;
        if (mFull) {
            // Overwrite oldest: advance tail
            mTail = increment(mTail);
        }
        mData[mHead] = T(fl::forward<Args>(args)...);
        mHead = increment(mHead);
        if (mHead == mTail) {
            mFull = true;
        }
        return true;
    }

    bool pop_front(T *dst = nullptr) FL_NO_EXCEPT {
        if (empty()) {
            return false;
        }
        if (dst) {
            *dst = fl::move(mData[mTail]);
        }
        mData[mTail] = T();
        mTail = increment(mTail);
        mFull = false;
        return true;
    }

    bool push_front(const T &value) FL_NO_EXCEPT {
        if (mCapacity == 0) return false;
        if (mFull) {
            // Overwrite newest: retreat head
            mHead = decrement(mHead);
        }
        mTail = decrement(mTail);
        mData[mTail] = value;
        if (mHead == mTail) {
            mFull = true;
        }
        return true;
    }

    template<typename... Args>
    bool emplace_front(Args&&... args) FL_NO_EXCEPT {
        if (mCapacity == 0) return false;
        if (mFull) {
            // Overwrite newest: retreat head
            mHead = decrement(mHead);
        }
        mTail = decrement(mTail);
        mData[mTail] = T(fl::forward<Args>(args)...);
        if (mHead == mTail) {
            mFull = true;
        }
        return true;
    }

    bool pop_back(T *dst = nullptr) FL_NO_EXCEPT {
        if (empty()) {
            return false;
        }
        mHead = decrement(mHead);
        if (dst) {
            *dst = fl::move(mData[mHead]);
        }
        mData[mHead] = T();
        mFull = false;
        return true;
    }

    T& front() FL_NO_EXCEPT { return mData[mTail]; }
    const T& front() const FL_NO_EXCEPT { return mData[mTail]; }

    T& back() FL_NO_EXCEPT { return mData[decrement(mHead)]; }
    const T& back() const FL_NO_EXCEPT { return mData[decrement(mHead)]; }

    T& operator[](fl::size index) FL_NO_EXCEPT {
        return mData[(mTail + index) % mCapacity];
    }
    const T& operator[](fl::size index) const FL_NO_EXCEPT {
        return mData[(mTail + index) % mCapacity];
    }

    fl::size size() const FL_NO_EXCEPT {
        if (mCapacity == 0) return 0;
        if (mFull) return mCapacity;
        return (mHead + mCapacity - mTail) % mCapacity;
    }
    fl::size capacity() const FL_NO_EXCEPT { return mCapacity; }
    bool empty() const FL_NO_EXCEPT { return !mFull && mHead == mTail; }
    bool full() const FL_NO_EXCEPT { return mFull; }

    void clear() FL_NO_EXCEPT {
        while (!empty()) {
            pop_front();
        }
    }

    // Expose head/tail/full for move operations.
    fl::size head() const FL_NO_EXCEPT { return mHead; }
    fl::size tail() const FL_NO_EXCEPT { return mTail; }
    bool isFull() const FL_NO_EXCEPT { return mFull; }
    void setHead(fl::size h) FL_NO_EXCEPT { mHead = h; }
    void setTail(fl::size t) FL_NO_EXCEPT { mTail = t; }
    void setFull(bool f) FL_NO_EXCEPT { mFull = f; }

  private:
    fl::size increment(fl::size index) const FL_NO_EXCEPT { return (index + 1) % mCapacity; }
    fl::size decrement(fl::size index) const FL_NO_EXCEPT {
        return (index + mCapacity - 1) % mCapacity;
    }

    T* mData;
    fl::size mCapacity;
    fl::size mHead;
    fl::size mTail;
    bool mFull;
};

// Unified circular buffer: N > 0 for inline storage, N == 0 for dynamic.
// Uses vector_inlined internally — inline when N > 0, heap when N == 0.
template <typename T, fl::size N = 0>
class circular_buffer {
  public:
    // Default constructor — pre-sizes to N (useful when N > 0).
    circular_buffer() FL_NO_EXCEPT {
        mStorage.resize(N);
        mCore.assign(mStorage.data(), N);
    }

    // PMR-aware constructor.
    explicit circular_buffer(memory_resource* resource) FL_NO_EXCEPT
        : mStorage(resource) {
        mStorage.resize(N);
        mCore.assign(mStorage.data(), N);
    }

    // Capacity constructor — for dynamic (N==0) or overriding static size.
    explicit circular_buffer(fl::size capacity) FL_NO_EXCEPT {
        mStorage.resize(capacity);
        mCore.assign(mStorage.data(), capacity);
    }

    // Capacity constructor with PMR.
    circular_buffer(fl::size capacity, memory_resource* resource) FL_NO_EXCEPT
        : mStorage(resource) {
        mStorage.resize(capacity);
        mCore.assign(mStorage.data(), capacity);
    }

    circular_buffer(const circular_buffer& other) FL_NO_EXCEPT
        : mStorage(other.mStorage),
          mCore(mStorage.data(), mStorage.size()) {
        mCore.setHead(other.mCore.head());
        mCore.setTail(other.mCore.tail());
        mCore.setFull(other.mCore.isFull());
    }

    circular_buffer(circular_buffer&& other) FL_NO_EXCEPT
        : mStorage(fl::move(other.mStorage)),
          mCore(mStorage.data(), mStorage.size()) {
        mCore.setHead(other.mCore.head());
        mCore.setTail(other.mCore.tail());
        mCore.setFull(other.mCore.isFull());
        other.mCore.assign(nullptr, 0);
    }

    circular_buffer& operator=(const circular_buffer& other) FL_NO_EXCEPT {
        if (this != &other) {
            mStorage = other.mStorage;
            mCore.assign(mStorage.data(), mStorage.size());
            mCore.setHead(other.mCore.head());
            mCore.setTail(other.mCore.tail());
            mCore.setFull(other.mCore.isFull());
        }
        return *this;
    }

    circular_buffer& operator=(circular_buffer&& other) FL_NO_EXCEPT {
        if (this != &other) {
            mStorage = fl::move(other.mStorage);
            mCore.assign(mStorage.data(), mStorage.size());
            mCore.setHead(other.mCore.head());
            mCore.setTail(other.mCore.tail());
            mCore.setFull(other.mCore.isFull());
            other.mCore.assign(nullptr, 0);
        }
        return *this;
    }

    void push(const T &value) FL_NO_EXCEPT { mCore.push_back(value); }
    bool push_back(const T &value) FL_NO_EXCEPT { return mCore.push_back(value); }
    bool push_front(const T &value) FL_NO_EXCEPT { return mCore.push_front(value); }

    template<typename... Args>
    bool emplace_back(Args&&... args) FL_NO_EXCEPT { return mCore.emplace_back(fl::forward<Args>(args)...); }

    template<typename... Args>
    bool emplace_front(Args&&... args) FL_NO_EXCEPT { return mCore.emplace_front(fl::forward<Args>(args)...); }

    bool pop(T &value) FL_NO_EXCEPT { return mCore.pop_front(&value); }
    bool pop_front(T *dst = nullptr) FL_NO_EXCEPT { return mCore.pop_front(dst); }
    bool pop_back(T *dst = nullptr) FL_NO_EXCEPT { return mCore.pop_back(dst); }

    T& front() FL_NO_EXCEPT { return mCore.front(); }
    const T& front() const FL_NO_EXCEPT { return mCore.front(); }

    T& back() FL_NO_EXCEPT { return mCore.back(); }
    const T& back() const FL_NO_EXCEPT { return mCore.back(); }

    T& operator[](fl::size index) FL_NO_EXCEPT { return mCore[index]; }
    const T& operator[](fl::size index) const FL_NO_EXCEPT { return mCore[index]; }

    fl::size size() const FL_NO_EXCEPT { return mCore.size(); }
    fl::size capacity() const FL_NO_EXCEPT { return mCore.capacity(); }
    bool empty() const FL_NO_EXCEPT { return mCore.empty(); }
    bool full() const FL_NO_EXCEPT { return mCore.full(); }
    void clear() FL_NO_EXCEPT { mCore.clear(); }

    void resize(fl::size new_capacity) FL_NO_EXCEPT {
        // Save existing elements
        fl::size count = mCore.size();
        fl::size to_save = (count < new_capacity) ? count : new_capacity;
        // Use a temporary storage to save elements
        vector_inlined<T, (N > 0 ? N : 1)> saved;
        saved.resize(to_save);
        for (fl::size i = 0; i < to_save; ++i) {
            saved[i] = mCore[i];
        }
        // Resize storage
        mStorage.resize(new_capacity);
        mCore.assign(mStorage.data(), new_capacity);
        // Re-insert saved elements
        for (fl::size i = 0; i < to_save; ++i) {
            mCore.push_back(saved[i]);
        }
    }

    /// Equality comparison
    bool operator==(const circular_buffer& other) const FL_NO_EXCEPT {
        if (size() != other.size()) return false;
        for (fl::size i = 0; i < size(); ++i) {
            if (mCore[i] != other.mCore[i]) return false;
        }
        return true;
    }

    /// Inequality comparison
    bool operator!=(const circular_buffer& other) const FL_NO_EXCEPT {
        return !(*this == other);
    }

    /// Lexicographic comparison
    bool operator<(const circular_buffer& other) const FL_NO_EXCEPT {
        fl::size min_size = (size() < other.size()) ? size() : other.size();
        for (fl::size i = 0; i < min_size; ++i) {
            if (mCore[i] < other.mCore[i]) return true;
            if (other.mCore[i] < mCore[i]) return false;
        }
        return size() < other.size();
    }

    /// Less-than-or-equal comparison
    bool operator<=(const circular_buffer& other) const FL_NO_EXCEPT {
        return *this < other || *this == other;
    }

    /// Greater-than comparison
    bool operator>(const circular_buffer& other) const FL_NO_EXCEPT {
        return other < *this;
    }

    /// Greater-than-or-equal comparison
    bool operator>=(const circular_buffer& other) const FL_NO_EXCEPT {
        return other <= *this;
    }

    memory_resource* get_memory_resource() const FL_NO_EXCEPT { return mStorage.get_resource(); }

  private:
    vector_inlined<T, (N > 0 ? N : 1)> mStorage;
    circular_buffer_core<T> mCore;
};

} // namespace fl
