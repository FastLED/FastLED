#pragma once

#include "fl/type_traits.h"

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

} // namespace fl
