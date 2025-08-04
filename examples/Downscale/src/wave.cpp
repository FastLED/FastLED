
#include "wave.h"
#include "FastLED.h"

DEFINE_GRADIENT_PALETTE(electricBlueFirePal){
    0,   0,   0,   0,   // Black
    32,  0,   0,   70,  // Dark blue
    128, 20,  57,  255, // Electric blue
    255, 255, 255, 255  // White
};

DEFINE_GRADIENT_PALETTE(electricGreenFirePal){
    0,   0,   0,   0,   // black
    8,   128, 64,  64,  // green
    16,  255, 222, 222, // red
    64,  255, 255, 255, // white
    255, 255, 255, 255  // white
};

WaveFx::Args CreateArgsLower() {
    WaveFx::Args out;
    out.factor = SuperSample::SUPER_SAMPLE_2X;
    out.half_duplex = true;
    out.auto_updates = true;
    out.speed = 0.18f;
    out.dampening = 9.0f;
    out.crgbMap = fl::make_shared<WaveCrgbGradientMap>(electricBlueFirePal);
    return out;
}

WaveFx::Args CreateArgsUpper() {
    WaveFx::Args out;
    out.factor = SuperSample::SUPER_SAMPLE_2X;
    out.half_duplex = true;
    out.auto_updates = true;
    out.speed = 0.25f;
    out.dampening = 3.0f;
    out.crgbMap = fl::make_shared<WaveCrgbGradientMap>(electricGreenFirePal);
    return out;
}

WaveEffect NewWaveSimulation2D(const XYMap& xymap) {
    // only apply complex xymap as the last step after compositiing.
    XYMap xy_rect =
        XYMap::constructRectangularGrid(xymap.getWidth(), xymap.getHeight());
    Blend2dPtr fxBlend =
        fl::make_shared<Blend2d>(xymap); // Final transformation goes to the blend stack.
    int width = xymap.getWidth();
    int height = xymap.getHeight();
    XYMap xyRect(width, height, false);
    WaveFx::Args args_lower = CreateArgsLower();
    WaveFx::Args args_upper = CreateArgsUpper();
    WaveFxPtr wave_fx_low = fl::make_shared<WaveFx>(xy_rect, args_lower);
    WaveFxPtr wave_fx_high = fl::make_shared<WaveFx>(xy_rect, args_upper);
    Blend2dPtr blend_stack = fl::make_shared<Blend2d>(xymap);
    blend_stack->add(wave_fx_low);
    blend_stack->add(wave_fx_high);
    WaveEffect out = {
        .wave_fx_low = wave_fx_low,
        .wave_fx_high = wave_fx_high,
        .blend_stack = blend_stack,
    };

    return out;
}
