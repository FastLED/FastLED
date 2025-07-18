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

TEST_CASE("fl::make_shared basic functionality") {
    // Test default constructor
    auto ptr1 = fl::make_shared<TestClass>();
    CHECK(ptr1 != nullptr);
    CHECK_EQ(ptr1->getValue(), 0);
    
    // Test single argument constructor
    auto ptr2 = fl::make_shared<TestClass>(42);
    CHECK(ptr2 != nullptr);
    CHECK_EQ(ptr2->getValue(), 42);
    
    // Test multiple argument constructor
    auto ptr3 = fl::make_shared<TestClass>(10, 20);
    CHECK(ptr3 != nullptr);
    CHECK_EQ(ptr3->getValue(), 30);
}

TEST_CASE("fl::make_shared equivalence with NewPtr") {
    // Test that make_shared behaves the same as NewPtr
    auto ptr1 = fl::make_shared<TestClass>(100);
    auto ptr2 = fl::NewPtr<TestClass>(100);
    
    CHECK(ptr1 != nullptr);
    CHECK(ptr2 != nullptr);
    CHECK_EQ(ptr1->getValue(), ptr2->getValue());
    CHECK_EQ(ptr1->getValue(), 100);
}

TEST_CASE("fl::make_shared reference counting") {
    fl::Ptr<TestClass> ptr1;
    fl::Ptr<TestClass> ptr2;
    
    {
        auto ptr = fl::make_shared<TestClass>(50);
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

TEST_CASE("fl::make_shared perfect forwarding") {
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
    auto ptr = fl::make_shared<ForwardingTest>(42, true);
    CHECK_EQ(ptr->getValue(), 42);
    CHECK(ptr->isCopy());
}

TEST_CASE("fl::shared_ptr alias functionality") {
    // Test that fl::shared_ptr<T> works as an alias for fl::Ptr<T>
    fl::shared_ptr<TestClass> ptr1 = fl::make_shared<TestClass>(42);
    CHECK(ptr1 != nullptr);
    CHECK_EQ(ptr1->getValue(), 42);
    
    // Test assignment between shared_ptr and Ptr
    fl::Ptr<TestClass> ptr2 = ptr1;
    CHECK_EQ(ptr1.get(), ptr2.get());
    CHECK_EQ(ptr1->ref_count(), 2);
    CHECK_EQ(ptr2->ref_count(), 2);
    
    // Test that they are the same type
    fl::shared_ptr<TestClass> ptr3;
    ptr3 = ptr2;  // Should work seamlessly
    CHECK_EQ(ptr3->getValue(), 42);
    CHECK_EQ(ptr3->ref_count(), 3);
} 
