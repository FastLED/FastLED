#include "test.h"
#include "fl/shared_ptr.h"
#include "fl/weak_ptr.h"
#include "fl/memory.h"
#include "fl/compiler_control.h"

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
        fl::shared_ptr<TestClass> shared(new TestClass(42, &destructor_called));
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
    FL_DISABLE_WARNING(self-assign-overloaded)
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
