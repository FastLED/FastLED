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
#include "fl/chipsets/encoders/pixel_iterator.h"
#include "fl/gfx/eorder.h"
#include "fl/gfx/pipeline.h"
#include "fl/gfx/rgbw.h"
#include "fl/gfx/rgbww.h"
#include "pixel_controller.h"
#include "fl/stl/noexcept.h"

namespace fl {

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
                                   const Rgbw& rgbw, Rgbww rgbww);

    /// Destroys what `makeIterator` built. Null until installed.
    void (*destroyIterator)(void* source_storage, void* iterator_storage);
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
