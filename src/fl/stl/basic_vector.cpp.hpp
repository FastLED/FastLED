// IWYU pragma: private

/// @file basic_vector.cpp.hpp
/// @brief Compiled-once implementation of vector_basic methods.
/// Included from exactly one translation unit (fl.stl+.cpp via _build.cpp.hpp).

#include "fl/stl/basic_vector.h"
#include "fl/stl/cstring.h"  // fl::memcpy, fl::memmove, fl::memset
#include "platforms/assert_defs.h"  // FASTLED_ASSERT
#include "fl/stl/noexcept.h"

namespace fl {

// ======= TRIVIAL ELEMENT HELPERS =======

void vector_basic::trivial_copy(void* dst, const void* src, fl::size count) const {
    fl::memcpy(dst, src, count * mElementSize);
}

void vector_basic::trivial_move_left(void* dst, const void* src, fl::size count) const {
    fl::memmove(dst, src, count * mElementSize);
}

void vector_basic::trivial_default_construct(void* ptr, fl::size count) const {
    fl::memset(ptr, 0, count * mElementSize);
}

void vector_basic::trivial_swap(void* a, void* b) const {
    // Stack buffer for small elements, heap for large
    char stack_buf[64];
    void* tmp;
    bool heap_allocated = false;
    if (mElementSize <= sizeof(stack_buf)) {
        tmp = stack_buf;
    } else {
        tmp = mResource->allocate(mElementSize);
        heap_allocated = true;
    }
    fl::memcpy(tmp, a, mElementSize);
    fl::memcpy(a, b, mElementSize);
    fl::memcpy(b, tmp, mElementSize);
    if (heap_allocated) {
        mResource->deallocate(tmp, mElementSize);
    }
}

// ======= CAPACITY / GROWTH =======

void vector_basic::ensure_capacity(fl::size n) {
    if (n <= mCapacity) return;
    fl::size new_capacity = (3 * mCapacity) / 2;
    if (new_capacity < n) {
        new_capacity = n;
    }
    grow_to(new_capacity);
}

void vector_basic::grow_to(fl::size new_capacity) {
    // PATH 1: Try in-place reallocate if on heap and trivially copyable
    if (!isInline() && mArray && !mOps) {
        void* result = mResource->reallocate(mArray, mCapacity * mElementSize,
                                              new_capacity * mElementSize);
        if (result) {
            mArray = result;
            mCapacity = new_capacity;
            return;
        }
    }

    // PATH 2: Allocate new buffer, move elements, free old
    FASTLED_ASSERT(mResource != nullptr, "memory_resource is null");
    fl::size alloc_bytes = new_capacity * mElementSize;
    FASTLED_ASSERT(alloc_bytes > 0, "zero allocation in grow_to");
    void* new_buf = mResource->allocate(alloc_bytes);
    FASTLED_ASSERT(new_buf != nullptr, "allocation failed in grow_to");
    if (!new_buf) return;

    // Move existing elements to new buffer
    if (mSize > 0 && mArray) {
        if (mOps) {
            mOps->uninitialized_move_n(new_buf, mArray, mSize);
            mOps->destroy_n(mArray, mSize);
        } else {
            trivial_copy(new_buf, mArray, mSize);
        }
    }

    // Free old buffer (only if it's on the heap, not inline)
    if (!isInline() && mArray) {
        mResource->deallocate(mArray, mCapacity * mElementSize);
    }

    mArray = new_buf;
    mCapacity = new_capacity;
}

void vector_basic::reserve_impl(fl::size n) {
    if (n > mCapacity) {
        grow_to(n);
    }
}

void vector_basic::shrink_to_fit_impl() {
    if (mSize == 0) {
        if (!isInline() && mArray) {
            mResource->deallocate(mArray, mCapacity * mElementSize);
        }
        if (hasInlineBuffer()) {
            mArray = inlineBufferPtr();
            mCapacity = mInlineCapacity;
        } else {
            mArray = nullptr;
            mCapacity = 0;
        }
        return;
    }

    // If data fits in inline buffer and we're on heap, move back to inline
    if (hasInlineBuffer() && !isInline() && mSize <= mInlineCapacity) {
        void* inline_buf = inlineBufferPtr();
        if (mOps) {
            mOps->uninitialized_move_n(inline_buf, mArray, mSize);
            mOps->destroy_n(mArray, mSize);
        } else {
            trivial_copy(inline_buf, mArray, mSize);
        }
        mResource->deallocate(mArray, mCapacity * mElementSize);
        mArray = inline_buf;
        mCapacity = mInlineCapacity;
        return;
    }

    // Shrink heap allocation
    if (!isInline() && mCapacity > mSize) {
        void* new_buf = mResource->allocate(mSize * mElementSize);
        if (new_buf) {
            if (mOps) {
                mOps->uninitialized_move_n(new_buf, mArray, mSize);
                mOps->destroy_n(mArray, mSize);
            } else {
                trivial_copy(new_buf, mArray, mSize);
            }
            mResource->deallocate(mArray, mCapacity * mElementSize);
            mArray = new_buf;
            mCapacity = mSize;
        }
    }
}

// ======= PUSH / POP =======

void vector_basic::push_back_copy_impl(const void* element) {
    ensure_capacity(mSize + 1);
    if (mSize >= mCapacity) return;  // allocation failed
    void* dst = element_ptr(mSize);
    if (mOps) {
        mOps->copy_construct(dst, element);
    } else {
        fl::memcpy(dst, element, mElementSize);
    }
    ++mSize;
}

void vector_basic::push_back_move_impl(void* element) {
    ensure_capacity(mSize + 1);
    if (mSize >= mCapacity) return;  // allocation failed
    FASTLED_ASSERT(mArray != nullptr, "mArray null after ensure_capacity in push_back_move");
    void* dst = element_ptr(mSize);
    if (mOps) {
        mOps->move_construct(dst, element);
    } else {
        fl::memcpy(dst, element, mElementSize);
    }
    ++mSize;
}

void vector_basic::pop_back_impl() {
    if (mSize == 0) return;
    --mSize;
    if (mOps) {
        mOps->destroy(element_ptr(mSize));
    }
    // For trivial types, nothing to destroy
}

// ======= CLEAR =======

void vector_basic::clear_impl() {
    if (mArray && mSize > 0) {
        if (mOps) {
            mOps->destroy_n(mArray, mSize);
        }
        // For trivial types, nothing to destroy
    }
    mSize = 0;
}

// ======= ERASE =======

void vector_basic::erase_impl(fl::size index) {
    if (index >= mSize) return;
    erase_range_impl(index, 1);
}

void vector_basic::erase_range_impl(fl::size first_index, fl::size count) {
    if (count == 0 || first_index >= mSize) return;
    if (first_index + count > mSize) {
        count = mSize - first_index;
    }

    fl::size remaining = mSize - first_index - count;

    if (mOps) {
        // Destroy erased elements
        for (fl::size i = 0; i < count; ++i) {
            mOps->destroy(element_ptr(first_index + i));
        }
        // Shift remaining elements left one at a time
        for (fl::size i = 0; i < remaining; ++i) {
            void* dst = element_ptr(first_index + i);
            void* src = element_ptr(first_index + count + i);
            mOps->move_construct(dst, src);
            mOps->destroy(src);
        }
    } else {
        // Trivial: memmove the remaining elements left
        if (remaining > 0) {
            trivial_move_left(element_ptr(first_index),
                              element_ptr(first_index + count),
                              remaining);
        }
    }
    mSize -= count;
}

// ======= INSERT =======

void vector_basic::insert_copy_impl(fl::size index, const void* element) {
    if (index > mSize) index = mSize;
    ensure_capacity(mSize + 1);
    if (mSize >= mCapacity) return;  // allocation failed

    if (mOps) {
        // Shift elements right, starting from the end
        if (mSize > index) {
            // Move-construct the last element into uninitialized space
            mOps->move_construct(element_ptr(mSize), element_ptr(mSize - 1));
            // Move-assign backwards for the rest
            for (fl::size i = mSize - 1; i > index; --i) {
                // Destroy dst, then move-construct from src
                mOps->destroy(element_ptr(i));
                mOps->move_construct(element_ptr(i), element_ptr(i - 1));
            }
            // Destroy the slot and copy-construct the new element
            mOps->destroy(element_ptr(index));
        }
        mOps->copy_construct(element_ptr(index), element);
    } else {
        // Trivial: memmove right, then memcpy
        if (mSize > index) {
            trivial_move_left(element_ptr(index + 1), element_ptr(index),
                              mSize - index);
        }
        fl::memcpy(element_ptr(index), element, mElementSize);
    }
    ++mSize;
}

void vector_basic::insert_move_impl(fl::size index, void* element) {
    if (index > mSize) index = mSize;
    ensure_capacity(mSize + 1);
    if (mSize >= mCapacity) return;  // allocation failed

    if (mOps) {
        // Shift elements right, starting from the end
        if (mSize > index) {
            mOps->move_construct(element_ptr(mSize), element_ptr(mSize - 1));
            for (fl::size i = mSize - 1; i > index; --i) {
                mOps->destroy(element_ptr(i));
                mOps->move_construct(element_ptr(i), element_ptr(i - 1));
            }
            mOps->destroy(element_ptr(index));
        }
        mOps->move_construct(element_ptr(index), element);
    } else {
        if (mSize > index) {
            trivial_move_left(element_ptr(index + 1), element_ptr(index),
                              mSize - index);
        }
        fl::memcpy(element_ptr(index), element, mElementSize);
    }
    ++mSize;
}

// ======= RESIZE =======

void vector_basic::resize_impl(fl::size n) {
    if (n == mSize) return;

    if (n < mSize) {
        // Shrink: destroy excess elements
        if (mOps) {
            for (fl::size i = n; i < mSize; ++i) {
                mOps->destroy(element_ptr(i));
            }
        }
        mSize = n;
        return;
    }

    // Grow: ensure capacity and default-construct new elements
    ensure_capacity(n);
    if (mCapacity < n) return;  // allocation failed

    if (mOps) {
        if (mOps->default_construct) {
            for (fl::size i = mSize; i < n; ++i) {
                mOps->default_construct(element_ptr(i));
            }
        } else {
            // Type has no default constructor — zero-fill as fallback
            trivial_default_construct(element_ptr(mSize), n - mSize);
        }
    } else {
        trivial_default_construct(element_ptr(mSize), n - mSize);
    }
    mSize = n;
}

void vector_basic::resize_value_impl(fl::size n, const void* value) {
    if (n == mSize) return;

    if (n < mSize) {
        if (mOps) {
            for (fl::size i = n; i < mSize; ++i) {
                mOps->destroy(element_ptr(i));
            }
        }
        mSize = n;
        return;
    }

    ensure_capacity(n);
    if (mCapacity < n) return;

    if (mOps) {
        for (fl::size i = mSize; i < n; ++i) {
            mOps->copy_construct(element_ptr(i), value);
        }
    } else {
        for (fl::size i = mSize; i < n; ++i) {
            fl::memcpy(element_ptr(i), value, mElementSize);
        }
    }
    mSize = n;
}

// ======= SWAP =======

void vector_basic::swap_impl(vector_basic& other) {
    // Swap all members. This works correctly even with inline buffers
    // because we swap the offset too, and the inline data stays in place.
    // HOWEVER: if either vector uses inline storage, we need to handle
    // it carefully since inline buffers can't be swapped by pointer.

    bool this_inline = isInline();
    bool other_inline = other.isInline();

    if (!this_inline && !other_inline) {
        // Both on heap: just swap pointers and metadata
        void* tmp_array = mArray;
        mArray = other.mArray;
        other.mArray = tmp_array;

        fl::size tmp_cap = mCapacity;
        mCapacity = other.mCapacity;
        other.mCapacity = tmp_cap;
    } else if (this_inline && other_inline) {
        // Both inline: swap element data in-place
        fl::size max_size = mSize > other.mSize ? mSize : other.mSize;
        for (fl::size i = 0; i < max_size; ++i) {
            if (i < mSize && i < other.mSize) {
                if (mOps) {
                    mOps->swap_elements(element_ptr(i), other.element_ptr(i));
                } else {
                    trivial_swap(element_ptr(i), other.element_ptr(i));
                }
            } else if (i < mSize) {
                // Move this[i] to other[i], destroy this[i]
                if (mOps) {
                    mOps->move_construct(other.element_ptr(i), element_ptr(i));
                    mOps->destroy(element_ptr(i));
                } else {
                    fl::memcpy(other.element_ptr(i), element_ptr(i), mElementSize);
                }
            } else {
                // Move other[i] to this[i], destroy other[i]
                if (mOps) {
                    mOps->move_construct(element_ptr(i), other.element_ptr(i));
                    mOps->destroy(other.element_ptr(i));
                } else {
                    fl::memcpy(element_ptr(i), other.element_ptr(i), mElementSize);
                }
            }
        }
    } else if (this_inline) {
        // this is inline, other is on heap
        // Move this's inline data to a temp, take other's heap pointer,
        // move temp data to other's inline buffer
        void* other_array = other.mArray;
        fl::size other_cap = other.mCapacity;

        // Move this's elements to other's inline buffer
        if (other.hasInlineBuffer() && mSize <= other.mInlineCapacity) {
            other.mArray = other.inlineBufferPtr();
            other.mCapacity = other.mInlineCapacity;
            if (mOps) {
                mOps->uninitialized_move_n(other.mArray, mArray, mSize);
                mOps->destroy_n(mArray, mSize);
            } else {
                if (mSize > 0) trivial_copy(other.mArray, mArray, mSize);
            }
        } else {
            // Other has no inline buffer or data doesn't fit - allocate heap
            other.mArray = other.mResource->allocate(mSize * mElementSize);
            other.mCapacity = mSize;
            if (mOps) {
                mOps->uninitialized_move_n(other.mArray, mArray, mSize);
                mOps->destroy_n(mArray, mSize);
            } else {
                if (mSize > 0) trivial_copy(other.mArray, mArray, mSize);
            }
        }

        // This takes other's heap pointer
        mArray = other_array;
        mCapacity = other_cap;
    } else {
        // other is inline, this is on heap - mirror of above
        other.swap_impl(*this);
        return;
    }

    // Swap sizes
    fl::size tmp_size = mSize;
    mSize = other.mSize;
    other.mSize = tmp_size;

    // Swap resources
    memory_resource* tmp_res = mResource;
    mResource = other.mResource;
    other.mResource = tmp_res;
}

// ======= COPY / MOVE HELPERS =======

void vector_basic::copy_from(const vector_basic& other) {
    clear_impl();
    if (other.mSize == 0) return;

    reserve_impl(other.mSize);
    if (mCapacity < other.mSize) return;

    if (mOps) {
        for (fl::size i = 0; i < other.mSize; ++i) {
            mOps->copy_construct(element_ptr(i), other.element_ptr(i));
        }
    } else {
        trivial_copy(mArray, other.mArray, other.mSize);
    }
    mSize = other.mSize;
}

void vector_basic::move_from(vector_basic& other) FL_NOEXCEPT {
    if (other.isInline()) {
        // Can't steal inline buffer — must move elements
        reserve_impl(other.mSize);
        if (mOps) {
            mOps->uninitialized_move_n(mArray, other.mArray, other.mSize);
        } else {
            if (other.mSize > 0) {
                trivial_copy(mArray, other.mArray, other.mSize);
            }
        }
        mSize = other.mSize;
        mResource = other.mResource;
        other.mSize = 0;
    } else {
        // Steal heap pointer
        mArray = other.mArray;
        mSize = other.mSize;
        mCapacity = other.mCapacity;
        mResource = other.mResource;

        // Leave other in valid empty state
        if (other.hasInlineBuffer()) {
            other.mArray = other.inlineBufferPtr();
            other.mCapacity = other.mInlineCapacity;
        } else {
            other.mArray = nullptr;
            other.mCapacity = 0;
        }
        other.mSize = 0;
    }
}

void vector_basic::move_assign(vector_basic& other) FL_NOEXCEPT {
    if (this == &other) return;
    clear_impl();
    if (!isInline() && mArray) {
        mResource->deallocate(mArray, mCapacity * mElementSize);
        mArray = nullptr;
        mCapacity = 0;
    }
    if (hasInlineBuffer()) {
        mArray = inlineBufferPtr();
        mCapacity = mInlineCapacity;
    }
    move_from(other);
}

// ======= DESTRUCTOR =======

vector_basic::~vector_basic() FL_NOEXCEPT {
    clear_impl();
    if (!isInline() && mArray) {
        mResource->deallocate(mArray, mCapacity * mElementSize);
    }
}

} // namespace fl
