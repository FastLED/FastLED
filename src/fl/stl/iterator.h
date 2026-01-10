#pragma once

#include "fl/stl/move.h"

namespace fl {

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
    typedef void value_type;
    typedef void difference_type;
    typedef void pointer;
    typedef void reference;
    typedef void iterator_category;  // Output iterator

    // Constructor
    explicit back_insert_iterator(Container& c) : container(&c) {}

    // Assignment operator - calls push_back on the container
    // Uses template to accept any type that the container's push_back accepts
    template <typename T>
    back_insert_iterator& operator=(const T& value) {
        container->push_back(value);
        return *this;
    }

    // Move assignment operator - calls push_back with move
    // Uses template to accept any type that the container's push_back accepts
    template <typename T>
    back_insert_iterator& operator=(T&& value) {
        container->push_back(fl::move(value));
        return *this;
    }

    // Dereference operator (no-op for output iterator)
    back_insert_iterator& operator*() {
        return *this;
    }

    // Pre-increment operator (no-op for output iterator)
    back_insert_iterator& operator++() {
        return *this;
    }

    // Post-increment operator (no-op for output iterator)
    back_insert_iterator operator++(int) {
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
back_insert_iterator<Container> back_inserter(Container& c) {
    return back_insert_iterator<Container>(c);
}

} // namespace fl
