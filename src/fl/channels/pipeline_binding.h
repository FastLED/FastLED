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

#include "fl/channels/color_profile.h"
#include "fl/gfx/pipeline.h"
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

}  // namespace fl
