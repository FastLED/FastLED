#pragma once

// IWYU pragma: private

/// @file spi_output_selector.h
/// @brief Picks the Teensy LC SPI writer for a pin pair.
///
/// ARMHardwareSPIOutput writes SPI0's data register, so its bytes reach only the
/// SPI0 pads (KL26 reference manual, rev. 3.2, chapter 10 signal multiplexing).
/// On any other pin pair those bytes went nowhere: only writeBit's GPIO pulses
/// appeared. Such pairs take the software writer, which drives the GPIO itself.

#include "fl/stl/int.h"
#include "fl/stl/type_traits.h"
#include "platforms/arm/teensy/teensy_lc/fastspi_arm_kl26.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"

namespace fl {

/// True when both pins are SPI0 pads: MOSI on 11 (PTC6 ALT2), 12 (PTC7 ALT5),
/// 7 (PTD2 ALT2) or 8 (PTD3 ALT5), and SCK on 13 (PTC5 ALT2) or 14 (PTD1 ALT2).
template <fl::u8 DATA_PIN, fl::u8 CLOCK_PIN>
struct TeensyLcSpi0Pins {
    enum {
        value = (DATA_PIN == 11 || DATA_PIN == 12 || DATA_PIN == 7 || DATA_PIN == 8) &&
                (CLOCK_PIN == 13 || CLOCK_PIN == 14)
    };
};

/// The writer fl::SPIOutput uses on Teensy LC for a pin pair.
template <fl::u8 DATA_PIN, fl::u8 CLOCK_PIN, fl::u32 SPI_CLOCK_DIVIDER>
using TeensyLcSPIOutputBase = typename fl::conditional<
    TeensyLcSpi0Pins<DATA_PIN, CLOCK_PIN>::value,
    ARMHardwareSPIOutput<DATA_PIN, CLOCK_PIN, SPI_CLOCK_DIVIDER, 0x40076000>,
    fl::GenericSoftwareSPIOutput<DATA_PIN, CLOCK_PIN, SPI_CLOCK_DIVIDER>>::type;

}  // namespace fl
