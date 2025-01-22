#pragma once

namespace fl {

template<typename Key, typename Value>
struct Pair {
    Key first = Key();
    Value second = Value();
    Pair(const Key& k, const Value& v) : first(k), second(v) {}
};

}  // namespace fl
