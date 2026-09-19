// ok no header - implementation for fl/channels/pipeline_binding.h

#include "fl/channels/pipeline_binding.h"

#include "fl/channels/channel_events.h"
#include "fl/channels/color_managed_source.h"
#include "fl/channels/five_bit_semantics.h"
#include "fl/channels/cled_controller.h"
#include "fl/stl/new.h"
#include "power_mgt.h"

namespace fl {

bool buildPipelineForBinding(const ColorProfileBinding& binding,
                             StreamingPipelineQ16* out) FL_NO_EXCEPT {
    if (out == nullptr) {
        return false;
    }
    const EmitterProfile* profile = binding.profile();
    if (profile == nullptr) {
        // Not an error: a channel with no profile bound stays on the legacy
        // path, which is what `active()` being false means.
        return false;
    }
    return buildStreamingPipelineQ16(binding.mSource, *profile, binding.mGamut,
                                     out);
}

namespace {

/// Builds the managed iterator in the caller's storage.
///
/// Named for this file because .cpp.hpp files share a translation unit under
/// the unity build, so an anonymous namespace does not isolate it.
PixelIterator* makeColorPipelineIterator(
    void* source_storage, void* iterator_storage,
    PixelController<RGB, 1, 0xFFFFFFFF>& controller, EOrder order,
    const StreamingPipelineQ16& pipeline, const Rgbw& rgbw,
    Rgbww rgbww) FL_NO_EXCEPT {
    ColorManagedPixelSource* source =
        new (source_storage) ColorManagedPixelSource(controller, order, pipeline);
    return new (iterator_storage) PixelIterator(source, rgbw, rgbww);
}

void setColorPipelineFlux(StreamingPipelineQ16* pipeline,
                          u8 brightness) FL_NO_EXCEPT {
    setPipelineFluxQ16(pipeline, FluxScalar::fromBrightness(brightness));
}

void destroyColorPipelineIterator(void* source_storage,
                                  void* iterator_storage) FL_NO_EXCEPT {
    static_cast<PixelIterator*>(iterator_storage)->~PixelIterator();
    static_cast<ColorManagedPixelSource*>(source_storage)
        ->~ColorManagedPixelSource();
}

#if FL_COLOR_PIPELINE_SHARED
u32 colorPipelineUnscaledPowerMilliwatts(const StreamingPipelineQ16& pipeline,
                                         span<const CRGB> leds,
                                         const Rgbw& rgbw) FL_NO_EXCEPT {
    // Demand at full brightness: a copy of the pipeline at unity flux, so the
    // frame's brightness and the limiter's own previous scalar are not folded
    // into the number the limiter is about to scale.
    StreamingPipelineQ16 unity = pipeline;
    setPipelineFluxQ16(&unity, FluxScalar::unity());

    // The power model is 8-bit, so each solved drive is rounded to its 8-bit
    // equivalent here and charged through the same estimator every other
    // controller uses -- same per-emitter mW, same response exponent, same
    // idle draw, same RGBW conversion. This buffer exists only for the
    // estimate; nothing here reaches the output path.
    // Summing per chunk truncates each chunk's per-emitter total separately:
    // under 3 mW low per 32 pixels, well inside the model's own precision.
    enum { kChunk = 32 };
    CRGB chunk[kChunk];
    u32 total = 0;
    fl::size filled = 0;
    for (fl::size i = 0; i < leds.size(); ++i) {
        i32 drives[3];
        processPixelQ16(unity, leds[i].r, leds[i].g, leds[i].b, drives);
        for (int c = 0; c < 3; ++c) {
            i32 d = drives[c];
            if (d < 0) {
                d = 0;
            }
            if (d > 65536) {
                d = 65536;
            }
            chunk[filled].raw[c] = static_cast<u8>((d * 255 + 32768) >> 16);
        }
        if (++filled == kChunk) {
            total += calculate_unscaled_power_mW(span<const CRGB>(chunk, filled), rgbw);
            filled = 0;
        }
    }
    if (filled != 0) {
        total += calculate_unscaled_power_mW(span<const CRGB>(chunk, filled), rgbw);
    }
    return total;
}
#endif  // FL_COLOR_PIPELINE_SHARED

bool encodeColorPipelineHdWide(PixelIterator& pixels, vector_psram<u8>* out,
                               SpiChipset chip,
                               const CLEDController& controller) FL_NO_EXCEPT {
#if FASTLED_HD_COLOR_MIXING && !FL_PLATFORM_HAS_TINY_MEMORY
    // The profile is read here rather than by the caller, so the accessor
    // links only with the hook.
    const EmitterProfile* profile = controller.emitterProfile();
    const u8 min_field = hdMinimumField(
        fiveBitSemanticsFor(chip, profile != nullptr
                                      ? profile->five_bit_semantics
                                      : FiveBitSemantics::NotApplicable),
        detail::hdFieldFloor());
    if (min_field >= 31) {
        // Field held fixed: the caller's existing path is exactly that.
        return false;
    }
    pixels.writeFiveBitWide(fl::back_inserter(*out), min_field);
    return true;
#else
    FL_UNUSED(pixels);
    FL_UNUSED(out);
    FL_UNUSED(chip);
    FL_UNUSED(controller);
    return false;
#endif
}

void notifyColorPipelineProfileClearedByLegacy() FL_NO_EXCEPT {
    ChannelEvents::instance().onColorProfileWarning(
        ColorProfileEvent{-1, {}, ColorProfileWarning::ProfileClearedByLegacy});
}

}  // namespace

ColorPipelineHooks& colorPipelineHooks() FL_NO_EXCEPT {
    // Not a function-local static with a non-trivial constructor: this is a
    // zero-initialized aggregate, so there is no guard variable and no
    // Teensy 3.x `__cxa_guard` conflict.
    static ColorPipelineHooks hooks = {};
    return hooks;
}

void installColorPipelineHooks() FL_NO_EXCEPT {
    ColorPipelineHooks& hooks = colorPipelineHooks();
    hooks.build = &buildPipelineForBinding;
    hooks.makeIterator = &makeColorPipelineIterator;
    hooks.destroyIterator = &destroyColorPipelineIterator;
    hooks.setFlux = &setColorPipelineFlux;
    hooks.notifyProfileClearedByLegacy = &notifyColorPipelineProfileClearedByLegacy;
    hooks.encodeHdWide = &encodeColorPipelineHdWide;
#if FL_COLOR_PIPELINE_SHARED
    hooks.unscaledPowerMilliwatts = &colorPipelineUnscaledPowerMilliwatts;
#endif
}

void notifyColorProfileClearedByLegacy() FL_NO_EXCEPT {
    // Null unless a profile was ever bound, and nothing can have been
    // cleared in that case either.
    void (*notify)() = colorPipelineHooks().notifyProfileClearedByLegacy;
    if (notify != nullptr) {
        notify();
    }
}

}  // namespace fl
