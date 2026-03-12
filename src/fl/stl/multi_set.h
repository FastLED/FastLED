#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/assert.h"  // IWYU pragma: keep
#include "fl/stl/comparators.h"  // IWYU pragma: keep
#include "fl/stl/type_traits.h"  // IWYU pragma: keep
#include "fl/stl/detail/rbtree.h"
#include "fl/stl/allocator.h"

namespace fl {

// Forward declaration for drop-in std::multiset replacement
template <typename Key, typename Compare = less<Key>, typename Allocator = allocator<Key>>
class MultiSet;

// Specialized Red-Black Tree for multisets (allows duplicate keys)
// Based on SetRedBlackTree but with modified insert logic
template <typename Key, typename Compare = less<Key>, typename Allocator = allocator<Key>>
class MultiSetTree {
public:
    using key_type = Key;
    using value_type = Key;
    using size_type = fl::size;
    using difference_type = ptrdiff_t;
    using key_compare = Compare;
    using value_compare = Compare;
    using reference = const value_type&;
    using const_reference = const value_type&;
    using pointer = const value_type*;
    using const_pointer = const value_type*;
    using allocator_type = Allocator;

private:
    // Forward declare iterator wrapper classes
    class ConstIteratorWrapper;
    class ConstReverseIteratorWrapper;

    // Store keys along with unique IDs to allow duplicate keys
    struct ValueWithId {
        Key value;
        fl::size unique_id;

        ValueWithId() = default;
        ValueWithId(const Key& v, fl::size id) : value(v), unique_id(id) {}
        ValueWithId(Key&& v, fl::size id) : value(fl::move(v)), unique_id(id) {}

        // For comparisons - only compare keys, not IDs
        const Key& get_key() const { return value; }
    };

    // Comparator that compares only keys
    struct KeyCompareWithId {
        Compare mComp;

        KeyCompareWithId(const Compare& comp = Compare()) : mComp(comp) {}

        bool operator()(const ValueWithId& a, const ValueWithId& b) const {
            // Compare keys; if keys are equal, compare IDs to maintain stable order
            if (mComp(a.value, b.value)) {
                return true;
            }
            if (mComp(b.value, a.value)) {
                return false;
            }
            // Keys are equal - use ID for ordering
            return a.unique_id < b.unique_id;
        }
    };

    using TreeType = RedBlackTree<ValueWithId, KeyCompareWithId, Allocator>;
    TreeType mTree;
    fl::size mNextId = 0;

    // Convert tree iterator to our iterator
    class ConstIteratorWrapper {
    private:
        friend class MultiSetTree;
        typename TreeType::const_iterator mTreeIt;

    public:
        ConstIteratorWrapper() = default;
        explicit ConstIteratorWrapper(typename TreeType::const_iterator it) : mTreeIt(it) {}

        const value_type& operator*() const {
            return (*mTreeIt).value;
        }
        const value_type* operator->() const {
            return &((*mTreeIt).value);
        }

        ConstIteratorWrapper& operator++() {
            ++mTreeIt;
            return *this;
        }

        ConstIteratorWrapper operator++(int) {
            ConstIteratorWrapper tmp = *this;
            ++mTreeIt;
            return tmp;
        }

        ConstIteratorWrapper& operator--() {
            --mTreeIt;
            return *this;
        }

        ConstIteratorWrapper operator--(int) {
            ConstIteratorWrapper tmp = *this;
            --mTreeIt;
            return tmp;
        }

        bool operator==(const ConstIteratorWrapper& other) const { return mTreeIt == other.mTreeIt; }
        bool operator!=(const ConstIteratorWrapper& other) const { return mTreeIt != other.mTreeIt; }
    };

    class ConstReverseIteratorWrapper {
    private:
        friend class MultiSetTree;
        typename TreeType::const_reverse_iterator mTreeRIt;

    public:
        ConstReverseIteratorWrapper() = default;
        explicit ConstReverseIteratorWrapper(typename TreeType::const_reverse_iterator it) : mTreeRIt(it) {}

        const value_type& operator*() const {
            return (*mTreeRIt).value;
        }
        const value_type* operator->() const {
            return &((*mTreeRIt).value);
        }

        ConstReverseIteratorWrapper& operator++() {
            ++mTreeRIt;
            return *this;
        }

        ConstReverseIteratorWrapper operator++(int) {
            ConstReverseIteratorWrapper tmp = *this;
            ++mTreeRIt;
            return tmp;
        }

        ConstReverseIteratorWrapper& operator--() {
            --mTreeRIt;
            return *this;
        }

        ConstReverseIteratorWrapper operator--(int) {
            ConstReverseIteratorWrapper tmp = *this;
            --mTreeRIt;
            return tmp;
        }

        bool operator==(const ConstReverseIteratorWrapper& other) const { return mTreeRIt == other.mTreeRIt; }
        bool operator!=(const ConstReverseIteratorWrapper& other) const { return mTreeRIt != other.mTreeRIt; }
    };

public:
    using iterator = ConstIteratorWrapper;
    using const_iterator = ConstIteratorWrapper;
    using reverse_iterator = ConstReverseIteratorWrapper;
    using const_reverse_iterator = ConstReverseIteratorWrapper;

    // Constructors and destructor
    MultiSetTree(const Compare& comp = Compare(), const Allocator& alloc = Allocator())
        : mTree(KeyCompareWithId(comp), alloc) {}

    MultiSetTree(const MultiSetTree& other) = default;
    MultiSetTree& operator=(const MultiSetTree& other) = default;

    MultiSetTree(MultiSetTree&& other) = default;
    MultiSetTree& operator=(MultiSetTree&& other) = default;

    // Initializer list constructor
    MultiSetTree(fl::initializer_list<value_type> init,
                const Compare& comp = Compare(),
                const Allocator& alloc = Allocator())
        : mTree(KeyCompareWithId(comp), alloc) {
        for (const auto& value : init) {
            insert(value);
        }
    }

    ~MultiSetTree() = default;

    // Iterators
    iterator begin() { return iterator(mTree.begin()); }
    const_iterator begin() const { return const_iterator(mTree.begin()); }
    const_iterator cbegin() const { return const_iterator(mTree.cbegin()); }
    iterator end() { return iterator(mTree.end()); }
    const_iterator end() const { return const_iterator(mTree.end()); }
    const_iterator cend() const { return const_iterator(mTree.cend()); }

    // Reverse iterators
    reverse_iterator rbegin() { return reverse_iterator(mTree.rbegin()); }
    reverse_iterator rend() { return reverse_iterator(mTree.rend()); }
    const_reverse_iterator rbegin() const { return const_reverse_iterator(mTree.rbegin()); }
    const_reverse_iterator rend() const { return const_reverse_iterator(mTree.rend()); }

    // Capacity
    bool empty() const { return mTree.empty(); }
    fl::size size() const { return mTree.size(); }
    fl::size max_size() const { return mTree.max_size(); }

    // Modifiers
    void clear() { mTree.clear(); mNextId = 0; }

    // Insert always succeeds for multiset
    iterator insert(const value_type& value) {
        ValueWithId vwid(value, mNextId++);
        auto result = mTree.insert(vwid);
        return iterator(result.first);
    }

    iterator insert(value_type&& value) {
        ValueWithId vwid(fl::move(value), mNextId++);
        auto result = mTree.insert(fl::move(vwid));
        return iterator(result.first);
    }

    template<typename... Args>
    iterator emplace(Args&&... args) {
        value_type temp(fl::forward<Args>(args)...);
        ValueWithId vwid(fl::move(temp), mNextId++);
        auto result = mTree.insert(fl::move(vwid));
        return iterator(result.first);
    }

    // Range insert
    template <typename InputIt>
    void insert(InputIt first, InputIt last) {
        for (InputIt it = first; it != last; ++it) {
            insert(*it);
        }
    }

    // Initializer list insert
    void insert(fl::initializer_list<value_type> ilist) {
        for (const auto& value : ilist) {
            insert(value);
        }
    }

    // Erase by iterator
    iterator erase(const_iterator pos) {
        return iterator(mTree.erase(pos.mTreeIt));
    }

    // Erase all elements with the given key
    fl::size erase(const Key& key) {
        auto range = equal_range(key);
        fl::size count = 0;
        auto it = range.first;
        while (it != range.second) {
            it = erase(it);
            ++count;
        }
        return count;
    }

    // Range erase
    iterator erase(const_iterator first, const_iterator last) {
        // Erase each element in the range [first, last)
        iterator result = begin();  // Default return

        while (first != last) {
            result = erase(first);  // erase(const_iterator pos) returns iterator to next element
            // Update first to point to the next element (which is now at the erased position)
            first = ConstIteratorWrapper(result.mTreeIt);
        }

        return result;
    }

    void swap(MultiSetTree& other) {
        mTree.swap(other.mTree);
        fl::fl_swap(mNextId, other.mNextId);
    }

    // Lookup
    fl::size count(const Key& key) const {
        auto range = equal_range(key);
        fl::size count = 0;
        for (auto it = range.first; it != range.second; ++it) {
            ++count;
        }
        return count;
    }

    iterator find(const Key& key) {
        ValueWithId search_key(key, 0);
        auto it = mTree.lower_bound(search_key);
        if (it != mTree.end() && it->value == key) {
            return iterator(it);
        }
        return end();
    }

    const_iterator find(const Key& key) const {
        ValueWithId search_key(key, 0);
        auto it = mTree.lower_bound(search_key);
        if (it != mTree.end() && it->value == key) {
            return const_iterator(it);
        }
        return end();
    }

    bool contains(const Key& key) const {
        return find(key) != end();
    }

    // equal_range returns [lower_bound, upper_bound) for the given key
    fl::pair<iterator, iterator> equal_range(const Key& key) {
        iterator lower = lower_bound(key);
        iterator upper = upper_bound(key);
        return fl::pair<iterator, iterator>(lower, upper);
    }

    fl::pair<const_iterator, const_iterator> equal_range(const Key& key) const {
        const_iterator lower = lower_bound(key);
        const_iterator upper = upper_bound(key);
        return fl::pair<const_iterator, const_iterator>(lower, upper);
    }

    iterator lower_bound(const Key& key) {
        ValueWithId search_key(key, 0);
        return iterator(mTree.lower_bound(search_key));
    }

    const_iterator lower_bound(const Key& key) const {
        ValueWithId search_key(key, 0);
        return const_iterator(mTree.lower_bound(search_key));
    }

    iterator upper_bound(const Key& key) {
        ValueWithId search_key(key, mNextId);  // Use mNextId to get upper bound
        return iterator(mTree.upper_bound(search_key));
    }

    const_iterator upper_bound(const Key& key) const {
        ValueWithId search_key(key, mNextId);
        return const_iterator(mTree.upper_bound(search_key));
    }

    // Observers
    key_compare key_comp() const { return key_compare(); }

    value_compare value_comp() const {
        return value_compare();
    }

    allocator_type get_allocator() const {
        return mTree.get_allocator();
    }

    // Comparison operators
    bool operator==(const MultiSetTree& other) const {
        if (size() != other.size()) return false;
        for (auto it1 = begin(), it2 = other.begin(); it1 != end(); ++it1, ++it2) {
            if (*it1 != *it2) {
                return false;
            }
        }
        return true;
    }

    bool operator!=(const MultiSetTree& other) const {
        return !(*this == other);
    }

    bool operator<(const MultiSetTree& other) const {
        for (auto it1 = begin(), it2 = other.begin(); it1 != end() && it2 != other.end(); ++it1, ++it2) {
            if (*it1 < *it2) return true;
            if (*it2 < *it1) return false;
        }
        return size() < other.size();
    }

    bool operator<=(const MultiSetTree& other) const {
        return *this < other || *this == other;
    }

    bool operator>(const MultiSetTree& other) const {
        return other < *this;
    }

    bool operator>=(const MultiSetTree& other) const {
        return other <= *this;
    }
};

} // namespace fl

// Drop-in replacement for std::multiset
namespace fl {

// Default multiset uses slab allocator for better performance
template <typename Key, typename Compare = fl::less<Key>>
using multi_set = MultiSetTree<Key, Compare, fl::allocator_slab<char>>;

template <typename Key, typename Compare, typename Allocator>
void fl_swap(MultiSetTree<Key, Compare, Allocator>& lhs, MultiSetTree<Key, Compare, Allocator>& rhs) {
    lhs.swap(rhs);
}
} // namespace fl
