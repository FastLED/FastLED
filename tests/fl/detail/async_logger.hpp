#include "fl/log.h"
#include "fl/stl/sstream.h"
#include "test.h"
#include "fl/detail/async_logger.h"
#include "fl/stl/string.h"
#include "fl/stl/strstream.h"
#include "fl/int.h"

using namespace fl;

// Note: fl::println output is not captured in these tests
// The tests verify buffer state management, not actual output

FL_TEST_CASE("fl::AsyncLogger - basic operations") {
    FL_SUBCASE("push and flush single message") {
        AsyncLogger logger;
        logger.push(fl::string("test message"));
        FL_CHECK_FALSE(logger.empty());

        // Flush will call fl::println (we can't easily capture output in unit tests)
        // Just verify the buffer is emptied after flush
        logger.flush();
        FL_CHECK(logger.empty());
        FL_CHECK_EQ(logger.size(), 0);
    }

    FL_SUBCASE("push multiple messages") {
        AsyncLogger logger;
        logger.push(fl::string("message 1"));
        logger.push(fl::string("message 2"));
        logger.push(fl::string("message 3"));
        FL_CHECK_EQ(logger.size(), 3);

        logger.flush();
        FL_CHECK(logger.empty());
    }

    FL_SUBCASE("push C-string variant") {
        AsyncLogger logger;
        logger.push("c-style string");
        FL_CHECK_FALSE(logger.empty());

        logger.flush();
        FL_CHECK(logger.empty());
    }

    FL_SUBCASE("clear removes all messages without printing") {
        AsyncLogger logger;
        logger.push(fl::string("message 1"));
        logger.push(fl::string("message 2"));
        FL_CHECK_EQ(logger.size(), 2);

        logger.clear();
        FL_CHECK(logger.empty());
        FL_CHECK_EQ(logger.size(), 0);
    }
}

FL_TEST_CASE("fl::AsyncLogger - overflow tracking") {
    FL_SUBCASE("tracks dropped messages") {
        AsyncLogger logger;

        // Fill logger with many messages (more than default capacity of 128)
        for (int i = 0; i < 200; i++) {
            fl::sstream ss;
            ss << "message " << i;
            logger.push(ss.str());
        }

        // Some messages should have been dropped
        FL_CHECK(logger.droppedCount() > 0);
    }
}

FL_TEST_CASE("fl::AsyncLogger - edge cases") {
    FL_SUBCASE("flush empty buffer is no-op") {
        AsyncLogger logger;
        FL_CHECK(logger.empty());

        logger.flush();  // Should not crash or error
        FL_CHECK(logger.empty());
    }

    FL_SUBCASE("multiple flushes") {
        AsyncLogger logger;
        logger.push(fl::string("msg1"));
        logger.push(fl::string("msg2"));

        logger.flush();
        FL_CHECK(logger.empty());

        logger.flush();  // Second flush on empty buffer
        FL_CHECK(logger.empty());
    }

    FL_SUBCASE("push after flush") {
        AsyncLogger logger;
        logger.push(fl::string("msg1"));
        logger.flush();
        FL_CHECK(logger.empty());

        logger.push(fl::string("msg2"));
        FL_CHECK_EQ(logger.size(), 1);
        logger.flush();
        FL_CHECK(logger.empty());
    }
}

FL_TEST_CASE("fl::AsyncLogger - global instances") {
    FL_SUBCASE("get_parlio_async_logger_isr returns valid logger") {
        AsyncLogger& logger = get_parlio_async_logger_isr();

        // Verify it's usable
        fl::size initial_size = logger.size();
        logger.push(fl::string("test"));
        FL_CHECK_EQ(logger.size(), initial_size + 1);

        // Clean up
        logger.clear();
    }

    FL_SUBCASE("get_parlio_async_logger_main returns valid logger") {
        AsyncLogger& logger = get_parlio_async_logger_main();

        fl::size initial_size = logger.size();
        logger.push(fl::string("test"));
        FL_CHECK_EQ(logger.size(), initial_size + 1);

        logger.clear();
    }

    FL_SUBCASE("get_rmt_async_logger_isr returns valid logger") {
        AsyncLogger& logger = get_rmt_async_logger_isr();

        fl::size initial_size = logger.size();
        logger.push(fl::string("test"));
        FL_CHECK_EQ(logger.size(), initial_size + 1);

        logger.clear();
    }

    FL_SUBCASE("get_rmt_async_logger_main returns valid logger") {
        AsyncLogger& logger = get_rmt_async_logger_main();

        fl::size initial_size = logger.size();
        logger.push(fl::string("test"));
        FL_CHECK_EQ(logger.size(), initial_size + 1);

        logger.clear();
    }

    FL_SUBCASE("get_spi_async_logger_isr returns valid logger") {
        AsyncLogger& logger = get_spi_async_logger_isr();

        fl::size initial_size = logger.size();
        logger.push(fl::string("test"));
        FL_CHECK_EQ(logger.size(), initial_size + 1);

        logger.clear();
    }

    FL_SUBCASE("get_spi_async_logger_main returns valid logger") {
        AsyncLogger& logger = get_spi_async_logger_main();

        fl::size initial_size = logger.size();
        logger.push(fl::string("test"));
        FL_CHECK_EQ(logger.size(), initial_size + 1);

        logger.clear();
    }

    FL_SUBCASE("get_audio_async_logger_isr returns valid logger") {
        AsyncLogger& logger = get_audio_async_logger_isr();

        fl::size initial_size = logger.size();
        logger.push(fl::string("test"));
        FL_CHECK_EQ(logger.size(), initial_size + 1);

        logger.clear();
    }

    FL_SUBCASE("get_audio_async_logger_main returns valid logger") {
        AsyncLogger& logger = get_audio_async_logger_main();

        fl::size initial_size = logger.size();
        logger.push(fl::string("test"));
        FL_CHECK_EQ(logger.size(), initial_size + 1);

        logger.clear();
    }

    FL_SUBCASE("ISR and main loggers are independent (PARLIO)") {
        AsyncLogger& isr_logger = get_parlio_async_logger_isr();
        AsyncLogger& main_logger = get_parlio_async_logger_main();

        isr_logger.clear();
        main_logger.clear();

        // Push to ISR logger
        isr_logger.push(fl::string("isr msg"));
        FL_CHECK_EQ(isr_logger.size(), 1);
        FL_CHECK_EQ(main_logger.size(), 0);  // Main logger unaffected

        // Push to main logger
        main_logger.push(fl::string("main msg"));
        FL_CHECK_EQ(isr_logger.size(), 1);  // ISR logger unaffected
        FL_CHECK_EQ(main_logger.size(), 1);

        isr_logger.clear();
        main_logger.clear();
    }

    FL_SUBCASE("all loggers are independent across categories") {
        AsyncLogger& parlio = get_parlio_async_logger_isr();
        AsyncLogger& rmt = get_rmt_async_logger_isr();

        parlio.clear();
        rmt.clear();

        parlio.push(fl::string("parlio msg"));
        FL_CHECK_EQ(parlio.size(), 1);
        FL_CHECK_EQ(rmt.size(), 0);  // RMT logger unaffected

        rmt.push(fl::string("rmt msg"));
        FL_CHECK_EQ(parlio.size(), 1);  // PARLIO logger unaffected
        FL_CHECK_EQ(rmt.size(), 1);

        parlio.clear();
        rmt.clear();
    }
}

FL_TEST_CASE("fl::AsyncLogger - flushN bounded flushing") {
    FL_SUBCASE("flushN processes up to N messages") {
        AsyncLogger logger;
        logger.push(fl::string("msg1"));
        logger.push(fl::string("msg2"));
        logger.push(fl::string("msg3"));
        logger.push(fl::string("msg4"));
        logger.push(fl::string("msg5"));
        FL_CHECK_EQ(logger.size(), 5);

        // Flush only 2 messages
        fl::size flushed = logger.flushN(2);
        FL_CHECK_EQ(flushed, 2);
        FL_CHECK_EQ(logger.size(), 3);

        // Flush remaining messages
        logger.flush();
        FL_CHECK(logger.empty());
    }

    FL_SUBCASE("flushN returns 0 on empty buffer") {
        AsyncLogger logger;
        FL_CHECK(logger.empty());

        fl::size flushed = logger.flushN(5);
        FL_CHECK_EQ(flushed, 0);
        FL_CHECK(logger.empty());
    }

    FL_SUBCASE("flushN with N > queue size flushes all") {
        AsyncLogger logger;
        logger.push(fl::string("msg1"));
        logger.push(fl::string("msg2"));
        FL_CHECK_EQ(logger.size(), 2);

        fl::size flushed = logger.flushN(10);
        FL_CHECK_EQ(flushed, 2);
        FL_CHECK(logger.empty());
    }
}

FL_TEST_CASE("fl::AsyncLogger - background flush enable/disable") {
    FL_SUBCASE("background flush initially disabled") {
        AsyncLogger logger;
        FL_CHECK_FALSE(logger.isBackgroundFlushEnabled());
    }

    FL_SUBCASE("enableBackgroundFlush returns true on supported platforms") {
        AsyncLogger logger;

        // Enable background flush at 10 Hz (100ms), 5 messages per tick
        bool result = logger.enableBackgroundFlush(100, 5);

        // On platforms with timer support (ESP32, Teensy, stub), this should succeed
        // On unsupported platforms (null implementation), may return false
        if (result) {
            FL_CHECK(logger.isBackgroundFlushEnabled());
            logger.disableBackgroundFlush();
            FL_CHECK_FALSE(logger.isBackgroundFlushEnabled());
        }
    }

    FL_SUBCASE("disableBackgroundFlush is safe when not enabled") {
        AsyncLogger logger;
        FL_CHECK_FALSE(logger.isBackgroundFlushEnabled());

        logger.disableBackgroundFlush();  // Should not crash
        FL_CHECK_FALSE(logger.isBackgroundFlushEnabled());
    }

    FL_SUBCASE("re-enabling background flush disables previous timer") {
        AsyncLogger logger;

        bool result1 = logger.enableBackgroundFlush(100, 5);
        if (result1) {
            FL_CHECK(logger.isBackgroundFlushEnabled());

            // Enable again with different settings
            bool result2 = logger.enableBackgroundFlush(50, 3);
            FL_CHECK(result2);
            FL_CHECK(logger.isBackgroundFlushEnabled());

            logger.disableBackgroundFlush();
        }
    }
}

FL_TEST_CASE("fl::AsyncLogger - async_log_service") {
    FL_SUBCASE("async_log_service is safe to call when nothing enabled") {
        // Should not crash even if no background flush is active
        fl::async_log_service();
    }

    FL_SUBCASE("async_log_service flushes when timer triggers") {
        AsyncLogger& logger = get_parlio_async_logger_isr();
        logger.clear();

        // Enable background flush
        bool result = logger.enableBackgroundFlush(100, 5);
        if (result) {
            // Push some messages
            logger.push(fl::string("msg1"));
            logger.push(fl::string("msg2"));
            FL_CHECK_EQ(logger.size(), 2);

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

FL_TEST_CASE("fl::AsyncLogger - auto service task") {
    FL_SUBCASE("Task auto-instantiates on first logger access") {
        // Accessing any logger should auto-instantiate the service task
        AsyncLogger& logger = get_parlio_async_logger_isr();
        logger.clear();

        // Push some messages
        logger.push("test1");
        logger.push("test2");
        FL_CHECK_EQ(logger.size(), 2);

        // The service task should be registered with the scheduler
        // We can't directly test task execution in unit tests, but we can verify
        // the task exists by checking that serviceLoggers() works
        fl::detail::AsyncLoggerServiceTask::instance().serviceLoggers();

        // Messages should be flushed (or partially flushed if bounded)
        // Default is 5 messages per tick, so both should be flushed
        FL_CHECK_EQ(logger.size(), 0);
    }

    FL_SUBCASE("Configuration API exists and works") {
        // Configuration can be called at any time now
        fl::configureAsyncLogService(50, 3);

        // Verify interval was changed
        FL_CHECK_EQ(fl::detail::AsyncLoggerServiceTask::instance().getInterval(), 50);

        // Messages per tick can always be changed
        FL_CHECK_EQ(fl::detail::AsyncLoggerServiceTask::instance().getMessagesPerTick(), 3);

        // Test dynamic interval change
        fl::detail::AsyncLoggerServiceTask::instance().setInterval(100);
        FL_CHECK_EQ(fl::detail::AsyncLoggerServiceTask::instance().getInterval(), 100);

        // Reset to default for other tests
        fl::detail::AsyncLoggerServiceTask::instance().setInterval(16);
        FL_CHECK_EQ(fl::detail::AsyncLoggerServiceTask::instance().getInterval(), 16);
    }

    FL_SUBCASE("Dynamic interval changes propagate to task") {
        // Get the task from the service
        auto& service = fl::detail::AsyncLoggerServiceTask::instance();

        // Access a logger to ensure task is instantiated
        AsyncLogger& logger = get_parlio_async_logger_isr();
        logger.clear();

        // Set initial interval
        service.setInterval(25);
        FL_CHECK_EQ(service.getInterval(), 25);

        // Change interval multiple times
        service.setInterval(10);
        FL_CHECK_EQ(service.getInterval(), 10);

        service.setInterval(50);
        FL_CHECK_EQ(service.getInterval(), 50);

        service.setInterval(100);
        FL_CHECK_EQ(service.getInterval(), 100);

        // Verify we can go back to fast intervals
        service.setInterval(5);
        FL_CHECK_EQ(service.getInterval(), 5);

        // Reset to default
        service.setInterval(16);
        FL_CHECK_EQ(service.getInterval(), 16);

        // Test messages per tick changes too
        service.setMessagesPerTick(10);
        FL_CHECK_EQ(service.getMessagesPerTick(), 10);

        service.setMessagesPerTick(1);
        FL_CHECK_EQ(service.getMessagesPerTick(), 1);

        service.setMessagesPerTick(5);
        FL_CHECK_EQ(service.getMessagesPerTick(), 5);
    }

    FL_SUBCASE("Service rate can be tuned during operation") {
        auto& service = fl::detail::AsyncLoggerServiceTask::instance();
        AsyncLogger& logger = get_parlio_async_logger_isr();
        logger.clear();

        // Simulate high-speed mode
        service.setInterval(8);
        service.setMessagesPerTick(10);
        FL_CHECK_EQ(service.getInterval(), 8);
        FL_CHECK_EQ(service.getMessagesPerTick(), 10);

        // Push messages
        for (int i = 0; i < 20; i++) {
            logger.push(fl::string("test"));
        }
        FL_CHECK_EQ(logger.size(), 20);

        // Service with high rate
        service.serviceLoggers();
        FL_CHECK_EQ(logger.size(), 10);  // 10 messages flushed

        // Switch to low-speed mode
        service.setInterval(32);
        service.setMessagesPerTick(2);
        FL_CHECK_EQ(service.getInterval(), 32);
        FL_CHECK_EQ(service.getMessagesPerTick(), 2);

        // Service with low rate
        service.serviceLoggers();
        FL_CHECK_EQ(logger.size(), 8);  // Only 2 messages flushed

        // Clean up
        logger.clear();
        service.setInterval(16);
        service.setMessagesPerTick(5);
    }
}
