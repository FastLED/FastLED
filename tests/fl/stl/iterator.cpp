

// Comprehensive container iterator tests
// Verifies that all container types properly implement iterator interfaces:
// 1. Forward iterators (begin/end, operator++, operator*)
// 2. Const iterators (immutable access)
// 3. Reverse iterators (rbegin/rend) for bidirectional containers
// 4. Empty state after move (begin() == end())

#include "test.h"
#include "test_container_helpers.h"

#include "fl/stl/array.h"
#include "fl/stl/deque.h"
#include "fl/stl/iterator.h"
#include "fl/stl/list.h"
#include "fl/stl/map.h"
#include "fl/stl/move.h"
#include "fl/stl/new.h"
#include "fl/stl/set.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/string.h"
#include "fl/stl/unordered_map.h"
#include "fl/stl/unordered_set.h"
#include "fl/stl/vector.h"

using namespace test_helpers;



FL_TEST_CASE("back_inserter with vector") {
    fl::vector<int> vec;

    FL_SUBCASE("Basic insertion") {
        auto inserter = fl::back_inserter(vec);
        *inserter = 10;
        *inserter = 20;
        *inserter = 30;

        FL_CHECK(vec.size() == 3);
        FL_CHECK(vec[0] == 10);
        FL_CHECK(vec[1] == 20);
        FL_CHECK(vec[2] == 30);
    }

    FL_SUBCASE("Pre-increment") {
        auto inserter = fl::back_inserter(vec);
        ++inserter = 10;

        FL_CHECK(vec.size() == 1);
        FL_CHECK(vec[0] == 10);
    }

    FL_SUBCASE("Post-increment") {
        auto inserter = fl::back_inserter(vec);
        inserter++ = 10;

        FL_CHECK(vec.size() == 1);
        FL_CHECK(vec[0] == 10);
    }

    FL_SUBCASE("Dereference") {
        auto inserter = fl::back_inserter(vec);
        *inserter = 10;
        *inserter = 20;

        FL_CHECK(vec.size() == 2);
        FL_CHECK(vec[0] == 10);
        FL_CHECK(vec[1] == 20);
    }
}

FL_TEST_CASE("back_inserter with FixedVector") {
    fl::FixedVector<int, 5> vec;

    FL_SUBCASE("Basic insertion") {
        auto inserter = fl::back_inserter(vec);
        *inserter = 10;
        *inserter = 20;
        *inserter = 30;

        FL_CHECK(vec.size() == 3);
        FL_CHECK(vec[0] == 10);
        FL_CHECK(vec[1] == 20);
        FL_CHECK(vec[2] == 30);
    }
}

FL_TEST_CASE("back_inserter with InlinedVector") {
    fl::InlinedVector<int, 3> vec;

    FL_SUBCASE("Basic insertion within inline capacity") {
        auto inserter = fl::back_inserter(vec);
        *inserter = 10;
        *inserter = 20;

        FL_CHECK(vec.size() == 2);
        FL_CHECK(vec[0] == 10);
        FL_CHECK(vec[1] == 20);
    }

    FL_SUBCASE("Insertion beyond inline capacity (heap allocation)") {
        auto inserter = fl::back_inserter(vec);
        *inserter = 10;
        *inserter = 20;
        *inserter = 30;
        *inserter = 40;

        FL_CHECK(vec.size() == 4);
        FL_CHECK(vec[0] == 10);
        FL_CHECK(vec[1] == 20);
        FL_CHECK(vec[2] == 30);
        FL_CHECK(vec[3] == 40);
    }
}

FL_TEST_CASE("back_inserter with move semantics") {
    struct MoveOnly {
        int value;

        MoveOnly(int v) : value(v) {}

        // Delete copy constructor and copy assignment
        MoveOnly(const MoveOnly&) = delete;
        MoveOnly& operator=(const MoveOnly&) = delete;

        // Move constructor and move assignment
        MoveOnly(MoveOnly&& other) : value(other.value) {
            other.value = -1;
        }

        MoveOnly& operator=(MoveOnly&& other) {
            value = other.value;
            other.value = -1;
            return *this;
        }
    };

    fl::vector<MoveOnly> vec;

    FL_SUBCASE("Move insertion") {
        auto inserter = fl::back_inserter(vec);
        *inserter = MoveOnly(42);

        FL_CHECK(vec.size() == 1);
        FL_CHECK(vec[0].value == 42);
    }
}

// Test integration with algorithms
FL_TEST_CASE("back_inserter with algorithm integration") {
    fl::vector<int> source;
    source.push_back(1);
    source.push_back(2);
    source.push_back(3);

    fl::vector<int> dest;

    FL_SUBCASE("Manual copy using back_inserter") {
        auto inserter = fl::back_inserter(dest);
        for (auto it = source.begin(); it != source.end(); ++it) {
            *inserter++ = *it;
        }

        FL_CHECK(dest.size() == 3);
        FL_CHECK(dest[0] == 1);
        FL_CHECK(dest[1] == 2);
        FL_CHECK(dest[2] == 3);
    }
}




// ============================================================================
// ITERATOR TESTS - Verify iterator support and empty state after move
// ============================================================================

FL_TEST_CASE("Iterator support - containers with shared_ptr") {
    FL_SUBCASE("fl::vector") {
        test_container_iterators_with_shared_ptr<fl::vector<fl::shared_ptr<int>>>();
    }

    FL_SUBCASE("fl::deque") {
        test_container_iterators_with_shared_ptr<fl::deque<fl::shared_ptr<int>>>();
    }

    FL_SUBCASE("fl::list") {
        test_container_iterators_with_shared_ptr<fl::list<fl::shared_ptr<int>>>();
    }

    FL_SUBCASE("fl::set") {
        test_container_iterators_with_shared_ptr<fl::set<fl::shared_ptr<int>>>();
    }

    FL_SUBCASE("fl::VectorSet") {
        test_container_iterators_with_shared_ptr<fl::VectorSet<fl::shared_ptr<int>>>();
    }

    FL_SUBCASE("fl::unordered_set") {
        test_container_iterators_with_shared_ptr<fl::unordered_set<fl::shared_ptr<int>>>();
    }
}

FL_TEST_CASE("Iterator support - linear containers") {
    FL_SUBCASE("fl::deque - iterators") {
        fl::deque<int> source;
        source.push_back(1);
        source.push_back(2);
        source.push_back(3);

        // Mutable iterator
        auto it = source.begin();
        FL_CHECK(*it == 1);
        *it = 10;  // Modify through iterator
        FL_CHECK(source[0] == 10);

        // Const iterator via const reference
        const auto& const_source = source;
        auto cit = const_source.begin();
        FL_CHECK(*cit == 10);

        // Move and check empty
        fl::deque<int> destination = fl::move(source);
        FL_CHECK(source.begin() == source.end());
        FL_CHECK(destination.begin() != destination.end());
    }

    FL_SUBCASE("fl::list - iterators") {
        fl::list<int> source;
        source.push_back(1);
        source.push_back(2);
        source.push_back(3);

        // Forward iteration
        int sum = 0;
        for (auto it = source.begin(); it != source.end(); ++it) {
            sum += *it;
        }
        FL_CHECK(sum == 6);

        // Const iteration via const reference
        const auto& const_source = source;
        int const_sum = 0;
        for (auto it = const_source.begin(); it != const_source.end(); ++it) {
            const_sum += *it;
        }
        FL_CHECK(const_sum == 6);

        // Move and check empty
        fl::list<int> destination = fl::move(source);
        FL_CHECK(source.begin() == source.end());
        FL_CHECK(destination.begin() != destination.end());
    }
}

FL_TEST_CASE("Iterator support - set containers with int") {
    FL_SUBCASE("fl::set - const iterators (immutable keys)") {
        fl::set<int> source;
        source.insert(30);
        source.insert(10);
        source.insert(20);

        // Set iterators are always const (keys are immutable)
        auto it = source.begin();
        FL_CHECK(*it == 10);  // Sorted
        ++it;
        FL_CHECK(*it == 20);
        ++it;
        FL_CHECK(*it == 30);

        // Const iterators via const reference
        const auto& const_source = source;
        auto cit = const_source.begin();
        FL_CHECK(*cit == 10);

        // Move and check empty
        fl::set<int> destination = fl::move(source);
        FL_CHECK(source.begin() == source.end());
        const auto& const_empty = source;
        FL_CHECK(const_empty.begin() == const_empty.end());
        FL_CHECK(destination.begin() != destination.end());
    }

    FL_SUBCASE("fl::VectorSet - iterators") {
        fl::VectorSet<int> source;
        source.insert(1);
        source.insert(2);
        source.insert(3);

        // Iteration order is insertion order
        auto it = source.begin();
        FL_CHECK(*it == 1);
        ++it;
        FL_CHECK(*it == 2);

        // Move and check empty
        fl::VectorSet<int> destination = fl::move(source);
        FL_CHECK(source.begin() == source.end());
        FL_CHECK(destination.begin() != destination.end());
    }

    FL_SUBCASE("fl::unordered_set - iterators") {
        fl::unordered_set<int> source;
        source.insert(1);
        source.insert(2);
        source.insert(3);

        // Verify has elements
        FL_CHECK(source.begin() != source.end());
        int count = 0;
        for (auto it = source.begin(); it != source.end(); ++it) {
            count++;
        }
        FL_CHECK(count == 3);

        // Move and check empty
        fl::unordered_set<int> destination = fl::move(source);
        FL_CHECK(source.begin() == source.end());
        FL_CHECK(destination.begin() != destination.end());
    }
}

FL_TEST_CASE("Iterator support - map containers") {
    FL_SUBCASE("fl::map - iterators return pairs") {
        fl::map<int, fl::string> source;
        source[1] = "one";
        source[2] = "two";
        source[3] = "three";

        // Mutable iterator - returns pair<const Key, Value>
        auto it = source.begin();
        FL_CHECK(it->first == 1);
        FL_CHECK(it->second == "one");
        it->second = "ONE";  // Can modify value
        FL_CHECK(source[1] == "ONE");

        // Const iterator
        const auto& const_source = source;
        auto cit = const_source.begin();
        FL_CHECK(cit->first == 1);
        FL_CHECK(cit->second == "ONE");

        // Iteration
        int key_sum = 0;
        for (auto it = source.begin(); it != source.end(); ++it) {
            key_sum += it->first;
        }
        FL_CHECK(key_sum == 6);

        // Move and check empty
        fl::map<int, fl::string> destination = fl::move(source);
        FL_CHECK(source.begin() == source.end());
        const auto& const_empty = source;
        FL_CHECK(const_empty.begin() == const_empty.end());
        FL_CHECK(destination.begin() != destination.end());
    }

    FL_SUBCASE("fl::unordered_map - iterators") {
        fl::unordered_map<int, int> source;
        source[1] = 100;
        source[2] = 200;
        source[3] = 300;

        // Iterate and sum values
        int value_sum = 0;
        for (auto it = source.begin(); it != source.end(); ++it) {
            value_sum += it->second;
        }
        FL_CHECK(value_sum == 600);

        // Const iteration via const reference
        const auto& const_source = source;
        int const_sum = 0;
        for (auto it = const_source.begin(); it != const_source.end(); ++it) {
            const_sum += it->second;
        }
        FL_CHECK(const_sum == 600);

        // Move and check empty
        fl::unordered_map<int, int> destination = fl::move(source);
        FL_CHECK(source.begin() == source.end());
        FL_CHECK(destination.begin() != destination.end());
    }

    FL_SUBCASE("fl::SortedHeapMap - iterators") {
        fl::SortedHeapMap<int, int> source;
        source.insert(3, 30);
        source.insert(1, 10);
        source.insert(2, 20);

        // Should be sorted by key
        auto it = source.begin();
        FL_CHECK(it->first == 1);
        ++it;
        FL_CHECK(it->first == 2);
        ++it;
        FL_CHECK(it->first == 3);

        // Move and check empty
        fl::SortedHeapMap<int, int> destination = fl::move(source);
        FL_CHECK(source.begin() == source.end());
        FL_CHECK(destination.begin() != destination.end());
    }

    FL_SUBCASE("fl::string - iterators") {
        fl::string source = "hello";

        // Forward iteration
        auto it = source.begin();
        FL_CHECK(*it == 'h');
        ++it;
        FL_CHECK(*it == 'e');

        // Modify through iterator
        *it = 'E';
        FL_CHECK(source[1] == 'E');

        // Const iterators
        const auto& const_source = source;
        auto cit = const_source.begin();
        FL_CHECK(*cit == 'h');

        // Count characters
        int count = 0;
        for (auto c : source) {
            count++;
            (void)c;
        }
        FL_CHECK(count == 5);

        // Move and check empty
        fl::string destination = fl::move(source);
        FL_CHECK(source.begin() == source.end());
        FL_CHECK(destination.begin() != destination.end());
    }

    FL_SUBCASE("fl::FixedVector - iterators") {
        fl::FixedVector<int, 10> source;
        source.push_back(10);
        source.push_back(20);
        source.push_back(30);

        // Forward iteration
        auto it = source.begin();
        FL_CHECK(*it == 10);
        ++it;
        FL_CHECK(*it == 20);

        // Modify through iterator
        *it = 200;
        FL_CHECK(source[1] == 200);

        // Const iterators
        const auto& const_source = source;
        auto cit = const_source.begin();
        FL_CHECK(*cit == 10);

        // Move and check empty
        fl::FixedVector<int, 10> destination = fl::move(source);
        FL_CHECK(source.begin() == source.end());
        FL_CHECK(destination.begin() != destination.end());
    }

    FL_SUBCASE("fl::InlinedVector - iterators") {
        fl::InlinedVector<int, 2> source;
        source.push_back(10);
        source.push_back(20);
        source.push_back(30);  // Forces heap allocation

        // Forward iteration
        auto it = source.begin();
        FL_CHECK(*it == 10);
        ++it;
        FL_CHECK(*it == 20);
        ++it;
        FL_CHECK(*it == 30);

        // Modify through iterator
        it = source.begin();
        *it = 100;
        FL_CHECK(source[0] == 100);

        // Move and check empty
        fl::InlinedVector<int, 2> destination = fl::move(source);
        FL_CHECK(source.begin() == source.end());
        FL_CHECK(destination.begin() != destination.end());
    }

    FL_SUBCASE("fl::FixedMap - iterators") {
        fl::FixedMap<int, int, 10> source;
        source.insert(10, 100);
        source.insert(20, 200);
        source.insert(30, 300);

        // Iterate over pairs
        bool found_10 = false;
        bool found_20 = false;
        for (auto it = source.begin(); it != source.end(); ++it) {
            if (it->first == 10) {
                FL_CHECK(it->second == 100);
                found_10 = true;
            }
            if (it->first == 20) {
                FL_CHECK(it->second == 200);
                found_20 = true;
            }
        }
        FL_CHECK(found_10);
        FL_CHECK(found_20);

        // Move and check empty
        fl::FixedMap<int, int, 10> destination = fl::move(source);
        FL_CHECK(source.begin() == source.end());
        FL_CHECK(destination.begin() != destination.end());
    }

    FL_SUBCASE("fl::VectorSetFixed - iterators") {
        fl::VectorSetFixed<int, 10> source;
        source.insert(10);
        source.insert(20);
        source.insert(30);

        // Iterate in insertion order
        auto it = source.begin();
        FL_CHECK(*it == 10);
        ++it;
        FL_CHECK(*it == 20);
        ++it;
        FL_CHECK(*it == 30);

        // Const iterators
        const auto& const_source = source;
        auto cit = const_source.begin();
        FL_CHECK(*cit == 10);

        // Move and check empty
        fl::VectorSetFixed<int, 10> destination = fl::move(source);
        FL_CHECK(source.begin() == source.end());
        FL_CHECK(destination.begin() != destination.end());
    }

    FL_SUBCASE("fl::array - iterators") {
        fl::array<int, 3> source = {10, 20, 30};

        // Forward iteration
        auto it = source.begin();
        FL_CHECK(*it == 10);
        ++it;
        FL_CHECK(*it == 20);
        ++it;
        FL_CHECK(*it == 30);

        // Modify through iterator
        it = source.begin();
        *it = 100;
        FL_CHECK(source[0] == 100);

        // Const iterators (cbegin/cend)
        auto cit = source.cbegin();
        FL_CHECK(*cit == 100);

        // Range-based for loop
        int sum = 0;
        for (int val : source) {
            sum += val;
        }
        FL_CHECK(sum == 150);  // 100 + 20 + 30
    }
}

// ============================================================================
// REVERSE ITERATOR TESTS - Bidirectional and ordered containers
// ============================================================================

FL_TEST_CASE("Reverse iterator support - bidirectional containers") {
    FL_SUBCASE("fl::vector") {
        test_container_reverse_iterators<fl::vector<int>>();
    }

    FL_SUBCASE("fl::deque") {
        test_container_reverse_iterators<fl::deque<int>>();
    }

    FL_SUBCASE("fl::list") {
        test_container_reverse_iterators<fl::list<int>>();
    }

    FL_SUBCASE("fl::VectorSet") {
        // VectorSet uses insert, not push_back
        fl::VectorSet<int> source;
        source.insert(10);
        source.insert(20);
        source.insert(30);

        // Reverse iteration (insertion order, reversed)
        auto rit = source.rbegin();
        FL_CHECK(*rit == 30);

        // Move and verify
        fl::VectorSet<int> destination = fl::move(source);
        FL_CHECK(source.rbegin() == source.rend());  // Empty
        FL_CHECK(destination.rbegin() != destination.rend());
        FL_CHECK(*destination.rbegin() == 30);
    }

    FL_SUBCASE("fl::set") {
        // Set uses insert, not push_back, so create a custom test
        fl::set<int> source;
        source.insert(30);
        source.insert(10);
        source.insert(20);

        // Reverse iteration (sorted, descending: 30, 20, 10)
        auto rit = source.rbegin();
        FL_CHECK(*rit == 30);
        ++rit;
        FL_CHECK(*rit == 20);
        ++rit;
        FL_CHECK(*rit == 10);

        // Sum in reverse
        int sum = 0;
        for (auto it = source.rbegin(); it != source.rend(); ++it) {
            sum += *it;
        }
        FL_CHECK(sum == 60);

        // Move and verify
        fl::set<int> destination = fl::move(source);
        FL_CHECK(source.rbegin() == source.rend());  // Empty
        FL_CHECK(destination.rbegin() != destination.rend());
        FL_CHECK(*destination.rbegin() == 30);
    }

    FL_SUBCASE("fl::string - reverse iterators") {
        fl::string source = "hello";

        // Reverse iteration spells "olleh"
        auto rit = source.rbegin();
        FL_CHECK(*rit == 'o');  // Last char

        // Count characters in reverse
        int count = 0;
        for (auto it = source.rbegin(); it != source.rend(); ++it) {
            count++;
        }
        FL_CHECK(count == 5);

        // Const reverse iteration
        const auto& const_source = source;
        auto crit = const_source.rbegin();
        FL_CHECK(*crit == 'o');

        // Move and verify
        fl::string destination = fl::move(source);
        if (!destination.empty()) {
            auto drit = destination.rbegin();
            FL_CHECK(*drit == 'o');
        }
    }

    FL_SUBCASE("fl::map - reverse iterators with key-value pairs") {
        fl::map<int, int> source;
        source.insert(fl::pair<int, int>(30, 300));
        source.insert(fl::pair<int, int>(10, 100));
        source.insert(fl::pair<int, int>(20, 200));

        // Reverse iteration (sorted by key, descending: 30, 20, 10)
        auto rit = source.rbegin();
        FL_CHECK(rit->first == 30);
        FL_CHECK(rit->second == 300);
        ++rit;
        FL_CHECK(rit->first == 20);
        FL_CHECK(rit->second == 200);
        ++rit;
        FL_CHECK(rit->first == 10);
        FL_CHECK(rit->second == 100);

        // Const reverse iterators
        const auto& const_source = source;
        auto crit = const_source.rbegin();
        FL_CHECK(crit->first == 30);

        // Move and verify
        fl::map<int, int> destination = fl::move(source);
        FL_CHECK(source.rbegin() == source.rend());  // Empty
        FL_CHECK(destination.rbegin() != destination.rend());
        FL_CHECK(destination.rbegin()->first == 30);
    }

    FL_SUBCASE("fl::SortedHeapMap - reverse iterators") {
        fl::SortedHeapMap<int, int> source;
        source.insert(30, 300);
        source.insert(10, 100);
        source.insert(20, 200);

        // Reverse iteration (sorted by key, descending: 30, 20, 10)
        auto rit = source.rbegin();
        FL_CHECK(rit->first == 30);
        FL_CHECK(rit->second == 300);
        ++rit;
        FL_CHECK(rit->first == 20);
        FL_CHECK(rit->second == 200);
        ++rit;
        FL_CHECK(rit->first == 10);
        FL_CHECK(rit->second == 100);

        // Const reverse iterators
        const auto& const_source = source;
        auto crit = const_source.rbegin();
        FL_CHECK(crit->first == 30);

        // Move and verify
        fl::SortedHeapMap<int, int> destination = fl::move(source);
        FL_CHECK(source.rbegin() == source.rend());  // Empty
        FL_CHECK(destination.rbegin() != destination.rend());
        FL_CHECK(destination.rbegin()->first == 30);
    }
}
