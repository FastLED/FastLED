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



} // namespace fl
