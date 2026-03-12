#pragma once

#include "fl/int.h"
#include "fl/stl/move.h"    // for fl::move
#include "fl/stl/vector.h"  // for fl::vector_inlined

namespace fl {

// Core circular buffer logic operating on caller-owned storage.
// Uses a boolean flag (mFull) to distinguish full from empty when head == tail.
template <typename T>
class circular_buffer_core {
  public:
    circular_buffer_core() : mData(nullptr), mCapacity(0), mHead(0), mTail(0), mFull(false) {}

    circular_buffer_core(T* data, fl::size capacity)
        : mData(data), mCapacity(capacity), mHead(0), mTail(0), mFull(false) {}

    void assign(T* data, fl::size capacity) {
        mData = data;
        mCapacity = capacity;
        mHead = 0;
        mTail = 0;
        mFull = false;
    }

    bool push_back(const T &value) {
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
    bool emplace_back(Args&&... args) {
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

    bool pop_front(T *dst = nullptr) {
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

    bool push_front(const T &value) {
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
    bool emplace_front(Args&&... args) {
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

    bool pop_back(T *dst = nullptr) {
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

    T& front() { return mData[mTail]; }
    const T& front() const { return mData[mTail]; }

    T& back() { return mData[decrement(mHead)]; }
    const T& back() const { return mData[decrement(mHead)]; }

    T& operator[](fl::size index) {
        return mData[(mTail + index) % mCapacity];
    }
    const T& operator[](fl::size index) const {
        return mData[(mTail + index) % mCapacity];
    }

    fl::size size() const {
        if (mCapacity == 0) return 0;
        if (mFull) return mCapacity;
        return (mHead + mCapacity - mTail) % mCapacity;
    }
    fl::size capacity() const { return mCapacity; }
    bool empty() const { return !mFull && mHead == mTail; }
    bool full() const { return mFull; }

    void clear() {
        while (!empty()) {
            pop_front();
        }
    }

    // Expose head/tail/full for move operations.
    fl::size head() const { return mHead; }
    fl::size tail() const { return mTail; }
    bool isFull() const { return mFull; }
    void setHead(fl::size h) { mHead = h; }
    void setTail(fl::size t) { mTail = t; }
    void setFull(bool f) { mFull = f; }

    void swap(circular_buffer_core& other) {
        fl_swap(mData, other.mData);
        fl_swap(mCapacity, other.mCapacity);
        fl_swap(mHead, other.mHead);
        fl_swap(mTail, other.mTail);
        fl_swap(mFull, other.mFull);
    }

  private:
    fl::size increment(fl::size index) const { return (index + 1) % mCapacity; }
    fl::size decrement(fl::size index) const {
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
    circular_buffer() {
        mStorage.resize(N);
        mCore.assign(mStorage.data(), N);
    }

    // Capacity constructor — for dynamic (N==0) or overriding static size.
    explicit circular_buffer(fl::size capacity) {
        mStorage.resize(capacity);
        mCore.assign(mStorage.data(), capacity);
    }

    circular_buffer(const circular_buffer& other)
        : mStorage(other.mStorage),
          mCore(mStorage.data(), mStorage.size()) {
        mCore.setHead(other.mCore.head());
        mCore.setTail(other.mCore.tail());
        mCore.setFull(other.mCore.isFull());
    }

    circular_buffer(circular_buffer&& other)
        : mStorage(fl::move(other.mStorage)),
          mCore(mStorage.data(), mStorage.size()) {
        mCore.setHead(other.mCore.head());
        mCore.setTail(other.mCore.tail());
        mCore.setFull(other.mCore.isFull());
        other.mCore.assign(nullptr, 0);
    }

    circular_buffer& operator=(const circular_buffer& other) {
        if (this != &other) {
            mStorage = other.mStorage;
            mCore.assign(mStorage.data(), mStorage.size());
            mCore.setHead(other.mCore.head());
            mCore.setTail(other.mCore.tail());
            mCore.setFull(other.mCore.isFull());
        }
        return *this;
    }

    circular_buffer& operator=(circular_buffer&& other) {
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

    void push(const T &value) { mCore.push_back(value); }
    bool push_back(const T &value) { return mCore.push_back(value); }
    bool push_front(const T &value) { return mCore.push_front(value); }

    template<typename... Args>
    bool emplace_back(Args&&... args) { return mCore.emplace_back(fl::forward<Args>(args)...); }

    template<typename... Args>
    bool emplace_front(Args&&... args) { return mCore.emplace_front(fl::forward<Args>(args)...); }

    bool pop(T &value) { return mCore.pop_front(&value); }
    bool pop_front(T *dst = nullptr) { return mCore.pop_front(dst); }
    bool pop_back(T *dst = nullptr) { return mCore.pop_back(dst); }

    T& front() { return mCore.front(); }
    const T& front() const { return mCore.front(); }

    T& back() { return mCore.back(); }
    const T& back() const { return mCore.back(); }

    T& operator[](fl::size index) { return mCore[index]; }
    const T& operator[](fl::size index) const { return mCore[index]; }

    fl::size size() const { return mCore.size(); }
    fl::size capacity() const { return mCore.capacity(); }
    bool empty() const { return mCore.empty(); }
    bool full() const { return mCore.full(); }
    void clear() { mCore.clear(); }

    void resize(fl::size new_capacity) {
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
    bool operator==(const circular_buffer& other) const {
        if (size() != other.size()) return false;
        for (fl::size i = 0; i < size(); ++i) {
            if (mCore[i] != other.mCore[i]) return false;
        }
        return true;
    }

    /// Inequality comparison
    bool operator!=(const circular_buffer& other) const {
        return !(*this == other);
    }

    /// Lexicographic comparison
    bool operator<(const circular_buffer& other) const {
        fl::size min_size = (size() < other.size()) ? size() : other.size();
        for (fl::size i = 0; i < min_size; ++i) {
            if (mCore[i] < other.mCore[i]) return true;
            if (other.mCore[i] < mCore[i]) return false;
        }
        return size() < other.size();
    }

    /// Less-than-or-equal comparison
    bool operator<=(const circular_buffer& other) const {
        return *this < other || *this == other;
    }

    /// Greater-than comparison
    bool operator>(const circular_buffer& other) const {
        return other < *this;
    }

    /// Greater-than-or-equal comparison
    bool operator>=(const circular_buffer& other) const {
        return other <= *this;
    }

    void swap(circular_buffer& other) {
        fl_swap(mStorage, other.mStorage);
        fl_swap(mCore, other.mCore);
    }

  private:
    vector_inlined<T, (N > 0 ? N : 1)> mStorage;
    circular_buffer_core<T> mCore;
};

template <typename T, fl::size N>
void fl_swap(circular_buffer<T, N>& lhs, circular_buffer<T, N>& rhs) {
    lhs.swap(rhs);
}

} // namespace fl
