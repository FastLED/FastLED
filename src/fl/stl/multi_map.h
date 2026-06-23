#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/assert.h"  // IWYU pragma: keep
#include "fl/stl/comparators.h"  // IWYU pragma: keep
#include "fl/stl/pair.h"
#include "fl/stl/type_traits.h"  // IWYU pragma: keep
#include "fl/stl/detail/rbtree.h"
#include "fl/stl/allocator.h"
#include "fl/stl/noexcept.h"

namespace fl {

// Forward declaration for drop-in std::multimap replacement
template <typename Key, typename Value, typename Compare = less<Key>, typename Allocator = allocator<fl::pair<Key, Value>>>
class MultiMap;

// Specialized Red-Black Tree for multimaps (allows duplicate keys)
// Based on MapRedBlackTree but with modified insert logic
template <typename Key, typename Value, typename Compare = less<Key>, typename Allocator = allocator<fl::pair<Key, Value>>>
class MultiMapTree {
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
    // Forward declare iterator wrapper classes
    class IteratorWrapper;
    class ConstIteratorWrapper;
    class ReverseIteratorWrapper;
    class ConstReverseIteratorWrapper;

    // Store pairs along with unique IDs to allow duplicate keys
    struct ValueWithId {
        value_type value;
        fl::size unique_id;

        ValueWithId() FL_NOEXCEPT = default;
        ValueWithId(const value_type& v, fl::size id) : value(v), unique_id(id) {}
        ValueWithId(value_type&& v, fl::size id) : value(fl::move(v)), unique_id(id) {}

        // For comparisons - only compare keys, not IDs
        const Key& get_key() const { return value.first; }
    };

    // Comparator that compares only keys
    struct PairCompareWithId {
        Compare mComp;

        PairCompareWithId(const Compare& comp = Compare()) : mComp(comp) {}

        bool operator()(const ValueWithId& a, const ValueWithId& b) const {
            // Compare keys; if keys are equal, compare IDs to maintain stable order
            if (mComp(a.value.first, b.value.first)) {
                return true;
            }
            if (mComp(b.value.first, a.value.first)) {
                return false;
            }
            // Keys are equal - use ID for ordering
            return a.unique_id < b.unique_id;
        }
    };

    using TreeType = RedBlackTree<ValueWithId, PairCompareWithId, Allocator>;
    TreeType mTree;
    fl::size mNextId = 0;

    // Convert tree iterator to our iterator
    class IteratorWrapper {
    private:
        friend class MultiMapTree;
        friend class ConstIteratorWrapper;
        typename TreeType::iterator mTreeIt;

    public:
        IteratorWrapper() FL_NOEXCEPT = default;
        explicit IteratorWrapper(typename TreeType::iterator it) : mTreeIt(it) {}

        value_type& operator*() {
            return (*mTreeIt).value;
        }
        value_type* operator->() {
            return &((*mTreeIt).value);
        }

        IteratorWrapper& operator++() {
            ++mTreeIt;
            return *this;
        }

        IteratorWrapper operator++(int) {
            IteratorWrapper tmp = *this;
            ++mTreeIt;
            return tmp;
        }

        IteratorWrapper& operator--() {
            --mTreeIt;
            return *this;
        }

        IteratorWrapper operator--(int) {
            IteratorWrapper tmp = *this;
            --mTreeIt;
            return tmp;
        }

        bool operator==(const IteratorWrapper& other) const { return mTreeIt == other.mTreeIt; }
        bool operator!=(const IteratorWrapper& other) const { return mTreeIt != other.mTreeIt; }
    };

    class ConstIteratorWrapper {
    private:
        friend class MultiMapTree;
        friend class IteratorWrapper;
        typename TreeType::const_iterator mTreeIt;

    public:
        ConstIteratorWrapper() FL_NOEXCEPT = default;
        explicit ConstIteratorWrapper(typename TreeType::const_iterator it) : mTreeIt(it) {}
        // Allow implicit conversion from non-const iterator
        ConstIteratorWrapper(const IteratorWrapper& it) : mTreeIt(it.mTreeIt) {}

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

    class ReverseIteratorWrapper {
    private:
        friend class MultiMapTree;
        friend class ConstReverseIteratorWrapper;
        typename TreeType::reverse_iterator mTreeRIt;

    public:
        ReverseIteratorWrapper() FL_NOEXCEPT = default;
        explicit ReverseIteratorWrapper(typename TreeType::reverse_iterator it) : mTreeRIt(it) {}

        value_type& operator*() {
            return (*mTreeRIt).value;
        }
        value_type* operator->() {
            return &((*mTreeRIt).value);
        }

        ReverseIteratorWrapper& operator++() {
            ++mTreeRIt;
            return *this;
        }

        ReverseIteratorWrapper operator++(int) {
            ReverseIteratorWrapper tmp = *this;
            ++mTreeRIt;
            return tmp;
        }

        ReverseIteratorWrapper& operator--() {
            --mTreeRIt;
            return *this;
        }

        ReverseIteratorWrapper operator--(int) {
            ReverseIteratorWrapper tmp = *this;
            --mTreeRIt;
            return tmp;
        }

        bool operator==(const ReverseIteratorWrapper& other) const { return mTreeRIt == other.mTreeRIt; }
        bool operator!=(const ReverseIteratorWrapper& other) const { return mTreeRIt != other.mTreeRIt; }
    };

    class ConstReverseIteratorWrapper {
    private:
        friend class MultiMapTree;
        typename TreeType::const_reverse_iterator mTreeRIt;

    public:
        ConstReverseIteratorWrapper() FL_NOEXCEPT = default;
        explicit ConstReverseIteratorWrapper(typename TreeType::const_reverse_iterator it) : mTreeRIt(it) {}
        // Allow implicit conversion from non-const reverse iterator
        ConstReverseIteratorWrapper(const ReverseIteratorWrapper& it) : mTreeRIt(it.mTreeRIt) {}

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
    using iterator = IteratorWrapper;
    using const_iterator = ConstIteratorWrapper;
    using reverse_iterator = ReverseIteratorWrapper;
    using const_reverse_iterator = ConstReverseIteratorWrapper;

    // Value comparison class for std::multimap compatibility
    class value_compare {
        friend class MultiMapTree;
        Compare mComp;
        value_compare(Compare c) : mComp(c) {}
    public:
        bool operator()(const value_type& x, const value_type& y) const {
            return mComp(x.first, y.first);
        }
    };

    // Constructors and destructor
    MultiMapTree(const Compare& comp = Compare(), const Allocator& alloc = Allocator())
        : mTree(PairCompareWithId(comp), alloc) {}

    MultiMapTree(const MultiMapTree& other) FL_NOEXCEPT = default;
    MultiMapTree& operator=(const MultiMapTree& other) FL_NOEXCEPT = default;

    MultiMapTree(MultiMapTree&& other) FL_NOEXCEPT = default;
    MultiMapTree& operator=(MultiMapTree&& other) FL_NOEXCEPT = default;

    // Initializer list constructor
    MultiMapTree(fl::initializer_list<value_type> init,
                const Compare& comp = Compare(),
                const Allocator& alloc = Allocator())
        : mTree(PairCompareWithId(comp), alloc) {
        for (const auto& value : init) {
            insert(value);
        }
    }

    ~MultiMapTree() FL_NOEXCEPT = default;

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

    // Insert always succeeds for multimap
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
    iterator erase(iterator pos) {
        return iterator(mTree.erase(pos.mTreeIt));
    }

    // Also accept const_iterator for STL compatibility
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

    void swap(MultiMapTree& other) {
        mTree.swap(other.mTree);
        fl::swap(mNextId, other.mNextId);
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
        ValueWithId search_key(value_type(key, Value()), 0);
        auto it = mTree.lower_bound(search_key);
        if (it != mTree.end() && it->value.first == key) {
            return iterator(it);
        }
        return end();
    }

    const_iterator find(const Key& key) const {
        ValueWithId search_key(value_type(key, Value()), 0);
        auto it = mTree.lower_bound(search_key);
        if (it != mTree.end() && it->value.first == key) {
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
        ValueWithId search_key(value_type(key, Value()), 0);
        return iterator(mTree.lower_bound(search_key));
    }

    const_iterator lower_bound(const Key& key) const {
        ValueWithId search_key(value_type(key, Value()), 0);
        return const_iterator(mTree.lower_bound(search_key));
    }

    iterator upper_bound(const Key& key) {
        ValueWithId search_key(value_type(key, Value()), mNextId);  // Use mNextId to get upper bound
        return iterator(mTree.upper_bound(search_key));
    }

    const_iterator upper_bound(const Key& key) const {
        ValueWithId search_key(value_type(key, Value()), mNextId);
        return const_iterator(mTree.upper_bound(search_key));
    }

    // Observers
    key_compare key_comp() const { return key_compare(); }

    value_compare value_comp() const {
        return value_compare(key_compare());
    }

    allocator_type get_allocator() const {
        return mTree.get_allocator();
    }

    // Comparison operators
    bool operator==(const MultiMapTree& other) const {
        if (size() != other.size()) return false;
        for (auto it1 = begin(), it2 = other.begin(); it1 != end(); ++it1, ++it2) {
            if (it1->first != it2->first || it1->second != it2->second) {
                return false;
            }
        }
        return true;
    }

    bool operator!=(const MultiMapTree& other) const {
        return !(*this == other);
    }

    bool operator<(const MultiMapTree& other) const {
        for (auto it1 = begin(), it2 = other.begin(); it1 != end() && it2 != other.end(); ++it1, ++it2) {
            if (it1->first < it2->first) return true;
            if (it2->first < it1->first) return false;
            if (it1->second < it2->second) return true;
            if (it2->second < it1->second) return false;
        }
        return size() < other.size();
    }

    bool operator<=(const MultiMapTree& other) const {
        return *this < other || *this == other;
    }

    bool operator>(const MultiMapTree& other) const {
        return other < *this;
    }

    bool operator>=(const MultiMapTree& other) const {
        return other <= *this;
    }
};

} // namespace fl

// Drop-in replacement for std::multimap
namespace fl {

// Default multimap uses slab allocator for better performance
// Matches the behavior of fl::map
template <typename Key, typename T, typename Compare = fl::less<Key>>
using multi_map = MultiMapTree<Key, T, Compare, fl::allocator_slab<char>>;

} // namespace fl
