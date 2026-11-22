// fl::string optimization tests (numeric append performance, hex/octal,
// thread safety, buffer sizes, write-method numeric variants, memory
// efficiency, StringFormatter buffer reuse, precision/accuracy,
// construction from span).
// Extracted from string.cpp (sub-issue of #3131, meta #3127).

#include "fl/stl/compiler_control.h"
#include "fl/stl/span.h"
#include "fl/stl/string.h"
#include "fl/stl/thread.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

// SECTION: Tests from string_optimization.cpp
//=============================================================================


FL_TEST_CASE("fl::string - Numeric append performance patterns") {
    // Test numeric append operations that currently allocate temporary fl::string buffers
    // These tests validate that optimizations don't break functionality

    FL_SUBCASE("Integer append operations") {
        fl::string s;

        // Test various integer types
        s.append(static_cast<i8>(127));
        FL_CHECK(s == "127");

        s.clear();
        s.append(static_cast<u8>(255));
        FL_CHECK(s == "255");

        s.clear();
        s.append(static_cast<i16>(-32768));
        FL_CHECK(s == "-32768");

        s.clear();
        s.append(static_cast<u16>(65535));
        FL_CHECK(s == "65535");

        s.clear();
        s.append(static_cast<i32>(-2147483647));
        FL_CHECK(s == "-2147483647");

        s.clear();
        s.append(static_cast<u32>(4294967295U));
        FL_CHECK(s == "4294967295");
    }

    FL_SUBCASE("64-bit integer append operations") {
        fl::string s;

        s.append(static_cast<int64_t>(-9223372036854775807LL));
        FL_CHECK(s == "-9223372036854775807");

        s.clear();
        s.append(static_cast<uint64_t>(18446744073709551615ULL));
        FL_CHECK(s == "18446744073709551615");
    }

    FL_SUBCASE("Float append operations") {
        fl::string s;

        s.append(3.14159f);
        // Check that it contains a decimal representation
        FL_CHECK(s.size() > 0);
        FL_CHECK(s.find('.') != string::npos);

        s.clear();
        s.append(-273.15f);
        FL_CHECK(s.size() > 0);
        FL_CHECK(s[0] == '-');
    }

    FL_SUBCASE("Mixed numeric append operations") {
        fl::string s;

        s.append("Value: ");
        s.append(42);
        s.append(", Float: ");
        s.append(3.14f);
        s.append(", Hex: 0x");
        s.appendHex(static_cast<u32>(255));

        FL_CHECK(s.find("42") != string::npos);
        FL_CHECK(s.find("3.14") != string::npos);
        // Check for either lowercase or uppercase hex output
        bool has_hex = (s.find("ff") != string::npos) || (s.find("FF") != string::npos);
        FL_CHECK(has_hex);
    }

    FL_SUBCASE("Rapid numeric append sequence") {
        fl::string s;

        // Simulate rapid appends that would benefit from buffer reuse
        for (int i = 0; i < 100; ++i) {
            s.append(i);
            if (i < 99) {
                s.append(",");
            }
        }

        FL_CHECK(s.find("0,1,2") != string::npos);
        FL_CHECK(s.find("98,99") != string::npos);
    }
}

FL_TEST_CASE("fl::string - Hexadecimal formatting") {
    FL_SUBCASE("Hex append basic") {
        fl::string s;

        s.appendHex(static_cast<u8>(0xFF));
        FL_CHECK(s.size() > 0);

        s.clear();
        s.appendHex(static_cast<u32>(0xDEADBEEF));
        FL_CHECK(s.size() > 0);
    }

    FL_SUBCASE("Hex append 64-bit") {
        fl::string s;

        s.appendHex(static_cast<uint64_t>(0xFEEDFACECAFEBEEFULL));
        FL_CHECK(s.size() > 0);
    }
}

FL_TEST_CASE("fl::string - Octal formatting") {
    FL_SUBCASE("Octal append basic") {
        fl::string s;

        s.appendOct(static_cast<u32>(8));
        FL_CHECK(s == "10");  // 8 in octal is "10"

        s.clear();
        s.appendOct(static_cast<u32>(64));
        FL_CHECK(s == "100");  // 64 in octal is "100"
    }
}

FL_TEST_CASE("fl::string - Thread safety of numeric operations") {
    // Test that numeric append operations work correctly when called from multiple threads
    // This is important if we use thread-local buffers for optimization

    FL_SUBCASE("Concurrent numeric appends") {
        const int kNumThreads = 4;
        const int kIterations = 100;

        fl::vector<fl::thread> threads;
        fl::vector<fl::string> results(kNumThreads);

        for (int t = 0; t < kNumThreads; ++t) {
            threads.emplace_back([t, &results]() {
                fl::string& s = results[t];
                for (int i = 0; i < kIterations; ++i) {
                    s.append(t * 1000 + i);
                    s.append(",");
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        // Verify each thread produced correct output
        for (int t = 0; t < kNumThreads; ++t) {
            const fl::string& s = results[t];
            FL_CHECK(s.size() > 0);

            // Check that the string starts with the thread's base value
            char expected_start[32];
            snprintf(expected_start, sizeof(expected_start), "%d,", t * 1000);
            FL_CHECK(s.find(expected_start) == 0);
        }
    }

    FL_SUBCASE("Concurrent mixed format appends") {
        const int kNumThreads = 4;

        fl::vector<fl::thread> threads;
        fl::vector<fl::string> results(kNumThreads);

        for (int t = 0; t < kNumThreads; ++t) {
            threads.emplace_back([t, &results]() {
                fl::string& s = results[t];

                // Mix different formatting operations
                s.append("Dec:");
                s.append(t);
                s.append(",Hex:");
                s.appendHex(t);
                s.append(",Oct:");
                s.appendOct(t);
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        // Verify correct output
        for (int t = 0; t < kNumThreads; ++t) {
            const fl::string& s = results[t];
            FL_CHECK(s.find("Dec:") != string::npos);
            FL_CHECK(s.find("Hex:") != string::npos);
            FL_CHECK(s.find("Oct:") != string::npos);
        }
    }
}

FL_TEST_CASE("fl::string - Buffer size requirements") {
    // Test edge cases for numeric formatting buffer sizes

    FL_SUBCASE("Maximum 64-bit value") {
        fl::string s;

        // Maximum uint64_t requires 20 digits in decimal
        s.append(static_cast<uint64_t>(18446744073709551615ULL));
        FL_CHECK(s.size() == 20);
        FL_CHECK(s == "18446744073709551615");
    }

    FL_SUBCASE("Minimum int64_t value") {
        fl::string s;

        // Minimum int64_t: -9223372036854775808 (20 digits + sign)
        // Note: We use -9223372036854775807 to avoid overflow issues
        s.append(static_cast<int64_t>(-9223372036854775807LL));
        FL_CHECK(s.size() == 20);  // 19 digits + sign
    }

    FL_SUBCASE("Hex formatting maximum") {
        fl::string s;

        // Maximum uint64_t in hex: 16 hex digits
        s.appendHex(static_cast<uint64_t>(0xFFFFFFFFFFFFFFFFULL));
        FL_CHECK(s.size() == 16);
    }

    FL_SUBCASE("Float formatting buffer requirements") {
        fl::string s;

        // Test various float edge cases
        s.append(1.234567890123456789f);  // Precision test
        FL_CHECK(s.size() > 0);

        s.clear();
        s.append(-1.234567890123456789f);
        FL_CHECK(s.size() > 0);
        FL_CHECK(s[0] == '-');

        s.clear();
        s.append(0.0f);
        FL_CHECK(s.size() > 0);
    }
}

FL_TEST_CASE("fl::string - Write method numeric variants") {
    // Test the write() methods that take numeric types
    // These also use temporary fl::string buffers

    FL_SUBCASE("write() with integers") {
        fl::string s;

        s.write(static_cast<u16>(42));
        FL_CHECK(s == "42");

        s.clear();
        s.write(static_cast<u32>(4294967295U));
        FL_CHECK(s == "4294967295");

        s.clear();
        s.write(static_cast<uint64_t>(18446744073709551615ULL));
        FL_CHECK(s == "18446744073709551615");
    }

    FL_SUBCASE("write() with signed integers") {
        fl::string s;

        s.write(static_cast<i32>(-2147483647));
        FL_CHECK(s == "-2147483647");

        s.clear();
        s.write(static_cast<i8>(-128));
        FL_CHECK(s == "-128");
    }

    FL_SUBCASE("Sequential write operations") {
        fl::string s;

        s.append("Count: ");
        s.write(static_cast<u32>(100));
        s.append(", Value: ");
        s.write(static_cast<i32>(-50));

        FL_CHECK(s.find("100") != string::npos);
        FL_CHECK(s.find("-50") != string::npos);
    }
}

FL_TEST_CASE("fl::string - Memory efficiency improvements") {
    // Test patterns that could benefit from thread-local buffer optimization

    FL_SUBCASE("Repeated small string builds") {
        // This pattern creates many temporary fl::string buffers (reduced from 1000 to 500 for performance)
        fl::vector<fl::string> results;

        for (int i = 0; i < 500; ++i) {
            fl::string s;
            s.append("Item ");
            s.append(i);
            s.append(": Value=");
            s.append(i * 2);
            results.push_back(s);
        }

        FL_CHECK(results.size() == 500);
        FL_CHECK(results[0] == "Item 0: Value=0");
        FL_CHECK(results[499] == "Item 499: Value=998");
    }

    FL_SUBCASE("String builder pattern") {
        fl::string s;

        // Simulate building a complex string with many numeric appends
        for (int i = 0; i < 50; ++i) {
            s.append("Entry[");
            s.append(i);
            s.append("]=");
            s.append(i * i);
            s.append("; ");
        }

        FL_CHECK(s.find("Entry[0]=0") != string::npos);
        FL_CHECK(s.find("Entry[49]=2401") != string::npos);
    }
}

FL_TEST_CASE("fl::string - StringFormatter buffer reuse") {
    // Test that StringFormatter can safely reuse buffers across multiple calls

    FL_SUBCASE("Repeated calls with same formatter") {
        fl::string results[10];

        for (int i = 0; i < 10; ++i) {
            results[i].append(i * 111);
        }

        // Verify all results are independent
        FL_CHECK(results[0] == "0");
        FL_CHECK(results[1] == "111");
        FL_CHECK(results[9] == "999");
    }

    FL_SUBCASE("Interleaved formatting operations") {
        fl::string s1, s2;

        // Interleave operations on two strings
        s1.append(100);
        s2.append(200);
        s1.append(300);
        s2.append(400);

        FL_CHECK(s1.find("100") != string::npos);
        FL_CHECK(s1.find("300") != string::npos);
        FL_CHECK(s2.find("200") != string::npos);
        FL_CHECK(s2.find("400") != string::npos);
    }
}

FL_TEST_CASE("fl::string - Precision and accuracy") {
    // Ensure optimizations don't affect output correctness

    FL_SUBCASE("Float precision") {
        fl::string s;

        s.append(1.5f);
        FL_CHECK(s.find("1.5") != string::npos);

        s.clear();
        s.append(0.123f);
        FL_CHECK(s.size() > 0);
    }

    FL_SUBCASE("Negative zero handling") {
        fl::string s;
        s.append(-0.0f);
        FL_CHECK(s.size() > 0);
    }

    FL_SUBCASE("All integer sizes produce correct output") {
        fl::string s;

        // Test boundary values for each integer type
        s.append(static_cast<u8>(255));
        FL_CHECK(s == "255");

        s.clear();
        s.append(static_cast<i8>(-128));
        FL_CHECK(s == "-128");

        s.clear();
        s.append(static_cast<u16>(65535));
        FL_CHECK(s == "65535");

        s.clear();
        s.append(static_cast<i16>(-32768));
        FL_CHECK(s == "-32768");
    }
}

FL_TEST_CASE("fl::string - Construction from span") {
    FL_SUBCASE("Construction from span<const char>") {
        const char data[] = "hello world";
        fl::span<const char> sp(data, 5); // Only first 5 chars: "hello"
        fl::string s(sp);

        FL_CHECK(s.size() == 5);
        FL_CHECK(s == "hello");
    }

    FL_SUBCASE("Construction from span<char>") {
        char data[] = "test string";
        fl::span<char> sp(data, 4); // Only first 4 chars: "test"
        fl::string s(sp);

        FL_CHECK(s.size() == 4);
        FL_CHECK(s == "test");
    }

    FL_SUBCASE("Construction from empty span<const char>") {
        fl::span<const char> sp;
        fl::string s(sp);

        FL_CHECK(s.size() == 0);
        FL_CHECK(s.empty());
    }

    FL_SUBCASE("Construction from empty span<char>") {
        fl::span<char> sp;
        fl::string s(sp);

        FL_CHECK(s.size() == 0);
        FL_CHECK(s.empty());
    }

    FL_SUBCASE("Span with entire string") {
        const char data[] = "full content";
        fl::span<const char> sp(data, sizeof(data) - 1); // Exclude null terminator
        fl::string s(sp);

        FL_CHECK(s.size() == 12);
        FL_CHECK(s == "full content");
    }

    FL_SUBCASE("Modifications don't affect original span") {
        char data[] = "original";
        fl::span<char> sp(data, 8);
        fl::string s(sp);

        s.append(" modified");

        FL_CHECK(s == "original modified");
        FL_CHECK(fl::strcmp(data, "original") == 0); // Original unchanged
    }
}

//=============================================================================

} // FL_TEST_FILE
