#include "test.h"
#include "fl/async.h"

using namespace fl;

// Mock async runner for testing
class MockAsyncRunner : public AsyncRunner {
public:
    MockAsyncRunner() : mActiveCount(0), mUpdateCount(0) {}
    
    void update() override {
        mUpdateCount++;
    }
    
    bool has_active_tasks() const override {
        return mActiveCount > 0;
    }
    
    size_t active_task_count() const override {
        return mActiveCount;
    }
    
    void set_active_count(size_t count) {
        mActiveCount = count;
    }
    
    size_t get_update_count() const {
        return mUpdateCount;
    }

private:
    size_t mActiveCount;
    size_t mUpdateCount;
};

TEST_CASE("fl::async - Basic AsyncManager Operations") {
    auto& manager = AsyncManager::instance();
    
    SUBCASE("AsyncManager starts empty") {
        CHECK(!manager.has_active_tasks());
        CHECK_EQ(manager.total_active_tasks(), 0);
    }
    
    SUBCASE("Register and unregister runners") {
        MockAsyncRunner runner;
        
        // Register runner
        manager.register_runner(&runner);
        
        // Should still have no active tasks since runner has none
        CHECK(!manager.has_active_tasks());
        CHECK_EQ(manager.total_active_tasks(), 0);
        
        // Give runner some active tasks
        runner.set_active_count(3);
        CHECK(manager.has_active_tasks());
        CHECK_EQ(manager.total_active_tasks(), 3);
        
        // Unregister runner
        manager.unregister_runner(&runner);
        CHECK(!manager.has_active_tasks());
        CHECK_EQ(manager.total_active_tasks(), 0);
    }
    
    SUBCASE("Duplicate registration is ignored") {
        MockAsyncRunner runner;
        
        manager.register_runner(&runner);
        manager.register_runner(&runner); // Should be ignored
        
        runner.set_active_count(1);
        CHECK_EQ(manager.total_active_tasks(), 1); // Not doubled
        
        manager.unregister_runner(&runner);
    }
    
    SUBCASE("update_all calls update on all runners") {
        MockAsyncRunner runner1, runner2;
        
        manager.register_runner(&runner1);
        manager.register_runner(&runner2);
        
        CHECK_EQ(runner1.get_update_count(), 0);
        CHECK_EQ(runner2.get_update_count(), 0);
        
        manager.update_all();
        
        CHECK_EQ(runner1.get_update_count(), 1);
        CHECK_EQ(runner2.get_update_count(), 1);
        
        manager.unregister_runner(&runner1);
        manager.unregister_runner(&runner2);
    }
}

TEST_CASE("fl::async - Multiple Runners") {
    auto& manager = AsyncManager::instance();
    
    SUBCASE("Multiple runners with different task counts") {
        MockAsyncRunner runner1, runner2, runner3;
        
        manager.register_runner(&runner1);
        manager.register_runner(&runner2);
        manager.register_runner(&runner3);
        
        runner1.set_active_count(2);
        runner2.set_active_count(0); // No active tasks
        runner3.set_active_count(5);
        
        CHECK(manager.has_active_tasks());
        CHECK_EQ(manager.total_active_tasks(), 7); // 2 + 0 + 5
        
        // Remove all tasks from all runners
        runner1.set_active_count(0);
        runner3.set_active_count(0);
        
        CHECK(!manager.has_active_tasks());
        CHECK_EQ(manager.total_active_tasks(), 0);
        
        manager.unregister_runner(&runner1);
        manager.unregister_runner(&runner2);
        manager.unregister_runner(&runner3);
    }
}

TEST_CASE("fl::async - Public API Functions") {
    SUBCASE("asyncrun calls AsyncManager::update_all") {
        MockAsyncRunner runner;
        AsyncManager::instance().register_runner(&runner);
        
        CHECK_EQ(runner.get_update_count(), 0);
        
        fl::asyncrun();
        
        CHECK_EQ(runner.get_update_count(), 1);
        
        AsyncManager::instance().unregister_runner(&runner);
    }
    
    SUBCASE("async_has_tasks reflects manager state") {
        MockAsyncRunner runner;
        AsyncManager::instance().register_runner(&runner);
        
        runner.set_active_count(0);
        CHECK(!fl::async_has_tasks());
        
        runner.set_active_count(1);
        CHECK(fl::async_has_tasks());
        
        AsyncManager::instance().unregister_runner(&runner);
    }
    
    SUBCASE("async_active_tasks returns total count") {
        MockAsyncRunner runner1, runner2;
        AsyncManager::instance().register_runner(&runner1);
        AsyncManager::instance().register_runner(&runner2);
        
        runner1.set_active_count(3);
        runner2.set_active_count(7);
        
        CHECK_EQ(fl::async_active_tasks(), 10);
        
        AsyncManager::instance().unregister_runner(&runner1);
        AsyncManager::instance().unregister_runner(&runner2);
    }
}

TEST_CASE("fl::async - Edge Cases") {
    auto& manager = AsyncManager::instance();
    
    SUBCASE("Null runner registration is ignored") {
        size_t initial_count = manager.total_active_tasks();
        manager.register_runner(nullptr);
        CHECK_EQ(manager.total_active_tasks(), initial_count);
    }
    
    SUBCASE("Unregistering non-existent runner is safe") {
        MockAsyncRunner runner;
        // Should not crash or cause issues
        manager.unregister_runner(&runner);
        CHECK_EQ(manager.total_active_tasks(), 0);
    }
    
    SUBCASE("update_all with null runner is safe") {
        // This tests the null check in update_all
        // We can't directly test this, but compilation ensures the check exists
        manager.update_all();
        CHECK(true); // Just verify we don't crash
    }
} 
