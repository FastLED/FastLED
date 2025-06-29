// g++ --std=c++11 test.cpp

#include "test.h"

#include "fl/slice.h"
#include "fl/span.h"
#include "fl/unused.h"
#include "fl/vector.h"
#include "fl/array.h"
#include "fl/string.h"
#include "test.h"

using namespace fl;

TEST_CASE("vector slice") {
    HeapVector<int> vec;

    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);
    vec.push_back(4);

    Slice<int> slice(vec.data(), vec.size());

    REQUIRE_EQ(slice.length(), 4);
    REQUIRE_EQ(slice[0], 1);
    REQUIRE_EQ(slice[1], 2);
    REQUIRE_EQ(slice[2], 3);
    REQUIRE_EQ(slice[3], 4);

    Slice<int> slice2 = slice.slice(1, 3);
    REQUIRE_EQ(slice2.length(), 2);
    REQUIRE_EQ(slice2[0], 2);
    REQUIRE_EQ(slice2[1], 3);
}

// === NEW COMPREHENSIVE fl::span<T> TESTS ===

TEST_CASE("fl::span<T> alias functionality") {
    SUBCASE("span is alias for Slice") {
        fl::vector<int> vec;
        vec.push_back(10);
        vec.push_back(20);
        vec.push_back(30);
        
        // Test that span<T> and Slice<T> are interchangeable
        fl::span<int> span1(vec);
        fl::Slice<int> slice1(vec);
        
        REQUIRE_EQ(span1.size(), slice1.size());
        REQUIRE_EQ(span1.data(), slice1.data());
        REQUIRE_EQ(span1[0], slice1[0]);
        REQUIRE_EQ(span1[1], slice1[1]);
        REQUIRE_EQ(span1[2], slice1[2]);
    }
}

TEST_CASE("fl::span container constructors") {
    SUBCASE("fl::vector construction") {
        fl::vector<int> vec;
        vec.push_back(1);
        vec.push_back(2);
        vec.push_back(3);
        
        fl::span<int> span(vec);
        
        REQUIRE_EQ(span.size(), 3);
        REQUIRE_EQ(span[0], 1);
        REQUIRE_EQ(span[1], 2);
        REQUIRE_EQ(span[2], 3);
        REQUIRE_EQ(span.data(), vec.data());
    }
    
    SUBCASE("const fl::vector construction") {
        fl::vector<int> vec;
        vec.push_back(10);
        vec.push_back(20);
        const fl::vector<int>& const_vec = vec;
        
        fl::span<const int> span(const_vec);
        
        REQUIRE_EQ(span.size(), 2);
        REQUIRE_EQ(span[0], 10);
        REQUIRE_EQ(span[1], 20);
    }
    
    SUBCASE("fl::array construction") {
        fl::array<int, 4> arr = {1, 2, 3, 4};
        
        fl::span<int> span(arr);
        
        REQUIRE_EQ(span.size(), 4);
        REQUIRE_EQ(span[0], 1);
        REQUIRE_EQ(span[1], 2);
        REQUIRE_EQ(span[2], 3);
        REQUIRE_EQ(span[3], 4);
        REQUIRE_EQ(span.data(), arr.data());
    }
    
    SUBCASE("const fl::array construction") {
        const fl::array<int, 3> arr = {5, 6, 7};
        
        fl::span<const int> span(arr);
        
        REQUIRE_EQ(span.size(), 3);
        REQUIRE_EQ(span[0], 5);
        REQUIRE_EQ(span[1], 6);
        REQUIRE_EQ(span[2], 7);
    }
}

TEST_CASE("fl::span C-style array constructors") {
    SUBCASE("non-const C-style array") {
        int arr[] = {1, 2, 3, 4, 5};
        
        fl::span<int> span(arr);
        
        REQUIRE_EQ(span.size(), 5);
        REQUIRE_EQ(span[0], 1);
        REQUIRE_EQ(span[4], 5);
        
        // Test modification through span
        span[0] = 10;
        REQUIRE_EQ(arr[0], 10);
    }
    
    SUBCASE("const C-style array") {
        const int arr[] = {10, 20, 30};
        
        fl::span<const int> span(arr);
        
        REQUIRE_EQ(span.size(), 3);
        REQUIRE_EQ(span[0], 10);
        REQUIRE_EQ(span[1], 20);
        REQUIRE_EQ(span[2], 30);
    }
}

TEST_CASE("fl::span iterator constructors") {
    SUBCASE("iterator construction from vector") {
        fl::vector<int> vec;
        vec.push_back(100);
        vec.push_back(200);
        vec.push_back(300);
        
        fl::span<int> span(vec.begin(), vec.end());
        
        REQUIRE_EQ(span.size(), 3);
        REQUIRE_EQ(span[0], 100);
        REQUIRE_EQ(span[1], 200);
        REQUIRE_EQ(span[2], 300);
    }
    
    SUBCASE("iterator construction from C array") {
        int arr[] = {1, 2, 3, 4};
        
        fl::span<int> span(arr, arr + 4);
        
        REQUIRE_EQ(span.size(), 4);
        REQUIRE_EQ(span[0], 1);
        REQUIRE_EQ(span[3], 4);
    }
}

TEST_CASE("fl::span const conversions") {
    SUBCASE("automatic promotion to const span") {
        fl::vector<int> vec;
        vec.push_back(1);
        vec.push_back(2);
        
        fl::span<int> mutable_span(vec);
        fl::span<const int> const_span = mutable_span; // automatic conversion
        
        REQUIRE_EQ(const_span.size(), 2);
        REQUIRE_EQ(const_span[0], 1);
        REQUIRE_EQ(const_span[1], 2);
        REQUIRE_EQ(const_span.data(), mutable_span.data());
    }
    
    SUBCASE("function accepting const span") {
        auto test_func = [](fl::span<const int> span) -> int {
            return span.size();
        };
        
        fl::vector<int> vec;
        vec.push_back(1);
        vec.push_back(2);
        vec.push_back(3);
        
        fl::span<int> mutable_span(vec);
        
        // Should automatically convert to const span
        int result = test_func(mutable_span);
        REQUIRE_EQ(result, 3);
    }
}

TEST_CASE("fl::span copy and assignment") {
    SUBCASE("copy constructor") {
        fl::vector<int> vec;
        vec.push_back(10);
        vec.push_back(20);
        
        fl::span<int> span1(vec);
        fl::span<int> span2(span1);
        
        REQUIRE_EQ(span2.size(), span1.size());
        REQUIRE_EQ(span2.data(), span1.data());
        REQUIRE_EQ(span2[0], 10);
        REQUIRE_EQ(span2[1], 20);
    }
    
    SUBCASE("assignment operator") {
        fl::vector<int> vec1;
        vec1.push_back(1);
        vec1.push_back(2);
        
        fl::vector<int> vec2;
        vec2.push_back(3);
        vec2.push_back(4);
        vec2.push_back(5);
        
        fl::span<int> span1(vec1);
        fl::span<int> span2(vec2);
        
        span1 = span2;
        
        REQUIRE_EQ(span1.size(), 3);
        REQUIRE_EQ(span1.data(), vec2.data());
        REQUIRE_EQ(span1[0], 3);
        REQUIRE_EQ(span1[1], 4);
        REQUIRE_EQ(span1[2], 5);
    }
}

TEST_CASE("fl::span basic operations") {
    fl::vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);
    vec.push_back(4);
    vec.push_back(5);
    
    fl::span<int> span(vec);
    
    SUBCASE("size and length") {
        REQUIRE_EQ(span.size(), 5);
        REQUIRE_EQ(span.length(), 5);
    }
    
    SUBCASE("empty check") {
        CHECK_FALSE(span.empty());
        
        fl::span<int> empty_span;
        CHECK(empty_span.empty());
    }
    
    SUBCASE("data access") {
        REQUIRE_EQ(span.data(), vec.data());
        
        const fl::span<int>& const_span = span;
        REQUIRE_EQ(const_span.data(), vec.data());
    }
    
    SUBCASE("front and back") {
        REQUIRE_EQ(span.front(), 1);
        REQUIRE_EQ(span.back(), 5);
        
        const fl::span<int>& const_span = span;
        REQUIRE_EQ(const_span.front(), 1);
        REQUIRE_EQ(const_span.back(), 5);
    }
    
    SUBCASE("iterator access") {
        REQUIRE_EQ(*span.begin(), 1);
        REQUIRE_EQ(*(span.end() - 1), 5);
        REQUIRE_EQ(span.end() - span.begin(), 5);
    }
}

TEST_CASE("fl::span slicing operations") {
    fl::vector<int> vec;
    for (int i = 0; i < 10; ++i) {
        vec.push_back(i);
    }
    
    fl::span<int> span(vec);
    
    SUBCASE("slice with start and end") {
        auto sub_span = span.slice(2, 6);
        
        REQUIRE_EQ(sub_span.size(), 4);
        REQUIRE_EQ(sub_span[0], 2);
        REQUIRE_EQ(sub_span[1], 3);
        REQUIRE_EQ(sub_span[2], 4);
        REQUIRE_EQ(sub_span[3], 5);
    }
    
    SUBCASE("slice with start only") {
        auto sub_span = span.slice(7);
        
        REQUIRE_EQ(sub_span.size(), 3);
        REQUIRE_EQ(sub_span[0], 7);
        REQUIRE_EQ(sub_span[1], 8);
        REQUIRE_EQ(sub_span[2], 9);
    }
    
    SUBCASE("empty slice") {
        auto empty_slice = span.slice(5, 5);
        REQUIRE_EQ(empty_slice.size(), 0);
        CHECK(empty_slice.empty());
    }
}

TEST_CASE("fl::span find operation") {
    fl::vector<int> vec;
    vec.push_back(10);
    vec.push_back(20);
    vec.push_back(30);
    vec.push_back(20);
    vec.push_back(40);
    
    fl::span<int> span(vec);
    
    SUBCASE("find existing element") {
        REQUIRE_EQ(span.find(20), 1); // First occurrence
        REQUIRE_EQ(span.find(30), 2);
        REQUIRE_EQ(span.find(40), 4);
    }
    
    SUBCASE("find non-existing element") {
        REQUIRE_EQ(span.find(99), size_t(-1));
        REQUIRE_EQ(span.find(0), size_t(-1));
    }
    
    SUBCASE("find in empty span") {
        fl::span<int> empty_span;
        REQUIRE_EQ(empty_span.find(10), size_t(-1));
    }
}

TEST_CASE("fl::span pop operations") {
    fl::vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);
    vec.push_back(4);
    
    fl::span<int> span(vec);
    
    SUBCASE("pop_front") {
        REQUIRE_EQ(span.size(), 4);
        REQUIRE_EQ(span.front(), 1);
        
        bool result = span.pop_front();
        CHECK(result);
        REQUIRE_EQ(span.size(), 3);
        REQUIRE_EQ(span.front(), 2);
        
        result = span.pop_front();
        CHECK(result);
        REQUIRE_EQ(span.size(), 2);
        REQUIRE_EQ(span.front(), 3);
    }
    
    SUBCASE("pop_back") {
        REQUIRE_EQ(span.size(), 4);
        REQUIRE_EQ(span.back(), 4);
        
        bool result = span.pop_back();
        CHECK(result);
        REQUIRE_EQ(span.size(), 3);
        REQUIRE_EQ(span.back(), 3);
        
        result = span.pop_back();
        CHECK(result);
        REQUIRE_EQ(span.size(), 2);
        REQUIRE_EQ(span.back(), 2);
    }
    
    SUBCASE("pop from empty span") {
        fl::span<int> empty_span;
        
        bool result = empty_span.pop_front();
        CHECK_FALSE(result);
        
        result = empty_span.pop_back();
        CHECK_FALSE(result);
    }
    
    SUBCASE("pop until empty") {
        while (span.size() > 0) {
            bool result = span.pop_front();
            CHECK(result);
        }
        
        CHECK(span.empty());
        bool result = span.pop_front();
        CHECK_FALSE(result);
    }
}

TEST_CASE("fl::span with different types") {
    SUBCASE("string span") {
        fl::vector<fl::string> string_vec;
        string_vec.push_back(fl::string("hello"));
        string_vec.push_back(fl::string("world"));
        string_vec.push_back(fl::string("test"));
        
        fl::span<fl::string> span(string_vec);
        
        REQUIRE_EQ(span.size(), 3);
        REQUIRE_EQ(span[0], "hello");
        REQUIRE_EQ(span[1], "world");
        REQUIRE_EQ(span[2], "test");
        
        // Test find with strings
        REQUIRE_EQ(span.find(fl::string("world")), 1);
        REQUIRE_EQ(span.find(fl::string("notfound")), size_t(-1));
    }
    
    SUBCASE("const char* span") {
        const char* arr[] = {"apple", "banana", "cherry"};
        
        fl::span<const char*> span(arr);
        
        REQUIRE_EQ(span.size(), 3);
        REQUIRE_EQ(fl::string(span[0]), "apple");
        REQUIRE_EQ(fl::string(span[1]), "banana");
        REQUIRE_EQ(fl::string(span[2]), "cherry");
    }
    
    SUBCASE("byte span") {
        uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0xFF};
        
        fl::span<uint8_t> span(data);
        
        REQUIRE_EQ(span.size(), 5);
        REQUIRE_EQ(span[0], 0x01);
        REQUIRE_EQ(span[4], 0xFF);
        
        // Test slicing with bytes
        auto sub_span = span.slice(1, 4);
        REQUIRE_EQ(sub_span.size(), 3);
        REQUIRE_EQ(sub_span[0], 0x02);
        REQUIRE_EQ(sub_span[2], 0x04);
    }
}

TEST_CASE("fl::span edge cases") {
    SUBCASE("empty span operations") {
        fl::span<int> empty_span;
        
        CHECK(empty_span.empty());
        REQUIRE_EQ(empty_span.size(), 0);
        REQUIRE_EQ(empty_span.length(), 0);
        REQUIRE_EQ(empty_span.begin(), empty_span.end());
        REQUIRE_EQ(empty_span.find(1), size_t(-1));
        CHECK_FALSE(empty_span.pop_front());
        CHECK_FALSE(empty_span.pop_back());
        
        // Empty slice operations
        auto sub_span = empty_span.slice(0, 0);
        CHECK(sub_span.empty());
        
        auto sub_span2 = empty_span.slice(0);
        CHECK(sub_span2.empty());
    }
    
    SUBCASE("single element span") {
        int single = 42;
        fl::span<int> span(&single, 1);
        
        REQUIRE_EQ(span.size(), 1);
        CHECK_FALSE(span.empty());
        REQUIRE_EQ(span[0], 42);
        REQUIRE_EQ(span.front(), 42);
        REQUIRE_EQ(span.back(), 42);
        REQUIRE_EQ(span.find(42), 0);
        REQUIRE_EQ(span.find(1), size_t(-1));
        
        // Pop operations on single element
        bool result = span.pop_front();
        CHECK(result);
        CHECK(span.empty());
    }
    
    SUBCASE("type conversion scenarios") {
        int arr[] = {1, 2, 3};
        
        // Non-const to const conversion
        fl::span<int> mutable_span(arr);
        fl::span<const int> const_span = mutable_span;
        
        REQUIRE_EQ(mutable_span.size(), const_span.size());
        REQUIRE_EQ(mutable_span.data(), const_span.data());
        
        // Verify both refer to same data
        mutable_span[0] = 10;
        REQUIRE_EQ(const_span[0], 10);
    }
}

TEST_CASE("fl::span parameter usage patterns") {
    SUBCASE("function accepting span parameter") {
        auto sum_func = [](fl::span<const int> numbers) -> int {
            int sum = 0;
            for (size_t i = 0; i < numbers.size(); ++i) {
                sum += numbers[i];
            }
            return sum;
        };
        
        // Test with vector
        fl::vector<int> vec;
        vec.push_back(1);
        vec.push_back(2);
        vec.push_back(3);
        REQUIRE_EQ(sum_func(vec), 6);
        
        // Test with array
        fl::array<int, 3> arr = {4, 5, 6};
        REQUIRE_EQ(sum_func(arr), 15);
        
        // Test with C-style array
        int c_arr[] = {7, 8, 9};
        REQUIRE_EQ(sum_func(c_arr), 24);
    }
    
    SUBCASE("function returning span") {
        auto get_middle = [](fl::span<int> data) -> fl::span<int> {
            if (data.size() <= 2) {
                return fl::span<int>();
            }
            return data.slice(1, data.size() - 1);
        };
        
        fl::vector<int> vec;
        for (int i = 0; i < 5; ++i) {
            vec.push_back(i);
        }
        
        auto middle = get_middle(vec);
        REQUIRE_EQ(middle.size(), 3);
        REQUIRE_EQ(middle[0], 1);
        REQUIRE_EQ(middle[1], 2);
        REQUIRE_EQ(middle[2], 3);
    }
}

// === EXISTING MATRIX TESTS ===

TEST_CASE("matrix compile") {
    int data[2][2] = {{1, 2}, {3, 4}};

    // Window from (0,0) up to (1,1)
    MatrixSlice<int> slice(&data[0][0], // data pointer
                           2,           // data width
                           2,           // data height
                           0, 0,        // bottom-left x,y
                           1, 1         // top-right x,y
    );

    FASTLED_UNUSED(slice);
    // just a compile‐time smoke test
}

TEST_CASE("matrix slice returns correct values") {
    int data[2][2] = {{1, 2}, {3, 4}};

    // Window from (0,0) up to (1,1)
    MatrixSlice<int> slice(&data[0][0], // data pointer
                           2,           // data width
                           2,           // data height
                           0, 0,        // bottom-left x,y
                           1, 1         // top-right x,y
    );

    // sanity‐check each element
    REQUIRE_EQ(slice(0, 0), data[0][0]);
    REQUIRE_EQ(slice(1, 0), data[0][1]);
    REQUIRE_EQ(slice(0, 1), data[1][0]);
    REQUIRE_EQ(slice(1, 1), data[1][1]);

    // Require that the [][] operator works the same as the data
    REQUIRE_EQ(slice[0][0], data[0][0]);
    REQUIRE_EQ(slice[0][1], data[0][1]);
    REQUIRE_EQ(slice[1][0], data[1][0]);
    REQUIRE_EQ(slice[1][1], data[1][1]);
}

TEST_CASE("4x4 matrix slice returns correct values") {
    int data[4][4] = {
        {1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16}};

    // Take a 2×2 window from (1,1) up to (2,2)
    MatrixSlice<int> slice(&data[0][0], // data pointer
                           4,           // data width
                           4,           // data height
                           1, 1,        // bottom-left x,y
                           2, 2         // top-right x,y
    );

    // test array access
    REQUIRE_EQ(slice[0][0], data[1][1]);
    REQUIRE_EQ(slice[0][1], data[1][2]);
    REQUIRE_EQ(slice[1][0], data[2][1]);
    REQUIRE_EQ(slice[1][1], data[2][2]);

    // Remember that array access is row-major, so data[y][x] == slice(x,y)
    REQUIRE_EQ(slice(0, 0), data[1][1]);
    REQUIRE_EQ(slice(1, 0), data[1][2]);
    REQUIRE_EQ(slice(0, 1), data[2][1]);
    REQUIRE_EQ(slice(1, 1), data[2][2]);
}
