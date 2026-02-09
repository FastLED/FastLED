/// @file async_logger_output.cpp
/// @brief Test that async logger mechanism works end-to-end
///
/// This test verifies that:
/// 1. Messages can be pushed to async loggers (both ISR and main variants)
/// 2. Flush processes messages correctly
/// 3. The macros FL_LOG_ASYNC and FL_LOG_ASYNC_ISR work as expected
/// 4. Global logger instances (PARLIO, RMT, SPI) are accessible and functional
///
/// NOTE: Actual println output cannot be easily captured in unit tests.
/// Manual verification via Serial.println should be done for end-to-end testing.


#include "fl/log.h"
#include "fl/stl/sstream.h"
#include "test.h"
#include "fl/detail/async_logger.h"
#include "fl/stl/string.h"
#include "fl/stl/strstream.h"
#include "fl/int.h"

using namespace fl;

FL_TEST_CASE("AsyncLogger - basic push and flush workflow") {
    FL_SUBCASE("ISR logger handles const char* messages") {
        AsyncLogger logger;

        // Push ISR-style messages (const char* literals)
        logger.push("ISR message 1");
        logger.push("ISR message 2");
        logger.push("ISR message 3");

        FL_CHECK_EQ(logger.size(), 3);
        FL_CHECK_FALSE(logger.empty());

        // Flush should process all messages (calls println for each)
        logger.flush();

        FL_CHECK(logger.empty());
        FL_CHECK_EQ(logger.size(), 0);
    }

    FL_SUBCASE("Main logger handles fl::string messages") {
        AsyncLogger logger;

        // Push main-thread style messages (fl::string with formatting)
        logger.push(fl::string("Main message 1"));
        logger.push((fl::sstream() << "Value: " << 42).str());
        logger.push((fl::sstream() << "Counter: " << 100).str());

        FL_CHECK_EQ(logger.size(), 3);
        FL_CHECK_FALSE(logger.empty());

        // Flush should process all messages
        logger.flush();

        FL_CHECK(logger.empty());
        FL_CHECK_EQ(logger.size(), 0);
    }

    FL_SUBCASE("flushN processes bounded number of messages") {
        AsyncLogger logger;

        logger.push("Message 1");
        logger.push("Message 2");
        logger.push("Message 3");
        logger.push("Message 4");
        logger.push("Message 5");

        FL_CHECK_EQ(logger.size(), 5);

        // Flush only 2 messages
        fl::size flushed = logger.flushN(2);

        FL_CHECK_EQ(flushed, 2);
        FL_CHECK_EQ(logger.size(), 3);

        // Flush remaining messages
        logger.flush();

        FL_CHECK(logger.empty());
        FL_CHECK_EQ(logger.size(), 0);
    }

    FL_SUBCASE("Mixed const char* and fl::string messages") {
        AsyncLogger logger;

        logger.push("Literal message");
        logger.push(fl::string("String object"));
        logger.push((fl::sstream() << "Stream: " << 123).str());
        logger.push("Another literal");

        FL_CHECK_EQ(logger.size(), 4);

        logger.flush();

        FL_CHECK(logger.empty());
        FL_CHECK_EQ(logger.size(), 0);
    }
}

FL_TEST_CASE("AsyncLogger - macro integration test") {
    FL_SUBCASE("FL_LOG_ASYNC macro queues messages") {
        AsyncLogger logger;

        // Test FL_LOG_ASYNC macro (main thread, supports stream expressions)
        FL_LOG_ASYNC(logger, "Test message " << 1);
        FL_LOG_ASYNC(logger, "Test message " << 2);
        FL_LOG_ASYNC(logger, "Value: " << 42 << ", Name: test");

        FL_CHECK_EQ(logger.size(), 3);

        logger.flush();

        FL_CHECK(logger.empty());
    }

    FL_SUBCASE("FL_LOG_ASYNC_ISR macro queues messages") {
        AsyncLogger logger;

        // Test FL_LOG_ASYNC_ISR macro (ISR-safe, const char* only)
        FL_LOG_ASYNC_ISR(logger, "ISR test 1");
        FL_LOG_ASYNC_ISR(logger, "ISR test 2");
        FL_LOG_ASYNC_ISR(logger, "ISR test 3");

        FL_CHECK_EQ(logger.size(), 3);

        logger.flush();

        FL_CHECK(logger.empty());
    }

    FL_SUBCASE("Mixed FL_LOG_ASYNC and FL_LOG_ASYNC_ISR") {
        AsyncLogger logger;

        FL_LOG_ASYNC_ISR(logger, "ISR message");
        FL_LOG_ASYNC(logger, "Main message " << 123);
        FL_LOG_ASYNC_ISR(logger, "Another ISR");
        FL_LOG_ASYNC(logger, "Another main");

        FL_CHECK_EQ(logger.size(), 4);

        logger.flush();

        FL_CHECK(logger.empty());
    }
}

FL_TEST_CASE("AsyncLogger - global logger instances work correctly") {
    FL_SUBCASE("PARLIO ISR logger is functional") {
        AsyncLogger& logger = get_parlio_async_logger_isr();
        fl::size initial_size = logger.size();

        logger.push("PARLIO ISR test");

        FL_CHECK_EQ(logger.size(), initial_size + 1);

        logger.flush();

        FL_CHECK_EQ(logger.size(), initial_size);
    }

    FL_SUBCASE("PARLIO main logger is functional") {
        AsyncLogger& logger = get_parlio_async_logger_main();
        fl::size initial_size = logger.size();

        logger.push((fl::sstream() << "PARLIO main value: " << 99).str());

        FL_CHECK_EQ(logger.size(), initial_size + 1);

        logger.flush();

        FL_CHECK_EQ(logger.size(), initial_size);
    }

    FL_SUBCASE("RMT ISR logger is functional") {
        AsyncLogger& logger = get_rmt_async_logger_isr();
        fl::size initial_size = logger.size();

        logger.push("RMT ISR test");

        FL_CHECK_EQ(logger.size(), initial_size + 1);

        logger.flush();

        FL_CHECK_EQ(logger.size(), initial_size);
    }

    FL_SUBCASE("SPI ISR logger is functional") {
        AsyncLogger& logger = get_spi_async_logger_isr();
        fl::size initial_size = logger.size();

        logger.push("SPI ISR test");

        FL_CHECK_EQ(logger.size(), initial_size + 1);

        logger.flush();

        FL_CHECK_EQ(logger.size(), initial_size);
    }

    FL_SUBCASE("AUDIO ISR logger is functional") {
        AsyncLogger& logger = get_audio_async_logger_isr();
        fl::size initial_size = logger.size();

        logger.push("AUDIO ISR test");

        FL_CHECK_EQ(logger.size(), initial_size + 1);

        logger.flush();

        FL_CHECK_EQ(logger.size(), initial_size);
    }

    FL_SUBCASE("All loggers are independent") {
        AsyncLogger& parlio = get_parlio_async_logger_isr();
        AsyncLogger& rmt = get_rmt_async_logger_isr();
        AsyncLogger& spi = get_spi_async_logger_isr();

        fl::size parlio_size = parlio.size();
        fl::size rmt_size = rmt.size();
        fl::size spi_size = spi.size();

        // Push to PARLIO only
        parlio.push("PARLIO message");

        FL_CHECK_EQ(parlio.size(), parlio_size + 1);
        FL_CHECK_EQ(rmt.size(), rmt_size);      // Unchanged
        FL_CHECK_EQ(spi.size(), spi_size);      // Unchanged

        // Clean up
        parlio.clear();
    }
}

FL_TEST_CASE("AsyncLogger - edge cases and error handling") {
    FL_SUBCASE("flushing empty logger is safe") {
        AsyncLogger logger;
        FL_CHECK(logger.empty());

        logger.flush();  // Should not crash

        FL_CHECK(logger.empty());
    }

    FL_SUBCASE("flushN on empty logger returns 0") {
        AsyncLogger logger;
        FL_CHECK(logger.empty());

        fl::size flushed = logger.flushN(10);

        FL_CHECK_EQ(flushed, 0);
        FL_CHECK(logger.empty());
    }

    FL_SUBCASE("clear removes messages without flushing") {
        AsyncLogger logger;

        logger.push("Message 1");
        logger.push("Message 2");
        FL_CHECK_EQ(logger.size(), 2);

        logger.clear();

        FL_CHECK(logger.empty());
        FL_CHECK_EQ(logger.size(), 0);
    }

    FL_SUBCASE("multiple sequential flush operations") {
        AsyncLogger logger;

        logger.push("Message 1");
        logger.flush();
        FL_CHECK(logger.empty());

        logger.push("Message 2");
        logger.flush();
        FL_CHECK(logger.empty());

        logger.push("Message 3");
        logger.flush();
        FL_CHECK(logger.empty());
    }
}
