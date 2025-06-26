#pragma once

#include "fl/move.h"
#include "fl/compiler_control.h"

namespace fl {

template <typename Key, typename Value> struct Pair {
    Key first = Key();
    Value second = Value();
    Pair() = default;
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING(null-dereference)
    Pair(const Key &k, const Value &v) : first(k), second(v) {}
    FL_DISABLE_WARNING_POP
    
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
