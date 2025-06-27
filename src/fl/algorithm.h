#pragma once

#include "fl/type_traits.h"
#include "fl/move.h"

namespace fl {

template <typename Iterator>
void reverse(Iterator first, Iterator last) {
    while ((first != last) && (first != --last)) {
        swap(*first++, *last);
    }
}

template <typename Iterator>
Iterator max_element(Iterator first, Iterator last) {
    if (first == last) {
        return last;
    }
    
    Iterator max_iter = first;
    ++first;
    
    while (first != last) {
        if (*max_iter < *first) {
            max_iter = first;
        }
        ++first;
    }
    
    return max_iter;
}

template <typename Iterator, typename Compare>
Iterator max_element(Iterator first, Iterator last, Compare comp) {
    if (first == last) {
        return last;
    }
    
    Iterator max_iter = first;
    ++first;
    
    while (first != last) {
        if (comp(*max_iter, *first)) {
            max_iter = first;
        }
        ++first;
    }
    
    return max_iter;
}

template <typename Iterator>
Iterator min_element(Iterator first, Iterator last) {
    if (first == last) {
        return last;
    }
    
    Iterator min_iter = first;
    ++first;
    
    while (first != last) {
        if (*first < *min_iter) {
            min_iter = first;
        }
        ++first;
    }
    
    return min_iter;
}

template <typename Iterator, typename Compare>
Iterator min_element(Iterator first, Iterator last, Compare comp) {
    if (first == last) {
        return last;
    }
    
    Iterator min_iter = first;
    ++first;
    
    while (first != last) {
        if (comp(*first, *min_iter)) {
            min_iter = first;
        }
        ++first;
    }
    
    return min_iter;
}



template <typename Iterator1, typename Iterator2>
bool equal(Iterator1 first1, Iterator1 last1, Iterator2 first2) {
    while (first1 != last1) {
        if (*first1 != *first2) {
            return false;
        }
        ++first1;
        ++first2;
    }
    return true;
}

template <typename Iterator1, typename Iterator2, typename BinaryPredicate>
bool equal(Iterator1 first1, Iterator1 last1, Iterator2 first2, BinaryPredicate pred) {
    while (first1 != last1) {
        if (!pred(*first1, *first2)) {
            return false;
        }
        ++first1;
        ++first2;
    }
    return true;
}

template <typename Iterator1, typename Iterator2>
bool equal(Iterator1 first1, Iterator1 last1, Iterator2 first2, Iterator2 last2) {
    while (first1 != last1 && first2 != last2) {
        if (*first1 != *first2) {
            return false;
        }
        ++first1;
        ++first2;
    }
    return first1 == last1 && first2 == last2;
}

template <typename Iterator1, typename Iterator2, typename BinaryPredicate>
bool equal(Iterator1 first1, Iterator1 last1, Iterator2 first2, Iterator2 last2, BinaryPredicate pred) {
    while (first1 != last1 && first2 != last2) {
        if (!pred(*first1, *first2)) {
            return false;
        }
        ++first1;
        ++first2;
    }
    return first1 == last1 && first2 == last2;
}

template <typename Container1, typename Container2>
bool equal_container(const Container1& c1, const Container2& c2) {
    size_t size1 = c1.size();
    size_t size2 = c2.size();
    if (size1 != size2) {
        return false;
    }
    return equal(c1.begin(), c1.end(), c2.begin(), c2.end());
}

template <typename Container1, typename Container2, typename BinaryPredicate>
bool equal_container(const Container1& c1, const Container2& c2, BinaryPredicate pred) {
    size_t size1 = c1.size();
    size_t size2 = c2.size();
    if (size1 != size2) {
        return false;
    }
    return equal(c1.begin(), c1.end(), c2.begin(), c2.end(), pred);
}


template <typename Iterator, typename T>
void fill(Iterator first, Iterator last, const T& value) {
    while (first != last) {
        *first = value;
        ++first;
    }
}

template <typename Iterator, typename T>
Iterator find(Iterator first, Iterator last, const T& value) {
    while (first != last) {
        if (*first == value) {
            return first;
        }
        ++first;
    }
    return last;
}

template <typename Iterator, typename UnaryPredicate>
Iterator find_if(Iterator first, Iterator last, UnaryPredicate pred) {
    while (first != last) {
        if (pred(*first)) {
            return first;
        }
        ++first;
    }
    return last;
}

template <typename Iterator, typename UnaryPredicate>
Iterator find_if_not(Iterator first, Iterator last, UnaryPredicate pred) {
    while (first != last) {
        if (!pred(*first)) {
            return first;
        }
        ++first;
    }
    return last;
}

namespace detail {

// Insertion sort implementation for small arrays
template <typename Iterator, typename Compare>
void insertion_sort(Iterator first, Iterator last, Compare comp) {
    if (first == last) return;
    
    for (Iterator i = first + 1; i != last; ++i) {
        auto value = fl::move(*i);
        Iterator j = i;
        
        while (j != first && comp(value, *(j - 1))) {
            *j = fl::move(*(j - 1));
            --j;
        }
        
        *j = fl::move(value);
    }
}

// Median-of-three pivot selection
template <typename Iterator, typename Compare>
Iterator median_of_three(Iterator first, Iterator middle, Iterator last, Compare comp) {
    if (comp(*middle, *first)) {
        if (comp(*last, *middle)) {
            return middle;
        } else if (comp(*last, *first)) {
            return last;
        } else {
            return first;
        }
    } else {
        if (comp(*last, *first)) {
            return first;
        } else if (comp(*last, *middle)) {
            return last;
        } else {
            return middle;
        }
    }
}

// Partition function for quicksort
template <typename Iterator, typename Compare>
Iterator partition(Iterator first, Iterator last, Compare comp) {
    Iterator middle = first + (last - first) / 2;
    Iterator pivot_iter = median_of_three(first, middle, last - 1, comp);
    
    // Move pivot to end
    swap(*pivot_iter, *(last - 1));
    Iterator pivot = last - 1;
    
    Iterator i = first;
    for (Iterator j = first; j != pivot; ++j) {
        if (comp(*j, *pivot)) {
            swap(*i, *j);
            ++i;
        }
    }
    
    swap(*i, *pivot);
    return i;
}

// Heapsort implementation (fallback for deep recursion)
template <typename Iterator, typename Compare>
void sift_down(Iterator first, Iterator start, Iterator end, Compare comp) {
    Iterator root = start;
    
    while (root - first <= (end - first - 2) / 2) {
        Iterator child = first + 2 * (root - first) + 1;
        Iterator swap_iter = root;
        
        if (comp(*swap_iter, *child)) {
            swap_iter = child;
        }
        
        if (child + 1 <= end && comp(*swap_iter, *(child + 1))) {
            swap_iter = child + 1;
        }
        
        if (swap_iter == root) {
            return;
        } else {
            swap(*root, *swap_iter);
            root = swap_iter;
        }
    }
}

template <typename Iterator, typename Compare>
void heapify(Iterator first, Iterator last, Compare comp) {
    Iterator start = first + (last - first - 2) / 2;
    
    while (true) {
        sift_down(first, start, last - 1, comp);
        if (start == first) {
            break;
        }
        --start;
    }
}

template <typename Iterator, typename Compare>
void heap_sort(Iterator first, Iterator last, Compare comp) {
    heapify(first, last, comp);
    
    Iterator end = last - 1;
    while (end > first) {
        swap(*end, *first);
        sift_down(first, first, end - 1, comp);
        --end;
    }
}

// Introsort implementation
template <typename Iterator, typename Compare>
void introsort_impl(Iterator first, Iterator last, int depth_limit, Compare comp) {
    while (last - first > 16) {  // Use insertion sort for small arrays
        if (depth_limit == 0) {
            // Too deep, use heapsort
            heap_sort(first, last, comp);
            return;
        }
        
        --depth_limit;
        Iterator pivot = partition(first, last, comp);
        
        // Recursively sort the larger partition first to limit stack depth
        if (pivot - first < last - pivot - 1) {
            introsort_impl(first, pivot, depth_limit, comp);
            first = pivot + 1;
        } else {
            introsort_impl(pivot + 1, last, depth_limit, comp);
            last = pivot;
        }
    }
}

// Calculate depth limit for introsort
template <typename Iterator>
int calculate_depth_limit(Iterator first, Iterator last) {
    int depth = 0;
    for (auto n = last - first; n > 1; n >>= 1) {
        ++depth;
    }
    return depth * 2;  // 2 * log2(n)
}

} // namespace detail

// Sort function with custom comparator
template <typename Iterator, typename Compare>
void sort(Iterator first, Iterator last, Compare comp) {
    if (first == last || first + 1 == last) {
        return;  // Already sorted or empty
    }
    
    int depth_limit = detail::calculate_depth_limit(first, last);
    detail::introsort_impl(first, last, depth_limit, comp);
    
    // Final insertion sort pass for small remaining unsorted regions
    detail::insertion_sort(first, last, comp);
}

// Sort function with default comparator
template <typename Iterator>
void sort(Iterator first, Iterator last) {
    sort(first, last, [](const auto& a, const auto& b) { return a < b; });
}

} // namespace fl
