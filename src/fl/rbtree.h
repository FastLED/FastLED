#pragma once

#include "fl/assert.h"
#include "fl/comparators.h"
#include "fl/namespace.h"
#include "fl/pair.h"
#include "fl/type_traits.h"
#include "fl/type_traits.h"
#include "fl/algorithm.h"
#include "fl/allocator.h"

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
    enum Color { RED, BLACK };

private:
    struct RBNode {
        value_type data;
        Color color;
        RBNode* left;
        RBNode* right;
        RBNode* parent;

        RBNode(const value_type& val, Color c = RED, RBNode* p = nullptr)
            : data(val), color(c), left(nullptr), right(nullptr), parent(p) {}
        
        template<typename... Args>
        RBNode(Color c, RBNode* p, Args&&... args)
            : data(fl::forward<Args>(args)...), color(c), left(nullptr), right(nullptr), parent(p) {}
    };

    using NodeAllocator = typename Allocator::template rebind<RBNode>::other;

    RBNode* root_;
    fl::size size_;
    Compare comp_;
    NodeAllocator alloc_;

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
        while (z->parent != nullptr && z->parent->parent != nullptr && z->parent->color == RED) {
            if (z->parent == z->parent->parent->left) {
                RBNode* y = z->parent->parent->right;
                if (y != nullptr && y->color == RED) {
                    z->parent->color = BLACK;
                    y->color = BLACK;
                    z->parent->parent->color = RED;
                    z = z->parent->parent;
                } else {
                    if (z == z->parent->right) {
                        z = z->parent;
                        rotateLeft(z);
                    }
                    z->parent->color = BLACK;
                    z->parent->parent->color = RED;
                    rotateRight(z->parent->parent);
                }
            } else {
                RBNode* y = z->parent->parent->left;
                if (y != nullptr && y->color == RED) {
                    z->parent->color = BLACK;
                    y->color = BLACK;
                    z->parent->parent->color = RED;
                    z = z->parent->parent;
                } else {
                    if (z == z->parent->left) {
                        z = z->parent;
                        rotateRight(z);
                    }
                    z->parent->color = BLACK;
                    z->parent->parent->color = RED;
                    rotateLeft(z->parent->parent);
                }
            }
        }
        root_->color = BLACK;
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
        while ((x != root_) && (x == nullptr || x->color == BLACK)) {
            if (x == (xParent ? xParent->left : nullptr)) {
                RBNode* w = xParent ? xParent->right : nullptr;
                if (w && w->color == RED) {
                    w->color = BLACK;
                    if (xParent) { xParent->color = RED; rotateLeft(xParent); }
                    w = xParent ? xParent->right : nullptr;
                }
                bool wLeftBlack = (!w || !w->left || w->left->color == BLACK);
                bool wRightBlack = (!w || !w->right || w->right->color == BLACK);
                if (wLeftBlack && wRightBlack) {
                    if (w) w->color = RED;
                    x = xParent;
                    xParent = xParent ? xParent->parent : nullptr;
                } else {
                    if (!w || (w->right == nullptr || w->right->color == BLACK)) {
                        if (w && w->left) w->left->color = BLACK;
                        if (w) { w->color = RED; rotateRight(w); }
                        w = xParent ? xParent->right : nullptr;
                    }
                    if (w) w->color = xParent ? xParent->color : BLACK;
                    if (xParent) xParent->color = BLACK;
                    if (w && w->right) w->right->color = BLACK;
                    if (xParent) rotateLeft(xParent);
                    x = root_;
                }
            } else {
                RBNode* w = xParent ? xParent->left : nullptr;
                if (w && w->color == RED) {
                    w->color = BLACK;
                    if (xParent) { xParent->color = RED; rotateRight(xParent); }
                    w = xParent ? xParent->left : nullptr;
                }
                bool wRightBlack = (!w || !w->right || w->right->color == BLACK);
                bool wLeftBlack = (!w || !w->left || w->left->color == BLACK);
                if (wRightBlack && wLeftBlack) {
                    if (w) w->color = RED;
                    x = xParent;
                    xParent = xParent ? xParent->parent : nullptr;
                } else {
                    if (!w || (w->left == nullptr || w->left->color == BLACK)) {
                        if (w && w->right) w->right->color = BLACK;
                        if (w) { w->color = RED; rotateLeft(w); }
                        w = xParent ? xParent->left : nullptr;
                    }
                    if (w) w->color = xParent ? xParent->color : BLACK;
                    if (xParent) xParent->color = BLACK;
                    if (w && w->left) w->left->color = BLACK;
                    if (xParent) rotateRight(xParent);
                    x = root_;
                }
            }
        }
        if (x) x->color = BLACK;
    }

    RBNode* findNode(const value_type& value) const {
        RBNode* current = root_;
        while (current != nullptr) {
            if (comp_(value, current->data)) {
                current = current->left;
            } else if (comp_(current->data, value)) {
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
            alloc_.destroy(node);
            alloc_.deallocate(node, 1);
        }
    }

    RBNode* copyTree(RBNode* node, RBNode* parent = nullptr) {
        if (node == nullptr) return nullptr;
        
        RBNode* newNode = alloc_.allocate(1);
        if (newNode == nullptr) {
            return nullptr;
        }
        
        alloc_.construct(newNode, node->data, node->color, parent);
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
            if (comp_(value, current->data)) {
                current = current->left;
            } else if (comp_(current->data, value)) {
                current = current->right;
            } else {
                return fl::pair<iterator, bool>(iterator(current, this), false);
            }
        }
        
        RBNode* newNode = alloc_.allocate(1);
        if (newNode == nullptr) {
            return fl::pair<iterator, bool>(end(), false);
        }
        
        alloc_.construct(newNode, fl::forward<U>(value), RED, parent);
        
        if (parent == nullptr) {
            root_ = newNode;
        } else if (comp_(newNode->data, parent->data)) {
            parent->left = newNode;
        } else {
            parent->right = newNode;
        }
        
        insertFixup(newNode);
        ++size_;
        
        return fl::pair<iterator, bool>(iterator(newNode, this), true);
    }

    // Bound helpers to avoid duplication between const/non-const
    RBNode* lowerBoundNode(const value_type& value) const {
        RBNode* current = root_;
        RBNode* result = nullptr;
        while (current != nullptr) {
            if (!comp_(current->data, value)) {
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
            if (comp_(value, current->data)) {
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
        RBNode* node_;
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
        iterator() : node_(nullptr), mTree(nullptr) {}
        iterator(RBNode* n, const RedBlackTree* t) : node_(n), mTree(t) {}

        value_type& operator*() const { 
            FASTLED_ASSERT(node_ != nullptr, "RedBlackTree::iterator: dereferencing end iterator");
            return node_->data; 
        }
        value_type* operator->() const { 
            FASTLED_ASSERT(node_ != nullptr, "RedBlackTree::iterator: dereferencing end iterator");
            return &(node_->data); 
        }

        iterator& operator++() {
            if (node_) {
                node_ = successor(node_);
            }
            return *this;
        }

        iterator operator++(int) {
            iterator temp = *this;
            ++(*this);
            return temp;
        }

        iterator& operator--() {
            if (node_) {
                node_ = predecessor(node_);
            } else if (mTree && mTree->root_) {
                node_ = mTree->maximum(mTree->root_);
            }
            return *this;
        }

        iterator operator--(int) {
            iterator temp = *this;
            --(*this);
            return temp;
        }

        bool operator==(const iterator& other) const {
            return node_ == other.node_;
        }

        bool operator!=(const iterator& other) const {
            return node_ != other.node_;
        }
    };

    class const_iterator {
        friend class RedBlackTree;
        friend class iterator;
    private:
        const RBNode* node_;
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
        const_iterator() : node_(nullptr), mTree(nullptr) {}
        const_iterator(const RBNode* n, const RedBlackTree* t) : node_(n), mTree(t) {}
        const_iterator(const iterator& it) : node_(it.node_), mTree(it.mTree) {}

        const value_type& operator*() const { 
            FASTLED_ASSERT(node_ != nullptr, "RedBlackTree::iterator: dereferencing end iterator");
            return node_->data; 
        }
        const value_type* operator->() const { 
            FASTLED_ASSERT(node_ != nullptr, "RedBlackTree::iterator: dereferencing end iterator");
            return &(node_->data); 
        }

        const_iterator& operator++() {
            if (node_) {
                node_ = successor(node_);
            }
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator temp = *this;
            ++(*this);
            return temp;
        }

        const_iterator& operator--() {
            if (node_) {
                node_ = predecessor(node_);
            } else if (mTree && mTree->root_) {
                // Decrementing from end() should give us the maximum element
                node_ = mTree->maximum(mTree->root_);
            }
            return *this;
        }

        const_iterator operator--(int) {
            const_iterator temp = *this;
            --(*this);
            return temp;
        }

        bool operator==(const const_iterator& other) const {
            return node_ == other.node_;
        }

        bool operator!=(const const_iterator& other) const {
            return node_ != other.node_;
        }
    };

    // Constructors and destructor
    RedBlackTree(const Compare& comp = Compare(), const Allocator& alloc = Allocator()) 
        : root_(nullptr), size_(0), comp_(comp), alloc_(alloc) {}

    RedBlackTree(const RedBlackTree& other) 
        : root_(nullptr), size_(other.size_), comp_(other.comp_), alloc_(other.alloc_) {
        if (other.root_) {
            root_ = copyTree(other.root_);
        }
    }

    RedBlackTree& operator=(const RedBlackTree& other) {
        if (this != &other) {
            clear();
            size_ = other.size_;
            comp_ = other.comp_;
            alloc_ = other.alloc_;
            if (other.root_) {
                root_ = copyTree(other.root_);
            }
        }
        return *this;
    }

    ~RedBlackTree() {
        clear();
    }

    // Iterators
    iterator begin() {
        if (root_ == nullptr) return end();
        return iterator(minimum(root_), this);
    }

    const_iterator begin() const {
        if (root_ == nullptr) return end();
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

    // Capacity
    bool empty() const { return size_ == 0; }
    fl::size size() const { return size_; }
    fl::size max_size() const { return fl::size(-1); }

    // Modifiers
    void clear() {
        destroyTree(root_);
        root_ = nullptr;
        size_ = 0;
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
        if (pos.node_ == nullptr) return end();
        
        RBNode* nodeToDelete = const_cast<RBNode*>(pos.node_);
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
        
        alloc_.destroy(nodeToDelete);
        alloc_.deallocate(nodeToDelete, 1);
        --size_;
        
        if (originalColor == BLACK) {
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
        fl::swap(size_, other.size_);
        fl::swap(comp_, other.comp_);
        fl::swap(alloc_, other.alloc_);
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
        return comp_;
    }

    // Comparison operators
    bool operator==(const RedBlackTree& other) const {
        if (size_ != other.size_) return false;
        
        const_iterator it1 = begin();
        const_iterator it2 = other.begin();
        
        while (it1 != end() && it2 != other.end()) {
            // Two values are equal if neither is less than the other
            if (comp_(*it1, *it2) || comp_(*it2, *it1)) {
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
        Compare comp_;
        
        PairCompare(const Compare& comp = Compare()) : comp_(comp) {}
        
        bool operator()(const value_type& a, const value_type& b) const {
            return comp_(a.first, b.first);
        }
    };

    using TreeType = RedBlackTree<value_type, PairCompare, Allocator>;
    TreeType mTree;

public:
    using iterator = typename TreeType::iterator;
    using const_iterator = typename TreeType::const_iterator;

    // Constructors and destructor
    MapRedBlackTree(const Compare& comp = Compare(), const Allocator& alloc = Allocator()) 
        : mTree(PairCompare(comp), alloc) {}

    MapRedBlackTree(const MapRedBlackTree& other) = default;
    MapRedBlackTree& operator=(const MapRedBlackTree& other) = default;
    ~MapRedBlackTree() = default;

    // Iterators
    iterator begin() { return mTree.begin(); }
    const_iterator begin() const { return mTree.begin(); }
    const_iterator cbegin() const { return mTree.cbegin(); }
    iterator end() { return mTree.end(); }
    const_iterator end() const { return mTree.end(); }
    const_iterator cend() const { return mTree.cend(); }

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

    iterator erase(const_iterator pos) {
        return mTree.erase(pos);
    }

    fl::size erase(const Key& key) {
        return mTree.erase(value_type(key, Value()));
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
        return mTree.value_comp().comp_;
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
