#include "test.h"
#include "fl/shared_ptr.h"
#include "fl/weak_ptr.h"
#include "fl/memory.h"
#include "fl/compiler_control.h"
#include "fl/vector.h"

// Test class that does NOT inherit from fl::Referent (non-intrusive)
class TestClass {
public:
    TestClass() : value_(0), destructor_called_(nullptr) {}
    TestClass(int value) : value_(value), destructor_called_(nullptr) {}
    
    // Constructor that allows tracking destructor calls
    TestClass(int value, bool* destructor_flag) : value_(value), destructor_called_(destructor_flag) {}
    
    ~TestClass() {
        if (destructor_called_) {
            *destructor_called_ = true;
        }
    }
    
    int getValue() const { return value_; }
    void setValue(int value) { value_ = value; }
    
private:
    int value_;
    bool* destructor_called_;
};

TEST_CASE("fl::weak_ptr default construction") {
    fl::weak_ptr<TestClass> weak;
    CHECK_EQ(weak.use_count(), 0);
    CHECK(weak.expired());
    
    auto shared = weak.lock();
    CHECK(!shared);
    CHECK_EQ(shared.get(), nullptr);
}

TEST_CASE("fl::weak_ptr construction from shared_ptr") {
    fl::shared_ptr<TestClass> shared = fl::make_shared<TestClass>(42);
    CHECK_EQ(shared.use_count(), 1);
    
    fl::weak_ptr<TestClass> weak(shared);
    CHECK_EQ(weak.use_count(), 1);
    CHECK_EQ(shared.use_count(), 1); // weak_ptr doesn't increase shared count
    CHECK(!weak.expired());
    
    auto locked = weak.lock();
    CHECK(locked);
    CHECK_EQ(locked.use_count(), 2); // lock() creates a shared_ptr
    CHECK_EQ(locked->getValue(), 42);
}

TEST_CASE("fl::weak_ptr copy construction") {
    fl::shared_ptr<TestClass> shared = fl::make_shared<TestClass>(42);
    fl::weak_ptr<TestClass> weak1(shared);
    fl::weak_ptr<TestClass> weak2(weak1);
    
    CHECK_EQ(weak1.use_count(), 1);
    CHECK_EQ(weak2.use_count(), 1);
    CHECK(!weak1.expired());
    CHECK(!weak2.expired());
    
    auto locked1 = weak1.lock();
    auto locked2 = weak2.lock();
    CHECK_EQ(locked1.get(), locked2.get());
    CHECK_EQ(locked1->getValue(), 42);
}

TEST_CASE("fl::weak_ptr move construction") {
    fl::shared_ptr<TestClass> shared = fl::make_shared<TestClass>(42);
    fl::weak_ptr<TestClass> weak1(shared);
    fl::weak_ptr<TestClass> weak2(fl::move(weak1));
    
    CHECK_EQ(weak1.use_count(), 0);
    CHECK(weak1.expired());
    CHECK_EQ(weak2.use_count(), 1);
    CHECK(!weak2.expired());
    
    auto locked = weak2.lock();
    CHECK(locked);
    CHECK_EQ(locked->getValue(), 42);
}

TEST_CASE("fl::weak_ptr assignment from shared_ptr") {
    fl::shared_ptr<TestClass> shared1 = fl::make_shared<TestClass>(42);
    fl::shared_ptr<TestClass> shared2 = fl::make_shared<TestClass>(100);
    fl::weak_ptr<TestClass> weak(shared1);
    
    CHECK_EQ(weak.lock()->getValue(), 42);
    
    weak = shared2;
    CHECK_EQ(weak.lock()->getValue(), 100);
}

TEST_CASE("fl::weak_ptr assignment from weak_ptr") {
    fl::shared_ptr<TestClass> shared1 = fl::make_shared<TestClass>(42);
    fl::shared_ptr<TestClass> shared2 = fl::make_shared<TestClass>(100);
    fl::weak_ptr<TestClass> weak1(shared1);
    fl::weak_ptr<TestClass> weak2(shared2);
    
    CHECK_EQ(weak1.lock()->getValue(), 42);
    CHECK_EQ(weak2.lock()->getValue(), 100);
    
    weak1 = weak2;
    CHECK_EQ(weak1.lock()->getValue(), 100);
    CHECK_EQ(weak2.lock()->getValue(), 100);
}

TEST_CASE("fl::weak_ptr move assignment") {
    fl::shared_ptr<TestClass> shared1 = fl::make_shared<TestClass>(42);
    fl::shared_ptr<TestClass> shared2 = fl::make_shared<TestClass>(100);
    fl::weak_ptr<TestClass> weak1(shared1);
    fl::weak_ptr<TestClass> weak2(shared2);
    
    weak1 = fl::move(weak2);
    CHECK_EQ(weak1.lock()->getValue(), 100);
    CHECK(weak2.expired());
}

TEST_CASE("fl::weak_ptr expiration when shared_ptr destroyed") {
    bool destructor_called = false;
    fl::weak_ptr<TestClass> weak;
    
    {
        fl::shared_ptr<TestClass> shared = fl::make_shared<TestClass>(42, &destructor_called);
        weak = shared;
        CHECK(!weak.expired());
        CHECK_EQ(weak.use_count(), 1);
        CHECK(!destructor_called);
        
        auto locked = weak.lock();
        CHECK(locked);
        CHECK_EQ(locked->getValue(), 42);
    }
    
    // shared_ptr is destroyed, object should be destroyed
    CHECK(destructor_called);
    CHECK(weak.expired());
    CHECK_EQ(weak.use_count(), 0);
    
    auto locked = weak.lock();
    CHECK(!locked);
    CHECK_EQ(locked.get(), nullptr);
}

TEST_CASE("fl::weak_ptr with multiple shared_ptr references") {
    fl::shared_ptr<TestClass> shared1 = fl::make_shared<TestClass>(42);
    fl::shared_ptr<TestClass> shared2 = shared1;
    fl::weak_ptr<TestClass> weak(shared1);
    
    CHECK_EQ(shared1.use_count(), 2);
    CHECK_EQ(weak.use_count(), 2);
    CHECK(!weak.expired());
    
    shared1.reset();
    CHECK_EQ(shared2.use_count(), 1);
    CHECK_EQ(weak.use_count(), 1);
    CHECK(!weak.expired());
    
    shared2.reset();
    CHECK(weak.expired());
    CHECK_EQ(weak.use_count(), 0);
}

TEST_CASE("fl::weak_ptr reset functionality") {
    fl::shared_ptr<TestClass> shared = fl::make_shared<TestClass>(42);
    fl::weak_ptr<TestClass> weak(shared);
    
    CHECK(!weak.expired());
    CHECK_EQ(weak.use_count(), 1);
    
    weak.reset();
    CHECK(weak.expired());
    CHECK_EQ(weak.use_count(), 0);
    
    auto locked = weak.lock();
    CHECK(!locked);
}

TEST_CASE("fl::weak_ptr swap functionality") {
    fl::shared_ptr<TestClass> shared1 = fl::make_shared<TestClass>(42);
    fl::shared_ptr<TestClass> shared2 = fl::make_shared<TestClass>(100);
    fl::weak_ptr<TestClass> weak1(shared1);
    fl::weak_ptr<TestClass> weak2(shared2);
    
    CHECK_EQ(weak1.lock()->getValue(), 42);
    CHECK_EQ(weak2.lock()->getValue(), 100);
    
    weak1.swap(weak2);
    CHECK_EQ(weak1.lock()->getValue(), 100);
    CHECK_EQ(weak2.lock()->getValue(), 42);
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
    CHECK(order1 != order2);
    
    // Test owner_before with shared_ptr
    bool order3 = weak1.owner_before(shared2);
    bool order4 = weak2.owner_before(shared1);
    CHECK(order3 != order4);
}

TEST_CASE("fl::weak_ptr conversion to shared_ptr") {
    fl::shared_ptr<TestClass> shared = fl::make_shared<TestClass>(42);
    fl::weak_ptr<TestClass> weak(shared);
    
    // Test construction from weak_ptr
    fl::shared_ptr<TestClass> converted(weak);
    CHECK(converted);
    CHECK_EQ(converted.use_count(), 2);
    CHECK_EQ(shared.use_count(), 2);
    CHECK_EQ(converted->getValue(), 42);
    CHECK_EQ(converted.get(), shared.get());
}

TEST_CASE("fl::weak_ptr conversion from expired weak_ptr") {
    fl::weak_ptr<TestClass> weak;
    
    {
        fl::shared_ptr<TestClass> shared = fl::make_shared<TestClass>(42);
        weak = shared;
        CHECK(!weak.expired());
    }
    
    CHECK(weak.expired());
    
    // Converting expired weak_ptr should result in empty shared_ptr
    fl::shared_ptr<TestClass> converted(weak);
    CHECK(!converted);
    CHECK_EQ(converted.get(), nullptr);
    CHECK_EQ(converted.use_count(), 0);
}

TEST_CASE("fl::weak_ptr multiple weak references") {
    fl::shared_ptr<TestClass> shared = fl::make_shared<TestClass>(42);
    fl::weak_ptr<TestClass> weak1(shared);
    fl::weak_ptr<TestClass> weak2(shared);
    fl::weak_ptr<TestClass> weak3(weak1);
    
    CHECK_EQ(shared.use_count(), 1);
    CHECK_EQ(weak1.use_count(), 1);
    CHECK_EQ(weak2.use_count(), 1);
    CHECK_EQ(weak3.use_count(), 1);
    
    shared.reset();
    
    CHECK(weak1.expired());
    CHECK(weak2.expired());
    CHECK(weak3.expired());
    CHECK_EQ(weak1.use_count(), 0);
    CHECK_EQ(weak2.use_count(), 0);
    CHECK_EQ(weak3.use_count(), 0);
}

TEST_CASE("fl::weak_ptr self-assignment safety") {
    fl::shared_ptr<TestClass> shared = fl::make_shared<TestClass>(42);
    fl::weak_ptr<TestClass> weak(shared);
    
    CHECK_EQ(weak.use_count(), 1);
    CHECK(!weak.expired());
    
    // Self-assignment should not change anything
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_SELF_ASSIGN_OVERLOADED
    weak = weak;  // Test self-assignment
    FL_DISABLE_WARNING_POP
    CHECK_EQ(weak.use_count(), 1);
    CHECK(!weak.expired());
    CHECK_EQ(weak.lock()->getValue(), 42);
    
    // Self-move assignment should not break anything
    weak = fl::move(weak);
    CHECK_EQ(weak.use_count(), 1);
    CHECK(!weak.expired());
    CHECK_EQ(weak.lock()->getValue(), 42);
}

// Node class for testing circular references and self-assignment scenarios
class Node {
public:
    Node(int value) : value_(value), destructor_called_(nullptr) {}
    Node(int value, bool* destructor_flag) : value_(value), destructor_called_(destructor_flag) {}
    
    ~Node() {
        if (destructor_called_) {
            *destructor_called_ = true;
        }
    }
    
    int getValue() const { return value_; }
    void setValue(int value) { value_ = value; }
    
    void setNext(fl::shared_ptr<Node> next) { next_ = next; }
    fl::shared_ptr<Node> getNext() const { return next_; }
    
    void setWeakNext(fl::weak_ptr<Node> next) { weak_next_ = next; }
    fl::weak_ptr<Node> getWeakNext() const { return weak_next_; }
    
private:
    int value_;
    bool* destructor_called_;
    fl::shared_ptr<Node> next_;
    fl::weak_ptr<Node> weak_next_;
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
        
        CHECK(!weak.expired());
        CHECK_EQ(weak.use_count(), 1);
        CHECK(!destructor_called);
        
        // Verify we can still access the object
        auto locked = weak.lock();
        CHECK(locked);
        CHECK_EQ(locked->getValue(), 100);
    } // shared_ptr goes out of scope here
    
    // Object should be destroyed, weak_ptr should be expired
    CHECK(destructor_called);
    CHECK(weak.expired());
    CHECK_EQ(weak.use_count(), 0);
    
    // Attempting to lock should return empty shared_ptr - no segfault!
    auto locked = weak.lock();
    CHECK(!locked);
    CHECK_EQ(locked.get(), nullptr);
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
        
        CHECK_EQ(weak1.use_count(), 1);
        CHECK_EQ(weak2.use_count(), 1);
        CHECK_EQ(weak3.use_count(), 1);
        CHECK(!destructor_called);
    } // shared_ptr destroyed
    
    // All weak_ptrs should be expired
    CHECK(destructor_called);
    CHECK(weak1.expired());
    CHECK(weak2.expired());
    CHECK(weak3.expired());
    
    // None should be able to lock
    CHECK(!weak1.lock());
    CHECK(!weak2.lock());
    CHECK(!weak3.lock());
}

TEST_CASE("fl::weak_ptr dead memory safety - repeated lock attempts") {
    bool destructor_called = false;
    fl::weak_ptr<TestClass> weak;
    
    {
        //auto shared = fl::shared_ptr<TestClass>(new TestClass(42, &destructor_called));
        auto shared = fl::make_shared<TestClass>(42, &destructor_called);
        weak = shared;
    }
    
    CHECK(destructor_called);
    CHECK(weak.expired());
    
    // Try locking multiple times - should never segfault
    for (int i = 0; i < 10; ++i) {
        auto locked = weak.lock();
        CHECK(!locked);
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
        
        CHECK_EQ(nodeA.use_count(), 2); // nodeA and nodeB->next_
        CHECK_EQ(nodeB.use_count(), 2); // nodeB and nodeA->next_
        CHECK(!nodeA_destroyed);
        CHECK(!nodeB_destroyed);
    } // nodeA and nodeB local variables destroyed
    
    // Due to circular reference, objects are NOT destroyed yet
    // This is expected behavior - circular references prevent cleanup
    // In real code, you'd use weak_ptr to break cycles
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
        
        CHECK_EQ(nodeA.use_count(), 1); // Only nodeA variable
        CHECK_EQ(nodeB.use_count(), 2); // nodeB variable and nodeA->next_
        CHECK(!nodeA_destroyed);
        CHECK(!nodeB_destroyed);
    } // nodeA and nodeB local variables destroyed
    
    // Objects should be properly destroyed since cycle is broken
    CHECK(nodeA_destroyed);
    CHECK(nodeB_destroyed);
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
    CHECK_EQ(nodeA->getValue(), 1);
    CHECK_EQ(nodeB->getValue(), 2);
    CHECK_EQ(nodeA->getNext().get(), nodeB.get());
    CHECK_EQ(nodeA.use_count(), 1); // Only nodeA variable
    CHECK_EQ(nodeB.use_count(), 2); // nodeB variable + nodeA->next_
    CHECK(!nodeA_destroyed);
    CHECK(!nodeB_destroyed);
    
    // Get a reference to A before the dangerous assignment
    auto aRef = nodeA;
    CHECK_EQ(aRef.get(), nodeA.get());
    CHECK_EQ(nodeA.use_count(), 2); // nodeA + aRef
    CHECK_EQ(nodeB.use_count(), 2); // nodeB + nodeA->next_
    
    // Now do the dangerous assignment: a = b (while a is referenced through aRef)
    // This could cause issues if a gets destroyed while setting itself to b
    nodeA = nodeB; // a = b (dangerous assignment)
    
    // Verify no segfault occurred and state is consistent
    CHECK_EQ(nodeA.get(), nodeB.get()); // nodeA should now point to nodeB
    CHECK_EQ(nodeA->getValue(), 2); // Should have nodeB's value
    CHECK_EQ(nodeB->getValue(), 2); // nodeB unchanged
    CHECK(!nodeA_destroyed); // Original nodeA object should still exist
    CHECK(!nodeB_destroyed);
    
    // aRef should still be valid (original nodeA should still exist)
    CHECK(aRef);
    CHECK_EQ(aRef->getValue(), 1); // Original nodeA value
    CHECK_EQ(aRef.use_count(), 1); // Only aRef now points to original nodeA
    
    // nodeB should now have increased reference count
    CHECK_EQ(nodeB.use_count(), 3); // nodeB + nodeA + nodeA->next_ (which points to nodeB)
    
    // Clean up - clear the circular reference in the original node
    aRef->setNext(nullptr);
    CHECK_EQ(nodeB.use_count(), 2); // nodeB + nodeA
    CHECK(!nodeA_destroyed); // Original nodeA still referenced by aRef
    CHECK(!nodeB_destroyed);
    
    // Clear the reference to original nodeA
    aRef.reset();
    CHECK(nodeA_destroyed); // Now original nodeA should be destroyed
    CHECK(!nodeB_destroyed); // nodeB still referenced by nodeA
    
    // Clear final reference
    nodeA.reset();
    nodeB.reset();
    CHECK(nodeB_destroyed); // Now nodeB should be destroyed
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
        
        CHECK(!weakA.expired());
        CHECK(!weakB.expired());
        CHECK(!weakC.expired());
        CHECK(!nodeA_destroyed);
        CHECK(!nodeB_destroyed);
        CHECK(!nodeC_destroyed);
    } // All shared_ptr variables destroyed
    
    // All objects should be destroyed since no circular strong references exist
    CHECK(nodeA_destroyed);
    CHECK(nodeB_destroyed);
    CHECK(nodeC_destroyed);
    CHECK(weakA.expired());
    CHECK(weakB.expired());
    CHECK(weakC.expired());
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
        CHECK(weak.expired());
        CHECK(!weak.lock());
    }
}
