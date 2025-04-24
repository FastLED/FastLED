
// g++ --std=c++11 test.cpp

#include "test.h"

#include "fl/type_traits.h"

TEST_CASE("Test fl::move") {
    // Test with a simple class that tracks move operations
    class MoveTracker {
    public:
        MoveTracker() : moved_from(false) {}
        
        // Copy constructor
        MoveTracker(const MoveTracker& other) : moved_from(false) {
            // Regular copy
        }
        
        // Move constructor
        MoveTracker(MoveTracker&& other) : moved_from(false) {
            other.moved_from = true;
        }
        
        bool was_moved_from() const { return moved_from; }
        
    private:
        bool moved_from;
    };
    
    // Test 1: Basic move operation
    {
        MoveTracker original;
        REQUIRE_FALSE(original.was_moved_from());
        
        // Use fl::move to trigger move constructor
        MoveTracker moved(fl::move(original));
        
        // Original should be marked as moved from
        REQUIRE(original.was_moved_from());
        REQUIRE_FALSE(moved.was_moved_from());
    }
    
    // Test 2: Move vs copy behavior
    {
        MoveTracker original;
        
        // Regular copy - shouldn't mark original as moved
        MoveTracker copied(original);
        REQUIRE_FALSE(original.was_moved_from());
        
        // Move should mark as moved
        MoveTracker moved(fl::move(original));
        REQUIRE(original.was_moved_from());
    }
}


