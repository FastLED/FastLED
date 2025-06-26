#include "fl/istream.h"
#include "test.h"
#include <cstring>

// Test that fl::cin and fl::istream compile without errors

TEST_CASE("fl::istream basic instantiation compiles") {
    // Test that we can create an istream instance
    fl::istream test_stream;
    
    // Test that global cin exists
    fl::istream* cin_ptr = &fl::cin;
    CHECK(cin_ptr != nullptr);
    
    // Test basic state methods compile
    bool good = test_stream.good();
    bool fail = test_stream.fail();
    bool eof = test_stream.eof();
    test_stream.clear();
    
    // Suppress unused variable warnings
    (void)good;
    (void)fail;
    (void)eof;
    
    CHECK(true); // If we got here, compilation succeeded
}

TEST_CASE("fl::istream input operators compile") {
    fl::istream test_stream;
    
    // Test all input operator overloads compile
    fl::string str_val;
    char char_val;
    int8_t int8_val;
    uint8_t uint8_val;
    int16_t int16_val;
    uint16_t uint16_val;
    int32_t int32_val;
    uint32_t uint32_val;
    float float_val;
    double double_val;
    
    // These should compile even though they won't read anything without input
    test_stream >> str_val;
    test_stream >> char_val;
    test_stream >> int8_val;
    test_stream >> uint8_val;
    test_stream >> int16_val;
    test_stream >> uint16_val;
    test_stream >> int32_val;
    test_stream >> uint32_val;
    test_stream >> float_val;
    test_stream >> double_val;
    
#if FASTLED_STRSTREAM_USES_SIZE_T
    size_t size_val;
    test_stream >> size_val;
    (void)size_val;
#endif
    
    // Suppress unused variable warnings
    (void)str_val;
    (void)char_val;
    (void)int8_val;
    (void)uint8_val;
    (void)int16_val;
    (void)uint16_val;
    (void)int32_val;
    (void)uint32_val;
    (void)float_val;
    (void)double_val;
    
    CHECK(true); // If we got here, all operators compiled
}

TEST_CASE("fl::istream chaining operations compile") {
    fl::istream test_stream;
    
    // Test that chaining input operations compiles
    int a, b, c;
    fl::string str1, str2;
    
    // This should compile (chain of >> operators)
    test_stream >> a >> b >> c;
    test_stream >> str1 >> str2;
    test_stream >> str1 >> a >> str2 >> b;
    
    // Suppress unused variable warnings
    (void)a;
    (void)b;
    (void)c;
    (void)str1;
    (void)str2;
    
    CHECK(true); // If we got here, chaining compiled
}

TEST_CASE("fl::istream additional methods compile") {
    fl::istream test_stream;
    
    // Test getline method compiles
    fl::string line;
    test_stream.getline(line);
    
    // Test get/peek/putback methods compile
    int ch = test_stream.get();
    int peek_ch = test_stream.peek();
    test_stream.putback('A');
    
    // Suppress unused variable warnings
    (void)line;
    (void)ch;
    (void)peek_ch;
    
    CHECK(true); // If we got here, all methods compiled
}

TEST_CASE("fl::cin global instance compiles") {
    // Test that we can use the global fl::cin instance
    fl::string test_str;
    int test_int;
    char test_char;
    
    // These operations should compile
    // (They won't read anything in the test environment, but should compile)
    fl::cin >> test_str;
    fl::cin >> test_int;
    fl::cin >> test_char;
    
    // Test chaining with global cin
    fl::cin >> test_str >> test_int >> test_char;
    
    // Test state checking
    bool cin_good = fl::cin.good();
    bool cin_fail = fl::cin.fail();
    bool cin_eof = fl::cin.eof();
    fl::cin.clear();
    
    // Test getline with global cin
    fl::string line;
    fl::cin.getline(line);
    
    // Suppress unused variable warnings
    (void)test_str;
    (void)test_int;
    (void)test_char;
    (void)cin_good;
    (void)cin_fail;
    (void)cin_eof;
    (void)line;
    
    CHECK(true); // If we got here, global cin compiled
}

TEST_CASE("fl::istream state management compiles") {
    fl::istream test_stream;
    
    // Test all state management methods
    CHECK_FALSE(test_stream.fail()); // Initially should not fail
    CHECK(test_stream.good()); // Initially should be good
    
    // Test that we can call clear to reset state
    test_stream.clear();
    CHECK_FALSE(test_stream.fail());
    CHECK(test_stream.good());
    
    // Test eof checking (should not be EOF initially when no input attempted)
    bool eof_result = test_stream.eof();
    (void)eof_result; // Suppress unused warning
    
    CHECK(true); // State management compiled and basic checks passed
}

TEST_CASE("fl::istream types match expected interfaces") {
    // Verify that istream has the expected interface similar to std::istream
    
    // Test return type of operators is istream& (for chaining)
    fl::istream test_stream;
    fl::string str;
    int num;
    
    // This tests that >> returns istream& by allowing chaining
    auto& result1 = (test_stream >> str);
    auto& result2 = (result1 >> num);
    
    // Test that the return is the same object (reference)
    CHECK(&result1 == &test_stream);
    CHECK(&result2 == &test_stream);
    
    // Test that getline returns istream&
    auto& result3 = test_stream.getline(str);
    CHECK(&result3 == &test_stream);
    
    // Test that putback returns istream&
    auto& result4 = test_stream.putback('X');
    CHECK(&result4 == &test_stream);
    
    // Suppress unused variable warnings
    (void)str;
    (void)num;
}
