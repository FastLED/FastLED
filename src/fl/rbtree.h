#pragma once

#include "fl/assert.h"
#include "fl/comparators.h"
#include "fl/namespace.h"
#include "fl/pair.h"
#include "fl/template_magic.h"
#include "fl/type_traits.h"
#include "fl/algorithm.h"
#include "fl/slab_allocator.h"

namespace fl {

// Red-Black Tree implementation
// This is a self-balancing binary search tree with O(log n) operations
template <typename Key, typename Value, typename Compare = DefaultLess<Key>, typename Allocator = allocator_slab<char>>
class MapRedBlackTree {
public:
    using key_type = Key;
    using mapped_type = Value;
    using value_type = fl::Pair<Key, Value>;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using key_compare = Compare;
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
        
        Node(const Key& k, const Value& v, Color c = RED, Node* p = nullptr)
            : data(k, v), color(c), left(nullptr), right(nullptr), parent(p) {}
    };

    using NodeAllocator = typename Allocator::template rebind<Node>::other;

    Node* root_;
    size_type size_;
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

    Node* findNode(const Key& key) const {
        Node* current = root_;
        while (current != nullptr) {
            if (comp_(key, current->data.first)) {
                current = current->left;
            } else if (comp_(current->data.first, key)) {
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
        friend class MapRedBlackTree;
        friend class const_iterator;
    private:
        Node* node_;
        const MapRedBlackTree* tree_;

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
        iterator(Node* n, const MapRedBlackTree* t) : node_(n), tree_(t) {}

        value_type& operator*() const { 
            FASTLED_ASSERT(node_ != nullptr, "MapRedBlackTree::iterator: dereferencing end iterator");
            return node_->data; 
        }
        value_type* operator->() const { 
            FASTLED_ASSERT(node_ != nullptr, "MapRedBlackTree::iterator: dereferencing end iterator");
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
        friend class MapRedBlackTree;
        friend class iterator;
    private:
        const Node* node_;
        const MapRedBlackTree* tree_;

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
        const_iterator(const Node* n, const MapRedBlackTree* t) : node_(n), tree_(t) {}
        const_iterator(const iterator& it) : node_(it.node_), tree_(it.tree_) {}

        const value_type& operator*() const { 
            FASTLED_ASSERT(node_ != nullptr, "MapRedBlackTree::iterator: dereferencing end iterator");
            return node_->data; 
        }
        const value_type* operator->() const { 
            FASTLED_ASSERT(node_ != nullptr, "MapRedBlackTree::iterator: dereferencing end iterator");
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
    MapRedBlackTree(const Compare& comp = Compare(), const Allocator& alloc = Allocator()) 
        : root_(nullptr), size_(0), comp_(comp), alloc_(alloc) {}

    MapRedBlackTree(const MapRedBlackTree& other) 
        : root_(nullptr), size_(other.size_), comp_(other.comp_), alloc_(other.alloc_) {
        if (other.root_) {
            root_ = copyTree(other.root_);
        }
    }

    MapRedBlackTree& operator=(const MapRedBlackTree& other) {
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

    ~MapRedBlackTree() {
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
    size_type size() const { return size_; }
    size_type max_size() const { return size_type(-1); }

    // Element access
    Value& operator[](const Key& key) {
        Node* node = findNode(key);
        if (node) {
            return node->data.second;
        }
        
        // Insert new node
        Node* newNode = alloc_.allocate(1);
        if (newNode == nullptr) {
            // Out of memory - we cannot create the value
            // This will cause undefined behavior, but it's better than segfault
            static Value default_value;
            return default_value;
        }
        alloc_.construct(newNode, key, Value());
        
        if (root_ == nullptr) {
            root_ = newNode;
            root_->color = BLACK;
        } else {
            Node* parent = nullptr;
            Node* current = root_;
            
            while (current != nullptr) {
                parent = current;
                if (comp_(key, current->data.first)) {
                    current = current->left;
                } else {
                    current = current->right;
                }
            }
            
            newNode->parent = parent;
            if (comp_(key, parent->data.first)) {
                parent->left = newNode;
            } else {
                parent->right = newNode;
            }
            
            insertFixup(newNode);
        }
        
        ++size_;
        return newNode->data.second;
    }

    Value& at(const Key& key) {
        Node* node = findNode(key);
        FASTLED_ASSERT(node != nullptr, "MapRedBlackTree::at: key not found");
        return node->data.second;
    }

    const Value& at(const Key& key) const {
        Node* node = findNode(key);
        FASTLED_ASSERT(node != nullptr, "MapRedBlackTree::at: key not found");
        return node->data.second;
    }

    // Modifiers
    void clear() {
        destroyTree(root_);
        root_ = nullptr;
        size_ = 0;
    }

    fl::Pair<iterator, bool> insert(const value_type& value) {
        Node* existing = findNode(value.first);
        if (existing) {
            return fl::Pair<iterator, bool>(iterator(existing, this), false);
        }

        Node* newNode = alloc_.allocate(1);
        if (newNode == nullptr) {
            // Out of memory - return failed insertion
            return fl::Pair<iterator, bool>(end(), false);
        }
        alloc_.construct(newNode, value);
        
        if (root_ == nullptr) {
            root_ = newNode;
            root_->color = BLACK;
        } else {
            Node* parent = nullptr;
            Node* current = root_;
            
            while (current != nullptr) {
                parent = current;
                if (comp_(value.first, current->data.first)) {
                    current = current->left;
                } else {
                    current = current->right;
                }
            }
            
            newNode->parent = parent;
            if (comp_(value.first, parent->data.first)) {
                parent->left = newNode;
            } else {
                parent->right = newNode;
            }
            
            insertFixup(newNode);
        }
        
        ++size_;
        return fl::Pair<iterator, bool>(iterator(newNode, this), true);
    }

    template<class... Args>
    fl::Pair<iterator, bool> emplace(Args&&... args) {
        value_type value(fl::forward<Args>(args)...);
        return insert(value);
    }

    iterator erase(const_iterator pos) {
        if (pos == end()) return end();
        
        Node* nodeToDelete = const_cast<Node*>(pos.node_);
        Node* successor = nullptr;
        
        if (pos != end()) {
            const_iterator next = pos;
            ++next;
            successor = const_cast<Node*>(next.node_);
        }
        
        erase(nodeToDelete->data.first);
        return iterator(successor, this);
    }

    size_type erase(const Key& key) {
        Node* nodeToDelete = findNode(key);
        if (nodeToDelete == nullptr) {
            return 0;
        }

        Node* y = nodeToDelete;
        Node* x;
        Color yOriginalColor = y->color;

        if (nodeToDelete->left == nullptr) {
            x = nodeToDelete->right;
            transplant(nodeToDelete, nodeToDelete->right);
        } else if (nodeToDelete->right == nullptr) {
            x = nodeToDelete->left;
            transplant(nodeToDelete, nodeToDelete->left);
        } else {
            y = minimum(nodeToDelete->right);
            yOriginalColor = y->color;
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

        if (yOriginalColor == BLACK) {
            deleteFixup(x);
        }

        return 1;
    }

    void swap(MapRedBlackTree& other) {
        fl::swap(root_, other.root_);
        fl::swap(size_, other.size_);
        fl::swap(comp_, other.comp_);
    }

    // Lookup
    size_type count(const Key& key) const {
        return findNode(key) != nullptr ? 1 : 0;
    }

    iterator find(const Key& key) {
        Node* node = findNode(key);
        return node ? iterator(node, this) : end();
    }

    const_iterator find(const Key& key) const {
        Node* node = findNode(key);
        return node ? const_iterator(node, this) : end();
    }

    bool contains(const Key& key) const {
        return findNode(key) != nullptr;
    }

    fl::Pair<iterator, iterator> equal_range(const Key& key) {
        iterator it = find(key);
        if (it == end()) {
            return fl::Pair<iterator, iterator>(it, it);
        }
        iterator next = it;
        ++next;
        return fl::Pair<iterator, iterator>(it, next);
    }

    fl::Pair<const_iterator, const_iterator> equal_range(const Key& key) const {
        const_iterator it = find(key);
        if (it == end()) {
            return fl::Pair<const_iterator, const_iterator>(it, it);
        }
        const_iterator next = it;
        ++next;
        return fl::Pair<const_iterator, const_iterator>(it, next);
    }

    iterator lower_bound(const Key& key) {
        Node* current = root_;
        Node* result = nullptr;
        
        while (current != nullptr) {
            if (!comp_(current->data.first, key)) {
                result = current;
                current = current->left;
            } else {
                current = current->right;
            }
        }
        
        return result ? iterator(result, this) : end();
    }

    const_iterator lower_bound(const Key& key) const {
        Node* current = root_;
        Node* result = nullptr;
        
        while (current != nullptr) {
            if (!comp_(current->data.first, key)) {
                result = current;
                current = current->left;
            } else {
                current = current->right;
            }
        }
        
        return result ? const_iterator(result, this) : end();
    }

    iterator upper_bound(const Key& key) {
        Node* current = root_;
        Node* result = nullptr;
        
        while (current != nullptr) {
            if (comp_(key, current->data.first)) {
                result = current;
                current = current->left;
            } else {
                current = current->right;
            }
        }
        
        return result ? iterator(result, this) : end();
    }

    const_iterator upper_bound(const Key& key) const {
        Node* current = root_;
        Node* result = nullptr;
        
        while (current != nullptr) {
            if (comp_(key, current->data.first)) {
                result = current;
                current = current->left;
            } else {
                current = current->right;
            }
        }
        
        return result ? const_iterator(result, this) : end();
    }

    // Observers
    key_compare key_comp() const {
        return comp_;
    }

    // Comparison operators
    bool operator==(const MapRedBlackTree& other) const {
        if (size() != other.size()) {
            return false;
        }
        
        const_iterator it1 = begin();
        const_iterator it2 = other.begin();
        
        while (it1 != end() && it2 != other.end()) {
            if (it1->first != it2->first || it1->second != it2->second) {
                return false;
            }
            ++it1;
            ++it2;
        }
        
        return it1 == end() && it2 == other.end();
    }

    bool operator!=(const MapRedBlackTree& other) const {
        return !(*this == other);
    }
};

} // namespace fl
