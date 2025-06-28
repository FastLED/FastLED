// g++ --std=c++11 test.cpp

#include "test.h"

#include "fl/algorithm.h"
#include "fl/dbg.h"
#include "fl/vector.h"
#include "fl/functional.h"
#include "fl/map.h"
#include "fl/random.h"
#include "test.h"

using namespace fl;

TEST_CASE("reverse an int list") {
    fl::vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);
    vec.push_back(4);
    vec.push_back(5);

    fl::reverse(vec.begin(), vec.end());
    CHECK_EQ(vec[0], 5);
    CHECK_EQ(vec[1], 4);
    CHECK_EQ(vec[2], 3);
    CHECK_EQ(vec[3], 2);
    CHECK_EQ(vec[4], 1);
}

// This test should NOT compile - trying to sort a map should fail
// because it would break the tree's internal ordering invariant
#ifdef COMPILE_ERROR_TEST_MAP_SORT
TEST_CASE("fl::sort on fl::fl_map should fail to compile") {
    fl::fl_map<int, fl::string> map;
    map[3] = "three";
    map[1] = "one";
    map[2] = "two";

    // This CORRECTLY produces a compilation error!
    // ERROR: no match for 'operator+' on map iterators
    // 
    // fl::fl_map iterators are bidirectional (not random access)
    // fl::sort requires random access iterators for:
    //   - Iterator arithmetic: first + 1, last - first
    //   - Efficient pivot selection and partitioning
    //
    // This prevents users from accidentally corrupting the tree structure
    // by calling sort on a map, which would break ordering invariants.
    fl::sort(map.begin(), map.end());
}
#endif

TEST_CASE("fl::sort with default comparator") {
    SUBCASE("Sort integers") {
        fl::vector<int> vec;
        vec.push_back(5);
        vec.push_back(2);
        vec.push_back(8);
        vec.push_back(1);
        vec.push_back(9);
        vec.push_back(3);

        fl::sort(vec.begin(), vec.end());

        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 2);
        CHECK_EQ(vec[2], 3);
        CHECK_EQ(vec[3], 5);
        CHECK_EQ(vec[4], 8);
        CHECK_EQ(vec[5], 9);
    }

    SUBCASE("Sort empty container") {
        fl::vector<int> vec;
        fl::sort(vec.begin(), vec.end());
        CHECK_EQ(vec.size(), 0);
    }

    SUBCASE("Sort single element") {
        fl::vector<int> vec;
        vec.push_back(42);
        fl::sort(vec.begin(), vec.end());
        CHECK_EQ(vec.size(), 1);
        CHECK_EQ(vec[0], 42);
    }

    SUBCASE("Sort two elements") {
        fl::vector<int> vec;
        vec.push_back(3);
        vec.push_back(1);
        fl::sort(vec.begin(), vec.end());
        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 3);
    }

    SUBCASE("Sort already sorted array") {
        fl::vector<int> vec;
        vec.push_back(1);
        vec.push_back(2);
        vec.push_back(3);
        vec.push_back(4);
        vec.push_back(5);

        fl::sort(vec.begin(), vec.end());

        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 2);
        CHECK_EQ(vec[2], 3);
        CHECK_EQ(vec[3], 4);
        CHECK_EQ(vec[4], 5);
    }

    SUBCASE("Sort reverse sorted array") {
        fl::vector<int> vec;
        vec.push_back(5);
        vec.push_back(4);
        vec.push_back(3);
        vec.push_back(2);
        vec.push_back(1);

        fl::sort(vec.begin(), vec.end());

        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 2);
        CHECK_EQ(vec[2], 3);
        CHECK_EQ(vec[3], 4);
        CHECK_EQ(vec[4], 5);
    }

    SUBCASE("Sort with duplicates") {
        fl::vector<int> vec;
        vec.push_back(3);
        vec.push_back(1);
        vec.push_back(4);
        vec.push_back(1);
        vec.push_back(5);
        vec.push_back(3);
        vec.push_back(1);

        fl::sort(vec.begin(), vec.end());

        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 1);
        CHECK_EQ(vec[2], 1);
        CHECK_EQ(vec[3], 3);
        CHECK_EQ(vec[4], 3);
        CHECK_EQ(vec[5], 4);
        CHECK_EQ(vec[6], 5);
    }
}

TEST_CASE("fl::sort with custom comparator") {
    SUBCASE("Sort integers in descending order") {
        fl::vector<int> vec;
        vec.push_back(5);
        vec.push_back(2);
        vec.push_back(8);
        vec.push_back(1);
        vec.push_back(9);
        vec.push_back(3);

        fl::sort(vec.begin(), vec.end(), [](int a, int b) { return a > b; });

        CHECK_EQ(vec[0], 9);
        CHECK_EQ(vec[1], 8);
        CHECK_EQ(vec[2], 5);
        CHECK_EQ(vec[3], 3);
        CHECK_EQ(vec[4], 2);
        CHECK_EQ(vec[5], 1);
    }

    SUBCASE("Sort using fl::less") {
        fl::vector<int> vec;
        vec.push_back(5);
        vec.push_back(2);
        vec.push_back(8);
        vec.push_back(1);

        fl::sort(vec.begin(), vec.end(), fl::less<int>());

        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 2);
        CHECK_EQ(vec[2], 5);
        CHECK_EQ(vec[3], 8);
    }

    SUBCASE("Sort custom struct by member") {
        struct Point {
            int x, y;
            Point() : x(0), y(0) {}  // Add default constructor
            Point(int x, int y) : x(x), y(y) {}
        };

        fl::vector<Point> vec;
        vec.push_back(Point(3, 1));
        vec.push_back(Point(1, 3));
        vec.push_back(Point(2, 2));

        // Sort by x coordinate
        fl::sort(vec.begin(), vec.end(), [](const Point& a, const Point& b) {
            return a.x < b.x;
        });

        CHECK_EQ(vec[0].x, 1);
        CHECK_EQ(vec[1].x, 2);
        CHECK_EQ(vec[2].x, 3);
    }
}

TEST_CASE("fl::sort with different container types") {
    SUBCASE("Sort FixedVector") {
        fl::FixedVector<int, 6> vec;
        vec.push_back(5);
        vec.push_back(2);
        vec.push_back(8);
        vec.push_back(1);
        vec.push_back(9);
        vec.push_back(3);

        fl::sort(vec.begin(), vec.end());

        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 2);
        CHECK_EQ(vec[2], 3);
        CHECK_EQ(vec[3], 5);
        CHECK_EQ(vec[4], 8);
        CHECK_EQ(vec[5], 9);
    }

    SUBCASE("Sort HeapVector") {
        fl::HeapVector<int> vec;
        vec.push_back(5);
        vec.push_back(2);
        vec.push_back(8);
        vec.push_back(1);

        fl::sort(vec.begin(), vec.end());

        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 2);
        CHECK_EQ(vec[2], 5);
        CHECK_EQ(vec[3], 8);
    }
}

TEST_CASE("fl::sort stability and performance") {
    SUBCASE("Large array sort") {
        fl::vector<int> vec;
        // Create a larger array for testing
        for (int i = 100; i >= 1; --i) {
            vec.push_back(i);
        }

        fl::sort(vec.begin(), vec.end());

        // Verify all elements are in order
        for (size_t i = 0; i < vec.size(); ++i) {
            CHECK_EQ(vec[i], static_cast<int>(i + 1));
        }
    }

    SUBCASE("Partial sort") {
        fl::vector<int> vec;
        vec.push_back(5);
        vec.push_back(2);
        vec.push_back(8);
        vec.push_back(1);
        vec.push_back(9);
        vec.push_back(3);

        // Sort only first 3 elements
        fl::sort(vec.begin(), vec.begin() + 3);

        CHECK_EQ(vec[0], 2);
        CHECK_EQ(vec[1], 5);
        CHECK_EQ(vec[2], 8);
        // Rest should be unchanged
        CHECK_EQ(vec[3], 1);
        CHECK_EQ(vec[4], 9);
        CHECK_EQ(vec[5], 3);
    }
}

TEST_CASE("fl::sort edge cases") {
    SUBCASE("All elements equal") {
        fl::vector<int> vec;
        vec.push_back(5);
        vec.push_back(5);
        vec.push_back(5);
        vec.push_back(5);
        vec.push_back(5);

        fl::sort(vec.begin(), vec.end());

        for (size_t i = 0; i < vec.size(); ++i) {
            CHECK_EQ(vec[i], 5);
        }
    }

    SUBCASE("Mixed positive and negative") {
        fl::vector<int> vec;
        vec.push_back(-5);
        vec.push_back(2);
        vec.push_back(-8);
        vec.push_back(1);
        vec.push_back(0);
        vec.push_back(-1);

        fl::sort(vec.begin(), vec.end());

        CHECK_EQ(vec[0], -8);
        CHECK_EQ(vec[1], -5);
        CHECK_EQ(vec[2], -1);
        CHECK_EQ(vec[3], 0);
        CHECK_EQ(vec[4], 1);
        CHECK_EQ(vec[5], 2);
    }
}

TEST_CASE("fl::stable_sort with default comparator") {
    SUBCASE("Sort integers") {
        fl::vector<int> vec;
        vec.push_back(5);
        vec.push_back(2);
        vec.push_back(8);
        vec.push_back(1);
        vec.push_back(9);
        vec.push_back(3);

        fl::stable_sort(vec.begin(), vec.end());

        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 2);
        CHECK_EQ(vec[2], 3);
        CHECK_EQ(vec[3], 5);
        CHECK_EQ(vec[4], 8);
        CHECK_EQ(vec[5], 9);
    }

    SUBCASE("Sort empty container") {
        fl::vector<int> vec;
        fl::stable_sort(vec.begin(), vec.end());
        CHECK_EQ(vec.size(), 0);
    }

    SUBCASE("Sort single element") {
        fl::vector<int> vec;
        vec.push_back(42);
        fl::stable_sort(vec.begin(), vec.end());
        CHECK_EQ(vec.size(), 1);
        CHECK_EQ(vec[0], 42);
    }

    SUBCASE("Sort two elements") {
        fl::vector<int> vec;
        vec.push_back(3);
        vec.push_back(1);
        fl::stable_sort(vec.begin(), vec.end());
        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 3);
    }

    SUBCASE("Sort already sorted array") {
        fl::vector<int> vec;
        vec.push_back(1);
        vec.push_back(2);
        vec.push_back(3);
        vec.push_back(4);
        vec.push_back(5);

        fl::stable_sort(vec.begin(), vec.end());

        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 2);
        CHECK_EQ(vec[2], 3);
        CHECK_EQ(vec[3], 4);
        CHECK_EQ(vec[4], 5);
    }

    SUBCASE("Sort with duplicates") {
        fl::vector<int> vec;
        vec.push_back(3);
        vec.push_back(1);
        vec.push_back(4);
        vec.push_back(1);
        vec.push_back(5);
        vec.push_back(3);
        vec.push_back(1);

        fl::stable_sort(vec.begin(), vec.end());

        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 1);
        CHECK_EQ(vec[2], 1);
        CHECK_EQ(vec[3], 3);
        CHECK_EQ(vec[4], 3);
        CHECK_EQ(vec[5], 4);
        CHECK_EQ(vec[6], 5);
    }
}

TEST_CASE("fl::stable_sort stability test") {
    // Test that stable_sort maintains relative order of equal elements
    struct NumberWithIndex {
        int value;
        int original_index;
        
        NumberWithIndex() : value(0), original_index(0) {}
        NumberWithIndex(int v, int idx) : value(v), original_index(idx) {}
    };

    SUBCASE("Maintain order of equal elements") {
        fl::vector<NumberWithIndex> vec;
        vec.push_back(NumberWithIndex(3, 0));
        vec.push_back(NumberWithIndex(1, 1));
        vec.push_back(NumberWithIndex(3, 2));  // Same value as index 0
        vec.push_back(NumberWithIndex(1, 3));  // Same value as index 1
        vec.push_back(NumberWithIndex(2, 4));
        vec.push_back(NumberWithIndex(3, 5));  // Same value as indices 0 and 2

        // Sort by value only (ignore original_index in comparison)
        fl::stable_sort(vec.begin(), vec.end(), 
                       [](const NumberWithIndex& a, const NumberWithIndex& b) {
                           return a.value < b.value;
                       });

        // Verify sorted by value
        CHECK_EQ(vec[0].value, 1);
        CHECK_EQ(vec[1].value, 1);
        CHECK_EQ(vec[2].value, 2);
        CHECK_EQ(vec[3].value, 3);
        CHECK_EQ(vec[4].value, 3);
        CHECK_EQ(vec[5].value, 3);

        // Verify stability: equal elements maintain their relative original order
        // First 1 should be from original index 1, second 1 from original index 3
        CHECK_EQ(vec[0].original_index, 1);
        CHECK_EQ(vec[1].original_index, 3);
        
        // The 3's should be in order: original indices 0, 2, 5
        CHECK_EQ(vec[3].original_index, 0);
        CHECK_EQ(vec[4].original_index, 2);
        CHECK_EQ(vec[5].original_index, 5);
    }
    
    SUBCASE("Large stability test") {
        fl::vector<NumberWithIndex> vec;
        
        // Create pattern: values 1,2,3,1,2,3,1,2,3...
        for (int i = 0; i < 30; ++i) {
            vec.push_back(NumberWithIndex((i % 3) + 1, i));
        }
        
        fl::stable_sort(vec.begin(), vec.end(),
                       [](const NumberWithIndex& a, const NumberWithIndex& b) {
                           return a.value < b.value;
                       });
        
        // Verify all 1's come first, then 2's, then 3's
        for (int i = 0; i < 10; ++i) {
            CHECK_EQ(vec[i].value, 1);
        }
        for (int i = 10; i < 20; ++i) {
            CHECK_EQ(vec[i].value, 2);
        }
        for (int i = 20; i < 30; ++i) {
            CHECK_EQ(vec[i].value, 3);
        }
        
        // Verify stability within each group
        // 1's should have original indices: 0, 3, 6, 9, 12, 15, 18, 21, 24, 27
        int expected_1_indices[] = {0, 3, 6, 9, 12, 15, 18, 21, 24, 27};
        for (int i = 0; i < 10; ++i) {
            CHECK_EQ(vec[i].original_index, expected_1_indices[i]);
        }
        
        // 2's should have original indices: 1, 4, 7, 10, 13, 16, 19, 22, 25, 28
        int expected_2_indices[] = {1, 4, 7, 10, 13, 16, 19, 22, 25, 28};
        for (int i = 0; i < 10; ++i) {
            CHECK_EQ(vec[i + 10].original_index, expected_2_indices[i]);
        }
        
        // 3's should have original indices: 2, 5, 8, 11, 14, 17, 20, 23, 26, 29
        int expected_3_indices[] = {2, 5, 8, 11, 14, 17, 20, 23, 26, 29};
        for (int i = 0; i < 10; ++i) {
            CHECK_EQ(vec[i + 20].original_index, expected_3_indices[i]);
        }
    }
}

TEST_CASE("fl::stable_sort with custom comparator") {
    SUBCASE("Sort integers in descending order") {
        fl::vector<int> vec;
        vec.push_back(5);
        vec.push_back(2);
        vec.push_back(8);
        vec.push_back(1);
        vec.push_back(9);
        vec.push_back(3);

        fl::stable_sort(vec.begin(), vec.end(), [](int a, int b) { return a > b; });

        CHECK_EQ(vec[0], 9);
        CHECK_EQ(vec[1], 8);
        CHECK_EQ(vec[2], 5);
        CHECK_EQ(vec[3], 3);
        CHECK_EQ(vec[4], 2);
        CHECK_EQ(vec[5], 1);
    }

    SUBCASE("Sort using fl::less") {
        fl::vector<int> vec;
        vec.push_back(5);
        vec.push_back(2);
        vec.push_back(8);
        vec.push_back(1);

        fl::stable_sort(vec.begin(), vec.end(), fl::less<int>());

        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 2);
        CHECK_EQ(vec[2], 5);
        CHECK_EQ(vec[3], 8);
    }
}

TEST_CASE("fl::stable_sort with different container types") {
    SUBCASE("Sort FixedVector") {
        fl::FixedVector<int, 6> vec;
        vec.push_back(5);
        vec.push_back(2);
        vec.push_back(8);
        vec.push_back(1);
        vec.push_back(9);
        vec.push_back(3);

        fl::stable_sort(vec.begin(), vec.end());

        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 2);
        CHECK_EQ(vec[2], 3);
        CHECK_EQ(vec[3], 5);
        CHECK_EQ(vec[4], 8);
        CHECK_EQ(vec[5], 9);
    }

    SUBCASE("Sort HeapVector") {
        fl::HeapVector<int> vec;
        vec.push_back(5);
        vec.push_back(2);
        vec.push_back(8);
        vec.push_back(1);

        fl::stable_sort(vec.begin(), vec.end());

        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 2);
        CHECK_EQ(vec[2], 5);
        CHECK_EQ(vec[3], 8);
    }
}

TEST_CASE("fl::stable_sort edge cases and stress tests") {
    SUBCASE("Very large array with many duplicates") {
        fl::vector<int> vec;
        
        // Create a pattern: 1,2,3,1,2,3,... for 100 elements
        for (int i = 0; i < 100; ++i) {
            vec.push_back((i % 3) + 1);
        }
        
        fl::stable_sort(vec.begin(), vec.end());
        
        // Verify all 1's come first (should be ~33 of them)
        int ones_count = 0;
        int twos_count = 0;
        int threes_count = 0;
        
        for (size_t i = 0; i < vec.size(); ++i) {
            if (vec[i] == 1) {
                ones_count++;
                // All 1's should come before any 2's or 3's
                CHECK_LT(i, 34); // Should be in first ~33 positions
            } else if (vec[i] == 2) {
                twos_count++;
                // All 2's should come after 1's but before 3's
                CHECK_GE(i, 33);
                CHECK_LT(i, 67);
            } else if (vec[i] == 3) {
                threes_count++;
                // All 3's should come last
                CHECK_GE(i, 66);
            }
        }
        
        CHECK_EQ(ones_count, 34);   // 0,3,6,9,... up to 99 = 34 elements
        CHECK_EQ(twos_count, 33);   // 1,4,7,10,... up to 97 = 33 elements  
        CHECK_EQ(threes_count, 33); // 2,5,8,11,... up to 98 = 33 elements
    }

    SUBCASE("Array with all identical elements") {
        fl::vector<int> vec;
        for (int i = 0; i < 50; ++i) {
            vec.push_back(42);
        }
        
        fl::stable_sort(vec.begin(), vec.end());
        
        // All elements should remain 42
        for (size_t i = 0; i < vec.size(); ++i) {
            CHECK_EQ(vec[i], 42);
        }
    }

    SUBCASE("Reverse sorted large array") {
        fl::vector<int> vec;
        for (int i = 100; i >= 1; --i) {
            vec.push_back(i);
        }
        
        fl::stable_sort(vec.begin(), vec.end());
        
        // Should be sorted 1,2,3,...,100
        for (size_t i = 0; i < vec.size(); ++i) {
            CHECK_EQ(vec[i], static_cast<int>(i + 1));
        }
    }

    SUBCASE("Array with extreme values") {
        fl::vector<int> vec;
        vec.push_back(2147483647);  // INT_MAX
        vec.push_back(-2147483648); // INT_MIN  
        vec.push_back(0);
        vec.push_back(1);
        vec.push_back(-1);
        
        fl::stable_sort(vec.begin(), vec.end());
        
        CHECK_EQ(vec[0], -2147483648);
        CHECK_EQ(vec[1], -1);
        CHECK_EQ(vec[2], 0);
        CHECK_EQ(vec[3], 1);
        CHECK_EQ(vec[4], 2147483647);
    }
}

TEST_CASE("fl::stable_sort with complex objects") {
    struct Person {
        fl::string name;
        int age;
        int id; // for tracking original order
        
        Person() : name(""), age(0), id(0) {}
        Person(const fl::string& n, int a, int i) : name(n), age(a), id(i) {}
    };

    SUBCASE("Sort by age, maintain name order for same age") {
        fl::vector<Person> people;
        people.push_back(Person("Alice", 25, 0));
        people.push_back(Person("Bob", 30, 1));
        people.push_back(Person("Charlie", 25, 2));  // Same age as Alice
        people.push_back(Person("David", 20, 3));
        people.push_back(Person("Eve", 30, 4));      // Same age as Bob
        
        // Sort by age, stable sort should maintain relative order for same ages
        fl::stable_sort(people.begin(), people.end(), 
                       [](const Person& a, const Person& b) {
                           return a.age < b.age;
                       });
        
        // Verify age order
        CHECK_EQ(people[0].age, 20);  // David
        CHECK_EQ(people[1].age, 25);  // Alice (should come before Charlie)
        CHECK_EQ(people[2].age, 25);  // Charlie  
        CHECK_EQ(people[3].age, 30);  // Bob (should come before Eve)
        CHECK_EQ(people[4].age, 30);  // Eve
        
        // Verify stability within age groups
        CHECK_EQ(people[0].id, 3);    // David
        CHECK_EQ(people[1].id, 0);    // Alice came before Charlie originally
        CHECK_EQ(people[2].id, 2);    // Charlie
        CHECK_EQ(people[3].id, 1);    // Bob came before Eve originally  
        CHECK_EQ(people[4].id, 4);    // Eve
    }
}

TEST_CASE("fl::stable_sort algorithm boundaries") {
    SUBCASE("Test small array threshold (≤32 elements)") {
        // Test exactly 32 elements (boundary case)
        fl::vector<int> vec;
        for (int i = 32; i >= 1; --i) {
            vec.push_back(i);
        }
        
        fl::stable_sort(vec.begin(), vec.end());
        
        for (size_t i = 0; i < vec.size(); ++i) {
            CHECK_EQ(vec[i], static_cast<int>(i + 1));
        }
    }

    SUBCASE("Test large array (>32 elements)") {
        // Test 33 elements (just over boundary)
        fl::vector<int> vec;
        for (int i = 33; i >= 1; --i) {
            vec.push_back(i);
        }
        
        fl::stable_sort(vec.begin(), vec.end());
        
        for (size_t i = 0; i < vec.size(); ++i) {
            CHECK_EQ(vec[i], static_cast<int>(i + 1));
        }
    }

    SUBCASE("Test rotation edge cases") {
        // Single element rotations
        fl::vector<int> vec;
        vec.push_back(2);
        vec.push_back(1);
        
        fl::stable_sort(vec.begin(), vec.end());
        
        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 2);
    }
}

TEST_CASE("fl::stable_sort vs fl::sort comparison") {
    struct IndexedValue {
        int value;
        int original_index;
        
        IndexedValue() : value(0), original_index(0) {}
        IndexedValue(int v, int idx) : value(v), original_index(idx) {}
    };

    SUBCASE("Compare stability between sort and stable_sort") {
        // Create two identical arrays
        fl::vector<IndexedValue> vec_stable;
        fl::vector<IndexedValue> vec_unstable;
        
        // Add elements with same values but different original indices
        for (int i = 0; i < 20; ++i) {
            int value = (i % 5);  // Creates many duplicates: 0,1,2,3,4,0,1,2,3,4,...
            vec_stable.push_back(IndexedValue(value, i));
            vec_unstable.push_back(IndexedValue(value, i));
        }
        
        auto comparator = [](const IndexedValue& a, const IndexedValue& b) {
            return a.value < b.value;
        };
        
        fl::stable_sort(vec_stable.begin(), vec_stable.end(), comparator);
        fl::sort(vec_unstable.begin(), vec_unstable.end(), comparator);
        
        // Both should be sorted by value
        for (size_t i = 0; i < vec_stable.size(); ++i) {
            CHECK_EQ(vec_stable[i].value, vec_unstable[i].value);
        }
        
        // stable_sort should preserve relative order of equal elements
        // Check that for each value group, original indices are in increasing order
        for (int target_value = 0; target_value < 5; ++target_value) {
            fl::vector<int> stable_indices;
            
            for (size_t i = 0; i < vec_stable.size(); ++i) {
                if (vec_stable[i].value == target_value) {
                    stable_indices.push_back(vec_stable[i].original_index);
                }
            }
            
            // In stable sort, indices should be in original order: 0,5,10,15 for value 0, etc.
            for (size_t i = 1; i < stable_indices.size(); ++i) {
                CHECK_LT(stable_indices[i-1], stable_indices[i]);
            }
        }
    }
}

TEST_CASE("fl::fl_random basic functionality") {
    SUBCASE("Default constructor") {
        fl::fl_random rng;
        
        // Should be able to generate numbers
        uint32_t val1 = rng();
        uint32_t val2 = rng();
        
        // Values should be in valid range
        CHECK_GE(val1, fl::fl_random::minimum());
        CHECK_LE(val1, fl::fl_random::maximum());
        CHECK_GE(val2, fl::fl_random::minimum());
        CHECK_LE(val2, fl::fl_random::maximum());
    }

    SUBCASE("Constructor with seed") {
        fl::fl_random rng1(12345);
        fl::fl_random rng2(12345);  // Same seed
        fl::fl_random rng3(54321);  // Different seed
        
        // Same seed should produce same sequence
        uint32_t val1a = rng1();
        uint32_t val1b = rng1();
        
        uint32_t val2a = rng2();
        uint32_t val2b = rng2();
        
        CHECK_EQ(val1a, val2a);
        CHECK_EQ(val1b, val2b);
        
        // Different seed should likely produce different values
        uint32_t val3a = rng3();
        CHECK_NE(val1a, val3a);  // Very unlikely to be equal
    }

    SUBCASE("Range generation with single parameter") {
        fl::fl_random rng(12345);
        
        // Test range [0, 10)
        for (int i = 0; i < 100; ++i) {
            uint32_t val = rng(10);
            CHECK_GE(val, 0);
            CHECK_LT(val, 10);
        }
        
        // Test range [0, 1) - should always be 0
        for (int i = 0; i < 10; ++i) {
            uint32_t val = rng(1);
            CHECK_EQ(val, 0);
        }
    }

    SUBCASE("Range generation with min and max") {
        fl::fl_random rng(12345);
        
        // Test range [5, 15)
        for (int i = 0; i < 100; ++i) {
            uint32_t val = rng(5, 15);
            CHECK_GE(val, 5);
            CHECK_LT(val, 15);
        }
        
        // Test range [100, 101) - should always be 100
        for (int i = 0; i < 10; ++i) {
            uint32_t val = rng(100, 101);
            CHECK_EQ(val, 100);
        }
    }

    SUBCASE("8-bit random generation") {
        fl::fl_random rng(12345);
        
        // Test random8()
        for (int i = 0; i < 50; ++i) {
            uint8_t val = rng.random8();
            CHECK_GE(val, 0);
            CHECK_LE(val, 255);
        }
        
        // Test random8(n)
        for (int i = 0; i < 50; ++i) {
            uint8_t val = rng.random8(50);
            CHECK_GE(val, 0);
            CHECK_LT(val, 50);
        }
        
        // Test random8(min, max)
        for (int i = 0; i < 50; ++i) {
            uint8_t val = rng.random8(10, 20);
            CHECK_GE(val, 10);
            CHECK_LT(val, 20);
        }
    }

    SUBCASE("16-bit random generation") {
        fl::fl_random rng(12345);
        
        // Test random16()
        for (int i = 0; i < 50; ++i) {
            uint16_t val = rng.random16();
            CHECK_GE(val, 0);
            CHECK_LE(val, 65535);
        }
        
        // Test random16(n)
        for (int i = 0; i < 50; ++i) {
            uint16_t val = rng.random16(1000);
            CHECK_GE(val, 0);
            CHECK_LT(val, 1000);
        }
        
        // Test random16(min, max)
        for (int i = 0; i < 50; ++i) {
            uint16_t val = rng.random16(500, 1500);
            CHECK_GE(val, 500);
            CHECK_LT(val, 1500);
        }
    }

    SUBCASE("Seed management") {
        fl::fl_random rng;
        
        // Set and get seed
        rng.set_seed(42);
        CHECK_EQ(rng.get_seed(), 42);
        
        // Add entropy
        rng.add_entropy(100);
        CHECK_EQ(rng.get_seed(), 142);  // 42 + 100
    }

    SUBCASE("Static min/max methods") {
        CHECK_EQ(fl::fl_random::minimum(), 0);
        CHECK_EQ(fl::fl_random::maximum(), 4294967295U);
    }
}

TEST_CASE("fl::fl_random deterministic behavior") {
    SUBCASE("Same seed produces same sequence") {
        fl::fl_random rng1(12345);
        fl::fl_random rng2(12345);
        
        fl::vector<uint32_t> seq1;
        fl::vector<uint32_t> seq2;
        
        // Generate same sequence from both generators
        for (int i = 0; i < 20; ++i) {
            seq1.push_back(rng1());
            seq2.push_back(rng2());
        }
        
        // Sequences should be identical
        for (size_t i = 0; i < seq1.size(); ++i) {
            CHECK_EQ(seq1[i], seq2[i]);
        }
    }

    SUBCASE("Different seeds produce different sequences") {
        fl::fl_random rng1(12345);
        fl::fl_random rng2(54321);
        
        fl::vector<uint32_t> seq1;
        fl::vector<uint32_t> seq2;
        
        // Generate sequences from both generators
        for (int i = 0; i < 20; ++i) {
            seq1.push_back(rng1());
            seq2.push_back(rng2());
        }
        
        // Sequences should be different (very unlikely to be identical)
        bool different = false;
        for (size_t i = 0; i < seq1.size(); ++i) {
            if (seq1[i] != seq2[i]) {
                different = true;
                break;
            }
        }
        CHECK(different);
    }
}

TEST_CASE("fl::default_random global instance") {
    SUBCASE("Global instance is accessible") {
        // Should be able to use the global instance
        uint32_t val1 = fl::default_random()();
        uint32_t val2 = fl::default_random()();
        
        // Values should be in valid range
        CHECK_GE(val1, fl::fl_random::minimum());
        CHECK_LE(val1, fl::fl_random::maximum());
        CHECK_GE(val2, fl::fl_random::minimum());
        CHECK_LE(val2, fl::fl_random::maximum());
    }

    SUBCASE("Global instance can be seeded") {
        fl::default_random().set_seed(12345);
        CHECK_EQ(fl::default_random().get_seed(), 12345);
        
        uint32_t val1 = fl::default_random()();
        
        // Reset to same seed and verify same value
        fl::default_random().set_seed(12345);
        uint32_t val2 = fl::default_random()();
        
        CHECK_EQ(val1, val2);
    }
}

TEST_CASE("fl::shuffle with fl::fl_random") {
    SUBCASE("Shuffle with explicit fl::fl_random instance") {
        fl::vector<int> vec;
        vec.push_back(1);
        vec.push_back(2);
        vec.push_back(3);
        vec.push_back(4);
        vec.push_back(5);
        
        fl::fl_random rng(12345);
        fl::shuffle(vec.begin(), vec.end(), rng);
        
        // All elements should still be present
        CHECK_EQ(vec.size(), 5);
        
        // Check each original element is still present
        for (int i = 1; i <= 5; ++i) {
            bool found = false;
            for (size_t j = 0; j < vec.size(); ++j) {
                if (vec[j] == i) {
                    found = true;
                    break;
                }
            }
            CHECK(found);
        }
    }

    SUBCASE("Deterministic shuffle with same seed") {
        fl::vector<int> vec1;
        fl::vector<int> vec2;
        
        for (int i = 1; i <= 10; ++i) {
            vec1.push_back(i);
            vec2.push_back(i);
        }
        
        fl::fl_random rng1(12345);
        fl::fl_random rng2(12345);  // Same seed
        
        fl::shuffle(vec1.begin(), vec1.end(), rng1);
        fl::shuffle(vec2.begin(), vec2.end(), rng2);
        
        // Same seed should produce same shuffle
        CHECK_EQ(vec1.size(), vec2.size());
        for (size_t i = 0; i < vec1.size(); ++i) {
            CHECK_EQ(vec1[i], vec2[i]);
        }
    }

    SUBCASE("Shuffle with global default_random") {
        fl::vector<int> vec;
        vec.push_back(10);
        vec.push_back(20);
        vec.push_back(30);
        vec.push_back(40);
        vec.push_back(50);
        
        // This should use the global default_random instance
        fl::shuffle(vec.begin(), vec.end());
        
        // All elements should still be present
        CHECK_EQ(vec.size(), 5);
        
        // Check each original element is still present
        int expected[] = {10, 20, 30, 40, 50};
        for (int i = 0; i < 5; ++i) {
            bool found = false;
            for (size_t j = 0; j < vec.size(); ++j) {
                if (vec[j] == expected[i]) {
                    found = true;
                    break;
                }
            }
            CHECK(found);
        }
    }
}
