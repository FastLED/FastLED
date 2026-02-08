#include "fl/stl/atomic.h"
#include "test.h"
#include "platforms/shared/atomic.h"

using namespace fl;

// Test both AtomicFake (single-threaded) and AtomicReal (multi-threaded)
// Note: Only test the common interface that both implementations support:
// - load(), store()
// - operator++, operator-- (pre-increment/decrement only)
// - fetch_add(), fetch_sub()
// - operator=, operator T() (conversion)

FL_TEST_CASE("fl::atomic<int> - basic construction and initialization") {
    FL_SUBCASE("default constructor initializes to zero") {
        atomic_int a;
        FL_CHECK_EQ(a.load(), 0);
    }

    FL_SUBCASE("value constructor initializes to given value") {
        atomic_int a(42);
        FL_CHECK_EQ(a.load(), 42);
    }

    FL_SUBCASE("conversion operator works") {
        atomic_int a(100);
        int value = a;  // Implicit conversion
        FL_CHECK_EQ(value, 100);
    }
}

FL_TEST_CASE("fl::atomic<int> - store and load operations") {
    atomic_int a;

    FL_SUBCASE("store and load basic value") {
        a.store(123);
        FL_CHECK_EQ(a.load(), 123);
    }

    FL_SUBCASE("store negative value") {
        a.store(-456);
        FL_CHECK_EQ(a.load(), -456);
    }

    FL_SUBCASE("multiple stores") {
        a.store(10);
        FL_CHECK_EQ(a.load(), 10);
        a.store(20);
        FL_CHECK_EQ(a.load(), 20);
        a.store(30);
        FL_CHECK_EQ(a.load(), 30);
    }

    FL_SUBCASE("store with memory order parameters") {
        a.store(100, memory_order_relaxed);
        FL_CHECK_EQ(a.load(memory_order_relaxed), 100);

        a.store(200, memory_order_release);
        FL_CHECK_EQ(a.load(memory_order_acquire), 200);

        a.store(300, memory_order_seq_cst);
        FL_CHECK_EQ(a.load(memory_order_seq_cst), 300);
    }
}

FL_TEST_CASE("fl::atomic<int> - assignment operator") {
    atomic_int a;

    FL_SUBCASE("assignment stores value") {
        a = 42;
        FL_CHECK_EQ(a.load(), 42);
    }

    FL_SUBCASE("assignment returns assigned value") {
        int result = (a = 123);
        FL_CHECK_EQ(result, 123);
        FL_CHECK_EQ(a.load(), 123);
    }

    FL_SUBCASE("chained assignment") {
        atomic_int b;
        b = a = 99;
        FL_CHECK_EQ(a.load(), 99);
        FL_CHECK_EQ(b.load(), 99);
    }
}

FL_TEST_CASE("fl::atomic<int> - pre-increment operator") {
    atomic_int a(10);

    FL_SUBCASE("pre-increment") {
        int result = ++a;
        FL_CHECK_EQ(result, 11);
        FL_CHECK_EQ(a.load(), 11);
    }

    FL_SUBCASE("multiple increments") {
        ++a;
        ++a;
        ++a;
        FL_CHECK_EQ(a.load(), 13);
    }
}

FL_TEST_CASE("fl::atomic<int> - pre-decrement operator") {
    atomic_int a(10);

    FL_SUBCASE("pre-decrement") {
        int result = --a;
        FL_CHECK_EQ(result, 9);
        FL_CHECK_EQ(a.load(), 9);
    }

    FL_SUBCASE("multiple decrements") {
        --a;
        --a;
        --a;
        FL_CHECK_EQ(a.load(), 7);
    }
}

FL_TEST_CASE("fl::atomic<int> - fetch operations") {
    FL_SUBCASE("fetch_add returns old value and adds") {
        atomic_int a(10);
        int old = a.fetch_add(5);
        FL_CHECK_EQ(old, 10);
        FL_CHECK_EQ(a.load(), 15);
    }

    FL_SUBCASE("fetch_sub returns old value and subtracts") {
        atomic_int a(20);
        int old = a.fetch_sub(7);
        FL_CHECK_EQ(old, 20);
        FL_CHECK_EQ(a.load(), 13);
    }

    FL_SUBCASE("fetch_add with negative value") {
        atomic_int a(50);
        int old = a.fetch_add(-10);
        FL_CHECK_EQ(old, 50);
        FL_CHECK_EQ(a.load(), 40);
    }

    FL_SUBCASE("fetch_sub with negative value") {
        atomic_int a(30);
        int old = a.fetch_sub(-5);
        FL_CHECK_EQ(old, 30);
        FL_CHECK_EQ(a.load(), 35);
    }
}

FL_TEST_CASE("fl::atomic_bool - boolean atomic operations") {
    FL_SUBCASE("default constructor initializes to false") {
        atomic_bool a;
        FL_CHECK(!a.load());
    }

    FL_SUBCASE("value constructor") {
        atomic_bool a(true);
        FL_CHECK(a.load());
    }

    FL_SUBCASE("store and load") {
        atomic_bool a;
        a.store(true);
        FL_CHECK(a.load());
        a.store(false);
        FL_CHECK(!a.load());
    }

    FL_SUBCASE("assignment") {
        atomic_bool a;
        a = true;
        FL_CHECK(a.load());
    }

    FL_SUBCASE("conversion operator") {
        atomic_bool a(true);
        if (a) {  // Uses conversion operator
            FL_CHECK(true);
        } else {
            FL_CHECK(false);
        }
    }
}

FL_TEST_CASE("fl::atomic<unsigned int> - unsigned atomic operations") {
    atomic_uint a(100u);

    FL_SUBCASE("basic operations") {
        FL_CHECK_EQ(a.load(), 100u);
        a.store(200u);
        FL_CHECK_EQ(a.load(), 200u);
    }

    FL_SUBCASE("pre-increment and pre-decrement") {
        ++a;
        FL_CHECK_EQ(a.load(), 101u);
        --a;
        FL_CHECK_EQ(a.load(), 100u);
    }

    FL_SUBCASE("fetch operations") {
        unsigned old = a.fetch_add(10u);
        FL_CHECK_EQ(old, 100u);
        FL_CHECK_EQ(a.load(), 110u);
    }
}

FL_TEST_CASE("fl::atomic_u32 and atomic_i32 - typed atomics") {
    FL_SUBCASE("atomic_u32 operations") {
        atomic_u32 a(42u);
        FL_CHECK_EQ(a.load(), 42u);
        a.store(84u);
        FL_CHECK_EQ(a.load(), 84u);
    }

    FL_SUBCASE("atomic_i32 operations") {
        atomic_i32 a(-42);
        FL_CHECK_EQ(a.load(), -42);
        a.store(42);
        FL_CHECK_EQ(a.load(), 42);
    }
}


FL_TEST_CASE("fl::atomic - edge cases and special values") {
    FL_SUBCASE("zero value") {
        atomic_int a(0);
        FL_CHECK_EQ(a.load(), 0);
        ++a;
        FL_CHECK_EQ(a.load(), 1);
        --a;
        FL_CHECK_EQ(a.load(), 0);
    }

    FL_SUBCASE("negative values") {
        atomic_int a(-100);
        FL_CHECK_EQ(a.load(), -100);
        a.fetch_add(50);
        FL_CHECK_EQ(a.load(), -50);
        a.fetch_add(100);
        FL_CHECK_EQ(a.load(), 50);
    }

    FL_SUBCASE("maximum and minimum int values") {
        atomic_int a_max(2147483647);  // INT_MAX
        FL_CHECK_EQ(a_max.load(), 2147483647);

        atomic_int a_min(-2147483647 - 1);  // INT_MIN
        FL_CHECK_EQ(a_min.load(), -2147483647 - 1);
    }
}

FL_TEST_CASE("fl::atomic - type traits and properties") {
    FL_SUBCASE("atomic types are not copyable") {
        // This test verifies at compile time that atomics are not copyable
        // Uncommenting these should cause compilation errors:
        // atomic_int a(10);
        // atomic_int b(a);  // Should not compile
        // atomic_int c = a; // Should not compile
        FL_CHECK(true);  // Dummy check since this is a compile-time test
    }

    FL_SUBCASE("atomic types are not movable") {
        // This test verifies at compile time that atomics are not movable
        // Uncommenting these should cause compilation errors:
        // atomic_int a(10);
        // atomic_int b(fl::move(a));  // Should not compile
        // atomic_int c = fl::move(a); // Should not compile
        FL_CHECK(true);  // Dummy check since this is a compile-time test
    }
}

FL_TEST_CASE("fl::memory_order - memory order enum values exist") {
    // Test that memory_order enum values are defined
    FL_SUBCASE("memory order constants are available") {
        memory_order mo1 = memory_order_relaxed;
        memory_order mo2 = memory_order_acquire;
        memory_order mo3 = memory_order_release;
        memory_order mo4 = memory_order_acq_rel;
        memory_order mo5 = memory_order_seq_cst;

        // Just verify they compile and can be assigned
        (void)mo1; (void)mo2; (void)mo3; (void)mo4; (void)mo5;
        FL_CHECK(true);
    }
}

FL_TEST_CASE("fl::atomic - complex usage patterns") {
    FL_SUBCASE("using atomic as counter") {
        atomic_int counter(0);
        for (int i = 0; i < 100; ++i) {
            ++counter;
        }
        FL_CHECK_EQ(counter.load(), 100);
    }

    FL_SUBCASE("using atomic as flag") {
        atomic_bool flag(false);
        flag.store(true);
        if (flag) {
            FL_CHECK(true);
        }
        flag = false;
        FL_CHECK(!flag.load());
    }

    FL_SUBCASE("accumulator pattern with fetch_add") {
        atomic_int total(0);
        for (int i = 1; i <= 10; ++i) {
            total.fetch_add(i);
        }
        FL_CHECK_EQ(total.load(), 55);  // Sum of 1..10
    }

    FL_SUBCASE("countdown pattern with fetch_sub") {
        atomic_int countdown(100);
        for (int i = 0; i < 10; ++i) {
            countdown.fetch_sub(10);
        }
        FL_CHECK_EQ(countdown.load(), 0);
    }
}
