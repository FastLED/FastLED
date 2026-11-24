/// @file pin_samd_table_size.cpp
/// @brief The SAMD g_APinDescription bound covers variant pins past PINS_COUNT
/// (#5185). The SparkFun SAMD21 Dev variant defines PINS_COUNT 26 and
/// PIN_LED_TXL 26, and g_APinDescription[26] (PA27) is a real entry.

#include "test.h"

#include "fl/stl/int.h"

// SparkFun SAMD21 Dev variant.h values.
#define PINS_COUNT 26u
#define PIN_LED_RXL 25
#define PIN_LED_TXL 26
// Adafruit Crickit M0 defines PIN_ATN 38 with a 37-entry table, so PIN_ATN
// must not widen the bound.
#define PIN_ATN 38

#include "platforms/arm/samd/pin_samd_table_size.hpp"

FL_TEST_CASE("samd pin table bound includes PIN_LED_TXL past PINS_COUNT") {
    // 27, not 39: PIN_ATN is ignored because it can exceed the table.
    FL_CHECK_EQ(fl::platforms::samd_pin_description_count(), fl::u32(27));
}

#undef PIN_ATN
#undef PIN_LED_TXL
#undef PIN_LED_RXL
#undef PINS_COUNT
