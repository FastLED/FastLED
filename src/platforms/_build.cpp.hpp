
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
#include "platforms/adafruit/_build.cpp.hpp"
#include "platforms/arduino/_build.cpp.hpp"
#include "platforms/arm/_build.cpp.hpp"
#include "platforms/avr/_build.cpp.hpp"
#include "platforms/esp/_build.cpp.hpp"
#include "platforms/posix/_build.cpp.hpp"
#include "platforms/shared/_build.cpp.hpp"
#include "platforms/stub/_build.cpp.hpp"
#include "platforms/teensy/_build.cpp.hpp"
#include "platforms/wasm/_build.cpp.hpp"
#include "platforms/win/_build.cpp.hpp"
