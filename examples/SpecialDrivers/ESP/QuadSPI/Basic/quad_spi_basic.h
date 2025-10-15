/// @file quad_spi_basic.h
/// @brief Header for QuadSPI Basic example implementation
///
/// NOTE: This code is separated into .h/.cpp files to work around PlatformIO's
/// Library Dependency Finder (LDF). The LDF aggressively scans .ino files and
/// can pull in unwanted dependencies (like TinyUSB audio code), but it does NOT
/// traverse into .cpp files the same way. By keeping the implementation in .cpp,
/// we avoid LDF triggering malloc dependencies on platforms like NRF52.

#pragma once

void quad_spi_basic_setup();
void quad_spi_basic_loop();
