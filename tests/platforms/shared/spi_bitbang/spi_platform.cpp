/// @file spi_platform.cpp
/// @brief Compiles and runs every SpiBlock*::transmit() body under host simulation,
/// so the target-selection guard in spi_platform.h is really instantiated.

#include "test.h"

#include "fl/stl/int.h"
#include "platforms/shared/spi_bitbang/host_sim.h"
#include "platforms/shared/spi_bitbang/spi_block_1.h"
#include "platforms/shared/spi_bitbang/spi_block_16.h"
#include "platforms/shared/spi_bitbang/spi_block_2.h"
#include "platforms/shared/spi_bitbang/spi_block_32.h"
#include "platforms/shared/spi_bitbang/spi_block_4.h"
#include "platforms/shared/spi_bitbang/spi_block_8.h"
#include "platforms/shared/spi_bitbang/spi_platform.h"

namespace fl {

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("host simulation has GPIO writes") { FL_CHECK_EQ(FL_SPI_GPIO_AVAILABLE, 1); }

FL_TEST_CASE("one SpiBlock1 byte: data, clock low, then clock high") {
    fl_gpio_sim_init();
    SpiBlock1 spi;
    FL_REQUIRE(spi.setPinMapping(0, 1));
    const u8 data[] = {1};
    spi.loadBuffer(data, 1);
    spi.transmit();
    FL_GPIO_Event event;
    FL_REQUIRE(fl_gpio_sim_read_event(&event));
    FL_CHECK_EQ(event.event_type, 0);
    FL_CHECK_EQ(event.gpio_mask, 1U);
    FL_REQUIRE(fl_gpio_sim_read_event(&event));
    FL_CHECK_EQ(event.event_type, 1);
    FL_CHECK_EQ(event.gpio_mask, 2U);
    FL_REQUIRE(fl_gpio_sim_read_event(&event));
    FL_CHECK_EQ(event.event_type, 0);
    FL_CHECK_EQ(event.gpio_mask, 2U);
    FL_CHECK_FALSE(fl_gpio_sim_read_event(&event));
}

FL_TEST_CASE("every width transmits through the selected writes: three per byte") {
    fl_gpio_sim_init();
    const u8 data[] = {0xA5, 0x5A};
    {
        fl_gpio_sim_clear();
        SpiBlock1 spi;
        FL_REQUIRE(spi.setPinMapping(0, 1));
        spi.loadBuffer(data, 2);
        spi.transmit();
        FL_CHECK_EQ(fl_gpio_sim_get_event_count(), 6U);
    }
    {
        fl_gpio_sim_clear();
        SpiBlock2 spi;
        FL_REQUIRE(spi.setPinMapping(0, 1, 2));
        spi.loadBuffer(data, 2);
        spi.transmit();
        FL_CHECK_EQ(fl_gpio_sim_get_event_count(), 6U);
    }
    {
        fl_gpio_sim_clear();
        SpiBlock4 spi;
        FL_REQUIRE(spi.setPinMapping(0, 1, 2, 3, 4));
        spi.loadBuffer(data, 2);
        spi.transmit();
        FL_CHECK_EQ(fl_gpio_sim_get_event_count(), 6U);
    }
    {
        fl_gpio_sim_clear();
        SpiBlock8 spi;
        FL_REQUIRE(spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, 8));
        spi.loadBuffer(data, 2);
        spi.transmit();
        FL_CHECK_EQ(fl_gpio_sim_get_event_count(), 6U);
    }
    {
        fl_gpio_sim_clear();
        SpiBlock16 spi;
        FL_REQUIRE(spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16));
        spi.loadBuffer(data, 2);
        spi.transmit();
        FL_CHECK_EQ(fl_gpio_sim_get_event_count(), 6U);
    }
    {
        fl_gpio_sim_clear();
        SpiBlock32 spi;
        // 33 pins cannot fit one 32-bit bank; the clock shares lane 31, which is only ever cleared.
        FL_REQUIRE(spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 31));
        spi.loadBuffer(data, 2);
        spi.transmit();
        FL_CHECK_EQ(fl_gpio_sim_get_event_count(), 6U);
    }
}

} // FL_TEST_FILE

} // namespace fl
