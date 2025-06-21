#pragma once

#include "fl/move.h"

namespace fl {

template <typename Key, typename Value> struct Pair {
    Key first = Key();
    Value second = Value();
    Pair() = default;
    Pair(const Key &k, const Value &v) : first(k), second(v) {}
    
    // Move constructor
    Pair(Pair &&other) noexcept : first(fl::move(other.first)), second(fl::move(other.second)) {}
    
    // Move assignment operator
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
