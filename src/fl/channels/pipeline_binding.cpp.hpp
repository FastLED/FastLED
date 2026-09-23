// ok no header - implementation for fl/channels/pipeline_binding.h

#include "fl/channels/pipeline_binding.h"

#include "fl/channels/channel_events.h"
#include "fl/channels/color_managed_source.h"
#include "fl/channels/five_bit_semantics.h"
#include "fl/channels/cled_controller.h"
#include "fl/stl/new.h"

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
                                     out, binding.mHasTargetWhite
                                              ? &binding.mTargetWhite : nullptr);
}

namespace {

// Keep topology admission behind the installed hook: ordinary unprofiled
// channels must not link the wide-profile checks into their hot path.
bool buildPipelineForChannelBinding(const ColorProfileBinding& binding,
                                    const ChannelOptions& options,
                                    const ChipsetVariant& chipset,
                                    StreamingPipelineQ16* out) FL_NO_EXCEPT {
    const EmitterProfile* profile = binding.profile();
    if (profile != nullptr) {
        const colorimetric_response::EmitterTopology topology = profile->topology;
        const Rgbw* rgbw = options.mWhiteCfg.ptr<Rgbw>();
        const Rgbww* rgbww = options.mWhiteCfg.ptr<Rgbww>();
        const bool is_rgbw = rgbw != nullptr && rgbw->active();
        const bool is_rgbww = rgbww != nullptr && rgbww->active();
        const bool mismatch =
            (topology == colorimetric_response::EmitterTopology::RGB &&
             (is_rgbw || is_rgbww)) ||
            (topology == colorimetric_response::EmitterTopology::RGBW &&
             !is_rgbw) ||
            (topology == colorimetric_response::EmitterTopology::RGBWW &&
             !is_rgbww);
        if (mismatch) {
            return false;
        }
        if (topology != colorimetric_response::EmitterTopology::RGB) {
            // RGB-only codecs call loadAndScaleRGB[16] and cannot consume a
            // wide solve. Reject at binding instead of reading the RGB gamut
            // that wide pipelines do not build or silently dropping W/WW.
            if (!chipset.is<ClocklessChipset>()) {
                return false;
            }
            const ClocklessEncoder encoder =
                chipset.ptr<ClocklessChipset>()->encoder;
            const bool supports_rgbw =
                encoder == ClocklessEncoder::CLOCKLESS_ENCODER_WS2812 ||
                encoder == ClocklessEncoder::CLOCKLESS_ENCODER_UCS7604_8BIT;
            const bool supports_rgbww =
                encoder == ClocklessEncoder::CLOCKLESS_ENCODER_WS2812 ||
                encoder == ClocklessEncoder::CLOCKLESS_ENCODER_TM1812_RGBWW;
            if (topology == colorimetric_response::EmitterTopology::RGBW
                    ? !supports_rgbw : !supports_rgbww) {
                return false;
            }
        }
    }
    return buildPipelineForBinding(binding, out);
}

}  // namespace

namespace {

/// Builds the managed iterator in the caller's storage.
///
/// Named for this file because .cpp.hpp files share a translation unit under
/// the unity build, so an anonymous namespace does not isolate it.
PixelIterator* makeColorPipelineIterator(
    void* source_storage, void* iterator_storage,
    PixelController<RGB, 1, 0xFFFFFFFF>& controller, EOrder order,
    ColorPipelineFrameRef pipeline, const Rgbw& rgbw,
    Rgbww rgbww, u8 dither_phase, u8 brightness) FL_NO_EXCEPT {
    ColorManagedPixelSource* source = new (source_storage)
        ColorManagedPixelSource(controller, order, pipeline, dither_phase);
#if FL_COLOR_PIPELINE_SHARED
    const PowerFrameHooks& hooks = powerFrameHooks();
    if (hooks.frameFluxActive) {
        source->setFlux(FluxScalar::fromRawQ16(
            static_cast<i32>(hooks.frameFluxQ16)));
    } else
#endif
    {
        source->setFlux(brightness);
    }
    return new (iterator_storage) PixelIterator(source, rgbw, rgbww);
}

void destroyColorPipelineIterator(void* source_storage,
                                  void* iterator_storage) FL_NO_EXCEPT {
    static_cast<PixelIterator*>(iterator_storage)->~PixelIterator();
    static_cast<ColorManagedPixelSource*>(source_storage)
        ->~ColorManagedPixelSource();
}

#if FL_COLOR_PIPELINE_SHARED
void colorPipelineBuildPowerHistogram(
    const StreamingPipelineQ16& pipeline, span<const CRGB> leds,
    u8 emitter_count, ManagedPowerHistogram* out) FL_NO_EXCEPT {
    if (out == nullptr) return;
    *out = ManagedPowerHistogram();
    out->pixels = static_cast<u32>(leds.size());
    out->emitters = emitter_count;
    for (fl::size i = 0; i < leds.size(); ++i) {
        i32 light[5] = {};
        if (pipeline.wide) {
            processPixelWideLinearQ16(pipeline, leds[i].r, leds[i].g,
                                      leds[i].b, light);
        } else {
            i32 rgb_light[3];
            processPixelLinearQ16(pipeline, leds[i].r, leds[i].g,
                                  leds[i].b, rgb_light);
            for (int c = 0; c < 3; ++c) light[c] = rgb_light[c];
        }
        for (u8 c = 0; c < emitter_count; ++c) {
            if (light[c] > 0) {
                const u32 bounded = light[c] > 65536 ? 65536u :
                    static_cast<u32>(light[c]);
                const u8 bin = static_cast<u8>((bounded - 1u) >> 10);
                ++out->counts[c][bin];
            }
        }
    }
}

u64 colorPipelineHistogramPowerNumerator(
    const StreamingPipelineQ16& pipeline,
    const ManagedPowerHistogram& histogram, FluxScalar flux,
    PowerCodecPolicy codec, const u8 (&weights)[5],
    u8 (*electrical_map)(u8)) FL_NO_EXCEPT {
    u64 numerator = 0;
    for (u8 c = 0; c < histogram.emitters; ++c) {
        for (u8 bin = 0; bin < ManagedPowerHistogram::kBins; ++bin) {
            const u32 count = histogram.counts[c][bin];
            if (count == 0) continue;
            const i32 upper_light = static_cast<i32>((bin + 1u) << 10);
            const i32 drive = encodeLinearEmitterQ16(pipeline, c, upper_light,
                                                     flux);
            if (drive <= 0) continue;
            u8 mapped_upper = 255;
            if (codec.kind == PowerCodecKind::Byte) {
                const u32 code = (static_cast<u32>(drive) * 255u + 65535u) >> 16;
                mapped_upper = electrical_map(static_cast<u8>(code > 255 ? 255 : code));
            } else if (codec.kind == PowerCodecKind::Native7 ||
                       codec.kind == PowerCodecKind::Native5) {
                const u32 max_code = codec.kind == PowerCodecKind::Native7 ? 127u : 31u;
                const u32 native = (static_cast<u32>(drive) * max_code + 65535u) >> 16;
                const u32 equivalent = (native * 255u + max_code - 1u) / max_code;
                mapped_upper = electrical_map(static_cast<u8>(equivalent > 255 ? 255 : equivalent));
            } else if (codec.kind == PowerCodecKind::FiveBit) {
                mapped_upper = 0;
                const u32 min_field = codec.min_field < 1 ? 1 : codec.min_field;
                for (u32 field = min_field; field <= 31; ++field) {
                    const u64 numerator_code = static_cast<u64>(drive) * 255u * 31u;
                    const u64 denominator = static_cast<u64>(65536) * field;
                    const u32 code = static_cast<u32>((numerator_code + denominator - 1) / denominator);
                    const u8 bounded_code = static_cast<u8>(code > 255 ? 255 : code);
                    const u32 weighted = (static_cast<u32>(electrical_map(bounded_code)) *
                                          field + 30u) / 31u;
                    if (weighted > mapped_upper) mapped_upper = static_cast<u8>(weighted);
                }
            }
            numerator += static_cast<u64>(count) * mapped_upper * weights[c];
        }
    }
    return numerator;
}

u32 colorPipelineUnscaledPowerMilliwatts(
    const StreamingPipelineQ16& pipeline, span<const CRGB> leds,
    u8 emitter_count, ColorPipelineHooks::PowerEstimator estimate) FL_NO_EXCEPT {
    // Demand at full brightness: a copy of the pipeline at unity flux, so the
    // frame's brightness and the limiter's own previous scalar are not folded
    // into the number the limiter is about to scale.
    StreamingPipelineQ16 unity = pipeline;
    setPipelineFluxQ16(&unity, FluxScalar::unity());

    // Charge the actual solved physical emitter codes. Passing the RGB
    // subset through the legacy RGBW estimator would extract white a second
    // time, and RGBWW cannot be represented by its three-channel fold.
    // Summing per chunk truncates each chunk's per-emitter total separately:
    // under 3 mW low per 32 pixels, well inside the model's own precision.
    enum { kChunk = 32 };
    u8 chunk[kChunk * 5];
    u32 total = 0;
    fl::size filled = 0;
    for (fl::size i = 0; i < leds.size(); ++i) {
        i32 drives[5] = {};
        if (pipeline.wide) {
            processPixelWideQ16(unity, leds[i].r, leds[i].g, leds[i].b,
                                drives);
        } else {
            i32 rgb_drives[3];
            processPixelQ16(unity, leds[i].r, leds[i].g, leds[i].b,
                            rgb_drives);
            for (int c = 0; c < 3; ++c) drives[c] = rgb_drives[c];
        }
        for (int c = 0; c < emitter_count; ++c) {
            i32 d = drives[c];
            if (d < 0) {
                d = 0;
            }
            if (d > 65536) {
                d = 65536;
            }
            chunk[filled * emitter_count + c] =
                static_cast<u8>((d * 255 + 32768) >> 16);
        }
        if (++filled == kChunk) {
            total += estimate(span<const u8>(chunk, filled * emitter_count),
                              emitter_count);
            filled = 0;
        }
    }
    if (filled != 0) {
        total += estimate(span<const u8>(chunk, filled * emitter_count),
                          emitter_count);
    }
    return total;
}
#endif  // FL_COLOR_PIPELINE_SHARED

bool encodeColorPipelineManagedSpi(PixelIterator& pixels,
                                   vector_psram<u8>* out, SpiChipset chip,
                                   const CLEDController& controller) FL_NO_EXCEPT {
#if !FL_PLATFORM_HAS_TINY_MEMORY
    switch (chip) {
        case SpiChipset::LPD8806:
            pixels.writeLPD8806Wide(fl::back_inserter(*out));
            return true;
        case SpiChipset::LPD6803:
            pixels.writeLPD6803Wide(fl::back_inserter(*out));
            return true;
        default:
            break;
    }
#endif
#if FASTLED_HD_COLOR_MIXING && !FL_PLATFORM_HAS_TINY_MEMORY
    // Non-HD APA102/SK9822: the field is held at 31 on a managed channel,
    // whatever FASTLED_USE_GLOBAL_BRIGHTNESS says (#4457). That option
    // derives a strip-wide field from the first pixel and rescales only that
    // pixel -- a second shaping stage on drives the pipeline already solved,
    // dimming every other pixel by field/31. B1's conservative treatment is a
    // fixed field. Through the wide writer the HD path already links, pinned
    // at 31: same framing, one quantization from the 16-bit drive, and no new
    // encoder instantiation in sketches that bind no profile.
    switch (chip) {
        case SpiChipset::APA102:
        case SpiChipset::DOTSTAR:
        case SpiChipset::HD107:
        case SpiChipset::SK9822:
            pixels.writeFiveBitWide(fl::back_inserter(*out), 31);
            return true;
        default:
            break;
    }
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

#if FL_COLOR_PIPELINE_SHARED
PowerFrameHooks& powerFrameHooks() FL_NO_EXCEPT {
    static PowerFrameHooks hooks = {};
    return hooks;
}
#endif

void installColorPipelineHooks() FL_NO_EXCEPT {
    ColorPipelineHooks& hooks = colorPipelineHooks();
    hooks.build = &buildPipelineForChannelBinding;
    hooks.makeIterator = &makeColorPipelineIterator;
    hooks.destroyIterator = &destroyColorPipelineIterator;
    hooks.notifyProfileClearedByLegacy = &notifyColorPipelineProfileClearedByLegacy;
    hooks.encodeManagedSpi = &encodeColorPipelineManagedSpi;
#if FL_COLOR_PIPELINE_SHARED
    hooks.unscaledPowerMilliwatts = &colorPipelineUnscaledPowerMilliwatts;
    PowerFrameHooks& power_hooks = powerFrameHooks();
    power_hooks.buildPowerHistogram = &colorPipelineBuildPowerHistogram;
    power_hooks.histogramPowerNumerator = &colorPipelineHistogramPowerNumerator;
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
