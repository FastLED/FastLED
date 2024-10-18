
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "ptr.h"

class MyClass;
typedef Ptr<MyClass> MyClassPtr;
class MyClass : public Referent {
  public:
    MyClass() {}
    ~MyClass() {
        destructor_signal = 0xdeadbeef;
    }
    virtual void ref() { Referent::ref(); }
    virtual void unref() { Referent::unref(); }
    virtual void destroy() { Referent::destroy(); }
    uint32_t destructor_signal = 0;
};

TEST_CASE("Ptr basic functionality") {
    Ptr<MyClass> ptr = MyClassPtr::New();

    SUBCASE("Ptr is not null after construction") {
        CHECK(ptr.get() != nullptr);
    }

    SUBCASE("Ptr increments reference count") {
        CHECK(ptr->ref_count() == 1);
    }

    SUBCASE("Ptr can be reassigned") {
        Ptr<MyClass> ptr2 = ptr;
        CHECK(ptr2.get() == ptr.get());
        CHECK(ptr->ref_count() == 2);
    }
}

TEST_CASE("Ptr move semantics") {

    SUBCASE("Move constructor works correctly") {
        Ptr<MyClass> ptr1 = MyClassPtr::New();
        MyClass *rawPtr = ptr1.get();
        Ptr<MyClass> ptr2(std::move(ptr1));
        CHECK(ptr2.get() == rawPtr);
        CHECK(ptr1.get() == nullptr);
        CHECK(ptr2->ref_count() == 1);
    }

    SUBCASE("Move assignment works correctly") {
        Ptr<MyClass> ptr1 = MyClassPtr::New();
        MyClass *rawPtr = ptr1.get();
        Ptr<MyClass> ptr2;
        ptr2 = std::move(ptr1);
        CHECK(ptr2.get() == rawPtr);
        CHECK(ptr1.get() == nullptr);
        CHECK(ptr2->ref_count() == 1);
    }
}

TEST_CASE("Ptr reference counting") {

    SUBCASE("Reference count increases when copied") {
        Ptr<MyClass> ptr1 = MyClassPtr::New();
        Ptr<MyClass> ptr2 = ptr1;
        CHECK(ptr1->ref_count() == 2);
        CHECK(ptr2->ref_count() == 2);
    }

    SUBCASE("Reference count decreases when Ptr goes out of scope") {
        Ptr<MyClass> ptr1 = MyClassPtr::New();
        {
            Ptr<MyClass> ptr2 = ptr1;
            CHECK(ptr1->ref_count() == 2);
        }
        CHECK(ptr1->ref_count() == 1);
    }
}



TEST_CASE("Ptr reset functionality") {

    SUBCASE("Reset to nullptr") {
        Ptr<MyClass> ptr = Ptr<MyClass>::New();
        CHECK_EQ(1, ptr->ref_count());
        ptr->ref();
        CHECK_EQ(2, ptr->ref_count());
        MyClass *originalPtr = ptr.get();
        ptr.reset();
        CHECK_EQ(1, originalPtr->ref_count());
        CHECK(ptr.get() == nullptr);
        CHECK(originalPtr->ref_count() == 1);
        originalPtr->unref();
    }

}


TEST_CASE("Ptr from static memory") {
    MyClass staticObject;
    {
        MyClassPtr ptr = MyClassPtr::NoTracking(staticObject);
    }
    CHECK_EQ(staticObject.ref_count(), 0);
    CHECK_NE(staticObject.destructor_signal, 0xdeadbeef);
}

TEST_CASE("WeakPtr functionality") {
    WeakPtr<MyClass> weakPtr;
    MyClassPtr strongPtr = MyClassPtr::New();
    weakPtr = strongPtr;

    REQUIRE_EQ(strongPtr->ref_count(), 1);
    bool expired = weakPtr.expired();
    REQUIRE_FALSE(expired);
    REQUIRE(weakPtr != nullptr);
    
    SUBCASE("WeakPtr can be locked to get a strong reference") {
        MyClassPtr lockedPtr = weakPtr.lock();
        CHECK(lockedPtr.get() == strongPtr.get());
        CHECK_EQ(strongPtr->ref_count(), 2);
    }
    weakPtr.reset();
    expired = weakPtr.expired();
    CHECK(expired);
}

TEST_CASE("WeakPtr functionality early expiration") {
    WeakPtr<MyClass> weakPtr;
    MyClassPtr strongPtr = MyClassPtr::New();
    weakPtr = strongPtr;

    REQUIRE_EQ(strongPtr->ref_count(), 1);
    bool expired = weakPtr.expired();
    REQUIRE_FALSE(expired);
    REQUIRE(weakPtr != nullptr);
    
    SUBCASE("WeakPtr can be locked to get a strong reference") {
        MyClassPtr lockedPtr = weakPtr.lock();
        CHECK(lockedPtr.get() == strongPtr.get());
        CHECK_EQ(strongPtr->ref_count(), 2);
    }
    strongPtr.reset();
    expired = weakPtr.expired();
    CHECK(expired);
}

TEST_CASE("WeakPtr additional functionality") {
    SUBCASE("WeakPtr default constructor") {
        WeakPtr<MyClass> weakPtr;
        CHECK(weakPtr.expired());
        CHECK(weakPtr.lock() == nullptr);
    }

    SUBCASE("WeakPtr assignment and reset") {
        MyClassPtr strongPtr1 = MyClassPtr::New();
        MyClassPtr strongPtr2 = MyClassPtr::New();
        WeakPtr<MyClass> weakPtr = strongPtr1;

        CHECK_FALSE(weakPtr.expired());
        CHECK(weakPtr.lock().get() == strongPtr1.get());

        weakPtr = strongPtr2;
        CHECK_FALSE(weakPtr.expired());
        CHECK(weakPtr.lock().get() == strongPtr2.get());

        weakPtr.reset();
        CHECK(weakPtr.expired());
        CHECK(weakPtr.lock() == nullptr);
    }

    SUBCASE("WeakPtr multiple instances") {
        MyClassPtr strongPtr = MyClassPtr::New();
        WeakPtr<MyClass> weakPtr1 = strongPtr;
        WeakPtr<MyClass> weakPtr2 = strongPtr;

        CHECK_FALSE(weakPtr1.expired());
        CHECK_FALSE(weakPtr2.expired());
        CHECK(weakPtr1.lock().get() == weakPtr2.lock().get());

        strongPtr.reset();
        CHECK(weakPtr1.expired());
        CHECK(weakPtr2.expired());
    }

    SUBCASE("WeakPtr with temporary strong pointer") {
        WeakPtr<MyClass> weakPtr;
        {
            MyClassPtr tempStrongPtr = MyClassPtr::New();
            weakPtr = tempStrongPtr;
            CHECK_FALSE(weakPtr.expired());
        }
        CHECK(weakPtr.expired());
    }

    SUBCASE("WeakPtr lock performance") {
        MyClassPtr strongPtr = MyClassPtr::New();
        WeakPtr<MyClass> weakPtr = strongPtr;

        for (int i = 0; i < 1000; ++i) {
            MyClassPtr lockedPtr = weakPtr.lock();
            CHECK(lockedPtr.get() == strongPtr.get());
        }
        CHECK_EQ(strongPtr->ref_count(), 1);
    }

    SUBCASE("WeakPtr with inheritance") {
        class DerivedClass : public MyClass {};
        Ptr<DerivedClass> derivedPtr = Ptr<DerivedClass>::New();
        WeakPtr<MyClass> weakBasePtr = derivedPtr;
        WeakPtr<DerivedClass> weakDerivedPtr = derivedPtr;

        CHECK_FALSE(weakBasePtr.expired());
        CHECK_FALSE(weakDerivedPtr.expired());
        CHECK(weakBasePtr.lock().get() == weakDerivedPtr.lock().get());
    }
}


