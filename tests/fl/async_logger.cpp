#include "test.h"
#include "fl/log.h"
#include "fl/str.h"
#include <sstream>

using namespace fl;

// Helper to capture fl::println output for testing
namespace {
    fl::vector<fl::string> captured_output;
    bool capture_enabled = false;

    // Override fl::println for testing (simple approach - just append to vector)
    // Note: This is a test-only helper, not the actual override mechanism
}

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
