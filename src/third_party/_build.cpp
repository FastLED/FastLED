/// @file _build.cpp
/// @brief Master unity build for third_party/ library
/// Compiles entire third_party/ code into single object file
/// This is the ONLY .cpp file in src/third_party/** that should be compiled

// Subdirectory implementations (alphabetical order)
#include "third_party/stb/truetype/_build.hpp"
#include "third_party/TJpg_Decoder/_build.hpp"
#include "third_party/TJpg_Decoder/src/_build.hpp"
#include "third_party/cq_kernel/_build.hpp"
#include "third_party/libhelix_mp3/_build.hpp"
#include "third_party/libnsgif/_build.hpp"
#include "third_party/libnsgif/src/_build.hpp"
#include "third_party/mpeg1_decoder/_build.hpp"
#include "third_party/object_fled/src/_build.hpp"
#include "third_party/stb/_build.hpp"
