// ok cpp include
//
// Regression test for FastLED#4113.
//
// fl/spi_bus.h reaches every platform through fl/channels/cled_controller.h.
// On STM32 the CMSIS device headers are included first and define the SPI
// peripheral base pointers as *object-like macros*:
//
//     #define SPI2 ((SPI_TypeDef *) SPI2_BASE)
//
// so an enumerator named SPI2 was rewritten by the preprocessor into
//
//     ((SPI_TypeDef *) SPI2_BASE) = 2,
//
// and every STM32 board failed to compile with "expected identifier". Seven
// board builds were red on master for it. Arduino-ESP32 3.1.0+ defines SPI2
// too, so this is not STM32-specific.
//
// This file stands in for CMSIS: it defines the same shape of macro before
// including the header, which reproduces the failure without needing an ARM
// toolchain. If someone reintroduces a vendor-macro-shaped enumerator name,
// this stops compiling -- which is the point, and is why the check lives here
// rather than as a runtime assertion.

struct SPI_TypeDef {
    int dummy;
};

#define SPI2_BASE 0x40003800UL
#define SPI3_BASE 0x40003C00UL
#define SPI2 ((SPI_TypeDef *)SPI2_BASE)
#define SPI3 ((SPI_TypeDef *)SPI3_BASE)
// Arduino-ESP32 3.1.0+ and various vendor headers also claim these.
#define HSPI 2
#define VSPI 3

#include "fl/spi_bus.h"

#undef SPI2
#undef SPI3
#undef HSPI
#undef VSPI

#include "test.h"

FL_TEST_CASE("fl::Esp32SpiBus survives vendor SPI peripheral macros") {
    // Compiling at all is the assertion. These pin the wire values, which the
    // ESP-IDF backend maps straight onto SPI2_HOST / SPI3_HOST.
    FL_CHECK_EQ(static_cast<int>(fl::Esp32SpiBus::AUTO), 0);
    FL_CHECK_EQ(static_cast<int>(fl::Esp32SpiBus::HOST2), 2);
    FL_CHECK_EQ(static_cast<int>(fl::Esp32SpiBus::HOST3), 3);
}
