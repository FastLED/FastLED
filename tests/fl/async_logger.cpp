#include "fl/log.h"
#include "fl/stl/sstream.h"
#include "doctest.h"
#include "fl/detail/async_logger.h"
#include "fl/stl/string.h"
#include "fl/stl/strstream.h"
#include "fl/int.h"

using namespace fl;

// Note: fl::println output is not captured in these tests
// The tests verify buffer state management, not actual output

TEST_CASE("fl::AsyncLogger - basic operations") {
    SUBCASE("push and flush single message") {
        AsyncLogger logger;
        logger.push(fl::string("test message"));
        CHECK_FALSE(logger.empty());

        // Flush will call fl::println (we can't easily capture output in unit tests)
        // Just verify the buffer is emptied after flush
        logger.flush();
        CHECK(logger.empty());
        CHECK_EQ(logger.size(), 0);
    }

    SUBCASE("push multiple messages") {
        AsyncLogger logger;
        logger.push(fl::string("message 1"));
        logger.push(fl::string("message 2"));
        logger.push(fl::string("message 3"));
        CHECK_EQ(logger.size(), 3);

        logger.flush();
        CHECK(logger.empty());
    }

    SUBCASE("push C-string variant") {
        AsyncLogger logger;
        logger.push("c-style string");
        CHECK_FALSE(logger.empty());

        logger.flush();
        CHECK(logger.empty());
    }

    SUBCASE("clear removes all messages without printing") {
        AsyncLogger logger;
        logger.push(fl::string("message 1"));
        logger.push(fl::string("message 2"));
        CHECK_EQ(logger.size(), 2);

        logger.clear();
        CHECK(logger.empty());
        CHECK_EQ(logger.size(), 0);
    }
}

TEST_CASE("fl::AsyncLogger - overflow tracking") {
    SUBCASE("tracks dropped messages") {
        AsyncLogger logger;

        // Fill logger with many messages (more than default capacity of 128)
        for (int i = 0; i < 200; i++) {
            fl::sstream ss;
            ss << "message " << i;
            logger.push(ss.str());
        }

        // Some messages should have been dropped
        CHECK(logger.droppedCount() > 0);
    }
}

TEST_CASE("fl::AsyncLogger - edge cases") {
    SUBCASE("flush empty buffer is no-op") {
        AsyncLogger logger;
        CHECK(logger.empty());

        logger.flush();  // Should not crash or error
        CHECK(logger.empty());
    }

    SUBCASE("multiple flushes") {
        AsyncLogger logger;
        logger.push(fl::string("msg1"));
        logger.push(fl::string("msg2"));

        logger.flush();
        CHECK(logger.empty());

        logger.flush();  // Second flush on empty buffer
        CHECK(logger.empty());
    }

    SUBCASE("push after flush") {
        AsyncLogger logger;
        logger.push(fl::string("msg1"));
        logger.flush();
        CHECK(logger.empty());

        logger.push(fl::string("msg2"));
        CHECK_EQ(logger.size(), 1);
        logger.flush();
        CHECK(logger.empty());
    }
}

TEST_CASE("fl::AsyncLogger - global instances") {
    SUBCASE("get_parlio_async_logger_isr returns valid logger") {
        AsyncLogger& logger = get_parlio_async_logger_isr();

        // Verify it's usable
        fl::size initial_size = logger.size();
        logger.push(fl::string("test"));
        CHECK_EQ(logger.size(), initial_size + 1);

        // Clean up
        logger.clear();
    }

    SUBCASE("get_parlio_async_logger_main returns valid logger") {
        AsyncLogger& logger = get_parlio_async_logger_main();

        fl::size initial_size = logger.size();
        logger.push(fl::string("test"));
        CHECK_EQ(logger.size(), initial_size + 1);

        logger.clear();
    }

    SUBCASE("get_rmt_async_logger_isr returns valid logger") {
        AsyncLogger& logger = get_rmt_async_logger_isr();

        fl::size initial_size = logger.size();
        logger.push(fl::string("test"));
        CHECK_EQ(logger.size(), initial_size + 1);

        logger.clear();
    }

    SUBCASE("get_rmt_async_logger_main returns valid logger") {
        AsyncLogger& logger = get_rmt_async_logger_main();

        fl::size initial_size = logger.size();
        logger.push(fl::string("test"));
        CHECK_EQ(logger.size(), initial_size + 1);

        logger.clear();
    }

    SUBCASE("get_spi_async_logger_isr returns valid logger") {
        AsyncLogger& logger = get_spi_async_logger_isr();

        fl::size initial_size = logger.size();
        logger.push(fl::string("test"));
        CHECK_EQ(logger.size(), initial_size + 1);

        logger.clear();
    }

    SUBCASE("get_spi_async_logger_main returns valid logger") {
        AsyncLogger& logger = get_spi_async_logger_main();

        fl::size initial_size = logger.size();
        logger.push(fl::string("test"));
        CHECK_EQ(logger.size(), initial_size + 1);

        logger.clear();
    }

    SUBCASE("get_audio_async_logger_isr returns valid logger") {
        AsyncLogger& logger = get_audio_async_logger_isr();

        fl::size initial_size = logger.size();
        logger.push(fl::string("test"));
        CHECK_EQ(logger.size(), initial_size + 1);

        logger.clear();
    }

    SUBCASE("get_audio_async_logger_main returns valid logger") {
        AsyncLogger& logger = get_audio_async_logger_main();

        fl::size initial_size = logger.size();
        logger.push(fl::string("test"));
        CHECK_EQ(logger.size(), initial_size + 1);

        logger.clear();
    }

    SUBCASE("ISR and main loggers are independent (PARLIO)") {
        AsyncLogger& isr_logger = get_parlio_async_logger_isr();
        AsyncLogger& main_logger = get_parlio_async_logger_main();

        isr_logger.clear();
        main_logger.clear();

        // Push to ISR logger
        isr_logger.push(fl::string("isr msg"));
        CHECK_EQ(isr_logger.size(), 1);
        CHECK_EQ(main_logger.size(), 0);  // Main logger unaffected

        // Push to main logger
        main_logger.push(fl::string("main msg"));
        CHECK_EQ(isr_logger.size(), 1);  // ISR logger unaffected
        CHECK_EQ(main_logger.size(), 1);

        isr_logger.clear();
        main_logger.clear();
    }

    SUBCASE("all loggers are independent across categories") {
        AsyncLogger& parlio = get_parlio_async_logger_isr();
        AsyncLogger& rmt = get_rmt_async_logger_isr();

        parlio.clear();
        rmt.clear();

        parlio.push(fl::string("parlio msg"));
        CHECK_EQ(parlio.size(), 1);
        CHECK_EQ(rmt.size(), 0);  // RMT logger unaffected

        rmt.push(fl::string("rmt msg"));
        CHECK_EQ(parlio.size(), 1);  // PARLIO logger unaffected
        CHECK_EQ(rmt.size(), 1);

        parlio.clear();
        rmt.clear();
    }
}

TEST_CASE("fl::AsyncLogger - flushN bounded flushing") {
    SUBCASE("flushN processes up to N messages") {
        AsyncLogger logger;
        logger.push(fl::string("msg1"));
        logger.push(fl::string("msg2"));
        logger.push(fl::string("msg3"));
        logger.push(fl::string("msg4"));
        logger.push(fl::string("msg5"));
        CHECK_EQ(logger.size(), 5);

        // Flush only 2 messages
        fl::size flushed = logger.flushN(2);
        CHECK_EQ(flushed, 2);
        CHECK_EQ(logger.size(), 3);

        // Flush remaining messages
        logger.flush();
        CHECK(logger.empty());
    }

    SUBCASE("flushN returns 0 on empty buffer") {
        AsyncLogger logger;
        CHECK(logger.empty());

        fl::size flushed = logger.flushN(5);
        CHECK_EQ(flushed, 0);
        CHECK(logger.empty());
    }

    SUBCASE("flushN with N > queue size flushes all") {
        AsyncLogger logger;
        logger.push(fl::string("msg1"));
        logger.push(fl::string("msg2"));
        CHECK_EQ(logger.size(), 2);

        fl::size flushed = logger.flushN(10);
        CHECK_EQ(flushed, 2);
        CHECK(logger.empty());
    }
}

TEST_CASE("fl::AsyncLogger - background flush enable/disable") {
    SUBCASE("background flush initially disabled") {
        AsyncLogger logger;
        CHECK_FALSE(logger.isBackgroundFlushEnabled());
    }

    SUBCASE("enableBackgroundFlush returns true on supported platforms") {
        AsyncLogger logger;

        // Enable background flush at 10 Hz (100ms), 5 messages per tick
        bool result = logger.enableBackgroundFlush(100, 5);

        // On platforms with timer support (ESP32, Teensy, stub), this should succeed
        // On unsupported platforms (null implementation), may return false
        if (result) {
            CHECK(logger.isBackgroundFlushEnabled());
            logger.disableBackgroundFlush();
            CHECK_FALSE(logger.isBackgroundFlushEnabled());
        }
    }

    SUBCASE("disableBackgroundFlush is safe when not enabled") {
        AsyncLogger logger;
        CHECK_FALSE(logger.isBackgroundFlushEnabled());

        logger.disableBackgroundFlush();  // Should not crash
        CHECK_FALSE(logger.isBackgroundFlushEnabled());
    }

    SUBCASE("re-enabling background flush disables previous timer") {
        AsyncLogger logger;

        bool result1 = logger.enableBackgroundFlush(100, 5);
        if (result1) {
            CHECK(logger.isBackgroundFlushEnabled());

            // Enable again with different settings
            bool result2 = logger.enableBackgroundFlush(50, 3);
            CHECK(result2);
            CHECK(logger.isBackgroundFlushEnabled());

            logger.disableBackgroundFlush();
        }
    }
}

TEST_CASE("fl::AsyncLogger - async_log_service") {
    SUBCASE("async_log_service is safe to call when nothing enabled") {
        // Should not crash even if no background flush is active
        fl::async_log_service();
    }

    SUBCASE("async_log_service flushes when timer triggers") {
        AsyncLogger& logger = get_parlio_async_logger_isr();
        logger.clear();

        // Enable background flush
        bool result = logger.enableBackgroundFlush(100, 5);
        if (result) {
            // Push some messages
            logger.push(fl::string("msg1"));
            logger.push(fl::string("msg2"));
            CHECK_EQ(logger.size(), 2);

            // Note: We can't easily test the timer ISR in unit tests
            // The timer would set the flag, then async_log_service() would flush
            // For now, just verify the service function doesn't crash
            fl::async_log_service();

            // Clean up
            logger.disableBackgroundFlush();
            logger.clear();
        }
    }
}
