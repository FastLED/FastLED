/// @file remote/_build.hpp
/// @brief Unity build header for fl/remote/ directory
/// Includes in dependency order: base types, rpc subsystem, then remote

#include "fl/remote/types.cpp.hpp"
#include "fl/remote/rpc/_build.hpp"
#include "fl/remote/remote.cpp.hpp"
