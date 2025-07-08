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

TEST_CASE("fl::cin_real global instance compiles") {
    // Test that we can use the fl::cin_real() function
    fl::istream_real* cin_real_ptr = &fl::cin_real();
    CHECK(cin_real_ptr != nullptr);
    
    // Test that we can call methods on cin_real()
    bool cin_real_good = fl::cin_real().good();
    bool cin_real_fail = fl::cin_real().fail();
    bool cin_real_eof = fl::cin_real().eof();
    fl::cin_real().clear();
    
    // Suppress unused variable warnings
    (void)cin_real_good;
    (void)cin_real_fail;
    (void)cin_real_eof;
    
    CHECK(true); // If we got here, cin_real() compiled and is accessible
}

TEST_CASE("fl::istream input operators compile") {
    fl::istream test_stream;
    
    // Test all input operator overloads compile
    Str str_val;
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
    
    fl::size size_val;
    test_stream >> size_val;
    (void)size_val;
    
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
    int32_t a, b, c;
    Str str1, str2;
    
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
    Str line;
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
    Str test_str;
    int32_t test_int;
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
    Str line;
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
    Str str;
    int32_t num;
    
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

#ifdef FASTLED_TESTING

// Helper class to mock input data for testing
class InputMocker {
private:
    Str data_;
    fl::size pos_;
    
public:
    InputMocker(const char* input_data) : data_(input_data), pos_(0) {}
    
    int available() {
        return (pos_ < data_.size()) ? (data_.size() - pos_) : 0;
    }
    
    int read() {
        if (pos_ < data_.size()) {
            return static_cast<int>(static_cast<unsigned char>(data_[pos_++]));
        }
        return -1;
    }
    
    void reset() {
        pos_ = 0;
    }
};

TEST_CASE("fl::istream handler injection test") {
    // Clear any existing handlers first
    fl::clear_io_handlers();
    
    SUBCASE("Test basic handler injection") {
        // Test that we can inject handlers and they work
        bool available_called = false;
        bool read_called = false;
        
        fl::inject_available_handler([&available_called]() { 
            available_called = true;
            return 5; 
        });
        fl::inject_read_handler([&read_called]() { 
            read_called = true;
            return 'H'; 
        });
        
        // Call the functions and verify handlers are called
        int avail = fl::available();
        int ch = fl::read();
        
        CHECK(available_called);
        CHECK(read_called);
        CHECK(avail == 5);
        CHECK(ch == 'H');
    }
    
    // Clean up handlers
    fl::clear_io_handlers();
}

TEST_CASE("fl::istream simple debug test") {
    // Clear any existing handlers first
    fl::clear_io_handlers();
    
    SUBCASE("Simple single word parsing") {
        InputMocker mocker("Hello");
        
        fl::inject_available_handler([&mocker]() { return mocker.available(); });
        fl::inject_read_handler([&mocker]() { return mocker.read(); });
        
        fl::istream test_stream;
        Str word;
        
        test_stream >> word;
        
        // Debug output
        FL_WARN("Parsed word: '" << word.c_str() << "'");
        FL_WARN("Word length: " << word.size());
        FL_WARN("Stream good: " << test_stream.good());
        FL_WARN("Stream fail: " << test_stream.fail());
        
        CHECK(test_stream.good());
        CHECK(word.size() == 5);
        CHECK(strcmp(word.c_str(), "Hello") == 0);
    }
    
    // Clean up handlers
    fl::clear_io_handlers();
}

TEST_CASE("fl::istream integer parsing with mock input") {
    // Clear any existing handlers first
    fl::clear_io_handlers();
    
    SUBCASE("Parse positive integer from 'Number: 10'") {
        InputMocker mocker("Number: 10");
        
        fl::inject_available_handler([&mocker]() { return mocker.available(); });
        fl::inject_read_handler([&mocker]() { return mocker.read(); });
        
        fl::istream test_stream;
        Str label;
        int32_t number = 0;
        
        test_stream >> label >> number;
        
        CHECK(test_stream.good());
        CHECK(label == "Number:");
        CHECK(number == 10);
    }
    
    SUBCASE("Parse negative integer from 'Value: -42'") {
        InputMocker mocker("Value: -42");
        
        fl::inject_available_handler([&mocker]() { return mocker.available(); });
        fl::inject_read_handler([&mocker]() { return mocker.read(); });
        
        fl::istream test_stream;
        Str label;
        int32_t number = 0;
        
        test_stream >> label >> number;
        
        CHECK(test_stream.good());
        CHECK(label == "Value:");
        CHECK(number == -42);
    }
    
    SUBCASE("Parse unsigned integer from 'count: 255'") {
        InputMocker mocker("count: 255");
        
        fl::inject_available_handler([&mocker]() { return mocker.available(); });
        fl::inject_read_handler([&mocker]() { return mocker.read(); });
        
        fl::istream test_stream;
        Str label;
        uint32_t number = 0;
        
        test_stream >> label >> number;
        
        CHECK(test_stream.good());
        CHECK(label == "count:");
        CHECK(number == 255);
    }
    
    SUBCASE("Parse int8_t from 'byte: 127'") {
        InputMocker mocker("byte: 127");
        
        fl::inject_available_handler([&mocker]() { return mocker.available(); });
        fl::inject_read_handler([&mocker]() { return mocker.read(); });
        
        fl::istream test_stream;
        Str label;
        int8_t number = 0;
        
        test_stream >> label >> number;
        
        CHECK(test_stream.good());
        CHECK(label == "byte:");
        CHECK(number == 127);
    }
    
    SUBCASE("Parse int16_t from 'short: -1000'") {
        InputMocker mocker("short: -1000");
        
        fl::inject_available_handler([&mocker]() { return mocker.available(); });
        fl::inject_read_handler([&mocker]() { return mocker.read(); });
        
        fl::istream test_stream;
        Str label;
        int16_t number = 0;
        
        test_stream >> label >> number;
        
        CHECK(test_stream.good());
        CHECK(label == "short:");
        CHECK(number == -1000);
    }
    
    // Clean up handlers
    fl::clear_io_handlers();
}

TEST_CASE("fl::istream float parsing with mock input") {
    // Clear any existing handlers first
    fl::clear_io_handlers();
    
    SUBCASE("Parse float from 'number: 1.0f'") {
        InputMocker mocker("number: 1.0f");
        
        fl::inject_available_handler([&mocker]() { return mocker.available(); });
        fl::inject_read_handler([&mocker]() { return mocker.read(); });
        
        fl::istream test_stream;
        Str label;
        float number = 0.0f;
        
        test_stream >> label >> number;
        
        CHECK(test_stream.good());
        CHECK(label == "number:");
        CHECK(number == doctest::Approx(1.0f));
    }
    
    SUBCASE("Parse float from 'pi: 3.14159'") {
        InputMocker mocker("pi: 3.14159");
        
        fl::inject_available_handler([&mocker]() { return mocker.available(); });
        fl::inject_read_handler([&mocker]() { return mocker.read(); });
        
        fl::istream test_stream;
        Str label;
        float number = 0.0f;
        
        test_stream >> label >> number;
        
        CHECK(test_stream.good());
        CHECK(label == "pi:");
        CHECK(number == doctest::Approx(3.14159f));
    }
    
    SUBCASE("Parse negative float from 'temp: -25.5'") {
        InputMocker mocker("temp: -25.5");
        
        fl::inject_available_handler([&mocker]() { return mocker.available(); });
        fl::inject_read_handler([&mocker]() { return mocker.read(); });
        
        fl::istream test_stream;
        Str label;
        float number = 0.0f;
        
        test_stream >> label >> number;
        
        CHECK(test_stream.good());
        CHECK(label == "temp:");
        CHECK(number == doctest::Approx(-25.5f));
    }
    
    SUBCASE("Parse double from 'precision: 123.456789'") {
        InputMocker mocker("precision: 123.456789");
        
        fl::inject_available_handler([&mocker]() { return mocker.available(); });
        fl::inject_read_handler([&mocker]() { return mocker.read(); });
        
        fl::istream test_stream;
        Str label;
        double number = 0.0;
        
        test_stream >> label >> number;
        
        CHECK(test_stream.good());
        CHECK(label == "precision:");
        CHECK(number == doctest::Approx(123.456789));
    }
    
    // Clean up handlers
    fl::clear_io_handlers();
}

TEST_CASE("fl::istream string and character parsing with mock input") {
    // Clear any existing handlers first
    fl::clear_io_handlers();
    
    SUBCASE("Parse string from 'name: FastLED'") {
        InputMocker mocker("name: FastLED");
        
        fl::inject_available_handler([&mocker]() { return mocker.available(); });
        fl::inject_read_handler([&mocker]() { return mocker.read(); });
        
        fl::istream test_stream;
        Str label, value;
        
        test_stream >> label >> value;
        
        CHECK(test_stream.good());
        CHECK(label == "name:");
        CHECK(value == "FastLED");
    }
    
    SUBCASE("Parse character from 'letter: A'") {
        InputMocker mocker("letter: A");
        
        fl::inject_available_handler([&mocker]() { return mocker.available(); });
        fl::inject_read_handler([&mocker]() { return mocker.read(); });
        
        fl::istream test_stream;
        Str label;
        char ch = 0;
        
        test_stream >> label >> ch;
        
        CHECK(test_stream.good());
        CHECK(label == "letter:");
        CHECK(ch == 'A');
    }
    
    SUBCASE("Parse multiple words with spaces") {
        InputMocker mocker("Hello World Test 42");
        
        fl::inject_available_handler([&mocker]() { return mocker.available(); });
        fl::inject_read_handler([&mocker]() { return mocker.read(); });
        
        fl::istream test_stream;
        Str word1, word2, word3;
        int32_t number = 0;
        
        test_stream >> word1 >> word2 >> word3 >> number;
        
        CHECK(test_stream.good());
        CHECK(word1 == "Hello");
        CHECK(word2 == "World");
        CHECK(word3 == "Test");
        CHECK(number == 42);
    }
    
    // Clean up handlers
    fl::clear_io_handlers();
}

TEST_CASE("fl::istream mixed data type parsing") {
    // Clear any existing handlers first
    fl::clear_io_handlers();
    
    SUBCASE("Parse mixed types from 'LED strip: 144 brightness: 0.8 enabled: Y'") {
        InputMocker mocker("LED strip: 144 brightness: 0.8 enabled: Y");
        
        fl::inject_available_handler([&mocker]() { return mocker.available(); });
        fl::inject_read_handler([&mocker]() { return mocker.read(); });
        
        fl::istream test_stream;
        Str led_label, strip_label, bright_label, enabled_label;
        int32_t count = 0;
        float brightness = 0.0f;
        char enabled = 0;
        
        test_stream >> led_label >> strip_label >> count >> bright_label >> brightness >> enabled_label >> enabled;
        
        CHECK(test_stream.good());
        CHECK(led_label == "LED");
        CHECK(strip_label == "strip:");
        CHECK(count == 144);
        CHECK(bright_label == "brightness:");
        CHECK(brightness == doctest::Approx(0.8f));
        CHECK(enabled_label == "enabled:");
        CHECK(enabled == 'Y');
    }
    
    SUBCASE("Parse configuration data 'width: 32 height: 16 fps: 60.0'") {
        InputMocker mocker("width: 32 height: 16 fps: 60.0");
        
        fl::inject_available_handler([&mocker]() { return mocker.available(); });
        fl::inject_read_handler([&mocker]() { return mocker.read(); });
        
        fl::istream test_stream;
        Str width_label, height_label, fps_label;
        uint16_t width = 0, height = 0;
        float fps = 0.0f;
        
        test_stream >> width_label >> width >> height_label >> height >> fps_label >> fps;
        
        CHECK(test_stream.good());
        CHECK(width_label == "width:");
        CHECK(width == 32);
        CHECK(height_label == "height:");
        CHECK(height == 16);
        CHECK(fps_label == "fps:");
        CHECK(fps == doctest::Approx(60.0f));
    }
    
    // Clean up handlers
    fl::clear_io_handlers();
}

TEST_CASE("fl::istream error handling with mock input") {
    // Clear any existing handlers first
    fl::clear_io_handlers();
    
    SUBCASE("Invalid integer parsing") {
        InputMocker mocker("value: abc");
        
        fl::inject_available_handler([&mocker]() { return mocker.available(); });
        fl::inject_read_handler([&mocker]() { return mocker.read(); });
        
        fl::istream test_stream;
        Str label;
        int32_t number = 0;
        
        test_stream >> label >> number;
        
        CHECK(label == "value:");
        CHECK(test_stream.fail());
        CHECK_FALSE(test_stream.good());
    }
    
    SUBCASE("Integer overflow handling") {
        InputMocker mocker("big: 999999999999999999999");
        
        fl::inject_available_handler([&mocker]() { return mocker.available(); });
        fl::inject_read_handler([&mocker]() { return mocker.read(); });
        
        fl::istream test_stream;
        Str label;
        int32_t number = 0;
        
        test_stream >> label >> number;
        
        CHECK(label == "big:");
        CHECK(test_stream.fail());
        CHECK_FALSE(test_stream.good());
    }
    
    SUBCASE("Clear error state and continue") {
        InputMocker mocker("bad: abc good: 123");
        
        fl::inject_available_handler([&mocker]() { return mocker.available(); });
        fl::inject_read_handler([&mocker]() { return mocker.read(); });
        
        fl::istream test_stream;
        Str label1, label2;
        int32_t number1 = 0, number2 = 0;
        
        // First read should fail
        test_stream >> label1 >> number1;
        CHECK(label1 == "bad:");
        CHECK(test_stream.fail());
        
        // Clear error state
        test_stream.clear();
        CHECK(test_stream.good());
        
        // Continue reading should work
        test_stream >> label2 >> number2;
        CHECK(test_stream.good());
        CHECK(label2 == "good:");
        CHECK(number2 == 123);
    }
    
    // Clean up handlers
    fl::clear_io_handlers();
}

TEST_CASE("fl::istream getline with mock input") {
    // Clear any existing handlers first
    fl::clear_io_handlers();
    
    SUBCASE("Read full line with getline") {
        InputMocker mocker("This is a complete line with spaces\nSecond line");
        
        fl::inject_available_handler([&mocker]() { return mocker.available(); });
        fl::inject_read_handler([&mocker]() { return mocker.read(); });
        
        fl::istream test_stream;
        Str line1, line2;
        
        test_stream.getline(line1);
        test_stream.getline(line2);
        
        CHECK(test_stream.good());
        CHECK(line1 == "This is a complete line with spaces");
        CHECK(line2 == "Second line");
    }
    
    // Clean up handlers
    fl::clear_io_handlers();
}

#endif // FASTLED_TESTING
