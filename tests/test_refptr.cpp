
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "ref.h"

#include "namespace.h"
FASTLED_USING_NAMESPACE

class MyClass;
typedef Ref<MyClass> MyClassRef;
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

TEST_CASE("Ref basic functionality") {
    Ref<MyClass> ptr = MyClassRef::New();

    SUBCASE("Ref is not null after construction") {
        CHECK(ptr.get() != nullptr);
    }

    SUBCASE("Ref increments reference count") {
        CHECK(ptr->ref_count() == 1);
    }

    SUBCASE("Ref can be reassigned") {
        Ref<MyClass> ptr2 = ptr;
        CHECK(ptr2.get() == ptr.get());
        CHECK(ptr->ref_count() == 2);
    }
}

TEST_CASE("Ref move semantics") {

    SUBCASE("Move constructor works correctly") {
        Ref<MyClass> ptr1 = MyClassRef::New();
        MyClass *rawRef = ptr1.get();
        Ref<MyClass> ptr2(std::move(ptr1));
        CHECK(ptr2.get() == rawRef);
        CHECK(ptr1.get() == nullptr);
        CHECK(ptr2->ref_count() == 1);
    }

    SUBCASE("Move assignment works correctly") {
        Ref<MyClass> ptr1 = MyClassRef::New();
        MyClass *rawRef = ptr1.get();
        Ref<MyClass> ptr2;
        ptr2 = std::move(ptr1);
        CHECK(ptr2.get() == rawRef);
        CHECK(ptr1.get() == nullptr);
        CHECK(ptr2->ref_count() == 1);
    }
}

TEST_CASE("Ref reference counting") {

    SUBCASE("Reference count increases when copied") {
        Ref<MyClass> ptr1 = MyClassRef::New();
        Ref<MyClass> ptr2 = ptr1;
        CHECK(ptr1->ref_count() == 2);
        CHECK(ptr2->ref_count() == 2);
    }

    SUBCASE("Reference count decreases when Ref goes out of scope") {
        Ref<MyClass> ptr1 = MyClassRef::New();
        {
            Ref<MyClass> ptr2 = ptr1;
            CHECK(ptr1->ref_count() == 2);
        }
        CHECK(ptr1->ref_count() == 1);
    }
}



TEST_CASE("Ref reset functionality") {

    SUBCASE("Reset to nullptr") {
        Ref<MyClass> ptr = Ref<MyClass>::New();
        CHECK_EQ(1, ptr->ref_count());
        ptr->ref();
        CHECK_EQ(2, ptr->ref_count());
        MyClass *originalRef = ptr.get();
        ptr.reset();
        CHECK_EQ(1, originalRef->ref_count());
        CHECK(ptr.get() == nullptr);
        CHECK(originalRef->ref_count() == 1);
        originalRef->unref();
    }

}


TEST_CASE("Ref from static memory") {
    MyClass staticObject;
    {
        MyClassRef ptr = MyClassRef::NoTracking(staticObject);
    }
    CHECK_EQ(staticObject.ref_count(), 0);
    CHECK_NE(staticObject.destructor_signal, 0xdeadbeef);
}

TEST_CASE("WeakRef functionality") {
    WeakRef<MyClass> weakRef;
    MyClassRef strongRef = MyClassRef::New();
    weakRef = strongRef;

    REQUIRE_EQ(strongRef->ref_count(), 1);
    bool expired = weakRef.expired();
    REQUIRE_FALSE(expired);
    REQUIRE(weakRef != nullptr);
    
    SUBCASE("WeakRef can be locked to get a strong reference") {
        MyClassRef lockedRef = weakRef.lock();
        CHECK(lockedRef.get() == strongRef.get());
        CHECK_EQ(strongRef->ref_count(), 2);
    }
    weakRef.reset();
    expired = weakRef.expired();
    CHECK(expired);
}

TEST_CASE("WeakRef functionality early expiration") {
    WeakRef<MyClass> weakRef;
    MyClassRef strongRef = MyClassRef::New();
    weakRef = strongRef;

    REQUIRE_EQ(strongRef->ref_count(), 1);
    bool expired = weakRef.expired();
    REQUIRE_FALSE(expired);
    REQUIRE(weakRef != nullptr);
    
    SUBCASE("WeakRef can be locked to get a strong reference") {
        MyClassRef lockedRef = weakRef.lock();
        CHECK(lockedRef.get() == strongRef.get());
        CHECK_EQ(strongRef->ref_count(), 2);
    }
    strongRef.reset();
    expired = weakRef.expired();
    CHECK(expired);
}

TEST_CASE("WeakRef additional functionality") {
    SUBCASE("WeakRef default constructor") {
        WeakRef<MyClass> weakRef;
        CHECK(weakRef.expired());
        CHECK(weakRef.lock() == nullptr);
    }

    SUBCASE("WeakRef assignment and reset") {
        MyClassRef strongRef1 = MyClassRef::New();
        MyClassRef strongRef2 = MyClassRef::New();
        WeakRef<MyClass> weakRef = strongRef1;

        CHECK_FALSE(weakRef.expired());
        CHECK(weakRef.lock().get() == strongRef1.get());

        weakRef = strongRef2;
        CHECK_FALSE(weakRef.expired());
        CHECK(weakRef.lock().get() == strongRef2.get());

        weakRef.reset();
        CHECK(weakRef.expired());
        CHECK(weakRef.lock() == nullptr);
    }

    SUBCASE("WeakRef multiple instances") {
        MyClassRef strongRef = MyClassRef::New();
        WeakRef<MyClass> weakRef1 = strongRef;
        WeakRef<MyClass> weakRef2 = strongRef;

        CHECK_FALSE(weakRef1.expired());
        CHECK_FALSE(weakRef2.expired());
        CHECK(weakRef1.lock().get() == weakRef2.lock().get());

        strongRef.reset();
        CHECK(weakRef1.expired());
        CHECK(weakRef2.expired());
    }

    SUBCASE("WeakRef with temporary strong pointer") {
        WeakRef<MyClass> weakRef;
        {
            MyClassRef tempStrongRef = MyClassRef::New();
            weakRef = tempStrongRef;
            CHECK_FALSE(weakRef.expired());
        }
        CHECK(weakRef.expired());
    }

    SUBCASE("WeakRef lock performance") {
        MyClassRef strongRef = MyClassRef::New();
        WeakRef<MyClass> weakRef = strongRef;

        for (int i = 0; i < 1000; ++i) {
            MyClassRef lockedRef = weakRef.lock();
            CHECK(lockedRef.get() == strongRef.get());
        }
        CHECK_EQ(strongRef->ref_count(), 1);
    }

    SUBCASE("WeakRef with inheritance") {
        class DerivedClass : public MyClass {};
        Ref<DerivedClass> derivedRef = Ref<DerivedClass>::New();
        WeakRef<MyClass> weakBaseRef = derivedRef;
        WeakRef<DerivedClass> weakDerivedRef = derivedRef;

        CHECK_FALSE(weakBaseRef.expired());
        CHECK_FALSE(weakDerivedRef.expired());
        CHECK(weakBaseRef.lock().get() == weakDerivedRef.lock().get());
    }
}


