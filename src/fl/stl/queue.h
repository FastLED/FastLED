#pragma once

#include "fl/stl/deque.h"
#include "fl/stl/move.h"
namespace fl {

/// @brief A first-in, first-out (FIFO) queue container adapter
/// 
/// This queue is implemented as a container adapter that uses fl::deque as the
/// underlying container by default. It provides standard queue operations: push 
/// elements to the back and pop elements from the front.
///
/// @tparam T The type of elements stored in the queue
/// @tparam Container The underlying container type (defaults to fl::deque<T>)
template <typename T, typename Container = fl::deque<T>>
class queue {
public:
    using value_type = T;
    using size_type = fl::size;
    using reference = T&;
    using const_reference = const T&;
    using container_type = Container;

private:
    Container mContainer;

public:
    /// @brief Default constructor - creates an empty queue
    queue() = default;

    /// @brief Construct queue with a copy of the given container
    /// @param container Container to copy
    explicit queue(const Container& container) : mContainer(container) {}

    /// @brief Construct queue by moving the given container
    /// @param container Container to move
    explicit queue(Container&& container) : mContainer(fl::move(container)) {}

    /// @brief Copy constructor
    /// @param other Queue to copy
    queue(const queue& other) = default;

    /// @brief Move constructor
    /// @param other Queue to move
    queue(queue&& other) = default;

    /// @brief Copy assignment operator
    /// @param other Queue to copy
    /// @return Reference to this queue
    queue& operator=(const queue& other) = default;

    /// @brief Move assignment operator
    /// @param other Queue to move
    /// @return Reference to this queue
    queue& operator=(queue&& other) = default;

    /// @brief Destructor
    ~queue() = default;

    /// @brief Access the first element (front of queue)
    /// @return Reference to the front element
    /// @note Undefined behavior if queue is empty
    reference front() {
        return mContainer.front();
    }

    /// @brief Access the first element (front of queue) - const version
    /// @return Const reference to the front element
    /// @note Undefined behavior if queue is empty
    const_reference front() const {
        return mContainer.front();
    }

    /// @brief Access the last element (back of queue)
    /// @return Reference to the back element
    /// @note Undefined behavior if queue is empty
    reference back() {
        return mContainer.back();
    }

    /// @brief Access the last element (back of queue) - const version
    /// @return Const reference to the back element
    /// @note Undefined behavior if queue is empty
    const_reference back() const {
        return mContainer.back();
    }

    /// @brief Check if the queue is empty
    /// @return True if queue is empty, false otherwise
    bool empty() const {
        return mContainer.empty();
    }

    /// @brief Get the number of elements in the queue
    /// @return Number of elements
    size_type size() const {
        return mContainer.size();
    }

    /// @brief Add an element to the back of the queue
    /// @param value Element to add
    void push(const value_type& value) {
        mContainer.push_back(value);
    }

    /// @brief Add an element to the back of the queue (move version)
    /// @param value Element to move and add
    void push(value_type&& value) {
        mContainer.push_back(fl::move(value));
    }

    /// @brief Construct and add an element to the back of the queue
    /// @tparam Args Argument types for element construction
    /// @param args Arguments to forward to element constructor
    template<typename... Args>
    void emplace(Args&&... args) {
        mContainer.emplace_back(fl::forward<Args>(args)...);
    }

    /// @brief Remove the front element from the queue
    /// @note Undefined behavior if queue is empty
    void pop() {
        mContainer.pop_front();
    }

    /// @brief Swap the contents with another queue
    /// @param other Queue to swap with
    void swap(queue& other) {
        mContainer.swap(other.mContainer);
    }

    /// @brief Remove all elements from the queue
    void clear() {
        mContainer.clear();
    }

    /// @brief Get access to the underlying container (for advanced use)
    /// @return Reference to the underlying container
    Container& get_container() {
        return mContainer;
    }

    /// @brief Get access to the underlying container (const version)
    /// @return Const reference to the underlying container
    const Container& get_container() const {
        return mContainer;
    }

    /// @brief Equality comparison
    /// @param other Queue to compare with
    /// @return true if both queues contain the same elements in the same order
    bool operator==(const queue& other) const {
        return mContainer == other.mContainer;
    }

    /// @brief Inequality comparison
    /// @param other Queue to compare with
    /// @return true if queues are not equal
    bool operator!=(const queue& other) const {
        return mContainer != other.mContainer;
    }

    /// @brief Less-than comparison (lexicographic)
    /// @param other Queue to compare with
    /// @return true if this queue is lexicographically less than other
    bool operator<(const queue& other) const {
        return mContainer < other.mContainer;
    }

    /// @brief Less-than-or-equal comparison
    /// @param other Queue to compare with
    /// @return true if this queue is lexicographically less than or equal to other
    bool operator<=(const queue& other) const {
        return mContainer <= other.mContainer;
    }

    /// @brief Greater-than comparison
    /// @param other Queue to compare with
    /// @return true if this queue is lexicographically greater than other
    bool operator>(const queue& other) const {
        return mContainer > other.mContainer;
    }

    /// @brief Greater-than-or-equal comparison
    /// @param other Queue to compare with
    /// @return true if this queue is lexicographically greater than or equal to other
    bool operator>=(const queue& other) const {
        return mContainer >= other.mContainer;
    }
};

/// @brief Swap two queues
/// @tparam T Element type
/// @tparam Container Container type
/// @param lhs First queue
/// @param rhs Second queue
template <typename T, typename Container>
void swap(queue<T, Container>& lhs, queue<T, Container>& rhs) {
    lhs.swap(rhs);
}

/// @brief Non-member equality comparison for queues
template <typename T, typename Container>
bool operator==(const queue<T, Container>& lhs, const queue<T, Container>& rhs) {
    return lhs.get_container() == rhs.get_container();
}

/// @brief Non-member inequality comparison for queues
template <typename T, typename Container>
bool operator!=(const queue<T, Container>& lhs, const queue<T, Container>& rhs) {
    return lhs.get_container() != rhs.get_container();
}

/// @brief Non-member less-than comparison for queues
template <typename T, typename Container>
bool operator<(const queue<T, Container>& lhs, const queue<T, Container>& rhs) {
    return lhs.get_container() < rhs.get_container();
}

/// @brief Non-member less-than-or-equal comparison for queues
template <typename T, typename Container>
bool operator<=(const queue<T, Container>& lhs, const queue<T, Container>& rhs) {
    return lhs.get_container() <= rhs.get_container();
}

/// @brief Non-member greater-than comparison for queues
template <typename T, typename Container>
bool operator>(const queue<T, Container>& lhs, const queue<T, Container>& rhs) {
    return lhs.get_container() > rhs.get_container();
}

/// @brief Non-member greater-than-or-equal comparison for queues
template <typename T, typename Container>
bool operator>=(const queue<T, Container>& lhs, const queue<T, Container>& rhs) {
    return lhs.get_container() >= rhs.get_container();
}

} // namespace fl 
