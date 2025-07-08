
#include "src/fl/set.h"
#include <iostream>
#include <cassert>

int main() {
    // Test fl::set basic functionality
    fl::set<int> my_set;
    
    // Test insertion
    auto result1 = my_set.insert(5);
    assert(result1.second == true);  // Should be inserted
    
    auto result2 = my_set.insert(3);
    assert(result2.second == true);  // Should be inserted
    
    auto result3 = my_set.insert(5);
    assert(result3.second == false); // Should not be inserted (duplicate)
    
    auto result4 = my_set.insert(7);
    assert(result4.second == true);  // Should be inserted
    
    // Test size
    assert(my_set.size() == 3);
    
    // Test contains/has
    assert(my_set.contains(5) == true);
    assert(my_set.contains(3) == true);
    assert(my_set.contains(7) == true);
    assert(my_set.contains(10) == false);
    
    assert(my_set.has(5) == true);
    assert(my_set.has(10) == false);
    
    // Test find
    auto it = my_set.find(5);
    assert(it != my_set.end());
    assert(*it == 5);
    
    auto it_not_found = my_set.find(100);
    assert(it_not_found == my_set.end());
    
    // Test iteration (should be in sorted order since we use SortedHeapMap)
    std::vector<int> expected = {3, 5, 7};
    std::vector<int> actual;
    for (const auto& val : my_set) {
        actual.push_back(val);
    }
    assert(actual == expected);
    
    // Test erase
    assert(my_set.erase(3) == 1);  // Should remove 1 element
    assert(my_set.erase(3) == 0);  // Should remove 0 elements (not found)
    assert(my_set.size() == 2);
    assert(my_set.contains(3) == false);
    
    // Test emplace
    auto result5 = my_set.emplace(1);
    assert(result5.second == true);
    assert(my_set.contains(1) == true);
    
    // Test clear
    my_set.clear();
    assert(my_set.empty() == true);
    assert(my_set.size() == 0);
    
    std::cout << "All fl::set tests passed!" << std::endl;
    return 0;
}
