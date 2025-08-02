

#include "doctest.h"
#include "fl/task.h"
#include "fl/async.h"
#include "fl/engine_events.h"
#include "fl/time.h"



using namespace fl;

namespace {

// Helper class to track frame events for testing
class TestFrameListener : public fl::EngineEvents::Listener {
public:
    TestFrameListener() {
        fl::EngineEvents::addListener(this);
    }
    
    ~TestFrameListener() {
        fl::EngineEvents::removeListener(this);
    }
    
    void onEndFrame() override {
        frame_count++;
        // Pump the scheduler to execute after_frame tasks specifically
        fl::Scheduler::instance().update_after_frame_tasks();
    }
    
    int frame_count = 0;
};

} // anonymous namespace

TEST_CASE("Task self-registration and destruction behavior [task]") {
    
    SUBCASE("Task auto-registers when callback is set - SUCCESS") {
        // Clear any leftover tasks from previous tests
        fl::Scheduler::instance().clear_all_tasks();
        
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
        TestFrameListener listener;
        fl::EngineEvents::onEndFrame();
        
        // Task should now execute due to auto-registration
        CHECK(task_executed);
        
        // Clean up
        fl::Scheduler::instance().clear_all_tasks();
    }
    
    SUBCASE("Fluent API pattern works with auto-registration") {
        // Clear any leftover tasks from previous tests
        fl::Scheduler::instance().clear_all_tasks();
        
        bool task_executed = false;
        
        // This fluent pattern should now work correctly
        fl::task::after_frame().then([&task_executed]() {
            task_executed = true;
        });
        // Entire chain destructs here, but task was auto-registered
        
        // Simulate frame end event  
        TestFrameListener listener;
        fl::EngineEvents::onEndFrame();
        
        // Task should execute
        CHECK(task_executed);
        
        // Clean up
        fl::Scheduler::instance().clear_all_tasks();
    }
    
    SUBCASE("Multiple auto-registering tasks work correctly") {
        // Clear any leftover tasks from previous tests
        fl::Scheduler::instance().clear_all_tasks();
        
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
        TestFrameListener listener;
        fl::EngineEvents::onEndFrame();
        
        // All 3 tasks should execute
        CHECK_EQ(tasks_executed, 3);
        
        // Clean up
        fl::Scheduler::instance().clear_all_tasks();
    }
    
    SUBCASE("Manual registration still works (backward compatibility)") {
        // Clear any leftover tasks from previous tests
        fl::Scheduler::instance().clear_all_tasks();
        
        bool task_executed = false;
        
        // Old style should still work
        auto task = fl::task::after_frame()
            .then([&task_executed]() {
                task_executed = true;
            });
        
        // Manual add should work (though now redundant since auto-registration already happened)
        fl::Scheduler::instance().add_task(task);
        
        // Simulate frame end event
        TestFrameListener listener;
        fl::EngineEvents::onEndFrame();
        
        // Task should execute (only once, not twice)
        CHECK(task_executed);
        
        // Clean up
        fl::Scheduler::instance().clear_all_tasks();
    }
    
    SUBCASE("Task cancellation works with auto-registered tasks") {
        // Clear any leftover tasks from previous tests
        fl::Scheduler::instance().clear_all_tasks();
        
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
        TestFrameListener listener;
        fl::EngineEvents::onEndFrame();
        
        // Task should NOT execute due to cancellation
        CHECK_FALSE(task_executed);
        
        // Clean up
        fl::Scheduler::instance().clear_all_tasks();
    }
    
    SUBCASE("Tasks without callbacks don't auto-register") {
        // Clear any leftover tasks from previous tests
        fl::Scheduler::instance().clear_all_tasks();
        
        // Create task without callback - should not auto-register
        auto task = fl::task::after_frame();
        
        CHECK_FALSE(task.has_then());
        CHECK(task.is_valid()); // Task should be valid but not auto-registered
        
        // Clean up
        fl::Scheduler::instance().clear_all_tasks();
    }
    
    SUBCASE("every_ms task runs immediately once then respects timing interval") {
        // Clear any leftover tasks from previous tests
        fl::Scheduler::instance().clear_all_tasks();
        
        int execution_count = 0;
        
        // Create a task that runs every 100ms and auto-registers
        auto task = fl::task::every_ms(100)
            .then([&execution_count]() {
                execution_count++;
            });
        
        // Task should be auto-registered and ready to run immediately
        CHECK(task.is_valid());
        CHECK(task.has_then());
        
        // First update - should run immediately
        fl::Scheduler::instance().update();
        CHECK_EQ(execution_count, 1);
        
        // Immediate second update - should NOT run (not enough time passed)
        fl::Scheduler::instance().update();
        CHECK_EQ(execution_count, 1); // Still 1, didn't run again
        
        // Manually advance the task's last run time to simulate 50ms passing
        uint32_t current_time = fl::time();
        task.set_last_run_time(current_time - 50);
        
        // Update - should still NOT run (only 50ms passed, need 100ms)
        fl::Scheduler::instance().update();
        CHECK_EQ(execution_count, 1); // Still 1
        
        // Manually advance to simulate 100ms+ passing
        task.set_last_run_time(current_time - 100);
        
        // Update - should run now (100ms has passed)
        fl::Scheduler::instance().update();
        CHECK_EQ(execution_count, 2); // Should be 2 now
        
        // Immediate update again - should NOT run
        fl::Scheduler::instance().update();
        CHECK_EQ(execution_count, 2); // Still 2
        
        // Clean up
        fl::Scheduler::instance().clear_all_tasks();
    }
    
    SUBCASE("after_frame task executes when FastLED.show() is called") {
        // Clear any leftover tasks from previous tests - CRITICAL for test isolation
        fl::Scheduler::instance().clear_all_tasks();
        
        int execution_count = 0;
        
        // Create an after_frame task that auto-registers
        auto task = fl::task::after_frame()
            .then([&execution_count]() {
                execution_count++;
            });
        
        // Task should be auto-registered
        CHECK(task.is_valid());
        CHECK(task.has_then());
        
        // Initial state - task hasn't run yet
        CHECK_EQ(execution_count, 0);
        
        // Manually calling scheduler update shouldn't trigger frame tasks yet
        fl::Scheduler::instance().update();
        CHECK_EQ(execution_count, 0); // Still 0

        
        // Create a frame listener for this specific test
        TestFrameListener listener;
        
        // Instead of calling FastLED.show(), directly trigger the engine events
        // This tests the task system without requiring LED hardware setup
        fl::EngineEvents::onEndFrame();
        
        // The after_frame task should have executed
        CHECK_EQ(execution_count, 1);
        
        // Calling onEndFrame() again should execute the task again (it's been removed as one-shot)
        // But since it's already removed, create another task to test
        auto task2 = fl::task::after_frame()
            .then([&execution_count]() {
                execution_count++;
            });
        
        fl::EngineEvents::onEndFrame();
        CHECK_EQ(execution_count, 2);
        
        // Clean up
        fl::Scheduler::instance().clear_all_tasks();
    }
} 
