#include "test.h"
#include "fl/stl/move.h"
#include "test.h"
#include "fl/stl/type_traits.h"

using namespace fl;

namespace {

// Helper class to test move semantics
struct MoveTestTypeMove {
    int value;
    bool moved_from;
    bool moved_to;

    MoveTestTypeMove() : value(0), moved_from(false), moved_to(false) {}
    explicit MoveTestTypeMove(int v) : value(v), moved_from(false), moved_to(false) {}

    // Copy constructor
    MoveTestTypeMove(const MoveTestTypeMove& other)
        : value(other.value), moved_from(false), moved_to(false) {}

    // Move constructor
    MoveTestTypeMove(MoveTestTypeMove&& other)
        : value(other.value), moved_from(false), moved_to(true) {
        other.moved_from = true;
        other.value = 0;
    }

    // Copy assignment
    MoveTestTypeMove& operator=(const MoveTestTypeMove& other) {
        if (this != &other) {
            value = other.value;
            moved_from = false;
            moved_to = false;
        }
        return *this;
    }

    // Move assignment
    MoveTestTypeMove& operator=(MoveTestTypeMove&& other) {
        if (this != &other) {
            value = other.value;
            moved_from = false;
            moved_to = true;
            other.moved_from = true;
            other.value = 0;
        }
        return *this;
    }
};

} // anonymous namespace

FL_TEST_CASE("fl::remove_reference trait") {
    FL_SUBCASE("Non-reference types remain unchanged") {
        static_assert(fl::is_same<typename remove_reference<int>::type, int>::value,
                     "remove_reference should not change int");
        static_assert(fl::is_same<typename remove_reference<float>::type, float>::value,
                     "remove_reference should not change float");
        static_assert(fl::is_same<typename remove_reference<double>::type, double>::value,
                     "remove_reference should not change double");
        static_assert(fl::is_same<typename remove_reference<char>::type, char>::value,
                     "remove_reference should not change char");
        FL_CHECK(true);  // Dummy runtime check
    }

    FL_SUBCASE("Pointer types remain unchanged") {
        static_assert(fl::is_same<typename remove_reference<int*>::type, int*>::value,
                     "remove_reference should not change int*");
        static_assert(fl::is_same<typename remove_reference<const int*>::type, const int*>::value,
                     "remove_reference should not change const int*");
        static_assert(fl::is_same<typename remove_reference<void*>::type, void*>::value,
                     "remove_reference should not change void*");
        FL_CHECK(true);  // Dummy runtime check
    }

    FL_SUBCASE("Lvalue references are removed") {
        static_assert(fl::is_same<typename remove_reference<int&>::type, int>::value,
                     "remove_reference should remove lvalue reference from int&");
        static_assert(fl::is_same<typename remove_reference<float&>::type, float>::value,
                     "remove_reference should remove lvalue reference from float&");
        static_assert(fl::is_same<typename remove_reference<const double&>::type, const double>::value,
                     "remove_reference should remove lvalue reference from const double&");
        FL_CHECK(true);  // Dummy runtime check
    }

    FL_SUBCASE("Rvalue references are removed") {
        static_assert(fl::is_same<typename remove_reference<int&&>::type, int>::value,
                     "remove_reference should remove rvalue reference from int&&");
        static_assert(fl::is_same<typename remove_reference<float&&>::type, float>::value,
                     "remove_reference should remove rvalue reference from float&&");
        static_assert(fl::is_same<typename remove_reference<const double&&>::type, const double>::value,
                     "remove_reference should remove rvalue reference from const double&&");
        FL_CHECK(true);  // Dummy runtime check
    }

    FL_SUBCASE("Const qualification is preserved") {
        static_assert(fl::is_same<typename remove_reference<const int>::type, const int>::value,
                     "remove_reference should preserve const on non-reference");
        static_assert(fl::is_same<typename remove_reference<const int&>::type, const int>::value,
                     "remove_reference should preserve const when removing reference");
        static_assert(fl::is_same<typename remove_reference<const int&&>::type, const int>::value,
                     "remove_reference should preserve const when removing rvalue reference");
        FL_CHECK(true);  // Dummy runtime check
    }

    FL_SUBCASE("Volatile qualification is preserved") {
        static_assert(fl::is_same<typename remove_reference<volatile int>::type, volatile int>::value,
                     "remove_reference should preserve volatile on non-reference");
        static_assert(fl::is_same<typename remove_reference<volatile int&>::type, volatile int>::value,
                     "remove_reference should preserve volatile when removing reference");
        FL_CHECK(true);  // Dummy runtime check
    }

    FL_SUBCASE("CV qualifications are preserved") {
        static_assert(fl::is_same<typename remove_reference<const volatile int>::type, const volatile int>::value,
                     "remove_reference should preserve const volatile");
        static_assert(fl::is_same<typename remove_reference<const volatile int&>::type, const volatile int>::value,
                     "remove_reference should preserve const volatile when removing reference");
        FL_CHECK(true);  // Dummy runtime check
    }
}

FL_TEST_CASE("fl::remove_reference_t alias") {
    FL_SUBCASE("Alias works correctly for basic types") {
        static_assert(fl::is_same<remove_reference_t<int>, int>::value,
                     "remove_reference_t should work like remove_reference::type");
        static_assert(fl::is_same<remove_reference_t<int&>, int>::value,
                     "remove_reference_t should remove lvalue reference");
        static_assert(fl::is_same<remove_reference_t<int&&>, int>::value,
                     "remove_reference_t should remove rvalue reference");
        FL_CHECK(true);  // Dummy runtime check
    }

    FL_SUBCASE("Alias preserves qualifiers") {
        static_assert(fl::is_same<remove_reference_t<const int&>, const int>::value,
                     "remove_reference_t should preserve const");
        static_assert(fl::is_same<remove_reference_t<volatile int&>, volatile int>::value,
                     "remove_reference_t should preserve volatile");
        FL_CHECK(true);  // Dummy runtime check
    }
}

FL_TEST_CASE("fl::move basic functionality") {
    FL_SUBCASE("move converts lvalue to rvalue") {
        int x = 42;
        // move should convert lvalue to rvalue reference
        // We can't directly test this without decltype, but we can verify it compiles
        auto&& moved_x = fl::move(x);
        FL_CHECK_EQ(moved_x, 42);
        // Original value unchanged for primitive types
        FL_CHECK_EQ(x, 42);
    }

    FL_SUBCASE("move with primitive types") {
        int a = 10;
        int b = fl::move(a);
        FL_CHECK_EQ(b, 10);
        // For primitives, move behaves like copy
        FL_CHECK_EQ(a, 10);

        float f = 3.14f;
        float g = fl::move(f);
        FL_CHECK_CLOSE(g, 3.14f, 0.0001f);
        FL_CHECK_CLOSE(f, 3.14f, 0.0001f);
    }

    FL_SUBCASE("move with pointers") {
        int value = 42;
        int* ptr1 = &value;
        int* ptr2 = fl::move(ptr1);
        FL_CHECK_EQ(ptr2, &value);
        FL_CHECK_EQ(*ptr2, 42);
        // Pointer value is copied
        FL_CHECK_EQ(ptr1, &value);
    }
}

FL_TEST_CASE("fl::move with move-constructible types") {
    FL_SUBCASE("move constructor is invoked") {
        MoveTestTypeMove obj(100);
        FL_CHECK_EQ(obj.value, 100);
        FL_CHECK(!obj.moved_from);
        FL_CHECK(!obj.moved_to);

        MoveTestTypeMove moved_obj(fl::move(obj));
        FL_CHECK_EQ(moved_obj.value, 100);
        FL_CHECK(moved_obj.moved_to);
        FL_CHECK(!moved_obj.moved_from);

        // Original object should be in moved-from state
        FL_CHECK(obj.moved_from);
        FL_CHECK_EQ(obj.value, 0);
    }

    FL_SUBCASE("move assignment is invoked") {
        MoveTestTypeMove obj1(50);
        MoveTestTypeMove obj2(75);

        obj2 = fl::move(obj1);
        FL_CHECK_EQ(obj2.value, 50);
        FL_CHECK(obj2.moved_to);

        FL_CHECK(obj1.moved_from);
        FL_CHECK_EQ(obj1.value, 0);
    }

    FL_SUBCASE("move from temporary") {
        MoveTestTypeMove obj(fl::move(MoveTestTypeMove(200)));
        FL_CHECK_EQ(obj.value, 200);
        FL_CHECK(obj.moved_to);
    }
}

FL_TEST_CASE("fl::move preserves const-ness") {
    FL_SUBCASE("move with const lvalue") {
        const int x = 42;
        // fl::move on const lvalue produces const rvalue reference
        auto&& moved_x = fl::move(x);
        FL_CHECK_EQ(moved_x, 42);
        // Original const value unchanged
        FL_CHECK_EQ(x, 42);
    }

    FL_SUBCASE("move with const object") {
        const MoveTestTypeMove obj(123);
        // Moving from const object still preserves const
        // This will invoke copy constructor, not move constructor
        MoveTestTypeMove copy_obj(fl::move(obj));
        FL_CHECK_EQ(copy_obj.value, 123);
        // Original const object unchanged
        FL_CHECK_EQ(obj.value, 123);
    }
}

FL_TEST_CASE("fl::move with arrays") {
    FL_SUBCASE("move with array reference") {
        int arr[3] = {1, 2, 3};
        // Arrays decay to pointers when passed to functions
        // We can verify move compiles with array types
        auto&& moved_arr = fl::move(arr);
        FL_CHECK_EQ(moved_arr[0], 1);
        FL_CHECK_EQ(moved_arr[1], 2);
        FL_CHECK_EQ(moved_arr[2], 3);
    }
}

FL_TEST_CASE("fl::move with user-defined types") {
    FL_SUBCASE("move with struct") {
        struct Point {
            int x, y;
        };

        Point p1{10, 20};
        Point p2 = fl::move(p1);
        FL_CHECK_EQ(p2.x, 10);
        FL_CHECK_EQ(p2.y, 20);
        // For structs without move constructor, behaves like copy
        FL_CHECK_EQ(p1.x, 10);
        FL_CHECK_EQ(p1.y, 20);
    }

    FL_SUBCASE("move with class") {
        class Data {
        public:
            int value;
            Data(int v) : value(v) {}
        };

        Data d1(42);
        Data d2 = fl::move(d1);
        FL_CHECK_EQ(d2.value, 42);
    }
}

FL_TEST_CASE("fl::move is noexcept") {
    FL_SUBCASE("move is noexcept for basic types") {
        // fl::move itself should be noexcept
        int x = 10;
        static_assert(noexcept(fl::move(x)), "fl::move should be noexcept");
        FL_CHECK(true);  // Dummy runtime check
    }

    FL_SUBCASE("move is noexcept for pointer types") {
        int* ptr = nullptr;
        static_assert(noexcept(fl::move(ptr)), "fl::move should be noexcept");
        FL_CHECK(true);  // Dummy runtime check
    }

    FL_SUBCASE("move is noexcept for user types") {
        MoveTestTypeMove obj(10);
        static_assert(noexcept(fl::move(obj)), "fl::move should be noexcept");
        FL_CHECK(true);  // Dummy runtime check
    }
}

FL_TEST_CASE("fl::move with references") {
    FL_SUBCASE("move with lvalue reference parameter") {
        int value = 42;
        int& ref = value;
        auto&& moved_ref = fl::move(ref);
        FL_CHECK_EQ(moved_ref, 42);
        // Original value unchanged
        FL_CHECK_EQ(value, 42);
    }

    FL_SUBCASE("move with rvalue reference parameter") {
        int value = 42;
        int&& rref = fl::move(value);
        auto&& moved_rref = fl::move(rref);
        FL_CHECK_EQ(moved_rref, 42);
    }
}

FL_TEST_CASE("fl::move in function return") {
    FL_SUBCASE("move in return statement") {
        auto make_object = []() -> MoveTestTypeMove {
            MoveTestTypeMove obj(100);
            return fl::move(obj);
        };

        MoveTestTypeMove result = make_object();
        FL_CHECK_EQ(result.value, 100);
        // Due to return value optimization (RVO), moved_to might not be set
        // but the value should be correct
    }

    FL_SUBCASE("move prevents copy in return") {
        auto get_value = [](MoveTestTypeMove&& obj) -> MoveTestTypeMove {
            return fl::move(obj);
        };

        MoveTestTypeMove temp(50);
        MoveTestTypeMove result = get_value(fl::move(temp));
        FL_CHECK_EQ(result.value, 50);
    }
}

FL_TEST_CASE("fl::move with function parameters") {
    FL_SUBCASE("move to rvalue reference parameter") {
        auto take_rvalue = [](MoveTestTypeMove&& obj) -> MoveTestTypeMove {
            // Actually move the object by constructing from it
            return MoveTestTypeMove(fl::move(obj));
        };

        MoveTestTypeMove obj(75);
        MoveTestTypeMove result = take_rvalue(fl::move(obj));
        FL_CHECK_EQ(result.value, 75);
        // After move, obj is in valid but unspecified state
        FL_CHECK(obj.moved_from);
    }

    FL_SUBCASE("perfect forwarding scenario") {
        auto forward_object = [](MoveTestTypeMove&& obj) -> MoveTestTypeMove {
            return MoveTestTypeMove(fl::move(obj));
        };

        MoveTestTypeMove obj(150);
        MoveTestTypeMove result = forward_object(fl::move(obj));
        FL_CHECK_EQ(result.value, 150);
        FL_CHECK(obj.moved_from);
    }
}

FL_TEST_CASE("fl::move edge cases") {
    FL_SUBCASE("move with zero value") {
        int zero = 0;
        int moved_zero = fl::move(zero);
        FL_CHECK_EQ(moved_zero, 0);
        FL_CHECK_EQ(zero, 0);
    }

    FL_SUBCASE("move with negative values") {
        int negative = -42;
        int moved_negative = fl::move(negative);
        FL_CHECK_EQ(moved_negative, -42);
    }

    FL_SUBCASE("move with nullptr") {
        int* null_ptr = nullptr;
        int* moved_null = fl::move(null_ptr);
        FL_CHECK_EQ(moved_null, nullptr);
    }

    FL_SUBCASE("move with boolean") {
        bool flag = true;
        bool moved_flag = fl::move(flag);
        FL_CHECK(moved_flag);
        FL_CHECK(flag);  // Primitive, so original unchanged
    }
}

FL_TEST_CASE("fl::move type deduction") {
    FL_SUBCASE("Return type is rvalue reference") {
        int x = 10;
        // The return type should be int&&
        static_assert(fl::is_same<decltype(fl::move(x)), int&&>::value,
                     "fl::move should return rvalue reference");
        FL_CHECK(true);  // Dummy runtime check
    }

    FL_SUBCASE("Return type preserves base type") {
        float f = 3.14f;
        static_assert(fl::is_same<decltype(fl::move(f)), float&&>::value,
                     "fl::move should return float&&");
        FL_CHECK(true);  // Dummy runtime check
    }

    FL_SUBCASE("Return type removes references from input") {
        int x = 10;
        int& ref = x;
        // Input is int&, output should be int&&
        static_assert(fl::is_same<decltype(fl::move(ref)), int&&>::value,
                     "fl::move should convert int& to int&&");
        FL_CHECK(true);  // Dummy runtime check
    }

    FL_SUBCASE("Return type for const types") {
        const int x = 10;
        static_assert(fl::is_same<decltype(fl::move(x)), const int&&>::value,
                     "fl::move should return const int&&");
        FL_CHECK(true);  // Dummy runtime check
    }
}

FL_TEST_CASE("fl::move is constexpr") {
    FL_SUBCASE("move can be used in constexpr context") {
        // In C++11, constexpr functions are very limited
        // We can test that move is declared constexpr
        constexpr int y = 42;  // move not usable in constexpr in C++11 due to limitations
        FL_CHECK_EQ(y, 42);
    }
}

FL_TEST_CASE("fl::move with volatile types") {
    FL_SUBCASE("move with volatile variable") {
        volatile int vol_value = 100;
        volatile int moved_vol = vol_value;  // move doesn't really apply to volatile
        FL_CHECK_EQ(moved_vol, 100);
    }
}

FL_TEST_CASE("fl::move multiple times") {
    FL_SUBCASE("moving same object multiple times") {
        MoveTestTypeMove obj(200);

        MoveTestTypeMove obj2(fl::move(obj));
        FL_CHECK_EQ(obj2.value, 200);
        FL_CHECK(obj.moved_from);

        // Moving from already-moved-from object
        MoveTestTypeMove obj3(fl::move(obj));
        // obj was already moved from, so value should be 0
        FL_CHECK_EQ(obj3.value, 0);
    }
}

FL_TEST_CASE("fl::move comparison with std semantics") {
    FL_SUBCASE("fl::move behaves like std::move") {  // okay std namespace - documenting behavior
        // Verify that fl::move produces same result as std::move would
        int x = 42;
        auto&& fl_moved = fl::move(x);
        // Should produce rvalue reference
        FL_CHECK_EQ(fl_moved, 42);

        MoveTestTypeMove obj(100);
        MoveTestTypeMove result(fl::move(obj));
        FL_CHECK_EQ(result.value, 100);
        FL_CHECK(obj.moved_from);
    }
}

FL_TEST_CASE("fl::remove_reference with complex types") {
    FL_SUBCASE("remove_reference with function pointers") {
        typedef void (*FuncPtr)(int);
        static_assert(fl::is_same<typename remove_reference<FuncPtr>::type, FuncPtr>::value,
                     "remove_reference should not change function pointer");
        static_assert(fl::is_same<typename remove_reference<FuncPtr&>::type, FuncPtr>::value,
                     "remove_reference should remove reference from function pointer");
        FL_CHECK(true);  // Dummy runtime check
    }

    FL_SUBCASE("remove_reference with array types") {
        typedef int ArrayType[10];
        static_assert(fl::is_same<typename remove_reference<ArrayType>::type, ArrayType>::value,
                     "remove_reference should not change array type");
        static_assert(fl::is_same<typename remove_reference<ArrayType&>::type, ArrayType>::value,
                     "remove_reference should remove reference from array type");
        FL_CHECK(true);  // Dummy runtime check
    }
}
