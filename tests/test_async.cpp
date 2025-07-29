#include "doctest.h"
#include "fl/async.h"
#include "fl/task.h"
#include "fl/time.h"
#include "fl/warn.h"

TEST_CASE("Async tasks run correctly [async]") {
    // Clear any leftover tasks from previous tests
    fl::Scheduler::instance().clear_all_tasks();
    
    SUBCASE("A simple task runs") {
        bool task_ran = false;
        auto task = fl::task::every_ms(10);
        task.then([&task_ran]() {
            task_ran = true;
        });

        fl::Scheduler::instance().add_task(task);

        fl::Scheduler::instance().update();

        REQUIRE(task_ran);
        
        // Clear task after this subcase to prevent interference
        fl::Scheduler::instance().clear_all_tasks();
    }

    SUBCASE("Task timing works correctly") {
        // Clear any leftover tasks from previous subcases
        fl::Scheduler::instance().clear_all_tasks();
        
        int run_count = 0;
        auto task = fl::task::every_ms(50); // 50ms interval
        task.then([&run_count]() {
            run_count++;
        });
        
        fl::Scheduler::instance().add_task(task);
        
        // Run immediately - should execute
        fl::Scheduler::instance().update();
        REQUIRE(run_count == 1);
        
        // Run again immediately - should not execute (not enough time passed)
        fl::Scheduler::instance().update();
        REQUIRE(run_count == 1);
        
        // Clear task after this subcase
        fl::Scheduler::instance().clear_all_tasks();
    }
}
