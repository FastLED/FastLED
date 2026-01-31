/// @file _build.cpp
/// @brief Master unity build for fl/ library
/// Compiles entire fl/ namespace into single object file
/// This is the ONLY .cpp file in src/fl/** that should be compiled

// Root directory implementations
#include "fl/_build.hpp"

// Subdirectory implementations (alphabetical order)
#include "fl/audio/_build.hpp"
#include "fl/channels/_build.hpp"
#include "fl/font/_build.hpp"
#include "fl/channels/adapters/_build.hpp"
#include "fl/chipsets/_build.hpp"
#include "fl/codec/_build.hpp"
#include "fl/detail/_build.hpp"
#include "fl/details/_build.hpp"
#include "fl/fx/1d/_build.hpp"
#include "fl/fx/2d/_build.hpp"
#include "fl/fx/_build.hpp"
#include "fl/fx/audio/_build.hpp"
#include "fl/fx/audio/detectors/_build.hpp"
#include "fl/fx/detail/_build.hpp"
#include "fl/fx/video/_build.hpp"
#include "fl/fx/wled/_build.hpp"
#include "fl/sensors/_build.hpp"
#include "fl/spi/_build.hpp"
#include "fl/stl/_build.hpp"
#include "fl/stl/detail/_build.hpp"
