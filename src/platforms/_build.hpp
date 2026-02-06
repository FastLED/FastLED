/// @file _build.hpp
/// @brief Unity build header for platforms/ directory
/// Includes all root-level implementations and subdirectory _build.hpp files

// Root directory implementations (alphabetical order)
#include "platforms/arduino/platform_time.cpp.hpp"
#include "platforms/compile_test.cpp.hpp"
#include "platforms/debug_setup.cpp.hpp"
#include "platforms/ota.cpp.hpp"
#include "platforms/stub_main.cpp.hpp"

// Subdirectory implementations (alphabetical order - one level down only)
#include "platforms/adafruit/_build.hpp"
#include "platforms/arduino/_build.hpp"
#include "platforms/arm/_build.hpp"
#include "platforms/avr/_build.hpp"
#include "platforms/esp/_build.hpp"
#include "platforms/posix/_build.hpp"
#include "platforms/shared/_build.hpp"
#include "platforms/stub/_build.hpp"
#include "platforms/teensy/_build.hpp"
#include "platforms/wasm/_build.hpp"
#include "platforms/win/_build.hpp"
