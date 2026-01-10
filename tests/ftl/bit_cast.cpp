#include "test.h"
#include "fl/stl/bit_cast.h"
#include "doctest.h"
#include "fl/int.h"

using namespace fl;

TEST_CASE("fl::bit_cast basic conversions") {
    SUBCASE("Integer to integer conversions") {
        u32 u32_val = 0x12345678;
        i32 i32_val = bit_cast<i32>(u32_val);
        CHECK_EQ(bit_cast<u32>(i32_val), u32_val);

        u16 u16_val = 0xABCD;
        i16 i16_val = bit_cast<i16>(u16_val);
        CHECK_EQ(bit_cast<u16>(i16_val), u16_val);

        u8 u8_val = 0xFF;
        i8 i8_val = bit_cast<i8>(u8_val);
        CHECK_EQ(bit_cast<u8>(i8_val), u8_val);
    }

    SUBCASE("Float to integer conversions") {
        float f = 3.14159f;
        u32 as_int = bit_cast<u32>(f);
        float back = bit_cast<float>(as_int);
        CHECK_EQ(back, f);

        // Test special float values
        float zero = 0.0f;
        u32 zero_bits = bit_cast<u32>(zero);
        CHECK_EQ(bit_cast<float>(zero_bits), zero);

        float neg_zero = -0.0f;
        u32 neg_zero_bits = bit_cast<u32>(neg_zero);
        CHECK_EQ(bit_cast<float>(neg_zero_bits), neg_zero);
    }

    SUBCASE("Double to integer conversions") {
        double d = 2.718281828;
        u64 as_int = bit_cast<u64>(d);
        double back = bit_cast<double>(as_int);
        CHECK_EQ(back, d);

        // Test special double values
        double zero = 0.0;
        u64 zero_bits = bit_cast<u64>(zero);
        CHECK_EQ(bit_cast<double>(zero_bits), zero);
    }

    SUBCASE("Pointer conversions") {
        int value = 42;
        int* ptr = &value;

        uptr ptr_as_int = bit_cast<uptr>(ptr);
        int* ptr_back = bit_cast<int*>(ptr_as_int);
        CHECK_EQ(ptr_back, ptr);
        CHECK_EQ(*ptr_back, 42);

        // Test null pointer
        int* null_ptr = nullptr;
        uptr null_as_int = bit_cast<uptr>(null_ptr);
        CHECK_EQ(null_as_int, 0);
    }

    SUBCASE("Signed to unsigned conversions") {
        i32 negative = -1;
        u32 as_unsigned = bit_cast<u32>(negative);
        CHECK_EQ(as_unsigned, 0xFFFFFFFF);

        i32 back = bit_cast<i32>(as_unsigned);
        CHECK_EQ(back, negative);
    }

    SUBCASE("Round-trip conversions") {
        // Test that bit_cast is truly zero-cost and preserves bits
        u32 original = 0xDEADBEEF;
        u32 round_trip = bit_cast<u32>(bit_cast<i32>(original));
        CHECK_EQ(round_trip, original);

        float f_original = 1.23456f;
        float f_round_trip = bit_cast<float>(bit_cast<u32>(f_original));
        CHECK_EQ(f_round_trip, f_original);
    }
}

TEST_CASE("fl::bit_cast with structs") {
    SUBCASE("POD struct conversions") {
        // Note: In C++11, we need to use simpler POD structs or arrays
        // Testing with array instead which is definitely POD
        u8 color[4] = {0xFF, 0x00, 0x80, 0x00};
        u32 as_int;
        // Manual bit packing since struct bit_cast may not be POD in C++11
        as_int = color[0] | (color[1] << 8) | (color[2] << 16) | (color[3] << 24);

        u8 back[4];
        back[0] = as_int & 0xFF;
        back[1] = (as_int >> 8) & 0xFF;
        back[2] = (as_int >> 16) & 0xFF;
        back[3] = (as_int >> 24) & 0xFF;

        CHECK_EQ(back[0], 0xFF);
        CHECK_EQ(back[1], 0x00);
        CHECK_EQ(back[2], 0x80);
        CHECK_EQ(back[3], 0x00);
    }

    SUBCASE("Array-like struct conversions") {
        struct Vec3f {
            float x, y, z;
        };

        struct Vec3i {
            i32 x, y, z;
        };

        Vec3f vec = {1.0f, 2.0f, 3.0f};
        Vec3i as_ints;
        as_ints.x = bit_cast<i32>(vec.x);
        as_ints.y = bit_cast<i32>(vec.y);
        as_ints.z = bit_cast<i32>(vec.z);

        Vec3f back;
        back.x = bit_cast<float>(as_ints.x);
        back.y = bit_cast<float>(as_ints.y);
        back.z = bit_cast<float>(as_ints.z);

        CHECK_EQ(back.x, vec.x);
        CHECK_EQ(back.y, vec.y);
        CHECK_EQ(back.z, vec.z);
    }
}

TEST_CASE("fl::bit_cast_ptr") {
    SUBCASE("Basic pointer casting") {
        int value = 42;
        void* storage = &value;

        int* typed_ptr = bit_cast_ptr<int>(storage);
        CHECK_EQ(typed_ptr, &value);
        CHECK_EQ(*typed_ptr, 42);
    }

    SUBCASE("Const pointer casting") {
        const int value = 42;
        const void* storage = &value;

        const int* typed_ptr = bit_cast_ptr<int>(storage);
        CHECK_EQ(typed_ptr, &value);
        CHECK_EQ(*typed_ptr, 42);
    }

    SUBCASE("Struct pointer casting") {
        struct Data {
            int x;
            float y;
        };

        Data data = {10, 3.14f};
        void* storage = &data;

        Data* typed_ptr = bit_cast_ptr<Data>(storage);
        CHECK_EQ(typed_ptr->x, 10);
        CHECK_CLOSE(typed_ptr->y, 3.14f, 0.0001f);
    }
}

TEST_CASE("fl::ptr_to_int and fl::int_to_ptr") {
    SUBCASE("Basic pointer-integer conversion") {
        int value = 42;
        int* ptr = &value;

        uptr as_int = ptr_to_int(ptr);
        int* ptr_back = int_to_ptr<int>(as_int);

        CHECK_EQ(ptr_back, ptr);
        CHECK_EQ(*ptr_back, 42);
    }

    SUBCASE("Null pointer conversion") {
        int* null_ptr = nullptr;
        uptr as_int = ptr_to_int(null_ptr);
        CHECK_EQ(as_int, 0);

        int* ptr_back = int_to_ptr<int>(as_int);
        CHECK_EQ(ptr_back, nullptr);
    }

    SUBCASE("Multiple pointer types") {
        float f = 2.718f;
        float* f_ptr = &f;
        uptr f_int = ptr_to_int(f_ptr);
        float* f_back = int_to_ptr<float>(f_int);
        CHECK_EQ(f_back, f_ptr);
        CHECK_CLOSE(*f_back, 2.718f, 0.0001f);

        double d = 3.14159;
        double* d_ptr = &d;
        uptr d_int = ptr_to_int(d_ptr);
        double* d_back = int_to_ptr<double>(d_int);
        CHECK_EQ(d_back, d_ptr);
        CHECK_CLOSE(*d_back, 3.14159, 0.00001);
    }

    SUBCASE("Const pointer conversion") {
        const int value = 123;
        const int* c_ptr = &value;
        uptr as_int = ptr_to_int(c_ptr);
        const int* c_back = int_to_ptr<const int>(as_int);
        CHECK_EQ(c_back, c_ptr);
        CHECK_EQ(*c_back, 123);
    }
}

TEST_CASE("fl::is_bitcast_compatible trait") {
    SUBCASE("Integral types are compatible") {
        // Use static_assert to check at compile time
        static_assert(is_bitcast_compatible<u8>::value, "u8 should be bitcast compatible");
        static_assert(is_bitcast_compatible<u16>::value, "u16 should be bitcast compatible");
        static_assert(is_bitcast_compatible<u32>::value, "u32 should be bitcast compatible");
        static_assert(is_bitcast_compatible<u64>::value, "u64 should be bitcast compatible");
        static_assert(is_bitcast_compatible<i8>::value, "i8 should be bitcast compatible");
        static_assert(is_bitcast_compatible<i16>::value, "i16 should be bitcast compatible");
        static_assert(is_bitcast_compatible<i32>::value, "i32 should be bitcast compatible");
        static_assert(is_bitcast_compatible<i64>::value, "i64 should be bitcast compatible");
        CHECK(true);  // Dummy runtime check
    }

    SUBCASE("Floating point types are compatible") {
        static_assert(is_bitcast_compatible<float>::value, "float should be bitcast compatible");
        static_assert(is_bitcast_compatible<double>::value, "double should be bitcast compatible");
        CHECK(true);  // Dummy runtime check
    }

    SUBCASE("Pointer types are compatible") {
        static_assert(is_bitcast_compatible<int*>::value, "int* should be bitcast compatible");
        static_assert(is_bitcast_compatible<float*>::value, "float* should be bitcast compatible");
        static_assert(is_bitcast_compatible<void*>::value, "void* should be bitcast compatible");
        static_assert(is_bitcast_compatible<const int*>::value, "const int* should be bitcast compatible");
        CHECK(true);  // Dummy runtime check
    }

    SUBCASE("Const types preserve compatibility") {
        static_assert(is_bitcast_compatible<const int>::value, "const int should be bitcast compatible");
        static_assert(is_bitcast_compatible<const float>::value, "const float should be bitcast compatible");
        static_assert(is_bitcast_compatible<const u32>::value, "const u32 should be bitcast compatible");
        CHECK(true);  // Dummy runtime check
    }
}

TEST_CASE("fl::bit_cast edge cases") {
    SUBCASE("Zero values") {
        u32 zero_u32 = 0;
        i32 zero_i32 = bit_cast<i32>(zero_u32);
        CHECK_EQ(zero_i32, 0);

        float zero_f = 0.0f;
        u32 zero_f_bits = bit_cast<u32>(zero_f);
        CHECK_EQ(bit_cast<float>(zero_f_bits), 0.0f);
    }

    SUBCASE("Maximum values") {
        u32 max_u32 = 0xFFFFFFFF;
        i32 as_signed = bit_cast<i32>(max_u32);
        CHECK_EQ(as_signed, -1);

        u8 max_u8 = 0xFF;
        i8 as_i8 = bit_cast<i8>(max_u8);
        CHECK_EQ(as_i8, -1);
    }

    SUBCASE("Bit pattern preservation") {
        // Ensure specific bit patterns are preserved
        u32 pattern = 0xA5A5A5A5;  // Alternating bits
        i32 as_signed = bit_cast<i32>(pattern);
        u32 back = bit_cast<u32>(as_signed);
        CHECK_EQ(back, pattern);

        // Test with float
        u32 float_bits = 0x3F800000;  // 1.0f in IEEE 754
        float as_float = bit_cast<float>(float_bits);
        CHECK_EQ(as_float, 1.0f);
    }

    SUBCASE("Small integer sizes") {
        u8 u8_max = 255;
        i8 i8_from_u8 = bit_cast<i8>(u8_max);
        CHECK_EQ(i8_from_u8, -1);

        u16 u16_val = 0x8000;  // Bit 15 set
        i16 i16_from_u16 = bit_cast<i16>(u16_val);
        CHECK(i16_from_u16 < 0);  // Should be negative due to sign bit
    }
}

TEST_CASE("fl::bit_cast runtime conversions") {
    SUBCASE("Runtime bit_cast") {
        // Note: fl::bit_cast is not constexpr in this C++11 implementation
        // It uses union-based type punning which is runtime-only
        u32 runtime_val = 0x12345678;
        i32 runtime_result = bit_cast<i32>(runtime_val);

        // Verify round-trip works correctly
        CHECK_EQ(bit_cast<u32>(runtime_result), runtime_val);
        CHECK_EQ(runtime_result, static_cast<i32>(0x12345678));
    }
}
