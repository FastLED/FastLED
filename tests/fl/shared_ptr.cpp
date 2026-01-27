#include "fl/stl/shared_ptr.h"
#include "fl/compiler_control.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/stl/move.h"
#include "fl/stl/vector.h"

namespace shared_ptr_test {

// Test class that does NOT inherit from fl::Referent (non-intrusive)
class TestClass {
public:
    TestClass() : mValue(0), mDestructorCalled(nullptr) {}
    TestClass(int value) : mValue(value), mDestructorCalled(nullptr) {}
    TestClass(int a, int b) : mValue(a + b), mDestructorCalled(nullptr) {}

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

// Derived class for testing polymorphism
class DerivedTestClass : public TestClass {
public:
    DerivedTestClass() : TestClass(), mExtraValue(0) {}
    DerivedTestClass(int value, int extra) : TestClass(value), mExtraValue(extra) {}

    int getExtraValue() const { return mExtraValue; }

private:
    int mExtraValue;
};

// Custom deleter for testing
struct CustomDeleter {
    mutable bool* called_flag;
    
    CustomDeleter() : called_flag(new bool(false)) {}
    
    // Copy constructor shares the flag
    CustomDeleter(const CustomDeleter& other) : called_flag(other.called_flag) {}
    
    // Assignment operator shares the flag  
    CustomDeleter& operator=(const CustomDeleter& other) {
        called_flag = other.called_flag;
        return *this;
    }
    
    ~CustomDeleter() {
        // Don't delete called_flag here since it's shared
    }
    
    void operator()(TestClass* ptr) const {
        *called_flag = true;
        delete ptr;
    }
    
    bool called() const { return *called_flag; }
};

TEST_CASE("fl::shared_ptr default construction") {
    fl::shared_ptr<TestClass> ptr;
    FL_CHECK(!ptr);
    FL_CHECK_EQ(ptr.get(), nullptr);
    FL_CHECK_EQ(ptr.use_count(), 0);
    FL_CHECK(!ptr.unique());
}

TEST_CASE("fl::shared_ptr nullptr construction") {
    fl::shared_ptr<TestClass> ptr(nullptr);
    FL_CHECK(!ptr);
    FL_CHECK_EQ(ptr.get(), nullptr);
    FL_CHECK_EQ(ptr.use_count(), 0);
}

TEST_CASE("fl::shared_ptr construction from raw pointer") {
    bool destructor_called = false;
    {
        fl::shared_ptr<TestClass> ptr = fl::make_shared<TestClass>(42, &destructor_called);
        FL_CHECK(ptr);
        FL_CHECK_NE(ptr.get(), nullptr);
        FL_CHECK_EQ(ptr->getValue(), 42);
        FL_CHECK_EQ(ptr.use_count(), 1);
        FL_CHECK(ptr.unique());
    }
    // Destructor should be called when shared_ptr goes out of scope
    FL_CHECK(destructor_called);
}

TEST_CASE("fl::shared_ptr construction with custom deleter") {
    CustomDeleter deleter;
    {
        //fl::shared_ptr<TestClass> ptr(new TestClass(42), deleter);
        fl::shared_ptr<TestClass> ptr = fl::make_shared_with_deleter<TestClass>(deleter, 42);
        FL_CHECK(ptr);
        FL_CHECK_EQ(ptr->getValue(), 42);
        FL_CHECK_EQ(ptr.use_count(), 1);
    }
    // Custom deleter should be called
    FL_CHECK(deleter.called());
}

TEST_CASE("fl::shared_ptr copy construction") {
    //fl::shared_ptr<TestClass> ptr1(new TestClass(42));
    fl::shared_ptr<TestClass> ptr1 = fl::make_shared<TestClass>(42);
    FL_CHECK_EQ(ptr1.use_count(), 1);
    
    fl::shared_ptr<TestClass> ptr2(ptr1);
    FL_CHECK_EQ(ptr1.use_count(), 2);
    FL_CHECK_EQ(ptr2.use_count(), 2);
    FL_CHECK_EQ(ptr1.get(), ptr2.get());
    FL_CHECK_EQ(ptr2->getValue(), 42);
}

TEST_CASE("fl::shared_ptr move construction") {
    //fl::shared_ptr<TestClass> ptr1(new TestClass(42));
    fl::shared_ptr<TestClass> ptr1 = fl::make_shared<TestClass>(42);
    TestClass* raw_ptr = ptr1.get();
    FL_CHECK_EQ(ptr1.use_count(), 1);
    
    fl::shared_ptr<TestClass> ptr2(fl::move(ptr1));
    FL_CHECK_EQ(ptr1.get(), nullptr);
    FL_CHECK_EQ(ptr1.use_count(), 0);
    FL_CHECK_EQ(ptr2.get(), raw_ptr);
    FL_CHECK_EQ(ptr2.use_count(), 1);
    FL_CHECK_EQ(ptr2->getValue(), 42);
}

TEST_CASE("fl::shared_ptr assignment operator") {
    //fl::shared_ptr<TestClass> ptr1(new TestClass(42));
    fl::shared_ptr<TestClass> ptr1 = fl::make_shared<TestClass>(42);
    //fl::shared_ptr<TestClass> ptr2(new TestClass(100));
    fl::shared_ptr<TestClass> ptr2 = fl::make_shared<TestClass>(100);
    
    FL_CHECK_EQ(ptr1.use_count(), 1);
    FL_CHECK_EQ(ptr2.use_count(), 1);
    FL_CHECK_NE(ptr1.get(), ptr2.get());
    
    ptr2 = ptr1;
    FL_CHECK_EQ(ptr1.use_count(), 2);
    FL_CHECK_EQ(ptr2.use_count(), 2);
    FL_CHECK_EQ(ptr1.get(), ptr2.get());
    FL_CHECK_EQ(ptr2->getValue(), 42);
}

TEST_CASE("fl::shared_ptr move assignment") {
    //fl::shared_ptr<TestClass> ptr1(new TestClass(42));
    fl::shared_ptr<TestClass> ptr1 = fl::make_shared<TestClass>(42);
    //fl::shared_ptr<TestClass> ptr2(new TestClass(100));
    fl::shared_ptr<TestClass> ptr2 = fl::make_shared<TestClass>(100);
    TestClass* raw_ptr = ptr1.get();
    
    ptr2 = fl::move(ptr1);
    FL_CHECK_EQ(ptr1.get(), nullptr);
    FL_CHECK_EQ(ptr1.use_count(), 0);
    FL_CHECK_EQ(ptr2.get(), raw_ptr);
    FL_CHECK_EQ(ptr2.use_count(), 1);
    FL_CHECK_EQ(ptr2->getValue(), 42);
}

TEST_CASE("fl::shared_ptr reset functionality") {
    bool destructor_called = false;
    //fl::shared_ptr<TestClass> ptr(new TestClass(42, &destructor_called));
    fl::shared_ptr<TestClass> ptr = fl::make_shared<TestClass>(42, &destructor_called);
    FL_CHECK(ptr);
    FL_CHECK_EQ(ptr.use_count(), 1);
    
    ptr.reset();
    FL_CHECK(!ptr);
    FL_CHECK_EQ(ptr.get(), nullptr);
    FL_CHECK_EQ(ptr.use_count(), 0);
    FL_CHECK(destructor_called);
}

TEST_CASE("fl::shared_ptr reset with new pointer") {
    //fl::shared_ptr<TestClass> ptr(new TestClass(42));
    fl::shared_ptr<TestClass> ptr = fl::make_shared<TestClass>(42);
    FL_CHECK_EQ(ptr->getValue(), 42);
    
    //ptr.reset(new TestClass(100));
    ptr.reset(fl::make_shared<TestClass>(100));
    FL_CHECK_EQ(ptr->getValue(), 100);
    FL_CHECK_EQ(ptr.use_count(), 1);
}

TEST_CASE("fl::shared_ptr reset with custom deleter") {
    CustomDeleter deleter;
    //fl::shared_ptr<TestClass> ptr(new TestClass(42));
    fl::shared_ptr<TestClass> ptr = fl::make_shared<TestClass>(42);
    
    //ptr.reset(new TestClass(100), deleter);
    ptr.reset(fl::make_shared_with_deleter<TestClass>(deleter, 100));
    FL_CHECK_EQ(ptr->getValue(), 100);
    FL_CHECK_EQ(ptr.use_count(), 1);
    
    ptr.reset();
    FL_CHECK(deleter.called());
}

TEST_CASE("fl::shared_ptr swap functionality") {
    //fl::shared_ptr<TestClass> ptr1(new TestClass(42));
    fl::shared_ptr<TestClass> ptr1 = fl::make_shared<TestClass>(42);
    //fl::shared_ptr<TestClass> ptr2(new TestClass(100));
    fl::shared_ptr<TestClass> ptr2 = fl::make_shared<TestClass>(100);
    TestClass* raw_ptr1 = ptr1.get();
    TestClass* raw_ptr2 = ptr2.get();
    
    ptr1.swap(ptr2);
    FL_CHECK_EQ(ptr1.get(), raw_ptr2);
    FL_CHECK_EQ(ptr2.get(), raw_ptr1);
    FL_CHECK_EQ(ptr1->getValue(), 100);
    FL_CHECK_EQ(ptr2->getValue(), 42);
}

TEST_CASE("fl::shared_ptr operator* and operator->") {
    //fl::shared_ptr<TestClass> ptr(new TestClass(42));
    fl::shared_ptr<TestClass> ptr = fl::make_shared<TestClass>(42);
    
    FL_CHECK_EQ((*ptr).getValue(), 42);
    FL_CHECK_EQ(ptr->getValue(), 42);
    
    ptr->setValue(100);
    FL_CHECK_EQ(ptr->getValue(), 100);
}

TEST_CASE("fl::shared_ptr bool conversion") {
    fl::shared_ptr<TestClass> ptr1;
    //fl::shared_ptr<TestClass> ptr2(new TestClass(42));
    fl::shared_ptr<TestClass> ptr2 = fl::make_shared<TestClass>(42);
    
    FL_CHECK(!ptr1);
    FL_CHECK(ptr2);
    
    if (ptr1) {
        FAIL("Null pointer should be false");
    }
    
    if (!ptr2) {
        FAIL("Valid pointer should be true");
    }
}

TEST_CASE("fl::shared_ptr comparison operators") {
    //fl::shared_ptr<TestClass> ptr1(new TestClass(42));
    fl::shared_ptr<TestClass> ptr1 = fl::make_shared<TestClass>(42);
    fl::shared_ptr<TestClass> ptr2(ptr1);
    //fl::shared_ptr<TestClass> ptr3(new TestClass(100));
    fl::shared_ptr<TestClass> ptr3 = fl::make_shared<TestClass>(100);
    fl::shared_ptr<TestClass> null_ptr;
    
    // Equality - using CHECK_EQ for better error messages
    FL_CHECK_EQ(ptr1 == ptr2, true);
    FL_CHECK_EQ(ptr1 == ptr3, false);
    FL_CHECK_EQ(null_ptr == nullptr, true);
    FL_CHECK_EQ(nullptr == null_ptr, true);
    FL_CHECK_EQ(ptr1 == nullptr, false);
    
    // Inequality - using CHECK_EQ for better error messages
    FL_CHECK_EQ(ptr1 != ptr2, false);
    FL_CHECK_EQ(ptr1 != ptr3, true);
    FL_CHECK_EQ(null_ptr != nullptr, false);
    FL_CHECK_EQ(ptr1 != nullptr, true);
}

TEST_CASE("fl::shared_ptr polymorphism") {
    // Test with derived class
    //fl::shared_ptr<DerivedTestClass> derived_ptr(new DerivedTestClass(42, 100));
    fl::shared_ptr<DerivedTestClass> derived_ptr = fl::make_shared<DerivedTestClass>(42, 100);
    fl::shared_ptr<TestClass> base_ptr(derived_ptr);
    
    FL_CHECK_EQ(base_ptr.use_count(), 2);
    FL_CHECK_EQ(derived_ptr.use_count(), 2);
    FL_CHECK_EQ(base_ptr->getValue(), 42);
    
    // Both should point to the same object
    FL_CHECK_EQ(base_ptr.get(), derived_ptr.get());
}

TEST_CASE("fl::make_shared basic functionality") {
    // Test default constructor
    auto ptr1 = fl::make_shared<TestClass>();
    FL_CHECK(ptr1);
    FL_CHECK_EQ(ptr1->getValue(), 0);
    FL_CHECK_EQ(ptr1.use_count(), 1);
    
    // Test single argument constructor
    auto ptr2 = fl::make_shared<TestClass>(42);
    FL_CHECK(ptr2);
    FL_CHECK_EQ(ptr2->getValue(), 42);
    FL_CHECK_EQ(ptr2.use_count(), 1);
    
    // Test multiple argument constructor
    auto ptr3 = fl::make_shared<TestClass>(10, 20);
    FL_CHECK(ptr3);
    FL_CHECK_EQ(ptr3->getValue(), 30);
    FL_CHECK_EQ(ptr3.use_count(), 1);
}

TEST_CASE("fl::make_shared memory optimization") {
    // make_shared should use inlined storage for better performance
    auto ptr = fl::make_shared<TestClass>(42);
    FL_CHECK(ptr);
    FL_CHECK_EQ(ptr->getValue(), 42);
    FL_CHECK_EQ(ptr.use_count(), 1);
    
    // Test copy construction with make_shared
    auto ptr2 = ptr;
    FL_CHECK_EQ(ptr.use_count(), 2);
    FL_CHECK_EQ(ptr2.use_count(), 2);
    FL_CHECK_EQ(ptr.get(), ptr2.get());
}

TEST_CASE("fl::shared_ptr reference counting stress test") {
    const int NUM_COPIES = 10;
    //fl::shared_ptr<TestClass> original(new TestClass(42));
    fl::shared_ptr<TestClass> original = fl::make_shared<TestClass>(42);
    FL_CHECK_EQ(original.use_count(), 1);
    
    // Create multiple copies
    fl::vector<fl::shared_ptr<TestClass>> copies;
    for (int i = 0; i < NUM_COPIES; ++i) {
        copies.push_back(original);
        FL_CHECK_EQ(original.use_count(), i + 2);
    }
    
    // Verify all copies point to the same object
    for (const auto& copy : copies) {
        FL_CHECK_EQ(copy.get(), original.get());
        FL_CHECK_EQ(copy->getValue(), 42);
        FL_CHECK_EQ(copy.use_count(), NUM_COPIES + 1);
    }
    
    // Clear copies one by one
    for (int i = 0; i < NUM_COPIES; ++i) {
        copies.pop_back();
        FL_CHECK_EQ(original.use_count(), NUM_COPIES - i);
    }
    
    FL_CHECK_EQ(original.use_count(), 1);
    FL_CHECK(original.unique());
}

TEST_CASE("fl::shared_ptr destruction order") {
    bool destructor_called = false;
    {
        //fl::shared_ptr<TestClass> ptr1(new TestClass(42, &destructor_called));
        fl::shared_ptr<TestClass> ptr1 = fl::make_shared<TestClass>(42, &destructor_called);
        {
            fl::shared_ptr<TestClass> ptr2 = ptr1;
            FL_CHECK_EQ(ptr1.use_count(), 2);
            FL_CHECK(!destructor_called);
        }
        FL_CHECK_EQ(ptr1.use_count(), 1);
        FL_CHECK(!destructor_called);
    }
    FL_CHECK(destructor_called);
}

TEST_CASE("fl::shared_ptr self-assignment safety") {
    //fl::shared_ptr<TestClass> ptr(new TestClass(42));
    fl::shared_ptr<TestClass> ptr = fl::make_shared<TestClass>(42);
    FL_CHECK_EQ(ptr.use_count(), 1);
    
    // Self-assignment should not change anything
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_SELF_ASSIGN_OVERLOADED
    ptr = ptr;  // Test self-assignment
    FL_DISABLE_WARNING_POP
    FL_CHECK_EQ(ptr.use_count(), 1);
    FL_CHECK_EQ(ptr->getValue(), 42);
    
    // Self-move assignment should not break anything
    ptr = fl::move(ptr);
    FL_CHECK_EQ(ptr.use_count(), 1);
    FL_CHECK_EQ(ptr->getValue(), 42);
}

// Node class for testing circular references and self-assignment scenarios
class SharedNode {
public:
    SharedNode(int value) : mValue(value), mDestructorCalled(nullptr) {}
    SharedNode(int value, bool* destructor_flag) : mValue(value), mDestructorCalled(destructor_flag) {}

    ~SharedNode() {
        if (mDestructorCalled) {
            *mDestructorCalled = true;
        }
    }

    int getValue() const { return mValue; }
    void setValue(int value) { mValue = value; }

    void setNext(fl::shared_ptr<SharedNode> next) { mNext = next; }
    fl::shared_ptr<SharedNode> getNext() const { return mNext; }

private:
    int mValue;
    bool* mDestructorCalled;
    fl::shared_ptr<SharedNode> mNext;
};

TEST_CASE("fl::shared_ptr self-assignment safety - a = b scenario") {
    bool nodeA_destroyed = false;
    bool nodeB_destroyed = false;
    
    auto nodeA = fl::make_shared<SharedNode>(1, &nodeA_destroyed);
    auto nodeB = fl::make_shared<SharedNode>(2, &nodeB_destroyed);
    
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

} // namespace shared_ptr_test
