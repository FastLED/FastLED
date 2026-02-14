#pragma once

// ok no namespace fl

/// @file platforms/esp/8266/ldf_headers.h
/// ESP8266 PlatformIO Library Dependency Finder (LDF) hints
///
/// This file contains #if 0 blocks with library includes to hint dependencies
/// to PlatformIO's LDF scanner without actually compiling the code.

// Force LDF to detect SPI library dependency
#if 0
#include <SPI.h>
#endif
