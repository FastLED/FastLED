#pragma once

#include "fl/bitset.h"
#include "fl/bitset_dynamic.h"

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

// Helper to parse a bitstring and set bits in a bitset
template<typename SetBitFunc>
inline void parse_bitstring(const char* bitstring, SetBitFunc&& set_bit) {
    if (!bitstring) return;
    
    // First pass: find the total length including valid '0'/'1' chars
    fl::u32 total_len = 0;
    while (bitstring[total_len] != '\0') ++total_len;
    
    // Second pass: parse only valid '0'/'1' characters
    fl::u32 bit_index = 0;
    for (fl::u32 i = 0; i < total_len; ++i) {
        if (bitstring[i] == '1') {
            set_bit(bit_index, true);
            ++bit_index;
        } else if (bitstring[i] == '0') {
            set_bit(bit_index, false);
            ++bit_index;
        }
        // ignore other chars (don't increment bit_index)
    }
}

} // namespace detail

// Implementation for bitset_dynamic::to_string
void bitset_dynamic::to_string(string* dst) const {
    detail::to_string(_blocks, _size, dst);
}

// Implementation for BitsetFixed<N> constructor from bitstring

template <fl::u32 N>
fl::BitsetFixed<N>::BitsetFixed(const char* bitstring) : _blocks{} {
    detail::parse_bitstring(bitstring, [this](fl::u32 i, bool v) {
        if (i < N) this->set(i, v);
    });
}



// Implementation for BitsetInlined<N> constructor from bitstring

template <fl::u32 N>
fl::BitsetInlined<N>::BitsetInlined(const char* bitstring) : _storage(typename BitsetInlined<N>::fixed_bitset()) {
    if (!bitstring) return;
    
    // Count valid '0'/'1' characters
    fl::u32 valid_bits = 0;
    for (fl::u32 i = 0; bitstring[i] != '\0'; ++i) {
        if (bitstring[i] == '0' || bitstring[i] == '1') {
            ++valid_bits;
        }
    }
    
    if (valid_bits <= N) {
        // Use fixed bitset
        detail::parse_bitstring(bitstring, [this](fl::u32 i, bool v) {
            if (i < N) this->_storage.template ptr<fixed_bitset>()->set(i, v);
        });
    } else {
        // Use dynamic bitset
        _storage = bitset_dynamic(bitstring);
    }
}

} // namespace fl

// Explicit instantiations for common sizes used in tests
template fl::BitsetFixed<4>::BitsetFixed(const char* bitstring);
template fl::BitsetFixed<8>::BitsetFixed(const char* bitstring);
template fl::BitsetFixed<16>::BitsetFixed(const char* bitstring);
template fl::BitsetFixed<32>::BitsetFixed(const char* bitstring);
template fl::BitsetFixed<64>::BitsetFixed(const char* bitstring);
template fl::BitsetFixed<100>::BitsetFixed(const char* bitstring);
template fl::BitsetFixed<200>::BitsetFixed(const char* bitstring);

template fl::BitsetInlined<8>::BitsetInlined(const char* bitstring);
template fl::BitsetInlined<16>::BitsetInlined(const char* bitstring);
template fl::BitsetInlined<32>::BitsetInlined(const char* bitstring);
template fl::BitsetInlined<64>::BitsetInlined(const char* bitstring);
template fl::BitsetInlined<100>::BitsetInlined(const char* bitstring);
template fl::BitsetInlined<200>::BitsetInlined(const char* bitstring);
