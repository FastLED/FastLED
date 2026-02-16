// ok standalone
// Test for make_shared control block optimization
// Verifies that make_shared uses single allocation (inlined control block)

#include "test.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/atomic.h"
#include "fl/stl/vector.h"
#include "fl/stl/malloc.h"

using namespace fl;

// ============================================================================
// ALLOCATION TRACKING
// ============================================================================

namespace {

struct AllocationStats {
    fl::atomic<int> alloc_count{0};
    fl::atomic<int> free_count{0};
    fl::atomic<size_t> total_bytes{0};

    void reset() {
        alloc_count.store(0);
        free_count.store(0);
        total_bytes.store(0);
    }
};

AllocationStats g_stats;
bool g_tracking_enabled = false;

} // namespace

// Override global new/delete to track allocations
void* operator new(size_t size) {
    void* ptr = fl::malloc(size);
    if (!ptr) {
        // Exceptions may be disabled, just return null
        return nullptr;
    }

    if (g_tracking_enabled) {
        g_stats.alloc_count.fetch_add(1);
        g_stats.total_bytes.fetch_add(size);
    }

    return ptr;
}

void operator delete(void* ptr) noexcept {
    if (!ptr) return;

    if (g_tracking_enabled) {
        g_stats.free_count.fetch_add(1);
    }

    fl::free(ptr);
}

void operator delete(void* ptr, size_t) noexcept {
    ::operator delete(ptr);
}

// ============================================================================
// TEST TYPES
// ============================================================================

struct SimpleType {
    int value;
    SimpleType(int v = 0) : value(v) {}
};

struct LargeType {
    char data[1024];
    int value;
    LargeType(int v = 0) : value(v) {
        for (size_t i = 0; i < sizeof(data); i++) {
            data[i] = static_cast<char>(i);
        }
    }
};

struct TypeWithDestructor {
    static int construct_count;
    static int destruct_count;

    int value;

    TypeWithDestructor(int v = 0) : value(v) {
        construct_count++;
    }

    ~TypeWithDestructor() {
        destruct_count++;
    }
};

int TypeWithDestructor::construct_count = 0;
int TypeWithDestructor::destruct_count = 0;

// ============================================================================
// TESTS
// ============================================================================

FL_TEST_CASE("make_shared uses single allocation") {
    g_stats.reset();
    g_tracking_enabled = true;

    {
        auto ptr = fl::make_shared<SimpleType>(42);
        FL_CHECK(ptr);
        FL_CHECK(ptr->value == 42);
    }

    g_tracking_enabled = false;

    int allocs = g_stats.alloc_count.load();
    int frees = g_stats.free_count.load();

    // Should be exactly 1 allocation (control block + object inlined)
    FL_CHECK(allocs == 1);
    FL_CHECK(frees == 1);
    FL_CHECK(allocs == frees);  // No leaks
}

FL_TEST_CASE("make_shared with large object uses single allocation") {
    g_stats.reset();
    g_tracking_enabled = true;

    {
        auto ptr = fl::make_shared<LargeType>(123);
        FL_CHECK(ptr);
        FL_CHECK(ptr->value == 123);
        FL_CHECK(ptr->data[0] == 0);
        FL_CHECK(ptr->data[100] == 100);
    }

    g_tracking_enabled = false;

    int allocs = g_stats.alloc_count.load();
    int frees = g_stats.free_count.load();

    // Should be exactly 1 allocation even for large objects
    FL_CHECK(allocs == 1);
    FL_CHECK(frees == 1);
    FL_CHECK(allocs == frees);
}

FL_TEST_CASE("make_shared properly destructs objects") {
    TypeWithDestructor::construct_count = 0;
    TypeWithDestructor::destruct_count = 0;

    {
        auto ptr1 = fl::make_shared<TypeWithDestructor>(1);
        auto ptr2 = fl::make_shared<TypeWithDestructor>(2);
        auto ptr3 = ptr1;  // Copy

        FL_CHECK(TypeWithDestructor::construct_count == 2);
        FL_CHECK(TypeWithDestructor::destruct_count == 0);

        FL_CHECK(ptr1->value == 1);
        FL_CHECK(ptr2->value == 2);
        FL_CHECK(ptr3->value == 1);
    }

    // All destructors should be called
    FL_CHECK(TypeWithDestructor::construct_count == 2);
    FL_CHECK(TypeWithDestructor::destruct_count == 2);
}

FL_TEST_CASE("make_shared_with_deleter uses two allocations") {
    // Custom deleter prevents inlined optimization
    g_stats.reset();
    g_tracking_enabled = true;

    {
        auto deleter = [](SimpleType* p) { delete p; };
        auto ptr = fl::make_shared_with_deleter<SimpleType>(deleter, 42);
        FL_CHECK(ptr);
        FL_CHECK(ptr->value == 42);
    }

    g_tracking_enabled = false;

    int allocs = g_stats.alloc_count.load();
    int frees = g_stats.free_count.load();

    // Should be 2 allocations (object + control block with deleter)
    FL_CHECK(allocs == 2);
    FL_CHECK(frees == 2);
    FL_CHECK(allocs == frees);
}

FL_TEST_CASE("make_shared with multiple arguments") {
    struct MultiArg {
        int a, b, c;
        MultiArg(int x, int y, int z) : a(x), b(y), c(z) {}
    };

    g_stats.reset();
    g_tracking_enabled = true;

    {
        auto ptr = fl::make_shared<MultiArg>(1, 2, 3);
        FL_CHECK(ptr->a == 1);
        FL_CHECK(ptr->b == 2);
        FL_CHECK(ptr->c == 3);
    }

    g_tracking_enabled = false;

    int allocs = g_stats.alloc_count.load();
    FL_CHECK(allocs == 1);  // Single allocation
}

FL_TEST_CASE("make_shared reference counting works correctly") {
    g_stats.reset();
    g_tracking_enabled = true;

    {
        auto ptr1 = fl::make_shared<SimpleType>(100);
        FL_CHECK(g_stats.alloc_count.load() == 1);

        {
            auto ptr2 = ptr1;  // Copy - no new allocation
            FL_CHECK(g_stats.alloc_count.load() == 1);
            FL_CHECK(ptr2->value == 100);

            auto ptr3 = ptr1;  // Another copy
            FL_CHECK(g_stats.alloc_count.load() == 1);
        }

        // ptr2 and ptr3 destroyed, but object still alive
        FL_CHECK(g_stats.free_count.load() == 0);
        FL_CHECK(ptr1->value == 100);
    }

    g_tracking_enabled = false;

    // Now object should be destroyed
    FL_CHECK(g_stats.free_count.load() == 1);
}

FL_TEST_CASE("make_shared exception safety") {
#ifdef __cpp_exceptions
    struct construction_error {};

    struct ThrowingType {
        ThrowingType() { throw construction_error(); }
    };

    g_stats.reset();
    g_tracking_enabled = true;

    bool threw = false;
    try {
        auto ptr = fl::make_shared<ThrowingType>();
    } catch (const construction_error&) {
        threw = true;
    }

    g_tracking_enabled = false;

    FL_CHECK(threw);

    // Control block should be cleaned up despite exception
    int allocs = g_stats.alloc_count.load();
    int frees = g_stats.free_count.load();
    FL_CHECK(allocs == 1);  // Control block allocated
    FL_CHECK(frees == 1);   // Control block freed (no leak)
#else
    // Exceptions disabled - skip this test
    FL_CHECK(true);
#endif
}

FL_TEST_CASE("make_shared alignment requirements") {
    // Test 16-byte alignment (supported by standard new on all platforms)
    // Note: Over-aligned types (>16 bytes) require C++17 aligned new or custom allocator
    struct AlignedType {
        FL_ALIGNAS(16) char data[128];
        int value;

        AlignedType(int v) : value(v) {
            for (size_t i = 0; i < sizeof(data); i++) {
                data[i] = static_cast<char>(v);
            }
        }
    };

    auto ptr = fl::make_shared<AlignedType>(42);
    FL_CHECK(ptr);
    FL_CHECK(ptr->value == 42);

    // Verify alignment (address should be 16-byte aligned)
    uintptr_t addr = fl::bit_cast<uintptr_t>(ptr.get());
    FL_CHECK((addr % 16) == 0);
}

FL_TEST_CASE("make_shared memory savings benchmark") {
    // Verify make_shared uses single allocation per object
    constexpr int NUM_OBJECTS = 100;

    g_stats.reset();
    g_tracking_enabled = true;

    {
        fl::vector<fl::shared_ptr<SimpleType>> ptrs;
        ptrs.reserve(NUM_OBJECTS);  // Pre-allocate to avoid vector growth noise

        for (int i = 0; i < NUM_OBJECTS; i++) {
            ptrs.push_back(fl::make_shared<SimpleType>(i));
        }

        // Verify all objects were created correctly
        for (int i = 0; i < NUM_OBJECTS; i++) {
            FL_CHECK(ptrs[i]->value == i);
        }
    }

    g_tracking_enabled = false;

    int total_allocs = g_stats.alloc_count.load();
    int total_frees = g_stats.free_count.load();

    // Should be NUM_OBJECTS allocations (1 per object, inlined control block)
    // Plus 1 for vector backing array
    FL_CHECK(total_allocs <= NUM_OBJECTS + 5);  // +5 for vector overhead
    FL_CHECK(total_allocs == total_frees);  // No leaks
}
