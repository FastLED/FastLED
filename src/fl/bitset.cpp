#include "fl/bitset.h"

#include "fl/string.h"

namespace fl {

namespace detail {
void to_string(const fl::u16 *bit_data, fl::u32 bit_count, string* dst) {
    fl::string& result = *dst;
    constexpr fl::u32 bits_per_block = 8 * sizeof(fl::u16); // 16 bits per block
    
    for (fl::u32 i = 0; i < bit_count; ++i) {
        const fl::u32 block_idx = i / bits_per_block;
        const fl::u32 bit_offset = i % bits_per_block;
        
        // Extract the bit from the block
        bool bit_value = (bit_data[block_idx] >> bit_offset) & 1;
        result.append(bit_value ? "1" : "0");
    }
}
} // namespace detail

// Implementation for bitset_dynamic::to_string
void bitset_dynamic::to_string(string* dst) const {
    detail::to_string(_blocks, _size, dst);
}

} // namespace fl
