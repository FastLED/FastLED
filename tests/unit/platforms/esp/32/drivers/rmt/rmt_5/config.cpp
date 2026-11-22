/// @file config.cpp
/// @brief Verify legacy RMT static-allocation setting maps to the FL_ spelling.

#define FASTLED_RMT_STATIC_ALLOCATION 1
#include "platforms/esp/32/drivers/rmt/rmt_5/config.h"

#if FL_RMT_STATIC_ALLOCATION != 1
#error "FASTLED_RMT_STATIC_ALLOCATION must enable FL_RMT_STATIC_ALLOCATION"
#endif

#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("RMT static allocation config preserves the legacy setting") {
    FL_CHECK_EQ(FL_RMT_STATIC_ALLOCATION, 1);
    FL_CHECK_EQ(FASTLED_RMT_STATIC_ALLOCATION, 1);
}

} // FL_TEST_FILE
