
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

WaveFx::Args args_lower = WaveFx::Args{
    .factor = SUPER_SAMPLE_2X,
    .half_duplex = true,
    .speed = 0.18f,
    .dampening = 9.0f,
    .crgbMap = WaveCrgbGradientMapPtr::New(electricBlueFirePal),
};
WaveFx::Args args_upper = WaveFx::Args{
    .factor = SUPER_SAMPLE_2X,
    .half_duplex = true,
    .speed = 0.25f,
    .dampening = 3.0f,
    .crgbMap = WaveCrgbGradientMapPtr::New(electricGreenFirePal),
};


// struct Scene {
//     WaveFxPtr wave_fx_low;;
//     WaveFxPtr wave_fx_high;
//     Blend2dPtr out_fx;
// };


WaveEffect NewWaveSimulation2D(const XYMap xymap) {
    // only apply complex xymap as the last step after compositiing.
    XYMap xy_rect =
        XYMap::constructRectangularGrid(xymap.getWidth(), xymap.getHeight());
    Blend2dPtr fxBlend =  NewPtr<Blend2d>(xymap); // Final transformation goes to the blend stack.
    int width = xymap.getWidth();
    int height = xymap.getHeight();
    XYMap xyRect(width, height, false);
    WaveFxPtr wave_fx_low = NewPtr<WaveFx>(xy_rect, args_lower);
    WaveFxPtr wave_fx_high = NewPtr<WaveFx>(xy_rect, args_upper);
    Blend2dPtr blend_stack = NewPtr<Blend2d>(xymap);
    blend_stack->add(wave_fx_low);
    blend_stack->add(wave_fx_high);
    WaveEffect out = {
        .wave_fx_low = wave_fx_low,
        .wave_fx_high = wave_fx_high,
        .blend_stack = blend_stack,
    };

    return out;
}
