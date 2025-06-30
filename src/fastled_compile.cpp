// FastLED All Source Build File
// This file includes all .cpp.hpp files for unified compilation
// Generated automatically by scripts/all_source_build.py

#include "fl/compiler_control.h"

#if FASTLED_ALL_SRC


// Platform implementations (auto-generated)
#include "platforms/arm/k20/clockless_objectfled.cpp.hpp"
#include "platforms/avr/avr_millis_timer_source.cpp.hpp"
#include "platforms/compile_test.cpp.hpp"
#include "platforms/esp/32/clockless_i2s_esp32s3.cpp.hpp"
#include "platforms/esp/32/i2s/i2s_esp32dev.cpp.hpp"
#include "platforms/esp/32/rmt_4/idf4_rmt.cpp.hpp"
#include "platforms/esp/32/rmt_4/idf4_rmt_impl.cpp.hpp"
#include "platforms/esp/32/rmt_5/idf5_rmt.cpp.hpp"
#include "platforms/esp/32/rmt_5/strip_rmt.cpp.hpp"
#include "platforms/esp/32/spi_ws2812/strip_spi.cpp.hpp"
#include "platforms/shared/ui/json/audio.cpp.hpp"
#include "platforms/shared/ui/json/button.cpp.hpp"
#include "platforms/shared/ui/json/checkbox.cpp.hpp"
#include "platforms/shared/ui/json/description.cpp.hpp"
#include "platforms/shared/ui/json/dropdown.cpp.hpp"
#include "platforms/shared/ui/json/help.cpp.hpp"
#include "platforms/shared/ui/json/number_field.cpp.hpp"
#include "platforms/shared/ui/json/slider.cpp.hpp"
#include "platforms/shared/ui/json/title.cpp.hpp"
#include "platforms/shared/ui/json/ui.cpp.hpp"
#include "platforms/shared/ui/json/ui_internal.cpp.hpp"
#include "platforms/shared/ui/json/ui_manager.cpp.hpp"
#include "platforms/stub/led_sysdefs_stub.cpp.hpp"
#include "platforms/wasm/active_strip_data.cpp.hpp"
#include "platforms/wasm/compiler/Arduino.cpp.hpp"
#include "platforms/wasm/engine_listener.cpp.hpp"
#include "platforms/wasm/fastspi_wasm.cpp.hpp"
#include "platforms/wasm/fs_wasm.cpp.hpp"
#include "platforms/wasm/js.cpp.hpp"
#include "platforms/wasm/js_bindings.cpp.hpp"
#include "platforms/wasm/timer.cpp.hpp"
#include "platforms/wasm/ui.cpp.hpp"

// Third-party implementations (auto-generated, excluding FFT)
#include "third_party/object_fled/src/OjectFLED.cpp.hpp"

// SENSORS MODULE (hierarchical)
#ifdef FASTLED_ALL_SRC
#include "sensors/sensors_compile.hpp"

// FX MODULE (hierarchical)
#include "fx/fx_compile.hpp"

// FL MODULE (hierarchical)
#include "fl/fl_compile.hpp"
#endif // FASTLED_ALL_SRC

// SENSORS MODULE (hierarchical)
#ifdef FASTLED_ALL_SRC
#include "sensors/sensors_compile.hpp"
#endif // FASTLED_ALL_SRC



#endif // FASTLED_ALL_SRC
