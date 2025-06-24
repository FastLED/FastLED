#pragma once

#include "fl/move.h"

namespace fl {

template <typename Key, typename Value> struct Pair {
    Key first = Key();
    Value second = Value();
    Pair() = default;
    Pair(const Key &k, const Value &v) : first(k), second(v) {}
    
    // Rule of 5: copy constructor, copy assignment, move constructor, move assignment, destructor
    Pair(const Pair &other) = default;
    Pair &operator=(const Pair &other) = default;
    Pair(Pair &&other) noexcept : first(fl::move(other.first)), second(fl::move(other.second)) {}
    Pair &operator=(Pair &&other) noexcept {
        if (this != &other) {
            first = fl::move(other.first);
            second = fl::move(other.second);
        }
        return *this;
    }
};

// std compatibility
template <typename Key, typename Value> using pair = Pair<Key, Value>;

} // namespace fl
