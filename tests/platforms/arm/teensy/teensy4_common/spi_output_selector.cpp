#include "test.h"

#include "platforms/arm/teensy/teensy4_common/spi_output_selector.h"

using fl::platforms::teensy::Teensy4SpiPinRoute;

FL_TEST_CASE("Teensy 4 SPI output selects hardware only for valid pin pairs") {
    FL_CHECK((Teensy4SpiPinRoute<11, 13, false>::is_hardware));
    FL_CHECK((Teensy4SpiPinRoute<11, 13, false>::bus_index == 0));
    FL_CHECK((Teensy4SpiPinRoute<12, 13, false>::is_hardware));
    FL_CHECK((Teensy4SpiPinRoute<12, 13, false>::bus_index == 0));
    FL_CHECK((Teensy4SpiPinRoute<26, 27, false>::is_hardware));
    FL_CHECK((Teensy4SpiPinRoute<26, 27, false>::bus_index == 1));
    FL_CHECK((Teensy4SpiPinRoute<1, 27, false>::is_hardware));
    FL_CHECK((Teensy4SpiPinRoute<1, 27, false>::bus_index == 1));
    FL_CHECK((Teensy4SpiPinRoute<35, 37, false>::is_hardware));
    FL_CHECK((Teensy4SpiPinRoute<35, 37, false>::bus_index == 2));
    FL_CHECK((Teensy4SpiPinRoute<34, 37, false>::is_hardware));
    FL_CHECK((Teensy4SpiPinRoute<34, 37, false>::bus_index == 2));

    FL_CHECK((Teensy4SpiPinRoute<43, 45, true>::is_hardware));
    FL_CHECK((Teensy4SpiPinRoute<43, 45, true>::bus_index == 2));
    FL_CHECK((Teensy4SpiPinRoute<42, 45, true>::is_hardware));
    FL_CHECK((Teensy4SpiPinRoute<42, 45, true>::bus_index == 2));

    FL_CHECK_FALSE((Teensy4SpiPinRoute<1, 2, true>::is_hardware));
    FL_CHECK_FALSE((Teensy4SpiPinRoute<11, 27, true>::is_hardware));
    FL_CHECK_FALSE((Teensy4SpiPinRoute<35, 37, true>::is_hardware));
    FL_CHECK_FALSE((Teensy4SpiPinRoute<50, 49, true>::is_hardware));
}
