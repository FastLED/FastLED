#include "pixel_controller.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("PixelController uses the same temporal dither phase for multiple controllers") {
    CRGB pixel(40, 40, 40);
    ColorAdjustment adjustment = ColorAdjustment::noAdjustment();
    adjustment.premixed = CRGB(24, 24, 24);

    PixelController<RGB> first(&pixel, 1, adjustment, BINARY_DITHER);
    PixelController<RGB> second(&pixel, 1, adjustment, BINARY_DITHER);

    FL_CHECK_EQ(first.d[0], second.d[0]);
    FL_CHECK_EQ(first.d[1], second.d[1]);
    FL_CHECK_EQ(first.d[2], second.d[2]);
}

FL_TEST_CASE("PixelController advances temporal dither phase per frame") {
    CRGB pixel(1, 1, 1);
    ColorAdjustment adjustment = ColorAdjustment::noAdjustment();
    adjustment.premixed = CRGB(32, 32, 32);

    fl::u8 frame_phases[8];
    for (fl::u8 frame = 0; frame < 8; ++frame) {
        fl::detail::advanceDitherFrame();
        PixelController<RGB> pixels(&pixel, 1, adjustment, BINARY_DITHER);
        frame_phases[frame] = pixels.d[0];
    }

    for (fl::u8 frame = 1; frame < 8; ++frame) {
        FL_CHECK_NE(frame_phases[frame - 1], frame_phases[frame]);
    }

    PixelController<RGB> pixels(&pixel, 1, adjustment, BINARY_DITHER);
    if (pixels.d[0] == pixels.e[0] - pixels.d[0]) {
        fl::detail::advanceDitherFrame();
        pixels.init_binary_dithering();
    }
    const fl::u8 frame_start = pixels.d[0];
    pixels.stepDithering();
    FL_CHECK_EQ(pixels.d[0], pixels.e[0] - frame_start);
    FL_CHECK_NE(pixels.d[0], frame_start);
    pixels.stepDithering();
    FL_CHECK_EQ(pixels.d[0], frame_start);
}

} // FL_TEST_FILE
