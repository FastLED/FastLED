
#pragma once

#include "fl/hash_map.h"

namespace fl {

// just define a hashset to be a hashmap with a dummy value

template <typename Key, typename Hash = Hash<Key>,
          typename KeyEqual = EqualTo<Key>>
class HashSet : public HashMap<Key, bool, Hash, KeyEqual> {
  public:
    using Base = HashMap<Key, bool, Hash, KeyEqual>;
    using iterator = typename Base::iterator;
    using const_iterator = typename Base::const_iterator;

    HashSet(fl::size initial_capacity = 8, float max_load = 0.7f)
        : Base(initial_capacity, max_load) {}

    void insert(const Key &key) { Base::insert(key, true); }

    void erase(const Key &key) { Base::erase(key); }

    iterator find(const Key &key) { return Base::find(key); }
};

template <typename Key, typename Hash = Hash<Key>,
          typename KeyEqual = EqualTo<Key>>
using hash_set = HashSet<Key, Hash, KeyEqual>;

} // namespace fl
