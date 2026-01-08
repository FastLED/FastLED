#include "fl/stl/atomic.h"
#include "doctest.h"
#include "platforms/shared/atomic.h"

using namespace fl;

// Test both AtomicFake (single-threaded) and AtomicReal (multi-threaded)
// Note: Only test the common interface that both implementations support:
// - load(), store()
// - operator++, operator-- (pre-increment/decrement only)
// - fetch_add(), fetch_sub()
// - operator=, operator T() (conversion)

TEST_CASE("fl::atomic<int> - basic construction and initialization") {
    SUBCASE("default constructor initializes to zero") {
        atomic_int a;
        CHECK_EQ(a.load(), 0);
    }

    SUBCASE("value constructor initializes to given value") {
        atomic_int a(42);
        CHECK_EQ(a.load(), 42);
    }

    SUBCASE("conversion operator works") {
        atomic_int a(100);
        int value = a;  // Implicit conversion
        CHECK_EQ(value, 100);
    }
}

TEST_CASE("fl::atomic<int> - store and load operations") {
    atomic_int a;

    SUBCASE("store and load basic value") {
        a.store(123);
        CHECK_EQ(a.load(), 123);
    }

    SUBCASE("store negative value") {
        a.store(-456);
        CHECK_EQ(a.load(), -456);
    }

    SUBCASE("multiple stores") {
        a.store(10);
        CHECK_EQ(a.load(), 10);
        a.store(20);
        CHECK_EQ(a.load(), 20);
        a.store(30);
        CHECK_EQ(a.load(), 30);
    }

    SUBCASE("store with memory order parameters") {
        a.store(100, memory_order_relaxed);
        CHECK_EQ(a.load(memory_order_relaxed), 100);

        a.store(200, memory_order_release);
        CHECK_EQ(a.load(memory_order_acquire), 200);

        a.store(300, memory_order_seq_cst);
        CHECK_EQ(a.load(memory_order_seq_cst), 300);
    }
}

TEST_CASE("fl::atomic<int> - assignment operator") {
    atomic_int a;

    SUBCASE("assignment stores value") {
        a = 42;
        CHECK_EQ(a.load(), 42);
    }

    SUBCASE("assignment returns assigned value") {
        int result = (a = 123);
        CHECK_EQ(result, 123);
        CHECK_EQ(a.load(), 123);
    }

    SUBCASE("chained assignment") {
        atomic_int b;
        b = a = 99;
        CHECK_EQ(a.load(), 99);
        CHECK_EQ(b.load(), 99);
    }
}

TEST_CASE("fl::atomic<int> - pre-increment operator") {
    atomic_int a(10);

    SUBCASE("pre-increment") {
        int result = ++a;
        CHECK_EQ(result, 11);
        CHECK_EQ(a.load(), 11);
    }

    SUBCASE("multiple increments") {
        ++a;
        ++a;
        ++a;
        CHECK_EQ(a.load(), 13);
    }
}

TEST_CASE("fl::atomic<int> - pre-decrement operator") {
    atomic_int a(10);

    SUBCASE("pre-decrement") {
        int result = --a;
        CHECK_EQ(result, 9);
        CHECK_EQ(a.load(), 9);
    }

    SUBCASE("multiple decrements") {
        --a;
        --a;
        --a;
        CHECK_EQ(a.load(), 7);
    }
}

TEST_CASE("fl::atomic<int> - fetch operations") {
    SUBCASE("fetch_add returns old value and adds") {
        atomic_int a(10);
        int old = a.fetch_add(5);
        CHECK_EQ(old, 10);
        CHECK_EQ(a.load(), 15);
    }

    SUBCASE("fetch_sub returns old value and subtracts") {
        atomic_int a(20);
        int old = a.fetch_sub(7);
        CHECK_EQ(old, 20);
        CHECK_EQ(a.load(), 13);
    }

    SUBCASE("fetch_add with negative value") {
        atomic_int a(50);
        int old = a.fetch_add(-10);
        CHECK_EQ(old, 50);
        CHECK_EQ(a.load(), 40);
    }

    SUBCASE("fetch_sub with negative value") {
        atomic_int a(30);
        int old = a.fetch_sub(-5);
        CHECK_EQ(old, 30);
        CHECK_EQ(a.load(), 35);
    }
}

TEST_CASE("fl::atomic_bool - boolean atomic operations") {
    SUBCASE("default constructor initializes to false") {
        atomic_bool a;
        CHECK(!a.load());
    }

    SUBCASE("value constructor") {
        atomic_bool a(true);
        CHECK(a.load());
    }

    SUBCASE("store and load") {
        atomic_bool a;
        a.store(true);
        CHECK(a.load());
        a.store(false);
        CHECK(!a.load());
    }

    SUBCASE("assignment") {
        atomic_bool a;
        a = true;
        CHECK(a.load());
    }

    SUBCASE("conversion operator") {
        atomic_bool a(true);
        if (a) {  // Uses conversion operator
            CHECK(true);
        } else {
            CHECK(false);
        }
    }
}

TEST_CASE("fl::atomic<unsigned int> - unsigned atomic operations") {
    atomic_uint a(100u);

    SUBCASE("basic operations") {
        CHECK_EQ(a.load(), 100u);
        a.store(200u);
        CHECK_EQ(a.load(), 200u);
    }

    SUBCASE("pre-increment and pre-decrement") {
        ++a;
        CHECK_EQ(a.load(), 101u);
        --a;
        CHECK_EQ(a.load(), 100u);
    }

    SUBCASE("fetch operations") {
        unsigned old = a.fetch_add(10u);
        CHECK_EQ(old, 100u);
        CHECK_EQ(a.load(), 110u);
    }
}

TEST_CASE("fl::atomic_u32 and atomic_i32 - typed atomics") {
    SUBCASE("atomic_u32 operations") {
        atomic_u32 a(42u);
        CHECK_EQ(a.load(), 42u);
        a.store(84u);
        CHECK_EQ(a.load(), 84u);
    }

    SUBCASE("atomic_i32 operations") {
        atomic_i32 a(-42);
        CHECK_EQ(a.load(), -42);
        a.store(42);
        CHECK_EQ(a.load(), 42);
    }
}


TEST_CASE("fl::atomic - edge cases and special values") {
    SUBCASE("zero value") {
        atomic_int a(0);
        CHECK_EQ(a.load(), 0);
        ++a;
        CHECK_EQ(a.load(), 1);
        --a;
        CHECK_EQ(a.load(), 0);
    }

    SUBCASE("negative values") {
        atomic_int a(-100);
        CHECK_EQ(a.load(), -100);
        a.fetch_add(50);
        CHECK_EQ(a.load(), -50);
        a.fetch_add(100);
        CHECK_EQ(a.load(), 50);
    }

    SUBCASE("maximum and minimum int values") {
        atomic_int a_max(2147483647);  // INT_MAX
        CHECK_EQ(a_max.load(), 2147483647);

        atomic_int a_min(-2147483647 - 1);  // INT_MIN
        CHECK_EQ(a_min.load(), -2147483647 - 1);
    }
}

TEST_CASE("fl::atomic - type traits and properties") {
    SUBCASE("atomic types are not copyable") {
        // This test verifies at compile time that atomics are not copyable
        // Uncommenting these should cause compilation errors:
        // atomic_int a(10);
        // atomic_int b(a);  // Should not compile
        // atomic_int c = a; // Should not compile
        CHECK(true);  // Dummy check since this is a compile-time test
    }

    SUBCASE("atomic types are not movable") {
        // This test verifies at compile time that atomics are not movable
        // Uncommenting these should cause compilation errors:
        // atomic_int a(10);
        // atomic_int b(fl::move(a));  // Should not compile
        // atomic_int c = fl::move(a); // Should not compile
        CHECK(true);  // Dummy check since this is a compile-time test
    }
}

TEST_CASE("fl::memory_order - memory order enum values exist") {
    // Test that memory_order enum values are defined
    SUBCASE("memory order constants are available") {
        memory_order mo1 = memory_order_relaxed;
        memory_order mo2 = memory_order_acquire;
        memory_order mo3 = memory_order_release;
        memory_order mo4 = memory_order_acq_rel;
        memory_order mo5 = memory_order_seq_cst;

        // Just verify they compile and can be assigned
        (void)mo1; (void)mo2; (void)mo3; (void)mo4; (void)mo5;
        CHECK(true);
    }
}

TEST_CASE("fl::atomic - complex usage patterns") {
    SUBCASE("using atomic as counter") {
        atomic_int counter(0);
        for (int i = 0; i < 100; ++i) {
            ++counter;
        }
        CHECK_EQ(counter.load(), 100);
    }

    SUBCASE("using atomic as flag") {
        atomic_bool flag(false);
        flag.store(true);
        if (flag) {
            CHECK(true);
        }
        flag = false;
        CHECK(!flag.load());
    }

    SUBCASE("accumulator pattern with fetch_add") {
        atomic_int total(0);
        for (int i = 1; i <= 10; ++i) {
            total.fetch_add(i);
        }
        CHECK_EQ(total.load(), 55);  // Sum of 1..10
    }

    SUBCASE("countdown pattern with fetch_sub") {
        atomic_int countdown(100);
        for (int i = 0; i < 10; ++i) {
            countdown.fetch_sub(10);
        }
        CHECK_EQ(countdown.load(), 0);
    }
}
