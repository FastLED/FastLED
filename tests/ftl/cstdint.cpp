#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"
#include "test.h"
#include "stdint.h" // ok include - testing FL types against standard

// Test C++ standard integer types and macros defined in fl/stl/stdint.h
// This header provides C++ integer type definitions without <stdint.h>
// Note: Uses fl:: namespace types internally but exposes standard C++ type names
// ok INT_MAX (this file tests that the numeric limit macros are properly defined)

FL_TEST_CASE("uint8_t type") {
    FL_SUBCASE("uint8_t is defined") {
        uint8_t value = 0;
        FL_CHECK(sizeof(value) == 1);
    }

    FL_SUBCASE("uint8_t is unsigned") {
        uint8_t value = 255;
        FL_CHECK(value > 0);
        value = value + 1;  // Should wrap to 0
        FL_CHECK(value == 0);
    }

    FL_SUBCASE("uint8_t can hold 0-255") {
        uint8_t min_val = 0;
        uint8_t max_val = 255;
        FL_CHECK(min_val == 0);
        FL_CHECK(max_val == 255);
    }

    FL_SUBCASE("uint8_t size is 1 byte") {
        FL_CHECK(sizeof(uint8_t) == 1);
    }
}

FL_TEST_CASE("int8_t type") {
    FL_SUBCASE("int8_t is defined") {
        int8_t value = 0;
        FL_CHECK(sizeof(value) == 1);
    }

    FL_SUBCASE("int8_t is signed") {
        int8_t positive = 10;
        int8_t negative = -10;
        FL_CHECK(positive > 0);
        FL_CHECK(negative < 0);
    }

    FL_SUBCASE("int8_t can hold -128 to 127") {
        int8_t min_val = -128;
        int8_t max_val = 127;
        FL_CHECK(min_val == -128);
        FL_CHECK(max_val == 127);
    }

    FL_SUBCASE("int8_t size is 1 byte") {
        FL_CHECK(sizeof(int8_t) == 1);
    }
}

FL_TEST_CASE("uint16_t type") {
    FL_SUBCASE("uint16_t is defined") {
        uint16_t value = 0;
        FL_CHECK(sizeof(value) == 2);
    }

    FL_SUBCASE("uint16_t is unsigned") {
        uint16_t value = 65535;
        FL_CHECK(value > 0);
        value = value + 1;  // Should wrap to 0
        FL_CHECK(value == 0);
    }

    FL_SUBCASE("uint16_t can hold 0-65535") {
        uint16_t min_val = 0;
        uint16_t max_val = 65535;
        FL_CHECK(min_val == 0);
        FL_CHECK(max_val == 65535);
    }

    FL_SUBCASE("uint16_t size is 2 bytes") {
        FL_CHECK(sizeof(uint16_t) == 2);
    }
}

FL_TEST_CASE("int16_t type") {
    FL_SUBCASE("int16_t is defined") {
        int16_t value = 0;
        FL_CHECK(sizeof(value) == 2);
    }

    FL_SUBCASE("int16_t is signed") {
        int16_t positive = 1000;
        int16_t negative = -1000;
        FL_CHECK(positive > 0);
        FL_CHECK(negative < 0);
    }

    FL_SUBCASE("int16_t can hold -32768 to 32767") {
        int16_t min_val = -32768;
        int16_t max_val = 32767;
        FL_CHECK(min_val == -32768);
        FL_CHECK(max_val == 32767);
    }

    FL_SUBCASE("int16_t size is 2 bytes") {
        FL_CHECK(sizeof(int16_t) == 2);
    }
}

FL_TEST_CASE("uint32_t type") {
    FL_SUBCASE("uint32_t is defined") {
        uint32_t value = 0;
        FL_CHECK(sizeof(value) == 4);
    }

    FL_SUBCASE("uint32_t is unsigned") {
        uint32_t value = 4294967295U;
        FL_CHECK(value > 0);
        value = value + 1;  // Should wrap to 0
        FL_CHECK(value == 0);
    }

    FL_SUBCASE("uint32_t can hold 0-4294967295") {
        uint32_t min_val = 0;
        uint32_t max_val = 4294967295U;
        FL_CHECK(min_val == 0);
        FL_CHECK(max_val == 4294967295U);
    }

    FL_SUBCASE("uint32_t size is 4 bytes") {
        FL_CHECK(sizeof(uint32_t) == 4);
    }
}

FL_TEST_CASE("int32_t type") {
    FL_SUBCASE("int32_t is defined") {
        int32_t value = 0;
        FL_CHECK(sizeof(value) == 4);
    }

    FL_SUBCASE("int32_t is signed") {
        int32_t positive = 100000;
        int32_t negative = -100000;
        FL_CHECK(positive > 0);
        FL_CHECK(negative < 0);
    }

    FL_SUBCASE("int32_t can hold -2147483648 to 2147483647") {
        int32_t min_val = -2147483647 - 1;  // INT32_MIN
        int32_t max_val = 2147483647;       // INT32_MAX
        FL_CHECK(min_val == -2147483648LL);
        FL_CHECK(max_val == 2147483647);
    }

    FL_SUBCASE("int32_t size is 4 bytes") {
        FL_CHECK(sizeof(int32_t) == 4);
    }
}

FL_TEST_CASE("uint64_t type") {
    FL_SUBCASE("uint64_t is defined") {
        uint64_t value = 0;
        FL_CHECK(sizeof(value) == 8);
    }

    FL_SUBCASE("uint64_t is unsigned") {
        uint64_t value = 18446744073709551615ULL;
        FL_CHECK(value > 0);
        value = value + 1;  // Should wrap to 0
        FL_CHECK(value == 0);
    }

    FL_SUBCASE("uint64_t can hold large values") {
        uint64_t min_val = 0;
        uint64_t max_val = 18446744073709551615ULL;
        FL_CHECK(min_val == 0);
        FL_CHECK(max_val == 18446744073709551615ULL);
    }

    FL_SUBCASE("uint64_t size is 8 bytes") {
        FL_CHECK(sizeof(uint64_t) == 8);
    }
}

FL_TEST_CASE("int64_t type") {
    FL_SUBCASE("int64_t is defined") {
        int64_t value = 0;
        FL_CHECK(sizeof(value) == 8);
    }

    FL_SUBCASE("int64_t is signed") {
        int64_t positive = 10000000000LL;
        int64_t negative = -10000000000LL;
        FL_CHECK(positive > 0);
        FL_CHECK(negative < 0);
    }

    FL_SUBCASE("int64_t can hold large values") {
        int64_t min_val = -9223372036854775807LL - 1;  // INT64_MIN
        int64_t max_val = 9223372036854775807LL;       // INT64_MAX
        FL_CHECK(min_val < 0);
        FL_CHECK(max_val > 0);
    }

    FL_SUBCASE("int64_t size is 8 bytes") {
        FL_CHECK(sizeof(int64_t) == 8);
    }
}

FL_TEST_CASE("size_t type") {
    FL_SUBCASE("size_t is defined") {
        size_t value = 0;
        FL_CHECK(sizeof(value) >= sizeof(void*));  // size_t must be able to hold any array size
    }

    FL_SUBCASE("size_t is unsigned") {
        size_t value = 100;
        FL_CHECK(value > 0);
        size_t zero = 0;
        FL_CHECK(zero == 0);
        FL_CHECK(zero - 1 > 0);  // Wraps around (unsigned)
    }

    FL_SUBCASE("size_t can hold array sizes") {
        char array[100];  // Small array for testing
        size_t size = sizeof(array);
        FL_CHECK(size == 100);
    }
}

FL_TEST_CASE("ptrdiff_t type") {
    FL_SUBCASE("ptrdiff_t is defined") {
        ptrdiff_t value = 0;
        FL_CHECK(sizeof(value) >= sizeof(void*));  // ptrdiff_t must be able to hold pointer difference
    }

    FL_SUBCASE("ptrdiff_t is signed") {
        ptrdiff_t positive = 100;
        ptrdiff_t negative = -100;
        FL_CHECK(positive > 0);
        FL_CHECK(negative < 0);
    }

    FL_SUBCASE("ptrdiff_t can hold pointer differences") {
        int array[10];
        ptrdiff_t diff = &array[5] - &array[2];
        FL_CHECK(diff == 3);
    }
}

FL_TEST_CASE("uintptr_t type") {
    FL_SUBCASE("uintptr_t is defined") {
        uintptr_t value = 0;
        FL_CHECK(sizeof(value) == sizeof(void*));  // uintptr_t must be same size as pointer
    }

    FL_SUBCASE("uintptr_t is unsigned") {
        uintptr_t value = 100;
        FL_CHECK(value > 0);
    }

    FL_SUBCASE("uintptr_t can hold pointer values") {
        int x = 42;
        void* ptr = &x;
        uintptr_t ptr_val = reinterpret_cast<uintptr_t>(ptr);
        void* ptr_back = reinterpret_cast<void*>(ptr_val);
        FL_CHECK(ptr == ptr_back);
    }
}

FL_TEST_CASE("intptr_t type") {
    FL_SUBCASE("intptr_t is defined") {
        intptr_t value = 0;
        FL_CHECK(sizeof(value) == sizeof(void*));  // intptr_t must be same size as pointer
    }

    FL_SUBCASE("intptr_t is signed") {
        intptr_t positive = 100;
        intptr_t negative = -100;
        FL_CHECK(positive > 0);
        FL_CHECK(negative < 0);
    }

    FL_SUBCASE("intptr_t can hold pointer values") {
        int x = 42;
        void* ptr = &x;
        intptr_t ptr_val = reinterpret_cast<intptr_t>(ptr);
        void* ptr_back = reinterpret_cast<void*>(ptr_val);
        FL_CHECK(ptr == ptr_back);
    }
}

FL_TEST_CASE("INT8_MIN macro") {
    FL_SUBCASE("INT8_MIN is defined") {
        #ifdef INT8_MIN
        FL_CHECK(true);
        #else
        FL_CHECK(false);
        #endif
    }

    FL_SUBCASE("INT8_MIN value is -128") {
        FL_CHECK(INT8_MIN == -128);
    }

    FL_SUBCASE("INT8_MIN matches int8_t minimum") {
        int8_t min_val = INT8_MIN;
        FL_CHECK(min_val == -128);
    }
}

FL_TEST_CASE("INT8_MAX macro") {
    FL_SUBCASE("INT8_MAX is defined") {
        #ifdef INT8_MAX
        FL_CHECK(true);
        #else
        FL_CHECK(false);
        #endif
    }

    FL_SUBCASE("INT8_MAX value is 127") {
        FL_CHECK(INT8_MAX == 127);
    }

    FL_SUBCASE("INT8_MAX matches int8_t maximum") {
        int8_t max_val = INT8_MAX;
        FL_CHECK(max_val == 127);
    }
}

FL_TEST_CASE("INT16_MIN macro") {
    FL_SUBCASE("INT16_MIN is defined") {
        #ifdef INT16_MIN
        FL_CHECK(true);
        #else
        FL_CHECK(false);
        #endif
    }

    FL_SUBCASE("INT16_MIN value is -32768") {
        FL_CHECK(INT16_MIN == -32768);
    }

    FL_SUBCASE("INT16_MIN matches int16_t minimum") {
        int16_t min_val = INT16_MIN;
        FL_CHECK(min_val == -32768);
    }
}

FL_TEST_CASE("INT16_MAX macro") {
    FL_SUBCASE("INT16_MAX is defined") {
        #ifdef INT16_MAX
        FL_CHECK(true);
        #else
        FL_CHECK(false);
        #endif
    }

    FL_SUBCASE("INT16_MAX value is 32767") {
        FL_CHECK(INT16_MAX == 32767);
    }

    FL_SUBCASE("INT16_MAX matches int16_t maximum") {
        int16_t max_val = INT16_MAX;
        FL_CHECK(max_val == 32767);
    }
}

FL_TEST_CASE("INT32_MIN macro") {
    FL_SUBCASE("INT32_MIN is defined") {
        #ifdef INT32_MIN
        FL_CHECK(true);
        #else
        FL_CHECK(false);
        #endif
    }

    FL_SUBCASE("INT32_MIN value is -2147483648") {
        FL_CHECK(INT32_MIN == -2147483648LL);
    }

    FL_SUBCASE("INT32_MIN matches int32_t minimum") {
        int32_t min_val = INT32_MIN;
        FL_CHECK(min_val == -2147483648LL);
    }
}

FL_TEST_CASE("INT32_MAX macro") {
    FL_SUBCASE("INT32_MAX is defined") {
        #ifdef INT32_MAX
        FL_CHECK(true);
        #else
        FL_CHECK(false);
        #endif
    }

    FL_SUBCASE("INT32_MAX value is 2147483647") {
        FL_CHECK(INT32_MAX == 2147483647);
    }

    FL_SUBCASE("INT32_MAX matches int32_t maximum") {
        int32_t max_val = INT32_MAX;
        FL_CHECK(max_val == 2147483647);
    }
}

FL_TEST_CASE("INT64_MIN macro") {
    FL_SUBCASE("INT64_MIN is defined") {
        #ifdef INT64_MIN
        FL_CHECK(true);
        #else
        FL_CHECK(false);
        #endif
    }

    FL_SUBCASE("INT64_MIN value is very negative") {
        FL_CHECK(INT64_MIN == -9223372036854775807LL - 1);
    }

    FL_SUBCASE("INT64_MIN matches int64_t minimum") {
        int64_t min_val = INT64_MIN;
        FL_CHECK(min_val < 0);
        FL_CHECK(min_val == -9223372036854775807LL - 1);
    }
}

FL_TEST_CASE("INT64_MAX macro") {
    FL_SUBCASE("INT64_MAX is defined") {
        #ifdef INT64_MAX
        FL_CHECK(true);
        #else
        FL_CHECK(false);
        #endif
    }

    FL_SUBCASE("INT64_MAX value is very positive") {
        FL_CHECK(INT64_MAX == 9223372036854775807LL);
    }

    FL_SUBCASE("INT64_MAX matches int64_t maximum") {
        int64_t max_val = INT64_MAX;
        FL_CHECK(max_val > 0);
        FL_CHECK(max_val == 9223372036854775807LL);
    }
}

FL_TEST_CASE("UINT8_MAX macro") {
    FL_SUBCASE("UINT8_MAX is defined") {
        #ifdef UINT8_MAX
        FL_CHECK(true);
        #else
        FL_CHECK(false);
        #endif
    }

    FL_SUBCASE("UINT8_MAX value is 255") {
        FL_CHECK(UINT8_MAX == 0xFF);
        FL_CHECK(UINT8_MAX == 255);
    }

    FL_SUBCASE("UINT8_MAX matches uint8_t maximum") {
        uint8_t max_val = UINT8_MAX;
        FL_CHECK(max_val == 255);
    }
}

FL_TEST_CASE("UINT16_MAX macro") {
    FL_SUBCASE("UINT16_MAX is defined") {
        #ifdef UINT16_MAX
        FL_CHECK(true);
        #else
        FL_CHECK(false);
        #endif
    }

    FL_SUBCASE("UINT16_MAX value is 65535") {
        FL_CHECK(UINT16_MAX == 0xFFFF);
        FL_CHECK(UINT16_MAX == 65535);
    }

    FL_SUBCASE("UINT16_MAX matches uint16_t maximum") {
        uint16_t max_val = UINT16_MAX;
        FL_CHECK(max_val == 65535);
    }
}

FL_TEST_CASE("UINT32_MAX macro") {
    FL_SUBCASE("UINT32_MAX is defined") {
        #ifdef UINT32_MAX
        FL_CHECK(true);
        #else
        FL_CHECK(false);
        #endif
    }

    FL_SUBCASE("UINT32_MAX value is 4294967295") {
        FL_CHECK(UINT32_MAX == 0xFFFFFFFFU);
        FL_CHECK(UINT32_MAX == 4294967295U);
    }

    FL_SUBCASE("UINT32_MAX matches uint32_t maximum") {
        uint32_t max_val = UINT32_MAX;
        FL_CHECK(max_val == 4294967295U);
    }
}

FL_TEST_CASE("UINT64_MAX macro") {
    FL_SUBCASE("UINT64_MAX is defined") {
        #ifdef UINT64_MAX
        FL_CHECK(true);
        #else
        FL_CHECK(false);
        #endif
    }

    FL_SUBCASE("UINT64_MAX value is 18446744073709551615") {
        FL_CHECK(UINT64_MAX == 0xFFFFFFFFFFFFFFFFULL);
        FL_CHECK(UINT64_MAX == 18446744073709551615ULL);
    }

    FL_SUBCASE("UINT64_MAX matches uint64_t maximum") {
        uint64_t max_val = UINT64_MAX;
        FL_CHECK(max_val == 18446744073709551615ULL);
    }
}

FL_TEST_CASE("type sizes are consistent") {
    FL_SUBCASE("8-bit types are 1 byte") {
        FL_CHECK(sizeof(int8_t) == 1);
        FL_CHECK(sizeof(uint8_t) == 1);
    }

    FL_SUBCASE("16-bit types are 2 bytes") {
        FL_CHECK(sizeof(int16_t) == 2);
        FL_CHECK(sizeof(uint16_t) == 2);
    }

    FL_SUBCASE("32-bit types are 4 bytes") {
        FL_CHECK(sizeof(int32_t) == 4);
        FL_CHECK(sizeof(uint32_t) == 4);
    }

    FL_SUBCASE("64-bit types are 8 bytes") {
        FL_CHECK(sizeof(int64_t) == 8);
        FL_CHECK(sizeof(uint64_t) == 8);
    }

    FL_SUBCASE("pointer-sized types match pointer size") {
        FL_CHECK(sizeof(intptr_t) == sizeof(void*));
        FL_CHECK(sizeof(uintptr_t) == sizeof(void*));
        FL_CHECK(sizeof(size_t) >= sizeof(void*));
        FL_CHECK(sizeof(ptrdiff_t) >= sizeof(void*));
    }
}

FL_TEST_CASE("signed and unsigned pairs") {
    FL_SUBCASE("8-bit signed and unsigned have same size") {
        FL_CHECK(sizeof(int8_t) == sizeof(uint8_t));
    }

    FL_SUBCASE("16-bit signed and unsigned have same size") {
        FL_CHECK(sizeof(int16_t) == sizeof(uint16_t));
    }

    FL_SUBCASE("32-bit signed and unsigned have same size") {
        FL_CHECK(sizeof(int32_t) == sizeof(uint32_t));
    }

    FL_SUBCASE("64-bit signed and unsigned have same size") {
        FL_CHECK(sizeof(int64_t) == sizeof(uint64_t));
    }

    FL_SUBCASE("pointer-sized signed and unsigned have same size") {
        FL_CHECK(sizeof(intptr_t) == sizeof(uintptr_t));
    }
}

FL_TEST_CASE("overflow behavior") {
    FL_SUBCASE("uint8_t overflow wraps to 0") {
        uint8_t value = UINT8_MAX;
        value = value + 1;
        FL_CHECK(value == 0);
    }

    FL_SUBCASE("uint16_t overflow wraps to 0") {
        uint16_t value = UINT16_MAX;
        value = value + 1;
        FL_CHECK(value == 0);
    }

    FL_SUBCASE("uint32_t overflow wraps to 0") {
        uint32_t value = UINT32_MAX;
        value = value + 1;
        FL_CHECK(value == 0);
    }

    FL_SUBCASE("uint64_t overflow wraps to 0") {
        uint64_t value = UINT64_MAX;
        value = value + 1;
        FL_CHECK(value == 0);
    }
}

FL_TEST_CASE("arithmetic operations") {
    FL_SUBCASE("uint8_t addition") {
        uint8_t a = 100;
        uint8_t b = 50;
        uint8_t result = a + b;
        FL_CHECK(result == 150);
    }

    FL_SUBCASE("int8_t subtraction") {
        int8_t a = 50;
        int8_t b = 30;
        int8_t result = a - b;
        FL_CHECK(result == 20);
    }

    FL_SUBCASE("uint16_t multiplication") {
        uint16_t a = 200;
        uint16_t b = 3;
        uint16_t result = a * b;
        FL_CHECK(result == 600);
    }

    FL_SUBCASE("int32_t division") {
        int32_t a = 1000;
        int32_t b = 10;
        int32_t result = a / b;
        FL_CHECK(result == 100);
    }

    FL_SUBCASE("uint64_t large operations") {
        uint64_t a = 1000000000ULL;
        uint64_t b = 1000000000ULL;
        uint64_t result = a * b;
        FL_CHECK(result == 1000000000000000000ULL);
    }
}

FL_TEST_CASE("type conversions") {
    FL_SUBCASE("uint8_t to uint16_t") {
        uint8_t small = 200;
        uint16_t large = small;
        FL_CHECK(large == 200);
    }

    FL_SUBCASE("int8_t to int16_t preserves sign") {
        int8_t negative = -50;
        int16_t larger = negative;
        FL_CHECK(larger == -50);
        FL_CHECK(larger < 0);
    }

    FL_SUBCASE("uint32_t to uint64_t") {
        uint32_t small = 4000000000U;
        uint64_t large = small;
        FL_CHECK(large == 4000000000ULL);
    }

    FL_SUBCASE("size_t to uintptr_t") {
        size_t s = 12345;
        uintptr_t p = static_cast<uintptr_t>(s);
        FL_CHECK(p == 12345);
    }
}

FL_TEST_CASE("limits consistency") {
    FL_SUBCASE("INT8_MIN and INT8_MAX are consistent") {
        FL_CHECK(INT8_MAX > INT8_MIN);
        FL_CHECK(INT8_MAX - INT8_MIN == 255);
    }

    FL_SUBCASE("INT16_MIN and INT16_MAX are consistent") {
        FL_CHECK(INT16_MAX > INT16_MIN);
        FL_CHECK(INT16_MAX - INT16_MIN == 65535);
    }

    FL_SUBCASE("INT32_MIN and INT32_MAX are consistent") {
        FL_CHECK(INT32_MAX > INT32_MIN);
    }

    FL_SUBCASE("UINT8_MAX is maximum for uint8_t") {
        uint8_t max_val = UINT8_MAX;
        uint8_t over = max_val + 1;
        FL_CHECK(over == 0);  // Wraps around
    }

    FL_SUBCASE("UINT16_MAX is maximum for uint16_t") {
        uint16_t max_val = UINT16_MAX;
        uint16_t over = max_val + 1;
        FL_CHECK(over == 0);  // Wraps around
    }
}

FL_TEST_CASE("pointer arithmetic with ptrdiff_t") {
    FL_SUBCASE("array pointer difference") {
        int array[100];
        int* start = &array[0];
        int* end = &array[99];
        ptrdiff_t diff = end - start;
        FL_CHECK(diff == 99);
    }

    FL_SUBCASE("negative pointer difference") {
        int array[100];
        int* start = &array[10];
        int* end = &array[5];
        ptrdiff_t diff = end - start;
        FL_CHECK(diff == -5);
    }

    FL_SUBCASE("zero pointer difference") {
        int x = 42;
        int* ptr1 = &x;
        int* ptr2 = &x;
        ptrdiff_t diff = ptr2 - ptr1;
        FL_CHECK(diff == 0);
    }
}

FL_TEST_CASE("size_t with sizeof") {
    FL_SUBCASE("sizeof returns size_t") {
        size_t size_of_int = sizeof(int);
        FL_CHECK(size_of_int >= 1);  // At least 1 byte
    }

    FL_SUBCASE("sizeof array") {
        char array[100];
        size_t array_size = sizeof(array);
        FL_CHECK(array_size == 100);
    }

    FL_SUBCASE("sizeof struct") {
        struct TestStruct {
            int a;
            char b;
        };
        size_t struct_size = sizeof(TestStruct);
        FL_CHECK(struct_size >= 5);  // At least int + char
    }
}
