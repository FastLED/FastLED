
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