#include "fl/align.h"
#include "fl/stl/cstddef.h"
#include "doctest.h"
#include "fl/int.h"

using namespace fl;

// Test FL_ALIGN_BYTES macro value
TEST_CASE("FL_ALIGN_BYTES") {
    SUBCASE("value is compile-time constant") {
        // FL_ALIGN_BYTES should be a compile-time constant
        constexpr int bytes = FL_ALIGN_BYTES;

#ifdef __EMSCRIPTEN__
        CHECK_EQ(bytes, 8);
#else
        CHECK_EQ(bytes, 1);
#endif
    }

    SUBCASE("can be used in array declarations") {
        // Test that FL_ALIGN_BYTES can be used as array size
        char buffer[FL_ALIGN_BYTES];
        (void)buffer; // Suppress unused variable warning
        CHECK(true); // If we get here, array declaration worked
    }
}

// Test FL_ALIGN macro
TEST_CASE("FL_ALIGN") {
    SUBCASE("struct alignment") {
        struct FL_ALIGN AlignedStruct {
            char c;
            int i;
        };

        AlignedStruct as;
        (void)as; // Suppress unused variable warning

#ifdef __EMSCRIPTEN__
        // On Emscripten, should be 8-byte aligned
        CHECK_EQ(alignof(AlignedStruct), 8);
#else
        // On other platforms, FL_ALIGN is a no-op
        // Alignment is natural alignment of the struct
        CHECK(alignof(AlignedStruct) >= 1);
#endif
    }

    SUBCASE("class alignment") {
        class FL_ALIGN AlignedClass {
        public:
            double d;
            int i;
        };

        AlignedClass ac;
        (void)ac; // Suppress unused variable warning

#ifdef __EMSCRIPTEN__
        // On Emscripten, should be 8-byte aligned
        CHECK_EQ(alignof(AlignedClass), 8);
#else
        // On other platforms, alignment is natural
        CHECK(alignof(AlignedClass) >= 1);
#endif
    }

    SUBCASE("multiple aligned members") {
        struct Container {
            FL_ALIGN int a;
            FL_ALIGN int b;
            FL_ALIGN int c;
        };

        Container cnt;
        (void)cnt; // Suppress unused variable warning

        // Container should compile successfully
        CHECK(sizeof(Container) >= 3 * sizeof(int));
    }
}

// Test FL_ALIGN_AS macro
TEST_CASE("FL_ALIGN_AS") {
    SUBCASE("align to int") {
        struct FL_ALIGN_AS(int) AlignedToInt {
            char c;
        };

        AlignedToInt ati;
        (void)ati; // Suppress unused variable warning

#ifdef __AVR__
        // On AVR (8-bit), FL_ALIGN_AS is a no-op to save RAM
        CHECK(alignof(AlignedToInt) >= 1);
#else
        // Should be aligned to int (4 bytes on most platforms)
        CHECK_EQ(alignof(AlignedToInt), alignof(int));
#endif
    }

    SUBCASE("align to double") {
        struct FL_ALIGN_AS(double) AlignedToDouble {
            char c;
            int i;
        };

        AlignedToDouble atd;
        (void)atd; // Suppress unused variable warning

#ifdef __AVR__
        // On AVR (8-bit), FL_ALIGN_AS is a no-op to save RAM
        CHECK(alignof(AlignedToDouble) >= 1);
#else
        // Should be aligned to double (8 bytes on most platforms)
        CHECK_EQ(alignof(AlignedToDouble), alignof(double));
#endif
    }

    SUBCASE("align to custom struct") {
        struct alignas(16) CustomAligned {
            int x, y, z, w;
        };

        struct FL_ALIGN_AS(CustomAligned) AlignedToCustom {
            char c;
        };

        AlignedToCustom atc;
        (void)atc; // Suppress unused variable warning

#ifdef __AVR__
        // On AVR (8-bit), FL_ALIGN_AS is a no-op to save RAM
        CHECK(alignof(AlignedToCustom) >= 1);
#else
        // Should be aligned to CustomAligned (16 bytes)
        CHECK_EQ(alignof(AlignedToCustom), alignof(CustomAligned));
        CHECK_EQ(alignof(AlignedToCustom), 16);
#endif
    }

    SUBCASE("align to pointer") {
        struct FL_ALIGN_AS(void*) AlignedToPointer {
            char data[7];
        };

        AlignedToPointer atp;
        (void)atp; // Suppress unused variable warning

#ifdef __AVR__
        // On AVR (8-bit), FL_ALIGN_AS is a no-op to save RAM
        CHECK(alignof(AlignedToPointer) >= 1);
#else
        // Should be aligned to pointer size
        CHECK_EQ(alignof(AlignedToPointer), alignof(void*));
#endif
    }
}

// Test alignment affects memory layout
TEST_CASE("FL_ALIGN memory layout") {
    SUBCASE("aligned struct affects size") {
        struct FL_ALIGN LargeAligned {
            char c;
        };

#ifdef __EMSCRIPTEN__
        // With 8-byte alignment, struct should be padded to 8 bytes
        CHECK_EQ(sizeof(LargeAligned), 8);
#else
        // Without alignment, struct can be 1 byte
        CHECK_EQ(sizeof(LargeAligned), 1);
#endif
    }

    SUBCASE("alignment affects array stride") {
        struct FL_ALIGN Element {
            char value;
        };

        Element array[3];
        (void)array; // Suppress unused variable warning

#ifdef __EMSCRIPTEN__
        // Each element should be 8 bytes apart
        CHECK_EQ(sizeof(Element), 8);
        CHECK_EQ(sizeof(array), 24);
#else
        // Each element is just 1 byte
        CHECK_EQ(sizeof(Element), 1);
        CHECK_EQ(sizeof(array), 3);
#endif
    }
}

// Test compile-time properties
TEST_CASE("FL_ALIGN compile-time checks") {
    SUBCASE("alignof is constexpr") {
        struct FL_ALIGN TestStruct {
            int x;
        };

        // alignof should work at compile time
        constexpr size_t alignment = alignof(TestStruct);
        (void)alignment; // Suppress unused variable warning

        CHECK(alignment >= 1);
    }

    SUBCASE("FL_ALIGN_AS is constexpr") {
        struct FL_ALIGN_AS(double) TestStruct {
            char c;
        };

        // alignof should work at compile time
        constexpr size_t alignment = alignof(TestStruct);
        (void)alignment; // Suppress unused variable warning

        CHECK(alignment >= 1);
    }
}

// Test edge cases
TEST_CASE("FL_ALIGN edge cases") {
    SUBCASE("empty struct with alignment") {
        struct FL_ALIGN EmptyAligned {
        };

        EmptyAligned ea;
        (void)ea; // Suppress unused variable warning

        // Even empty structs have size of at least 1
        CHECK(sizeof(EmptyAligned) >= 1);
    }

    SUBCASE("nested aligned structs") {
        struct FL_ALIGN Inner {
            char c;
        };

        struct FL_ALIGN Outer {
            Inner inner;
            int i;
        };

        Outer outer;
        (void)outer; // Suppress unused variable warning

        CHECK(sizeof(Outer) >= sizeof(Inner) + sizeof(int));
    }

    SUBCASE("alignment with union") {
        union FL_ALIGN AlignedUnion {
            char c;
            int i;
            double d;
        };

        AlignedUnion au;
        (void)au; // Suppress unused variable warning

#ifdef __EMSCRIPTEN__
        // Should be aligned to 8 bytes
        CHECK_EQ(alignof(AlignedUnion), 8);
#else
        // Natural alignment of union (max of members)
        CHECK(alignof(AlignedUnion) >= 1);
#endif
    }
}

// Test macro interaction
TEST_CASE("FL_ALIGN macro combinations") {
    SUBCASE("both FL_ALIGN and FL_ALIGN_AS") {
        struct FL_ALIGN FL_ALIGN_AS(double) BothAligned {
            char c;
        };

        BothAligned ba;
        (void)ba; // Suppress unused variable warning

#ifdef __AVR__
        // On AVR (8-bit), both macros are no-ops to save RAM
        CHECK(alignof(BothAligned) >= 1);
#else
        // Both macros should apply - FL_ALIGN_AS(double) takes precedence
        CHECK_EQ(alignof(BothAligned), alignof(double));
#endif
    }

    SUBCASE("multiple FL_ALIGN_AS in same struct") {
        struct Container {
            FL_ALIGN_AS(int) char a;
            FL_ALIGN_AS(double) char b;
        };

        Container cnt;
        (void)cnt; // Suppress unused variable warning

        // Should compile successfully
        CHECK(sizeof(Container) >= 2);
    }
}
