// This file is necessary for avr on platformio, possibly arduino ide too.

// According to this report, all arduino files must be located at one folder.
// However this is only seen in the avr platforms.
//
// https://community.platformio.org/t/platformio-not-building-all-project-library-files/22606
// The solution then is to detect AVR + platformio and selectively compile in files

#ifndef FORCE_INCLUDE_FOR_AVR
#if defined(ARDUINO) && defined(__AVR__)
#define FORCE_INCLUDE_FOR_AVR 1
#else
#define FORCE_INCLUDE_FOR_AVR 0
#endif
#endif

#if FORCE_INCLUDE_FOR_AVR
#include "fl/allocator.cpp"
#include "fl/audio.cpp"
#include "fl/blur.cpp"
#include "fl/bytestreammemory.cpp"
#include "fl/colorutils.cpp"
#include "fl/corkscrew.cpp"
#include "fl/downscale.cpp"
#include "fl/ease.cpp"
#include "fl/engine_events.cpp"
#include "fl/fft.cpp"
#include "fl/fft_impl.cpp"
#include "fl/file_system.cpp"
#include "fl/fill.cpp"
#include "fl/gamma.cpp"
#include "fl/gradient.cpp"
#include "fl/hsv16.cpp"
#include "fl/io.cpp"
#include "fl/istream.cpp"
#include "fl/json.cpp"
#include "fl/json_console.cpp"
#include "fl/leds.cpp"
#include "fl/line_simplification.cpp"
#include "fl/noise_woryley.cpp"
#include "fl/ostream.cpp"
#include "fl/ptr.cpp"
#include "fl/raster_sparse.cpp"
#include "fl/rectangular_draw_buffer.cpp"
#include "fl/screenmap.cpp"
#include "fl/sin32.cpp"
#include "fl/splat.cpp"
#include "fl/str.cpp"
#include "fl/strstream.cpp"
#include "fl/tile2x2.cpp"
#include "fl/time_alpha.cpp"
#include "fl/transform.cpp"
#include "fl/type_traits.cpp"
#include "fl/ui.cpp"
#include "fl/upscale.cpp"
#include "fl/wave_simulation.cpp"
#include "fl/wave_simulation_real.cpp"
#include "fl/xmap.cpp"
#include "fl/xymap.cpp"
#include "fl/xypath.cpp"
#include "fl/xypath_impls.cpp"
#include "fl/xypath_renderer.cpp"

// Sensors
#include "sensors/button.cpp"
#include "sensors/digital_pin.cpp"
#include "sensors/pir.cpp"
#endif
