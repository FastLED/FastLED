// This file includes all FastLED FX implementation files for allsrc build.
// All .cpp files have been converted to .hpp files to enable header-only builds
// across all platforms without conditional compilation.

#include "fx/2d/blend.hpp"
#include "fx/2d/noisepalette.hpp"
#include "fx/2d/scale_up.hpp"
#include "fx/2d/wave.hpp"
#include "fx/detail/fx_layer.hpp"
#include "fx/frame.hpp"
#include "fx/fx_engine.hpp"
#include "fx/time.hpp"
#include "fx/video/frame_interpolator.hpp"
#include "fx/video/frame_tracker.hpp"
#include "fx/video/pixel_stream.hpp"
#include "fx/video/video_impl.hpp"
#include "fx/video.hpp"
