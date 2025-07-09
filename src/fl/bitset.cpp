#include "fl/compiler_control.h"
#include "fl/bitset_dynamic.h"

#if !FASTLED_ALL_SRC
#include "fl/bitset.cpp.hpp"
#endif

// Implementation for bitset_dynamic constructor from bitstring
// This needs to be non-inline to be included in the library
fl::bitset_dynamic::bitset_dynamic(const char* bitstring) : _blocks(nullptr), _block_count(0), _size(0) {
    if (!bitstring) return;
    
    // Count valid '0'/'1' characters
    fl::u32 valid_bits = 0;
    for (fl::u32 i = 0; bitstring[i] != '\0'; ++i) {
        if (bitstring[i] == '0' || bitstring[i] == '1') {
            ++valid_bits;
        }
    }
    
    if (valid_bits == 0) return;
    resize(valid_bits);
    
    // Parse the bitstring and set bits (first character is MSB)
    fl::u32 bit_index = 0;
    for (fl::u32 i = 0; bitstring[i] != '\0'; ++i) {
        if (bitstring[i] == '1') {
            set(bit_index, true);
            ++bit_index;
        } else if (bitstring[i] == '0') {
            set(bit_index, false);
            ++bit_index;
        }
        // ignore other chars (don't increment bit_index)
    }
}
