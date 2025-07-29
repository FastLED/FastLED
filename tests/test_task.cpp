#include "doctest.h"
#include "fl/task.h"
#include "fl/async.h"
#include "fl/engine_events.h"

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
        // Pump the scheduler to execute after_frame tasks
        fl::Scheduler::instance().update();
    }
    
    int frame_count = 0;
};

} // anonymous namespace

TEST_CASE("Task self-registration and destruction behavior [task]") {
    // Clear any leftover tasks from previous tests
    fl::Scheduler::instance().clear_all_tasks();
    
    SUBCASE("Task auto-registers when callback is set - SUCCESS") {
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
        // Create task without callback - should not auto-register
        auto task = fl::task::after_frame();
        
        CHECK_FALSE(task.has_then());
        CHECK(task.is_valid()); // Task should be valid but not auto-registered
        
        // Clean up
        fl::Scheduler::instance().clear_all_tasks();
    }
} 
