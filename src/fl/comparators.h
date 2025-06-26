#pragma once

namespace fl {

template <typename T> struct DefaultLess {
    bool operator()(const T &a, const T &b) const { return a < b; }
};

} // namespace fl
