

#include "test.h"
#include "FastLED.h"
#include "fl/task/task.h"
#include "fl/task/executor.h"
#include "fl/system/engine_events.h"
#include "fl/stl/chrono.h"
#include "fl/stl/stdint.h"
#include "fl/stl/new.h"
#include "fl/stl/function.h"
#include "fl/stl/move.h"

FL_TEST_FILE(FL_FILEPATH) {




FL_TEST_CASE("Task self-registration and destruction behavior [task]") {
    
    FL_SUBCASE("Task auto-registers when callback is set - SUCCESS") {
        // Clear any leftover tasks from previous tests
        fl::task::Scheduler::instance().clear_all_tasks();
        
        bool task_executed = false;
        
        // Create task without manually adding to scheduler
        // Auto-registration happens when .then() is called
        {
            fl::task::after_frame()
                .then([&task_executed]() {
                    task_executed = true;
                });
            // Task temporary object destructs here, but it's already registered
        }
        
        // Simulate frame end event
        fl::EngineEvents::onEndFrame();
        
        // Task should now execute due to auto-registration
        FL_CHECK(task_executed);
        
        // Clean up
        fl::task::Scheduler::instance().clear_all_tasks();
    }
    
    FL_SUBCASE("Fluent API pattern works with auto-registration") {
        // Clear any leftover tasks from previous tests
        fl::task::Scheduler::instance().clear_all_tasks();
        
        bool task_executed = false;
        
        // This fluent pattern should now work correctly
        fl::task::after_frame().then([&task_executed]() {
            task_executed = true;
        });
        // Entire chain destructs here, but task was auto-registered
        
        // Simulate frame end event  
        fl::EngineEvents::onEndFrame();
        
        // Task should execute
        FL_CHECK(task_executed);
        
        // Clean up
        fl::task::Scheduler::instance().clear_all_tasks();
    }
    
    FL_SUBCASE("Multiple auto-registering tasks work correctly") {
        // Clear any leftover tasks from previous tests
        fl::task::Scheduler::instance().clear_all_tasks();
        
        int tasks_executed = 0;
        
        // Create multiple tasks without saving them - they auto-register
        for (int i = 0; i < 3; i++) {
            fl::task::after_frame()
                .then([&tasks_executed]() {
                    tasks_executed++;
                });
            // Each task auto-registers when .then() is called
        }
        
        // Simulate frame end event
        fl::EngineEvents::onEndFrame();
        
        // All 3 tasks should execute
        FL_CHECK_EQ(tasks_executed, 3);
        
        // Clean up
        fl::task::Scheduler::instance().clear_all_tasks();
    }
    
    FL_SUBCASE("Manual registration still works (backward compatibility)") {
        // Clear any leftover tasks from previous tests
        fl::task::Scheduler::instance().clear_all_tasks();
        
        int execution_count = 0;
        
        // Old style should still work
        auto task = fl::task::after_frame()
            .then([&execution_count]() {
                ++execution_count;
            });
        
        // Manual add should work (though now redundant since auto-registration already happened)
        fl::task::Scheduler::instance().add_task(task);
        
        // Simulate frame end event
        fl::EngineEvents::onEndFrame();
        
        // Task should execute (only once, not twice)
        FL_CHECK_EQ(execution_count, 1);
        
        // Clean up
        fl::task::Scheduler::instance().clear_all_tasks();
    }
    
    FL_SUBCASE("Task cancellation works with auto-registered tasks") {
        // Clear any leftover tasks from previous tests
        fl::task::Scheduler::instance().clear_all_tasks();
        
        bool task_executed = false;
        
        // Create auto-registering task and save reference for cancellation
        auto task = fl::task::after_frame()
            .then([&task_executed]() {
                task_executed = true;
            });
        // Task auto-registered when .then() was called
        
        // Cancel the task
        task.cancel();
        
        // Simulate frame end event
        fl::EngineEvents::onEndFrame();
        
        // Task should NOT execute due to cancellation
        FL_CHECK_FALSE(task_executed);
        
        // Clean up
        fl::task::Scheduler::instance().clear_all_tasks();
    }
    
    FL_SUBCASE("Tasks without callbacks don't auto-register") {
        // Clear any leftover tasks from previous tests
        fl::task::Scheduler::instance().clear_all_tasks();
        
        // Create task without callback - should not auto-register
        auto task = fl::task::after_frame();
        
        FL_CHECK_FALSE(task.has_then());
        FL_CHECK(task.is_valid()); // Task should be valid but not auto-registered
        
        // Clean up
        fl::task::Scheduler::instance().clear_all_tasks();
    }
    
    FL_SUBCASE("every_ms task runs immediately once then respects timing interval") {
        // Clear any leftover tasks from previous tests
        fl::task::Scheduler::instance().clear_all_tasks();
        
        int execution_count = 0;
        
        // Create a task that runs every 100ms and auto-registers
        auto task = fl::task::every_ms(100)
            .then([&execution_count]() {
                execution_count++;
            });
        
        // Task should be auto-registered and ready to run immediately
        FL_CHECK(task.is_valid());
        FL_CHECK(task.has_then());
        
        // First update - should run immediately
        fl::task::Scheduler::instance().update();
        FL_CHECK_EQ(execution_count, 1);
        
        // Immediate second update - should NOT run (not enough time passed)
        fl::task::Scheduler::instance().update();
        FL_CHECK_EQ(execution_count, 1); // Still 1, didn't run again
        
        // Manually advance the task's last run time to simulate 50ms passing
        uint32_t current_time = fl::millis();
        task.set_last_run_time(current_time - 50);
        
        // Update - should still NOT run (only 50ms passed, need 100ms)
        fl::task::Scheduler::instance().update();
        FL_CHECK_EQ(execution_count, 1); // Still 1
        
        // Manually advance to simulate 100ms+ passing
        task.set_last_run_time(current_time - 100);
        
        // Update - should run now (100ms has passed)
        fl::task::Scheduler::instance().update();
        FL_CHECK_EQ(execution_count, 2); // Should be 2 now
        
        // Immediate update again - should NOT run
        fl::task::Scheduler::instance().update();
        FL_CHECK_EQ(execution_count, 2); // Still 2
        
        // Clean up
        fl::task::Scheduler::instance().clear_all_tasks();
    }
    
    FL_SUBCASE("after_frame task executes when FastLED.show() is called") {
        // Clear any leftover tasks from previous tests - CRITICAL for test isolation
        fl::task::Scheduler::instance().clear_all_tasks();
        
        int execution_count = 0;
        
        // Create an after_frame task that auto-registers
        auto task = fl::task::after_frame()
            .then([&execution_count]() {
                execution_count++;
            });
        
        // Task should be auto-registered
        FL_CHECK(task.is_valid());
        FL_CHECK(task.has_then());
        
        // Initial state - task hasn't run yet
        FL_CHECK_EQ(execution_count, 0);
        
        // Manually calling scheduler update shouldn't trigger frame tasks yet
        fl::task::Scheduler::instance().update();
        FL_CHECK_EQ(execution_count, 0); // Still 0

        
        // No controller is required to exercise the real FastLED frame path.
        FastLED.show();
        
        // The after_frame task should have executed
        FL_CHECK_EQ(execution_count, 1);
        
        // Frame tasks recur at every matching boundary until canceled.
        FastLED.show();
        FL_CHECK_EQ(execution_count, 2);

        task.cancel();
        FastLED.show();
        FL_CHECK_EQ(execution_count, 2);
        
        // Clean up
        fl::task::Scheduler::instance().clear_all_tasks();
    }
}

FL_TEST_CASE("before_frame and after_frame follow the engine lifecycle [task]") {
    auto& scheduler = fl::task::Scheduler::instance();
    scheduler.clear_all_tasks();

    fl::vector<int> order;
    auto before = fl::task::before_frame().then([&order]() {
        order.push_back(1);
    });
    auto after = fl::task::after_frame().then([&order]() {
        order.push_back(2);
    });

    FastLED.show();
    FastLED.show();
    FastLED.showColor(CRGB::Black);

    FL_REQUIRE_EQ(order.size(), 6);
    FL_CHECK_EQ(order[0], 1);
    FL_CHECK_EQ(order[1], 2);
    FL_CHECK_EQ(order[2], 1);
    FL_CHECK_EQ(order[3], 2);
    FL_CHECK_EQ(order[4], 1);
    FL_CHECK_EQ(order[5], 2);

    before.cancel();
    after.cancel();
    FastLED.show();
    FL_CHECK_EQ(order.size(), 6);

    scheduler.clear_all_tasks();
}

FL_TEST_CASE("frame task dispatch is deferred and non-reentrant [task]") {
    auto& scheduler = fl::task::Scheduler::instance();
    scheduler.clear_all_tasks();

    int outer_count = 0;
    int deferred_count = 0;
    fl::task::Handle deferred;
    auto outer = fl::task::before_frame().then([&]() {
        ++outer_count;
        if (outer_count == 1) {
            deferred = fl::task::before_frame().then([&deferred_count]() {
                ++deferred_count;
            });
            FastLED.onBeginFrame();
        }
    });

    FastLED.onBeginFrame();
    FL_CHECK_EQ(outer_count, 1);
    FL_CHECK_EQ(deferred_count, 0);

    FastLED.onBeginFrame();
    FL_CHECK_EQ(outer_count, 2);
    FL_CHECK_EQ(deferred_count, 1);

    outer.cancel();
    deferred.cancel();
    scheduler.clear_all_tasks();
}

FL_TEST_CASE("frame task dispatch tolerates scheduler mutation [task]") {
    auto& scheduler = fl::task::Scheduler::instance();
    scheduler.clear_all_tasks();

    auto timer = fl::task::every_ms(1000).then([]() {});
    int second_count = 0;
    auto first = fl::task::before_frame().then([&timer]() {
        timer.cancel();
        fl::task::run(0, fl::task::ExecFlags::TASKS);
    });
    auto second = fl::task::before_frame().then([&second_count]() {
        ++second_count;
    });

    FastLED.onBeginFrame();
    FL_CHECK_EQ(second_count, 1);

    first.cancel();
    second.cancel();
    scheduler.clear_all_tasks();
}

} // FL_TEST_FILE
