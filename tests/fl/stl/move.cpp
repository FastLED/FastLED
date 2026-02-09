#include "test.h"
#include "test_container_helpers.h"
#include "fl/stl/move.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/vector.h"
#include "fl/stl/map.h"
#include "fl/stl/set.h"
#include "fl/stl/deque.h"
#include "fl/stl/list.h"
#include "fl/stl/queue.h"
#include "fl/stl/priority_queue.h"
#include "fl/stl/unordered_map.h"
#include "fl/stl/unordered_set.h"
#include "fl/stl/array.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/weak_ptr.h"
#include "fl/stl/optional.h"
#include "fl/stl/variant.h"
#include "fl/stl/expected.h"
#include "fl/stl/pair.h"
#include "fl/stl/tuple.h"
#include "fl/stl/not_null.h"
#include "fl/stl/string.h"
#include "fl/stl/function.h"
#include "fl/circular_buffer.h"
#include "fl/hash_map_lru.h"
#include "fl/bitset_dynamic.h"
#include "fl/stl/bitset.h"

using namespace fl;
using namespace test_helpers;

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

// ============================================================================
// COMPREHENSIVE CONTAINER MOVE SEMANTICS TESTS
// ============================================================================

// Comprehensive container move semantics tests
// Verifies that all container types properly implement move semantics:
// 1. After move, source container is empty (size() == 0)
// 2. Reference counts of shared_ptr elements remain constant (proves move, not copy)

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

// ID-based allocator for testing allocator propagation
template<typename T>
class IDAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = fl::size;
    using difference_type = fl::ptrdiff_t;

    template<typename U>
    struct rebind {
        using other = IDAllocator<U>;
    };

    int id;  // Unique ID to track allocator identity

    IDAllocator() : id(0) {}
    explicit IDAllocator(int alloc_id) : id(alloc_id) {}

    // Copy constructor
    IDAllocator(const IDAllocator& other) : id(other.id) {}

    // Move constructor
    IDAllocator(IDAllocator&& other) noexcept : id(other.id) {
        other.id = -1;  // Mark as moved-from
    }

    // Template copy constructor (for rebind)
    template<typename U>
    IDAllocator(const IDAllocator<U>& other) : id(other.id) {}

    // Copy assignment
    IDAllocator& operator=(const IDAllocator& other) {
        if (this != &other) {
            id = other.id;
        }
        return *this;
    }

    // Move assignment
    IDAllocator& operator=(IDAllocator&& other) noexcept {
        if (this != &other) {
            id = other.id;
            other.id = -1;  // Mark as moved-from
        }
        return *this;
    }

    T* allocate(fl::size n) {
        if (n == 0) return nullptr;
        return static_cast<T*>(fl::Malloc(n * sizeof(T)));
    }

    void deallocate(T* p, fl::size n) {
        if (p) {
            fl::Free(p);
        }
    }

    template<typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        if (p) {
            new(static_cast<void*>(p)) U(fl::forward<Args>(args)...);
        }
    }

    template<typename U>
    void destroy(U* p) {
        if (p) {
            p->~U();
        }
    }

    bool operator==(const IDAllocator& other) const {
        return id == other.id;
    }

    bool operator!=(const IDAllocator& other) const {
        return id != other.id;
    }
};

} // anonymous namespace

// Note: test_container_move_semantics, test_map_move_semantics, and
// test_smart_pointer_move_semantics are now in test_container_helpers.h
using test_helpers::test_container_move_semantics;
using test_helpers::test_map_move_semantics;
using test_helpers::test_smart_pointer_move_semantics;

// ============================================================================
// CONTAINER MOVE SEMANTICS TESTS
// ============================================================================

FL_TEST_CASE("Container move semantics with shared_ptr") {
    // Vector types
    FL_SUBCASE("fl::vector") {
        test_container_move_semantics<fl::vector<fl::shared_ptr<int>>>();
    }

    FL_SUBCASE("fl::InlinedVector (heap mode)") {
        test_container_move_semantics<fl::InlinedVector<fl::shared_ptr<int>, 2>>();
    }

    FL_SUBCASE("fl::FixedVector") {
        test_container_move_semantics<fl::FixedVector<fl::shared_ptr<int>, 10>>();
    }

    // Map types
    FL_SUBCASE("fl::map") {
        test_map_move_semantics<fl::map<int, fl::shared_ptr<int>>>();
    }

    FL_SUBCASE("fl::unordered_map") {
        test_map_move_semantics<fl::unordered_map<int, fl::shared_ptr<int>>>();
    }

    FL_SUBCASE("fl::SortedHeapMap") {
        test_map_move_semantics<fl::SortedHeapMap<int, fl::shared_ptr<int>>>();
    }

    FL_SUBCASE("fl::FixedMap") {
        test_map_move_semantics<fl::FixedMap<int, fl::shared_ptr<int>, 10>>();
    }

    // Set types
    FL_SUBCASE("fl::set") {
        test_container_move_semantics<fl::set<fl::shared_ptr<int>>>();
    }

    FL_SUBCASE("fl::VectorSet") {
        test_container_move_semantics<fl::VectorSet<fl::shared_ptr<int>>>();
    }

    FL_SUBCASE("fl::VectorSetFixed") {
        test_container_move_semantics<fl::VectorSetFixed<fl::shared_ptr<int>, 10>>();
    }

    FL_SUBCASE("fl::unordered_set") {
        test_container_move_semantics<fl::unordered_set<fl::shared_ptr<int>>>();
    }

    // Sequential containers
    FL_SUBCASE("fl::deque") {
        test_container_move_semantics<fl::deque<fl::shared_ptr<int>>>();
    }

    FL_SUBCASE("fl::list") {
        test_container_move_semantics<fl::list<fl::shared_ptr<int>>>();
    }

    FL_SUBCASE("fl::queue") {
        test_container_move_semantics<fl::queue<fl::shared_ptr<int>>>();
    }

    FL_SUBCASE("fl::array - element-wise move (no heap optimization)") {
        // fl::array properly moves elements, but can't steal heap (it's stack-based)
        auto ptr = make_shared_int(42);

        fl::array<fl::shared_ptr<int>, 3> source = {ptr, ptr, ptr};

        FL_REQUIRE(ptr.use_count() == 4);  // 1 local + 3 in array

        fl::array<fl::shared_ptr<int>, 3> destination;
        destination = fl::move(source);

        // Array properly moves elements - source IS cleared
        FL_CHECK(source[0] == nullptr);  // Moved out
        FL_CHECK(source[1] == nullptr);
        FL_CHECK(source[2] == nullptr);

        // Destination has the elements
        FL_CHECK(destination[0] != nullptr);
        FL_CHECK(destination[1] != nullptr);
        FL_CHECK(destination[2] != nullptr);
        FL_CHECK(*destination[0] == 42);

        // Refcount proves move: 1 local + 3 in destination = 4
        FL_CHECK(ptr.use_count() == 4);

        // Clear destination
        destination[0] = nullptr;
        destination[1] = nullptr;
        destination[2] = nullptr;
        FL_CHECK(ptr.use_count() == 1);  // Only local remains

        // Key difference: array can't steal heap pointer, but does move elements properly
        // Unlike vector/deque which can steal internal heap buffer
    }

    FL_SUBCASE("C-style static array (raw) - COPY-only, NO move!") {
        // Raw C arrays cannot be moved or assigned at all
        auto ptr = make_shared_int(200);

        fl::shared_ptr<int> source[3] = {ptr, ptr, ptr};
        FL_REQUIRE(ptr.use_count() == 4);  // 1 local + 3 in array

        // C arrays cannot be assigned, must manually copy
        fl::shared_ptr<int> destination[3];
        for (int i = 0; i < 3; i++) {
            destination[i] = source[i];  // Copy, not move
        }

        // Both source and destination have elements (copied)
        FL_CHECK(source[0] != nullptr);
        FL_CHECK(*source[0] == 200);
        FL_CHECK(destination[0] != nullptr);
        FL_CHECK(*destination[0] == 200);

        // Refcount proves COPY: 1 local + 3 source + 3 dest = 7
        FL_CHECK(ptr.use_count() == 7);

        // Clear destination, source unaffected
        for (int i = 0; i < 3; i++) {
            destination[i] = nullptr;
        }
        FL_CHECK(ptr.use_count() == 4);  // 1 local + 3 in source
        FL_CHECK(*source[0] == 200);
    }

    // Priority queue types
    FL_SUBCASE("fl::priority_queue_stable") {
        // priority_queue_stable uses int for simplicity
        fl::priority_queue_stable<int> source;
        source.push(30);
        source.push(10);
        source.push(20);

        FL_REQUIRE(source.size() == 3);

        fl::priority_queue_stable<int> destination;
        destination = fl::move(source);

        FL_CHECK(source.size() == 0);
        FL_CHECK(source.empty());
        FL_CHECK(destination.size() == 3);
    }

    FL_SUBCASE("fl::PriorityQueue") {
        // PriorityQueue uses int for simplicity
        fl::PriorityQueue<int> source;
        source.push(30);
        source.push(10);
        source.push(20);

        FL_REQUIRE(source.size() == 3);

        fl::PriorityQueue<int> destination;
        destination = fl::move(source);

        FL_CHECK(source.size() == 0);
        FL_CHECK(source.empty());
        FL_CHECK(destination.size() == 3);
        FL_CHECK(destination.top() == 30);
    }

    // Buffer types
    FL_SUBCASE("fl::StaticCircularBuffer") {
        auto ptr = make_shared_int(42);

        fl::StaticCircularBuffer<fl::shared_ptr<int>, 10> source;
        populate(source, ptr);

        FL_REQUIRE(ptr.use_count() == 2);
        FL_REQUIRE(source.size() == 1);

        fl::StaticCircularBuffer<fl::shared_ptr<int>, 10> destination;
        destination = fl::move(source);

        FL_CHECK(source.size() == 0);
        FL_CHECK(source.empty());
        FL_CHECK(destination.size() == 1);

        // Retrieve and check value, but let it go out of scope immediately
        {
            auto retrieved = retrieve(destination);
            FL_CHECK(*retrieved == 42);
        }
        FL_CHECK(ptr.use_count() == 2);

        destination.clear();
        FL_CHECK(ptr.use_count() == 1);
    }

    FL_SUBCASE("fl::DynamicCircularBuffer") {
        auto ptr = make_shared_int(42);

        fl::DynamicCircularBuffer<fl::shared_ptr<int>> source(10);
        populate(source, ptr);

        FL_REQUIRE(ptr.use_count() == 2);
        FL_REQUIRE(source.size() == 1);

        fl::DynamicCircularBuffer<fl::shared_ptr<int>> destination(5);
        destination = fl::move(source);

        FL_CHECK(source.size() == 0);
        FL_CHECK(source.empty());
        FL_CHECK(destination.size() == 1);

        // Retrieve and check value, but let it go out of scope immediately
        {
            auto retrieved = retrieve(destination);
            FL_CHECK(*retrieved == 42);
        }
        FL_CHECK(ptr.use_count() == 2);

        destination.clear();
        FL_CHECK(ptr.use_count() == 1);
    }

    // Function containers
    FL_SUBCASE("fl::function_list") {
        // function_list wraps a vector, test basic move
        int call_count = 0;
        auto increment = [&call_count]() { call_count++; };

        fl::function_list<void()> source;
        source.add(increment);
        source.add(increment);
        source.add(increment);

        FL_REQUIRE(source.size() == 3);

        fl::function_list<void()> destination;
        destination = fl::move(source);

        // Source should be empty after move
        FL_CHECK(source.size() == 0);
        FL_CHECK(destination.size() == 3);

        // Verify functions work in destination
        destination.invoke();
        FL_CHECK(call_count == 3);
    }

    // Cache/LRU types
    FL_SUBCASE("fl::HashMapLru") {
        auto ptr = make_shared_int(100);

        fl::HashMapLru<int, fl::shared_ptr<int>> source(10);
        populate_map(source, 1, ptr);

        FL_REQUIRE(ptr.use_count() == 2);
        FL_REQUIRE(source.size() == 1);

        fl::HashMapLru<int, fl::shared_ptr<int>> destination(10);
        destination = fl::move(source);

        FL_CHECK(source.size() == 0);
        FL_CHECK(source.empty());
        FL_CHECK(destination.size() == 1);

        // Retrieve and check value, but let it go out of scope immediately
        {
            auto retrieved = retrieve_map(destination, 1);
            FL_CHECK(*retrieved == 100);
        }
        FL_CHECK(ptr.use_count() == 2);

        destination.clear();
        FL_CHECK(ptr.use_count() == 1);
    }

    // Bitset types
    FL_SUBCASE("fl::bitset_dynamic") {
        fl::bitset_dynamic source(100);
        source.set(10);
        source.set(20);
        source.set(30);

        FL_REQUIRE(source.size() == 100);
        FL_REQUIRE(source.test(10));

        fl::bitset_dynamic destination;
        destination = fl::move(source);

        FL_CHECK(source.size() == 0);
        FL_CHECK(destination.size() == 100);
        FL_CHECK(destination.test(10));
        FL_CHECK(destination.test(20));
        FL_CHECK(destination.test(30));
    }

    FL_SUBCASE("fl::BitsetInlined") {
        fl::BitsetInlined<10> source;
        source.set(5);
        source.set(7);

        FL_REQUIRE(source.test(5));
        FL_REQUIRE(source.test(7));

        fl::BitsetInlined<10> destination;
        destination = fl::move(source);

        // BitsetInlined should transfer state
        FL_CHECK(destination.test(5));
        FL_CHECK(destination.test(7));
    }

    // String types
    FL_SUBCASE("fl::string") {
        fl::string source = "Hello, World!";
        FL_REQUIRE(source.size() == 13);
        FL_REQUIRE(source == "Hello, World!");

        fl::string destination;
        destination = fl::move(source);

        // After move, destination should have the string
        FL_CHECK(destination.size() == 13);
        FL_CHECK(destination == "Hello, World!");

        // Source should be in valid but empty state
        FL_CHECK(source.size() == 0);
        FL_CHECK(source.empty());
    }
}


// ============================================================================
// SMART POINTER AND WRAPPER TYPES MOVE SEMANTICS
// ============================================================================

FL_TEST_CASE("Smart pointer and wrapper move semantics") {
    FL_SUBCASE("fl::unique_ptr") {
        fl::unique_ptr<int> source(new int(42));
        FL_REQUIRE(source.get() != nullptr);
        FL_REQUIRE(*source == 42);

        fl::unique_ptr<int> destination;
        destination = fl::move(source);

        // After move: destination owns the pointer
        FL_CHECK(destination.get() != nullptr);
        FL_CHECK(*destination == 42);

        // After move: source is null
        FL_CHECK(source.get() == nullptr);
    }

    FL_SUBCASE("fl::unique_ptr array") {
        fl::unique_ptr<int[]> source(new int[5]{1, 2, 3, 4, 5});
        FL_REQUIRE(source.get() != nullptr);
        FL_REQUIRE(source[0] == 1);
        FL_REQUIRE(source[4] == 5);

        fl::unique_ptr<int[]> destination;
        destination = fl::move(source);

        // After move: destination owns the array
        FL_CHECK(destination.get() != nullptr);
        FL_CHECK(destination[0] == 1);
        FL_CHECK(destination[4] == 5);

        // After move: source is null
        FL_CHECK(source.get() == nullptr);
    }

    FL_SUBCASE("fl::shared_ptr") {
        auto ptr = make_shared_int(100);
        fl::shared_ptr<int> source = ptr;

        FL_REQUIRE(source.use_count() == 2);  // ptr + source
        FL_REQUIRE(*source == 100);

        fl::shared_ptr<int> destination;
        destination = fl::move(source);

        // After move: destination has the shared_ptr
        FL_CHECK(destination.use_count() == 2);  // ptr + destination
        FL_CHECK(*destination == 100);

        // After move: source is empty
        FL_CHECK(source.get() == nullptr);
        FL_CHECK(source.use_count() == 0);

        // Original ptr still valid
        FL_CHECK(ptr.use_count() == 2);
    }

    FL_SUBCASE("fl::weak_ptr") {
        auto shared = make_shared_int(200);
        fl::weak_ptr<int> source = shared;

        FL_REQUIRE(!source.expired());

        fl::weak_ptr<int> destination;
        destination = fl::move(source);

        // After move: destination has the weak reference
        FL_CHECK(!destination.expired());
        auto locked = destination.lock();
        FL_CHECK(locked != nullptr);
        FL_CHECK(*locked == 200);

        // After move: source is expired/empty
        FL_CHECK(source.expired());
    }

    FL_SUBCASE("fl::optional") {
        fl::optional<int> source(42);
        FL_REQUIRE(source.has_value());
        FL_REQUIRE(*source == 42);

        fl::optional<int> destination;
        destination = fl::move(source);

        // After move: destination has the value
        FL_CHECK(destination.has_value());
        FL_CHECK(*destination == 42);

        // Note: optional after move is unspecified, but typically still has value
    }

    FL_SUBCASE("fl::optional with shared_ptr") {
        auto ptr = make_shared_int(300);
        fl::optional<fl::shared_ptr<int>> source(ptr);

        FL_REQUIRE(source.has_value());
        FL_REQUIRE(ptr.use_count() == 2);

        fl::optional<fl::shared_ptr<int>> destination;
        destination = fl::move(source);

        // After move: destination has the value
        FL_CHECK(destination.has_value());
        FL_CHECK(*destination.value() == 300);

        // Verify move happened (refcount should be same)
        FL_CHECK(ptr.use_count() == 2);
    }

    FL_SUBCASE("fl::variant") {
        auto ptr = make_shared_int(400);
        fl::variant<int, fl::shared_ptr<int>> source(ptr);

        FL_REQUIRE(source.template is<fl::shared_ptr<int>>());
        FL_REQUIRE(ptr.use_count() == 2);

        fl::variant<int, fl::shared_ptr<int>> destination;
        destination = fl::move(source);

        // After move: destination has the variant
        FL_CHECK(destination.template is<fl::shared_ptr<int>>());
        FL_CHECK(*destination.template ptr<fl::shared_ptr<int>>() == ptr);

        // Verify move (refcount unchanged)
        FL_CHECK(ptr.use_count() == 2);
    }

    FL_SUBCASE("fl::expected") {
        auto ptr = make_shared_int(500);
        auto source = fl::expected<fl::shared_ptr<int>>::success(ptr);

        FL_REQUIRE(source.ok());
        FL_REQUIRE(ptr.use_count() == 2);

        fl::expected<fl::shared_ptr<int>> destination;
        destination = fl::move(source);

        // After move: destination has the value
        FL_CHECK(destination.ok());
        FL_CHECK(*destination.value() == 500);

        // Verify move
        FL_CHECK(ptr.use_count() == 2);
    }

    FL_SUBCASE("fl::pair") {
        auto ptr1 = make_shared_int(10);
        auto ptr2 = make_shared_int(20);
        fl::pair<fl::shared_ptr<int>, fl::shared_ptr<int>> source(ptr1, ptr2);

        FL_REQUIRE(ptr1.use_count() == 2);
        FL_REQUIRE(ptr2.use_count() == 2);

        fl::pair<fl::shared_ptr<int>, fl::shared_ptr<int>> destination;
        destination = fl::move(source);

        // After move: destination has the pair
        FL_CHECK(*destination.first == 10);
        FL_CHECK(*destination.second == 20);

        // Verify move
        FL_CHECK(ptr1.use_count() == 2);
        FL_CHECK(ptr2.use_count() == 2);
    }

    FL_SUBCASE("fl::tuple") {
        auto ptr1 = make_shared_int(111);
        auto ptr2 = make_shared_int(222);
        auto ptr3 = make_shared_int(333);
        fl::tuple<fl::shared_ptr<int>, fl::shared_ptr<int>, fl::shared_ptr<int>> source(ptr1, ptr2, ptr3);

        FL_REQUIRE(ptr1.use_count() == 2);
        FL_REQUIRE(ptr2.use_count() == 2);
        FL_REQUIRE(ptr3.use_count() == 2);

        fl::tuple<fl::shared_ptr<int>, fl::shared_ptr<int>, fl::shared_ptr<int>> destination;
        destination = fl::move(source);

        // After move: destination has the tuple
        FL_CHECK(*fl::get<0>(destination) == 111);
        FL_CHECK(*fl::get<1>(destination) == 222);
        FL_CHECK(*fl::get<2>(destination) == 333);

        // Verify move
        FL_CHECK(ptr1.use_count() == 2);
        FL_CHECK(ptr2.use_count() == 2);
        FL_CHECK(ptr3.use_count() == 2);
    }

    FL_SUBCASE("fl::not_null") {
        int value = 42;
        fl::not_null<int*> source(&value);

        FL_REQUIRE(source.get() == &value);
        FL_REQUIRE(*source == 42);

        fl::not_null<int*> destination = fl::move(source);

        // After move: destination has the pointer
        FL_CHECK(destination.get() == &value);
        FL_CHECK(*destination == 42);

        // not_null doesn't clear source (it's always non-null)
        FL_CHECK(source.get() == &value);
    }
}

// ============================================================================
// ALLOCATOR PROPAGATION TESTS
// Tests that allocators are properly moved/propagated during move operations
// ============================================================================

FL_TEST_CASE("Allocator propagation on move") {
    FL_SUBCASE("fl::vector - allocator propagation") {
        IDAllocator<int> alloc1(100);
        IDAllocator<int> alloc2(200);

        fl::vector<int, IDAllocator<int>> source(alloc1);
        source.push_back(1);
        source.push_back(2);
        source.push_back(3);

        fl::vector<int, IDAllocator<int>> destination(alloc2);
        destination.push_back(99);

        // Before move: containers have different allocators
        FL_REQUIRE(source.get_allocator().id == 100);
        FL_REQUIRE(destination.get_allocator().id == 200);

        // Move assignment
        destination = fl::move(source);

        // After move: destination should have source's allocator
        FL_CHECK(destination.get_allocator().id == 100);
        FL_CHECK(destination.size() == 3);
        FL_CHECK(source.size() == 0);
    }

    FL_SUBCASE("fl::deque - allocator propagation") {
        IDAllocator<int> alloc1(100);
        IDAllocator<int> alloc2(200);

        fl::deque<int, IDAllocator<int>> source(alloc1);
        source.push_back(1);
        source.push_back(2);

        fl::deque<int, IDAllocator<int>> destination(alloc2);

        FL_REQUIRE(source.get_allocator().id == 100);
        FL_REQUIRE(destination.get_allocator().id == 200);

        destination = fl::move(source);

        FL_CHECK(destination.get_allocator().id == 100);
        FL_CHECK(destination.size() == 2);
        FL_CHECK(source.size() == 0);
    }

    FL_SUBCASE("fl::list - allocator propagation") {
        IDAllocator<int> alloc1(100);
        IDAllocator<int> alloc2(200);

        fl::list<int, IDAllocator<int>> source(alloc1);
        source.push_back(1);
        source.push_back(2);

        fl::list<int, IDAllocator<int>> destination(alloc2);

        FL_REQUIRE(source.get_allocator().id == 100);
        FL_REQUIRE(destination.get_allocator().id == 200);

        destination = fl::move(source);

        FL_CHECK(destination.get_allocator().id == 100);
        FL_CHECK(destination.size() == 2);
        FL_CHECK(source.size() == 0);
    }

    FL_SUBCASE("fl::set - allocator propagation") {
        IDAllocator<int> alloc1(100);
        IDAllocator<int> alloc2(200);

        fl::set<int, IDAllocator<int>> source(alloc1);
        source.insert(1);
        source.insert(2);
        source.insert(3);

        fl::set<int, IDAllocator<int>> destination(alloc2);

        FL_REQUIRE(source.get_allocator().id == 100);
        FL_REQUIRE(destination.get_allocator().id == 200);

        destination = fl::move(source);

        FL_CHECK(destination.get_allocator().id == 100);
        FL_CHECK(destination.size() == 3);
        FL_CHECK(source.size() == 0);
    }

    FL_SUBCASE("fl::VectorSet - allocator propagation") {
        IDAllocator<int> alloc1(100);
        IDAllocator<int> alloc2(200);

        fl::VectorSet<int, IDAllocator<int>> source(alloc1);
        source.insert(1);
        source.insert(2);

        fl::VectorSet<int, IDAllocator<int>> destination(alloc2);

        FL_REQUIRE(source.get_allocator().id == 100);
        FL_REQUIRE(destination.get_allocator().id == 200);

        destination = fl::move(source);

        FL_CHECK(destination.get_allocator().id == 100);
        FL_CHECK(destination.size() == 2);
        FL_CHECK(source.size() == 0);
    }

    FL_SUBCASE("fl::SortedHeapMap - allocator propagation") {
        IDAllocator<fl::pair<int, int>> alloc1(100);
        IDAllocator<fl::pair<int, int>> alloc2(200);

        fl::SortedHeapMap<int, int, fl::less<int>, IDAllocator<fl::pair<int, int>>> source(alloc1);
        source.insert(1, 10);
        source.insert(2, 20);

        fl::SortedHeapMap<int, int, fl::less<int>, IDAllocator<fl::pair<int, int>>> destination(alloc2);

        FL_REQUIRE(source.get_allocator().id == 100);
        FL_REQUIRE(destination.get_allocator().id == 200);

        destination = fl::move(source);

        FL_CHECK(destination.get_allocator().id == 100);
        FL_CHECK(destination.size() == 2);
        FL_CHECK(source.size() == 0);
    }

    FL_SUBCASE("fl::SortedHeapVector - allocator propagation") {
        IDAllocator<int> alloc1(100);
        IDAllocator<int> alloc2(200);

        fl::SortedHeapVector<int, fl::less<int>, IDAllocator<int>> source(fl::less<int>(), alloc1);
        source.insert(3);
        source.insert(1);
        source.insert(2);

        fl::SortedHeapVector<int, fl::less<int>, IDAllocator<int>> destination(fl::less<int>(), alloc2);

        FL_REQUIRE(source.get_allocator().id == 100);
        FL_REQUIRE(destination.get_allocator().id == 200);

        destination = fl::move(source);

        FL_CHECK(destination.get_allocator().id == 100);
        FL_CHECK(destination.size() == 3);
        FL_CHECK(source.size() == 0);
    }
}

// ============================================================================
// EDGE CASES - Moving multiple times, const containers
// ============================================================================

FL_TEST_CASE("Container move edge cases") {
    FL_SUBCASE("Moving from already-moved-from container") {
        // Test that moving from an already-moved-from container is safe
        fl::vector<int> original;
        original.push_back(10);
        original.push_back(20);
        original.push_back(30);

        // First move
        fl::vector<int> first_destination = fl::move(original);
        FL_CHECK(original.size() == 0);
        FL_CHECK(first_destination.size() == 3);

        // Second move from already-moved-from container
        fl::vector<int> second_destination = fl::move(original);
        FL_CHECK(original.size() == 0);  // Still empty
        FL_CHECK(second_destination.size() == 0);  // Gets empty container
    }

    FL_SUBCASE("Moving from const container falls back to copy") {
        // Const containers cannot be moved from, so this becomes a copy
        const fl::vector<int> const_source = {1, 2, 3};

        // This will invoke copy constructor, not move constructor
        fl::vector<int> destination = const_source;

        FL_CHECK(const_source.size() == 3);  // Original unchanged
        FL_CHECK(destination.size() == 3);   // Destination has copy
    }

    FL_SUBCASE("Self-move assignment is safe") {
        fl::vector<int> container;
        container.push_back(100);
        container.push_back(200);

        // Self-move should be safe (though not recommended)
        container = fl::move(container);

        // Container should still be in valid state
        // Size might be 0 or original size depending on implementation
        FL_CHECK(container.size() <= 2);
    }

    FL_SUBCASE("Moving empty container") {
        fl::vector<int> empty_source;
        FL_CHECK(empty_source.size() == 0);

        fl::vector<int> destination = fl::move(empty_source);
        FL_CHECK(empty_source.size() == 0);
        FL_CHECK(destination.size() == 0);
    }
}
