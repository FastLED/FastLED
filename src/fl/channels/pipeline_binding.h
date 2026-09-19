#pragma once

// From a channel's colour-profile binding to a running pipeline (P6, #4040).
//
// `ColorProfileBinding` has carried a source profile, an emitter profile and
// a gamut policy for a while, and nothing read them: the binding's
// acceptance was tracked, but no pixel ever went through it. This is the
// adapter that turns one into a `StreamingPipelineQ16`.
//
// It lives here rather than in fl/gfx because the dependency runs that way
// round -- channels already include gfx headers, and gfx must not learn
// about channels.

#include "fl/channels/color_managed_source.h"
#include "fl/channels/color_profile.h"
#include "fl/channels/options.h"  // FL_COLOR_PIPELINE_SHARED
#include "fl/chipsets/encoders/pixel_iterator.h"
#include "fl/chipsets/spi_chipsets.h"
#include "fl/stl/vector.h"
#include "fl/gfx/eorder.h"
#include "fl/gfx/pipeline.h"
#include "fl/gfx/rgbw.h"
#include "fl/gfx/rgbww.h"
#include "pixel_controller.h"
#include "fl/stl/noexcept.h"

namespace fl {

class CLEDController;  // fl/channels/cled_controller.h; only a reference is named here

/// Build the streaming pipeline a binding describes.
///
/// False when the binding has no profile bound, or when the profiles it
/// names are degenerate -- the stages decide that, this reports it. A false
/// return is the caller's signal to leave the channel on its legacy path
/// rather than to fail the frame.
bool buildPipelineForBinding(const ColorProfileBinding& binding,
                             StreamingPipelineQ16* out) FL_NO_EXCEPT;

/// The two things `Channel` needs from the colour pipeline, reached through
/// pointers rather than by name.
///
/// This is a pay-for-what-you-use seam, not indirection for its own sake.
/// Naming `buildPipelineForBinding` and `ColorManagedPixelSource` directly
/// from `Channel` made the whole pipeline reachable from `show()` --
/// decode, the transfer LUTs, primaries to XYZ, adaptation, the gamut map,
/// OKLab, the cube root, the device solve -- so `--gc-sections` could not
/// drop any of it and every sketch paid about 3.5 KB of flash whether or not
/// it ever called `setColorProfile`.
///
/// Behind these pointers, the only thing that references the pipeline is the
/// installer, and the only thing that calls the installer is
/// `setColorProfile`. A program that never binds a profile never mentions
/// it, and the linker takes the lot.
/// Bytes `Channel` must reserve for the managed source and its iterator.
///
/// Taken with `sizeof` rather than guessed, so the two cannot drift. Naming
/// the types for their size does not defeat the elision: a header declares,
/// and it is *references* to the definitions that keep code alive.
constexpr fl::size kColorPipelineSourceStorage = sizeof(ColorManagedPixelSource);
constexpr fl::size kColorPipelineIteratorStorage = sizeof(PixelIterator);

struct ColorPipelineHooks {
    /// Derives a pipeline from a binding. Null until installed.
    bool (*build)(const ColorProfileBinding&, StreamingPipelineQ16*);

    /// Constructs a colour-managed `PixelIterator` in caller-provided
    /// storage. Null until installed.
    ///
    /// The storage is `Channel`'s, sized by `kColorPipelineIteratorStorage`,
    /// because the factory cannot return an object whose type the caller is
    /// forbidden to name -- that is the whole point of the seam.
    PixelIterator* (*makeIterator)(void* source_storage,
                                   void* iterator_storage,
                                   PixelController<RGB, 1, 0xFFFFFFFF>& controller,
                                   EOrder order,
                                   const StreamingPipelineQ16& pipeline,
                                   const Rgbw& rgbw, Rgbww rgbww,
                                   u8 dither_phase);

    /// Destroys what `makeIterator` built. Null until installed.
    void (*destroyIterator)(void* source_storage, void* iterator_storage);

    /// Sets the frame's amplitude on a pipeline. Null until installed.
    ///
    /// Here rather than called directly for the same reason as the rest:
    /// `setPipelineFluxQ16` and `FluxScalar::fromBrightness` are pipeline
    /// functions, and naming either from `Channel` keeps their translation
    /// units -- and what they pull in -- alive in every build.
    void (*setFlux)(StreamingPipelineQ16*, u8 brightness);

    /// Fires `ChannelEvents::onColorProfileWarning(ProfileClearedByLegacy)`.
    /// Null until installed.
    ///
    /// A legacy setter (`setCorrection`, `setTemperature`) can only clear a
    /// profile that was bound, and binding installs these hooks. Emitting
    /// the event directly from `ChannelOptions` instead kept the event list's
    /// invoke path -- `fl::vector`, `malloc`/`realloc`/`free`, a sort --
    /// alive in every sketch that calls `setCorrection`: ~8.3 KB on AVR.
    void (*notifyProfileClearedByLegacy)();

    /// Unscaled (full-brightness) power demand of `leds` as the pipeline will
    /// drive them, in mW, including idle draw. Null until installed.
    ///
    /// The limiter must charge the drives the strip actually lights, not the
    /// source triple the sketch wrote: for a bound profile the two differ by
    /// whatever the solve does, in either direction, and by up to 4.45x on
    /// dim emitters (#4344, #4156 R3). Evaluated at unity flux, so brightness
    /// and power limiting scale the result the same way they scale any other
    /// controller's demand.
    ///
    /// The drives are charged through `estimate`, the power limiter's own
    /// estimator, which the caller in power_mgt passes in. Naming it here
    /// would link the limiter and its tables into every build that can bind
    /// a profile, power limiting or not (#4472).
#if FL_COLOR_PIPELINE_SHARED
    using PowerEstimator = u32 (*)(span<const CRGB> leds, const Rgbw& rgbw);
    u32 (*unscaledPowerMilliwatts)(const StreamingPipelineQ16& pipeline,
                                   span<const CRGB> leds, const Rgbw& rgbw,
                                   PowerEstimator estimate);
#endif

    /// Chipset-specific quantization of a colour-managed SPI channel's wide
    /// drive, where the 8-bit path cannot do it in one step (#4042):
    /// - APA102-class HD: B1's joint code/field solve, when the chip's 5-bit
    ///   semantics (a bound profile overriding the chip default) allow the
    ///   field below 31 at the configured floor;
    /// - LPD8806 / LPD6803: one quantization to the chip's 7 / 5 bits (B3).
    /// Returns true if it encoded the frame, false to let the caller encode
    /// as before. Through the hook so none of it links into a sketch that
    /// binds no profile.
    bool (*encodeManagedSpi)(PixelIterator& pixels, vector_psram<u8>* out,
                             SpiChipset chip, const CLEDController& controller);
};

/// The installed hooks. Both pointers are null in a program that never binds
/// a colour profile.
ColorPipelineHooks& colorPipelineHooks() FL_NO_EXCEPT;

/// Point the hooks at the real implementations.
///
/// Called from `ChannelOptions::setColorProfile`, which is the only place
/// that can know the pipeline is wanted. Idempotent.
void installColorPipelineHooks() FL_NO_EXCEPT;

}  // namespace fl
