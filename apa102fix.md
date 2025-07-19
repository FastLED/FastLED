I was dissatisfied with a gamma corrected ramp using the APA102HD mode (it seemed to visibly step at one or more points), and some analysis of the 8 bit rgb + 5 bit brightness function showed why:
https://www.argyllcms.com/APA102_loglog.svg.

So here is a suggested replacement for five_bit_bitshift() in fl/five_bit_hd_gamma.cpp, that fixed the problem for me:

// Improved FastLED five_bit_bitshift() function
// This appears to exactly match the floating point reference,
// with a worst case resulting error of 0.2% over the full 16 bit input range,
// and an average error of 0.05% over that range. Errors scale with maximum
// magnitude.

// ix/31 * 255/65536 * 256 scaling factors, valid for indexes 1..31
static uint32_t bright_scale[32] =  {
    0, 2023680, 1011840, 674560, 505920, 404736, 337280, 289097,
    252960, 224853, 202368, 183971, 168640, 155668, 144549, 134912,
    126480, 119040, 112427, 106509, 101184, 96366, 91985, 87986,
    84320, 80947, 77834, 74951, 72274, 69782, 67456, 65280
};

// Since the return value wasn't used, it has been omitted.
// It's not clear what scale brightness is, or how it is to be applied,
// so we assume 8 bits applied over the given rgb values.
void five_bit_bitshift(uint16_t r16, uint16_t g16, uint16_t b16, uint8_t brightness,
                          CRGB *out, uint8_t *out_power_5bit) {
    uint8_t r8 = 0, g8 = 0, b8 = 0;

    // Apply any brightness setting (we assume brightness is 0..255)
    if (brightness != 0xff) {
        r16 = scale16by8(r16, brightness);
        g16 = scale16by8(g16, brightness);
        b16 = scale16by8(b16, brightness);
    }

    // Locate the largest value to set the brightness/scale factor
    uint16_t scale = max3(r16, g16, b16);

    if (scale == 0) {
        *out = CRGB(0, 0, 0);
        *out_power_5bit = 0;
        return;
    } else {
        uint32_t scalef;

        // Compute 5 bit quantized scale that is at or above the maximum value.
        scale = (scale + (2047 - (scale >> 5))) >> 11;

        // Adjust the 16 bit values to account for the scale, then round to 8 bits
        scalef = bright_scale[scale];
        r8 = (r16 * scalef + 0x808000) >> 24;
        g8 = (g16 * scalef + 0x808000) >> 24;
        b8 = (b16 * scalef + 0x808000) >> 24;

        *out = CRGB(r8, g8, b8);
        *out_power_5bit = scale;
        return;
    }
}