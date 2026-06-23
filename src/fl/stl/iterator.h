#pragma once

#include "fl/stl/move.h"
#include "fl/stl/noexcept.h"

namespace fl {

// Iterator category tags
struct input_iterator_tag {};
struct output_iterator_tag {};
struct forward_iterator_tag : public input_iterator_tag {};
struct bidirectional_iterator_tag : public forward_iterator_tag {};
struct random_access_iterator_tag : public bidirectional_iterator_tag {};

/// Iterator traits - provides standard typedefs for any iterator type
/// Specializations provided for raw pointers
/// Generic version - tries to extract nested typedefs, provides defaults if missing
template<typename Iterator, typename = void>
struct iterator_traits {
    // Try to use nested typedefs if they exist, otherwise provide defaults
    typedef typename Iterator::value_type value_type;
    typedef typename Iterator::reference reference;
    typedef typename Iterator::pointer pointer;
    typedef typename Iterator::difference_type difference_type;
    // iterator_category is optional - provide a default forward_iterator_tag if missing
    typedef forward_iterator_tag iterator_category;
};

/// Specialization for raw pointers
template<typename T>
struct iterator_traits<T*> {
    typedef T value_type;
    typedef T& reference;
    typedef T* pointer;
    typedef ptrdiff_t difference_type;
    typedef random_access_iterator_tag iterator_category;
};

/// Specialization for const raw pointers
template<typename T>
struct iterator_traits<const T*> {
    typedef T value_type;
    typedef const T& reference;
    typedef const T* pointer;
    typedef ptrdiff_t difference_type;
    typedef random_access_iterator_tag iterator_category;
};

/// Back insert iterator - an output iterator that inserts elements at the end of a container
///
/// This is similar to std::back_insert_iterator and provides a convenient way to insert elements
/// into containers that support push_back() (like fl::vector, fl::FixedVector, fl::InlinedVector).
///
/// Usage example:
/// @code
///     fl::vector<int> vec;
///     auto inserter = fl::back_inserter(vec);
///     *inserter++ = 10;
///     *inserter++ = 20;
///     *inserter++ = 30;
///     // vec now contains [10, 20, 30]
/// @endcode
///
/// Or use in algorithm-style iteration:
/// @code
///     fl::vector<int> source = {1, 2, 3, 4, 5};
///     fl::vector<int> dest;
///     auto inserter = fl::back_inserter(dest);
///     for (auto it = source.begin(); it != source.end(); ++it) {
///         *inserter++ = *it;
///     }
///     // dest now contains [1, 2, 3, 4, 5]
/// @endcode
template <typename Container>
class back_insert_iterator {
protected:
    Container* container;

public:
    // Iterator traits
    typedef Container container_type;
    typedef typename Container::value_type container_value_type;
    typedef void value_type;
    typedef void difference_type;
    typedef void pointer;
    typedef void reference;
    typedef void iterator_category;  // Output iterator

    // Constructor
    explicit back_insert_iterator(Container& c) FL_NOEXCEPT : container(&c) {}

    // Assignment operator - calls push_back on the container
    // Uses template to accept any type that the container's push_back accepts
    template <typename T>
    back_insert_iterator& operator=(const T& value) FL_NOEXCEPT {
        container->push_back(value);
        return *this;
    }

    // Move assignment operator - calls push_back with move
    // Uses template to accept any type that the container's push_back accepts
    template <typename T>
    back_insert_iterator& operator=(T&& value) FL_NOEXCEPT {
        container->push_back(fl::move(value));
        return *this;
    }

    // Dereference operator (no-op for output iterator)
    back_insert_iterator& operator*() FL_NOEXCEPT {
        return *this;
    }

    // Pre-increment operator (no-op for output iterator)
    back_insert_iterator& operator++() FL_NOEXCEPT {
        return *this;
    }

    // Post-increment operator (no-op for output iterator)
    back_insert_iterator operator++(int) FL_NOEXCEPT {
        return *this;
    }
};

/// Helper function to create a back_insert_iterator
///
/// This function template creates a back_insert_iterator for the given container,
/// providing a more concise syntax than explicitly constructing the iterator.
///
/// @param c Reference to the container to insert into (must have push_back() method)
/// @return A back_insert_iterator that will insert elements into the container
///
/// Example:
/// @code
///     fl::vector<int> vec;
///     auto it = fl::back_inserter(vec);
///     *it = 42;  // vec now contains [42]
/// @endcode
template <typename Container>
back_insert_iterator<Container> back_inserter(Container& c) FL_NOEXCEPT {
    return back_insert_iterator<Container>(c);
}

/// Reverse iterator adapter - reverses the direction of a bidirectional iterator
///
/// This adapter wraps any bidirectional iterator and reverses its direction.
/// Incrementing a reverse_iterator moves backwards through the sequence,
/// and dereferencing returns the element before the current position.
///
/// Usage example:
/// @code
///     fl::vector<int> vec = {1, 2, 3, 4, 5};
///     auto rit = fl::reverse_iterator<fl::vector<int>::iterator>(vec.end());
///     auto rend = fl::reverse_iterator<fl::vector<int>::iterator>(vec.begin());
///     while (rit != rend) {
///         // Iterates: 5, 4, 3, 2, 1
///         ++rit;
///     }
/// @endcode
template <typename Iterator>
class reverse_iterator {
public:
    typedef Iterator iterator_type;
    typedef typename Iterator::value_type value_type;
    typedef typename Iterator::reference reference;
    typedef typename Iterator::pointer pointer;
    typedef typename Iterator::difference_type difference_type;

protected:
    Iterator current;

public:
    // Constructors
    reverse_iterator() FL_NOEXCEPT : current() {}

    explicit reverse_iterator(Iterator it) FL_NOEXCEPT : current(it) {}

    template <typename U>
    reverse_iterator(const reverse_iterator<U>& other) FL_NOEXCEPT : current(other.base()) {}

    // Access to underlying iterator
    Iterator base() const FL_NOEXCEPT { return current; }

    // Dereference - returns element before current position
    reference operator*() const FL_NOEXCEPT {
        Iterator tmp = current;
        --tmp;
        return *tmp;
    }

    pointer operator->() const FL_NOEXCEPT {
        Iterator tmp = current;
        --tmp;
        return &(*tmp);
    }

    // Pre-increment - moves backwards
    reverse_iterator& operator++() FL_NOEXCEPT {
        --current;
        return *this;
    }

    // Post-increment
    reverse_iterator operator++(int) FL_NOEXCEPT {
        reverse_iterator tmp = *this;
        --current;
        return tmp;
    }

    // Pre-decrement - moves forwards
    reverse_iterator& operator--() FL_NOEXCEPT {
        ++current;
        return *this;
    }

    // Post-decrement
    reverse_iterator operator--(int) FL_NOEXCEPT {
        reverse_iterator tmp = *this;
        ++current;
        return tmp;
    }

    // Random access operators
    reverse_iterator operator+(difference_type n) const FL_NOEXCEPT {
        return reverse_iterator(current - n);
    }

    reverse_iterator operator-(difference_type n) const FL_NOEXCEPT {
        return reverse_iterator(current + n);
    }

    reverse_iterator& operator+=(difference_type n) FL_NOEXCEPT {
        current -= n;
        return *this;
    }

    reverse_iterator& operator-=(difference_type n) FL_NOEXCEPT {
        current += n;
        return *this;
    }

    difference_type operator-(const reverse_iterator& other) const FL_NOEXCEPT {
        return other.current - current;
    }

    reference operator[](difference_type n) const FL_NOEXCEPT {
        return *(*this + n);
    }

    // Comparison operators
    bool operator==(const reverse_iterator& other) const FL_NOEXCEPT {
        return current == other.current;
    }

    bool operator!=(const reverse_iterator& other) const FL_NOEXCEPT {
        return current != other.current;
    }

    bool operator<(const reverse_iterator& other) const FL_NOEXCEPT {
        return current > other.current;
    }

    bool operator>(const reverse_iterator& other) const FL_NOEXCEPT {
        return current < other.current;
    }

    bool operator<=(const reverse_iterator& other) const FL_NOEXCEPT {
        return current >= other.current;
    }

    bool operator>=(const reverse_iterator& other) const FL_NOEXCEPT {
        return current <= other.current;
    }

    // For comparing with different iterator types
    template <typename U>
    bool operator==(const reverse_iterator<U>& other) const FL_NOEXCEPT {
        return current == other.base();
    }

    template <typename U>
    bool operator!=(const reverse_iterator<U>& other) const FL_NOEXCEPT {
        return current != other.base();
    }
};

} // namespace fl
