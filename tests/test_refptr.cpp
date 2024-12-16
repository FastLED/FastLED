
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "fl/ptr.h"

#include "fl/namespace.h"

using namespace fl;

class MyClass;
typedef Ptr<MyClass> MyClassPtr;
class MyClass : public fl::Referent {
  public:
    MyClass() {}
    ~MyClass() {
        destructor_signal = 0xdeadbeef;
    }
    virtual void ref() override { Referent::ref(); }
    virtual void unref() override { Referent::unref(); }
    virtual void destroy() override { Referent::destroy(); }
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
        MyClassPtr strongRef1 = MyClassPtr::New();
        MyClassPtr strongRef2 = MyClassPtr::New();
        WeakPtr<MyClass> weakPtr = strongRef1;

        CHECK_FALSE(weakPtr.expired());
        CHECK(weakPtr.lock().get() == strongRef1.get());

        weakPtr = strongRef2;
        CHECK_FALSE(weakPtr.expired());
        CHECK(weakPtr.lock().get() == strongRef2.get());

        weakPtr.reset();
        CHECK(weakPtr.expired());
        CHECK(weakPtr.lock() == nullptr);
    }

    SUBCASE("WeakPtr multiple instances") {
        MyClassPtr strongPtr = MyClassPtr::New();
        WeakPtr<MyClass> weakRef1 = strongPtr;
        WeakPtr<MyClass> weakRef2 = strongPtr;

        CHECK_FALSE(weakRef1.expired());
        CHECK_FALSE(weakRef2.expired());
        CHECK(weakRef1.lock().get() == weakRef2.lock().get());

        strongPtr.reset();
        CHECK(weakRef1.expired());
        CHECK(weakRef2.expired());
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


