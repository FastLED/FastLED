/// @file rgbw.h
/// Public color namespace facade for RGBW colorimetric profile APIs.

#pragma once

#include "fl/gfx/rgbw.h"

namespace fl {
namespace color {

using ::fl::DiodeProfile;
using ::fl::InputGamut;
using ::fl::RgbwLutInterp;
using ::fl::disable_rgbw_colorimetric_lut;
using ::fl::enable_rgbw_colorimetric_lut;
using ::fl::kRgbwDefaultProfile;
using ::fl::make_diode_profile;
using ::fl::rgbw_colorimetric_lut_enabled;
using ::fl::rgbw_colorimetric_lut_memory_bytes;
using ::fl::set_input_gamut;

} // namespace color
} // namespace fl
