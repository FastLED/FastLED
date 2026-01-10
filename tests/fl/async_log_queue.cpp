#include "fl/detail/async_log_queue.h"
#include "fl/isr.h"  // For fl::isr::CriticalSection
#include "fl/stl/sstream.h"
#include "doctest.h"
#include "fl/stl/string.h"
#include "fl/stl/strstream.h"
#include "fl/int.h"

using namespace fl;

// Test with small sizes for easier testing of edge cases
constexpr fl::size TEST_DESC_COUNT = 8;  // Power of 2
constexpr fl::size TEST_ARENA_SIZE = 64; // Power of 2

TEST_CASE("fl::isr::CriticalSection - RAII interrupt control") {
    SUBCASE("constructor disables interrupts, destructor enables") {
        // This is hard to test directly without mocking, but we can verify it compiles
        {
            fl::isr::CriticalSection cs;
            // Interrupts should be disabled here
        }
        // Interrupts should be re-enabled here

        // Just verify construction/destruction works
        CHECK(true);
    }

    SUBCASE("non-copyable") {
        // This is a compile-time check
        // If this compiles, the test fails (should not be copyable)
        // fl::isr::CriticalSection cs1;
        // fl::isr::CriticalSection cs2 = cs1;  // Should not compile
        CHECK(true);
    }
}

TEST_CASE("fl::AsyncLogQueue - basic operations") {
    SUBCASE("constructor creates empty queue") {
        AsyncLogQueue<TEST_DESC_COUNT, TEST_ARENA_SIZE> queue;

        CHECK(queue.empty());
        CHECK_EQ(queue.size(), 0);
        CHECK_EQ(queue.capacity(), TEST_DESC_COUNT - 1);  // One slot reserved
        CHECK_EQ(queue.droppedCount(), 0);
    }

    SUBCASE("push and pop single message") {
        AsyncLogQueue<TEST_DESC_COUNT, TEST_ARENA_SIZE> queue;

        CHECK(queue.push("test message"));
        CHECK_FALSE(queue.empty());
        CHECK_EQ(queue.size(), 1);

        const char* msg;
        fl::u16 len;
        CHECK(queue.tryPop(&msg, &len));
        CHECK_EQ(len, 12);
        CHECK_EQ(fl::string(msg, len), "test message");

        queue.commit();
        CHECK(queue.empty());
        CHECK_EQ(queue.size(), 0);
    }

    SUBCASE("push fl::string variant") {
        AsyncLogQueue<TEST_DESC_COUNT, TEST_ARENA_SIZE> queue;

        fl::string msg = "string test";
        CHECK(queue.push(msg));

        const char* popped_msg;
        fl::u16 len;
        CHECK(queue.tryPop(&popped_msg, &len));
        CHECK_EQ(len, 11);
        CHECK_EQ(fl::string(popped_msg, len), "string test");

        queue.commit();
    }

    SUBCASE("push empty message") {
        AsyncLogQueue<TEST_DESC_COUNT, TEST_ARENA_SIZE> queue;

        CHECK(queue.push(""));  // Empty message accepted but not stored
        CHECK(queue.empty());   // Queue still empty
    }
}

TEST_CASE("fl::AsyncLogQueue - FIFO ordering") {
    SUBCASE("messages pop in FIFO order") {
        AsyncLogQueue<TEST_DESC_COUNT, TEST_ARENA_SIZE> queue;

        CHECK(queue.push("msg1"));
        CHECK(queue.push("msg2"));
        CHECK(queue.push("msg3"));
        CHECK_EQ(queue.size(), 3);

        const char* msg;
        fl::u16 len;

        CHECK(queue.tryPop(&msg, &len));
        CHECK_EQ(fl::string(msg, len), "msg1");
        queue.commit();

        CHECK(queue.tryPop(&msg, &len));
        CHECK_EQ(fl::string(msg, len), "msg2");
        queue.commit();

        CHECK(queue.tryPop(&msg, &len));
        CHECK_EQ(fl::string(msg, len), "msg3");
        queue.commit();

        CHECK(queue.empty());
    }
}

TEST_CASE("fl::AsyncLogQueue - descriptor ring overflow") {
    SUBCASE("descriptor ring full causes drop") {
        AsyncLogQueue<TEST_DESC_COUNT, TEST_ARENA_SIZE> queue;

        // Fill descriptor ring to capacity (N-1 slots)
        for (fl::size i = 0; i < TEST_DESC_COUNT - 1; i++) {
            CHECK(queue.push("x"));  // 1 byte message
        }
        CHECK_EQ(queue.size(), TEST_DESC_COUNT - 1);
        CHECK_EQ(queue.droppedCount(), 0);

        // Next push should fail (descriptor ring full)
        CHECK_FALSE(queue.push("overflow"));
        CHECK_EQ(queue.droppedCount(), 1);
    }

    SUBCASE("can push again after consuming") {
        AsyncLogQueue<TEST_DESC_COUNT, TEST_ARENA_SIZE> queue;

        // Fill to capacity
        for (fl::size i = 0; i < TEST_DESC_COUNT - 1; i++) {
            CHECK(queue.push("x"));
        }

        // Pop one message
        const char* msg;
        fl::u16 len;
        CHECK(queue.tryPop(&msg, &len));
        queue.commit();

        // Now we can push again
        CHECK(queue.push("new"));
        CHECK_EQ(queue.size(), TEST_DESC_COUNT - 1);
    }
}

TEST_CASE("fl::AsyncLogQueue - arena space management") {
    SUBCASE("arena full causes drop") {
        AsyncLogQueue<TEST_DESC_COUNT, TEST_ARENA_SIZE> queue;

        // Push messages until arena is nearly full
        // Arena size = 64, reserve 1 byte for full/empty distinction = 63 usable
        CHECK(queue.push("01234567890123456789012345678901"));  // 32 bytes
        CHECK(queue.push("0123456789012345678901234567890"));   // 31 bytes
        // Total: 63 bytes used

        // Next push should fail (arena full)
        CHECK_FALSE(queue.push("x"));
        CHECK_EQ(queue.droppedCount(), 1);
    }

    SUBCASE("arena space freed after commit") {
        AsyncLogQueue<TEST_DESC_COUNT, TEST_ARENA_SIZE> queue;

        CHECK(queue.push("01234567890123456789012345678901"));  // 32 bytes

        const char* msg;
        fl::u16 len;
        CHECK(queue.tryPop(&msg, &len));
        queue.commit();  // Free 32 bytes

        // Now we can push another 32-byte message
        CHECK(queue.push("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"));
    }
}

TEST_CASE("fl::AsyncLogQueue - arena wraparound with padding") {
    SUBCASE("message that would wrap gets padded") {
        // Use larger arena for this test (256 bytes)
        AsyncLogQueue<16, 256> queue;

        // Push message that goes near end of arena (201 bytes)
        const char* msg1 = "012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
        CHECK(queue.push(msg1));

        // Next message (40 bytes) would wrap if starting at position 201
        // Queue should insert padding (256 - 201 = 55 bytes) and wrap to position 0
        CHECK(queue.push("0123456789012345678901234567890123456789"));  // 40 bytes

        const char* msg;
        fl::u16 len;

        // First message should be at start of arena
        CHECK(queue.tryPop(&msg, &len));
        CHECK_EQ(len, 201);
        queue.commit();

        // Second message should be at position 0 (wrapped)
        CHECK(queue.tryPop(&msg, &len));
        CHECK_EQ(len, 40);
        queue.commit();
    }
}

TEST_CASE("fl::AsyncLogQueue - bounded string length") {
    SUBCASE("truncates string longer than MAX_MESSAGE_LENGTH") {
        AsyncLogQueue<128, 1024> queue;  // Larger queue for this test

        // Create a string longer than MAX_MESSAGE_LENGTH (512)
        fl::string long_msg;
        for (int i = 0; i < 600; i++) {
            long_msg.append("x");
        }

        CHECK(queue.push(long_msg));

        const char* msg;
        fl::u16 len;
        CHECK(queue.tryPop(&msg, &len));

        // Should be truncated to MAX_MESSAGE_LENGTH
        CHECK_EQ(len, AsyncLogQueue<>::MAX_MESSAGE_LENGTH);

        queue.commit();
    }
}

TEST_CASE("fl::AsyncLogQueue - edge cases") {
    SUBCASE("pop from empty queue returns false") {
        AsyncLogQueue<TEST_DESC_COUNT, TEST_ARENA_SIZE> queue;

        const char* msg;
        fl::u16 len;
        CHECK_FALSE(queue.tryPop(&msg, &len));
    }

    SUBCASE("multiple pops without commit") {
        AsyncLogQueue<TEST_DESC_COUNT, TEST_ARENA_SIZE> queue;

        CHECK(queue.push("msg1"));
        CHECK(queue.push("msg2"));

        const char* msg;
        fl::u16 len;

        // Pop first message
        CHECK(queue.tryPop(&msg, &len));
        CHECK_EQ(fl::string(msg, len), "msg1");

        // Pop again without commit should return same message
        CHECK(queue.tryPop(&msg, &len));
        CHECK_EQ(fl::string(msg, len), "msg1");

        // Commit and pop should get second message
        queue.commit();
        CHECK(queue.tryPop(&msg, &len));
        CHECK_EQ(fl::string(msg, len), "msg2");
    }

    SUBCASE("push after pop without commit") {
        AsyncLogQueue<TEST_DESC_COUNT, TEST_ARENA_SIZE> queue;

        CHECK(queue.push("msg1"));

        const char* msg;
        fl::u16 len;
        CHECK(queue.tryPop(&msg, &len));

        // Push another message before commit
        CHECK(queue.push("msg2"));

        // Popped message should still be msg1
        CHECK(queue.tryPop(&msg, &len));
        CHECK_EQ(fl::string(msg, len), "msg1");
    }
}

TEST_CASE("fl::AsyncLogQueue - drop counter") {
    SUBCASE("drop counter increments on overflow") {
        AsyncLogQueue<TEST_DESC_COUNT, TEST_ARENA_SIZE> queue;

        // Fill queue
        for (fl::size i = 0; i < TEST_DESC_COUNT - 1; i++) {
            CHECK(queue.push("x"));
        }

        // Overflow multiple times
        for (int i = 0; i < 5; i++) {
            CHECK_FALSE(queue.push("overflow"));
        }

        CHECK_EQ(queue.droppedCount(), 5);
    }

    SUBCASE("drop counter persists across pops") {
        AsyncLogQueue<TEST_DESC_COUNT, TEST_ARENA_SIZE> queue;

        CHECK_FALSE(queue.push("0123456789012345678901234567890123456789012345678901234567890123"));  // Too big for arena
        CHECK_EQ(queue.droppedCount(), 1);

        CHECK(queue.push("small"));

        const char* msg;
        fl::u16 len;
        queue.tryPop(&msg, &len);
        queue.commit();

        // Drop counter should still be 1
        CHECK_EQ(queue.droppedCount(), 1);
    }
}

TEST_CASE("fl::AsyncLogQueue - stress test") {
    SUBCASE("many push/pop cycles") {
        AsyncLogQueue<128, 1024> queue;

        for (int iteration = 0; iteration < 10; iteration++) {
            // Fill queue partially
            for (int i = 0; i < 50; i++) {
                fl::sstream ss;
                ss << "iter" << iteration << "_msg" << i;
                CHECK(queue.push(ss.str()));
            }

            // Drain queue
            int popped = 0;
            const char* msg;
            fl::u16 len;
            while (queue.tryPop(&msg, &len)) {
                queue.commit();
                popped++;
            }

            CHECK_EQ(popped, 50);
            CHECK(queue.empty());
        }

        CHECK_EQ(queue.droppedCount(), 0);
    }
}

TEST_CASE("fl::AsyncLogQueue - default template parameters") {
    SUBCASE("default constructor uses 128 descriptors and 4096 arena") {
        AsyncLogQueue<> queue;

        CHECK_EQ(queue.capacity(), 127);  // 128 - 1
        CHECK(queue.empty());
    }
}
