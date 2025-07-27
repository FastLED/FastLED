#include "test.h"
#include "fl/json.h"

TEST_CASE("JSON array iterator with int16_t vector") {
    fl::vector<int16_t> data = {1, 2, 3, 4, 5};
    fl::JsonValue value(data);
    
    // Test iteration with int16_t
    int16_t expected = 1;
    for (auto it = value.begin_array<int16_t>(); it != value.end_array<int16_t>(); ++it) {
        CHECK_EQ(*it, expected);
        expected++;
    }
    
    // Test iteration with int32_t (should convert properly)
    int32_t expected32 = 1;
    for (auto it = value.begin_array<int32_t>(); it != value.end_array<int32_t>(); ++it) {
        CHECK_EQ(*it, expected32);
        expected32++;
    }
}

TEST_CASE("JSON array iterator with uint8_t vector") {
    fl::vector<uint8_t> data = {10, 20, 30, 40, 50};
    fl::JsonValue value(data);
    
    // Test iteration with uint8_t
    uint8_t expected = 10;
    for (auto it = value.begin_array<uint8_t>(); it != value.end_array<uint8_t>(); ++it) {
        CHECK_EQ(*it, expected);
        expected += 10;
    }
    
    // Test iteration with int32_t (should convert properly)
    int32_t expected32 = 10;
    for (auto it = value.begin_array<int32_t>(); it != value.end_array<int32_t>(); ++it) {
        CHECK_EQ(*it, expected32);
        expected32 += 10;
    }
}

TEST_CASE("JSON array iterator with float vector") {
    fl::vector<float> data = {1.1f, 2.2f, 3.3f, 4.4f, 5.5f};
    fl::JsonValue value(data);
    
    // Test iteration with float
    float expected = 1.1f;
    for (auto it = value.begin_array<float>(); it != value.end_array<float>(); ++it) {
        CHECK_CLOSE(*it, expected, 0.01f);
        expected += 1.1f;
    }
    
    // Test iteration with double (should convert properly)
    double expectedDouble = 1.1;
    for (auto it = value.begin_array<double>(); it != value.end_array<double>(); ++it) {
        CHECK_CLOSE(static_cast<float>(*it), static_cast<float>(expectedDouble), 0.01f);
        expectedDouble += 1.1;
    }
}

TEST_CASE("JSON class array iterator") {
    fl::Json json = fl::Json::array();
    json.push_back(fl::Json(1));
    json.push_back(fl::Json(2));
    json.push_back(fl::Json(3));
    
    // Test with Json class
    int expected = 1;
    for (auto it = json.begin_array<int>(); it != json.end_array<int>(); ++it) {
        CHECK_EQ(*it, expected);
        expected++;
    }
}