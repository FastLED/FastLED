/// @file remote/_build.hpp
/// @brief Unity build header for fl/remote/ directory
/// Includes in dependency order: base types, transport, rpc subsystem, then remote

#include "fl/remote/types.cpp.hpp"
#include "fl/remote/transport/_build.cpp.hpp"
#include "fl/remote/rpc/_build.cpp.hpp"
#include "fl/remote/remote.cpp.hpp"
