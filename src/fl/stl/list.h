#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/allocator.h"
#include "fl/stl/initializer_list.h"
#include "fl/stl/new.h"
#include "fl/stl/move.h"
#include "fl/stl/type_traits.h"

namespace fl {

/// @brief A doubly-linked list container
///
/// This list is implemented as a doubly-linked list with sentinel nodes.
/// It provides constant time insertion and deletion at any position when
/// you have an iterator to that position.
///
/// @tparam T The type of elements stored in the list
/// @tparam Allocator The allocator type for memory management
template <typename T, typename Allocator = fl::allocator<T>>
class list {
private:
    struct Node {
        T data;
        Node* next;
        Node* prev;

        template<typename... Args>
        Node(Args&&... args) : data(fl::forward<Args>(args)...), next(nullptr), prev(nullptr) {}
    };

    // Node allocator (rebind T allocator to Node allocator)
    using NodeAllocator = typename Allocator::template rebind<Node>::other;

    Node* mHead;  // Sentinel node (circular list)
    fl::size mSize;
    NodeAllocator mAlloc;

    void init_sentinel() {
        mHead = mAlloc.allocate(1);
        mHead->next = mHead;
        mHead->prev = mHead;
        mSize = 0;
    }

    void destroy_sentinel() {
        if (mHead) {
            mAlloc.deallocate(mHead, 1);
            mHead = nullptr;
        }
    }

    Node* create_node(const T& value) {
        Node* node = mAlloc.allocate(1);
        mAlloc.construct(node, value);
        return node;
    }

    Node* create_node(T&& value) {
        Node* node = mAlloc.allocate(1);
        mAlloc.construct(node, fl::move(value));
        return node;
    }

    void destroy_node(Node* node) {
        if (node && node != mHead) {
            mAlloc.destroy(node);
            mAlloc.deallocate(node, 1);
        }
    }

    void link_before(Node* pos, Node* node) {
        node->next = pos;
        node->prev = pos->prev;
        pos->prev->next = node;
        pos->prev = node;
    }

    void unlink(Node* node) {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }

public:
    // Iterator implementation
    class iterator {
    private:
        Node* mNode;
        friend class list;

    public:
        iterator(Node* node) : mNode(node) {}

        T& operator*() const {
            return mNode->data;
        }

        T* operator->() const {
            return &mNode->data;
        }

        iterator& operator++() {
            mNode = mNode->next;
            return *this;
        }

        iterator operator++(int) {
            iterator temp = *this;
            mNode = mNode->next;
            return temp;
        }

        iterator& operator--() {
            mNode = mNode->prev;
            return *this;
        }

        iterator operator--(int) {
            iterator temp = *this;
            mNode = mNode->prev;
            return temp;
        }

        bool operator==(const iterator& other) const {
            return mNode == other.mNode;
        }

        bool operator!=(const iterator& other) const {
            return mNode != other.mNode;
        }
    };

    class const_iterator {
    private:
        const Node* mNode;
        friend class list;

    public:
        const_iterator(const Node* node) : mNode(node) {}
        const_iterator(const iterator& it) : mNode(it.mNode) {}

        const T& operator*() const {
            return mNode->data;
        }

        const T* operator->() const {
            return &mNode->data;
        }

        const_iterator& operator++() {
            mNode = mNode->next;
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator temp = *this;
            mNode = mNode->next;
            return temp;
        }

        const_iterator& operator--() {
            mNode = mNode->prev;
            return *this;
        }

        const_iterator operator--(int) {
            const_iterator temp = *this;
            mNode = mNode->prev;
            return temp;
        }

        bool operator==(const const_iterator& other) const {
            return mNode == other.mNode;
        }

        bool operator!=(const const_iterator& other) const {
            return mNode != other.mNode;
        }
    };

    // Constructors
    list() {
        init_sentinel();
    }

    explicit list(fl::size count, const T& value = T()) {
        init_sentinel();
        for (fl::size i = 0; i < count; ++i) {
            push_back(value);
        }
    }

    list(const list& other) {
        init_sentinel();
        for (const auto& value : other) {
            push_back(value);
        }
    }

    list(list&& other) {
        init_sentinel();
        swap(other);
    }

    list(fl::initializer_list<T> init) {
        init_sentinel();
        for (const auto& value : init) {
            push_back(value);
        }
    }

    // Destructor
    ~list() {
        clear();
        destroy_sentinel();
    }

    // Assignment operators
    list& operator=(const list& other) {
        if (this != &other) {
            clear();
            for (const auto& value : other) {
                push_back(value);
            }
        }
        return *this;
    }

    list& operator=(list&& other) {
        if (this != &other) {
            clear();
            swap(other);
        }
        return *this;
    }

    // Element access
    T& front() {
        return mHead->next->data;
    }

    const T& front() const {
        return mHead->next->data;
    }

    T& back() {
        return mHead->prev->data;
    }

    const T& back() const {
        return mHead->prev->data;
    }

    // Iterators
    iterator begin() {
        return iterator(mHead->next);
    }

    const_iterator begin() const {
        return const_iterator(mHead->next);
    }

    iterator end() {
        return iterator(mHead);
    }

    const_iterator end() const {
        return const_iterator(mHead);
    }

    // Capacity
    bool empty() const {
        return mSize == 0;
    }

    fl::size size() const {
        return mSize;
    }

    // Modifiers
    void clear() {
        while (!empty()) {
            pop_back();
        }
    }

    iterator insert(iterator pos, const T& value) {
        Node* new_node = create_node(value);
        link_before(pos.mNode, new_node);
        ++mSize;
        return iterator(new_node);
    }

    iterator insert(iterator pos, T&& value) {
        Node* new_node = create_node(fl::move(value));
        link_before(pos.mNode, new_node);
        ++mSize;
        return iterator(new_node);
    }

    iterator erase(iterator pos) {
        if (pos == end()) {
            return end();
        }
        Node* node = pos.mNode;
        Node* next_node = node->next;
        unlink(node);
        destroy_node(node);
        --mSize;
        return iterator(next_node);
    }

    iterator erase(iterator first, iterator last) {
        while (first != last) {
            first = erase(first);
        }
        return last;
    }

    void push_back(const T& value) {
        insert(end(), value);
    }

    void push_back(T&& value) {
        insert(end(), fl::move(value));
    }

    void push_front(const T& value) {
        insert(begin(), value);
    }

    void push_front(T&& value) {
        insert(begin(), fl::move(value));
    }

    void pop_back() {
        if (!empty()) {
            erase(iterator(mHead->prev));
        }
    }

    void pop_front() {
        if (!empty()) {
            erase(begin());
        }
    }

    void resize(fl::size count) {
        resize(count, T());
    }

    void resize(fl::size count, const T& value) {
        while (mSize > count) {
            pop_back();
        }
        while (mSize < count) {
            push_back(value);
        }
    }

    void swap(list& other) {
        if (this != &other) {
            Node* temp_head = mHead;
            fl::size temp_size = mSize;
            NodeAllocator temp_alloc = mAlloc;

            mHead = other.mHead;
            mSize = other.mSize;
            mAlloc = other.mAlloc;

            other.mHead = temp_head;
            other.mSize = temp_size;
            other.mAlloc = temp_alloc;
        }
    }

    // Operations
    void remove(const T& value) {
        iterator it = begin();
        while (it != end()) {
            if (*it == value) {
                it = erase(it);
            } else {
                ++it;
            }
        }
    }

    template<typename Predicate>
    void remove_if(Predicate pred) {
        iterator it = begin();
        while (it != end()) {
            if (pred(*it)) {
                it = erase(it);
            } else {
                ++it;
            }
        }
    }

    void reverse() {
        if (mSize <= 1) {
            return;
        }

        Node* current = mHead;
        do {
            Node* temp = current->next;
            current->next = current->prev;
            current->prev = temp;
            current = current->prev; // Move to next node (which is now prev)
        } while (current != mHead);
    }

    void unique() {
        if (mSize <= 1) {
            return;
        }

        iterator it = begin();
        while (it != end()) {
            iterator next = it;
            ++next;
            if (next != end() && *it == *next) {
                erase(next);
            } else {
                ++it;
            }
        }
    }

    void sort() {
        // Insertion sort for simplicity (appropriate for embedded systems)
        if (mSize <= 1) {
            return;
        }

        Node* sorted_end = mHead->next;

        while (sorted_end->next != mHead) {
            Node* current = sorted_end->next;

            // Find insertion point in sorted portion
            Node* insert_pos = mHead->next;
            while (insert_pos != sorted_end->next && insert_pos->data < current->data) {
                insert_pos = insert_pos->next;
            }

            // If current is already in correct position
            if (insert_pos == current) {
                sorted_end = current;
            } else {
                // Remove current from its position
                unlink(current);
                // Insert before insert_pos
                link_before(insert_pos, current);
            }
        }
    }

    template<typename Compare>
    void sort(Compare comp) {
        // Insertion sort with custom comparator
        if (mSize <= 1) {
            return;
        }

        Node* sorted_end = mHead->next;

        while (sorted_end->next != mHead) {
            Node* current = sorted_end->next;

            // Find insertion point in sorted portion
            Node* insert_pos = mHead->next;
            while (insert_pos != sorted_end->next && comp(insert_pos->data, current->data)) {
                insert_pos = insert_pos->next;
            }

            // If current is already in correct position
            if (insert_pos == current) {
                sorted_end = current;
            } else {
                // Remove current from its position
                unlink(current);
                // Insert before insert_pos
                link_before(insert_pos, current);
            }
        }
    }

    void splice(iterator pos, list& other) {
        if (other.empty()) {
            return;
        }
        splice(pos, other, other.begin(), other.end());
    }

    void splice(iterator pos, list& other, iterator it) {
        iterator next = it;
        ++next;
        splice(pos, other, it, next);
    }

    void splice(iterator pos, list& other, iterator first, iterator last) {
        if (first == last) {
            return;
        }

        // Count elements being moved
        fl::size count = 0;
        for (iterator it = first; it != last; ++it) {
            ++count;
        }

        // Unlink range from other list
        Node* first_node = first.mNode;
        Node* last_node = last.mNode;
        Node* before_first = first_node->prev;
        Node* after_last = last_node;
        Node* last_in_range = last_node->prev;

        // Connect the gap in other list
        before_first->next = after_last;
        after_last->prev = before_first;

        // Insert range before pos
        Node* pos_node = pos.mNode;
        Node* before_pos = pos_node->prev;

        before_pos->next = first_node;
        first_node->prev = before_pos;
        last_in_range->next = pos_node;
        pos_node->prev = last_in_range;

        // Update sizes
        mSize += count;
        other.mSize -= count;
    }

    // Find operation (non-standard but useful)
    iterator find(const T& value) {
        for (iterator it = begin(); it != end(); ++it) {
            if (*it == value) {
                return it;
            }
        }
        return end();
    }

    const_iterator find(const T& value) const {
        for (const_iterator it = begin(); it != end(); ++it) {
            if (*it == value) {
                return it;
            }
        }
        return end();
    }

    bool has(const T& value) const {
        return find(value) != end();
    }
};

// Swap function (non-member)
template <typename T, typename Allocator>
void swap(list<T, Allocator>& lhs, list<T, Allocator>& rhs) {
    lhs.swap(rhs);
}

} // namespace fl
