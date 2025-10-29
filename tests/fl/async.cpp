#include "test.h"
#include "fl/async.h"
#include "fl/task.h"
#include "fl/promise.h"

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

TEST_CASE("fl::await_top_level") {
    SUBCASE("returns immediately for completed promise") {
        auto p = fl::promise<int>::resolve(42);

        auto result = fl::await_top_level(p);
        CHECK(result.ok());
        CHECK_EQ(result.value(), 42);
    }

    SUBCASE("returns error for rejected promise") {
        auto p = fl::promise<int>::reject(Error("test error"));

        auto result = fl::await_top_level(p);
        CHECK(!result.ok());
        CHECK(result.error().message.c_str() != nullptr);
    }

    SUBCASE("handles invalid promise") {
        fl::promise<int> p; // Default constructed, invalid

        auto result = fl::await_top_level(p);
        CHECK(!result.ok());
        // Should contain "Invalid promise" error
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
