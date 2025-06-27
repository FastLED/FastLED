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

// Merge operation for merge sort (stable sort)
template <typename Iterator, typename Compare>
void merge(Iterator first, Iterator middle, Iterator last, Compare comp) {
    using value_type = typename fl::remove_reference<decltype(*first)>::type;
    
    // Calculate sizes
    auto left_size = middle - first;
    auto right_size = last - middle;
    
    // Create temporary buffer for left half
    value_type* temp = static_cast<value_type*>(operator new(left_size * sizeof(value_type)));
    
    // Copy left half to temporary buffer
    for (auto i = 0; i < left_size; ++i) {
        new (temp + i) value_type(fl::move(*(first + i)));
    }
    
    Iterator left = first;
    Iterator right = middle;
    Iterator result = first;
    auto temp_left = temp;
    auto temp_left_end = temp + left_size;
    
    // Merge the two halves back
    while (temp_left != temp_left_end && right != last) {
        if (!comp(*right, *temp_left)) {  // Use !comp for stability
            *result = fl::move(*temp_left);
            ++temp_left;
        } else {
            *result = fl::move(*right);
            ++right;
        }
        ++result;
    }
    
    // Copy remaining elements from left half
    while (temp_left != temp_left_end) {
        *result = fl::move(*temp_left);
        ++temp_left;
        ++result;
    }
    
    // Right half is already in place, no need to copy
    
    // Clean up temporary buffer
    for (auto i = 0; i < left_size; ++i) {
        temp[i].~value_type();
    }
    operator delete(temp);
}

// Merge sort implementation (stable)
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
    merge(first, middle, last, comp);
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

} // namespace fl
