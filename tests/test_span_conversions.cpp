#include "test.h"

#include "fl/span.h"
#include "fl/vector.h"
#include "fl/array.h"

using namespace fl;

TEST_CASE("fl::span explicit conversions work correctly") {
    SUBCASE("fl::vector to fl::span conversions") {
        // Test const fl::vector -> fl::span<const T>
        fl::vector<int> vec = {1, 2, 3, 4, 5};
        
        // ✅ These explicit conversions should work
        fl::span<const int> const_span(vec);
        CHECK(const_span.size() == 5);
        CHECK(const_span[0] == 1);
        CHECK(const_span[4] == 5);
        
        // ✅ Mutable conversion should work
        fl::span<int> mutable_span(vec);
        CHECK(mutable_span.size() == 5);
        mutable_span[0] = 10;
        CHECK(vec[0] == 10);  // Verify it's a view, not a copy
    }
    
    SUBCASE("fl::array to fl::span conversions") {
        fl::array<int, 4> arr = {10, 20, 30, 40};
        
        // ✅ These explicit conversions should work
        fl::span<const int> const_span(arr);
        CHECK(const_span.size() == 4);
        CHECK(const_span[0] == 10);
        CHECK(const_span[3] == 40);
        
        // ✅ Mutable conversion should work
        fl::span<int> mutable_span(arr);
        CHECK(mutable_span.size() == 4);
        mutable_span[0] = 100;
        CHECK(arr[0] == 100);  // Verify it's a view
    }
    
    SUBCASE("C-style array to fl::span conversions") {
        int c_array[] = {5, 10, 15, 20};
        
        // ✅ These explicit conversions should work
        fl::span<const int> const_span(c_array);
        CHECK(const_span.size() == 4);
        CHECK(const_span[0] == 5);
        CHECK(const_span[3] == 20);
        
        // ✅ Mutable conversion should work
        fl::span<int> mutable_span(c_array);
        CHECK(mutable_span.size() == 4);
        mutable_span[0] = 50;
        CHECK(c_array[0] == 50);  // Verify it's a view
    }
    
    SUBCASE("const array to const span") {
        const int const_array[] = {100, 200, 300};
        
        // ✅ Const array to const span should work
        fl::span<const int> const_span(const_array);
        CHECK(const_span.size() == 3);
        CHECK(const_span[0] == 100);
        CHECK(const_span[2] == 300);
    }
}

TEST_CASE("fl::span non-template function conversions work") {
    // These tests show that non-template functions CAN accept containers
    // via implicit conversion through our constructors
    
    auto process_const_span = [](fl::span<const int> data) -> int {
        int sum = 0;
        for (const auto& item : data) {
            sum += item;
        }
        return sum;
    };
    
    auto modify_span = [](fl::span<int> data) {
        for (auto& item : data) {
            item += 1;
        }
    };
    
    SUBCASE("fl::vector implicit conversion to non-template function") {
        fl::vector<int> vec = {1, 2, 3, 4, 5};
        
        // ✅ This should work - implicit conversion to function parameter
        int result = process_const_span(vec);
        CHECK(result == 15);
        
        // ✅ Mutable function should work too
        modify_span(vec);
        CHECK(vec[0] == 2);
        CHECK(vec[4] == 6);
    }
    
    SUBCASE("fl::array implicit conversion to non-template function") {
        fl::array<int, 3> arr = {10, 20, 30};
        
        // ✅ This should work
        int result = process_const_span(arr);
        CHECK(result == 60);
        
        // ✅ Mutable function should work
        modify_span(arr);
        CHECK(arr[0] == 11);
        CHECK(arr[2] == 31);
    }
    
    SUBCASE("C-style array implicit conversion to non-template function") {
        int c_array[] = {7, 14, 21};
        
        // ✅ This should work
        int result = process_const_span(c_array);
        CHECK(result == 42);
        
        // ✅ Mutable function should work
        modify_span(c_array);
        CHECK(c_array[0] == 8);
        CHECK(c_array[2] == 22);
    }
}

TEST_CASE("fl::span limitations - template argument deduction") {
    // This test documents what DOESN'T work due to C++ language limitations
    
    SUBCASE("template functions cannot deduce from implicit conversions") {
        fl::vector<int> vec = {1, 2, 3};
        
        // ❌ This would NOT work (commented out to avoid compilation errors):
        // template<typename T> void template_func(fl::span<T> data) { ... }
        // template_func(vec);  // Error: template argument deduction fails
        
        // ✅ This DOES work with explicit template parameters:
        auto template_func = [](fl::span<int> data) -> int {
            int sum = 0;
            for (const auto& item : data) {
                sum += static_cast<int>(item);
            }
            return sum;
        };
        
        int result = template_func(fl::span<int>(vec));  // Explicit conversion
        CHECK(result == 6);
        
        // 📝 This is correct C++ behavior - template argument deduction only 
        // considers exact type matches, not constructor conversions
    }
} 
