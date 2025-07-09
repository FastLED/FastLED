#include "test.h"
#include "fl/allocator.h"
#include "fl/vector.h"

using namespace fl;

// Global variables to track hook calls
static fl::vector_inlined<void*, 1000> gMallocCalls;
static fl::vector_inlined<fl::size, 1000> gMallocSizes;
static fl::vector_inlined<void*, 1000> gFreeCalls;

// Test hook implementation class
class TestMallocFreeHook : public MallocFreeHook {
public:
    void onMalloc(void* ptr, fl::size size) override {
        gMallocCalls.push_back(ptr);
        gMallocSizes.push_back(size);
    }
    
    void onFree(void* ptr) override {
        gFreeCalls.push_back(ptr);
    }
};

// Helper function to clear tracking data
static void ClearTrackingData() {
    gMallocCalls.clear();
    gMallocSizes.clear();
    gFreeCalls.clear();
    // SetMallocFreeHook(nullptr);
}

TEST_CASE("Malloc/Free Test Hooks - Basic functionality") {
    // Clear any previous tracking data
    ClearTrackingData();
    
    SUBCASE("Set and clear hooks") {
        // Create hook instance
        TestMallocFreeHook hook;
        
        // Set hook
        SetMallocFreeHook(&hook);
        
        // Clear hook
        ClearMallocFreeHook();
        
        // Test that hooks are cleared by doing allocations that shouldn't trigger callbacks
        ClearTrackingData();
        
        void* ptr1 = PSRamAllocate(100);
        void* ptr2 = PSRamAllocate(200);
        
        CHECK(gMallocCalls.empty());
        CHECK(gMallocSizes.empty());
        
        PSRamDeallocate(ptr1);
        PSRamDeallocate(ptr2);
        
        CHECK(gFreeCalls.empty());
    }
    
    SUBCASE("Malloc hook is called after allocation") {
        TestMallocFreeHook hook;
        SetMallocFreeHook(&hook);
        
        ClearTrackingData();
        
        // Test PSRamAllocate
        void* ptr1 = PSRamAllocate(100);
        REQUIRE(ptr1 != nullptr);
        
        CHECK(gMallocCalls.size() == 1);
        CHECK(gMallocCalls[0] == ptr1);
        CHECK(gMallocSizes.size() == 1);
        CHECK(gMallocSizes[0] == 100);
        
        // Test Malloc function
        ClearTrackingData();
        Malloc(200);
        
        CHECK(gMallocCalls.size() == 1);
        CHECK(gMallocSizes[0] == 200);
        
        // Cleanup
        PSRamDeallocate(ptr1);
        ClearMallocFreeHook();
    }
    
    SUBCASE("Free hook is called before deallocation") {
        TestMallocFreeHook hook;
        SetMallocFreeHook(&hook);
        
        ClearTrackingData();
        
        // Allocate some memory
        void* ptr1 = PSRamAllocate(100);
        void* ptr2 = PSRamAllocate(200);
        
        // Clear malloc tracking for this test
        ClearTrackingData();
        
        // Test PSRamDeallocate
        PSRamDeallocate(ptr1);
        
        CHECK(gFreeCalls.size() == 1);
        CHECK(gFreeCalls[0] == ptr1);
        
        // Test Free function
        ClearTrackingData();
        Free(ptr2);
        
        CHECK(gFreeCalls.size() == 1);
        CHECK(gFreeCalls[0] == ptr2);
        
        ClearMallocFreeHook();
    }
    
    SUBCASE("Both hooks work together") {
        TestMallocFreeHook hook;
        SetMallocFreeHook(&hook);
        
        ClearTrackingData();
        
        // Allocate memory
        void* ptr1 = PSRamAllocate(150);
        void* ptr2 = PSRamAllocate(250);
        
        CHECK(gMallocCalls.size() == 2);
        CHECK(gMallocSizes.size() == 2);
        CHECK(gMallocCalls[0] == ptr1);
        CHECK(gMallocCalls[1] == ptr2);
        CHECK(gMallocSizes[0] == 150);
        CHECK(gMallocSizes[1] == 250);
        
        // Clear malloc tracking for free test
        gMallocCalls.clear();
        gMallocSizes.clear();
        
        // Deallocate memory
        PSRamDeallocate(ptr1);
        PSRamDeallocate(ptr2);
        
        CHECK(gFreeCalls.size() == 2);
        CHECK(gFreeCalls[0] == ptr1);
        CHECK(gFreeCalls[1] == ptr2);
        
        // Verify malloc tracking wasn't affected by free operations
        CHECK(gMallocCalls.empty());
        CHECK(gMallocSizes.empty());
        
        ClearMallocFreeHook();
    }
    
    SUBCASE("Null pointer handling") {
        TestMallocFreeHook hook;
        SetMallocFreeHook(&hook);
        
        ClearTrackingData();
        
        // Test that hooks are not called for null pointer free
        Free(nullptr);
        
        CHECK(gFreeCalls.empty());
        
        // Test that hooks are not called for zero-size allocation
        void* ptr = PSRamAllocate(0);
        if (ptr == nullptr) {
            CHECK(gMallocCalls.empty());
            CHECK(gMallocSizes.empty());
        }
        
        ClearMallocFreeHook();
    }
    
    SUBCASE("Hook replacement") {
        // Create initial hook
        TestMallocFreeHook hook1;
        SetMallocFreeHook(&hook1);
        
        ClearTrackingData();
        
        void* ptr = PSRamAllocate(100);
        
        CHECK(gMallocCalls.size() == 1);
        
        // Replace with different hook
        fl::vector<void*> newMallocCalls;
        fl::vector<fl::size> newMallocSizes;
        fl::vector<void*> newFreeCalls;
        
        class NewTestHook : public MallocFreeHook {
        public:
            NewTestHook(fl::vector<void*>& mallocCalls, fl::vector<fl::size>& mallocSizes, fl::vector<void*>& freeCalls)
                : mMallocCalls(mallocCalls), mMallocSizes(mallocSizes), mFreeCalls(freeCalls) {}
            
            void onMalloc(void* ptr, fl::size size) override {
                mMallocCalls.push_back(ptr);
                mMallocSizes.push_back(size);
            }
            
            void onFree(void* ptr) override {
                mFreeCalls.push_back(ptr);
            }
            
        private:
            fl::vector<void*>& mMallocCalls;
            fl::vector<fl::size>& mMallocSizes;
            fl::vector<void*>& mFreeCalls;
        };
        
        NewTestHook hook2(newMallocCalls, newMallocSizes, newFreeCalls);
        SetMallocFreeHook(&hook2);
        
        void* ptr2 = PSRamAllocate(200);
        
        // Original hook should not be called
        CHECK(gMallocCalls.size() == 1);
        CHECK(gMallocSizes.size() == 1);
        
        // New hook should be called
        CHECK(newMallocCalls.size() == 1);
        CHECK(newMallocSizes.size() == 1);
        CHECK(newMallocCalls[0] == ptr2);
        CHECK(newMallocSizes[0] == 200);
        
        // Cleanup
        PSRamDeallocate(ptr);
        PSRamDeallocate(ptr2);
        ClearMallocFreeHook();
    }
}

TEST_CASE("Malloc/Free Test Hooks - Integration with allocators") {
    TestMallocFreeHook hook;
    SetMallocFreeHook(&hook);
    
    SUBCASE("Standard allocator integration") {
        ClearTrackingData();
        
        fl::allocator<int> alloc;
        
        // Allocate using standard allocator
        int* ptr = alloc.allocate(5);
        REQUIRE(ptr != nullptr);
        
        // Hook should be called
        CHECK(gMallocCalls.size() == 1);
        CHECK(gMallocCalls[0] == ptr);
        CHECK(gMallocSizes[0] == sizeof(int) * 5);
        
        // Deallocate
        gMallocCalls.clear();
        gMallocSizes.clear();
        
        alloc.deallocate(ptr, 5);
        
        CHECK(gFreeCalls.size() == 1);
        CHECK(gFreeCalls[0] == ptr);
    }
    
    SUBCASE("PSRAM allocator integration") {
        ClearTrackingData();
        
        fl::allocator_psram<int> alloc;
        
        // Allocate using PSRAM allocator
        int* ptr = alloc.allocate(3);
        REQUIRE(ptr != nullptr);
        
        // Hook should be called
        CHECK(gMallocCalls.size() == 1);
        CHECK(gMallocCalls[0] == ptr);
        CHECK(gMallocSizes[0] == sizeof(int) * 3);
        
        // Deallocate
        gMallocCalls.clear();
        gMallocSizes.clear();
        
        alloc.deallocate(ptr, 3);
        
        CHECK(gFreeCalls.size() == 1);
        CHECK(gFreeCalls[0] == ptr);
    }
    
    ClearMallocFreeHook();
}
