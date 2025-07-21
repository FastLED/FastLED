#pragma once

#include "fl/deque.h"
#include "fl/move.h"
#include "fl/namespace.h"

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

} // namespace fl 
