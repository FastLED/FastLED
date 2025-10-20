#pragma once

/// @file spi_output_template.h
/// @brief Stub platform SPIOutput template definition

#ifndef __INC_PLATFORMS_STUB_SPI_OUTPUT_TEMPLATE_H
#define __INC_PLATFORMS_STUB_SPI_OUTPUT_TEMPLATE_H

#include "fl/int.h"


/// Stub SPI output - no-op implementation for testing/simulation
template<u8 _DATA_PIN, u8 _CLOCK_PIN, u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public StubSPIOutput {};


#endif
