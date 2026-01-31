#include "fl/stl/shared_ptr.h"
#include "fl/stl/weak_ptr.h"
#include "fl/compiler_control.h"
#include "fl/stl/vector.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/stl/allocator.h"
#include "fl/stl/move.h"

namespace weak_ptr_test {

// Test class that does NOT inherit from fl::Referent (non-intrusive)
class TestClass {
public:
    TestClass() : mValue(0), mDestructorCalled(nullptr) {}
    TestClass(int value) : mValue(value), mDestructorCalled(nullptr) {}

    // Constructor that allows tracking destructor calls
    TestClass(int value, bool* destructor_flag) : mValue(value), mDestructorCalled(destructor_flag) {}

    ~TestClass() {
        if (mDestructorCalled) {
            *mDestructorCalled = true;
        }
    }

    int getValue() const { return mValue; }
    void setValue(int value) { mValue = value; }

private:
    int mValue;
    bool* mDestructorCalled;
};

TEST_CASE("fl::weak_ptr default construction") {
    fl::weak_ptr<TestClass> weak;
    FL_CHECK_EQ(weak.use_count(), 0);
    FL_CHECK(weak.expired());
    
    auto shared = weak.lock();
    FL_CHECK(!shared);
    FL_CHECK_EQ(shared.get(), nullptr);
}

TEST_CASE("fl::weak_ptr construction from shared_ptr") {
    fl::shared_ptr<TestClass> shared = fl::make_shared<TestClass>(42);
    FL_CHECK_EQ(shared.use_count(), 1);
    
    fl::weak_ptr<TestClass> weak(shared);
    FL_CHECK_EQ(weak.use_count(), 1);
    FL_CHECK_EQ(shared.use_count(), 1); // weak_ptr doesn't increase shared count
    FL_CHECK(!weak.expired());
    
    auto locked = weak.lock();
    FL_CHECK(locked);
    FL_CHECK_EQ(locked.use_count(), 2); // lock() creates a shared_ptr
    FL_CHECK_EQ(locked->getValue(), 42);
}

TEST_CASE("fl::weak_ptr copy construction") {
    fl::shared_ptr<TestClass> shared = fl::make_shared<TestClass>(42);
    fl::weak_ptr<TestClass> weak1(shared);
    fl::weak_ptr<TestClass> weak2(weak1);
    
    FL_CHECK_EQ(weak1.use_count(), 1);
    FL_CHECK_EQ(weak2.use_count(), 1);
    FL_CHECK(!weak1.expired());
    FL_CHECK(!weak2.expired());
    
    auto locked1 = weak1.lock();
    auto locked2 = weak2.lock();
    FL_CHECK_EQ(locked1.get(), locked2.get());
    FL_CHECK_EQ(locked1->getValue(), 42);
}

TEST_CASE("fl::weak_ptr move construction") {
    fl::shared_ptr<TestClass> shared = fl::make_shared<TestClass>(42);
    fl::weak_ptr<TestClass> weak1(shared);
    fl::weak_ptr<TestClass> weak2(fl::move(weak1));
    
    FL_CHECK_EQ(weak1.use_count(), 0);
    FL_CHECK(weak1.expired());
    FL_CHECK_EQ(weak2.use_count(), 1);
    FL_CHECK(!weak2.expired());
    
    auto locked = weak2.lock();
    FL_CHECK(locked);
    FL_CHECK_EQ(locked->getValue(), 42);
}

TEST_CASE("fl::weak_ptr assignment from shared_ptr") {
    fl::shared_ptr<TestClass> shared1 = fl::make_shared<TestClass>(42);
    fl::shared_ptr<TestClass> shared2 = fl::make_shared<TestClass>(100);
    fl::weak_ptr<TestClass> weak(shared1);
    
    FL_CHECK_EQ(weak.lock()->getValue(), 42);
    
    weak = shared2;
    FL_CHECK_EQ(weak.lock()->getValue(), 100);
}

TEST_CASE("fl::weak_ptr assignment from weak_ptr") {
    fl::shared_ptr<TestClass> shared1 = fl::make_shared<TestClass>(42);
    fl::shared_ptr<TestClass> shared2 = fl::make_shared<TestClass>(100);
    fl::weak_ptr<TestClass> weak1(shared1);
    fl::weak_ptr<TestClass> weak2(shared2);
    
    FL_CHECK_EQ(weak1.lock()->getValue(), 42);
    FL_CHECK_EQ(weak2.lock()->getValue(), 100);
    
    weak1 = weak2;
    FL_CHECK_EQ(weak1.lock()->getValue(), 100);
    FL_CHECK_EQ(weak2.lock()->getValue(), 100);
}

TEST_CASE("fl::weak_ptr move assignment") {
    fl::shared_ptr<TestClass> shared1 = fl::make_shared<TestClass>(42);
    fl::shared_ptr<TestClass> shared2 = fl::make_shared<TestClass>(100);
    fl::weak_ptr<TestClass> weak1(shared1);
    fl::weak_ptr<TestClass> weak2(shared2);
    
    weak1 = fl::move(weak2);
    FL_CHECK_EQ(weak1.lock()->getValue(), 100);
    FL_CHECK(weak2.expired());
}

TEST_CASE("fl::weak_ptr expiration when shared_ptr destroyed") {
    bool destructor_called = false;
    fl::weak_ptr<TestClass> weak;
    
    {
        fl::shared_ptr<TestClass> shared = fl::make_shared<TestClass>(42, &destructor_called);
        weak = shared;
        FL_CHECK(!weak.expired());
        FL_CHECK_EQ(weak.use_count(), 1);
        FL_CHECK(!destructor_called);
        
        auto locked = weak.lock();
        FL_CHECK(locked);
        FL_CHECK_EQ(locked->getValue(), 42);
    }
    
    // shared_ptr is destroyed, object should be destroyed
    FL_CHECK(destructor_called);
    FL_CHECK(weak.expired());
    FL_CHECK_EQ(weak.use_count(), 0);
    
    auto locked = weak.lock();
    FL_CHECK(!locked);
    FL_CHECK_EQ(locked.get(), nullptr);
}

TEST_CASE("fl::weak_ptr with multiple shared_ptr references") {
    fl::shared_ptr<TestClass> shared1 = fl::make_shared<TestClass>(42);
    fl::shared_ptr<TestClass> shared2 = shared1;
    fl::weak_ptr<TestClass> weak(shared1);
    
    FL_CHECK_EQ(shared1.use_count(), 2);
    FL_CHECK_EQ(weak.use_count(), 2);
    FL_CHECK(!weak.expired());
    
    shared1.reset();
    FL_CHECK_EQ(shared2.use_count(), 1);
    FL_CHECK_EQ(weak.use_count(), 1);
    FL_CHECK(!weak.expired());
    
    shared2.reset();
    FL_CHECK(weak.expired());
    FL_CHECK_EQ(weak.use_count(), 0);
}

TEST_CASE("fl::weak_ptr reset functionality") {
    fl::shared_ptr<TestClass> shared = fl::make_shared<TestClass>(42);
    fl::weak_ptr<TestClass> weak(shared);
    
    FL_CHECK(!weak.expired());
    FL_CHECK_EQ(weak.use_count(), 1);
    
    weak.reset();
    FL_CHECK(weak.expired());
    FL_CHECK_EQ(weak.use_count(), 0);
    
    auto locked = weak.lock();
    FL_CHECK(!locked);
}

TEST_CASE("fl::weak_ptr swap functionality") {
    fl::shared_ptr<TestClass> shared1 = fl::make_shared<TestClass>(42);
    fl::shared_ptr<TestClass> shared2 = fl::make_shared<TestClass>(100);
    fl::weak_ptr<TestClass> weak1(shared1);
    fl::weak_ptr<TestClass> weak2(shared2);
    
    FL_CHECK_EQ(weak1.lock()->getValue(), 42);
    FL_CHECK_EQ(weak2.lock()->getValue(), 100);
    
    weak1.swap(weak2);
    FL_CHECK_EQ(weak1.lock()->getValue(), 100);
    FL_CHECK_EQ(weak2.lock()->getValue(), 42);
}

TEST_CASE("fl::weak_ptr owner_before functionality") {
    fl::shared_ptr<TestClass> shared1 = fl::make_shared<TestClass>(42);
    fl::shared_ptr<TestClass> shared2 = fl::make_shared<TestClass>(100);
    fl::weak_ptr<TestClass> weak1(shared1);
    fl::weak_ptr<TestClass> weak2(shared2);
    
    // owner_before should provide a strict weak ordering
    bool order1 = weak1.owner_before(weak2);
    bool order2 = weak2.owner_before(weak1);
    
    // One should be true and the other false (strict ordering)
    FL_CHECK(order1 != order2);
    
    // Test owner_before with shared_ptr
    bool order3 = weak1.owner_before(shared2);
    bool order4 = weak2.owner_before(shared1);
    FL_CHECK(order3 != order4);
}

TEST_CASE("fl::weak_ptr conversion to shared_ptr") {
    fl::shared_ptr<TestClass> shared = fl::make_shared<TestClass>(42);
    fl::weak_ptr<TestClass> weak(shared);
    
    // Test construction from weak_ptr
    fl::shared_ptr<TestClass> converted(weak);
    FL_CHECK(converted);
    FL_CHECK_EQ(converted.use_count(), 2);
    FL_CHECK_EQ(shared.use_count(), 2);
    FL_CHECK_EQ(converted->getValue(), 42);
    FL_CHECK_EQ(converted.get(), shared.get());
}

TEST_CASE("fl::weak_ptr conversion from expired weak_ptr") {
    fl::weak_ptr<TestClass> weak;
    
    {
        fl::shared_ptr<TestClass> shared = fl::make_shared<TestClass>(42);
        weak = shared;
        FL_CHECK(!weak.expired());
    }
    
    FL_CHECK(weak.expired());
    
    // Converting expired weak_ptr should result in empty shared_ptr
    fl::shared_ptr<TestClass> converted(weak);
    FL_CHECK(!converted);
    FL_CHECK_EQ(converted.get(), nullptr);
    FL_CHECK_EQ(converted.use_count(), 0);
}

TEST_CASE("fl::weak_ptr multiple weak references") {
    fl::shared_ptr<TestClass> shared = fl::make_shared<TestClass>(42);
    fl::weak_ptr<TestClass> weak1(shared);
    fl::weak_ptr<TestClass> weak2(shared);
    fl::weak_ptr<TestClass> weak3(weak1);
    
    FL_CHECK_EQ(shared.use_count(), 1);
    FL_CHECK_EQ(weak1.use_count(), 1);
    FL_CHECK_EQ(weak2.use_count(), 1);
    FL_CHECK_EQ(weak3.use_count(), 1);
    
    shared.reset();
    
    FL_CHECK(weak1.expired());
    FL_CHECK(weak2.expired());
    FL_CHECK(weak3.expired());
    FL_CHECK_EQ(weak1.use_count(), 0);
    FL_CHECK_EQ(weak2.use_count(), 0);
    FL_CHECK_EQ(weak3.use_count(), 0);
}

TEST_CASE("fl::weak_ptr self-assignment safety") {
    fl::shared_ptr<TestClass> shared = fl::make_shared<TestClass>(42);
    fl::weak_ptr<TestClass> weak(shared);
    
    FL_CHECK_EQ(weak.use_count(), 1);
    FL_CHECK(!weak.expired());
    
    // Self-assignment should not change anything
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_SELF_ASSIGN_OVERLOADED
    weak = weak;  // Test self-assignment
    FL_DISABLE_WARNING_POP
    FL_CHECK_EQ(weak.use_count(), 1);
    FL_CHECK(!weak.expired());
    FL_CHECK_EQ(weak.lock()->getValue(), 42);
    
    // Self-move assignment should not break anything
    weak = fl::move(weak);
    FL_CHECK_EQ(weak.use_count(), 1);
    FL_CHECK(!weak.expired());
    FL_CHECK_EQ(weak.lock()->getValue(), 42);
}

// Node class for testing circular references and self-assignment scenarios
class Node {
public:
    Node(int value) : mValue(value), mDestructorCalled(nullptr) {}
    Node(int value, bool* destructor_flag) : mValue(value), mDestructorCalled(destructor_flag) {}

    ~Node() {
        if (mDestructorCalled) {
            *mDestructorCalled = true;
        }
    }

    int getValue() const { return mValue; }
    void setValue(int value) { mValue = value; }

    void setNext(fl::shared_ptr<Node> next) { mNext = next; }
    fl::shared_ptr<Node> getNext() const { return mNext; }

    void setWeakNext(fl::weak_ptr<Node> next) { mWeakNext = next; }
    fl::weak_ptr<Node> getWeakNext() const { return mWeakNext; }

private:
    int mValue;
    bool* mDestructorCalled;
    fl::shared_ptr<Node> mNext;
    fl::weak_ptr<Node> mWeakNext;
};

TEST_CASE("fl::weak_ptr dead memory safety - basic scenario") {
    bool destructor_called = false;
    fl::weak_ptr<TestClass> weak;
    
    // Create shared_ptr and weak_ptr in a scope
    {
        auto shared = fl::make_shared<TestClass>(42);
        //shared = fl::shared_ptr<TestClass>(new TestClass(100, &destructor_called));
        shared = fl::make_shared<TestClass>(100, &destructor_called);
        weak = shared;
        
        FL_CHECK(!weak.expired());
        FL_CHECK_EQ(weak.use_count(), 1);
        FL_CHECK(!destructor_called);
        
        // Verify we can still access the object
        auto locked = weak.lock();
        FL_CHECK(locked);
        FL_CHECK_EQ(locked->getValue(), 100);
    } // shared_ptr goes out of scope here
    
    // Object should be destroyed, weak_ptr should be expired
    FL_CHECK(destructor_called);
    FL_CHECK(weak.expired());
    FL_CHECK_EQ(weak.use_count(), 0);
    
    // Attempting to lock should return empty shared_ptr - no segfault!
    auto locked = weak.lock();
    FL_CHECK(!locked);
    FL_CHECK_EQ(locked.get(), nullptr);
}

TEST_CASE("fl::weak_ptr dead memory safety - multiple weak_ptrs") {
    bool destructor_called = false;
    fl::weak_ptr<TestClass> weak1, weak2, weak3;
    
    {
        //auto shared = fl::shared_ptr<TestClass>(new TestClass(42, &destructor_called));
        auto shared = fl::make_shared<TestClass>(42, &destructor_called);
        weak1 = shared;
        weak2 = weak1;
        weak3 = shared;
        
        FL_CHECK_EQ(weak1.use_count(), 1);
        FL_CHECK_EQ(weak2.use_count(), 1);
        FL_CHECK_EQ(weak3.use_count(), 1);
        FL_CHECK(!destructor_called);
    } // shared_ptr destroyed
    
    // All weak_ptrs should be expired
    FL_CHECK(destructor_called);
    FL_CHECK(weak1.expired());
    FL_CHECK(weak2.expired());
    FL_CHECK(weak3.expired());
    
    // None should be able to lock
    FL_CHECK(!weak1.lock());
    FL_CHECK(!weak2.lock());
    FL_CHECK(!weak3.lock());
}

TEST_CASE("fl::weak_ptr dead memory safety - repeated lock attempts") {
    bool destructor_called = false;
    fl::weak_ptr<TestClass> weak;
    
    {
        //auto shared = fl::shared_ptr<TestClass>(new TestClass(42, &destructor_called));
        auto shared = fl::make_shared<TestClass>(42, &destructor_called);
        weak = shared;
    }
    
    FL_CHECK(destructor_called);
    FL_CHECK(weak.expired());
    
    // Try locking multiple times - should never segfault
    for (int i = 0; i < 10; ++i) {
        auto locked = weak.lock();
        FL_CHECK(!locked);
    }
}

TEST_CASE("fl::weak_ptr circular reference - basic linked list") {
    bool nodeA_destroyed = false;
    bool nodeB_destroyed = false;
    
    {
        //auto nodeA = fl::shared_ptr<Node>(new Node(1, &nodeA_destroyed));
        auto nodeA = fl::make_shared<Node>(1, &nodeA_destroyed);
        //auto nodeB = fl::shared_ptr<Node>(new Node(2, &nodeB_destroyed));
        auto nodeB = fl::make_shared<Node>(2, &nodeB_destroyed);
        
        // Create circular reference: A -> B -> A
        nodeA->setNext(nodeB);
        nodeB->setNext(nodeA);
        
        FL_CHECK_EQ(nodeA.use_count(), 2); // nodeA and nodeB->next_
        FL_CHECK_EQ(nodeB.use_count(), 2); // nodeB and nodeA->next_
        FL_CHECK(!nodeA_destroyed);
        FL_CHECK(!nodeB_destroyed);

        // Break the cycle to allow cleanup (otherwise LSAN detects leak)
        // In real code, you'd use weak_ptr to break cycles instead
        nodeA->setNext(nullptr);
    } // nodeA and nodeB local variables destroyed

    // After breaking the cycle, objects should be destroyed
    FL_CHECK(nodeA_destroyed);
    FL_CHECK(nodeB_destroyed);
}

TEST_CASE("fl::weak_ptr circular reference - broken with weak_ptr") {
    bool nodeA_destroyed = false;
    bool nodeB_destroyed = false;
    
    {
        //auto nodeA = fl::shared_ptr<Node>(new Node(1, &nodeA_destroyed));
        auto nodeA = fl::make_shared<Node>(1, &nodeA_destroyed);
        //auto nodeB = fl::shared_ptr<Node>(new Node(2, &nodeB_destroyed));
        auto nodeB = fl::make_shared<Node>(2, &nodeB_destroyed);
        
        // Create non-circular reference: A -> B, A <- weak B
        nodeA->setNext(nodeB);
        nodeB->setWeakNext(nodeA); // Use weak_ptr to break cycle
        
        FL_CHECK_EQ(nodeA.use_count(), 1); // Only nodeA variable
        FL_CHECK_EQ(nodeB.use_count(), 2); // nodeB variable and nodeA->next_
        FL_CHECK(!nodeA_destroyed);
        FL_CHECK(!nodeB_destroyed);
    } // nodeA and nodeB local variables destroyed
    
    // Objects should be properly destroyed since cycle is broken
    FL_CHECK(nodeA_destroyed);
    FL_CHECK(nodeB_destroyed);
}

TEST_CASE("fl::weak_ptr self-assignment safety - a = b scenario") {
    bool nodeA_destroyed = false;
    bool nodeB_destroyed = false;
    
    //auto nodeA = fl::shared_ptr<Node>(new Node(1, &nodeA_destroyed));
    auto nodeA = fl::make_shared<Node>(1, &nodeA_destroyed);
    //auto nodeB = fl::shared_ptr<Node>(new Node(2, &nodeB_destroyed));
    auto nodeB = fl::make_shared<Node>(2, &nodeB_destroyed);
    
    // Test the scenario: a -> b, and we have a, and a = b
    nodeA->setNext(nodeB);
    
    // Verify initial state
    FL_CHECK_EQ(nodeA->getValue(), 1);
    FL_CHECK_EQ(nodeB->getValue(), 2);
    FL_CHECK_EQ(nodeA->getNext().get(), nodeB.get());
    FL_CHECK_EQ(nodeA.use_count(), 1); // Only nodeA variable
    FL_CHECK_EQ(nodeB.use_count(), 2); // nodeB variable + nodeA->next_
    FL_CHECK(!nodeA_destroyed);
    FL_CHECK(!nodeB_destroyed);
    
    // Get a reference to A before the dangerous assignment
    auto aRef = nodeA;
    FL_CHECK_EQ(aRef.get(), nodeA.get());
    FL_CHECK_EQ(nodeA.use_count(), 2); // nodeA + aRef
    FL_CHECK_EQ(nodeB.use_count(), 2); // nodeB + nodeA->next_
    
    // Now do the dangerous assignment: a = b (while a is referenced through aRef)
    // This could cause issues if a gets destroyed while setting itself to b
    nodeA = nodeB; // a = b (dangerous assignment)
    
    // Verify no segfault occurred and state is consistent
    FL_CHECK_EQ(nodeA.get(), nodeB.get()); // nodeA should now point to nodeB
    FL_CHECK_EQ(nodeA->getValue(), 2); // Should have nodeB's value
    FL_CHECK_EQ(nodeB->getValue(), 2); // nodeB unchanged
    FL_CHECK(!nodeA_destroyed); // Original nodeA object should still exist
    FL_CHECK(!nodeB_destroyed);
    
    // aRef should still be valid (original nodeA should still exist)
    FL_CHECK(aRef);
    FL_CHECK_EQ(aRef->getValue(), 1); // Original nodeA value
    FL_CHECK_EQ(aRef.use_count(), 1); // Only aRef now points to original nodeA
    
    // nodeB should now have increased reference count
    FL_CHECK_EQ(nodeB.use_count(), 3); // nodeB + nodeA + nodeA->next_ (which points to nodeB)
    
    // Clean up - clear the circular reference in the original node
    aRef->setNext(nullptr);
    FL_CHECK_EQ(nodeB.use_count(), 2); // nodeB + nodeA
    FL_CHECK(!nodeA_destroyed); // Original nodeA still referenced by aRef
    FL_CHECK(!nodeB_destroyed);
    
    // Clear the reference to original nodeA
    aRef.reset();
    FL_CHECK(nodeA_destroyed); // Now original nodeA should be destroyed
    FL_CHECK(!nodeB_destroyed); // nodeB still referenced by nodeA
    
    // Clear final reference
    nodeA.reset();
    nodeB.reset();
    FL_CHECK(nodeB_destroyed); // Now nodeB should be destroyed
}

TEST_CASE("fl::weak_ptr complex circular scenario with weak references") {
    bool nodeA_destroyed = false;
    bool nodeB_destroyed = false;
    bool nodeC_destroyed = false;
    
    fl::weak_ptr<Node> weakA, weakB, weakC;
    
    {
        //auto nodeA = fl::shared_ptr<Node>(new Node(1, &nodeA_destroyed));
        auto nodeA = fl::make_shared<Node>(1, &nodeA_destroyed);
        //auto nodeB = fl::shared_ptr<Node>(new Node(2, &nodeB_destroyed));
        auto nodeB = fl::make_shared<Node>(2, &nodeB_destroyed);
        //auto nodeC = fl::shared_ptr<Node>(new Node(3, &nodeC_destroyed));
        auto nodeC = fl::make_shared<Node>(3, &nodeC_destroyed);
        
        // Create complex references: A -> B -> C, with weak back-references
        nodeA->setNext(nodeB);
        nodeB->setNext(nodeC);
        nodeC->setWeakNext(nodeA); // Weak reference back to A
        
        weakA = nodeA;
        weakB = nodeB;
        weakC = nodeC;
        
        FL_CHECK(!weakA.expired());
        FL_CHECK(!weakB.expired());
        FL_CHECK(!weakC.expired());
        FL_CHECK(!nodeA_destroyed);
        FL_CHECK(!nodeB_destroyed);
        FL_CHECK(!nodeC_destroyed);
    } // All shared_ptr variables destroyed
    
    // All objects should be destroyed since no circular strong references exist
    FL_CHECK(nodeA_destroyed);
    FL_CHECK(nodeB_destroyed);
    FL_CHECK(nodeC_destroyed);
    FL_CHECK(weakA.expired());
    FL_CHECK(weakB.expired());
    FL_CHECK(weakC.expired());
}

TEST_CASE("fl::weak_ptr stress test - rapid creation and destruction") {
    fl::vector<fl::weak_ptr<TestClass>> weak_ptrs;
    weak_ptrs.reserve(100);
    
    // Create many shared_ptrs and weak_ptrs rapidly
    for (int i = 0; i < 100; ++i) {
        auto shared = fl::make_shared<TestClass>(i);
        weak_ptrs.push_back(fl::weak_ptr<TestClass>(shared));
        
        // Shared goes out of scope immediately - weak_ptr should handle this
    }
    
    // All weak_ptrs should be expired
    for (auto& weak : weak_ptrs) {
        FL_CHECK(weak.expired());
        FL_CHECK(!weak.lock());
    }
}

} // namespace weak_ptr_test
