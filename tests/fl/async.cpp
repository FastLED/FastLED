#include "fl/async.h"
#include "fl/task.h"
#include "fl/promise.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/promise_result.h"
#include "fl/stl/function.h"
#include "fl/stl/move.h"
#include "fl/stl/string.h"

using namespace fl;

// Test helper: A simple test async runner
class TestAsyncRunner : public fl::async_runner {
public:
    TestAsyncRunner() : update_count(0), active(false), task_count(0) {}

    void update() override {
        update_count++;
    }

    bool has_active_tasks() const override {
        return active;
    }

    size_t active_task_count() const override {
        return task_count;
    }

    void set_active(bool value) { active = value; }
    void set_task_count(size_t count) { task_count = count; }

    int update_count;

private:
    bool active;
    size_t task_count;
};

TEST_CASE("fl::async_runner interface") {
    SUBCASE("basic implementation") {
        TestAsyncRunner runner;

        CHECK_EQ(runner.update_count, 0);
        CHECK_EQ(runner.has_active_tasks(), false);
        CHECK_EQ(runner.active_task_count(), 0);

        runner.update();
        CHECK_EQ(runner.update_count, 1);

        runner.set_active(true);
        runner.set_task_count(5);
        CHECK_EQ(runner.has_active_tasks(), true);
        CHECK_EQ(runner.active_task_count(), 5);
    }
}

TEST_CASE("fl::AsyncManager") {
    SUBCASE("singleton instance") {
        AsyncManager& mgr1 = AsyncManager::instance();
        AsyncManager& mgr2 = AsyncManager::instance();
        CHECK_EQ(&mgr1, &mgr2);
    }

    SUBCASE("register and unregister runners") {
        AsyncManager& mgr = AsyncManager::instance();
        TestAsyncRunner runner1;
        TestAsyncRunner runner2;

        // Register runners
        mgr.register_runner(&runner1);
        mgr.register_runner(&runner2);

        // Verify they're registered by calling update
        mgr.update_all();
        CHECK_EQ(runner1.update_count, 1);
        CHECK_EQ(runner2.update_count, 1);

        // Unregister one
        mgr.unregister_runner(&runner1);
        mgr.update_all();
        CHECK_EQ(runner1.update_count, 1); // Not updated again
        CHECK_EQ(runner2.update_count, 2); // Updated again

        // Cleanup
        mgr.unregister_runner(&runner2);
    }

    SUBCASE("duplicate registration") {
        AsyncManager& mgr = AsyncManager::instance();
        TestAsyncRunner runner;

        // Register same runner multiple times
        mgr.register_runner(&runner);
        mgr.register_runner(&runner);
        mgr.register_runner(&runner);

        // Should only be registered once
        mgr.update_all();
        CHECK_EQ(runner.update_count, 1);

        // Cleanup
        mgr.unregister_runner(&runner);
    }

    SUBCASE("null runner handling") {
        AsyncManager& mgr = AsyncManager::instance();

        // Should handle null gracefully
        mgr.register_runner(nullptr);
        mgr.unregister_runner(nullptr);
        mgr.update_all(); // Should not crash
    }

    SUBCASE("has_active_tasks") {
        AsyncManager& mgr = AsyncManager::instance();
        TestAsyncRunner runner1;
        TestAsyncRunner runner2;

        mgr.register_runner(&runner1);
        mgr.register_runner(&runner2);

        runner1.set_active(false);
        runner2.set_active(false);
        CHECK_EQ(mgr.has_active_tasks(), false);

        runner1.set_active(true);
        CHECK_EQ(mgr.has_active_tasks(), true);

        runner1.set_active(false);
        runner2.set_active(true);
        CHECK_EQ(mgr.has_active_tasks(), true);

        // Cleanup
        mgr.unregister_runner(&runner1);
        mgr.unregister_runner(&runner2);
    }

    SUBCASE("total_active_tasks") {
        AsyncManager& mgr = AsyncManager::instance();
        TestAsyncRunner runner1;
        TestAsyncRunner runner2;

        mgr.register_runner(&runner1);
        mgr.register_runner(&runner2);

        runner1.set_task_count(3);
        runner2.set_task_count(5);

        CHECK_EQ(mgr.total_active_tasks(), 8);

        runner1.set_task_count(0);
        CHECK_EQ(mgr.total_active_tasks(), 5);

        // Cleanup
        mgr.unregister_runner(&runner1);
        mgr.unregister_runner(&runner2);
    }
}

TEST_CASE("fl::async_run") {
    SUBCASE("updates all registered runners") {
        AsyncManager& mgr = AsyncManager::instance();
        TestAsyncRunner runner;

        mgr.register_runner(&runner);

        CHECK_EQ(runner.update_count, 0);
        async_run();
        CHECK_EQ(runner.update_count, 1);
        async_run();
        CHECK_EQ(runner.update_count, 2);

        // Cleanup
        mgr.unregister_runner(&runner);
    }
}

TEST_CASE("fl::async_active_tasks") {
    SUBCASE("returns total active tasks") {
        AsyncManager& mgr = AsyncManager::instance();
        TestAsyncRunner runner1;
        TestAsyncRunner runner2;

        mgr.register_runner(&runner1);
        mgr.register_runner(&runner2);

        runner1.set_task_count(2);
        runner2.set_task_count(3);

        CHECK_EQ(async_active_tasks(), 5);

        // Cleanup
        mgr.unregister_runner(&runner1);
        mgr.unregister_runner(&runner2);
    }
}

TEST_CASE("fl::async_has_tasks") {
    SUBCASE("checks for any active tasks") {
        AsyncManager& mgr = AsyncManager::instance();
        TestAsyncRunner runner;

        mgr.register_runner(&runner);

        runner.set_active(false);
        CHECK_EQ(async_has_tasks(), false);

        runner.set_active(true);
        CHECK_EQ(async_has_tasks(), true);

        // Cleanup
        mgr.unregister_runner(&runner);
    }
}

TEST_CASE("fl::async_yield") {
    SUBCASE("pumps async tasks") {
        AsyncManager& mgr = AsyncManager::instance();
        TestAsyncRunner runner;

        mgr.register_runner(&runner);

        CHECK_EQ(runner.update_count, 0);
        async_yield();
        // async_yield calls async_run at least once, plus additional pumps
        CHECK(runner.update_count >= 1);

        // Cleanup
        mgr.unregister_runner(&runner);
    }
}

TEST_CASE("fl::await_top_level - Basic Operations") {
    SUBCASE("await_top_level resolved promise returns value") {
        auto promise = fl::promise<int>::resolve(42);
        auto result = fl::await_top_level(promise);  // Type automatically deduced!

        CHECK(result.ok());
        CHECK_EQ(result.value(), 42);
    }

    SUBCASE("await_top_level rejected promise returns error") {
        auto promise = fl::promise<int>::reject(fl::Error("Test error"));
        auto result = fl::await_top_level(promise);  // Type automatically deduced!

        CHECK(!result.ok());
        CHECK_EQ(result.error().message, "Test error");
    }

    SUBCASE("await_top_level invalid promise returns error") {
        fl::promise<int> invalid_promise; // Default constructor creates invalid promise
        auto result = fl::await_top_level(invalid_promise);  // Type automatically deduced!

        CHECK(!result.ok());
        CHECK_EQ(result.error().message, "Invalid promise");
    }

    SUBCASE("explicit template parameter still works") {
        auto promise = fl::promise<int>::resolve(42);
        auto result = fl::await_top_level<int>(promise);  // Explicit template parameter

        CHECK(result.ok());
        CHECK_EQ(result.value(), 42);
    }
}

TEST_CASE("fl::await_top_level - Asynchronous Completion") {
    SUBCASE("await_top_level waits for promise to be resolved") {
        auto promise = fl::promise<int>::create();
        bool promise_completed = false;

        // Simulate async completion in background
        // Note: In a real scenario, this would be done by an async system
        // For testing, we'll complete it immediately after starting await_top_level

        // Complete the promise with a value
        promise.complete_with_value(123);
        promise_completed = true;

        auto result = fl::await_top_level(promise);  // Type automatically deduced!

        CHECK(promise_completed);
        CHECK(result.ok());
        CHECK_EQ(result.value(), 123);
    }

    SUBCASE("await_top_level waits for promise to be rejected") {
        auto promise = fl::promise<int>::create();
        bool promise_completed = false;

        // Complete the promise with an error
        promise.complete_with_error(fl::Error("Async error"));
        promise_completed = true;

        auto result = fl::await_top_level(promise);  // Type automatically deduced!

        CHECK(promise_completed);
        CHECK(!result.ok());
        CHECK_EQ(result.error().message, "Async error");
    }
}

TEST_CASE("fl::await_top_level - Different Value Types") {
    SUBCASE("await_top_level with string type") {
        auto promise = fl::promise<fl::string>::resolve(fl::string("Hello, World!"));
        auto result = fl::await_top_level(promise);  // Type automatically deduced!

        CHECK(result.ok());
        CHECK_EQ(result.value(), "Hello, World!");
    }

    SUBCASE("await_top_level with custom struct") {
        struct TestData {
            int x;
            fl::string name;

            bool operator==(const TestData& other) const {
                return x == other.x && name == other.name;
            }
        };

        TestData expected{42, "test"};
        auto promise = fl::promise<TestData>::resolve(expected);
        auto result = fl::await_top_level(promise);  // Type automatically deduced!

        CHECK(result.ok());
        CHECK(result.value() == expected);
    }
}

TEST_CASE("fl::await_top_level - Error Handling") {
    SUBCASE("await_top_level preserves error message") {
        fl::string error_msg = "Detailed error message";
        auto promise = fl::promise<int>::reject(fl::Error(error_msg));
        auto result = fl::await_top_level(promise);  // Type automatically deduced!

        CHECK(!result.ok());
        CHECK_EQ(result.error().message, error_msg);
    }

    SUBCASE("await_top_level with custom error") {
        fl::Error custom_error("Custom error with details");
        auto promise = fl::promise<fl::string>::reject(custom_error);
        auto result = fl::await_top_level(promise);  // Type automatically deduced!

        CHECK(!result.ok());
        CHECK_EQ(result.error().message, "Custom error with details");
    }
}

TEST_CASE("fl::await_top_level - Multiple Awaits") {
    SUBCASE("multiple awaits on different promises") {
        auto promise1 = fl::promise<int>::resolve(10);
        auto promise2 = fl::promise<int>::resolve(20);
        auto promise3 = fl::promise<int>::reject(fl::Error("Error in promise 3"));

        auto result1 = fl::await_top_level(promise1);  // Type automatically deduced!
        auto result2 = fl::await_top_level(promise2);  // Type automatically deduced!
        auto result3 = fl::await_top_level(promise3);  // Type automatically deduced!

        // Check first result
        CHECK(result1.ok());
        CHECK_EQ(result1.value(), 10);

        // Check second result
        CHECK(result2.ok());
        CHECK_EQ(result2.value(), 20);

        // Check third result (error)
        CHECK(!result3.ok());
        CHECK_EQ(result3.error().message, "Error in promise 3");
    }

    SUBCASE("await_top_level same promise multiple times") {
        auto promise = fl::promise<int>::resolve(999);

        auto result1 = fl::await_top_level(promise);  // Type automatically deduced!
        auto result2 = fl::await_top_level(promise);  // Type automatically deduced!

        // Both awaits should return the same result
        CHECK(result1.ok());
        CHECK(result2.ok());

        CHECK_EQ(result1.value(), 999);
        CHECK_EQ(result2.value(), 999);
    }
}

TEST_CASE("fl::await_top_level - Boolean Conversion and Convenience") {
    SUBCASE("boolean conversion operator") {
        auto success_promise = fl::promise<int>::resolve(42);
        auto success_result = fl::await_top_level(success_promise);

        auto error_promise = fl::promise<int>::reject(fl::Error("Error"));
        auto error_result = fl::await_top_level(error_promise);

        // Test boolean conversion (should work like ok())
        CHECK(success_result);   // Implicit conversion to bool
        CHECK(!error_result);    // Implicit conversion to bool

        // Equivalent to ok() method
        CHECK(success_result.ok());
        CHECK(!error_result.ok());
    }

    SUBCASE("error_message convenience method") {
        auto success_promise = fl::promise<int>::resolve(42);
        auto success_result = fl::await_top_level(success_promise);

        auto error_promise = fl::promise<int>::reject(fl::Error("Test error"));
        auto error_result = fl::await_top_level(error_promise);

        // Test error_message convenience method
        CHECK_EQ(success_result.error_message(), "");  // Empty string for success
        CHECK_EQ(error_result.error_message(), "Test error");  // Error message for failure
    }
}

TEST_CASE("fl::Scheduler") {
    SUBCASE("singleton instance") {
        Scheduler& sched1 = Scheduler::instance();
        Scheduler& sched2 = Scheduler::instance();
        CHECK_EQ(&sched1, &sched2);
    }

    SUBCASE("add_task returns task id") {
        Scheduler& sched = Scheduler::instance();
        sched.clear_all_tasks();

        bool executed = false;
        auto t = fl::task::every_ms(1).then([&executed]() {
            executed = true;
        });

        int task_id = sched.add_task(fl::move(t));
        CHECK(task_id > 0);

        sched.clear_all_tasks();
    }

    SUBCASE("update executes ready tasks") {
        Scheduler& sched = Scheduler::instance();
        sched.clear_all_tasks();

        bool executed = false;
        auto t = fl::task::every_ms(0).then([&executed]() {
            executed = true;
        });

        sched.add_task(fl::move(t));

        CHECK_EQ(executed, false);
        sched.update();
        CHECK_EQ(executed, true);

        sched.clear_all_tasks();
    }

    SUBCASE("clear_all_tasks") {
        Scheduler& sched = Scheduler::instance();
        sched.clear_all_tasks();

        bool executed = false;
        auto t = fl::task::every_ms(1000).then([&executed]() {
            executed = true;
        });

        sched.add_task(fl::move(t));
        sched.clear_all_tasks();
        sched.update();

        CHECK_EQ(executed, false); // Task was cleared before execution
    }

    SUBCASE("update_before_frame_tasks") {
        Scheduler& sched = Scheduler::instance();
        sched.clear_all_tasks();

        bool before_executed = false;
        bool after_executed = false;

        auto before_task = fl::task::before_frame().then([&before_executed]() {
            before_executed = true;
        });

        auto after_task = fl::task::after_frame().then([&after_executed]() {
            after_executed = true;
        });

        sched.add_task(fl::move(before_task));
        sched.add_task(fl::move(after_task));

        sched.update_before_frame_tasks();
        CHECK_EQ(before_executed, true);
        CHECK_EQ(after_executed, false);

        sched.update_after_frame_tasks();
        CHECK_EQ(after_executed, true);

        sched.clear_all_tasks();
    }

    SUBCASE("update_after_frame_tasks") {
        Scheduler& sched = Scheduler::instance();
        sched.clear_all_tasks();

        bool executed = false;
        auto t = fl::task::after_frame().then([&executed]() {
            executed = true;
        });

        sched.add_task(fl::move(t));

        CHECK_EQ(executed, false);
        sched.update_after_frame_tasks();
        CHECK_EQ(executed, true);

        sched.clear_all_tasks();
    }
}

TEST_CASE("fl::async integration") {
    SUBCASE("scheduler tasks work with async_run") {
        Scheduler& sched = Scheduler::instance();
        sched.clear_all_tasks();

        bool executed = false;
        auto t = fl::task::every_ms(0).then([&executed]() {
            executed = true;
        });

        sched.add_task(fl::move(t));

        CHECK_EQ(executed, false);
        async_run(); // Should update scheduler and async manager
        CHECK_EQ(executed, true);

        sched.clear_all_tasks();
    }
}
