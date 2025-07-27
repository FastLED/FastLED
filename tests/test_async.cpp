#include "doctest.h"
#include "fl/async.h"
#include "fl/task.h"
#include "fl/time.h"
#include "fl/warn.h"

TEST_CASE("Async tasks run correctly [async]") {
    SUBCASE("A simple task runs") {
        bool task_ran = false;
        fl::unique_ptr<fl::task> task = fl::task::every_ms(10);
        task->then([&task_ran]() {
            task_ran = true;
        });

        fl::Scheduler::instance().add_task(fl::move(task));

        fl::Scheduler::instance().update();

        REQUIRE(task_ran);
    }

    SUBCASE("Recurring tasks run multiple times") {
        int run_count = 0;
        fl::unique_ptr<fl::task> task = fl::task::every_ms(10);
        task->then([&run_count]() {
            run_count++;
        });
        
        fl::Scheduler::instance().add_task(fl::move(task));
        
        // Run scheduler multiple times
        fl::Scheduler::instance().update();
        fl::Scheduler::instance().update();
        fl::Scheduler::instance().update();
        
        // Should have run 3 times
        REQUIRE(run_count == 3);
    }
    
    SUBCASE("Task timing works correctly") {
        int run_count = 0;
        fl::unique_ptr<fl::task> task = fl::task::every_ms(50); // 50ms interval
        task->then([&run_count]() {
            run_count++;
        });
        
        fl::Scheduler::instance().add_task(fl::move(task));
        
        // Run immediately - should execute
        fl::Scheduler::instance().update();
        REQUIRE(run_count == 1);
        
        // Run again immediately - should not execute (not enough time passed)
        fl::Scheduler::instance().update();
        REQUIRE(run_count == 1);
    }
    
    SUBCASE("Error handling works") {
        bool error_caught = false;
        fl::unique_ptr<fl::task> task = fl::task::every_ms(10);
        task->then([]() {
            throw fl::Error("Test error");
        }).catch_([&error_caught](const fl::Error& e) {
            error_caught = (e.message == "Test error");
        });
        
        fl::Scheduler::instance().add_task(fl::move(task));
        fl::Scheduler::instance().update();
        
        REQUIRE(error_caught);
    }
    
    SUBCASE("Frame-based tasks work") {
        int before_count = 0;
        int after_count = 0;
        
        fl::unique_ptr<fl::task> before_task = fl::task::before_frame();
        before_task->then([&before_count]() {
            before_count++;
        });
        
        fl::unique_ptr<fl::task> after_task = fl::task::after_frame();
        after_task->then([&after_count]() {
            after_count++;
        });
        
        fl::Scheduler::instance().add_task(fl::move(before_task));
        fl::Scheduler::instance().add_task(fl::move(after_task));
        
        fl::Scheduler::instance().update();
        
        REQUIRE(before_count == 1);
        REQUIRE(after_count == 1);
    }
}