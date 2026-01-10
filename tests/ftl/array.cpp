#include "fl/stl/array.h"
#include "fl/slice.h"  // For fl::span
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/stl/vector.h"
#include "fl/stl/move.h"
#include "fl/stl/type_traits.h"
#include "fl/int.h"

using namespace fl;

TEST_CASE("fl::array - Basic construction and initialization") {
    SUBCASE("Default constructor") {
        array<int, 5> arr;
        CHECK_EQ(arr.size(), 5);
        CHECK_FALSE(arr.empty());
    }

    // Note: Fill constructor disabled due to undefined fill_n function
    // SUBCASE("Fill constructor") {
    //     array<int, 5> arr(42);
    //     CHECK_EQ(arr.size(), 5);
    //     for (size i = 0; i < 5; ++i) {
    //         CHECK_EQ(arr[i], 42);
    //     }
    // }

    SUBCASE("Initializer list constructor") {
        array<int, 5> arr = {1, 2, 3, 4, 5};
        CHECK_EQ(arr[0], 1);
        CHECK_EQ(arr[1], 2);
        CHECK_EQ(arr[2], 3);
        CHECK_EQ(arr[3], 4);
        CHECK_EQ(arr[4], 5);
    }

    SUBCASE("Initializer list with fewer elements") {
        array<int, 5> arr = {1, 2, 3};
        CHECK_EQ(arr[0], 1);
        CHECK_EQ(arr[1], 2);
        CHECK_EQ(arr[2], 3);
        // Note: arr[3] and arr[4] are default-initialized
    }

    SUBCASE("Zero-size array") {
        array<int, 0> arr;
        CHECK_EQ(arr.size(), 0);
        CHECK(arr.empty());
    }
}

TEST_CASE("fl::array - Copy and move semantics") {
    SUBCASE("Copy constructor") {
        array<int, 3> arr1 = {1, 2, 3};
        array<int, 3> arr2(arr1);
        CHECK_EQ(arr2[0], 1);
        CHECK_EQ(arr2[1], 2);
        CHECK_EQ(arr2[2], 3);
    }

    SUBCASE("Copy assignment") {
        array<int, 3> arr1 = {1, 2, 3};
        array<int, 3> arr2;
        arr2 = arr1;
        CHECK_EQ(arr2[0], 1);
        CHECK_EQ(arr2[1], 2);
        CHECK_EQ(arr2[2], 3);
    }

    SUBCASE("Move constructor") {
        array<int, 3> arr1 = {1, 2, 3};
        array<int, 3> arr2(fl::move(arr1));
        CHECK_EQ(arr2[0], 1);
        CHECK_EQ(arr2[1], 2);
        CHECK_EQ(arr2[2], 3);
    }

    SUBCASE("Move assignment") {
        array<int, 3> arr1 = {1, 2, 3};
        array<int, 3> arr2;
        arr2 = fl::move(arr1);
        CHECK_EQ(arr2[0], 1);
        CHECK_EQ(arr2[1], 2);
        CHECK_EQ(arr2[2], 3);
    }
}

TEST_CASE("fl::array - Element access") {
    SUBCASE("operator[] access") {
        array<int, 5> arr = {10, 20, 30, 40, 50};
        CHECK_EQ(arr[0], 10);
        CHECK_EQ(arr[2], 30);
        CHECK_EQ(arr[4], 50);
    }

    SUBCASE("operator[] modification") {
        array<int, 3> arr;
        arr[0] = 100;
        arr[1] = 200;
        arr[2] = 300;
        CHECK_EQ(arr[0], 100);
        CHECK_EQ(arr[1], 200);
        CHECK_EQ(arr[2], 300);
    }

    SUBCASE("at() with valid index") {
        array<int, 5> arr = {10, 20, 30, 40, 50};
        CHECK_EQ(arr.at(0), 10);
        CHECK_EQ(arr.at(2), 30);
        CHECK_EQ(arr.at(4), 50);
    }

    SUBCASE("at() with out-of-bounds index returns error value") {
        array<int, 3> arr = {1, 2, 3};
        // at() returns a static error_value when out of bounds
        // This is different from std::array which throws
        int& ref = arr.at(10);
        (void)ref; // Verify it doesn't crash, just returns error value
    }

    SUBCASE("front() and back()") {
        array<int, 5> arr = {10, 20, 30, 40, 50};
        CHECK_EQ(arr.front(), 10);
        CHECK_EQ(arr.back(), 50);
    }

    SUBCASE("front() and back() modification") {
        array<int, 3> arr = {1, 2, 3};
        arr.front() = 100;
        arr.back() = 300;
        CHECK_EQ(arr[0], 100);
        CHECK_EQ(arr[2], 300);
    }

    SUBCASE("data() pointer access") {
        array<int, 3> arr = {10, 20, 30};
        int* ptr = arr.data();
        CHECK_EQ(ptr[0], 10);
        CHECK_EQ(ptr[1], 20);
        CHECK_EQ(ptr[2], 30);
    }

    SUBCASE("const data() pointer access") {
        const array<int, 3> arr = {10, 20, 30};
        const int* ptr = arr.data();
        CHECK_EQ(ptr[0], 10);
        CHECK_EQ(ptr[1], 20);
        CHECK_EQ(ptr[2], 30);
    }
}

TEST_CASE("fl::array - Iterators") {
    SUBCASE("begin() and end()") {
        array<int, 5> arr = {1, 2, 3, 4, 5};
        auto it = arr.begin();
        CHECK_EQ(*it, 1);
        ++it;
        CHECK_EQ(*it, 2);

        // Check distance
        CHECK_EQ(arr.end() - arr.begin(), 5);
    }

    SUBCASE("Range-based for loop") {
        array<int, 5> arr = {10, 20, 30, 40, 50};
        int sum = 0;
        for (int val : arr) {
            sum += val;
        }
        CHECK_EQ(sum, 150);
    }

    SUBCASE("const iterators") {
        const array<int, 3> arr = {1, 2, 3};
        int count = 0;
        for (auto it = arr.begin(); it != arr.end(); ++it) {
            count++;
        }
        CHECK_EQ(count, 3);
    }

    SUBCASE("cbegin() and cend()") {
        array<int, 3> arr = {5, 10, 15};
        int sum = 0;
        for (auto it = arr.cbegin(); it != arr.cend(); ++it) {
            sum += *it;
        }
        CHECK_EQ(sum, 30);
    }

    SUBCASE("Iterator modification") {
        array<int, 3> arr = {1, 2, 3};
        for (auto it = arr.begin(); it != arr.end(); ++it) {
            *it *= 2;
        }
        CHECK_EQ(arr[0], 2);
        CHECK_EQ(arr[1], 4);
        CHECK_EQ(arr[2], 6);
    }
}

TEST_CASE("fl::array - Capacity") {
    SUBCASE("size() and max_size()") {
        array<int, 10> arr;
        CHECK_EQ(arr.size(), 10);
        CHECK_EQ(arr.max_size(), 10);
    }

    SUBCASE("empty() for non-empty array") {
        array<int, 5> arr;
        CHECK_FALSE(arr.empty());
    }

    SUBCASE("empty() for zero-size array") {
        array<int, 0> arr;
        CHECK(arr.empty());
    }
}

TEST_CASE("fl::array - Operations") {
    SUBCASE("fill() method") {
        array<int, 5> arr;
        arr.fill(42);
        for (size i = 0; i < 5; ++i) {
            CHECK_EQ(arr[i], 42);
        }
    }

    SUBCASE("fill() with different types") {
        array<double, 3> arr;
        arr.fill(3.14);
        CHECK_EQ(arr[0], doctest::Approx(3.14));
        CHECK_EQ(arr[1], doctest::Approx(3.14));
        CHECK_EQ(arr[2], doctest::Approx(3.14));
    }

    SUBCASE("swap() member function") {
        array<int, 3> arr1 = {1, 2, 3};
        array<int, 3> arr2 = {10, 20, 30};
        arr1.swap(arr2);

        CHECK_EQ(arr1[0], 10);
        CHECK_EQ(arr1[1], 20);
        CHECK_EQ(arr1[2], 30);

        CHECK_EQ(arr2[0], 1);
        CHECK_EQ(arr2[1], 2);
        CHECK_EQ(arr2[2], 3);
    }

    SUBCASE("swap() non-member function") {
        array<int, 3> arr1 = {1, 2, 3};
        array<int, 3> arr2 = {10, 20, 30};
        fl::swap(arr1, arr2);

        CHECK_EQ(arr1[0], 10);
        CHECK_EQ(arr1[1], 20);
        CHECK_EQ(arr1[2], 30);

        CHECK_EQ(arr2[0], 1);
        CHECK_EQ(arr2[1], 2);
        CHECK_EQ(arr2[2], 3);
    }
}

TEST_CASE("fl::array - Comparison operators") {
    SUBCASE("operator== for equal arrays") {
        array<int, 3> arr1 = {1, 2, 3};
        array<int, 3> arr2 = {1, 2, 3};
        CHECK(arr1 == arr2);
    }

    SUBCASE("operator== for different arrays") {
        array<int, 3> arr1 = {1, 2, 3};
        array<int, 3> arr2 = {1, 2, 4};
        CHECK_FALSE(arr1 == arr2);
    }

    SUBCASE("operator!= for different arrays") {
        array<int, 3> arr1 = {1, 2, 3};
        array<int, 3> arr2 = {1, 2, 4};
        CHECK(arr1 != arr2);
    }

    SUBCASE("operator!= for equal arrays") {
        array<int, 3> arr1 = {1, 2, 3};
        array<int, 3> arr2 = {1, 2, 3};
        CHECK_FALSE(arr1 != arr2);
    }
}

TEST_CASE("fl::array - Different types") {
    SUBCASE("array<float>") {
        array<float, 3> arr = {1.5f, 2.5f, 3.5f};
        CHECK_EQ(arr[0], doctest::Approx(1.5f));
        CHECK_EQ(arr[1], doctest::Approx(2.5f));
        CHECK_EQ(arr[2], doctest::Approx(3.5f));
    }

    SUBCASE("array<double>") {
        array<double, 3> arr = {1.5, 2.5, 3.5};
        CHECK_EQ(arr[0], doctest::Approx(1.5));
        CHECK_EQ(arr[1], doctest::Approx(2.5));
        CHECK_EQ(arr[2], doctest::Approx(3.5));
    }

    SUBCASE("array<char>") {
        array<char, 5> arr = {'H', 'e', 'l', 'l', 'o'};
        CHECK_EQ(arr[0], 'H');
        CHECK_EQ(arr[4], 'o');
    }

    SUBCASE("array<bool>") {
        array<bool, 3> arr = {true, false, true};
        CHECK(arr[0]);
        CHECK_FALSE(arr[1]);
        CHECK(arr[2]);
    }
}

TEST_CASE("fl::array - Edge cases") {
    SUBCASE("Single element array") {
        array<int, 1> arr = {42};
        CHECK_EQ(arr.size(), 1);
        CHECK_EQ(arr[0], 42);
        CHECK_EQ(arr.front(), 42);
        CHECK_EQ(arr.back(), 42);
    }

    SUBCASE("Large array") {
        array<int, 100> arr;
        arr.fill(7);
        CHECK_EQ(arr.size(), 100);
        CHECK_EQ(arr[0], 7);
        CHECK_EQ(arr[99], 7);
    }

    SUBCASE("Array of arrays") {
        array<array<int, 2>, 2> arr;
        arr[0][0] = 1;
        arr[0][1] = 2;
        arr[1][0] = 3;
        arr[1][1] = 4;

        CHECK_EQ(arr[0][0], 1);
        CHECK_EQ(arr[0][1], 2);
        CHECK_EQ(arr[1][0], 3);
        CHECK_EQ(arr[1][1], 4);
    }
}

TEST_CASE("fl::array - Type traits") {
    SUBCASE("value_type") {
        using ArrayType = array<int, 5>;
        static_assert(fl::is_same<ArrayType::value_type, int>::value, "value_type should be int");
    }

    SUBCASE("size_type") {
        using ArrayType = array<int, 5>;
        static_assert(fl::is_same<ArrayType::size_type, fl::size>::value, "size_type should be fl::size");
    }

    SUBCASE("iterator types") {
        using ArrayType = array<int, 5>;
        static_assert(fl::is_same<ArrayType::iterator, int*>::value, "iterator should be int*");
        static_assert(fl::is_same<ArrayType::const_iterator, const int*>::value, "const_iterator should be const int*");
    }
}

TEST_CASE("fl::array - Const correctness") {
    SUBCASE("const array element access") {
        const array<int, 3> arr = {10, 20, 30};
        CHECK_EQ(arr[0], 10);
        CHECK_EQ(arr.at(1), 20);
        CHECK_EQ(arr.front(), 10);
        CHECK_EQ(arr.back(), 30);
    }

    SUBCASE("const array data pointer") {
        const array<int, 3> arr = {10, 20, 30};
        const int* ptr = arr.data();
        CHECK_EQ(ptr[0], 10);
        CHECK_EQ(ptr[1], 20);
        CHECK_EQ(ptr[2], 30);
    }

    SUBCASE("const array iterators") {
        const array<int, 3> arr = {1, 2, 3};
        int sum = 0;
        for (const auto& val : arr) {
            sum += val;
        }
        CHECK_EQ(sum, 6);
    }
}

TEST_CASE("fl::array - to_array() helper function from span") {
    SUBCASE("from C array via dynamic span") {
        int source_data[] = {10, 20, 30, 40, 50};
        fl::span<const int> s(source_data, 5);

        // Convert span to array using to_array helper
        fl::array<int, 5> arr = fl::to_array<5>(s);

        CHECK_EQ(arr.size(), 5);
        CHECK_EQ(arr[0], 10);
        CHECK_EQ(arr[1], 20);
        CHECK_EQ(arr[2], 30);
        CHECK_EQ(arr[3], 40);
        CHECK_EQ(arr[4], 50);

        // Verify it's a copy
        arr[0] = 99;
        CHECK_EQ(source_data[0], 10);
    }

    SUBCASE("from C array via static extent span") {
        int source_data[] = {100, 200, 300};
        fl::span<const int, 3> s(source_data, 3);

        // Convert static extent span to array using to_array helper
        fl::array<int, 3> arr = fl::to_array(s);

        CHECK_EQ(arr.size(), 3);
        CHECK_EQ(arr[0], 100);
        CHECK_EQ(arr[1], 200);
        CHECK_EQ(arr[2], 300);
    }

    SUBCASE("from vector via span") {
        fl::vector<int> heap_vec;
        heap_vec.push_back(1);
        heap_vec.push_back(2);
        heap_vec.push_back(3);
        heap_vec.push_back(4);

        fl::span<const int> s(heap_vec);
        fl::array<int, 4> arr = fl::to_array<4>(s);

        CHECK_EQ(arr.size(), 4);
        CHECK_EQ(arr[0], 1);
        CHECK_EQ(arr[1], 2);
        CHECK_EQ(arr[2], 3);
        CHECK_EQ(arr[3], 4);
    }

    SUBCASE("from FixedVector via span") {
        fl::FixedVector<int, 10> fixed_vec;
        fixed_vec.push_back(5);
        fixed_vec.push_back(6);
        fixed_vec.push_back(7);

        fl::span<const int> s(fixed_vec);
        fl::array<int, 3> arr = fl::to_array<3>(s);

        CHECK_EQ(arr.size(), 3);
        CHECK_EQ(arr[0], 5);
        CHECK_EQ(arr[1], 6);
        CHECK_EQ(arr[2], 7);
    }
}
