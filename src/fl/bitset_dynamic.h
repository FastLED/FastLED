#pragma once

#include "fl/stdint.h"
#include "fl/int.h"
#include <string.h> // for memcpy

#include "fl/math_macros.h"
#include "fl/memfill.h"
#include "fl/compiler_control.h"

namespace fl {

class string;

namespace detail {
void to_string(const fl::u16 *bit_data, fl::u32 bit_count, string* dst);
}

/// A dynamic bitset implementation that can be resized at runtime
class bitset_dynamic {
  private:
    static constexpr fl::u32 bits_per_block = 8 * sizeof(fl::u16);
    using block_type = fl::u16;

    block_type *_blocks = nullptr;
    fl::u32 _block_count = 0;
    fl::u32 _size = 0;

    // Helper to calculate block count from bit count
    static fl::u32 calc_block_count(fl::u32 bit_count) {
        return (bit_count + bits_per_block - 1) / bits_per_block;
    }

  public:
    // Default constructor
    bitset_dynamic() = default;

    // Constructor with initial size
    explicit bitset_dynamic(fl::u32 size) { resize(size); }

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
    FL_DISABLE_WARNING_NULL_DEREFERENCE
    void assign(fl::u32 n, bool value) {
        if (n > _size) {
            resize(n);
        }
        if (value) {
            // Set all bits to 1
            if (_blocks && _block_count > 0) {
                for (fl::u32 i = 0; i < _block_count; ++i) {
                    _blocks[i] = static_cast<block_type>(~block_type(0));
                }
                // Clear any bits beyond the actual size
                if (_size % bits_per_block != 0) {
                    fl::u32 last_block_idx = (_size - 1) / bits_per_block;
                    fl::u32 last_bit_pos = (_size - 1) % bits_per_block;
                    block_type mask = static_cast<block_type>((static_cast<block_type>(1) << (last_bit_pos + 1)) - 1);
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
    FL_DISABLE_WARNING_NULL_DEREFERENCE
    void resize(fl::u32 new_size) {
        if (new_size == _size)
            return;

        fl::u32 new_block_count = (new_size + bits_per_block - 1) / bits_per_block;

        if (new_block_count != _block_count) {
            block_type *new_blocks = new block_type[new_block_count];
            fl::memfill(new_blocks, 0, new_block_count * sizeof(block_type));

            if (_blocks) {
                fl::u32 copy_blocks = MIN(_block_count, new_block_count);
                memcpy(new_blocks, _blocks, copy_blocks * sizeof(block_type));
            }

            delete[] _blocks;
            _blocks = new_blocks;
            _block_count = new_block_count;
        }

        _size = new_size;

        // Clear any bits beyond the new size
        if (_blocks && _block_count > 0 && _size % bits_per_block != 0) {
            fl::u32 last_block_idx = (_size - 1) / bits_per_block;
            fl::u32 last_bit_pos = (_size - 1) % bits_per_block;
            block_type mask =
                static_cast<block_type>((static_cast<block_type>(1) << (last_bit_pos + 1)) - 1);
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
    FL_DISABLE_WARNING_NULL_DEREFERENCE
    void reset() noexcept {
        if (_blocks && _block_count > 0) {
            fl::memfill(_blocks, 0, _block_count * sizeof(block_type));
        }
    }
    FL_DISABLE_WARNING_POP

    // Reset a specific bit to 0
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_NULL_DEREFERENCE
    void reset(fl::u32 pos) noexcept {
        if (_blocks && pos < _size) {
            const fl::u32 idx = pos / bits_per_block;
            const fl::u32 off = pos % bits_per_block;
            _blocks[idx] &= ~(static_cast<block_type>(1) << off);
        }
    }
    FL_DISABLE_WARNING_POP

    // Set a specific bit to 1
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_NULL_DEREFERENCE
    void set(fl::u32 pos) noexcept {
        if (_blocks && pos < _size) {
            const fl::u32 idx = pos / bits_per_block;
            const fl::u32 off = pos % bits_per_block;
            _blocks[idx] |= (static_cast<block_type>(1) << off);
        }
    }
    FL_DISABLE_WARNING_POP

    // Set a specific bit to a given value
    void set(fl::u32 pos, bool value) noexcept {
        if (value) {
            set(pos);
        } else {
            reset(pos);
        }
    }

    // Flip a specific bit
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_NULL_DEREFERENCE
    void flip(fl::u32 pos) noexcept {
        if (_blocks && pos < _size) {
            const fl::u32 idx = pos / bits_per_block;
            const fl::u32 off = pos % bits_per_block;
            _blocks[idx] ^= (static_cast<block_type>(1) << off);
        }
    }
    FL_DISABLE_WARNING_POP

    // Flip all bits
    void flip() noexcept {
        if (!_blocks) return;
        
        for (fl::u32 i = 0; i < _block_count; ++i) {
            _blocks[i] = ~_blocks[i];
        }

        // Clear any bits beyond size
        if (_block_count > 0 && _size % bits_per_block != 0) {
            fl::u32 last_block_idx = (_size - 1) / bits_per_block;
            fl::u32 last_bit_pos = (_size - 1) % bits_per_block;
            block_type mask =
                static_cast<block_type>((static_cast<block_type>(1) << (last_bit_pos + 1)) - 1);
            _blocks[last_block_idx] &= mask;
        }
    }

    // Test if a bit is set
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_NULL_DEREFERENCE
    bool test(fl::u32 pos) const noexcept {
        if (_blocks && pos < _size) {
            const fl::u32 idx = pos / bits_per_block;
            const fl::u32 off = pos % bits_per_block;
            return (_blocks[idx] >> off) & 1;
        }
        return false;
    }
    FL_DISABLE_WARNING_POP

    // Count the number of set bits
    fl::u32 count() const noexcept {
        if (!_blocks) return 0;
        
        fl::u32 result = 0;
        for (fl::u32 i = 0; i < _block_count; ++i) {
            result += static_cast<fl::u32>(__builtin_popcount(_blocks[i]));
        }
        return result;
    }

    // Check if any bit is set
    bool any() const noexcept {
        if (!_blocks) return false;
        
        for (fl::u32 i = 0; i < _block_count; ++i) {
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

        for (fl::u32 i = 0; i < _block_count - 1; ++i) {
            if (_blocks[i] != static_cast<block_type>(~block_type(0)))
                return false;
        }

        // Check last block with mask for valid bits
        if (_block_count > 0) {
            fl::u32 last_bit_pos = (_size - 1) % bits_per_block;
            block_type mask =
                static_cast<block_type>((static_cast<block_type>(1) << (last_bit_pos + 1)) - 1);
            return (_blocks[_block_count - 1] & mask) == mask;
        }

        return true;
    }

    // Get the size of the bitset
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_NULL_DEREFERENCE
    fl::u32 size() const noexcept { 
        // Note: _size is a member variable, not a pointer, so this should be safe
        // but we add this comment to clarify for static analysis
        return _size; 
    }
    FL_DISABLE_WARNING_POP

    // Convert bitset to string representation
    void to_string(string* dst) const;

    // Access operator
    bool operator[](fl::u32 pos) const noexcept { return test(pos); }

    /// Finds the first bit that matches the test value.
    /// Returns the index of the first matching bit, or -1 if none found.
    /// @param test_value The value to search for (true or false)
    /// @param offset Starting position to search from (default: 0)
    fl::i32 find_first(bool test_value, fl::u32 offset = 0) const noexcept {
        // If offset is beyond our size, no match possible
        if (offset >= _size) {
            return -1;
        }

        // Calculate which block to start from
        fl::u32 start_block = offset / bits_per_block;
        fl::u32 start_bit = offset % bits_per_block;
        
        for (fl::u32 block_idx = start_block; block_idx < _block_count; ++block_idx) {
            block_type current_block = _blocks[block_idx];
            
            // For the last block, we need to mask out unused bits
            if (block_idx == _block_count - 1 && _size % bits_per_block != 0) {
                const fl::u32 valid_bits = _size % bits_per_block;
                block_type mask = (valid_bits == bits_per_block) 
                    ? static_cast<block_type>(~block_type(0)) 
                    : static_cast<block_type>(((block_type(1) << valid_bits) - 1));
                current_block &= mask;
            }
            
            // If looking for false bits, invert the block
            if (!test_value) {
                current_block = ~current_block;
            }
            
            // For the first block, mask out bits before the offset
            if (block_idx == start_block && start_bit > 0) {
                current_block &= ~static_cast<block_type>(((block_type(1) << start_bit) - 1));
            }
            
            // If there are any matching bits in this block
            if (current_block != 0) {
                // Find the first set bit
                fl::u32 bit_pos = static_cast<fl::u32>(__builtin_ctz(current_block));
                fl::u32 absolute_pos = block_idx * bits_per_block + bit_pos;
                
                // Make sure we haven't gone past the end of the bitset
                if (absolute_pos < _size) {
                    return static_cast<fl::i32>(absolute_pos);
                }
            }
        }
        
        return -1;  // No matching bit found
    }

    // Bitwise AND operator
    bitset_dynamic operator&(const bitset_dynamic &other) const {
        bitset_dynamic result(_size);
        
        if (!_blocks || !other._blocks || !result._blocks) {
            return result;
        }
        
        fl::u32 min_blocks = MIN(_block_count, other._block_count);

        for (fl::u32 i = 0; i < min_blocks; ++i) {
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
        
        fl::u32 min_blocks = MIN(_block_count, other._block_count);

        for (fl::u32 i = 0; i < min_blocks; ++i) {
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
        
        fl::u32 min_blocks = MIN(_block_count, other._block_count);

        for (fl::u32 i = 0; i < min_blocks; ++i) {
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

        for (fl::u32 i = 0; i < _block_count; ++i) {
            result._blocks[i] = ~_blocks[i];
        }

        // Clear any bits beyond size
        if (_block_count > 0 && _size % bits_per_block != 0) {
            fl::u32 last_block_idx = (_size - 1) / bits_per_block;
            fl::u32 last_bit_pos = (_size - 1) % bits_per_block;
            block_type mask =
                static_cast<block_type>((static_cast<block_type>(1) << (last_bit_pos + 1)) - 1);
            result._blocks[last_block_idx] &= mask;
        }

        return result;
    }
};

} // namespace fl