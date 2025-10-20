#pragma once

/// @file spi_output_template.h
/// @brief Stub platform SPIOutput template definition

#ifndef __INC_PLATFORMS_STUB_SPI_OUTPUT_TEMPLATE_H
#define __INC_PLATFORMS_STUB_SPI_OUTPUT_TEMPLATE_H

#include "fl/int.h"

FASTLED_NAMESPACE_BEGIN

/// Stub SPI output - no-op implementation for testing/simulation
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public fl::StubSPIOutput {};

FASTLED_NAMESPACE_END

#endif
