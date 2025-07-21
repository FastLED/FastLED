#pragma once

#include "fl/type_traits.h"
#include "fl/move.h"
#include "fl/random.h"

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
    fl::size size1 = c1.size();
    fl::size size2 = c2.size();
    if (size1 != size2) {
        return false;
    }
    return equal(c1.begin(), c1.end(), c2.begin(), c2.end());
}

template <typename Container1, typename Container2, typename BinaryPredicate>
bool equal_container(const Container1& c1, const Container2& c2, BinaryPredicate pred) {
    fl::size size1 = c1.size();
    fl::size size2 = c2.size();
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

template <typename Iterator, typename T>
Iterator remove(Iterator first, Iterator last, const T& value) {
    Iterator result = first;
    while (first != last) {
        if (!(*first == value)) {
            if (result != first) {
                *result = fl::move(*first);
            }
            ++result;
        }
        ++first;
    }
    return result;
}

template <typename Iterator, typename UnaryPredicate>
Iterator remove_if(Iterator first, Iterator last, UnaryPredicate pred) {
    Iterator result = first;
    while (first != last) {
        if (!pred(*first)) {
            if (result != first) {
                *result = fl::move(*first);
            }
            ++result;
        }
        ++first;
    }
    return result;
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

// Quicksort implementation
template <typename Iterator, typename Compare>
void quicksort_impl(Iterator first, Iterator last, Compare comp) {
    if (last - first <= 16) {  // Use insertion sort for small arrays
        insertion_sort(first, last, comp);
        return;
    }
    
    Iterator pivot = partition(first, last, comp);
    quicksort_impl(first, pivot, comp);
    quicksort_impl(pivot + 1, last, comp);
}

// Rotate elements in range [first, last) so that middle becomes the new first
template <typename Iterator>
void rotate_impl(Iterator first, Iterator middle, Iterator last) {
    if (first == middle || middle == last) {
        return;
    }
    
    Iterator next = middle;
    while (first != next) {
        swap(*first++, *next++);
        if (next == last) {
            next = middle;
        } else if (first == middle) {
            middle = next;
        }
    }
}

// Find the position where value should be inserted in sorted range [first, last)
template <typename Iterator, typename T, typename Compare>
Iterator lower_bound_impl(Iterator first, Iterator last, const T& value, Compare comp) {
    auto count = last - first;
    while (count > 0) {
        auto step = count / 2;
        Iterator it = first + step;
        if (comp(*it, value)) {
            first = ++it;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    return first;
}

// In-place merge operation for merge sort (stable sort)
template <typename Iterator, typename Compare>
void merge_inplace(Iterator first, Iterator middle, Iterator last, Compare comp) {
    // If one of the ranges is empty, nothing to merge
    if (first == middle || middle == last) {
        return;
    }
    
    // If arrays are small enough, use insertion-based merge
    auto left_size = middle - first;
    auto right_size = last - middle;
    if (left_size + right_size <= 32) {
        // Simple insertion-based merge for small arrays
        Iterator left = first;
        Iterator right = middle;
        
        while (left < middle && right < last) {
            if (!comp(*right, *left)) {
                // left element is in correct position
                ++left;
            } else {
                // right element needs to be inserted into left part
                auto value = fl::move(*right);
                Iterator shift_end = right;
                Iterator shift_start = left;
                
                // Shift elements to make room
                while (shift_end > shift_start) {
                    *shift_end = fl::move(*(shift_end - 1));
                    --shift_end;
                }
                
                *left = fl::move(value);
                ++left;
                ++middle;  // middle has shifted right
                ++right;
            }
        }
        return;
    }
    
    // For larger arrays, use rotation-based merge
    if (left_size == 0 || right_size == 0) {
        return;
    }
    
    if (left_size == 1) {
        // Find insertion point for the single left element in right array
        Iterator pos = lower_bound_impl(middle, last, *first, comp);
        rotate_impl(first, middle, pos);
        return;
    }
    
    if (right_size == 1) {
        // Find insertion point for the single right element in left array  
        Iterator pos = lower_bound_impl(first, middle, *(last - 1), comp);
        rotate_impl(pos, middle, last);
        return;
    }
    
    // Divide both arrays and recursively merge
    Iterator left_mid = first + left_size / 2;
    Iterator right_mid = lower_bound_impl(middle, last, *left_mid, comp);
    
    // Rotate to bring the two middle parts together
    rotate_impl(left_mid, middle, right_mid);
    
    // Update middle position
    Iterator new_middle = left_mid + (right_mid - middle);
    
    // Recursively merge the two parts
    merge_inplace(first, left_mid, new_middle, comp);
    merge_inplace(new_middle, right_mid, last, comp);
}

// Merge sort implementation (stable, in-place)
template <typename Iterator, typename Compare>
void mergesort_impl(Iterator first, Iterator last, Compare comp) {
    auto size = last - first;
    if (size <= 16) {  // Use insertion sort for small arrays (it's stable)
        insertion_sort(first, last, comp);
        return;
    }
    
    Iterator middle = first + size / 2;
    mergesort_impl(first, middle, comp);
    mergesort_impl(middle, last, comp);
    merge_inplace(first, middle, last, comp);
}

} // namespace detail

// Sort function with custom comparator (using quicksort)
template <typename Iterator, typename Compare>
void sort(Iterator first, Iterator last, Compare comp) {
    if (first == last || first + 1 == last) {
        return;  // Already sorted or empty
    }
    
    detail::quicksort_impl(first, last, comp);
}

// Sort function with default comparator
template <typename Iterator>
void sort(Iterator first, Iterator last) {
    // Use explicit template parameter to avoid C++14 auto in lambda
    typedef typename fl::remove_reference<decltype(*first)>::type value_type;
    sort(first, last, [](const value_type& a, const value_type& b) { return a < b; });
}

// Stable sort function with custom comparator (using merge sort)
template <typename Iterator, typename Compare>
void stable_sort(Iterator first, Iterator last, Compare comp) {
    if (first == last || first + 1 == last) {
        return;  // Already sorted or empty
    }
    
    detail::mergesort_impl(first, last, comp);
}

// Stable sort function with default comparator
template <typename Iterator>
void stable_sort(Iterator first, Iterator last) {
    // Use explicit template parameter to avoid C++14 auto in lambda
    typedef typename fl::remove_reference<decltype(*first)>::type value_type;
    stable_sort(first, last, [](const value_type& a, const value_type& b) { return a < b; });
}

// Shuffle function with custom random generator (Fisher-Yates shuffle)
template <typename Iterator, typename RandomGenerator>
void shuffle(Iterator first, Iterator last, RandomGenerator& g) {
    if (first == last) {
        return;  // Empty range, nothing to shuffle
    }
    
    auto n = last - first;
    for (auto i = n - 1; i > 0; --i) {
        // Generate random index from 0 to i (inclusive)
        auto j = g() % (i + 1);
        
        // Swap elements at positions i and j
        swap(*(first + i), *(first + j));
    }
}

// Shuffle function with fl::fl_random instance
template <typename Iterator>
void shuffle(Iterator first, Iterator last, fl_random& rng) {
    if (first == last) {
        return;  // Empty range, nothing to shuffle
    }
    
    auto n = last - first;
    for (auto i = n - 1; i > 0; --i) {
        // Generate random index from 0 to i (inclusive)
        auto j = rng(static_cast<u32>(i + 1));
        
        // Swap elements at positions i and j
        swap(*(first + i), *(first + j));
    }
}

// Shuffle function with default random generator
template <typename Iterator>
void shuffle(Iterator first, Iterator last) {
    shuffle(first, last, default_random());
}

} // namespace fl
