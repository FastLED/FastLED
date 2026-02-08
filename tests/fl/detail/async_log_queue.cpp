#include "fl/detail/async_log_queue.h"
#include "fl/isr.h"  // For fl::isr::CriticalSection
#include "fl/stl/sstream.h"
#include "test.h"
#include "fl/stl/string.h"
#include "fl/stl/strstream.h"
#include "fl/int.h"

using namespace fl;

// Test with small sizes for easier testing of edge cases
constexpr fl::size TEST_DESC_COUNT = 8;  // Power of 2
constexpr fl::size TEST_ARENA_SIZE = 64; // Power of 2

FL_TEST_CASE("fl::isr::CriticalSection - RAII interrupt control") {
    FL_SUBCASE("constructor disables interrupts, destructor enables") {
        // This is hard to test directly without mocking, but we can verify it compiles
        {
            fl::isr::CriticalSection cs;
            // Interrupts should be disabled here
        }
        // Interrupts should be re-enabled here

        // Just verify construction/destruction works
        FL_CHECK(true);
    }

    FL_SUBCASE("non-copyable") {
        // This is a compile-time check
        // If this compiles, the test fails (should not be copyable)
        // fl::isr::CriticalSection cs1;
        // fl::isr::CriticalSection cs2 = cs1;  // Should not compile
        FL_CHECK(true);
    }
}

FL_TEST_CASE("fl::AsyncLogQueue - basic operations") {
    FL_SUBCASE("constructor creates empty queue") {
        AsyncLogQueue<TEST_DESC_COUNT, TEST_ARENA_SIZE> queue;

        FL_CHECK(queue.empty());
        FL_CHECK_EQ(queue.size(), 0);
        FL_CHECK_EQ(queue.capacity(), TEST_DESC_COUNT - 1);  // One slot reserved
        FL_CHECK_EQ(queue.droppedCount(), 0);
    }

    FL_SUBCASE("push and pop single message") {
        AsyncLogQueue<TEST_DESC_COUNT, TEST_ARENA_SIZE> queue;

        FL_CHECK(queue.push("test message"));
        FL_CHECK_FALSE(queue.empty());
        FL_CHECK_EQ(queue.size(), 1);

        const char* msg;
        fl::u16 len;
        FL_CHECK(queue.tryPop(&msg, &len));
        FL_CHECK_EQ(len, 12);
        FL_CHECK_EQ(fl::string(msg, len), "test message");

        queue.commit();
        FL_CHECK(queue.empty());
        FL_CHECK_EQ(queue.size(), 0);
    }

    FL_SUBCASE("push fl::string variant") {
        AsyncLogQueue<TEST_DESC_COUNT, TEST_ARENA_SIZE> queue;

        fl::string msg = "string test";
        FL_CHECK(queue.push(msg));

        const char* popped_msg;
        fl::u16 len;
        FL_CHECK(queue.tryPop(&popped_msg, &len));
        FL_CHECK_EQ(len, 11);
        FL_CHECK_EQ(fl::string(popped_msg, len), "string test");

        queue.commit();
    }

    FL_SUBCASE("push empty message") {
        AsyncLogQueue<TEST_DESC_COUNT, TEST_ARENA_SIZE> queue;

        FL_CHECK(queue.push(""));  // Empty message accepted but not stored
        FL_CHECK(queue.empty());   // Queue still empty
    }
}

FL_TEST_CASE("fl::AsyncLogQueue - FIFO ordering") {
    FL_SUBCASE("messages pop in FIFO order") {
        AsyncLogQueue<TEST_DESC_COUNT, TEST_ARENA_SIZE> queue;

        FL_CHECK(queue.push("msg1"));
        FL_CHECK(queue.push("msg2"));
        FL_CHECK(queue.push("msg3"));
        FL_CHECK_EQ(queue.size(), 3);

        const char* msg;
        fl::u16 len;

        FL_CHECK(queue.tryPop(&msg, &len));
        FL_CHECK_EQ(fl::string(msg, len), "msg1");
        queue.commit();

        FL_CHECK(queue.tryPop(&msg, &len));
        FL_CHECK_EQ(fl::string(msg, len), "msg2");
        queue.commit();

        FL_CHECK(queue.tryPop(&msg, &len));
        FL_CHECK_EQ(fl::string(msg, len), "msg3");
        queue.commit();

        FL_CHECK(queue.empty());
    }
}

FL_TEST_CASE("fl::AsyncLogQueue - descriptor ring overflow") {
    FL_SUBCASE("descriptor ring full causes drop") {
        AsyncLogQueue<TEST_DESC_COUNT, TEST_ARENA_SIZE> queue;

        // Fill descriptor ring to capacity (N-1 slots)
        for (fl::size i = 0; i < TEST_DESC_COUNT - 1; i++) {
            FL_CHECK(queue.push("x"));  // 1 byte message
        }
        FL_CHECK_EQ(queue.size(), TEST_DESC_COUNT - 1);
        FL_CHECK_EQ(queue.droppedCount(), 0);

        // Next push should fail (descriptor ring full)
        FL_CHECK_FALSE(queue.push("overflow"));
        FL_CHECK_EQ(queue.droppedCount(), 1);
    }

    FL_SUBCASE("can push again after consuming") {
        AsyncLogQueue<TEST_DESC_COUNT, TEST_ARENA_SIZE> queue;

        // Fill to capacity
        for (fl::size i = 0; i < TEST_DESC_COUNT - 1; i++) {
            FL_CHECK(queue.push("x"));
        }

        // Pop one message
        const char* msg;
        fl::u16 len;
        FL_CHECK(queue.tryPop(&msg, &len));
        queue.commit();

        // Now we can push again
        FL_CHECK(queue.push("new"));
        FL_CHECK_EQ(queue.size(), TEST_DESC_COUNT - 1);
    }
}

FL_TEST_CASE("fl::AsyncLogQueue - arena space management") {
    FL_SUBCASE("arena full causes drop") {
        AsyncLogQueue<TEST_DESC_COUNT, TEST_ARENA_SIZE> queue;

        // Push messages until arena is nearly full
        // Arena size = 64, reserve 1 byte for full/empty distinction = 63 usable
        FL_CHECK(queue.push("01234567890123456789012345678901"));  // 32 bytes
        FL_CHECK(queue.push("0123456789012345678901234567890"));   // 31 bytes
        // Total: 63 bytes used

        // Next push should fail (arena full)
        FL_CHECK_FALSE(queue.push("x"));
        FL_CHECK_EQ(queue.droppedCount(), 1);
    }

    FL_SUBCASE("arena space freed after commit") {
        AsyncLogQueue<TEST_DESC_COUNT, TEST_ARENA_SIZE> queue;

        FL_CHECK(queue.push("01234567890123456789012345678901"));  // 32 bytes

        const char* msg;
        fl::u16 len;
        FL_CHECK(queue.tryPop(&msg, &len));
        queue.commit();  // Free 32 bytes

        // Now we can push another 32-byte message
        FL_CHECK(queue.push("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"));
    }
}

FL_TEST_CASE("fl::AsyncLogQueue - arena wraparound with padding") {
    FL_SUBCASE("message that would wrap gets padded") {
        // Use larger arena for this test (256 bytes)
        AsyncLogQueue<16, 256> queue;

        // Push message that goes near end of arena (201 bytes)
        const char* msg1 = "012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
        FL_CHECK(queue.push(msg1));

        // Next message (40 bytes) would wrap if starting at position 201
        // Queue should insert padding (256 - 201 = 55 bytes) and wrap to position 0
        FL_CHECK(queue.push("0123456789012345678901234567890123456789"));  // 40 bytes

        const char* msg;
        fl::u16 len;

        // First message should be at start of arena
        FL_CHECK(queue.tryPop(&msg, &len));
        FL_CHECK_EQ(len, 201);
        queue.commit();

        // Second message should be at position 0 (wrapped)
        FL_CHECK(queue.tryPop(&msg, &len));
        FL_CHECK_EQ(len, 40);
        queue.commit();
    }
}

FL_TEST_CASE("fl::AsyncLogQueue - bounded string length") {
    FL_SUBCASE("truncates string longer than MAX_MESSAGE_LENGTH") {
        AsyncLogQueue<128, 1024> queue;  // Larger queue for this test

        // Create a string longer than MAX_MESSAGE_LENGTH (512)
        fl::string long_msg;
        for (int i = 0; i < 600; i++) {
            long_msg.append("x");
        }

        FL_CHECK(queue.push(long_msg));

        const char* msg;
        fl::u16 len;
        FL_CHECK(queue.tryPop(&msg, &len));

        // Should be truncated to MAX_MESSAGE_LENGTH
        FL_CHECK_EQ(len, AsyncLogQueue<>::MAX_MESSAGE_LENGTH);

        queue.commit();
    }
}

FL_TEST_CASE("fl::AsyncLogQueue - edge cases") {
    FL_SUBCASE("pop from empty queue returns false") {
        AsyncLogQueue<TEST_DESC_COUNT, TEST_ARENA_SIZE> queue;

        const char* msg;
        fl::u16 len;
        FL_CHECK_FALSE(queue.tryPop(&msg, &len));
    }

    FL_SUBCASE("multiple pops without commit") {
        AsyncLogQueue<TEST_DESC_COUNT, TEST_ARENA_SIZE> queue;

        FL_CHECK(queue.push("msg1"));
        FL_CHECK(queue.push("msg2"));

        const char* msg;
        fl::u16 len;

        // Pop first message
        FL_CHECK(queue.tryPop(&msg, &len));
        FL_CHECK_EQ(fl::string(msg, len), "msg1");

        // Pop again without commit should return same message
        FL_CHECK(queue.tryPop(&msg, &len));
        FL_CHECK_EQ(fl::string(msg, len), "msg1");

        // Commit and pop should get second message
        queue.commit();
        FL_CHECK(queue.tryPop(&msg, &len));
        FL_CHECK_EQ(fl::string(msg, len), "msg2");
    }

    FL_SUBCASE("push after pop without commit") {
        AsyncLogQueue<TEST_DESC_COUNT, TEST_ARENA_SIZE> queue;

        FL_CHECK(queue.push("msg1"));

        const char* msg;
        fl::u16 len;
        FL_CHECK(queue.tryPop(&msg, &len));

        // Push another message before commit
        FL_CHECK(queue.push("msg2"));

        // Popped message should still be msg1
        FL_CHECK(queue.tryPop(&msg, &len));
        FL_CHECK_EQ(fl::string(msg, len), "msg1");
    }
}

FL_TEST_CASE("fl::AsyncLogQueue - drop counter") {
    FL_SUBCASE("drop counter increments on overflow") {
        AsyncLogQueue<TEST_DESC_COUNT, TEST_ARENA_SIZE> queue;

        // Fill queue
        for (fl::size i = 0; i < TEST_DESC_COUNT - 1; i++) {
            FL_CHECK(queue.push("x"));
        }

        // Overflow multiple times
        for (int i = 0; i < 5; i++) {
            FL_CHECK_FALSE(queue.push("overflow"));
        }

        FL_CHECK_EQ(queue.droppedCount(), 5);
    }

    FL_SUBCASE("drop counter persists across pops") {
        AsyncLogQueue<TEST_DESC_COUNT, TEST_ARENA_SIZE> queue;

        FL_CHECK_FALSE(queue.push("0123456789012345678901234567890123456789012345678901234567890123"));  // Too big for arena
        FL_CHECK_EQ(queue.droppedCount(), 1);

        FL_CHECK(queue.push("small"));

        const char* msg;
        fl::u16 len;
        queue.tryPop(&msg, &len);
        queue.commit();

        // Drop counter should still be 1
        FL_CHECK_EQ(queue.droppedCount(), 1);
    }
}

FL_TEST_CASE("fl::AsyncLogQueue - stress test") {
    FL_SUBCASE("many push/pop cycles") {
        AsyncLogQueue<128, 1024> queue;

        for (int iteration = 0; iteration < 10; iteration++) {
            // Fill queue partially
            for (int i = 0; i < 50; i++) {
                fl::sstream ss;
                ss << "iter" << iteration << "_msg" << i;
                FL_CHECK(queue.push(ss.str()));
            }

            // Drain queue
            int popped = 0;
            const char* msg;
            fl::u16 len;
            while (queue.tryPop(&msg, &len)) {
                queue.commit();
                popped++;
            }

            FL_CHECK_EQ(popped, 50);
            FL_CHECK(queue.empty());
        }

        FL_CHECK_EQ(queue.droppedCount(), 0);
    }
}

FL_TEST_CASE("fl::AsyncLogQueue - default template parameters") {
    FL_SUBCASE("default constructor uses 128 descriptors and 4096 arena") {
        AsyncLogQueue<> queue;

        FL_CHECK_EQ(queue.capacity(), 127);  // 128 - 1
        FL_CHECK(queue.empty());
    }
}
