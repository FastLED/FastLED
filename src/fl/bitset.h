#pragma once

#include "fl/bitset_dynamic.h"
#include "fl/type_traits.h"
#include "fl/variant.h"
#include "fl/stdint.h"
#include "fl/int.h"

namespace fl {

template <fl::u32 N> class BitsetInlined;

template <fl::u32 N> class BitsetFixed;

template <fl::u32 N = 256>
using bitset = BitsetInlined<N>; // inlined but can go bigger.

template <fl::u32 N>
using bitset_fixed = BitsetFixed<N>; // fixed size, no dynamic allocation.

/// A simple fixed-size Bitset implementation similar to std::Bitset.
template <fl::u32 N> class BitsetFixed {
  private:
    static constexpr fl::u32 bits_per_block = 8 * sizeof(fl::u64);
    static constexpr fl::u32 block_count =
        (N + bits_per_block - 1) / bits_per_block;
    using block_type = fl::u64;

    // Underlying blocks storing bits
    block_type _blocks[block_count];

  public:
    struct Proxy {
        BitsetFixed &_bitset;
        fl::u32 _pos;

        Proxy(BitsetFixed &bitset, fl::u32 pos) : _bitset(bitset), _pos(pos) {}

        Proxy &operator=(bool value) {
            _bitset.set(_pos, value);
            return *this;
        }

        operator bool() const { return _bitset.test(_pos); }
    };

    Proxy operator[](fl::u32 pos) { return Proxy(*this, pos); }

    /// Constructs a BitsetFixed with all bits reset.
    constexpr BitsetFixed() noexcept : _blocks{} {}

    /// Resets all bits to zero.
    void reset() noexcept {
        for (fl::u32 i = 0; i < block_count; ++i) {
            _blocks[i] = 0;
        }
    }

    /// Sets or clears the bit at position pos.
    BitsetFixed &set(fl::u32 pos, bool value = true) {
        if (pos < N) {
            const fl::u32 idx = pos / bits_per_block;
            const fl::u32 off = pos % bits_per_block;
            if (value) {
                _blocks[idx] |= (block_type(1) << off);
            } else {
                _blocks[idx] &= ~(block_type(1) << off);
            }
        }
        return *this;
    }

    void assign(fl::size n, bool value) {
        if (n > N) {
            n = N;
        }
        for (fl::size i = 0; i < n; ++i) {
            set(i, value);
        }
    }

    /// Clears the bit at position pos.
    BitsetFixed &reset(fl::u32 pos) { return set(pos, false); }

    /// Flips (toggles) the bit at position pos.
    BitsetFixed &flip(fl::u32 pos) {
        if (pos < N) {
            const fl::u32 idx = pos / bits_per_block;
            const fl::u32 off = pos % bits_per_block;
            _blocks[idx] ^= (block_type(1) << off);
        }
        return *this;
    }

    /// Flips all bits.
    BitsetFixed &flip() noexcept {
        for (fl::u32 i = 0; i < block_count; ++i) {
            _blocks[i] = ~_blocks[i];
        }
        // Mask out unused high bits in the last block
        if (N % bits_per_block != 0) {
            const fl::u32 extra = bits_per_block - (N % bits_per_block);
            _blocks[block_count - 1] &= (~block_type(0) >> extra);
        }
        return *this;
    }

    /// Tests whether the bit at position pos is set.
    bool test(fl::u32 pos) const noexcept {
        if (pos < N) {
            const fl::u32 idx = pos / bits_per_block;
            const fl::u32 off = pos % bits_per_block;
            return (_blocks[idx] >> off) & 1;
        }
        return false;
    }

    /// Returns the value of the bit at position pos.
    bool operator[](fl::u32 pos) const noexcept { return test(pos); }

    /// Returns the number of set bits.
    fl::u32 count() const noexcept {
        fl::u32 cnt = 0;
        // Count bits in all complete blocks
        for (fl::u32 i = 0; i < block_count - 1; ++i) {
            cnt += __builtin_popcountll(_blocks[i]);
        }

        // For the last block, we need to be careful about counting only valid
        // bits
        if (block_count > 0) {
            block_type last_block = _blocks[block_count - 1];
            // If N is not a multiple of bits_per_block, mask out the unused
            // bits
            if (N % bits_per_block != 0) {
                const fl::u32 valid_bits = N % bits_per_block;
                // Create a mask with only the valid bits set to 1
                block_type mask = (valid_bits == bits_per_block)
                                      ? ~block_type(0)
                                      : ((block_type(1) << valid_bits) - 1);
                last_block &= mask;
            }
            cnt += __builtin_popcountll(last_block);
        }

        return cnt;
    }

    /// Queries.
    bool any() const noexcept { return count() > 0; }
    bool none() const noexcept { return count() == 0; }
    bool all() const noexcept {
        if (N == 0)
            return true;

        // Check all complete blocks
        for (fl::u32 i = 0; i < block_count - 1; ++i) {
            if (_blocks[i] != ~block_type(0)) {
                return false;
            }
        }

        // Check the last block
        if (block_count > 0) {
            block_type mask;
            if (N % bits_per_block != 0) {
                // Create a mask for the valid bits in the last block
                mask = (block_type(1) << (N % bits_per_block)) - 1;
            } else {
                mask = ~block_type(0);
            }

            if ((_blocks[block_count - 1] & mask) != mask) {
                return false;
            }
        }

        return true;
    }

    /// Bitwise AND
    BitsetFixed &operator&=(const BitsetFixed &other) noexcept {
        for (fl::u32 i = 0; i < block_count; ++i) {
            _blocks[i] &= other._blocks[i];
        }
        return *this;
    }
    /// Bitwise OR
    BitsetFixed &operator|=(const BitsetFixed &other) noexcept {
        for (fl::u32 i = 0; i < block_count; ++i) {
            _blocks[i] |= other._blocks[i];
        }
        return *this;
    }
    /// Bitwise XOR
    BitsetFixed &operator^=(const BitsetFixed &other) noexcept {
        for (fl::u32 i = 0; i < block_count; ++i) {
            _blocks[i] ^= other._blocks[i];
        }
        return *this;
    }

    /// Size of the BitsetFixed (number of bits).
    constexpr fl::u32 size() const noexcept { return N; }

    /// Finds the first bit that matches the test value.
    /// Returns the index of the first matching bit, or -1 if none found.
    fl::i32 find_first(bool test_value) const noexcept {
        // If looking for true bits, we need to find the first set bit
        // If looking for false bits, we need to find the first unset bit
        
        for (fl::u32 block_idx = 0; block_idx < block_count; ++block_idx) {
            block_type current_block = _blocks[block_idx];
            
            // For the last block, we need to mask out unused bits
            if (block_idx == block_count - 1 && N % bits_per_block != 0) {
                const fl::u32 valid_bits = N % bits_per_block;
                block_type mask = (valid_bits == bits_per_block) 
                    ? ~block_type(0) 
                    : ((block_type(1) << valid_bits) - 1);
                current_block &= mask;
            }
            
            // If looking for false bits, invert the block
            if (!test_value) {
                current_block = ~current_block;
            }
            
            // If this block has any matching bits, find the first one
            if (current_block != 0) {
                // Step 1: Test entire u64 block
                if (current_block != 0) {
                    // Step 2: Use a union to treat as u16[4]
                    union { block_type u64; fl::u16 u16[4]; } u16_union = { current_block };
                    for (fl::u32 u16_idx = 0; u16_idx < 4; ++u16_idx) {
                        if (u16_union.u16[u16_idx] != 0) {
                            // Step 3: Use a union to treat as u8[2]
                            union { fl::u16 u16; fl::u8 u8[2]; } u8_union = { u16_union.u16[u16_idx] };
                            for (fl::u32 u8_idx = 0; u8_idx < 2; ++u8_idx) {
                                if (u8_union.u8[u8_idx] != 0) {
                                    // Step 4: Test each bit to find exact index
                                    fl::u8 byte = u8_union.u8[u8_idx];
                                    for (fl::u32 bit_idx = 0; bit_idx < 8; ++bit_idx) {
                                        if (byte & (1 << bit_idx)) {
                                            fl::u32 global_bit_idx = block_idx * bits_per_block + 
                                                                     u16_idx * 16 + u8_idx * 8 + bit_idx;
                                            // Check if this bit is within our valid range
                                            if (global_bit_idx < N) {
                                                return static_cast<fl::i32>(global_bit_idx);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        return -1; // No matching bit found
    }

    /// Friend operators for convenience.
    friend BitsetFixed operator&(BitsetFixed lhs,
                                 const BitsetFixed &rhs) noexcept {
        return lhs &= rhs;
    }
    friend BitsetFixed operator|(BitsetFixed lhs,
                                 const BitsetFixed &rhs) noexcept {
        return lhs |= rhs;
    }
    friend BitsetFixed operator^(BitsetFixed lhs,
                                 const BitsetFixed &rhs) noexcept {
        return lhs ^= rhs;
    }
    friend BitsetFixed operator~(BitsetFixed bs) noexcept { return bs.flip(); }
};

/// A Bitset implementation with inline storage that can grow if needed.
/// T is the storage type (u8, u16, u32, uint64_t)
/// N is the initial number of bits to store inline
template <fl::u32 N = 256> // Default size is 256 bits, or 32 bytes
class BitsetInlined {
  private:
    // Either store a fixed Bitset<N> or a dynamic Bitset
    using fixed_bitset = BitsetFixed<N>;
    Variant<fixed_bitset, bitset_dynamic> _storage;

  public:
    struct Proxy {
        BitsetInlined &_bitset;
        fl::u32 _pos;

        Proxy(BitsetInlined &bitset, fl::u32 pos)
            : _bitset(bitset), _pos(pos) {}

        Proxy &operator=(bool value) {
            _bitset.set(_pos, value);
            return *this;
        }

        operator bool() const { return _bitset.test(_pos); }
    };

    Proxy operator[](fl::u32 pos) { return Proxy(*this, pos); }

    /// Constructs a Bitset with all bits reset.
    BitsetInlined() : _storage(fixed_bitset()) {}
    BitsetInlined(fl::size size) : _storage(fixed_bitset()) {
        if (size > N) {
            _storage = bitset_dynamic(size);
        }
    }
    BitsetInlined(const BitsetInlined &other) : _storage(other._storage) {}
    BitsetInlined(BitsetInlined &&other) noexcept
        : _storage(fl::move(other._storage)) {}
    BitsetInlined &operator=(const BitsetInlined &other) {
        if (this != &other) {
            _storage = other._storage;
        }
        return *this;
    }
    BitsetInlined &operator=(BitsetInlined &&other) noexcept {
        if (this != &other) {
            _storage = fl::move(other._storage);
        }
        return *this;
    }

    /// Resets all bits to zero.
    void reset() noexcept {
        if (_storage.template is<fixed_bitset>()) {
            _storage.template ptr<fixed_bitset>()->reset();
        } else {
            _storage.template ptr<bitset_dynamic>()->reset();
        }
    }

    void assign(fl::size n, bool value) {
        resize(n);
        if (_storage.template is<fixed_bitset>()) {
            _storage.template ptr<fixed_bitset>()->assign(n, value);
        } else {
            _storage.template ptr<bitset_dynamic>()->assign(n, value);
        }
    }

    /// Resizes the Bitset if needed
    void resize(fl::u32 new_size) {
        if (new_size <= N) {
            // If we're already using the fixed Bitset, nothing to do
            if (_storage.template is<bitset_dynamic>()) {
                // Convert back to fixed Bitset
                fixed_bitset fixed;
                bitset_dynamic *dynamic =
                    _storage.template ptr<bitset_dynamic>();

                // Copy bits from dynamic to fixed
                for (fl::u32 i = 0; i < N && i < dynamic->size(); ++i) {
                    if (dynamic->test(i)) {
                        fixed.set(i);
                    }
                }

                _storage = fixed;
            }
        } else {
            // Need to use dynamic Bitset
            if (_storage.template is<fixed_bitset>()) {
                // Convert from fixed to dynamic
                bitset_dynamic dynamic(new_size);
                fixed_bitset *fixed = _storage.template ptr<fixed_bitset>();

                // Copy bits from fixed to dynamic
                for (fl::u32 i = 0; i < N; ++i) {
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
    BitsetInlined &set(fl::u32 pos, bool value = true) {
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
    BitsetInlined &reset(fl::u32 pos) { return set(pos, false); }

    /// Flips (toggles) the bit at position pos.
    BitsetInlined &flip(fl::u32 pos) {
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
    BitsetInlined &flip() noexcept {
        if (_storage.template is<fixed_bitset>()) {
            _storage.template ptr<fixed_bitset>()->flip();
        } else {
            _storage.template ptr<bitset_dynamic>()->flip();
        }
        return *this;
    }

    /// Tests whether the bit at position pos is set.
    bool test(fl::u32 pos) const noexcept {
        if (_storage.template is<fixed_bitset>()) {
            return pos < N ? _storage.template ptr<fixed_bitset>()->test(pos)
                           : false;
        } else {
            return _storage.template ptr<bitset_dynamic>()->test(pos);
        }
    }

    /// Returns the value of the bit at position pos.
    bool operator[](fl::u32 pos) const noexcept { return test(pos); }

    /// Returns the number of set bits.
    fl::u32 count() const noexcept {
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

    /// Size of the Bitset (number of bits).
    fl::u32 size() const noexcept {
        if (_storage.template is<fixed_bitset>()) {
            return N;
        } else {
            return _storage.template ptr<bitset_dynamic>()->size();
        }
    }

    /// Bitwise operators
    friend BitsetInlined operator~(const BitsetInlined &bs) noexcept {
        BitsetInlined result = bs;
        result.flip();
        return result;
    }

    friend BitsetInlined operator&(const BitsetInlined &lhs,
                                   const BitsetInlined &rhs) noexcept {
        BitsetInlined result = lhs;

        if (result._storage.template is<fixed_bitset>() &&
            rhs._storage.template is<fixed_bitset>()) {
            // Both are fixed, use the fixed implementation
            *result._storage.template ptr<fixed_bitset>() &=
                *rhs._storage.template ptr<fixed_bitset>();
        } else {
            // At least one is dynamic, handle bit by bit
            fl::u32 min_size =
                result.size() < rhs.size() ? result.size() : rhs.size();
            for (fl::u32 i = 0; i < min_size; ++i) {
                result.set(i, result.test(i) && rhs.test(i));
            }
            // Clear any bits beyond the size of rhs
            for (fl::u32 i = min_size; i < result.size(); ++i) {
                result.reset(i);
            }
        }

        return result;
    }

    friend BitsetInlined operator|(const BitsetInlined &lhs,
                                   const BitsetInlined &rhs) noexcept {
        BitsetInlined result = lhs;

        if (result._storage.template is<fixed_bitset>() &&
            rhs._storage.template is<fixed_bitset>()) {
            // Both are fixed, use the fixed implementation
            *result._storage.template ptr<fixed_bitset>() |=
                *rhs._storage.template ptr<fixed_bitset>();
        } else {
            // At least one is dynamic, handle bit by bit
            fl::u32 max_size =
                result.size() > rhs.size() ? result.size() : rhs.size();

            // Resize if needed
            if (result.size() < max_size) {
                result.resize(max_size);
            }

            // Set bits from rhs
            for (fl::u32 i = 0; i < rhs.size(); ++i) {
                if (rhs.test(i)) {
                    result.set(i);
                }
            }
        }

        return result;
    }

    friend BitsetInlined operator^(const BitsetInlined &lhs,
                                   const BitsetInlined &rhs) noexcept {
        BitsetInlined result = lhs;

        if (result._storage.template is<fixed_bitset>() &&
            rhs._storage.template is<fixed_bitset>()) {
            // Both are fixed, use the fixed implementation
            *result._storage.template ptr<fixed_bitset>() ^=
                *rhs._storage.template ptr<fixed_bitset>();
        } else {
            // At least one is dynamic, handle bit by bit
            fl::u32 max_size =
                result.size() > rhs.size() ? result.size() : rhs.size();

            // Resize if needed
            if (result.size() < max_size) {
                result.resize(max_size);
            }

            // XOR bits from rhs
            for (fl::u32 i = 0; i < rhs.size(); ++i) {
                result.set(i, result.test(i) != rhs.test(i));
            }
        }

        return result;
    }
};

} // namespace fl
