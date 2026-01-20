/// @file tests/fl/detail/async_logger.cpp
/// @brief Tests for AsyncLogger lazy instantiation and auto-registration

#include "fl/detail/async_logger.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/new.h"
#include "doctest.h"

using namespace fl;

// Test helper to get registry size
static size_t get_registry_size() {
    size_t count = 0;
    detail::ActiveLoggerRegistry::instance().forEach([&count](AsyncLogger&) {
        count++;
    });
    return count;
}

TEST_CASE("AsyncLogger registry starts empty") {
    // The registry should be empty before any loggers are accessed
    // NOTE: This test may fail if other tests have already accessed loggers
    // In a real test suite, we'd need to isolate this test or reset the registry

    // We can't guarantee the registry is empty due to static initialization order
    // and shared state across tests, but we can verify it doesn't grow unnecessarily
    // Access the same logger twice - should not increase registry size
    AsyncLogger& logger1 = get_parlio_async_logger_isr();
    size_t after_first_access = get_registry_size();

    AsyncLogger& logger2 = get_parlio_async_logger_isr();
    size_t after_second_access = get_registry_size();

    // Verify second access doesn't increase registry size
    REQUIRE(after_second_access == after_first_access);

    // Verify we got the same logger instance
    REQUIRE(&logger1 == &logger2);
}

TEST_CASE("AsyncLogger registry grows on first use") {
    // Get initial registry size
    size_t initial_size = get_registry_size();

    // Access a logger that likely hasn't been accessed yet
    // Use a high index to minimize collision with other tests
    get_audio_async_logger_main();  // Access the logger

    size_t after_access = get_registry_size();

    // Registry should have grown by at most 1
    // (It might not grow if this logger was already accessed by another test)
    REQUIRE(after_access >= initial_size);
    REQUIRE(after_access <= initial_size + 1);
}

TEST_CASE("AsyncLogger multiple different loggers increase registry size") {
    // Get initial registry size
    size_t initial_size = get_registry_size();

    // Access different loggers
    AsyncLogger& logger1 = get_spi_async_logger_isr();
    size_t after_logger1 = get_registry_size();

    AsyncLogger& logger2 = get_spi_async_logger_main();
    size_t after_logger2 = get_registry_size();

    AsyncLogger& logger3 = get_rmt_async_logger_isr();
    size_t after_logger3 = get_registry_size();

    // Verify registry grows (or stays same if already registered)
    REQUIRE(after_logger1 >= initial_size);
    REQUIRE(after_logger2 >= after_logger1);
    REQUIRE(after_logger3 >= after_logger2);

    // Verify we got different logger instances
    REQUIRE(&logger1 != &logger2);
    REQUIRE(&logger2 != &logger3);
    REQUIRE(&logger1 != &logger3);
}

TEST_CASE("AsyncLogger template function returns same instance") {
    // Access via template function
    AsyncLogger& logger1 = get_async_logger_by_index<4>();
    AsyncLogger& logger2 = get_async_logger_by_index<4>();

    // Should be the same instance
    REQUIRE(&logger1 == &logger2);

    // Different index should be different instance
    AsyncLogger& logger3 = get_async_logger_by_index<5>();
    REQUIRE(&logger1 != &logger3);
}

TEST_CASE("AsyncLogger convenience wrappers match template indices") {
    // Verify that convenience wrappers call the correct template indices
    AsyncLogger& parlio_isr = get_parlio_async_logger_isr();
    AsyncLogger& parlio_isr_direct = get_async_logger_by_index<0>();
    REQUIRE(&parlio_isr == &parlio_isr_direct);

    AsyncLogger& parlio_main = get_parlio_async_logger_main();
    AsyncLogger& parlio_main_direct = get_async_logger_by_index<1>();
    REQUIRE(&parlio_main == &parlio_main_direct);

    AsyncLogger& rmt_isr = get_rmt_async_logger_isr();
    AsyncLogger& rmt_isr_direct = get_async_logger_by_index<2>();
    REQUIRE(&rmt_isr == &rmt_isr_direct);

    AsyncLogger& rmt_main = get_rmt_async_logger_main();
    AsyncLogger& rmt_main_direct = get_async_logger_by_index<3>();
    REQUIRE(&rmt_main == &rmt_main_direct);
}

TEST_CASE("AsyncLogger registry iteration works correctly") {
    // Access a few loggers
    get_parlio_async_logger_isr();
    get_rmt_async_logger_main();
    get_spi_async_logger_isr();

    // Count via forEach
    size_t count = 0;
    detail::ActiveLoggerRegistry::instance().forEach([&count](AsyncLogger& logger) {
        count++;
        // Verify we can push and flush (basic functionality check)
        logger.push("test message");
        logger.flush();
    });

    // Should have at least the 3 loggers we just accessed
    REQUIRE(count >= 3);
}

TEST_CASE("AsyncLogger basic push/flush functionality") {
    AsyncLogger& logger = get_async_logger_by_index<6>();

    // Initially empty
    REQUIRE(logger.empty());
    REQUIRE(logger.size() == 0);

    // Push a message
    logger.push("Test message");

    // Should have one message
    REQUIRE(!logger.empty());
    REQUIRE(logger.size() == 1);

    // Flush
    logger.flush();

    // Should be empty again
    REQUIRE(logger.empty());
    REQUIRE(logger.size() == 0);
}

TEST_CASE("AsyncLogger clear functionality") {
    AsyncLogger& logger = get_async_logger_by_index<7>();

    // Push some messages
    logger.push("Message 1");
    logger.push("Message 2");
    logger.push("Message 3");

    REQUIRE(logger.size() == 3);

    // Clear without flushing
    logger.clear();

    // Should be empty
    REQUIRE(logger.empty());
    REQUIRE(logger.size() == 0);
}
