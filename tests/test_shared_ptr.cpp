#include "test.h"
#include "fl/shared_ptr.h"
#include "fl/memory.h"
#include "fl/compiler_control.h"

// Test class that does NOT inherit from fl::Referent (non-intrusive)
class TestClass {
public:
    TestClass() : value_(0), destructor_called_(nullptr) {}
    TestClass(int value) : value_(value), destructor_called_(nullptr) {}
    TestClass(int a, int b) : value_(a + b), destructor_called_(nullptr) {}
    
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

// Derived class for testing polymorphism
class DerivedTestClass : public TestClass {
public:
    DerivedTestClass() : TestClass(), extra_value_(0) {}
    DerivedTestClass(int value, int extra) : TestClass(value), extra_value_(extra) {}
    
    int getExtraValue() const { return extra_value_; }
    
private:
    int extra_value_;
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
    CHECK(!ptr);
    CHECK_EQ(ptr.get(), nullptr);
    CHECK_EQ(ptr.use_count(), 0);
    CHECK(!ptr.unique());
}

TEST_CASE("fl::shared_ptr nullptr construction") {
    fl::shared_ptr<TestClass> ptr(nullptr);
    CHECK(!ptr);
    CHECK_EQ(ptr.get(), nullptr);
    CHECK_EQ(ptr.use_count(), 0);
}

TEST_CASE("fl::shared_ptr construction from raw pointer") {
    bool destructor_called = false;
    {
        fl::shared_ptr<TestClass> ptr = fl::make_shared<TestClass>(42, &destructor_called);
        CHECK(ptr);
        CHECK_NE(ptr.get(), nullptr);
        CHECK_EQ(ptr->getValue(), 42);
        CHECK_EQ(ptr.use_count(), 1);
        CHECK(ptr.unique());
    }
    // Destructor should be called when shared_ptr goes out of scope
    CHECK(destructor_called);
}

TEST_CASE("fl::shared_ptr construction with custom deleter") {
    CustomDeleter deleter;
    {
        //fl::shared_ptr<TestClass> ptr(new TestClass(42), deleter);
        fl::shared_ptr<TestClass> ptr = fl::make_shared_with_deleter<TestClass>(deleter, 42);
        CHECK(ptr);
        CHECK_EQ(ptr->getValue(), 42);
        CHECK_EQ(ptr.use_count(), 1);
    }
    // Custom deleter should be called
    CHECK(deleter.called());
}

TEST_CASE("fl::shared_ptr copy construction") {
    //fl::shared_ptr<TestClass> ptr1(new TestClass(42));
    fl::shared_ptr<TestClass> ptr1 = fl::make_shared<TestClass>(42);
    CHECK_EQ(ptr1.use_count(), 1);
    
    fl::shared_ptr<TestClass> ptr2(ptr1);
    CHECK_EQ(ptr1.use_count(), 2);
    CHECK_EQ(ptr2.use_count(), 2);
    CHECK_EQ(ptr1.get(), ptr2.get());
    CHECK_EQ(ptr2->getValue(), 42);
}

TEST_CASE("fl::shared_ptr move construction") {
    //fl::shared_ptr<TestClass> ptr1(new TestClass(42));
    fl::shared_ptr<TestClass> ptr1 = fl::make_shared<TestClass>(42);
    TestClass* raw_ptr = ptr1.get();
    CHECK_EQ(ptr1.use_count(), 1);
    
    fl::shared_ptr<TestClass> ptr2(fl::move(ptr1));
    CHECK_EQ(ptr1.get(), nullptr);
    CHECK_EQ(ptr1.use_count(), 0);
    CHECK_EQ(ptr2.get(), raw_ptr);
    CHECK_EQ(ptr2.use_count(), 1);
    CHECK_EQ(ptr2->getValue(), 42);
}

TEST_CASE("fl::shared_ptr assignment operator") {
    //fl::shared_ptr<TestClass> ptr1(new TestClass(42));
    fl::shared_ptr<TestClass> ptr1 = fl::make_shared<TestClass>(42);
    //fl::shared_ptr<TestClass> ptr2(new TestClass(100));
    fl::shared_ptr<TestClass> ptr2 = fl::make_shared<TestClass>(100);
    
    CHECK_EQ(ptr1.use_count(), 1);
    CHECK_EQ(ptr2.use_count(), 1);
    CHECK_NE(ptr1.get(), ptr2.get());
    
    ptr2 = ptr1;
    CHECK_EQ(ptr1.use_count(), 2);
    CHECK_EQ(ptr2.use_count(), 2);
    CHECK_EQ(ptr1.get(), ptr2.get());
    CHECK_EQ(ptr2->getValue(), 42);
}

TEST_CASE("fl::shared_ptr move assignment") {
    //fl::shared_ptr<TestClass> ptr1(new TestClass(42));
    fl::shared_ptr<TestClass> ptr1 = fl::make_shared<TestClass>(42);
    //fl::shared_ptr<TestClass> ptr2(new TestClass(100));
    fl::shared_ptr<TestClass> ptr2 = fl::make_shared<TestClass>(100);
    TestClass* raw_ptr = ptr1.get();
    
    ptr2 = fl::move(ptr1);
    CHECK_EQ(ptr1.get(), nullptr);
    CHECK_EQ(ptr1.use_count(), 0);
    CHECK_EQ(ptr2.get(), raw_ptr);
    CHECK_EQ(ptr2.use_count(), 1);
    CHECK_EQ(ptr2->getValue(), 42);
}

TEST_CASE("fl::shared_ptr reset functionality") {
    bool destructor_called = false;
    //fl::shared_ptr<TestClass> ptr(new TestClass(42, &destructor_called));
    fl::shared_ptr<TestClass> ptr = fl::make_shared<TestClass>(42, &destructor_called);
    CHECK(ptr);
    CHECK_EQ(ptr.use_count(), 1);
    
    ptr.reset();
    CHECK(!ptr);
    CHECK_EQ(ptr.get(), nullptr);
    CHECK_EQ(ptr.use_count(), 0);
    CHECK(destructor_called);
}

TEST_CASE("fl::shared_ptr reset with new pointer") {
    //fl::shared_ptr<TestClass> ptr(new TestClass(42));
    fl::shared_ptr<TestClass> ptr = fl::make_shared<TestClass>(42);
    CHECK_EQ(ptr->getValue(), 42);
    
    //ptr.reset(new TestClass(100));
    ptr.reset(fl::make_shared<TestClass>(100));
    CHECK_EQ(ptr->getValue(), 100);
    CHECK_EQ(ptr.use_count(), 1);
}

TEST_CASE("fl::shared_ptr reset with custom deleter") {
    CustomDeleter deleter;
    //fl::shared_ptr<TestClass> ptr(new TestClass(42));
    fl::shared_ptr<TestClass> ptr = fl::make_shared<TestClass>(42);
    
    //ptr.reset(new TestClass(100), deleter);
    ptr.reset(fl::make_shared_with_deleter<TestClass>(deleter, 100));
    CHECK_EQ(ptr->getValue(), 100);
    CHECK_EQ(ptr.use_count(), 1);
    
    ptr.reset();
    CHECK(deleter.called());
}

TEST_CASE("fl::shared_ptr swap functionality") {
    //fl::shared_ptr<TestClass> ptr1(new TestClass(42));
    fl::shared_ptr<TestClass> ptr1 = fl::make_shared<TestClass>(42);
    //fl::shared_ptr<TestClass> ptr2(new TestClass(100));
    fl::shared_ptr<TestClass> ptr2 = fl::make_shared<TestClass>(100);
    TestClass* raw_ptr1 = ptr1.get();
    TestClass* raw_ptr2 = ptr2.get();
    
    ptr1.swap(ptr2);
    CHECK_EQ(ptr1.get(), raw_ptr2);
    CHECK_EQ(ptr2.get(), raw_ptr1);
    CHECK_EQ(ptr1->getValue(), 100);
    CHECK_EQ(ptr2->getValue(), 42);
}

TEST_CASE("fl::shared_ptr operator* and operator->") {
    //fl::shared_ptr<TestClass> ptr(new TestClass(42));
    fl::shared_ptr<TestClass> ptr = fl::make_shared<TestClass>(42);
    
    CHECK_EQ((*ptr).getValue(), 42);
    CHECK_EQ(ptr->getValue(), 42);
    
    ptr->setValue(100);
    CHECK_EQ(ptr->getValue(), 100);
}

TEST_CASE("fl::shared_ptr bool conversion") {
    fl::shared_ptr<TestClass> ptr1;
    //fl::shared_ptr<TestClass> ptr2(new TestClass(42));
    fl::shared_ptr<TestClass> ptr2 = fl::make_shared<TestClass>(42);
    
    CHECK(!ptr1);
    CHECK(ptr2);
    
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
    CHECK_EQ(ptr1 == ptr2, true);
    CHECK_EQ(ptr1 == ptr3, false);
    CHECK_EQ(null_ptr == nullptr, true);
    CHECK_EQ(nullptr == null_ptr, true);
    CHECK_EQ(ptr1 == nullptr, false);
    
    // Inequality - using CHECK_EQ for better error messages
    CHECK_EQ(ptr1 != ptr2, false);
    CHECK_EQ(ptr1 != ptr3, true);
    CHECK_EQ(null_ptr != nullptr, false);
    CHECK_EQ(ptr1 != nullptr, true);
}

TEST_CASE("fl::shared_ptr polymorphism") {
    // Test with derived class
    //fl::shared_ptr<DerivedTestClass> derived_ptr(new DerivedTestClass(42, 100));
    fl::shared_ptr<DerivedTestClass> derived_ptr = fl::make_shared<DerivedTestClass>(42, 100);
    fl::shared_ptr<TestClass> base_ptr(derived_ptr);
    
    CHECK_EQ(base_ptr.use_count(), 2);
    CHECK_EQ(derived_ptr.use_count(), 2);
    CHECK_EQ(base_ptr->getValue(), 42);
    
    // Both should point to the same object
    CHECK_EQ(base_ptr.get(), derived_ptr.get());
}

TEST_CASE("fl::make_shared basic functionality") {
    // Test default constructor
    auto ptr1 = fl::make_shared<TestClass>();
    CHECK(ptr1);
    CHECK_EQ(ptr1->getValue(), 0);
    CHECK_EQ(ptr1.use_count(), 1);
    
    // Test single argument constructor
    auto ptr2 = fl::make_shared<TestClass>(42);
    CHECK(ptr2);
    CHECK_EQ(ptr2->getValue(), 42);
    CHECK_EQ(ptr2.use_count(), 1);
    
    // Test multiple argument constructor
    auto ptr3 = fl::make_shared<TestClass>(10, 20);
    CHECK(ptr3);
    CHECK_EQ(ptr3->getValue(), 30);
    CHECK_EQ(ptr3.use_count(), 1);
}

TEST_CASE("fl::make_shared memory optimization") {
    // make_shared should use inlined storage for better performance
    auto ptr = fl::make_shared<TestClass>(42);
    CHECK(ptr);
    CHECK_EQ(ptr->getValue(), 42);
    CHECK_EQ(ptr.use_count(), 1);
    
    // Test copy construction with make_shared
    auto ptr2 = ptr;
    CHECK_EQ(ptr.use_count(), 2);
    CHECK_EQ(ptr2.use_count(), 2);
    CHECK_EQ(ptr.get(), ptr2.get());
}

TEST_CASE("fl::shared_ptr reference counting stress test") {
    const int NUM_COPIES = 10;
    //fl::shared_ptr<TestClass> original(new TestClass(42));
    fl::shared_ptr<TestClass> original = fl::make_shared<TestClass>(42);
    CHECK_EQ(original.use_count(), 1);
    
    // Create multiple copies
    fl::vector<fl::shared_ptr<TestClass>> copies;
    for (int i = 0; i < NUM_COPIES; ++i) {
        copies.push_back(original);
        CHECK_EQ(original.use_count(), i + 2);
    }
    
    // Verify all copies point to the same object
    for (const auto& copy : copies) {
        CHECK_EQ(copy.get(), original.get());
        CHECK_EQ(copy->getValue(), 42);
        CHECK_EQ(copy.use_count(), NUM_COPIES + 1);
    }
    
    // Clear copies one by one
    for (int i = 0; i < NUM_COPIES; ++i) {
        copies.pop_back();
        CHECK_EQ(original.use_count(), NUM_COPIES - i);
    }
    
    CHECK_EQ(original.use_count(), 1);
    CHECK(original.unique());
}

TEST_CASE("fl::shared_ptr destruction order") {
    bool destructor_called = false;
    {
        //fl::shared_ptr<TestClass> ptr1(new TestClass(42, &destructor_called));
        fl::shared_ptr<TestClass> ptr1 = fl::make_shared<TestClass>(42, &destructor_called);
        {
            fl::shared_ptr<TestClass> ptr2 = ptr1;
            CHECK_EQ(ptr1.use_count(), 2);
            CHECK(!destructor_called);
        }
        CHECK_EQ(ptr1.use_count(), 1);
        CHECK(!destructor_called);
    }
    CHECK(destructor_called);
}

TEST_CASE("fl::shared_ptr self-assignment safety") {
    //fl::shared_ptr<TestClass> ptr(new TestClass(42));
    fl::shared_ptr<TestClass> ptr = fl::make_shared<TestClass>(42);
    CHECK_EQ(ptr.use_count(), 1);
    
    // Self-assignment should not change anything
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_SELF_ASSIGN_OVERLOADED
    ptr = ptr;  // Test self-assignment
    FL_DISABLE_WARNING_POP
    CHECK_EQ(ptr.use_count(), 1);
    CHECK_EQ(ptr->getValue(), 42);
    
    // Self-move assignment should not break anything
    ptr = fl::move(ptr);
    CHECK_EQ(ptr.use_count(), 1);
    CHECK_EQ(ptr->getValue(), 42);
}

// Node class for testing circular references and self-assignment scenarios
class SharedNode {
public:
    SharedNode(int value) : value_(value), destructor_called_(nullptr) {}
    SharedNode(int value, bool* destructor_flag) : value_(value), destructor_called_(destructor_flag) {}
    
    ~SharedNode() {
        if (destructor_called_) {
            *destructor_called_ = true;
        }
    }
    
    int getValue() const { return value_; }
    void setValue(int value) { value_ = value; }
    
    void setNext(fl::shared_ptr<SharedNode> next) { next_ = next; }
    fl::shared_ptr<SharedNode> getNext() const { return next_; }
    
private:
    int value_;
    bool* destructor_called_;
    fl::shared_ptr<SharedNode> next_;
};

TEST_CASE("fl::shared_ptr self-assignment safety - a = b scenario") {
    bool nodeA_destroyed = false;
    bool nodeB_destroyed = false;
    
    auto nodeA = fl::make_shared<SharedNode>(1, &nodeA_destroyed);
    auto nodeB = fl::make_shared<SharedNode>(2, &nodeB_destroyed);
    
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
