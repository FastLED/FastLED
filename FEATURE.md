#include <variant>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <iostream>

namespace fl {

using StoreVariant = std::variant<
    std::vector<uint8_t>,
    std::vector<int16_t>,
    std::vector<float>,
    std::vector<int32_t>
>;

template <typename OutT>
struct Promote {
    OutT operator()(uint8_t val) const { return static_cast<OutT>(val); }
    OutT operator()(int16_t val) const { return static_cast<OutT>(val); }
    OutT operator()(float val)   const { return static_cast<OutT>(val); }
    OutT operator()(int32_t val) const { return static_cast<OutT>(val); }
};

class MultiArray {
    std::vector<StoreVariant> store;

public:
    void push_back(StoreVariant v) {
        store.push_back(std::move(v));
    }

    template <typename T>
    class iterator {
        using OuterIter = typename std::vector<StoreVariant>::iterator;

        OuterIter outer, outer_end;
        size_t inner_index = 0;

        void advance_to_next_valid() {
            while (outer != outer_end) {
                if (std::visit([](auto&& vec) {
                    using VecT = std::decay_t<decltype(vec)>;
                    using ElemT = typename VecT::value_type;
                    return std::is_convertible_v<ElemT, T>;
                }, *outer)) {
                    if (get_vector_size() > 0) return;
                }
                ++outer;
                inner_index = 0;
            }
        }

        size_t get_vector_size() const {
            return std::visit([](auto&& vec) {
                return vec.size();
            }, *outer);
        }

    public:
        iterator(OuterIter begin, OuterIter end)
            : outer(begin), outer_end(end) {
            advance_to_next_valid();
        }

        T operator*() const {
            return std::visit([this](auto&& vec) -> T {
                using ElemT = typename std::decay_t<decltype(vec)>::value_type;
                return Promote<T>{}(vec[inner_index]);
            }, *outer);
        }

        iterator& operator++() {
            ++inner_index;
            if (inner_index >= get_vector_size()) {
                ++outer;
                inner_index = 0;
                advance_to_next_valid();
            }
            return *this;
        }

        bool operator!=(const iterator& other) const {
            return outer != other.outer || inner_index != other.inner_index;
        }
    };

    template <typename T>
    iterator<T> begin() {
        return iterator<T>(store.begin(), store.end());
    }

    template <typename T>
    iterator<T> end() {
        return iterator<T>(store.end(), store.end());
    }
};

} // namespace fl
