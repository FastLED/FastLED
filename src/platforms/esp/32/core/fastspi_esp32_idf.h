#pragma once

// BETA WARNING: This is a beta version of the ESP32 SPI code without the Arduino framework.
// It may or may not work. Please send PR's if you fix it.
//
// ESP32 Hardware SPI stub implementation for pure ESP-IDF builds
// This file provides a minimal interface when Arduino framework is not available
// Future: Could implement using driver/spi_master.h for native ESP-IDF support

#include "crgb.h"

namespace fl {

// Stub implementation for IDE support and future ESP-IDF native SPI
// This provides a minimal interface when Arduino framework is not available
// Future: Could implement using driver/spi_master.h for native ESP-IDF support
template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint32_t SPI_SPEED>
class ESP32SPIOutput {
public:
	ESP32SPIOutput() {}
	ESP32SPIOutput(Selectable* /*pSelect*/) {}

	void setSelect(Selectable* /*pSelect*/) {}
	void init() {}
	static void stop() {}
	static void wait() {}
	static void waitFully() {}

	void writeByteNoWait(uint8_t /*b*/) {}
	void writeBytePostWait(uint8_t /*b*/) {}
	void writeWord(uint16_t /*w*/) {}
	void writeByte(uint8_t /*b*/) {}
	void writePixelsBulk(const CRGB* /*pixels*/, size_t /*n*/) {}

	void select() {}
	void release() {}
	void endTransaction() {}

	void writeBytesValue(uint8_t /*value*/, int /*len*/) {}
	void writeBytesValueRaw(uint8_t /*value*/, int /*len*/) {}

	template <class D> void writeBytes(uint8_t* /*data*/, int /*len*/) {}
	void writeBytes(uint8_t* /*data*/, int /*len*/) {}

	static void finalizeTransmission() {}

	template <uint8_t BIT> inline void writeBit(uint8_t /*b*/) {}

	template <uint8_t FLAGS, class D, EOrder RGB_ORDER>
	void writePixels(PixelController<RGB_ORDER> /*pixels*/, void* /*context*/) {}
};

}  // namespace fl
