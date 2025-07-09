#pragma once

/*
LRU (Least Recently Used) HashMap that is optimized for embedded devices.
This hashmap has a maximum size and will automatically evict the least
recently used items when it reaches capacity.
*/

#include "fl/hash_map.h"
#include "fl/type_traits.h"

namespace fl {

template <typename Key, typename T, typename Hash = Hash<Key>,
          typename KeyEqual = EqualTo<Key>,
          int INLINED_COUNT = FASTLED_HASHMAP_INLINED_COUNT>
class HashMapLru {
  private:
    // Wrapper for values that includes access time tracking
    struct ValueWithTimestamp {
        T value;
        u32 last_access_time;

        ValueWithTimestamp() : last_access_time(0) {}
        ValueWithTimestamp(const T &v, u32 time)
            : value(v), last_access_time(time) {}
    };

  public:
    HashMapLru(fl::size max_size) : mMaxSize(max_size), mCurrentTime(0) {
        // Ensure max size is at least 1
        if (mMaxSize < 1)
            mMaxSize = 1;
    }

    void setMaxSize(fl::size max_size) {
        while (mMaxSize < max_size) {
            // Evict oldest items until we reach the new max size
            evictOldest();
        }
        mMaxSize = max_size;
    }

    void swap(HashMapLru &other) {
        fl::swap(mMap, other.mMap);
        fl::swap(mMaxSize, other.mMaxSize);
        fl::swap(mCurrentTime, other.mCurrentTime);
    }

    // Insert or update a key-value pair
    void insert(const Key &key, const T &value) {
        // Only evict if we're at capacity AND this is a new key
        const ValueWithTimestamp *existing = mMap.find_value(key);

        auto curr = mCurrentTime++;

        if (existing) {
            // Update the value and access time
            ValueWithTimestamp &vwt =
                const_cast<ValueWithTimestamp &>(*existing);
            vwt.value = value;
            vwt.last_access_time = curr;
            return;
        }
        if (mMap.size() >= mMaxSize) {
            evictOldest();
        }

        // Insert or update the value with current timestamp
        ValueWithTimestamp vwt(value, mCurrentTime);
        mMap.insert(key, vwt);
    }

    // Get value for key, returns nullptr if not found
    T *find_value(const Key &key) {
        ValueWithTimestamp *vwt = mMap.find_value(key);
        if (vwt) {
            // Update access time
            auto curr = mCurrentTime++;
            vwt->last_access_time = curr;
            return &vwt->value;
        }
        return nullptr;
    }

    // Get value for key, returns nullptr if not found (const version)
    const T *find_value(const Key &key) const {
        const ValueWithTimestamp *vwt = mMap.find_value(key);
        return vwt ? &vwt->value : nullptr;
    }

    // Access operator - creates entry if not exists
    T &operator[](const Key &key) {
        // If we're at capacity and this is a new key, evict oldest
        auto curr = mCurrentTime++;

        auto entry = mMap.find_value(key);
        if (entry) {
            // Update access time
            entry->last_access_time = curr;
            return entry->value;
        }

        if (mMap.size() >= mMaxSize) {
            evictOldest();
        }

        // Get or create entry and update timestamp
        // mCurrentTime++;
        ValueWithTimestamp &vwt = mMap[key];
        vwt.last_access_time = curr;
        return vwt.value;
    }

    // Remove a key
    bool remove(const Key &key) { return mMap.remove(key); }

    // Clear the map
    void clear() {
        mMap.clear();
        mCurrentTime = 0;
    }

    // Size accessors
    fl::size size() const { return mMap.size(); }
    bool empty() const { return mMap.empty(); }
    fl::size capacity() const { return mMaxSize; }

  private:
    // Evict the least recently used item
    void evictOldest() {
        if (mMap.empty())
            return;

        // Find the entry with the oldest timestamp
        Key oldest_key;
        u32 oldest_time = UINT32_MAX;
        bool found = false;

        for (auto it = mMap.begin(); it != mMap.end(); ++it) {
            const auto &entry = *it;
            const ValueWithTimestamp &vwt = entry.second;

            if (vwt.last_access_time < oldest_time) {
                oldest_time = vwt.last_access_time;
                oldest_key = entry.first;
                found = true;
            }
        }

        // Remove the oldest entry
        if (found) {
            mMap.remove(oldest_key);
        }
    }

    HashMap<Key, ValueWithTimestamp, Hash, KeyEqual, INLINED_COUNT> mMap;
    fl::size mMaxSize;
    u32 mCurrentTime;
};

} // namespace fl
