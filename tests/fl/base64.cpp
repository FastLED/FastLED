#include "test.h"
#include "fl/remote/rpc/base64.h"

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("base64 encode/decode roundtrip") {
    FL_SUBCASE("empty data") {
        fl::vector<fl::u8> empty;
        fl::string encoded = fl::base64_encode(empty);
        FL_CHECK(encoded.empty());

        fl::vector<fl::u8> decoded = fl::base64_decode(encoded);
        FL_CHECK(decoded.empty());
    }

    FL_SUBCASE("single byte") {
        fl::vector<fl::u8> data = {0x41}; // 'A'
        fl::string encoded = fl::base64_encode(data);
        FL_CHECK_EQ(encoded, fl::string("QQ=="));

        fl::vector<fl::u8> decoded = fl::base64_decode(encoded);
        FL_REQUIRE(decoded.size() == 1);
        FL_CHECK_EQ(decoded[0], 0x41);
    }

    FL_SUBCASE("two bytes") {
        fl::vector<fl::u8> data = {0x41, 0x42}; // 'AB'
        fl::string encoded = fl::base64_encode(data);
        FL_CHECK_EQ(encoded, fl::string("QUI="));

        fl::vector<fl::u8> decoded = fl::base64_decode(encoded);
        FL_REQUIRE(decoded.size() == 2);
        FL_CHECK_EQ(decoded[0], 0x41);
        FL_CHECK_EQ(decoded[1], 0x42);
    }

    FL_SUBCASE("three bytes - no padding") {
        fl::vector<fl::u8> data = {0x41, 0x42, 0x43}; // 'ABC'
        fl::string encoded = fl::base64_encode(data);
        FL_CHECK_EQ(encoded, fl::string("QUJD"));

        fl::vector<fl::u8> decoded = fl::base64_decode(encoded);
        FL_REQUIRE(decoded.size() == 3);
        FL_CHECK_EQ(decoded[0], 0x41);
        FL_CHECK_EQ(decoded[1], 0x42);
        FL_CHECK_EQ(decoded[2], 0x43);
    }

    FL_SUBCASE("known value - Hello World") {
        const fl::u8 hello[] = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
        fl::span<const fl::u8> data(hello, sizeof(hello));
        fl::string encoded = fl::base64_encode(data);
        FL_CHECK_EQ(encoded, fl::string("SGVsbG8gV29ybGQ="));

        fl::vector<fl::u8> decoded = fl::base64_decode(encoded);
        FL_REQUIRE(decoded.size() == sizeof(hello));
        for (fl::size i = 0; i < sizeof(hello); i++) {
            FL_CHECK_EQ(decoded[i], hello[i]);
        }
    }

    FL_SUBCASE("binary data roundtrip") {
        fl::vector<fl::u8> data;
        for (int i = 0; i < 256; i++) {
            data.push_back((fl::u8)i);
        }
        fl::string encoded = fl::base64_encode(data);
        fl::vector<fl::u8> decoded = fl::base64_decode(encoded);
        FL_REQUIRE(decoded.size() == 256);
        for (int i = 0; i < 256; i++) {
            FL_CHECK_EQ(decoded[i], (fl::u8)i);
        }
    }
}

FL_TEST_CASE("base64 decode error handling") {
    FL_SUBCASE("invalid length") {
        fl::vector<fl::u8> result = fl::base64_decode("ABC"); // not multiple of 4
        FL_CHECK(result.empty());
    }

    FL_SUBCASE("invalid characters") {
        fl::vector<fl::u8> result = fl::base64_decode("!!!!");
        FL_CHECK(result.empty());
    }
}

} // FL_TEST_FILE
