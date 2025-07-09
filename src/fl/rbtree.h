#pragma once

#include "fl/assert.h"
#include "fl/comparators.h"
#include "fl/namespace.h"
#include "fl/pair.h"
#include "fl/template_magic.h"
#include "fl/type_traits.h"
#include "fl/algorithm.h"
#include "fl/allocator.h"

namespace fl {

// Generic Red-Black Tree implementation
// This is a self-balancing binary search tree with O(log n) operations
// T is the value type stored in the tree
// Compare is a comparator that can compare T values
template <typename T, typename Compare = DefaultLess<T>, typename Allocator = allocator_slab<char>>
class RedBlackTree {
public:
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
    struct Node {
        value_type data;
        Color color;
        Node* left;
        Node* right;
        Node* parent;

        Node(const value_type& val, Color c = RED, Node* p = nullptr)
            : data(val), color(c), left(nullptr), right(nullptr), parent(p) {}
        
        template<typename... Args>
        Node(Color c, Node* p, Args&&... args)
            : data(fl::forward<Args>(args)...), color(c), left(nullptr), right(nullptr), parent(p) {}
    };

    using NodeAllocator = typename Allocator::template rebind<Node>::other;

    Node* root_;
    fl::size size_;
    Compare comp_;
    NodeAllocator alloc_;

    // Helper methods
    void rotateLeft(Node* x) {
        Node* y = x->right;
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

    void rotateRight(Node* x) {
        Node* y = x->left;
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

    void insertFixup(Node* z) {
        while (z->parent != nullptr && z->parent->color == RED) {
            if (z->parent == z->parent->parent->left) {
                Node* y = z->parent->parent->right;
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
                Node* y = z->parent->parent->left;
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

    void transplant(Node* u, Node* v) {
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

    Node* minimum(Node* x) const {
        while (x->left != nullptr) {
            x = x->left;
        }
        return x;
    }

    Node* maximum(Node* x) const {
        while (x->right != nullptr) {
            x = x->right;
        }
        return x;
    }

    void deleteFixup(Node* x) {
        while (x != root_ && (x == nullptr || x->color == BLACK)) {
            if (x == (x && x->parent ? x->parent->left : nullptr)) {
                Node* w = x && x->parent ? x->parent->right : nullptr;
                if (w && w->color == RED) {
                    w->color = BLACK;
                    if (x && x->parent) {
                        x->parent->color = RED;
                        rotateLeft(x->parent);
                        w = x->parent->right;
                    }
                }
                if (w && (w->left == nullptr || w->left->color == BLACK) &&
                    (w->right == nullptr || w->right->color == BLACK)) {
                    w->color = RED;
                    x = x ? x->parent : nullptr;
                } else {
                    if (w && (w->right == nullptr || w->right->color == BLACK)) {
                        if (w->left) w->left->color = BLACK;
                        w->color = RED;
                        rotateRight(w);
                        w = x && x->parent ? x->parent->right : nullptr;
                    }
                    if (w) {
                        w->color = x && x->parent ? x->parent->color : BLACK;
                        if (w->right) w->right->color = BLACK;
                    }
                    if (x && x->parent) {
                        x->parent->color = BLACK;
                        rotateLeft(x->parent);
                    }
                    x = root_;
                }
            } else {
                Node* w = x && x->parent ? x->parent->left : nullptr;
                if (w && w->color == RED) {
                    w->color = BLACK;
                    if (x && x->parent) {
                        x->parent->color = RED;
                        rotateRight(x->parent);
                        w = x->parent->left;
                    }
                }
                if (w && (w->right == nullptr || w->right->color == BLACK) &&
                    (w->left == nullptr || w->left->color == BLACK)) {
                    w->color = RED;
                    x = x ? x->parent : nullptr;
                } else {
                    if (w && (w->left == nullptr || w->left->color == BLACK)) {
                        if (w->right) w->right->color = BLACK;
                        w->color = RED;
                        rotateLeft(w);
                        w = x && x->parent ? x->parent->left : nullptr;
                    }
                    if (w) {
                        w->color = x && x->parent ? x->parent->color : BLACK;
                        if (w->left) w->left->color = BLACK;
                    }
                    if (x && x->parent) {
                        x->parent->color = BLACK;
                        rotateRight(x->parent);
                    }
                    x = root_;
                }
            }
        }
        if (x) x->color = BLACK;
    }

    Node* findNode(const value_type& value) const {
        Node* current = root_;
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

    void destroyTree(Node* node) {
        if (node != nullptr) {
            destroyTree(node->left);
            destroyTree(node->right);
            alloc_.destroy(node);
            alloc_.deallocate(node, 1);
        }
    }

    Node* copyTree(Node* node, Node* parent = nullptr) {
        if (node == nullptr) return nullptr;
        
        Node* newNode = alloc_.allocate(1);
        if (newNode == nullptr) {
            // Out of memory - this is a critical error
            // In embedded systems, we can't recover from this
            return nullptr;
        }
        
        alloc_.construct(newNode, node->data, node->color, parent);
        newNode->left = copyTree(node->left, newNode);
        newNode->right = copyTree(node->right, newNode);
        return newNode;
    }

public:
    // Iterator implementation
    class iterator {
        friend class RedBlackTree;
        friend class const_iterator;
    private:
        Node* node_;
        const RedBlackTree* tree_;

        Node* successor(Node* x) const {
            if (x == nullptr) return nullptr;
            if (x->right != nullptr) {
                return tree_->minimum(x->right);
            }
            Node* y = x->parent;
            while (y != nullptr && x == y->right) {
                x = y;
                y = y->parent;
            }
            return y;
        }

        Node* predecessor(Node* x) const {
            if (x == nullptr) return nullptr;
            if (x->left != nullptr) {
                return tree_->maximum(x->left);
            }
            Node* y = x->parent;
            while (y != nullptr && x == y->left) {
                x = y;
                y = y->parent;
            }
            return y;
        }

    public:
        iterator() : node_(nullptr), tree_(nullptr) {}
        iterator(Node* n, const RedBlackTree* t) : node_(n), tree_(t) {}

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
            } else if (tree_ && tree_->root_) {
                // Decrementing from end() should give us the maximum element
                node_ = tree_->maximum(tree_->root_);
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
        const Node* node_;
        const RedBlackTree* tree_;

        const Node* successor(const Node* x) const {
            if (x == nullptr) return nullptr;
            if (x->right != nullptr) {
                return tree_->minimum(x->right);
            }
            const Node* y = x->parent;
            while (y != nullptr && x == y->right) {
                x = y;
                y = y->parent;
            }
            return y;
        }

        const Node* predecessor(const Node* x) const {
            if (x == nullptr) return nullptr;
            if (x->left != nullptr) {
                return tree_->maximum(x->left);
            }
            const Node* y = x->parent;
            while (y != nullptr && x == y->left) {
                x = y;
                y = y->parent;
            }
            return y;
        }

    public:
        const_iterator() : node_(nullptr), tree_(nullptr) {}
        const_iterator(const Node* n, const RedBlackTree* t) : node_(n), tree_(t) {}
        const_iterator(const iterator& it) : node_(it.node_), tree_(it.tree_) {}

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
            } else if (tree_ && tree_->root_) {
                // Decrementing from end() should give us the maximum element
                node_ = tree_->maximum(tree_->root_);
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
        Node* parent = nullptr;
        Node* current = root_;
        
        while (current != nullptr) {
            parent = current;
            if (comp_(value, current->data)) {
                current = current->left;
            } else if (comp_(current->data, value)) {
                current = current->right;
            } else {
                // Value already exists
                return fl::pair<iterator, bool>(iterator(current, this), false);
            }
        }
        
        Node* newNode = alloc_.allocate(1);
        if (newNode == nullptr) {
            return fl::pair<iterator, bool>(end(), false);
        }
        
        alloc_.construct(newNode, value, RED, parent);
        
        if (parent == nullptr) {
            root_ = newNode;
        } else if (comp_(value, parent->data)) {
            parent->left = newNode;
        } else {
            parent->right = newNode;
        }
        
        insertFixup(newNode);
        ++size_;
        
        return fl::pair<iterator, bool>(iterator(newNode, this), true);
    }

    fl::pair<iterator, bool> insert(value_type&& value) {
        Node* parent = nullptr;
        Node* current = root_;
        
        while (current != nullptr) {
            parent = current;
            if (comp_(value, current->data)) {
                current = current->left;
            } else if (comp_(current->data, value)) {
                current = current->right;
            } else {
                // Value already exists
                return fl::pair<iterator, bool>(iterator(current, this), false);
            }
        }
        
        Node* newNode = alloc_.allocate(1);
        if (newNode == nullptr) {
            return fl::pair<iterator, bool>(end(), false);
        }
        
        alloc_.construct(newNode, fl::move(value), RED, parent);
        
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

    template<typename... Args>
    fl::pair<iterator, bool> emplace(Args&&... args) {
        value_type value(fl::forward<Args>(args)...);
        return insert(fl::move(value));
    }

    iterator erase(const_iterator pos) {
        if (pos.node_ == nullptr) return end();
        
        Node* nodeToDelete = const_cast<Node*>(pos.node_);
        Node* successor = nullptr;
        
        if (nodeToDelete->right != nullptr) {
            successor = minimum(nodeToDelete->right);
        } else {
            Node* current = nodeToDelete;
            Node* parent = current->parent;
            while (parent != nullptr && current == parent->right) {
                current = parent;
                parent = parent->parent;
            }
            successor = parent;
        }
        
        Node* y = nodeToDelete;
        Node* x = nullptr;
        Color originalColor = y->color;
        
        if (nodeToDelete->left == nullptr) {
            x = nodeToDelete->right;
            transplant(nodeToDelete, nodeToDelete->right);
        } else if (nodeToDelete->right == nullptr) {
            x = nodeToDelete->left;
            transplant(nodeToDelete, nodeToDelete->left);
        } else {
            y = minimum(nodeToDelete->right);
            originalColor = y->color;
            x = y->right;
            
            if (y->parent == nodeToDelete) {
                if (x) x->parent = y;
            } else {
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
            deleteFixup(x);
        }
        
        return iterator(successor, this);
    }

    fl::size erase(const value_type& value) {
        Node* node = findNode(value);
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
        Node* node = findNode(value);
        return node ? iterator(node, this) : end();
    }

    const_iterator find(const value_type& value) const {
        Node* node = findNode(value);
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
        Node* current = root_;
        Node* result = nullptr;
        
        while (current != nullptr) {
            if (!comp_(current->data, value)) {
                result = current;
                current = current->left;
            } else {
                current = current->right;
            }
        }
        
        return result ? iterator(result, this) : end();
    }

    const_iterator lower_bound(const value_type& value) const {
        Node* current = root_;
        Node* result = nullptr;
        
        while (current != nullptr) {
            if (!comp_(current->data, value)) {
                result = current;
                current = current->left;
            } else {
                current = current->right;
            }
        }
        
        return result ? const_iterator(result, this) : end();
    }

    iterator upper_bound(const value_type& value) {
        Node* current = root_;
        Node* result = nullptr;
        
        while (current != nullptr) {
            if (comp_(value, current->data)) {
                result = current;
                current = current->left;
            } else {
                current = current->right;
            }
        }
        
        return result ? iterator(result, this) : end();
    }

    const_iterator upper_bound(const value_type& value) const {
        Node* current = root_;
        Node* result = nullptr;
        
        while (current != nullptr) {
            if (comp_(value, current->data)) {
                result = current;
                current = current->left;
            } else {
                current = current->right;
            }
        }
        
        return result ? const_iterator(result, this) : end();
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
template <typename Key, typename Value, typename Compare = DefaultLess<Key>, typename Allocator = allocator_slab<char>>
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
    TreeType tree_;

public:
    using iterator = typename TreeType::iterator;
    using const_iterator = typename TreeType::const_iterator;

    // Constructors and destructor
    MapRedBlackTree(const Compare& comp = Compare(), const Allocator& alloc = Allocator()) 
        : tree_(PairCompare(comp), alloc) {}

    MapRedBlackTree(const MapRedBlackTree& other) = default;
    MapRedBlackTree& operator=(const MapRedBlackTree& other) = default;
    ~MapRedBlackTree() = default;

    // Iterators
    iterator begin() { return tree_.begin(); }
    const_iterator begin() const { return tree_.begin(); }
    const_iterator cbegin() const { return tree_.cbegin(); }
    iterator end() { return tree_.end(); }
    const_iterator end() const { return tree_.end(); }
    const_iterator cend() const { return tree_.cend(); }

    // Capacity
    bool empty() const { return tree_.empty(); }
    fl::size size() const { return tree_.size(); }
    fl::size max_size() const { return tree_.max_size(); }

    // Element access
    Value& operator[](const Key& key) {
        auto result = tree_.insert(value_type(key, Value()));
        return result.first->second;
    }

    Value& at(const Key& key) {
        auto it = tree_.find(value_type(key, Value()));
        FASTLED_ASSERT(it != tree_.end(), "MapRedBlackTree::at: key not found");
        return it->second;
    }

    const Value& at(const Key& key) const {
        auto it = tree_.find(value_type(key, Value()));
        FASTLED_ASSERT(it != tree_.end(), "MapRedBlackTree::at: key not found");
        return it->second;
    }

    // Modifiers
    void clear() { tree_.clear(); }

    fl::pair<iterator, bool> insert(const value_type& value) {
        return tree_.insert(value);
    }

    fl::pair<iterator, bool> insert(value_type&& value) {
        return tree_.insert(fl::move(value));
    }

    template<typename... Args>
    fl::pair<iterator, bool> emplace(Args&&... args) {
        return tree_.emplace(fl::forward<Args>(args)...);
    }

    iterator erase(const_iterator pos) {
        return tree_.erase(pos);
    }

    fl::size erase(const Key& key) {
        return tree_.erase(value_type(key, Value()));
    }

    void swap(MapRedBlackTree& other) {
        tree_.swap(other.tree_);
    }

    // Lookup
    fl::size count(const Key& key) const {
        return tree_.count(value_type(key, Value()));
    }

    iterator find(const Key& key) {
        return tree_.find(value_type(key, Value()));
    }

    const_iterator find(const Key& key) const {
        return tree_.find(value_type(key, Value()));
    }

    bool contains(const Key& key) const {
        return tree_.contains(value_type(key, Value()));
    }

    fl::pair<iterator, iterator> equal_range(const Key& key) {
        return tree_.equal_range(value_type(key, Value()));
    }

    fl::pair<const_iterator, const_iterator> equal_range(const Key& key) const {
        return tree_.equal_range(value_type(key, Value()));
    }

    iterator lower_bound(const Key& key) {
        return tree_.lower_bound(value_type(key, Value()));
    }

    const_iterator lower_bound(const Key& key) const {
        return tree_.lower_bound(value_type(key, Value()));
    }

    iterator upper_bound(const Key& key) {
        return tree_.upper_bound(value_type(key, Value()));
    }

    const_iterator upper_bound(const Key& key) const {
        return tree_.upper_bound(value_type(key, Value()));
    }

    // Observers
    key_compare key_comp() const {
        return tree_.value_comp().comp_;
    }

    // Comparison operators
    bool operator==(const MapRedBlackTree& other) const {
        if (tree_.size() != other.tree_.size()) return false;
        
        auto it1 = tree_.begin();
        auto it2 = other.tree_.begin();
        
        while (it1 != tree_.end() && it2 != other.tree_.end()) {
            // Compare both key and value
            if (it1->first != it2->first || it1->second != it2->second) {
                return false;
            }
            ++it1;
            ++it2;
        }
        
        return it1 == tree_.end() && it2 == other.tree_.end();
    }

    bool operator!=(const MapRedBlackTree& other) const {
        return !(*this == other);
    }
};

// Specialized Red-Black Tree for sets (just keys, no values)
template <typename Key, typename Compare = DefaultLess<Key>, typename Allocator = allocator_slab<char>>
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
    TreeType tree_;

public:
    using iterator = typename TreeType::const_iterator;  // Set iterators are always const
    using const_iterator = typename TreeType::const_iterator;

    // Constructors and destructor
    SetRedBlackTree(const Compare& comp = Compare(), const Allocator& alloc = Allocator()) 
        : tree_(comp, alloc) {}

    SetRedBlackTree(const SetRedBlackTree& other) = default;
    SetRedBlackTree& operator=(const SetRedBlackTree& other) = default;
    ~SetRedBlackTree() = default;

    // Iterators
    const_iterator begin() const { return tree_.begin(); }
    const_iterator cbegin() const { return tree_.cbegin(); }
    const_iterator end() const { return tree_.end(); }
    const_iterator cend() const { return tree_.cend(); }

    // Capacity
    bool empty() const { return tree_.empty(); }
    fl::size size() const { return tree_.size(); }
    fl::size max_size() const { return tree_.max_size(); }

    // Modifiers
    void clear() { tree_.clear(); }

    fl::pair<const_iterator, bool> insert(const value_type& value) {
        auto result = tree_.insert(value);
        return fl::pair<const_iterator, bool>(result.first, result.second);
    }

    fl::pair<const_iterator, bool> insert(value_type&& value) {
        auto result = tree_.insert(fl::move(value));
        return fl::pair<const_iterator, bool>(result.first, result.second);
    }

    template<typename... Args>
    fl::pair<const_iterator, bool> emplace(Args&&... args) {
        auto result = tree_.emplace(fl::forward<Args>(args)...);
        return fl::pair<const_iterator, bool>(result.first, result.second);
    }

    const_iterator erase(const_iterator pos) {
        return tree_.erase(pos);
    }

    fl::size erase(const Key& key) {
        return tree_.erase(key);
    }

    void swap(SetRedBlackTree& other) {
        tree_.swap(other.tree_);
    }

    // Lookup
    fl::size count(const Key& key) const {
        return tree_.count(key);
    }

    const_iterator find(const Key& key) const {
        return tree_.find(key);
    }

    bool contains(const Key& key) const {
        return tree_.contains(key);
    }

    fl::pair<const_iterator, const_iterator> equal_range(const Key& key) const {
        return tree_.equal_range(key);
    }

    const_iterator lower_bound(const Key& key) const {
        return tree_.lower_bound(key);
    }

    const_iterator upper_bound(const Key& key) const {
        return tree_.upper_bound(key);
    }

    // Observers
    key_compare key_comp() const {
        return tree_.value_comp();
    }

    // Comparison operators
    bool operator==(const SetRedBlackTree& other) const {
        return tree_ == other.tree_;
    }

    bool operator!=(const SetRedBlackTree& other) const {
        return tree_ != other.tree_;
    }
};

} // namespace fl
