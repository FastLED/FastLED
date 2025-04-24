#pragma once

#include <stdint.h>
#include "fl/type_traits.h"
#include "fl/variant.h"
#include "fl/bitset_dynamic.h"

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

/// A bitset implementation with inline storage that can grow if needed.
/// T is the storage type (uint8_t, uint16_t, uint32_t, uint64_t)
/// N is the initial number of bits to store inline
template <uint32_t N = 256>  // Default size is 256 bits, or 32 bytes
class bitset_inlined {
private:    
    // Either store a fixed bitset<N> or a dynamic bitset
    using fixed_bitset = bitset<N>;
    Variant<fixed_bitset, bitset_dynamic> _storage;

public:
    /// Constructs a bitset with all bits reset.
    bitset_inlined() : _storage(fixed_bitset()) {}

    /// Resets all bits to zero.
    void reset() noexcept {
        if (_storage.template is<fixed_bitset>()) {
            _storage.template ptr<fixed_bitset>()->reset();
        } else {
            _storage.template ptr<bitset_dynamic>()->reset();
        }
    }

    /// Resizes the bitset if needed
    void resize(uint32_t new_size) {
        if (new_size <= N) {
            // If we're already using the fixed bitset, nothing to do
            if (_storage.template is<bitset_dynamic>()) {
                // Convert back to fixed bitset
                fixed_bitset fixed;
                bitset_dynamic* dynamic = _storage.template ptr<bitset_dynamic>();
                
                // Copy bits from dynamic to fixed
                for (uint32_t i = 0; i < N && i < dynamic->size(); ++i) {
                    if (dynamic->test(i)) {
                        fixed.set(i);
                    }
                }
                
                _storage = fixed;
            }
        } else {
            // Need to use dynamic bitset
            if (_storage.template is<fixed_bitset>()) {
                // Convert from fixed to dynamic
                bitset_dynamic dynamic(new_size);
                fixed_bitset* fixed = _storage.template ptr<fixed_bitset>();
                
                // Copy bits from fixed to dynamic
                for (uint32_t i = 0; i < N; ++i) {
                    if (fixed->test(i)) {
                        dynamic.set(i);
                    }
                }
                
                _storage = dynamic;
            } else {
                // Already using dynamic, just resize
                _storage.template ptr<bitset_dynamic>()->resize(new_size);
            }
        }
    }

    /// Sets or clears the bit at position pos.
    bitset_inlined& set(uint32_t pos, bool value = true) {
        if (pos >= N && _storage.template is<fixed_bitset>()) {
            resize(pos + 1);
        }
        
        if (_storage.template is<fixed_bitset>()) {
            if (pos < N) {
                _storage.template ptr<fixed_bitset>()->set(pos, value);
            }
        } else {
            if (pos >= _storage.template ptr<bitset_dynamic>()->size()) {
                _storage.template ptr<bitset_dynamic>()->resize(pos + 1);
            }
            _storage.template ptr<bitset_dynamic>()->set(pos, value);
        }
        return *this;
    }

    /// Clears the bit at position pos.
    bitset_inlined& reset(uint32_t pos) {
        return set(pos, false);
    }

    /// Flips (toggles) the bit at position pos.
    bitset_inlined& flip(uint32_t pos) {
        if (pos >= N && _storage.template is<fixed_bitset>()) {
            resize(pos + 1);
        }
        
        if (_storage.template is<fixed_bitset>()) {
            if (pos < N) {
                _storage.template ptr<fixed_bitset>()->flip(pos);
            }
        } else {
            if (pos >= _storage.template ptr<bitset_dynamic>()->size()) {
                _storage.template ptr<bitset_dynamic>()->resize(pos + 1);
            }
            _storage.template ptr<bitset_dynamic>()->flip(pos);
        }
        return *this;
    }

    /// Flips all bits.
    bitset_inlined& flip() noexcept {
        if (_storage.template is<fixed_bitset>()) {
            _storage.template ptr<fixed_bitset>()->flip();
        } else {
            _storage.template ptr<bitset_dynamic>()->flip();
        }
        return *this;
    }

    /// Tests whether the bit at position pos is set.
    bool test(uint32_t pos) const noexcept {
        if (_storage.template is<fixed_bitset>()) {
            return pos < N ? _storage.template ptr<fixed_bitset>()->test(pos) : false;
        } else {
            return _storage.template ptr<bitset_dynamic>()->test(pos);
        }
    }

    /// Returns the value of the bit at position pos.
    bool operator[](uint32_t pos) const noexcept {
        return test(pos);
    }

    /// Returns the number of set bits.
    uint32_t count() const noexcept {
        if (_storage.template is<fixed_bitset>()) {
            return _storage.template ptr<fixed_bitset>()->count();
        } else {
            return _storage.template ptr<bitset_dynamic>()->count();
        }
    }

    /// Queries.
    bool any() const noexcept {
        if (_storage.template is<fixed_bitset>()) {
            return _storage.template ptr<fixed_bitset>()->any();
        } else {
            return _storage.template ptr<bitset_dynamic>()->any();
        }
    }
    
    bool none() const noexcept {
        if (_storage.template is<fixed_bitset>()) {
            return _storage.template ptr<fixed_bitset>()->none();
        } else {
            return _storage.template ptr<bitset_dynamic>()->none();
        }
    }
    
    bool all() const noexcept {
        if (_storage.template is<fixed_bitset>()) {
            return _storage.template ptr<fixed_bitset>()->all();
        } else {
            return _storage.template ptr<bitset_dynamic>()->all();
        }
    }

    /// Size of the bitset (number of bits).
    uint32_t size() const noexcept {
        if (_storage.template is<fixed_bitset>()) {
            return N;
        } else {
            return _storage.template ptr<bitset_dynamic>()->size();
        }
    }
};

} // namespace fl
