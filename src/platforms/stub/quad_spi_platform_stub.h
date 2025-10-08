#pragma once

// Stub platform Quad-SPI configuration for testing
// This header is included by platforms/quad_spi_platform.h when FASTLED_TESTING is defined

namespace fl {

// Testing mode: Enable Quad-SPI on any platform when FASTLED_TESTING is defined
#define FASTLED_HAS_QUAD_SPI 1
#define FASTLED_QUAD_SPI_MAX_LANES 4
#define FASTLED_QUAD_SPI_NUM_BUSES 1
#define FASTLED_QUAD_SPI_BUS_2 2

}  // namespace fl
