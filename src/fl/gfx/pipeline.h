#pragma once

// The streaming pipeline core (P6, #4040).
//
// P6's stages have existed as separate modules for a while -- decode,
// primaries->XYZ, chromatic adaptation, the device solve, the gamut mapper,
// the flux scalar -- with nothing composing them. This is the composition:
// one bind-time build, and one per-pixel call that runs the whole chain.
//
// The chain, and why it is in this order:
//
//   1. `decodeTransferU16` turns each RGB8 code into u16 linear light. One
//      semantic conversion, per B3: there is no intermediate RGB8 linear
//      buffer and no RGB8->RGB8 correction anywhere in it.
//   2. `linearRgbToXyzQ16` takes those to s16.16 XYZ. Chromatic adaptation
//      is folded into that matrix at bind time, so it costs nothing here.
//   3. The gamut mapper maps the target onto the device hull and solves for
//      drives -- clamping belongs to it, not to the stages before.
//   4. `applyFluxScalar` scales the drives. C4's single insertion point:
//      brightness and the power limiter compose into one scalar before they
//      get here, and scaling all drives together preserves chromaticity by
//      construction. Nothing downstream rescales channels independently.
//
// Streaming, per C2: the per-pixel call carries no state and allocates
// nothing, so it runs inside `PixelController` iteration without an RGB16
// framebuffer.

#include "fl/gfx/color_profile.h"
#include "fl/gfx/colorimetric_response.h"
#include "fl/gfx/flux_scalar.h"
#include "fl/gfx/gamut_map.h"
#include "fl/gfx/source_xyz.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// Everything the per-pixel path needs, derived once when a profile binds.
///
/// Deliberately a plain aggregate of the stages' own bind-time state rather
/// than a rebuilt copy of it: the matrices here are the ones those modules
/// produce, so there is no second place for them to drift.
struct StreamingPipelineQ16 {
    /// The source's transfer function, applied per code.
    TransferFunction transfer;

    /// Source primaries to XYZ, with adaptation to D65 already folded in.
    SourceMatrixQ16 source;

    /// The device hull, its solve, and the lightness bound.
    GamutMapQ16 gamut;

    /// Brightness times power limiting, as one scalar (C4).
    ///
    /// Initialized here because `FluxScalar` has no default constructor --
    /// there is no sensible "unset" amplitude, and unity is what a pipeline
    /// that has never been told a brightness should do.
    FluxScalar flux = FluxScalar::unity();
};

/// Bind a source declaration and a device profile into a pipeline.
///
/// False when either half is degenerate -- collinear primaries, a white with
/// no cone response, a device that cannot make a neutral. The individual
/// stages decide that; this reports it.
///
/// The flux scalar starts at unity. `setPipelineFluxQ16` is how brightness
/// and the power limiter reach it, because they change between frames while
/// everything else here does not.
bool buildStreamingPipelineQ16(const SourceProfile& source,
                               const EmitterProfile& device,
                               StreamingPipelineQ16* out) FL_NO_EXCEPT;

/// Set the composed brightness-and-power scalar for the frames that follow.
void setPipelineFluxQ16(StreamingPipelineQ16* pipeline, FluxScalar flux) FL_NO_EXCEPT;

/// One pixel: an RGB8 code triple to three emitter drives in s16.16.
///
/// Drives always come back inside [0, 1]. No allocation, no state carried
/// between calls, no RGB8 intermediate.
void processPixelQ16(const StreamingPipelineQ16& pipeline, u8 r, u8 g, u8 b,
                     i32 (&drives)[3]) FL_NO_EXCEPT;

}  // namespace fl
