#pragma once

#include "fl/stl/algorithm.h"
#include "fl/stl/allocator.h"
#include "fl/stl/assert.h"
#include "fl/stl/comparators.h"
#include "fl/stl/initializer_list.h"
#include "fl/stl/pair.h"
#include "fl/stl/type_traits.h"

namespace fl {

// Generic Red-Black Tree implementation
// This is a self-balancing binary search tree with O(log n) operations
// T is the value type stored in the tree
// Compare is a comparator that can compare T values
template <typename T, typename Compare = less<T>, typename Allocator = allocator_slab<char>>
class RedBlackTree {
public:
    class iterator;
    class const_iterator;
    class reverse_iterator;
    class const_reverse_iterator;
    using value_type = T;
    using size_type = fl::size;
    using difference_type = ptrdiff_t;
    using compare_type = Compare;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using allocator_type = Allocator;

    // Red-Black Tree colors
    enum class Color { kRed, kBlack };

private:
    struct RBNode {
        value_type data;
        Color color;
        RBNode* left;
        RBNode* right;
        RBNode* parent;

        RBNode(const value_type& val, Color c = Color::kRed, RBNode* p = nullptr)
            : data(val), color(c), left(nullptr), right(nullptr), parent(p) {}
        
        template<typename... Args>
        RBNode(Color c, RBNode* p, Args&&... args)
            : data(fl::forward<Args>(args)...), color(c), left(nullptr), right(nullptr), parent(p) {}
    };

    using NodeAllocator = typename Allocator::template rebind<RBNode>::other;

    RBNode* root_;
    fl::size mSize;
    Compare mComp;
    NodeAllocator mAlloc;

    // Helper methods
    void rotateLeft(RBNode* x) {
        RBNode* y = x->right;
        x->right = y->left;
        if (y->left != nullptr) {
            y->left->parent = x;
        }
        y->parent = x->parent;
        if (x->parent == nullptr) {
            root_ = y;
        } else if (x == x->parent->left) {
            x->parent->left = y;
        } else {
            x->parent->right = y;
        }
        y->left = x;
        x->parent = y;
    }

    void rotateRight(RBNode* x) {
        RBNode* y = x->left;
        x->left = y->right;
        if (y->right != nullptr) {
            y->right->parent = x;
        }
        y->parent = x->parent;
        if (x->parent == nullptr) {
            root_ = y;
        } else if (x == x->parent->right) {
            x->parent->right = y;
        } else {
            x->parent->left = y;
        }
        y->right = x;
        x->parent = y;
    }

    void insertFixup(RBNode* z) {
        while (z->parent != nullptr && z->parent->parent != nullptr && z->parent->color == Color::kRed) {
            if (z->parent == z->parent->parent->left) {
                RBNode* y = z->parent->parent->right;
                if (y != nullptr && y->color == Color::kRed) {
                    z->parent->color = Color::kBlack;
                    y->color = Color::kBlack;
                    z->parent->parent->color = Color::kRed;
                    z = z->parent->parent;
                } else {
                    if (z == z->parent->right) {
                        z = z->parent;
                        rotateLeft(z);
                    }
                    z->parent->color = Color::kBlack;
                    z->parent->parent->color = Color::kRed;
                    rotateRight(z->parent->parent);
                }
            } else {
                RBNode* y = z->parent->parent->left;
                if (y != nullptr && y->color == Color::kRed) {
                    z->parent->color = Color::kBlack;
                    y->color = Color::kBlack;
                    z->parent->parent->color = Color::kRed;
                    z = z->parent->parent;
                } else {
                    if (z == z->parent->left) {
                        z = z->parent;
                        rotateRight(z);
                    }
                    z->parent->color = Color::kBlack;
                    z->parent->parent->color = Color::kRed;
                    rotateLeft(z->parent->parent);
                }
            }
        }
        root_->color = Color::kBlack;
    }

    void transplant(RBNode* u, RBNode* v) {
        if (u->parent == nullptr) {
            root_ = v;
        } else if (u == u->parent->left) {
            u->parent->left = v;
        } else {
            u->parent->right = v;
        }
        if (v != nullptr) {
            v->parent = u->parent;
        }
    }

    RBNode* minimum(RBNode* x) const {
        while (x->left != nullptr) {
            x = x->left;
        }
        return x;
    }

    RBNode* maximum(RBNode* x) const {
        while (x->right != nullptr) {
            x = x->right;
        }
        return x;
    }

    // Fixed to properly use xParent when x is nullptr; removes unused parameter warning and centralizes erase fixup
    void deleteFixup(RBNode* x, RBNode* xParent) {
        while ((x != root_) && (x == nullptr || x->color == Color::kBlack)) {
            if (x == (xParent ? xParent->left : nullptr)) {
                RBNode* w = xParent ? xParent->right : nullptr;
                if (w && w->color == Color::kRed) {
                    w->color = Color::kBlack;
                    if (xParent) { xParent->color = Color::kRed; rotateLeft(xParent); }
                    w = xParent ? xParent->right : nullptr;
                }
                bool wLeftBlack = (!w || !w->left || w->left->color == Color::kBlack);
                bool wRightBlack = (!w || !w->right || w->right->color == Color::kBlack);
                if (wLeftBlack && wRightBlack) {
                    if (w) w->color = Color::kRed;
                    x = xParent;
                    xParent = xParent ? xParent->parent : nullptr;
                } else {
                    if (!w || (w->right == nullptr || w->right->color == Color::kBlack)) {
                        if (w && w->left) w->left->color = Color::kBlack;
                        if (w) { w->color = Color::kRed; rotateRight(w); }
                        w = xParent ? xParent->right : nullptr;
                    }
                    if (w) w->color = xParent ? xParent->color : Color::kBlack;
                    if (xParent) xParent->color = Color::kBlack;
                    if (w && w->right) w->right->color = Color::kBlack;
                    if (xParent) rotateLeft(xParent);
                    x = root_;
                }
            } else {
                RBNode* w = xParent ? xParent->left : nullptr;
                if (w && w->color == Color::kRed) {
                    w->color = Color::kBlack;
                    if (xParent) { xParent->color = Color::kRed; rotateRight(xParent); }
                    w = xParent ? xParent->left : nullptr;
                }
                bool wRightBlack = (!w || !w->right || w->right->color == Color::kBlack);
                bool wLeftBlack = (!w || !w->left || w->left->color == Color::kBlack);
                if (wRightBlack && wLeftBlack) {
                    if (w) w->color = Color::kRed;
                    x = xParent;
                    xParent = xParent ? xParent->parent : nullptr;
                } else {
                    if (!w || (w->left == nullptr || w->left->color == Color::kBlack)) {
                        if (w && w->right) w->right->color = Color::kBlack;
                        if (w) { w->color = Color::kRed; rotateLeft(w); }
                        w = xParent ? xParent->left : nullptr;
                    }
                    if (w) w->color = xParent ? xParent->color : Color::kBlack;
                    if (xParent) xParent->color = Color::kBlack;
                    if (w && w->left) w->left->color = Color::kBlack;
                    if (xParent) rotateRight(xParent);
                    x = root_;
                }
            }
        }
        if (x) x->color = Color::kBlack;
    }

    RBNode* findNode(const value_type& value) const {
        RBNode* current = root_;
        while (current != nullptr) {
            if (mComp(value, current->data)) {
                current = current->left;
            } else if (mComp(current->data, value)) {
                current = current->right;
            } else {
                return current;
            }
        }
        return nullptr;
    }

    void destroyTree(RBNode* node) {
        if (node != nullptr) {
            destroyTree(node->left);
            destroyTree(node->right);
            mAlloc.destroy(node);
            mAlloc.deallocate(node, 1);
        }
    }

    RBNode* copyTree(RBNode* node, RBNode* parent = nullptr) {
        if (node == nullptr) return nullptr;

        RBNode* newNode = mAlloc.allocate(1);
        if (newNode == nullptr) {
            return nullptr;
        }

        mAlloc.construct(newNode, node->data, node->color, parent);
        newNode->left = copyTree(node->left, newNode);
        newNode->right = copyTree(node->right, newNode);
        return newNode;
    }

    // Shared insert implementation to reduce duplication
    template <typename U>
    fl::pair<iterator, bool> insertImpl(U&& value) {
        RBNode* parent = nullptr;
        RBNode* current = root_;
        
        while (current != nullptr) {
            parent = current;
            if (mComp(value, current->data)) {
                current = current->left;
            } else if (mComp(current->data, value)) {
                current = current->right;
            } else {
                return fl::pair<iterator, bool>(iterator(current, this), false);
            }
        }
        
        RBNode* newNode = mAlloc.allocate(1);
        if (newNode == nullptr) {
            return fl::pair<iterator, bool>(iterator(nullptr, this), false);
        }

        mAlloc.construct(newNode, fl::forward<U>(value), Color::kRed, parent);
        
        if (parent == nullptr) {
            root_ = newNode;
        } else if (mComp(newNode->data, parent->data)) {
            parent->left = newNode;
        } else {
            parent->right = newNode;
        }
        
        insertFixup(newNode);
        ++mSize;
        
        return fl::pair<iterator, bool>(iterator(newNode, this), true);
    }

    // Bound helpers to avoid duplication between const/non-const
    RBNode* lowerBoundNode(const value_type& value) const {
        RBNode* current = root_;
        RBNode* result = nullptr;
        while (current != nullptr) {
            if (!mComp(current->data, value)) {
                result = current;
                current = current->left;
            } else {
                current = current->right;
            }
        }
        return result;
    }

    RBNode* upperBoundNode(const value_type& value) const {
        RBNode* current = root_;
        RBNode* result = nullptr;
        while (current != nullptr) {
            if (mComp(value, current->data)) {
                result = current;
                current = current->left;
            } else {
                current = current->right;
            }
        }
        return result;
    }

public:
    // Iterator implementation
    class iterator {
        friend class RedBlackTree;
        friend class const_iterator;
    public:
        using value_type = T;
    private:
        RBNode* mNode;
        const RedBlackTree* mTree;

        RBNode* successor(RBNode* x) const {
            if (x == nullptr) return nullptr;
            if (x->right != nullptr) {
                return mTree->minimum(x->right);
            }
            RBNode* y = x->parent;
            while (y != nullptr && x == y->right) {
                x = y;
                y = y->parent;
            }
            return y;
        }

        RBNode* predecessor(RBNode* x) const {
            if (x == nullptr) return nullptr;
            if (x->left != nullptr) {
                return mTree->maximum(x->left);
            }
            RBNode* y = x->parent;
            while (y != nullptr && x == y->left) {
                x = y;
                y = y->parent;
            }
            return y;
        }

    public:
        iterator() : mNode(nullptr), mTree(nullptr) {}
        iterator(RBNode* n, const RedBlackTree* t) : mNode(n), mTree(t) {}

        value_type& operator*() const { 
            FASTLED_ASSERT(mNode != nullptr, "RedBlackTree::iterator: dereferencing end iterator");
            return mNode->data; 
        }
        value_type* operator->() const { 
            FASTLED_ASSERT(mNode != nullptr, "RedBlackTree::iterator: dereferencing end iterator");
            return &(mNode->data); 
        }

        iterator& operator++() {
            if (mNode) {
                mNode = successor(mNode);
            }
            return *this;
        }

        iterator operator++(int) {
            iterator temp = *this;
            ++(*this);
            return temp;
        }

        iterator& operator--() {
            if (mNode) {
                mNode = predecessor(mNode);
            } else if (mTree && mTree->root_) {
                mNode = mTree->maximum(mTree->root_);
            }
            return *this;
        }

        iterator operator--(int) {
            iterator temp = *this;
            --(*this);
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
        friend class RedBlackTree;
        friend class iterator;
    private:
        const RBNode* mNode;
        const RedBlackTree* mTree;

        const RBNode* successor(const RBNode* x) const {
            if (x == nullptr) return nullptr;
            if (x->right != nullptr) {
                return mTree->minimum(x->right);
            }
            const RBNode* y = x->parent;
            while (y != nullptr && x == y->right) {
                x = y;
                y = y->parent;
            }
            return y;
        }

        const RBNode* predecessor(const RBNode* x) const {
            if (x == nullptr) return nullptr;
            if (x->left != nullptr) {
                return mTree->maximum(x->left);
            }
            const RBNode* y = x->parent;
            while (y != nullptr && x == y->left) {
                x = y;
                y = y->parent;
            }
            return y;
        }

    public:
        const_iterator() : mNode(nullptr), mTree(nullptr) {}
        const_iterator(const RBNode* n, const RedBlackTree* t) : mNode(n), mTree(t) {}
        const_iterator(const iterator& it) : mNode(it.mNode), mTree(it.mTree) {}

        const value_type& operator*() const { 
            FASTLED_ASSERT(mNode != nullptr, "RedBlackTree::iterator: dereferencing end iterator");
            return mNode->data; 
        }
        const value_type* operator->() const { 
            FASTLED_ASSERT(mNode != nullptr, "RedBlackTree::iterator: dereferencing end iterator");
            return &(mNode->data); 
        }

        const_iterator& operator++() {
            if (mNode) {
                mNode = successor(mNode);
            }
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator temp = *this;
            ++(*this);
            return temp;
        }

        const_iterator& operator--() {
            if (mNode) {
                mNode = predecessor(mNode);
            } else if (mTree && mTree->root_) {
                // Decrementing from end() should give us the maximum element
                mNode = mTree->maximum(mTree->root_);
            }
            return *this;
        }

        const_iterator operator--(int) {
            const_iterator temp = *this;
            --(*this);
            return temp;
        }

        bool operator==(const const_iterator& other) const {
            return mNode == other.mNode;
        }

        bool operator!=(const const_iterator& other) const {
            return mNode != other.mNode;
        }

        // Cross-type comparison: const_iterator with iterator
        bool operator==(const iterator& other) const {
            return mNode == other.mNode;
        }

        bool operator!=(const iterator& other) const {
            return mNode != other.mNode;
        }

        // Friend functions for cross-type comparison (iterator with const_iterator)
        friend bool operator==(const iterator& lhs, const const_iterator& rhs) {
            return lhs.mNode == rhs.mNode;
        }

        friend bool operator!=(const iterator& lhs, const const_iterator& rhs) {
            return lhs.mNode != rhs.mNode;
        }
    };

    // Reverse iterator implementation
    class reverse_iterator {
        friend class RedBlackTree;
        friend class const_reverse_iterator;
    private:
        RBNode* mNode;
        const RedBlackTree* mTree;

        RBNode* successor(RBNode* x) const {
            if (x == nullptr) return nullptr;
            if (x->right != nullptr) {
                return mTree->minimum(x->right);
            }
            RBNode* y = x->parent;
            while (y != nullptr && x == y->right) {
                x = y;
                y = y->parent;
            }
            return y;
        }

        RBNode* predecessor(RBNode* x) const {
            if (x == nullptr) return nullptr;
            if (x->left != nullptr) {
                return mTree->maximum(x->left);
            }
            RBNode* y = x->parent;
            while (y != nullptr && x == y->left) {
                x = y;
                y = y->parent;
            }
            return y;
        }

    public:
        reverse_iterator() : mNode(nullptr), mTree(nullptr) {}
        reverse_iterator(RBNode* n, const RedBlackTree* t) : mNode(n), mTree(t) {}

        value_type& operator*() const {
            FASTLED_ASSERT(mNode != nullptr, "RedBlackTree::reverse_iterator: dereferencing end iterator");
            return mNode->data;
        }
        value_type* operator->() const {
            FASTLED_ASSERT(mNode != nullptr, "RedBlackTree::reverse_iterator: dereferencing end iterator");
            return &(mNode->data);
        }

        // Increment goes backward (predecessor)
        reverse_iterator& operator++() {
            if (mNode) {
                mNode = predecessor(mNode);
            }
            return *this;
        }

        reverse_iterator operator++(int) {
            reverse_iterator temp = *this;
            ++(*this);
            return temp;
        }

        // Decrement goes forward (successor)
        reverse_iterator& operator--() {
            if (mNode) {
                mNode = successor(mNode);
            } else if (mTree && mTree->root_) {
                // Decrementing from rend() should give us the minimum element
                mNode = mTree->minimum(mTree->root_);
            }
            return *this;
        }

        reverse_iterator operator--(int) {
            reverse_iterator temp = *this;
            --(*this);
            return temp;
        }

        bool operator==(const reverse_iterator& other) const {
            return mNode == other.mNode;
        }

        bool operator!=(const reverse_iterator& other) const {
            return mNode != other.mNode;
        }
    };

    class const_reverse_iterator {
        friend class RedBlackTree;
        friend class reverse_iterator;
    private:
        const RBNode* mNode;
        const RedBlackTree* mTree;

        const RBNode* successor(const RBNode* x) const {
            if (x == nullptr) return nullptr;
            if (x->right != nullptr) {
                return mTree->minimum(x->right);
            }
            const RBNode* y = x->parent;
            while (y != nullptr && x == y->right) {
                x = y;
                y = y->parent;
            }
            return y;
        }

        const RBNode* predecessor(const RBNode* x) const {
            if (x == nullptr) return nullptr;
            if (x->left != nullptr) {
                return mTree->maximum(x->left);
            }
            const RBNode* y = x->parent;
            while (y != nullptr && x == y->left) {
                x = y;
                y = y->parent;
            }
            return y;
        }

    public:
        const_reverse_iterator() : mNode(nullptr), mTree(nullptr) {}
        const_reverse_iterator(const RBNode* n, const RedBlackTree* t) : mNode(n), mTree(t) {}
        const_reverse_iterator(const reverse_iterator& it) : mNode(it.mNode), mTree(it.mTree) {}

        const value_type& operator*() const {
            FASTLED_ASSERT(mNode != nullptr, "RedBlackTree::const_reverse_iterator: dereferencing end iterator");
            return mNode->data;
        }
        const value_type* operator->() const {
            FASTLED_ASSERT(mNode != nullptr, "RedBlackTree::const_reverse_iterator: dereferencing end iterator");
            return &(mNode->data);
        }

        // Increment goes backward (predecessor)
        const_reverse_iterator& operator++() {
            if (mNode) {
                mNode = predecessor(mNode);
            }
            return *this;
        }

        const_reverse_iterator operator++(int) {
            const_reverse_iterator temp = *this;
            ++(*this);
            return temp;
        }

        // Decrement goes forward (successor)
        const_reverse_iterator& operator--() {
            if (mNode) {
                mNode = successor(mNode);
            } else if (mTree && mTree->root_) {
                // Decrementing from rend() should give us the minimum element
                mNode = mTree->minimum(mTree->root_);
            }
            return *this;
        }

        const_reverse_iterator operator--(int) {
            const_reverse_iterator temp = *this;
            --(*this);
            return temp;
        }

        bool operator==(const const_reverse_iterator& other) const {
            return mNode == other.mNode;
        }

        bool operator!=(const const_reverse_iterator& other) const {
            return mNode != other.mNode;
        }
    };

    // Constructors and destructor
    RedBlackTree(const Compare& comp = Compare(), const Allocator& alloc = Allocator())
        : root_(nullptr), mSize(0), mComp(comp), mAlloc(alloc) {}

    RedBlackTree(const RedBlackTree& other)
        : root_(nullptr), mSize(other.mSize), mComp(other.mComp), mAlloc(other.mAlloc) {
        if (other.root_) {
            root_ = copyTree(other.root_);
        }
    }

    RedBlackTree& operator=(const RedBlackTree& other) {
        if (this != &other) {
            clear();
            mSize = other.mSize;
            mComp = other.mComp;
            mAlloc = other.mAlloc;
            if (other.root_) {
                root_ = copyTree(other.root_);
            }
        }
        return *this;
    }

    // Move constructor
    RedBlackTree(RedBlackTree&& other)
        : root_(other.root_), mSize(other.mSize), mComp(fl::move(other.mComp)), mAlloc(fl::move(other.mAlloc)) {
        other.root_ = nullptr;
        other.mSize = 0;
    }

    // Move assignment operator
    RedBlackTree& operator=(RedBlackTree&& other) {
        if (this != &other) {
            clear();
            root_ = other.root_;
            mSize = other.mSize;
            mComp = fl::move(other.mComp);
            mAlloc = fl::move(other.mAlloc);
            other.root_ = nullptr;
            other.mSize = 0;
        }
        return *this;
    }

    ~RedBlackTree() {
        clear();
    }

    // Iterators
    iterator begin() {
        if (root_ == nullptr) return iterator(nullptr, this);
        return iterator(minimum(root_), this);
    }

    const_iterator begin() const {
        if (root_ == nullptr) return const_iterator(nullptr, this);
        return const_iterator(minimum(root_), this);
    }

    const_iterator cbegin() const {
        return begin();
    }

    iterator end() {
        return iterator(nullptr, this);
    }

    const_iterator end() const {
        return const_iterator(nullptr, this);
    }

    const_iterator cend() const {
        return end();
    }

    // Reverse iterators
    reverse_iterator rbegin() {
        if (root_ == nullptr) return reverse_iterator(nullptr, this);
        return reverse_iterator(maximum(root_), this);
    }

    reverse_iterator rend() {
        return reverse_iterator(nullptr, this);
    }

    const_reverse_iterator rbegin() const {
        if (root_ == nullptr) return const_reverse_iterator(nullptr, this);
        return const_reverse_iterator(maximum(root_), this);
    }

    const_reverse_iterator rend() const {
        return const_reverse_iterator(nullptr, this);
    }

    const_reverse_iterator crbegin() const {
        return rbegin();
    }

    const_reverse_iterator crend() const {
        return rend();
    }

    // Capacity
    bool empty() const { return mSize == 0; }
    fl::size size() const { return mSize; }
    fl::size max_size() const { return fl::size(-1); }

    // Modifiers
    void clear() {
        destroyTree(root_);
        root_ = nullptr;
        mSize = 0;
    }

    fl::pair<iterator, bool> insert(const value_type& value) {
        return insertImpl(value);
    }

    fl::pair<iterator, bool> insert(value_type&& value) {
        return insertImpl(fl::move(value));
    }

    template<typename... Args>
    fl::pair<iterator, bool> emplace(Args&&... args) {
        value_type value(fl::forward<Args>(args)...);
        return insert(fl::move(value));
    }

    iterator erase(const_iterator pos) {
        if (pos.mNode == nullptr) return end();
        
        RBNode* nodeToDelete = const_cast<RBNode*>(pos.mNode);
        RBNode* successor = nullptr;
        
        if (nodeToDelete->right != nullptr) {
            successor = minimum(nodeToDelete->right);
        } else {
            RBNode* current = nodeToDelete;
            RBNode* parent = current->parent;
            while (parent != nullptr && current == parent->right) {
                current = parent;
                parent = parent->parent;
            }
            successor = parent;
        }
        
        RBNode* y = nodeToDelete;
        RBNode* x = nullptr;
        RBNode* xParent = nullptr;
        Color originalColor = y->color;
        
        if (nodeToDelete->left == nullptr) {
            x = nodeToDelete->right;
            xParent = nodeToDelete->parent;
            transplant(nodeToDelete, nodeToDelete->right);
        } else if (nodeToDelete->right == nullptr) {
            x = nodeToDelete->left;
            xParent = nodeToDelete->parent;
            transplant(nodeToDelete, nodeToDelete->left);
        } else {
            y = minimum(nodeToDelete->right);
            originalColor = y->color;
            x = y->right;
            if (y->parent == nodeToDelete) {
                xParent = y;
                if (x) x->parent = y;
            } else {
                xParent = y->parent;
                transplant(y, y->right);
                y->right = nodeToDelete->right;
                y->right->parent = y;
            }
            
            transplant(nodeToDelete, y);
            y->left = nodeToDelete->left;
            y->left->parent = y;
            y->color = nodeToDelete->color;
        }
        
        mAlloc.destroy(nodeToDelete);
        mAlloc.deallocate(nodeToDelete, 1);
        --mSize;
        
        if (originalColor == Color::kBlack) {
            deleteFixup(x, xParent);
        }
        
        return iterator(successor, this);
    }

    fl::size erase(const value_type& value) {
        RBNode* node = findNode(value);
        if (node == nullptr) return 0;
        
        erase(const_iterator(node, this));
        return 1;
    }

    void swap(RedBlackTree& other) {
        fl::swap(root_, other.root_);
        fl::swap(mSize, other.mSize);
        fl::swap(mComp, other.mComp);
        fl::swap(mAlloc, other.mAlloc);
    }

    // Lookup
    fl::size count(const value_type& value) const {
        return findNode(value) != nullptr ? 1 : 0;
    }

    iterator find(const value_type& value) {
        RBNode* node = findNode(value);
        return node ? iterator(node, this) : end();
    }

    const_iterator find(const value_type& value) const {
        RBNode* node = findNode(value);
        return node ? const_iterator(node, this) : end();
    }

    bool contains(const value_type& value) const {
        return findNode(value) != nullptr;
    }

    fl::pair<iterator, iterator> equal_range(const value_type& value) {
        iterator lower = lower_bound(value);
        iterator upper = upper_bound(value);
        return fl::pair<iterator, iterator>(lower, upper);
    }

    fl::pair<const_iterator, const_iterator> equal_range(const value_type& value) const {
        const_iterator lower = lower_bound(value);
        const_iterator upper = upper_bound(value);
        return fl::pair<const_iterator, const_iterator>(lower, upper);
    }

    iterator lower_bound(const value_type& value) {
        RBNode* n = lowerBoundNode(value);
        return n ? iterator(n, this) : end();
    }

    const_iterator lower_bound(const value_type& value) const {
        RBNode* n = lowerBoundNode(value);
        return n ? const_iterator(n, this) : end();
    }

    iterator upper_bound(const value_type& value) {
        RBNode* n = upperBoundNode(value);
        return n ? iterator(n, this) : end();
    }

    const_iterator upper_bound(const value_type& value) const {
        RBNode* n = upperBoundNode(value);
        return n ? const_iterator(n, this) : end();
    }

    // Observers
    compare_type value_comp() const {
        return mComp;
    }

    // Returns a copy of the allocator object associated with the tree
    allocator_type get_allocator() const {
        return allocator_type(mAlloc);
    }

    // Comparison operators
    bool operator==(const RedBlackTree& other) const {
        if (mSize != other.mSize) return false;
        
        const_iterator it1 = begin();
        const_iterator it2 = other.begin();
        
        while (it1 != end() && it2 != other.end()) {
            // Two values are equal if neither is less than the other
            if (mComp(*it1, *it2) || mComp(*it2, *it1)) {
                return false;
            }
            ++it1;
            ++it2;
        }
        
        return it1 == end() && it2 == other.end();
    }

    bool operator!=(const RedBlackTree& other) const {
        return !(*this == other);
    }
};

// Specialized Red-Black Tree for key-value pairs (maps)
template <typename Key, typename Value, typename Compare = less<Key>, typename Allocator = allocator_slab<char>>
class MapRedBlackTree {
public:
    using key_type = Key;
    using mapped_type = Value;
    using value_type = fl::pair<Key, Value>;
    using size_type = fl::size;
    using difference_type = ptrdiff_t;
    using key_compare = Compare;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using allocator_type = Allocator;

private:
    // Comparator for pairs that compares only the key
    struct PairCompare {
        Compare mComp;

        PairCompare(const Compare& comp = Compare()) : mComp(comp) {}

        bool operator()(const value_type& a, const value_type& b) const {
            return mComp(a.first, b.first);
        }
    };

    using TreeType = RedBlackTree<value_type, PairCompare, Allocator>;
    TreeType mTree;

public:
    // Value comparison class for std::map compatibility
    class value_compare {
        friend class MapRedBlackTree;
        Compare mComp;
        value_compare(Compare c) : mComp(c) {}
    public:
        bool operator()(const value_type& x, const value_type& y) const {
            return mComp(x.first, y.first);
        }
    };

private:

public:
    using iterator = typename TreeType::iterator;
    using const_iterator = typename TreeType::const_iterator;
    using reverse_iterator = typename TreeType::reverse_iterator;
    using const_reverse_iterator = typename TreeType::const_reverse_iterator;

    // Constructors and destructor
    MapRedBlackTree(const Compare& comp = Compare(), const Allocator& alloc = Allocator())
        : mTree(PairCompare(comp), alloc) {}

    MapRedBlackTree(const MapRedBlackTree& other) = default;
    MapRedBlackTree& operator=(const MapRedBlackTree& other) = default;

    // Move constructor
    MapRedBlackTree(MapRedBlackTree&& other) = default;

    // Move assignment operator
    MapRedBlackTree& operator=(MapRedBlackTree&& other) = default;

    // Initializer list assignment operator
    // Replaces the contents of the map with the contents of the initializer list
    MapRedBlackTree& operator=(fl::initializer_list<value_type> ilist) {
        clear();
        for (const auto& value : ilist) {
            mTree.insert(value);
        }
        return *this;
    }

    // Range constructor - construct map from range [first, last)
    // Constructs a map with the contents of the range [first, last)
    // If the range contains duplicate keys, the first occurrence is kept
    template <typename InputIt>
    MapRedBlackTree(InputIt first, InputIt last,
                    const Compare& comp = Compare(),
                    const Allocator& alloc = Allocator())
        : mTree(PairCompare(comp), alloc) {
        // Insert all elements from the range
        for (InputIt it = first; it != last; ++it) {
            mTree.insert(*it);
        }
    }

    // Initializer list constructor - construct map from initializer list
    // Constructs a map with the contents of the initializer list
    // If the list contains duplicate keys, the first occurrence is kept
    MapRedBlackTree(fl::initializer_list<value_type> init,
                    const Compare& comp = Compare(),
                    const Allocator& alloc = Allocator())
        : mTree(PairCompare(comp), alloc) {
        // Insert all elements from the initializer list
        for (const auto& value : init) {
            mTree.insert(value);
        }
    }

    ~MapRedBlackTree() = default;

    // Iterators
    iterator begin() { return mTree.begin(); }
    const_iterator begin() const { return mTree.begin(); }
    const_iterator cbegin() const { return mTree.cbegin(); }
    iterator end() { return mTree.end(); }
    const_iterator end() const { return mTree.end(); }
    const_iterator cend() const { return mTree.cend(); }

    // Reverse iterators
    reverse_iterator rbegin() { return mTree.rbegin(); }
    reverse_iterator rend() { return mTree.rend(); }
    const_reverse_iterator rbegin() const { return mTree.rbegin(); }
    const_reverse_iterator rend() const { return mTree.rend(); }
    const_reverse_iterator crbegin() const { return mTree.crbegin(); }
    const_reverse_iterator crend() const { return mTree.crend(); }

    // Capacity
    bool empty() const { return mTree.empty(); }
    fl::size size() const { return mTree.size(); }
    fl::size max_size() const { return mTree.max_size(); }

    // Element access
    Value& operator[](const Key& key) {
        auto result = mTree.insert(value_type(key, Value()));
        return result.first->second;
    }

    Value& at(const Key& key) {
        auto it = mTree.find(value_type(key, Value()));
        FASTLED_ASSERT(it != mTree.end(), "MapRedBlackTree::at: key not found");
        return it->second;
    }

    const Value& at(const Key& key) const {
        auto it = mTree.find(value_type(key, Value()));
        FASTLED_ASSERT(it != mTree.end(), "MapRedBlackTree::at: key not found");
        return it->second;
    }

    // Modifiers
    void clear() { mTree.clear(); }

    fl::pair<iterator, bool> insert(const value_type& value) {
        return mTree.insert(value);
    }

    fl::pair<iterator, bool> insert(value_type&& value) {
        return mTree.insert(fl::move(value));
    }

    template<typename... Args>
    fl::pair<iterator, bool> emplace(Args&&... args) {
        return mTree.emplace(fl::forward<Args>(args)...);
    }

    // Range insert - insert elements from range [first, last)
    // Duplicates are ignored (existing keys are not overwritten)
    template <typename InputIt>
    void insert(InputIt first, InputIt last) {
        for (InputIt it = first; it != last; ++it) {
            mTree.insert(*it);
        }
    }

    // Initializer list insert - insert elements from initializer list
    // Duplicates are ignored (existing keys are not overwritten)
    void insert(fl::initializer_list<value_type> ilist) {
        for (const auto& value : ilist) {
            mTree.insert(value);
        }
    }

    // Hint-based insert - insert with iterator hint for potential optimization
    // Returns iterator to inserted element or existing element if key already exists
    // Note: Current implementation ignores hint parameter; optimization can be added later
    iterator insert(const_iterator hint, const value_type& value) {
        (void)hint; // Hint parameter not currently used in implementation
        auto result = mTree.insert(value);
        return result.first;
    }

    iterator insert(const_iterator hint, value_type&& value) {
        (void)hint; // Hint parameter not currently used in implementation
        auto result = mTree.insert(fl::move(value));
        return result.first;
    }

    // Hint-based emplace - construct element in-place with iterator hint for potential optimization
    // Returns iterator to emplaced element or existing element if key already exists
    // Note: Current implementation ignores hint parameter; optimization can be added later
    template<typename... Args>
    iterator emplace_hint(const_iterator hint, Args&&... args) {
        (void)hint; // Hint parameter not currently used in implementation
        auto result = mTree.emplace(fl::forward<Args>(args)...);
        return result.first;
    }

    // Insert or assign - insert if key doesn't exist, assign if it does
    // Returns pair<iterator, bool> where bool indicates if insertion took place
    template <typename M>
    fl::pair<iterator, bool> insert_or_assign(const Key& key, M&& obj) {
        auto it = mTree.find(value_type(key, Value()));
        if (it != mTree.end()) {
            // Key exists, assign new value
            it->second = fl::forward<M>(obj);
            return fl::pair<iterator, bool>(it, false);
        } else {
            // Key doesn't exist, insert
            return mTree.insert(value_type(key, fl::forward<M>(obj)));
        }
    }

    template <typename M>
    fl::pair<iterator, bool> insert_or_assign(Key&& key, M&& obj) {
        auto it = mTree.find(value_type(key, Value()));
        if (it != mTree.end()) {
            // Key exists, assign new value
            it->second = fl::forward<M>(obj);
            return fl::pair<iterator, bool>(it, false);
        } else {
            // Key doesn't exist, insert with moved key
            return mTree.insert(value_type(fl::move(key), fl::forward<M>(obj)));
        }
    }

    // Hint-based insert_or_assign (hint parameter currently ignored, same as non-hint version)
    template <typename M>
    iterator insert_or_assign(const_iterator hint, const Key& key, M&& obj) {
        (void)hint; // Hint parameter not currently used in implementation
        auto result = insert_or_assign(key, fl::forward<M>(obj));
        return result.first;
    }

    template <typename M>
    iterator insert_or_assign(const_iterator hint, Key&& key, M&& obj) {
        (void)hint; // Hint parameter not currently used in implementation
        auto result = insert_or_assign(fl::move(key), fl::forward<M>(obj));
        return result.first;
    }

    // try_emplace - insert if key doesn't exist (constructing value in-place), do nothing if it does
    // Returns pair<iterator, bool> where bool indicates if insertion took place
    // More efficient than insert_or_assign when value construction is expensive
    //
    // Key difference from insert_or_assign:
    // - try_emplace: Does NOT construct value_args if key exists (but does construct Value() for search)
    // - insert_or_assign: Constructs value first, then either inserts or assigns
    //
    // Note: This implementation constructs Value() once for the lower_bound search,
    // but does NOT construct the actual value from args... if the key already exists.
    // This is still more efficient than insert_or_assign when args... is expensive.
    template <typename... Args>
    fl::pair<iterator, bool> try_emplace(const Key& key, Args&&... args) {
        // Use lower_bound to find position (constructs Value() once for search)
        auto it = mTree.lower_bound(value_type(key, Value()));

        // Check if key already exists
        if (it != mTree.end() && it->first == key) {
            // Key exists, do NOT construct value from args
            return fl::pair<iterator, bool>(it, false);
        }

        // Key doesn't exist, now construct value from args and insert
        return mTree.emplace(key, fl::forward<Args>(args)...);
    }

    template <typename... Args>
    fl::pair<iterator, bool> try_emplace(Key&& key, Args&&... args) {
        // Use lower_bound to find position
        auto it = mTree.lower_bound(value_type(key, Value()));

        // Check if key already exists
        if (it != mTree.end() && it->first == key) {
            // Key exists, do NOT construct value from args
            return fl::pair<iterator, bool>(it, false);
        }

        // Key doesn't exist, construct value from args and insert with moved key
        return mTree.emplace(fl::move(key), fl::forward<Args>(args)...);
    }

    // Hint-based try_emplace (hint parameter currently ignored, same as non-hint version)
    template <typename... Args>
    iterator try_emplace(const_iterator hint, const Key& key, Args&&... args) {
        (void)hint; // Hint parameter not currently used in implementation
        auto result = try_emplace(key, fl::forward<Args>(args)...);
        return result.first;
    }

    template <typename... Args>
    iterator try_emplace(const_iterator hint, Key&& key, Args&&... args) {
        (void)hint; // Hint parameter not currently used in implementation
        auto result = try_emplace(fl::move(key), fl::forward<Args>(args)...);
        return result.first;
    }

    iterator erase(const_iterator pos) {
        return mTree.erase(pos);
    }

    fl::size erase(const Key& key) {
        return mTree.erase(value_type(key, Value()));
    }

    // Range erase - erase all elements in range [first, last)
    // Returns iterator following the last removed element
    iterator erase(const_iterator first, const_iterator last) {
        // Handle empty range case
        if (first == last) {
            // Convert const_iterator to iterator for empty range
            // We need to find the same position as an iterator
            if (first == mTree.cend()) {
                return mTree.end();
            }
            // For non-end iterator, find by constructing the full value_type for search
            return mTree.find(value_type(first->first, Value()));
        }

        // Erase elements one by one from first to last
        // Use an iterator to track position since erase returns iterator
        iterator current = mTree.erase(first); // Erase first element and get iterator to next

        // Continue erasing while we haven't reached last
        // We need to compare with last, but current is iterator and last is const_iterator
        // This works because iterator can be compared with const_iterator
        while (current != last) {
            current = mTree.erase(current);
        }

        // Return iterator following the last removed element
        return current;
    }

    void swap(MapRedBlackTree& other) {
        mTree.swap(other.mTree);
    }

    // Lookup
    fl::size count(const Key& key) const {
        return mTree.count(value_type(key, Value()));
    }

    iterator find(const Key& key) {
        return mTree.find(value_type(key, Value()));
    }

    const_iterator find(const Key& key) const {
        return mTree.find(value_type(key, Value()));
    }

    bool contains(const Key& key) const {
        return mTree.contains(value_type(key, Value()));
    }

    fl::pair<iterator, iterator> equal_range(const Key& key) {
        return mTree.equal_range(value_type(key, Value()));
    }

    fl::pair<const_iterator, const_iterator> equal_range(const Key& key) const {
        return mTree.equal_range(value_type(key, Value()));
    }

    iterator lower_bound(const Key& key) {
        return mTree.lower_bound(value_type(key, Value()));
    }

    const_iterator lower_bound(const Key& key) const {
        return mTree.lower_bound(value_type(key, Value()));
    }

    iterator upper_bound(const Key& key) {
        return mTree.upper_bound(value_type(key, Value()));
    }

    const_iterator upper_bound(const Key& key) const {
        return mTree.upper_bound(value_type(key, Value()));
    }

    // Observers
    key_compare key_comp() const {
        return mTree.value_comp().mComp;
    }

    value_compare value_comp() const {
        return value_compare(key_comp());
    }

    // Returns a copy of the allocator object associated with the map
    allocator_type get_allocator() const {
        return mTree.get_allocator();
    }

    // Comparison operators
    bool operator==(const MapRedBlackTree& other) const {
        if (mTree.size() != other.mTree.size()) return false;

        auto it1 = mTree.begin();
        auto it2 = other.mTree.begin();

        while (it1 != mTree.end() && it2 != other.mTree.end()) {
            // Compare both key and value
            if (it1->first != it2->first || it1->second != it2->second) {
                return false;
            }
            ++it1;
            ++it2;
        }

        return it1 == mTree.end() && it2 == other.mTree.end();
    }

    bool operator!=(const MapRedBlackTree& other) const {
        return !(*this == other);
    }

    // Lexicographic comparison operators
    // Compare maps element by element in sorted order
    // Returns true if this map is lexicographically less than other
    bool operator<(const MapRedBlackTree& other) const {
        return fl::lexicographical_compare(mTree.begin(), mTree.end(),
                                          other.mTree.begin(), other.mTree.end());
    }

    // Returns true if this map is lexicographically less than or equal to other
    bool operator<=(const MapRedBlackTree& other) const {
        return !(other < *this);
    }

    // Returns true if this map is lexicographically greater than other
    bool operator>(const MapRedBlackTree& other) const {
        return other < *this;
    }

    // Returns true if this map is lexicographically greater than or equal to other
    bool operator>=(const MapRedBlackTree& other) const {
        return !(*this < other);
    }
};

// Specialized Red-Black Tree for sets (just keys, no values)
template <typename Key, typename Compare = less<Key>, typename Allocator = allocator_slab<char>>
class SetRedBlackTree {
public:
    using key_type = Key;
    using value_type = Key;
    using size_type = fl::size;
    using difference_type = ptrdiff_t;
    using key_compare = Compare;
    using reference = const value_type&;
    using const_reference = const value_type&;
    using pointer = const value_type*;
    using const_pointer = const value_type*;
    using allocator_type = Allocator;

private:
    using TreeType = RedBlackTree<Key, Compare, Allocator>;
    TreeType mTree;

public:
    using iterator = typename TreeType::const_iterator;  // Set iterators are always const
    using const_iterator = typename TreeType::const_iterator;

    // Constructors and destructor
    SetRedBlackTree(const Compare& comp = Compare(), const Allocator& alloc = Allocator()) 
        : mTree(comp, alloc) {}

    SetRedBlackTree(const SetRedBlackTree& other) = default;
    SetRedBlackTree& operator=(const SetRedBlackTree& other) = default;
    ~SetRedBlackTree() = default;

    // Iterators
    const_iterator begin() const { return mTree.begin(); }
    const_iterator cbegin() const { return mTree.cbegin(); }
    const_iterator end() const { return mTree.end(); }
    const_iterator cend() const { return mTree.cend(); }

    // Capacity
    bool empty() const { return mTree.empty(); }
    fl::size size() const { return mTree.size(); }
    fl::size max_size() const { return mTree.max_size(); }

    // Modifiers
    void clear() { mTree.clear(); }

    fl::pair<const_iterator, bool> insert(const value_type& value) {
        auto result = mTree.insert(value);
        return fl::pair<const_iterator, bool>(result.first, result.second);
    }

    fl::pair<const_iterator, bool> insert(value_type&& value) {
        auto result = mTree.insert(fl::move(value));
        return fl::pair<const_iterator, bool>(result.first, result.second);
    }

    template<typename... Args>
    fl::pair<const_iterator, bool> emplace(Args&&... args) {
        auto result = mTree.emplace(fl::forward<Args>(args)...);
        return fl::pair<const_iterator, bool>(result.first, result.second);
    }

    const_iterator erase(const_iterator pos) {
        return mTree.erase(pos);
    }

    fl::size erase(const Key& key) {
        return mTree.erase(key);
    }

    void swap(SetRedBlackTree& other) {
        mTree.swap(other.mTree);
    }

    // Lookup
    fl::size count(const Key& key) const {
        return mTree.count(key);
    }

    const_iterator find(const Key& key) const {
        return mTree.find(key);
    }

    bool contains(const Key& key) const {
        return mTree.contains(key);
    }

    fl::pair<const_iterator, const_iterator> equal_range(const Key& key) const {
        return mTree.equal_range(key);
    }

    const_iterator lower_bound(const Key& key) const {
        return mTree.lower_bound(key);
    }

    const_iterator upper_bound(const Key& key) const {
        return mTree.upper_bound(key);
    }

    // Observers
    key_compare key_comp() const {
        return mTree.value_comp();
    }

    // Comparison operators
    bool operator==(const SetRedBlackTree& other) const {
        return mTree == other.mTree;
    }

    bool operator!=(const SetRedBlackTree& other) const {
        return mTree != other.mTree;
    }
};

} // namespace fl
