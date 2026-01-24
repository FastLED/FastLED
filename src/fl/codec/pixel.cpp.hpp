#include "fl/codec/pixel.h"
#include "fastled_progmem.h"

namespace fl {

// RGB565 to RGB888 conversion lookup tables (using proper rounding for optimal color accuracy)
// 5-bit to 8-bit: (i * 255.0) / 31.0 + 0.5 with rounding
const fl::u8 rgb565_5to8_table[32] FL_PROGMEM = {
    0, 8, 16, 25, 33, 41, 49, 58, 66, 74, 82, 90, 99, 107, 115, 123,
    132, 140, 148, 156, 165, 173, 181, 189, 197, 206, 214, 222, 230, 239, 247, 255
};

// 6-bit to 8-bit: (i * 255.0) / 63.0 + 0.5 with rounding
const fl::u8 rgb565_6to8_table[64] FL_PROGMEM = {
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 45, 49, 53, 57, 61,
    65, 69, 73, 77, 81, 85, 89, 93, 97, 101, 105, 109, 113, 117, 121, 125,
    130, 134, 138, 142, 146, 150, 154, 158, 162, 166, 170, 174, 178, 182, 186, 190,
    194, 198, 202, 206, 210, 215, 219, 223, 227, 231, 235, 239, 243, 247, 251, 255
};

// Convert RGB565 to RGB888 with proper scaling to full 8-bit range using lookup tables
void rgb565ToRgb888(fl::u16 rgb565, fl::u8& r, fl::u8& g, fl::u8& b) {
    // Extract RGB components from RGB565
    fl::u8 r5 = (rgb565 >> 11) & 0x1F;  // 5-bit red
    fl::u8 g6 = (rgb565 >> 5) & 0x3F;   // 6-bit green
    fl::u8 b5 = rgb565 & 0x1F;          // 5-bit blue

    // Use lookup tables for fast conversion
    r = FL_PGM_READ_BYTE_NEAR(&rgb565_5to8_table[r5]);
    g = FL_PGM_READ_BYTE_NEAR(&rgb565_6to8_table[g6]);
    b = FL_PGM_READ_BYTE_NEAR(&rgb565_5to8_table[b5]);
}

} // namespace fl