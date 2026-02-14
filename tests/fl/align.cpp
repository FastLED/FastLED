#include "fl/align.h"
#include "fl/stl/cstddef.h"
#include "test.h"
#include "fl/int.h"

using namespace fl;

// Test FL_ALIGN_BYTES macro value
FL_TEST_CASE("FL_ALIGN_BYTES") {
    FL_SUBCASE("value is compile-time constant") {
        // FL_ALIGN_BYTES should be a compile-time constant
        constexpr int bytes = FL_ALIGN_BYTES;

#ifdef __EMSCRIPTEN__
        FL_CHECK_EQ(bytes, 8);
#else
        FL_CHECK_EQ(bytes, 1);
#endif
    }

    FL_SUBCASE("can be used in array declarations") {
        // Test that FL_ALIGN_BYTES can be used as array size
        char buffer[FL_ALIGN_BYTES];
        (void)buffer; // Suppress unused variable warning
        FL_CHECK(true); // If we get here, array declaration worked
    }
}

// Test FL_ALIGN macro
FL_TEST_CASE("FL_ALIGN") {
    FL_SUBCASE("struct alignment") {
        struct FL_ALIGN AlignedStruct {
            char c;
            int i;
        };

        AlignedStruct as;
        (void)as; // Suppress unused variable warning

#ifdef __EMSCRIPTEN__
        // On Emscripten, should be 8-byte aligned
        FL_CHECK_EQ(alignof(AlignedStruct), 8);
#else
        // On other platforms, FL_ALIGN is a no-op
        // Alignment is natural alignment of the struct
        FL_CHECK(alignof(AlignedStruct) >= 1);
#endif
    }

    FL_SUBCASE("class alignment") {
        class FL_ALIGN AlignedClass {
        public:
            double d;
            int i;
        };

        AlignedClass ac;
        (void)ac; // Suppress unused variable warning

#ifdef __EMSCRIPTEN__
        // On Emscripten, should be 8-byte aligned
        FL_CHECK_EQ(alignof(AlignedClass), 8);
#else
        // On other platforms, alignment is natural
        FL_CHECK(alignof(AlignedClass) >= 1);
#endif
    }

    FL_SUBCASE("multiple aligned members") {
        struct Container {
            FL_ALIGN int a;
            FL_ALIGN int b;
            FL_ALIGN int c;
        };

        Container cnt;
        (void)cnt; // Suppress unused variable warning

        // Container should compile successfully
        FL_CHECK(sizeof(Container) >= 3 * sizeof(int));
    }
}

// Test FL_ALIGN_AS macro
FL_TEST_CASE("FL_ALIGN_AS") {
    FL_SUBCASE("align to int") {
        struct FL_ALIGN_AS(int) AlignedToInt {
            char c;
        };

        AlignedToInt ati;
        (void)ati; // Suppress unused variable warning

#ifdef __AVR__
        // On AVR (8-bit), FL_ALIGN_AS is a no-op to save RAM
        FL_CHECK(alignof(AlignedToInt) >= 1);
#else
        // Should be aligned to int (4 bytes on most platforms)
        FL_CHECK_EQ(alignof(AlignedToInt), alignof(int));
#endif
    }

    FL_SUBCASE("align to double") {
        struct FL_ALIGN_AS(double) AlignedToDouble {
            char c;
            int i;
        };

        AlignedToDouble atd;
        (void)atd; // Suppress unused variable warning

#ifdef __AVR__
        // On AVR (8-bit), FL_ALIGN_AS is a no-op to save RAM
        FL_CHECK(alignof(AlignedToDouble) >= 1);
#else
        // Should be aligned to double (8 bytes on most platforms)
        FL_CHECK_EQ(alignof(AlignedToDouble), alignof(double));
#endif
    }

    FL_SUBCASE("align to custom struct") {
        struct FL_ALIGNAS(16) CustomAligned {
            int x, y, z, w;
        };

        struct FL_ALIGN_AS(CustomAligned) AlignedToCustom {
            char c;
        };

        AlignedToCustom atc;
        (void)atc; // Suppress unused variable warning

#ifdef __AVR__
        // On AVR (8-bit), FL_ALIGN_AS is a no-op to save RAM
        FL_CHECK(alignof(AlignedToCustom) >= 1);
#else
        // Should be aligned to CustomAligned (16 bytes)
        FL_CHECK_EQ(alignof(AlignedToCustom), alignof(CustomAligned));
        FL_CHECK_EQ(alignof(AlignedToCustom), 16);
#endif
    }

    FL_SUBCASE("align to pointer") {
        struct FL_ALIGN_AS(void*) AlignedToPointer {
            char data[7];
        };

        AlignedToPointer atp;
        (void)atp; // Suppress unused variable warning

#ifdef __AVR__
        // On AVR (8-bit), FL_ALIGN_AS is a no-op to save RAM
        FL_CHECK(alignof(AlignedToPointer) >= 1);
#else
        // Should be aligned to pointer size
        FL_CHECK_EQ(alignof(AlignedToPointer), alignof(void*));
#endif
    }
}

// Test alignment affects memory layout
FL_TEST_CASE("FL_ALIGN memory layout") {
    FL_SUBCASE("aligned struct affects size") {
        struct FL_ALIGN LargeAligned {
            char c;
        };

#ifdef __EMSCRIPTEN__
        // With 8-byte alignment, struct should be padded to 8 bytes
        FL_CHECK_EQ(sizeof(LargeAligned), 8);
#else
        // Without alignment, struct can be 1 byte
        FL_CHECK_EQ(sizeof(LargeAligned), 1);
#endif
    }

    FL_SUBCASE("alignment affects array stride") {
        struct FL_ALIGN Element {
            char value;
        };

        Element array[3];
        (void)array; // Suppress unused variable warning

#ifdef __EMSCRIPTEN__
        // Each element should be 8 bytes apart
        FL_CHECK_EQ(sizeof(Element), 8);
        FL_CHECK_EQ(sizeof(array), 24);
#else
        // Each element is just 1 byte
        FL_CHECK_EQ(sizeof(Element), 1);
        FL_CHECK_EQ(sizeof(array), 3);
#endif
    }
}

// Test compile-time properties
FL_TEST_CASE("FL_ALIGN compile-time checks") {
    FL_SUBCASE("alignof is constexpr") {
        struct FL_ALIGN TestStruct {
            int x;
        };

        // alignof should work at compile time
        constexpr size_t alignment = alignof(TestStruct);
        (void)alignment; // Suppress unused variable warning

        FL_CHECK(alignment >= 1);
    }

    FL_SUBCASE("FL_ALIGN_AS is constexpr") {
        struct FL_ALIGN_AS(double) TestStruct {
            char c;
        };

        // alignof should work at compile time
        constexpr size_t alignment = alignof(TestStruct);
        (void)alignment; // Suppress unused variable warning

        FL_CHECK(alignment >= 1);
    }
}

// Test edge cases
FL_TEST_CASE("FL_ALIGN edge cases") {
    FL_SUBCASE("empty struct with alignment") {
        struct FL_ALIGN EmptyAligned {
        };

        EmptyAligned ea;
        (void)ea; // Suppress unused variable warning

        // Even empty structs have size of at least 1
        FL_CHECK(sizeof(EmptyAligned) >= 1);
    }

    FL_SUBCASE("nested aligned structs") {
        struct FL_ALIGN Inner {
            char c;
        };

        struct FL_ALIGN Outer {
            Inner inner;
            int i;
        };

        Outer outer;
        (void)outer; // Suppress unused variable warning

        FL_CHECK(sizeof(Outer) >= sizeof(Inner) + sizeof(int));
    }

    FL_SUBCASE("alignment with union") {
        union FL_ALIGN AlignedUnion {
            char c;
            int i;
            double d;
        };

        AlignedUnion au;
        (void)au; // Suppress unused variable warning

#ifdef __EMSCRIPTEN__
        // Should be aligned to 8 bytes
        FL_CHECK_EQ(alignof(AlignedUnion), 8);
#else
        // Natural alignment of union (max of members)
        FL_CHECK(alignof(AlignedUnion) >= 1);
#endif
    }
}

// Test macro interaction
FL_TEST_CASE("FL_ALIGN macro combinations") {
    FL_SUBCASE("both FL_ALIGN and FL_ALIGN_AS") {
        struct FL_ALIGN FL_ALIGN_AS(double) BothAligned {
            char c;
        };

        BothAligned ba;
        (void)ba; // Suppress unused variable warning

#ifdef __AVR__
        // On AVR (8-bit), both macros are no-ops to save RAM
        FL_CHECK(alignof(BothAligned) >= 1);
#else
        // Both macros should apply - FL_ALIGN_AS(double) takes precedence
        FL_CHECK_EQ(alignof(BothAligned), alignof(double));
#endif
    }

    FL_SUBCASE("multiple FL_ALIGN_AS in same struct") {
        struct Container {
            FL_ALIGN_AS(int) char a;
            FL_ALIGN_AS(double) char b;
        };

        Container cnt;
        (void)cnt; // Suppress unused variable warning

        // Should compile successfully
        FL_CHECK(sizeof(Container) >= 2);
    }
}
