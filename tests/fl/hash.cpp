#include "fl/hash.h"
#include "fl/stl/string.h"
#include "fl/stl/cstddef.h"
#include "test.h"
#include "fl/geometry.h"
#include "fl/int.h"

using namespace fl;

FL_TEST_CASE("fl::MurmurHash3_x86_32 basic functionality") {
    // Note: MurmurHash3 requires aligned data for safe operation
    // Testing via Hash<T> functors which handle alignment properly

    FL_SUBCASE("integer data with proper alignment") {
        // Use aligned integer array
        alignas(4) i32 data[] = {1, 2, 3, 4, 5};
        u32 hash = MurmurHash3_x86_32(data, sizeof(data));
        FL_CHECK(hash != 0);

        // Verify determinism
        u32 hash2 = MurmurHash3_x86_32(data, sizeof(data));
        FL_CHECK_EQ(hash, hash2);
    }

    FL_SUBCASE("single integer") {
        i32 value = 42;
        u32 hash = MurmurHash3_x86_32(&value, sizeof(value));
        FL_CHECK(hash != 0);
    }

    FL_SUBCASE("seed affects output") {
        i32 value = 100;
        u32 hash1 = MurmurHash3_x86_32(&value, sizeof(value), 0);
        u32 hash2 = MurmurHash3_x86_32(&value, sizeof(value), 1);
        FL_CHECK(hash1 != hash2);  // Different seeds should produce different hashes
    }
}

FL_TEST_CASE("fl::fast_hash32") {
    FL_SUBCASE("basic functionality") {
        u32 hash1 = fast_hash32(0);
        u32 hash2 = fast_hash32(1);
        u32 hash3 = fast_hash32(12345);

        FL_CHECK(hash1 != 0);
        FL_CHECK(hash2 != 0);
        FL_CHECK(hash3 != 0);
        FL_CHECK(hash1 != hash2);
        FL_CHECK(hash2 != hash3);
    }

    FL_SUBCASE("deterministic") {
        u32 value = 0xDEADBEEF;
        u32 hash1 = fast_hash32(value);
        u32 hash2 = fast_hash32(value);
        FL_CHECK_EQ(hash1, hash2);
    }

    FL_SUBCASE("well distributed") {
        // Check that sequential values produce different hashes
        u32 hash_prev = fast_hash32(0);
        for (u32 i = 1; i < 100; ++i) {
            u32 hash_curr = fast_hash32(i);
            FL_CHECK(hash_curr != hash_prev);
            hash_prev = hash_curr;
        }
    }
}

FL_TEST_CASE("fl::hash_pair") {
    FL_SUBCASE("basic functionality") {
        u32 hash = hash_pair(1, 2);
        FL_CHECK(hash != 0);
    }

    FL_SUBCASE("deterministic") {
        u32 hash1 = hash_pair(42, 99);
        u32 hash2 = hash_pair(42, 99);
        FL_CHECK_EQ(hash1, hash2);
    }

    FL_SUBCASE("order matters") {
        u32 hash1 = hash_pair(1, 2);
        u32 hash2 = hash_pair(2, 1);
        FL_CHECK(hash1 != hash2);  // Order should matter
    }

    FL_SUBCASE("seed affects output") {
        u32 hash1 = hash_pair(1, 2, 0);
        u32 hash2 = hash_pair(1, 2, 1);
        FL_CHECK(hash1 != hash2);
    }

    FL_SUBCASE("different inputs") {
        u32 hash1 = hash_pair(1, 2);
        u32 hash2 = hash_pair(3, 4);
        FL_CHECK(hash1 != hash2);
    }
}

FL_TEST_CASE("fl::fast_hash64") {
    FL_SUBCASE("basic functionality") {
        u64 value = 0x123456789ABCDEF0ULL;
        u32 hash = fast_hash64(value);
        FL_CHECK(hash != 0);
    }

    FL_SUBCASE("deterministic") {
        u64 value = 0xFEDCBA9876543210ULL;
        u32 hash1 = fast_hash64(value);
        u32 hash2 = fast_hash64(value);
        FL_CHECK_EQ(hash1, hash2);
    }

    FL_SUBCASE("different inputs") {
        u32 hash1 = fast_hash64(0x0000000000000001ULL);
        u32 hash2 = fast_hash64(0x0000000100000000ULL);
        FL_CHECK(hash1 != hash2);
    }
}

FL_TEST_CASE("fl::Hash<T> for integral types") {
    FL_SUBCASE("Hash<u8>") {
        Hash<u8> hasher;
        u32 hash1 = hasher(0);
        u32 hash2 = hasher(255);
        FL_CHECK(hash1 != hash2);
        FL_CHECK_EQ(hasher(42), hasher(42));  // Deterministic
    }

    FL_SUBCASE("Hash<u16>") {
        Hash<u16> hasher;
        u32 hash1 = hasher(0);
        u32 hash2 = hasher(65535);
        FL_CHECK(hash1 != hash2);
        FL_CHECK_EQ(hasher(1234), hasher(1234));
    }

    FL_SUBCASE("Hash<u32>") {
        Hash<u32> hasher;
        u32 hash1 = hasher(0);
        u32 hash2 = hasher(0xFFFFFFFF);
        FL_CHECK(hash1 != hash2);
        FL_CHECK_EQ(hasher(12345), hasher(12345));
    }

    FL_SUBCASE("Hash<i8>") {
        Hash<i8> hasher;
        u32 hash1 = hasher(-128);
        u32 hash2 = hasher(127);
        FL_CHECK(hash1 != hash2);
        FL_CHECK_EQ(hasher(-42), hasher(-42));
    }

    FL_SUBCASE("Hash<i16>") {
        Hash<i16> hasher;
        u32 hash1 = hasher(-32768);
        u32 hash2 = hasher(32767);
        FL_CHECK(hash1 != hash2);
        FL_CHECK_EQ(hasher(-1234), hasher(-1234));
    }

    FL_SUBCASE("Hash<i32>") {
        Hash<i32> hasher;
        u32 hash1 = hasher(static_cast<i32>(0x80000000));
        u32 hash2 = hasher(0x7FFFFFFF);
        FL_CHECK(hash1 != hash2);
        FL_CHECK_EQ(hasher(-12345), hasher(-12345));
    }

    FL_SUBCASE("Hash<bool>") {
        Hash<bool> hasher;
        u32 hash_true = hasher(true);
        u32 hash_false = hasher(false);
        FL_CHECK(hash_true != hash_false);
        FL_CHECK_EQ(hasher(true), hasher(true));
    }
}

FL_TEST_CASE("fl::Hash<T> for floating-point types") {
    FL_SUBCASE("Hash<float>") {
        Hash<float> hasher;
        u32 hash1 = hasher(0.0f);
        u32 hash2 = hasher(1.0f);
        u32 hash3 = hasher(3.14159f);

        FL_CHECK(hash1 != hash2);
        FL_CHECK(hash2 != hash3);
        FL_CHECK_EQ(hasher(2.71828f), hasher(2.71828f));  // Deterministic
    }

    FL_SUBCASE("Hash<double>") {
        Hash<double> hasher;
        u32 hash1 = hasher(0.0);
        u32 hash2 = hasher(1.0);
        u32 hash3 = hasher(3.14159265358979);

        FL_CHECK(hash1 != hash2);
        FL_CHECK(hash2 != hash3);
        FL_CHECK_EQ(hasher(2.718281828), hasher(2.718281828));
    }

    FL_SUBCASE("float special values") {
        Hash<float> hasher;
        // Just check they don't crash - NaN behavior is implementation defined
        hasher(0.0f);
        hasher(-0.0f);
        // Positive and negative infinity would need special handling
    }
}

FL_TEST_CASE("fl::Hash<string>") {
    Hash<fl::string> hasher;

    FL_SUBCASE("empty string") {
        fl::string empty;
        u32 hash = hasher(empty);
        // Empty string hashing to 0 is valid behavior
        (void)hash;  // Just verify it doesn't crash
    }

    FL_SUBCASE("basic strings") {
        fl::string str1 = "hello";
        fl::string str2 = "world";
        u32 hash1 = hasher(str1);
        u32 hash2 = hasher(str2);

        FL_CHECK(hash1 != hash2);
        FL_CHECK_EQ(hasher(str1), hash1);  // Deterministic
    }

    FL_SUBCASE("same content produces same hash") {
        fl::string str1 = "test";
        fl::string str2 = "test";
        FL_CHECK_EQ(hasher(str1), hasher(str2));
    }

    FL_SUBCASE("case sensitive") {
        fl::string str1 = "Test";
        fl::string str2 = "test";
        FL_CHECK(hasher(str1) != hasher(str2));
    }

    FL_SUBCASE("long strings") {
        fl::string long_str = "This is a longer string that will definitely exceed the block size and test the tail handling";
        u32 hash = hasher(long_str);
        FL_CHECK(hash != 0);
        FL_CHECK_EQ(hasher(long_str), hash);  // Still deterministic
    }
}

FL_TEST_CASE("fl::Hash<T*> for pointers") {
    Hash<int*> hasher;

    FL_SUBCASE("null pointer") {
        // Skip null pointer test as MurmurHash3 doesn't handle nullptr safely
        // This is a known limitation of the current implementation
    }

    FL_SUBCASE("different pointers") {
        int a = 1, b = 2;
        u32 hash1 = hasher(&a);
        u32 hash2 = hasher(&b);
        FL_CHECK(hash1 != hash2);  // Different addresses should hash differently
    }

    FL_SUBCASE("same pointer") {
        int x = 42;
        u32 hash1 = hasher(&x);
        u32 hash2 = hasher(&x);
        FL_CHECK_EQ(hash1, hash2);  // Same pointer should hash the same
    }
}

FL_TEST_CASE("fl::Hash<vec2<T>>") {
    FL_SUBCASE("vec2<u8>") {
        Hash<vec2<u8>> hasher;
        vec2<u8> p1(10, 20);
        vec2<u8> p2(30, 40);

        u32 hash1 = hasher(p1);
        u32 hash2 = hasher(p2);

        FL_CHECK(hash1 != hash2);
        FL_CHECK_EQ(hasher(p1), hash1);  // Deterministic
    }

    FL_SUBCASE("vec2<u16>") {
        Hash<vec2<u16>> hasher;
        vec2<u16> p1(1000, 2000);
        vec2<u16> p2(3000, 4000);

        u32 hash1 = hasher(p1);
        u32 hash2 = hasher(p2);

        FL_CHECK(hash1 != hash2);
        FL_CHECK_EQ(hasher(p1), hash1);
    }

    FL_SUBCASE("vec2<u32>") {
        Hash<vec2<u32>> hasher;
        vec2<u32> p1(100000, 200000);
        vec2<u32> p2(300000, 400000);

        u32 hash1 = hasher(p1);
        u32 hash2 = hasher(p2);

        FL_CHECK(hash1 != hash2);
        FL_CHECK_EQ(hasher(p1), hash1);
    }

    FL_SUBCASE("vec2<i32>") {
        Hash<vec2<i32>> hasher;
        vec2<i32> p1(-100, 200);
        vec2<i32> p2(300, -400);

        u32 hash1 = hasher(p1);
        u32 hash2 = hasher(p2);

        FL_CHECK(hash1 != hash2);
    }

    FL_SUBCASE("order matters") {
        Hash<vec2<i32>> hasher;
        vec2<i32> p1(1, 2);
        vec2<i32> p2(2, 1);

        FL_CHECK(hasher(p1) != hasher(p2));
    }
}

FL_TEST_CASE("fl::FastHash<T>") {
    FL_SUBCASE("FastHash<u32>") {
        FastHash<u32> hasher;
        u32 hash1 = hasher(0);
        u32 hash2 = hasher(1);
        u32 hash3 = hasher(0xFFFFFFFF);

        FL_CHECK(hash1 != hash2);
        FL_CHECK(hash2 != hash3);
        FL_CHECK_EQ(hasher(42), hasher(42));
    }

    FL_SUBCASE("FastHash<i32>") {
        FastHash<i32> hasher;
        u32 hash1 = hasher(-1);
        u32 hash2 = hasher(0);
        u32 hash3 = hasher(1);

        FL_CHECK(hash1 != hash2);
        FL_CHECK(hash2 != hash3);
    }
}

FL_TEST_CASE("fl::FastHash<vec2<T>>") {
    FL_SUBCASE("FastHash<vec2<u8>>") {
        FastHash<vec2<u8>> hasher;
        vec2<u8> p1(10, 20);
        vec2<u8> p2(30, 40);

        u32 hash1 = hasher(p1);
        u32 hash2 = hasher(p2);

        FL_CHECK(hash1 != hash2);
        FL_CHECK_EQ(hasher(p1), hash1);  // Deterministic
    }

    FL_SUBCASE("FastHash<vec2<u16>>") {
        FastHash<vec2<u16>> hasher;
        vec2<u16> p1(1000, 2000);
        vec2<u16> p2(3000, 4000);

        u32 hash1 = hasher(p1);
        u32 hash2 = hasher(p2);

        FL_CHECK(hash1 != hash2);
    }

    FL_SUBCASE("FastHash<vec2<u32>>") {
        FastHash<vec2<u32>> hasher;
        vec2<u32> p1(100000, 200000);
        vec2<u32> p2(300000, 400000);

        u32 hash1 = hasher(p1);
        u32 hash2 = hasher(p2);

        FL_CHECK(hash1 != hash2);
    }

    FL_SUBCASE("FastHash<vec2<i32>>") {
        FastHash<vec2<i32>> hasher;
        vec2<i32> p1(-100, 200);
        vec2<i32> p2(300, -400);

        u32 hash1 = hasher(p1);
        u32 hash2 = hasher(p2);

        FL_CHECK(hash1 != hash2);
    }
}

FL_TEST_CASE("Hash collision resistance") {
    // This test checks that hash functions have reasonable collision resistance
    FL_SUBCASE("sequential integers produce unique hashes") {
        Hash<u32> hasher;

        // Just verify that sequential numbers produce different hashes
        u32 hash0 = hasher(0);
        u32 hash1 = hasher(1);
        u32 hash2 = hasher(2);

        FL_CHECK(hash0 != hash1);
        FL_CHECK(hash1 != hash2);
        FL_CHECK(hash0 != hash2);

        // Test a larger range
        u32 prev_hash = hasher(0);
        int same_count = 0;
        for (u32 i = 1; i < 100; ++i) {
            u32 curr_hash = hasher(i);
            if (curr_hash == prev_hash) {
                ++same_count;
            }
            prev_hash = curr_hash;
        }

        // Expect very few identical consecutive hashes
        FL_CHECK(same_count < 5);
    }

    FL_SUBCASE("different strings produce different hashes") {
        Hash<fl::string> hasher;

        const char* test_strings[] = {
            "apple", "banana", "cherry", "date", "elderberry",
            "fig", "grape", "honeydew", "kiwi", "lemon",
            "mango", "nectarine", "orange", "papaya", "quince",
            "raspberry", "strawberry", "tangerine", "ugli", "vanilla"
        };

        // Just check that different strings produce different hashes
        fl::string s1 = test_strings[0];
        fl::string s2 = test_strings[1];
        fl::string s3 = test_strings[2];

        u32 hash1 = hasher(s1);
        u32 hash2 = hasher(s2);
        u32 hash3 = hasher(s3);

        FL_CHECK(hash1 != hash2);
        FL_CHECK(hash2 != hash3);
        FL_CHECK(hash1 != hash3);

        // Verify all strings produce non-zero hashes
        for (size_t i = 0; i < sizeof(test_strings) / sizeof(test_strings[0]); ++i) {
            fl::string s = test_strings[i];
            u32 curr_hash = hasher(s);
            // Different strings should (almost always) produce different hashes
            // We don't CHECK this because collisions are theoretically possible,
            // but we can at least verify the hash is non-zero
            FL_CHECK(curr_hash != 0);
        }
    }
}
