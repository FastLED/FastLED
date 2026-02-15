#pragma once

/// @file quad_spi_platform.h
/// @brief Simple platform detection for Quad-SPI examples
///
/// This header provides a simple compile-time check for platforms that
/// support the SPIQuad API. The actual platform capabilities are determined
/// at runtime through SPIQuad::getAll().
///
/// For example code, use: #if FASTLED_HAS_QUAD_SPI_API
/// For runtime detection, use: SPIQuad::getAll()

namespace fl {

// Simple compile-time flag for example compilation
// This only indicates that the SPIQuad API is available, not actual hardware support
// Use SPIQuad::getAll() at runtime to check for actual hardware controllers
#if defined(FASTLED_TESTING) || defined(ESP32) || \
    defined(__SAMD51G19A__) || defined(__SAMD51J19A__) || defined(__SAME51J19A__) || \
    defined(__SAMD51P19A__) || defined(__SAMD51P20A__)
    #define FASTLED_HAS_QUAD_SPI_API 1
#else
    #define FASTLED_HAS_QUAD_SPI_API 0
#endif

// Legacy macro for backwards compatibility with existing examples
#define FASTLED_HAS_QUAD_SPI FASTLED_HAS_QUAD_SPI_API

}  // namespace fl
