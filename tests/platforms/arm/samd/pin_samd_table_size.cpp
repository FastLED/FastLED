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

#include "platforms/arm/samd/pin_samd_table_size.h"

FL_TEST_CASE("samd pin table bound includes PIN_LED_TXL past PINS_COUNT") {
    FL_CHECK_EQ(fl::platforms::samd_pin_description_count(), fl::u32(27));
}

#undef PIN_LED_TXL
#undef PIN_LED_RXL
#undef PINS_COUNT
