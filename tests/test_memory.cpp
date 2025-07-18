#include "test.h"
#include "fl/memory.h"
#include "fl/referent.h"

// Test class that inherits from fl::Referent
class TestClass : public fl::Referent {
public:
    TestClass() : value_(0) {}
    TestClass(int value) : value_(value) {}
    TestClass(int a, int b) : value_(a + b) {}
    
    int getValue() const { return value_; }
    
private:
    int value_;
};

FASTLED_SMART_PTR(TestClass);

TEST_CASE("fl::make_intrusive basic functionality") {
    // Test default constructor
    auto ptr1 = fl::make_intrusive<TestClass>();
    CHECK(ptr1 != nullptr);
    CHECK_EQ(ptr1->getValue(), 0);
    
    // Test single argument constructor
    auto ptr2 = fl::make_intrusive<TestClass>(42);
    CHECK(ptr2 != nullptr);
    CHECK_EQ(ptr2->getValue(), 42);
    
    // Test multiple argument constructor
    auto ptr3 = fl::make_intrusive<TestClass>(10, 20);
    CHECK(ptr3 != nullptr);
    CHECK_EQ(ptr3->getValue(), 30);
}

TEST_CASE("fl::make_intrusive equivalence with NewPtr") {
    // Test that make_intrusive behaves the same as NewPtr
    auto ptr1 = fl::make_intrusive<TestClass>(100);
    auto ptr2 = fl::NewPtr<TestClass>(100);
    
    CHECK(ptr1 != nullptr);
    CHECK(ptr2 != nullptr);
    CHECK_EQ(ptr1->getValue(), ptr2->getValue());
    CHECK_EQ(ptr1->getValue(), 100);
}

TEST_CASE("fl::make_intrusive reference counting") {
    fl::Ptr<TestClass> ptr1;
    fl::Ptr<TestClass> ptr2;
    
    {
        auto ptr = fl::make_intrusive<TestClass>(50);
        CHECK_EQ(ptr->ref_count(), 1);
        
        ptr1 = ptr;
        CHECK_EQ(ptr->ref_count(), 2);
        CHECK_EQ(ptr1->ref_count(), 2);
        
        ptr2 = ptr1;
        CHECK_EQ(ptr->ref_count(), 3);
        CHECK_EQ(ptr1->ref_count(), 3);
        CHECK_EQ(ptr2->ref_count(), 3);
    }
    
    // Original ptr goes out of scope
    CHECK_EQ(ptr1->ref_count(), 2);
    CHECK_EQ(ptr2->ref_count(), 2);
    
    ptr1.reset();
    CHECK_EQ(ptr2->ref_count(), 1);
    CHECK_EQ(ptr2->getValue(), 50);
}

TEST_CASE("fl::make_intrusive perfect forwarding") {
    // Test that arguments are properly forwarded
    class ForwardingTest : public fl::Referent {
    public:
        ForwardingTest(int value, bool is_copy) : value_(value), is_copy_(is_copy) {}
        
        int getValue() const { return value_; }
        bool isCopy() const { return is_copy_; }
        
    private:
        int value_;
        bool is_copy_;
    };
    
    // Test argument forwarding with multiple parameters
    auto ptr = fl::make_intrusive<ForwardingTest>(42, true);
    CHECK_EQ(ptr->getValue(), 42);
    CHECK(ptr->isCopy());
}

TEST_CASE("fl::intrusive_ptr alias functionality") {
    // Test that fl::intrusive_ptr<T> works as an alias for fl::Ptr<T>
    fl::intrusive_ptr<TestClass> ptr1 = fl::make_intrusive<TestClass>(42);
    CHECK(ptr1 != nullptr);
    CHECK_EQ(ptr1->getValue(), 42);
    
    // Test assignment between intrusive_ptr and Ptr
    fl::Ptr<TestClass> ptr2 = ptr1;
    CHECK_EQ(ptr1.get(), ptr2.get());
    CHECK_EQ(ptr1->ref_count(), 2);
    CHECK_EQ(ptr2->ref_count(), 2);
    
    // Test that they are the same type
    fl::intrusive_ptr<TestClass> ptr3;
    ptr3 = ptr2;  // Should work seamlessly
    CHECK_EQ(ptr3->getValue(), 42);
    CHECK_EQ(ptr3->ref_count(), 3);
}

// Node class for testing circular references and self-assignment scenarios
class IntrusiveNode : public fl::Referent {
public:
    IntrusiveNode(int value) : value_(value) {}
    
    int getValue() const { return value_; }
    void setValue(int value) { value_ = value; }
    
    void setNext(fl::intrusive_ptr<IntrusiveNode> next) { next_ = next; }
    fl::intrusive_ptr<IntrusiveNode> getNext() const { return next_; }
    
private:
    int value_;
    fl::intrusive_ptr<IntrusiveNode> next_;
};

FASTLED_SMART_PTR(IntrusiveNode);

TEST_CASE("fl::intrusive_ptr self-assignment safety - a = b scenario") {
    auto nodeA = fl::make_intrusive<IntrusiveNode>(1);
    auto nodeB = fl::make_intrusive<IntrusiveNode>(2);
    
    // Test the scenario: a -> b, and we have a, and a = b
    nodeA->setNext(nodeB);
    
    // Verify initial state
    CHECK_EQ(nodeA->getValue(), 1);
    CHECK_EQ(nodeB->getValue(), 2);
    CHECK_EQ(nodeA->getNext().get(), nodeB.get());
    CHECK_EQ(nodeA->ref_count(), 1); // Only nodeA variable
    CHECK_EQ(nodeB->ref_count(), 2); // nodeB variable + nodeA->next_
    
    // Get a reference to A before the dangerous assignment
    auto aRef = nodeA;
    CHECK_EQ(aRef.get(), nodeA.get());
    CHECK_EQ(nodeA->ref_count(), 2); // nodeA + aRef
    CHECK_EQ(nodeB->ref_count(), 2); // nodeB + nodeA->next_
    
    // Now do the dangerous assignment: a = b (while a is referenced through aRef)
    // This could cause issues if a gets destroyed while setting itself to b
    nodeA = nodeB; // a = b (dangerous assignment)
    
    // Verify no segfault occurred and state is consistent
    CHECK_EQ(nodeA.get(), nodeB.get()); // nodeA should now point to nodeB
    CHECK_EQ(nodeA->getValue(), 2); // Should have nodeB's value
    CHECK_EQ(nodeB->getValue(), 2); // nodeB unchanged
    
    // aRef should still be valid (original nodeA should still exist)
    CHECK(aRef);
    CHECK_EQ(aRef->getValue(), 1); // Original nodeA value
    CHECK_EQ(aRef->ref_count(), 1); // Only aRef now points to original nodeA
    
    // nodeB should now have increased reference count
    CHECK_EQ(nodeB->ref_count(), 3); // nodeB + nodeA + nodeA->next_ (which points to nodeB)
    
    // Clean up - clear the circular reference in the original node
    aRef->setNext(fl::intrusive_ptr<IntrusiveNode>());
    CHECK_EQ(nodeB->ref_count(), 2); // nodeB + nodeA
}
