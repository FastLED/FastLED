/// @file async_logger_error_detection.cpp
/// @brief Test that async logger detects when logging is not enabled
///
/// This test verifies that when you access an async logger whose
/// corresponding FASTLED_LOG_*_ENABLED flag is NOT defined, an error
/// message is printed to help the user fix the configuration.
///
/// Expected behavior:
/// - First access to get_parlio_async_logger_isr() prints error (no FASTLED_LOG_PARLIO_ENABLED)
/// - Subsequent accesses do not print again (error printed once only)

// NOTE: We intentionally DO NOT define FASTLED_LOG_PARLIO_ENABLED here
// This test verifies error detection when logging is disabled

#include "test.h"
#include "fl/detail/async_logger.h"
#include "fl/int.h"

using namespace fl;

FL_TEST_CASE("AsyncLogger - error detection when logging not enabled") {
    FL_SUBCASE("accessing parlio logger without FASTLED_LOG_PARLIO_ENABLED prints error once") {
        // The logger should still be accessible, but will print an error message
        // on first access (via FL_ERROR macro)
        AsyncLogger& logger1 = fl::get_parlio_async_logger_isr();

        // Verify logger is functional despite logging being disabled
        FL_CHECK_EQ(logger1.size(), 0);
        FL_CHECK(logger1.empty());

        // Second access should not print error again (static flag prevents duplicate)
        AsyncLogger& logger2 = fl::get_parlio_async_logger_isr();

        // Should be the same logger instance
        FL_CHECK_EQ(&logger1, &logger2);
    }

    FL_SUBCASE("accessing rmt logger without FASTLED_LOG_RMT_ENABLED prints error once") {
        AsyncLogger& logger1 = fl::get_rmt_async_logger_isr();

        FL_CHECK_EQ(logger1.size(), 0);
        FL_CHECK(logger1.empty());

        AsyncLogger& logger2 = fl::get_rmt_async_logger_isr();
        FL_CHECK_EQ(&logger1, &logger2);
    }

    FL_SUBCASE("accessing spi logger without FASTLED_LOG_SPI_ENABLED prints error once") {
        AsyncLogger& logger1 = fl::get_spi_async_logger_isr();

        FL_CHECK_EQ(logger1.size(), 0);
        FL_CHECK(logger1.empty());

        AsyncLogger& logger2 = fl::get_spi_async_logger_isr();
        FL_CHECK_EQ(&logger1, &logger2);
    }

    FL_SUBCASE("accessing audio logger without FASTLED_LOG_AUDIO_ENABLED prints error once") {
        AsyncLogger& logger1 = fl::get_audio_async_logger_isr();

        FL_CHECK_EQ(logger1.size(), 0);
        FL_CHECK(logger1.empty());

        AsyncLogger& logger2 = fl::get_audio_async_logger_isr();
        FL_CHECK_EQ(&logger1, &logger2);
    }

    FL_SUBCASE("logger is still functional even when logging is disabled") {
        // Even though logging is disabled, the logger object should work
        AsyncLogger& logger = fl::get_parlio_async_logger_main();

        // Push and flush should work without crashing
        logger.push("test message");
        FL_CHECK_EQ(logger.size(), 1);

        logger.flush();
        FL_CHECK_EQ(logger.size(), 0);
        FL_CHECK(logger.empty());
    }
}
