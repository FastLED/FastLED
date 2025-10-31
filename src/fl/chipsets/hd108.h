#pragma once

#include "pixeltypes.h"
#include "fl/force_inline.h"
#include "pixel_iterator.h"
#include "crgb.h"
#include "eorder.h"
#include "fl/gamma.h"
#include "platforms/shared/spi_pixel_writer.h"
#include "platforms/spi_output_template.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// HD108/NS108 16-bit SPI chipset
// - SPI mode: MODE3 recommended (CPOL=1, CPHA=1)
//
// Key features:
// 1. 16-bit color depth (65536 levels per channel)
// 2. 5-bit global current control (0-31) for brightness management
// 3. Higher color accuracy than APA102, especially at low brightness
// 4. Dual-byte header encoding for brightness/current control
//
// Protocol:
// - Start frame: 64 bits (8 bytes) of zeros
// - Per LED: 2 header bytes + 6 color bytes (RGB @ 16-bit each)
// - End frame: (num_leds / 2) + 4 bytes of 0xFF for latching
//
// References:
// - GitHub Issue #1045: Community protocol discussion
// - Pull Request #2119: Initial implementation by arfoll
// - Manufacturer: www.hd108-led.com protocol documentation
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// HD108 Controller class
/// @tparam DATA_PIN the data pin for these LEDs
/// @tparam CLOCK_PIN the clock pin for these LEDs
/// @tparam RGB_ORDER the RGB ordering for these LEDs
/// @tparam SPI_SPEED the clock divider used for these LEDs (default 25 MHz, max 40 MHz)
template <int DATA_PIN, fl::u8 CLOCK_PIN, EOrder RGB_ORDER = GRB, uint32_t SPI_SPEED = DATA_RATE_MHZ(25)>
class HD108Controller : public CPixelLEDController<RGB_ORDER> {
	typedef fl::SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
	SPI mSPI;


public:
	HD108Controller() {}

	void init() override { mSPI.init(); }

protected:
void showPixels(PixelController<RGB_ORDER> &pixels) override {

	// Brightness conversion cache
	// Initialize with invalid marker - use 0x100 conceptually (impossible 8-bit value)
	// We'll detect this by checking if lastBrightness5 > 31
	fl::u8 lastBrightness8 = 0;
	fl::u8 lastBrightness5 = 0xFF;  // > 31, 0xff marks cache as invalid


    mSPI.select();

    // ---- Start frame: 64 bits of 0 ----
    // HD108 requires 8 bytes (64 bits) of zeros to initialize the strip
    // Note: Some sources mention 128 bits (16 bytes), but 64 bits works reliably
    for (int i = 0; i < 8; i++) mSPI.writeByte(0x00);

    while (pixels.has(1)) {
        // Load raw pixel data directly in OUTPUT order (respects RGB_ORDER template parameter)
        // loadByte<0> returns first channel in output order, <1> second, <2> third
        fl::u8 c0_8 = PixelController<RGB_ORDER>::template loadByte<0>(pixels);
        fl::u8 c1_8 = PixelController<RGB_ORDER>::template loadByte<1>(pixels);
        fl::u8 c2_8 = PixelController<RGB_ORDER>::template loadByte<2>(pixels);

        // Extract brightness from ColorAdjustment
        #if FASTLED_HD_COLOR_MIXING
        fl::u8 brightness = pixels.getBrightness();
        #else
        fl::u8 brightness = 255;  // Use full brightness if HD color mixing is disabled
        #endif

        // Apply gamma correction (2.8) to convert 8-bit to 16-bit for HD108
        // This provides smooth perceptual brightness transitions across the full 65K range
        fl::u16 c0_16 = fl::gamma_2_8(c0_8);
        fl::u16 c1_16 = fl::gamma_2_8(c1_8);
        fl::u16 c2_16 = fl::gamma_2_8(c2_8);

        // Map 8-bit brightness (0-255) to 5-bit (0-31) for HD108 current control
        // Formula: (brightness * 31 + 127) / 255 provides proper rounding
        // Caching optimization: reuse conversion if brightness unchanged from previous LED
        fl::u8 bri5;
        if (brightness == lastBrightness8 && lastBrightness5 <= 31) {
            // Use cached conversion (common when all LEDs have same brightness)
            // Only use cache if lastBrightness5 is valid (<=31)
            bri5 = lastBrightness5;
        } else {
            // Compute new conversion with rounding
            fl::u16 bri = ((fl::u16)brightness * 31 + 127) / 255;
            if (bri == 0 && brightness != 0) {
                bri = 1;  // Clamp to minimum visible: prevent invisible LEDs at brightness=1-7
            }
            bri5 = static_cast<fl::u8>(bri);
            // Update cache
            lastBrightness8 = brightness;
            lastBrightness5 = bri5;
        }

        // Header bytes: HD108 dual-byte encoding for brightness/current control
        // f0 format: 1bbbbb00 (bit 7=start marker, bits 6-2=brightness, bits 1-0=padding)
        // f1 format: bbbBBBBB (bits 7-5=lower 3 bits, bits 4-0=full 5 bits, overlapping)
        // This encoding may provide separate PWM and current controls (not fully documented)
        const fl::u8 f0 = fl::u8(0x80 | ((bri5 & 0x1F) << 2));
        const fl::u8 f1 = fl::u8(((bri5 & 0x07) << 5) | (bri5 & 0x1F));

        // Transmit LED frame: 2 header bytes + 6 color bytes (16-bit, big-endian, in RGB_ORDER)
        mSPI.writeByte(f0);
        mSPI.writeByte(f1);
        mSPI.writeByte(fl::u8(c0_16 >> 8)); mSPI.writeByte(fl::u8(c0_16 & 0xFF));  // Channel 0 MSB, LSB
        mSPI.writeByte(fl::u8(c1_16 >> 8)); mSPI.writeByte(fl::u8(c1_16 & 0xFF));  // Channel 1 MSB, LSB
        mSPI.writeByte(fl::u8(c2_16 >> 8)); mSPI.writeByte(fl::u8(c2_16 & 0xFF));  // Channel 2 MSB, LSB

        pixels.stepDithering();
        pixels.advanceData();
    }

    // ---- End frame: 0xFF bytes to latch data into LEDs ----
    // Formula: (num_leds / 2) + 4 provides sufficient clock pulses for 40 MHz operation
    // This is more conservative than APA102's (num_leds + 15) / 16 formula
    const int latch = pixels.size() / 2 + 4;
    for (int i = 0; i < latch; i++) mSPI.writeByte(0xFF);
	mSPI.endTransaction();
}
};
