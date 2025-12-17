/// @file fl/chipsets/spi_chipsets.h
/// @brief SPI LED chipset enumeration
///
/// Defines the SpiChipset enum class used throughout FastLED for identifying
/// SPI-based LED protocols (APA102, SK9822, WS2801, etc.)

#pragma once

namespace fl {

/// LED chipsets with SPI interface (clocked protocols)
/// Modern type-safe enum class - prefer this for new code
enum class SpiChipset {
	LPD6803,   ///< LPD6803 LED chipset
	LPD8806,   ///< LPD8806 LED chipset
	WS2801,    ///< WS2801 LED chipset
	WS2803,    ///< WS2803 LED chipset
	SM16716,   ///< SM16716 LED chipset
	P9813,     ///< P9813 LED chipset
	APA102,    ///< APA102 LED chipset
	SK9822,    ///< SK9822 LED chipset
	SK9822HD,  ///< SK9822 LED chipset with 5-bit gamma correction
	DOTSTAR,   ///< APA102 LED chipset alias
	DOTSTARHD, ///< APA102HD LED chipset alias
	APA102HD,  ///< APA102 LED chipset with 5-bit gamma correction
	HD107,     ///< Same as APA102, but in turbo 40-mhz mode.
	HD107HD,   ///< Same as APA102HD, but in turbo 40-mhz mode.
	HD108,     ///< 16-bit variant of HD107, always gamma corrected. No SD (standard definition) option available - all HD108s use gamma correction, and a non-gamma corrected version is not planned.
};

} // namespace fl
