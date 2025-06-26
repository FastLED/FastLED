#pragma once

#include "fl/stdint.h"
#include <string.h> // for memcpy

#include "fl/math_macros.h"
#include "fl/compiler_control.h"

namespace fl {

/// A dynamic bitset implementation that can be resized at runtime
class bitset_dynamic {
  private:
    static constexpr uint32_t bits_per_block = 8 * sizeof(uint64_t);
    using block_type = uint64_t;

    block_type *_blocks = nullptr;
    uint32_t _block_count = 0;
    uint32_t _size = 0;

    // Helper to calculate block count from bit count
    static uint32_t calc_block_count(uint32_t bit_count) {
        return (bit_count + bits_per_block - 1) / bits_per_block;
    }

  public:
    // Default constructor
    bitset_dynamic() = default;

    // Constructor with initial size
    explicit bitset_dynamic(uint32_t size) { resize(size); }

    // Copy constructor
    bitset_dynamic(const bitset_dynamic &other) {
        if (other._size > 0) {
            resize(other._size);
            memcpy(_blocks, other._blocks, _block_count * sizeof(block_type));
        }
    }

    // Move constructor
    bitset_dynamic(bitset_dynamic &&other) noexcept
        : _blocks(other._blocks), _block_count(other._block_count),
          _size(other._size) {
        other._blocks = nullptr;
        other._block_count = 0;
        other._size = 0;
    }

    // Copy assignment
    bitset_dynamic &operator=(const bitset_dynamic &other) {
        if (this != &other) {
            if (other._size > 0) {
                resize(other._size);
                memcpy(_blocks, other._blocks,
                       _block_count * sizeof(block_type));
            } else {
                clear();
            }
        }
        return *this;
    }

    // Move assignment
    bitset_dynamic &operator=(bitset_dynamic &&other) noexcept {
        if (this != &other) {
            delete[] _blocks;
            _blocks = other._blocks;
            _block_count = other._block_count;
            _size = other._size;
            other._blocks = nullptr;
            other._block_count = 0;
            other._size = 0;
        }
        return *this;
    }

    // Destructor
    ~bitset_dynamic() { delete[] _blocks; }

    // Assign n bits to the value specified
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING(null-dereference)
    void assign(size_t n, bool value) {
        if (n > _size) {
            resize(n);
        }
        if (value) {
            // Set all bits to 1
            if (_blocks && _block_count > 0) {
                for (uint32_t i = 0; i < _block_count; ++i) {
                    _blocks[i] = ~static_cast<block_type>(0);
                }
                // Clear any bits beyond the actual size
                if (_size % bits_per_block != 0) {
                    uint32_t last_block_idx = (_size - 1) / bits_per_block;
                    uint32_t last_bit_pos = (_size - 1) % bits_per_block;
                    block_type mask = (static_cast<block_type>(1) << (last_bit_pos + 1)) - 1;
                    _blocks[last_block_idx] &= mask;
                }
            }
        } else {
            // Set all bits to 0
            reset();
        }
    }
    FL_DISABLE_WARNING_POP

    // Resize the bitset
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING(null-dereference)
    void resize(uint32_t new_size) {
        if (new_size == _size)
            return;

        uint32_t new_block_count = (new_size + bits_per_block - 1) / bits_per_block;

        if (new_block_count != _block_count) {
            block_type *new_blocks = new block_type[new_block_count];
            memset(new_blocks, 0, new_block_count * sizeof(block_type));

            if (_blocks) {
                uint32_t copy_blocks = MIN(_block_count, new_block_count);
                memcpy(new_blocks, _blocks, copy_blocks * sizeof(block_type));
            }

            delete[] _blocks;
            _blocks = new_blocks;
            _block_count = new_block_count;
        }

        _size = new_size;

        // Clear any bits beyond the new size
        if (_blocks && _block_count > 0 && _size % bits_per_block != 0) {
            uint32_t last_block_idx = (_size - 1) / bits_per_block;
            uint32_t last_bit_pos = (_size - 1) % bits_per_block;
            block_type mask =
                (static_cast<block_type>(1) << (last_bit_pos + 1)) - 1;
            _blocks[last_block_idx] &= mask;
        }
    }
    FL_DISABLE_WARNING_POP

    // Clear the bitset (reset to empty)
    void clear() {
        delete[] _blocks;
        _blocks = nullptr;
        _block_count = 0;
        _size = 0;
    }

    // Reset all bits to 0
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING(null-dereference)
    void reset() noexcept {
        if (_blocks && _block_count > 0) {
            memset(_blocks, 0, _block_count * sizeof(block_type));
        }
    }
    FL_DISABLE_WARNING_POP

    // Reset a specific bit to 0
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING(null-dereference)
    void reset(uint32_t pos) noexcept {
        if (_blocks && pos < _size) {
            const uint32_t idx = pos / bits_per_block;
            const uint32_t off = pos % bits_per_block;
            _blocks[idx] &= ~(static_cast<block_type>(1) << off);
        }
    }
    FL_DISABLE_WARNING_POP

    // Set a specific bit to 1
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING(null-dereference)
    void set(uint32_t pos) noexcept {
        if (_blocks && pos < _size) {
            const uint32_t idx = pos / bits_per_block;
            const uint32_t off = pos % bits_per_block;
            _blocks[idx] |= (static_cast<block_type>(1) << off);
        }
    }
    FL_DISABLE_WARNING_POP

    // Set a specific bit to a given value
    void set(uint32_t pos, bool value) noexcept {
        if (value) {
            set(pos);
        } else {
            reset(pos);
        }
    }

    // Flip a specific bit
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING(null-dereference)
    void flip(uint32_t pos) noexcept {
        if (_blocks && pos < _size) {
            const uint32_t idx = pos / bits_per_block;
            const uint32_t off = pos % bits_per_block;
            _blocks[idx] ^= (static_cast<block_type>(1) << off);
        }
    }
    FL_DISABLE_WARNING_POP

    // Flip all bits
    void flip() noexcept {
        if (!_blocks) return;
        
        for (uint32_t i = 0; i < _block_count; ++i) {
            _blocks[i] = ~_blocks[i];
        }

        // Clear any bits beyond size
        if (_block_count > 0 && _size % bits_per_block != 0) {
            uint32_t last_block_idx = (_size - 1) / bits_per_block;
            uint32_t last_bit_pos = (_size - 1) % bits_per_block;
            block_type mask =
                (static_cast<block_type>(1) << (last_bit_pos + 1)) - 1;
            _blocks[last_block_idx] &= mask;
        }
    }

    // Test if a bit is set
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING(null-dereference)
    bool test(uint32_t pos) const noexcept {
        if (_blocks && pos < _size) {
            const uint32_t idx = pos / bits_per_block;
            const uint32_t off = pos % bits_per_block;
            return (_blocks[idx] >> off) & 1;
        }
        return false;
    }
    FL_DISABLE_WARNING_POP

    // Count the number of set bits
    uint32_t count() const noexcept {
        if (!_blocks) return 0;
        
        uint32_t result = 0;
        for (uint32_t i = 0; i < _block_count; ++i) {
            block_type v = _blocks[i];
            // Brian Kernighan's algorithm for counting bits
            while (v) {
                v &= (v - 1);
                ++result;
            }
        }
        return result;
    }

    // Check if any bit is set
    bool any() const noexcept {
        if (!_blocks) return false;
        
        for (uint32_t i = 0; i < _block_count; ++i) {
            if (_blocks[i] != 0)
                return true;
        }
        return false;
    }

    // Check if no bit is set
    bool none() const noexcept { return !any(); }

    // Check if all bits are set
    bool all() const noexcept {
        if (_size == 0)
            return true;
        
        if (!_blocks) return false;

        for (uint32_t i = 0; i < _block_count - 1; ++i) {
            if (_blocks[i] != ~static_cast<block_type>(0))
                return false;
        }

        // Check last block with mask for valid bits
        if (_block_count > 0) {
            uint32_t last_bit_pos = (_size - 1) % bits_per_block;
            block_type mask =
                (static_cast<block_type>(1) << (last_bit_pos + 1)) - 1;
            return (_blocks[_block_count - 1] & mask) == mask;
        }

        return true;
    }

    // Get the size of the bitset
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING(null-dereference)
    uint32_t size() const noexcept { 
        // Note: _size is a member variable, not a pointer, so this should be safe
        // but we add this comment to clarify for static analysis
        return _size; 
    }
    FL_DISABLE_WARNING_POP

    // Access operator
    bool operator[](uint32_t pos) const noexcept { return test(pos); }

    // Bitwise AND operator
    bitset_dynamic operator&(const bitset_dynamic &other) const {
        bitset_dynamic result(_size);
        
        if (!_blocks || !other._blocks || !result._blocks) {
            return result;
        }
        
        uint32_t min_blocks = MIN(_block_count, other._block_count);

        for (uint32_t i = 0; i < min_blocks; ++i) {
            result._blocks[i] = _blocks[i] & other._blocks[i];
        }

        return result;
    }

    // Bitwise OR operator
    bitset_dynamic operator|(const bitset_dynamic &other) const {
        bitset_dynamic result(_size);
        
        if (!_blocks || !other._blocks || !result._blocks) {
            return result;
        }
        
        uint32_t min_blocks = MIN(_block_count, other._block_count);

        for (uint32_t i = 0; i < min_blocks; ++i) {
            result._blocks[i] = _blocks[i] | other._blocks[i];
        }

        // Copy remaining blocks from the larger bitset
        if (_block_count > min_blocks) {
            memcpy(result._blocks + min_blocks, _blocks + min_blocks,
                   (_block_count - min_blocks) * sizeof(block_type));
        }

        return result;
    }

    // Bitwise XOR operator
    bitset_dynamic operator^(const bitset_dynamic &other) const {
        bitset_dynamic result(_size);
        
        if (!_blocks || !other._blocks || !result._blocks) {
            return result;
        }
        
        uint32_t min_blocks = MIN(_block_count, other._block_count);

        for (uint32_t i = 0; i < min_blocks; ++i) {
            result._blocks[i] = _blocks[i] ^ other._blocks[i];
        }

        // Copy remaining blocks from the larger bitset
        if (_block_count > min_blocks) {
            memcpy(result._blocks + min_blocks, _blocks + min_blocks,
                   (_block_count - min_blocks) * sizeof(block_type));
        }

        return result;
    }

    // Bitwise NOT operator
    bitset_dynamic operator~() const {
        bitset_dynamic result(_size);
        
        if (!_blocks || !result._blocks) {
            return result;
        }

        for (uint32_t i = 0; i < _block_count; ++i) {
            result._blocks[i] = ~_blocks[i];
        }

        // Clear any bits beyond size
        if (_block_count > 0 && _size % bits_per_block != 0) {
            uint32_t last_block_idx = (_size - 1) / bits_per_block;
            uint32_t last_bit_pos = (_size - 1) % bits_per_block;
            block_type mask =
                (static_cast<block_type>(1) << (last_bit_pos + 1)) - 1;
            result._blocks[last_block_idx] &= mask;
        }

        return result;
    }
};

} // namespace fl
