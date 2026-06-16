// fl::string memory-safety tests (StringHolder capacity bugs).
// Extracted from string.cpp (sub-issue of #3131, meta #3127).

#include "fl/stl/compiler_control.h"
#include "fl/stl/cstring.h"
#include "fl/stl/string.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

//=============================================================================
// SECTION: Tests from string_memory_bugs.cpp
//=============================================================================


FL_TEST_CASE("StringHolder - Capacity off-by-one bugs") {
    // These tests are designed to expose the bugs where mCapacity is set to mLength
    // instead of mLength + 1 in StringHolder constructors

    FL_SUBCASE("StringHolder(fl::size length) capacity bug") {
        // This constructor should allocate length+1 bytes and set mCapacity = length+1
        // But it incorrectly sets mCapacity = mLength (missing the +1 for null terminator)

        // Create a string that will use StringHolder(fl::size length) constructor
        // This happens when we create a string with a specific length
        fl::string s1("x");  // Short string, inline storage

        // Now force it to grow beyond inline storage
        // This will trigger StringHolder allocation
        fl::size target_size = FASTLED_STR_INLINED_SIZE + 10;
        for (fl::size i = 1; i < target_size; ++i) {
            s1.append("x");
        }

        FL_CHECK(s1.size() == target_size);
        FL_CHECK(s1.capacity() >= target_size);  // Should be >= target_size + 1 for null terminator

        // The bug manifests when we try to append more data
        // With incorrect capacity, buffer overruns can occur
        s1.append("y");
        FL_CHECK(s1.size() == target_size + 1);
        FL_CHECK(s1[target_size] == 'y');
        FL_CHECK(s1.c_str()[target_size + 1] == '\0');  // Null terminator should be present
    }

    FL_SUBCASE("StringHolder(const char*, fl::size) capacity bug") {
        // This constructor has the same bug: mCapacity = mLength instead of mLength + 1

        // Create a long string that will trigger heap allocation
        fl::string long_str(FASTLED_STR_INLINED_SIZE + 20, 'a');
        fl::string s(long_str.c_str());

        FL_CHECK(s.size() == long_str.length());

        // Verify capacity is correct (should include null terminator)
        // Bug: capacity will be equal to length, missing +1 for null terminator

        // This might not catch the bug directly, but the next operation will
        FL_CHECK(s.capacity() >= long_str.length());

        // Try to append - this can cause buffer issues with wrong capacity
        s.append("b");
        FL_CHECK(s.size() == long_str.length() + 1);
        FL_CHECK(s[long_str.length()] == 'b');

        // Verify null termination is intact
        FL_CHECK(s.c_str()[s.size()] == '\0');
    }

    FL_SUBCASE("StringHolder::grow() fallback path capacity bug") {
        // In grow(), when realloc fails and malloc is used as fallback,
        // mCapacity = mLength is used instead of mLength + 1

        // We can't easily force realloc to fail, but we can test the normal path
        // and document that line 79 in str.cpp has the same bug

        fl::string s("Start");

        // Grow the string multiple times
        // Note: "_extra_data_to_force_growth" is 27 characters
        for (int i = 0; i < 10; ++i) {
            fl::size old_size = s.size();
            s.append("_extra_data_to_force_growth");
            FL_CHECK(s.size() == old_size + 27);
        }

        // Verify final state
        FL_CHECK(s.size() == 5 + (10 * 27));
        FL_CHECK(s.capacity() >= s.size());
        FL_CHECK(s.c_str()[s.size()] == '\0');
    }

    FL_SUBCASE("Copy with length exactly at inline boundary") {
        // Test strings that are exactly at the boundary between inline and heap storage
        // This is where off-by-one errors are most likely to manifest

        fl::size boundary = FASTLED_STR_INLINED_SIZE - 1;
        fl::string boundary_str(boundary, 'b');

        fl::string s1(boundary_str.c_str());
        FL_CHECK(s1.size() == boundary);

        // This should still fit in inline storage (boundary + 1 for null terminator <= SIZE)
        // Now push it just over the boundary
        s1.append("X");
        FL_CHECK(s1.size() == boundary + 1);

        // At this point, if SIZE = 64 and boundary = 63, then:
        // - boundary + 1 = 64 characters
        // - We need 65 bytes total (64 + 1 for null terminator)
        // - This should trigger heap allocation

        // Verify we can still access and modify the string
        s1.append("Y");
        FL_CHECK(s1.size() == boundary + 2);
        FL_CHECK(s1[boundary] == 'X');
        FL_CHECK(s1[boundary + 1] == 'Y');
    }

    FL_SUBCASE("Null terminator preservation after operations") {
        // Verify that null terminators are always correctly placed

        fl::string s1("Hello");
        FL_CHECK(s1.c_str()[5] == '\0');
        FL_CHECK(fl::strlen(s1.c_str()) == 5);

        s1.append(" World");
        FL_CHECK(s1.c_str()[11] == '\0');
        FL_CHECK(fl::strlen(s1.c_str()) == 11);

        // Force heap allocation
        fl::string long_append(FASTLED_STR_INLINED_SIZE, 'x');
        s1.append(long_append.c_str());
        FL_CHECK(s1.c_str()[s1.size()] == '\0');
        FL_CHECK(fl::strlen(s1.c_str()) == s1.size());
    }

    FL_SUBCASE("Capacity after copy operations") {
        // Test that capacity is correctly maintained during copy-on-write operations

        fl::string long_str(FASTLED_STR_INLINED_SIZE + 50, 'c');
        fl::string s1(long_str.c_str());
        fl::string s2 = s1;  // Copy (copy-on-write)

        // Both should report same size
        FL_CHECK(s1.size() == s2.size());
        FL_CHECK(s1.size() == long_str.length());

        // Modify s2 to trigger copy-on-write
        s2.append("_modified");

        // s1 should be unchanged
        FL_CHECK(s1.size() == long_str.length());

        // s2 should have grown
        FL_CHECK(s2.size() == long_str.length() + 9);

        // Both should maintain proper null termination
        FL_CHECK(s1.c_str()[s1.size()] == '\0');
        FL_CHECK(s2.c_str()[s2.size()] == '\0');
        FL_CHECK(fl::strlen(s1.c_str()) == s1.size());
        FL_CHECK(fl::strlen(s2.c_str()) == s2.size());
    }
}

FL_TEST_CASE("StringHolder - hasCapacity checks") {
    // Test the hasCapacity() method which relies on mCapacity being correct

    FL_SUBCASE("Reserve and capacity tracking") {
        fl::string s;

        // Start with empty string
        FL_CHECK(s.empty());

        // Reserve space
        s.reserve(100);
        FL_CHECK(s.capacity() >= 100);

        // Add content up to reserved capacity
        for (fl::size i = 0; i < 50; ++i) {
            s.append("a");
        }
        FL_CHECK(s.size() == 50);

        // Capacity should accommodate null terminator
        // If capacity was set to size (bug), then we'd have buffer issues
        FL_CHECK(s.capacity() >= 50);

        // Continue appending
        for (fl::size i = 0; i < 50; ++i) {
            s.append("b");
        }
        FL_CHECK(s.size() == 100);

        // Verify null termination
        FL_CHECK(s.c_str()[100] == '\0');
        FL_CHECK(fl::strlen(s.c_str()) == 100);
    }

    FL_SUBCASE("Write operations and capacity") {
        fl::string s;

        // Use write() method which checks capacity
        const char* data1 = "First chunk of data";
        s.write(data1, fl::strlen(data1));
        FL_CHECK(s.size() == fl::strlen(data1));
        FL_CHECK(fl::strcmp(s.c_str(), data1) == 0);

        // Write more data
        const char* data2 = " and second chunk";
        s.write(data2, fl::strlen(data2));

        fl::size expected_size = fl::strlen(data1) + fl::strlen(data2);
        FL_CHECK(s.size() == expected_size);
        FL_CHECK(s.c_str()[expected_size] == '\0');

        // Force heap allocation by writing a large amount
        fl::size large_size = FASTLED_STR_INLINED_SIZE + 100;
        for (fl::size i = s.size(); i < large_size; ++i) {
            s.write('x');
        }

        FL_CHECK(s.size() == large_size);
        FL_CHECK(s.c_str()[large_size] == '\0');
    }
}

FL_TEST_CASE("StringHolder - Edge cases exposing capacity bugs") {
    FL_SUBCASE("Exact boundary conditions") {
        // Test strings of length 0, 1, SIZE-1, SIZE, SIZE+1

        // Length 0
        fl::string s0;
        FL_CHECK(s0.size() == 0);
        FL_CHECK(s0.c_str()[0] == '\0');

        // Length 1
        fl::string s1("a");
        FL_CHECK(s1.size() == 1);
        FL_CHECK(s1.c_str()[1] == '\0');
        FL_CHECK(fl::strlen(s1.c_str()) == 1);

        // Length SIZE-1 (should fit inline with null terminator)
        fl::string str_size_minus_1(FASTLED_STR_INLINED_SIZE - 1, 'm');
        fl::string s_sm1(str_size_minus_1.c_str());
        FL_CHECK(s_sm1.size() == FASTLED_STR_INLINED_SIZE - 1);
        FL_CHECK(s_sm1.c_str()[FASTLED_STR_INLINED_SIZE - 1] == '\0');

        // Length SIZE (exactly at boundary, needs heap)
        fl::string str_size(FASTLED_STR_INLINED_SIZE, 's');
        fl::string s_s(str_size.c_str());
        FL_CHECK(s_s.size() == FASTLED_STR_INLINED_SIZE);
        FL_CHECK(s_s.c_str()[FASTLED_STR_INLINED_SIZE] == '\0');
        FL_CHECK(fl::strlen(s_s.c_str()) == FASTLED_STR_INLINED_SIZE);

        // Length SIZE+1
        fl::string str_size_plus_1(FASTLED_STR_INLINED_SIZE + 1, 'p');
        fl::string s_sp1(str_size_plus_1.c_str());
        FL_CHECK(s_sp1.size() == FASTLED_STR_INLINED_SIZE + 1);
        FL_CHECK(s_sp1.c_str()[FASTLED_STR_INLINED_SIZE + 1] == '\0');
        FL_CHECK(fl::strlen(s_sp1.c_str()) == FASTLED_STR_INLINED_SIZE + 1);
    }

    FL_SUBCASE("Multiple append operations at boundaries") {
        fl::string s;

        // Build up to exactly SIZE-1
        for (fl::size i = 0; i < FASTLED_STR_INLINED_SIZE - 1; ++i) {
            s.append("a");
        }
        FL_CHECK(s.size() == FASTLED_STR_INLINED_SIZE - 1);

        // One more append pushes to exactly SIZE
        s.append("b");
        FL_CHECK(s.size() == FASTLED_STR_INLINED_SIZE);
        FL_CHECK(s.c_str()[FASTLED_STR_INLINED_SIZE] == '\0');

        // One more append forces heap allocation
        s.append("c");
        FL_CHECK(s.size() == FASTLED_STR_INLINED_SIZE + 1);
        FL_CHECK(s.c_str()[FASTLED_STR_INLINED_SIZE + 1] == '\0');

        // Verify content is correct
        FL_CHECK(s[FASTLED_STR_INLINED_SIZE - 1] == 'b');
        FL_CHECK(s[FASTLED_STR_INLINED_SIZE] == 'c');
    }

    FL_SUBCASE("Substr operations preserving null termination") {
        fl::string original("This is a test string for substring operations");

        fl::string sub1 = original.substr(0, 4);  // "This"
        FL_CHECK(sub1.size() == 4);
        FL_CHECK(sub1.c_str()[4] == '\0');
        FL_CHECK(fl::strcmp(sub1.c_str(), "This") == 0);

        fl::string sub2 = original.substr(10, 4);  // "test"
        FL_CHECK(sub2.size() == 4);
        FL_CHECK(sub2.c_str()[4] == '\0');
        FL_CHECK(fl::strcmp(sub2.c_str(), "test") == 0);

        fl::string sub3 = original.substr(original.size() - 10);  // "operations"
        FL_CHECK(sub3.size() == 10);
        FL_CHECK(sub3.c_str()[10] == '\0');
        FL_CHECK(fl::strcmp(sub3.c_str(), "operations") == 0);
    }
}

FL_TEST_CASE("StringHolder - Memory safety with incorrect capacity") {
    // These tests attempt to expose memory corruption that would occur
    // if capacity is set incorrectly (missing +1 for null terminator)

    FL_SUBCASE("Rapid growth and access patterns") {
        fl::string s("initial");

        // Grow in various increments
        s.append("_1234567890");
        FL_CHECK(fl::strlen(s.c_str()) == s.size());

        s.append("_abcdefghijklmnopqrstuvwxyz");
        FL_CHECK(fl::strlen(s.c_str()) == s.size());

        // Force transition from inline to heap multiple times
        s.clear();
        s = "short";
        FL_CHECK(fl::strlen(s.c_str()) == 5);

        fl::string long_data(FASTLED_STR_INLINED_SIZE * 2, 'L');
        s = long_data.c_str();
        FL_CHECK(fl::strlen(s.c_str()) == long_data.length());

        s.clear();
        s = "tiny";
        FL_CHECK(fl::strlen(s.c_str()) == 4);
    }

    FL_SUBCASE("Copy and modify patterns") {
        fl::string base(FASTLED_STR_INLINED_SIZE + 10, 'B');
        fl::string s1(base.c_str());

        // Create multiple copies
        fl::string s2 = s1;
        fl::string s3 = s1;
        fl::string s4 = s1;

        // Modify each copy differently
        s2.append("_s2");
        s3.append("_s3");
        s4.append("_s4");

        // All should maintain null termination
        FL_CHECK(fl::strlen(s1.c_str()) == s1.size());
        FL_CHECK(fl::strlen(s2.c_str()) == s2.size());
        FL_CHECK(fl::strlen(s3.c_str()) == s3.size());
        FL_CHECK(fl::strlen(s4.c_str()) == s4.size());

        // Original should be unchanged
        FL_CHECK(s1.size() == base.length());

        // Copies should have grown
        FL_CHECK(s2.size() == base.length() + 3);
        FL_CHECK(s3.size() == base.length() + 3);
        FL_CHECK(s4.size() == base.length() + 3);
    }

    FL_SUBCASE("Insert operations with capacity constraints") {
        fl::string s("Hello World");

        // Insert in the middle
        s.insert(5, " Beautiful");
        FL_CHECK(fl::strlen(s.c_str()) == s.size());
        FL_CHECK(fl::strcmp(s.c_str(), "Hello Beautiful World") == 0);

        // Insert at the beginning
        s.insert(0, ">> ");
        FL_CHECK(fl::strlen(s.c_str()) == s.size());

        // Insert at the end
        s.insert(s.size(), " <<");
        FL_CHECK(fl::strlen(s.c_str()) == s.size());

        // Verify null termination throughout
        FL_CHECK(s.c_str()[s.size()] == '\0');
    }
}


} // FL_TEST_FILE
