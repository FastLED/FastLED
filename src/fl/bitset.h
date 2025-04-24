#pragma once

#include <stdint.h>
#include "fl/type_traits.h"

namespace fl {

/// A simple fixed-size bitset implementation similar to std::bitset.
template <uint32_t N>
class bitset {
private:
    static constexpr uint32_t bits_per_block = 8 * sizeof(uint64_t);
    static constexpr uint32_t block_count = (N + bits_per_block - 1) / bits_per_block;
    using block_type = uint64_t;

    // Underlying blocks storing bits
    block_type _blocks[block_count];

public:
    /// Constructs a bitset with all bits reset.
    constexpr bitset() noexcept : _blocks{} {}

    /// Resets all bits to zero.
    void reset() noexcept {
        for (uint32_t i = 0; i < block_count; ++i) {
            _blocks[i] = 0;
        }
    }

    /// Sets or clears the bit at position pos.
    bitset& set(uint32_t pos, bool value = true) {
        if (pos < N) {
            const uint32_t idx = pos / bits_per_block;
            const uint32_t off = pos % bits_per_block;
            if (value) {
                _blocks[idx] |= (block_type(1) << off);
            } else {
                _blocks[idx] &= ~(block_type(1) << off);
            }
        }
        return *this;
    }

    /// Clears the bit at position pos.
    bitset& reset(uint32_t pos) {
        return set(pos, false);
    }

    /// Flips (toggles) the bit at position pos.
    bitset& flip(uint32_t pos) {
        if (pos < N) {
            const uint32_t idx = pos / bits_per_block;
            const uint32_t off = pos % bits_per_block;
            _blocks[idx] ^= (block_type(1) << off);
        }
        return *this;
    }

    /// Flips all bits.
    bitset& flip() noexcept {
        for (uint32_t i = 0; i < block_count; ++i) {
            _blocks[i] = ~_blocks[i];
        }
        // Mask out unused high bits in the last block
        if constexpr (N % bits_per_block) {
            const uint32_t extra = bits_per_block - (N % bits_per_block);
            _blocks[block_count - 1] &= (~block_type(0) >> extra);
        }
        return *this;
    }

    /// Tests whether the bit at position pos is set.
    bool test(uint32_t pos) const noexcept {
        if (pos < N) {
            const uint32_t idx = pos / bits_per_block;
            const uint32_t off = pos % bits_per_block;
            return (_blocks[idx] >> off) & 1;
        }
        return false;
    }

    /// Returns the value of the bit at position pos.
    bool operator[](uint32_t pos) const noexcept {
        return test(pos);
    }

    /// Returns the number of set bits.
    uint32_t count() const noexcept {
        uint32_t cnt = 0;
        for (uint32_t i = 0; i < block_count; ++i) {
            cnt += __builtin_popcountll(_blocks[i]);
        }
        return cnt;
    }

    /// Queries.
    bool any() const noexcept { return count() > 0; }
    bool none() const noexcept { return count() == 0; }
    bool all() const noexcept { return count() == N; }

    /// Bitwise AND
    bitset& operator&=(const bitset& other) noexcept {
        for (uint32_t i = 0; i < block_count; ++i) {
            _blocks[i] &= other._blocks[i];
        }
        return *this;
    }
    /// Bitwise OR
    bitset& operator|=(const bitset& other) noexcept {
        for (uint32_t i = 0; i < block_count; ++i) {
            _blocks[i] |= other._blocks[i];
        }
        return *this;
    }
    /// Bitwise XOR
    bitset& operator^=(const bitset& other) noexcept {
        for (uint32_t i = 0; i < block_count; ++i) {
            _blocks[i] ^= other._blocks[i];
        }
        return *this;
    }

    /// Size of the bitset (number of bits).
    constexpr uint32_t size() const noexcept { return N; }

    /// Friend operators for convenience.
    friend bitset operator&(bitset lhs, const bitset& rhs) noexcept { return lhs &= rhs; }
    friend bitset operator|(bitset lhs, const bitset& rhs) noexcept { return lhs |= rhs; }
    friend bitset operator^(bitset lhs, const bitset& rhs) noexcept { return lhs ^= rhs; }
    friend bitset operator~(bitset bs) noexcept { return bs.flip(); }
};

} // namespace fl
