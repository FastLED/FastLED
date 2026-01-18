
// TDD Tests for Type-Strict JSON-RPC Engine
// These tests define the expected behavior BEFORE implementation

#include "fl/json.h"
#include "fl/typed_rpc.h"

#include "doctest.h"

#if FASTLED_ENABLE_JSON

using namespace fl;

// Helper to extract tuple and result from JsonArgConverter output (C++11 compatible)
template<typename Tuple>
struct ConversionOutput {
    Tuple tuple;
    TypeConversionResult result;
};

template<typename Signature>
ConversionOutput<typename JsonArgConverter<Signature>::args_tuple> convertArgs(const Json& args) {
    ConversionOutput<typename JsonArgConverter<Signature>::args_tuple> output;
    fl::tuple<typename JsonArgConverter<Signature>::args_tuple, TypeConversionResult> conv =
        JsonArgConverter<Signature>::convert(args);
    output.tuple = fl::get<0>(conv);
    output.result = fl::get<1>(conv);
    return output;
}

// =============================================================================
// TEST SUITE: TypeConversionResult - Warning/Error System
// =============================================================================

TEST_CASE("TypeConversionResult - Basic Structure") {
    SUBCASE("Success result has no warnings or errors") {
        TypeConversionResult result = TypeConversionResult::success();
        CHECK(result.ok());
        CHECK_FALSE(result.hasWarning());
        CHECK_FALSE(result.hasError());
        CHECK(result.warnings().empty());
        CHECK(result.errorMessage().empty());
    }

    SUBCASE("Warning result indicates type promotion") {
        TypeConversionResult result = TypeConversionResult::warning("float 3.14 truncated to int 3");
        CHECK(result.ok());  // Warnings don't prevent success
        CHECK(result.hasWarning());
        CHECK_FALSE(result.hasError());
        CHECK_EQ(result.warnings().size(), 1);
        CHECK_EQ(result.warnings()[0], "float 3.14 truncated to int 3");
    }

    SUBCASE("Error result indicates critical mismatch") {
        TypeConversionResult result = TypeConversionResult::error("cannot convert object to int");
        CHECK_FALSE(result.ok());
        CHECK_FALSE(result.hasWarning());
        CHECK(result.hasError());
        CHECK_EQ(result.errorMessage(), "cannot convert object to int");
    }

    SUBCASE("Multiple warnings can be accumulated") {
        TypeConversionResult result = TypeConversionResult::success();
        result.addWarning("arg 0: string '123' converted to int");
        result.addWarning("arg 1: float 2.5 truncated to int 2");
        CHECK(result.ok());
        CHECK(result.hasWarning());
        CHECK_EQ(result.warnings().size(), 2);
    }
}

// =============================================================================
// TEST SUITE: JsonArgConverter - Type Extraction from fl::function
// =============================================================================

TEST_CASE("JsonArgConverter - Extract types from function signature") {
    SUBCASE("void() - no arguments") {
        using Converter = JsonArgConverter<void()>;
        CHECK_EQ(Converter::argCount(), 0);
    }

    SUBCASE("void(int) - single int argument") {
        using Converter = JsonArgConverter<void(int)>;
        CHECK_EQ(Converter::argCount(), 1);
    }

    SUBCASE("void(int, float, string) - multiple arguments") {
        using Converter = JsonArgConverter<void(int, float, fl::string)>;
        CHECK_EQ(Converter::argCount(), 3);
    }

    SUBCASE("int(float) - with return type") {
        using Converter = JsonArgConverter<int(float)>;
        CHECK_EQ(Converter::argCount(), 1);
    }
}

// =============================================================================
// TEST SUITE: JSON to Typed Args Conversion - Strict Type Matching
// =============================================================================

TEST_CASE("JsonArgConverter - Exact type matches (no warnings)") {
    SUBCASE("int argument from JSON integer") {
        Json args = Json::parse("[42]");
        REQUIRE(args.is_array());

        ConversionOutput<fl::tuple<int>> conv = convertArgs<void(int)>(args);
        CHECK(conv.result.ok());
        CHECK_FALSE(conv.result.hasWarning());
        CHECK_EQ(fl::get<0>(conv.tuple), 42);
    }

    SUBCASE("float argument from JSON number") {
        Json args = Json::parse("[3.14]");
        REQUIRE(args.is_array());

        ConversionOutput<fl::tuple<float>> conv = convertArgs<void(float)>(args);
        CHECK(conv.result.ok());
        CHECK_FALSE(conv.result.hasWarning());
        float val = fl::get<0>(conv.tuple);
        CHECK(val > 3.13f);
        CHECK(val < 3.15f);
    }

    SUBCASE("string argument from JSON string") {
        Json args = Json::parse(R"(["hello"])");
        REQUIRE(args.is_array());

        ConversionOutput<fl::tuple<fl::string>> conv = convertArgs<void(fl::string)>(args);
        CHECK(conv.result.ok());
        CHECK_FALSE(conv.result.hasWarning());
        CHECK_EQ(fl::get<0>(conv.tuple), "hello");
    }

    SUBCASE("bool argument from JSON boolean") {
        Json args = Json::parse("[true]");
        REQUIRE(args.is_array());

        ConversionOutput<fl::tuple<bool>> conv = convertArgs<void(bool)>(args);
        CHECK(conv.result.ok());
        CHECK_FALSE(conv.result.hasWarning());
        CHECK_EQ(fl::get<0>(conv.tuple), true);
    }

    SUBCASE("multiple arguments of same type") {
        Json args = Json::parse("[1, 2, 3]");
        REQUIRE(args.is_array());

        ConversionOutput<fl::tuple<int, int, int>> conv = convertArgs<void(int, int, int)>(args);
        CHECK(conv.result.ok());
        CHECK_FALSE(conv.result.hasWarning());
        CHECK_EQ(fl::get<0>(conv.tuple), 1);
        CHECK_EQ(fl::get<1>(conv.tuple), 2);
        CHECK_EQ(fl::get<2>(conv.tuple), 3);
    }

    SUBCASE("multiple arguments of different types") {
        Json args = Json::parse(R"([42, 3.14, "test", true])");
        REQUIRE(args.is_array());

        ConversionOutput<fl::tuple<int, float, fl::string, bool>> conv =
            convertArgs<void(int, float, fl::string, bool)>(args);
        CHECK(conv.result.ok());
        CHECK_FALSE(conv.result.hasWarning());
        CHECK_EQ(fl::get<0>(conv.tuple), 42);
        float val = fl::get<1>(conv.tuple);
        CHECK(val > 3.13f);
        CHECK(val < 3.15f);
        CHECK_EQ(fl::get<2>(conv.tuple), "test");
        CHECK_EQ(fl::get<3>(conv.tuple), true);
    }
}

TEST_CASE("JsonArgConverter - Type promotions with warnings") {
    SUBCASE("float to int - truncation warning") {
        Json args = Json::parse("[3.7]");
        REQUIRE(args.is_array());

        ConversionOutput<fl::tuple<int>> conv = convertArgs<void(int)>(args);
        CHECK(conv.result.ok());  // Conversion succeeds
        CHECK(conv.result.hasWarning());  // But with warning
        CHECK_EQ(fl::get<0>(conv.tuple), 3);  // Truncated value
        CHECK(conv.result.warnings()[0].find("truncat") != fl::string::npos);
    }

    SUBCASE("int to float - precision warning for large values") {
        // Large integers may lose precision when converted to float
        Json args = Json::parse("[16777217]");  // 2^24 + 1, beyond float precision
        REQUIRE(args.is_array());

        ConversionOutput<fl::tuple<float>> conv = convertArgs<void(float)>(args);
        CHECK(conv.result.ok());
        // May or may not warn depending on implementation
    }

    SUBCASE("string '123' to int - parse warning") {
        Json args = Json::parse(R"(["123"])");
        REQUIRE(args.is_array());

        ConversionOutput<fl::tuple<int>> conv = convertArgs<void(int)>(args);
        CHECK(conv.result.ok());  // Conversion succeeds
        CHECK(conv.result.hasWarning());  // With warning about string->int
        CHECK_EQ(fl::get<0>(conv.tuple), 123);
    }

    SUBCASE("string '3.14' to float - parse warning") {
        Json args = Json::parse(R"(["3.14"])");
        REQUIRE(args.is_array());

        ConversionOutput<fl::tuple<float>> conv = convertArgs<void(float)>(args);
        CHECK(conv.result.ok());
        CHECK(conv.result.hasWarning());
        float val = fl::get<0>(conv.tuple);
        CHECK(val > 3.13f);
        CHECK(val < 3.15f);
    }

    SUBCASE("bool to int - implicit conversion warning") {
        Json args = Json::parse("[true]");
        REQUIRE(args.is_array());

        ConversionOutput<fl::tuple<int>> conv = convertArgs<void(int)>(args);
        CHECK(conv.result.ok());
        CHECK(conv.result.hasWarning());
        CHECK_EQ(fl::get<0>(conv.tuple), 1);
    }

    SUBCASE("int to bool - implicit conversion warning") {
        Json args = Json::parse("[1]");
        REQUIRE(args.is_array());

        ConversionOutput<fl::tuple<bool>> conv = convertArgs<void(bool)>(args);
        CHECK(conv.result.ok());
        CHECK(conv.result.hasWarning());
        CHECK_EQ(fl::get<0>(conv.tuple), true);
    }

    SUBCASE("int 0 to bool - warning") {
        Json args = Json::parse("[0]");
        REQUIRE(args.is_array());

        ConversionOutput<fl::tuple<bool>> conv = convertArgs<void(bool)>(args);
        CHECK(conv.result.ok());
        CHECK(conv.result.hasWarning());
        CHECK_EQ(fl::get<0>(conv.tuple), false);
    }

    SUBCASE("string 'true' to bool - parse warning") {
        Json args = Json::parse(R"(["true"])");
        REQUIRE(args.is_array());

        ConversionOutput<fl::tuple<bool>> conv = convertArgs<void(bool)>(args);
        CHECK(conv.result.ok());
        CHECK(conv.result.hasWarning());
        CHECK_EQ(fl::get<0>(conv.tuple), true);
    }

    SUBCASE("int to string - stringify warning") {
        Json args = Json::parse("[42]");
        REQUIRE(args.is_array());

        ConversionOutput<fl::tuple<fl::string>> conv = convertArgs<void(fl::string)>(args);
        CHECK(conv.result.ok());
        CHECK(conv.result.hasWarning());
        CHECK_EQ(fl::get<0>(conv.tuple), "42");
    }
}

TEST_CASE("JsonArgConverter - Type errors (critical mismatches)") {
    SUBCASE("object to int - error") {
        Json args = Json::parse(R"([{"key": "value"}])");
        REQUIRE(args.is_array());

        ConversionOutput<fl::tuple<int>> conv = convertArgs<void(int)>(args);
        CHECK_FALSE(conv.result.ok());
        CHECK(conv.result.hasError());
        CHECK(conv.result.errorMessage().find("object") != fl::string::npos);
    }

    SUBCASE("array to int - error") {
        Json args = Json::parse("[[1, 2, 3]]");
        REQUIRE(args.is_array());

        ConversionOutput<fl::tuple<int>> conv = convertArgs<void(int)>(args);
        CHECK_FALSE(conv.result.ok());
        CHECK(conv.result.hasError());
    }

    SUBCASE("null to int - error") {
        Json args = Json::parse("[null]");
        REQUIRE(args.is_array());

        ConversionOutput<fl::tuple<int>> conv = convertArgs<void(int)>(args);
        CHECK_FALSE(conv.result.ok());
        CHECK(conv.result.hasError());
    }

    SUBCASE("unparseable string to int - error") {
        Json args = Json::parse(R"(["not_a_number"])");
        REQUIRE(args.is_array());

        ConversionOutput<fl::tuple<int>> conv = convertArgs<void(int)>(args);
        CHECK_FALSE(conv.result.ok());
        CHECK(conv.result.hasError());
    }

    SUBCASE("wrong argument count - too few") {
        Json args = Json::parse("[1]");
        REQUIRE(args.is_array());

        ConversionOutput<fl::tuple<int, int>> conv = convertArgs<void(int, int)>(args);
        CHECK_FALSE(conv.result.ok());
        CHECK(conv.result.hasError());
        CHECK(conv.result.errorMessage().find("argument") != fl::string::npos);
    }

    SUBCASE("wrong argument count - too many") {
        Json args = Json::parse("[1, 2, 3]");
        REQUIRE(args.is_array());

        ConversionOutput<fl::tuple<int>> conv = convertArgs<void(int)>(args);
        CHECK_FALSE(conv.result.ok());
        CHECK(conv.result.hasError());
    }

    SUBCASE("non-array args - error") {
        Json args = Json::parse("42");

        ConversionOutput<fl::tuple<int>> conv = convertArgs<void(int)>(args);
        CHECK_FALSE(conv.result.ok());
        CHECK(conv.result.hasError());
        CHECK(conv.result.errorMessage().find("array") != fl::string::npos);
    }
}

// =============================================================================
// TEST SUITE: TypedRpcBinding - Function Invocation with Type Safety
// =============================================================================

TEST_CASE("TypedRpcBinding - Invoke function with typed arguments") {
    SUBCASE("void function with no arguments") {
        bool called = false;
        fl::function<void()> fn = [&called]() { called = true; };

        TypedRpcBinding<void()> binding(fn);
        Json args = Json::parse("[]");

        TypeConversionResult result = binding.invoke(args);
        CHECK(result.ok());
        CHECK(called);
    }

    SUBCASE("void function with single int argument") {
        int received = 0;
        fl::function<void(int)> fn = [&received](int x) { received = x; };

        TypedRpcBinding<void(int)> binding(fn);
        Json args = Json::parse("[42]");

        TypeConversionResult result = binding.invoke(args);
        CHECK(result.ok());
        CHECK_EQ(received, 42);
    }

    SUBCASE("void function with multiple arguments") {
        int a = 0;
        float b = 0;
        fl::string c;
        fl::function<void(int, float, fl::string)> fn = [&](int x, float y, fl::string z) {
            a = x;
            b = y;
            c = z;
        };

        TypedRpcBinding<void(int, float, fl::string)> binding(fn);
        Json args = Json::parse(R"([1, 2.5, "test"])");

        TypeConversionResult result = binding.invoke(args);
        CHECK(result.ok());
        CHECK_EQ(a, 1);
        CHECK(b > 2.4f);
        CHECK(b < 2.6f);
        CHECK_EQ(c, "test");
    }

    SUBCASE("function with return value - int") {
        fl::function<int(int, int)> fn = [](int x, int y) -> int { return x + y; };

        TypedRpcBinding<int(int, int)> binding(fn);
        Json args = Json::parse("[10, 20]");

        fl::tuple<TypeConversionResult, Json> resultTuple = binding.invokeWithReturn(args);
        TypeConversionResult result = fl::get<0>(resultTuple);
        Json returnVal = fl::get<1>(resultTuple);
        CHECK(result.ok());
        CHECK_EQ(returnVal.as_int().value_or(0), 30);
    }

    SUBCASE("function with return value - string") {
        fl::function<fl::string(fl::string, int)> fn = [](fl::string prefix, int count) -> fl::string {
            fl::string result = prefix;
            for (int i = 0; i < count; i++) result += "!";
            return result;
        };

        TypedRpcBinding<fl::string(fl::string, int)> binding(fn);
        Json args = Json::parse(R"(["hello", 3])");

        fl::tuple<TypeConversionResult, Json> resultTuple = binding.invokeWithReturn(args);
        TypeConversionResult result = fl::get<0>(resultTuple);
        Json returnVal = fl::get<1>(resultTuple);
        CHECK(result.ok());
        CHECK(returnVal.is_string());
        CHECK_EQ(returnVal.as_string().value_or(""), "hello!!!");
    }

    SUBCASE("invocation with type promotion warning") {
        int received = 0;
        fl::function<void(int)> fn = [&received](int x) { received = x; };

        TypedRpcBinding<void(int)> binding(fn);
        Json args = Json::parse("[3.7]");  // float -> int

        TypeConversionResult result = binding.invoke(args);
        CHECK(result.ok());
        CHECK(result.hasWarning());
        CHECK_EQ(received, 3);
    }

    SUBCASE("invocation with type error") {
        fl::function<void(int)> fn = [](int x) { (void)x; };

        TypedRpcBinding<void(int)> binding(fn);
        Json args = Json::parse(R"([{"key": "value"}])");  // object -> int

        TypeConversionResult result = binding.invoke(args);
        CHECK_FALSE(result.ok());
        CHECK(result.hasError());
    }
}

// =============================================================================
// TEST SUITE: Edge Cases and Special Values
// =============================================================================

TEST_CASE("JsonArgConverter - Edge cases") {
    SUBCASE("empty argument list") {
        Json args = Json::parse("[]");
        ConversionOutput<fl::tuple<>> conv = convertArgs<void()>(args);
        CHECK(conv.result.ok());
    }

    SUBCASE("negative integers") {
        Json args = Json::parse("[-42]");
        ConversionOutput<fl::tuple<int>> conv = convertArgs<void(int)>(args);
        CHECK(conv.result.ok());
        CHECK_EQ(fl::get<0>(conv.tuple), -42);
    }

    SUBCASE("negative float") {
        Json args = Json::parse("[-3.14]");
        ConversionOutput<fl::tuple<float>> conv = convertArgs<void(float)>(args);
        CHECK(conv.result.ok());
        float val = fl::get<0>(conv.tuple);
        CHECK(val < -3.13f);
        CHECK(val > -3.15f);
    }

    SUBCASE("zero values") {
        Json args = Json::parse("[0, 0.0, false]");
        ConversionOutput<fl::tuple<int, float, bool>> conv = convertArgs<void(int, float, bool)>(args);
        CHECK(conv.result.ok());
        CHECK_EQ(fl::get<0>(conv.tuple), 0);
        float val = fl::get<1>(conv.tuple);
        CHECK(val > -0.001f);
        CHECK(val < 0.001f);
        CHECK_EQ(fl::get<2>(conv.tuple), false);
    }

    SUBCASE("empty string") {
        Json args = Json::parse(R"([""])");
        ConversionOutput<fl::tuple<fl::string>> conv = convertArgs<void(fl::string)>(args);
        CHECK(conv.result.ok());
        CHECK_EQ(fl::get<0>(conv.tuple), "");
    }

    SUBCASE("string with special characters") {
        Json args = Json::parse(R"(["hello\nworld\t!"])");
        ConversionOutput<fl::tuple<fl::string>> conv = convertArgs<void(fl::string)>(args);
        CHECK(conv.result.ok());
        CHECK_EQ(fl::get<0>(conv.tuple), "hello\nworld\t!");
    }

    SUBCASE("large integer") {
        Json args = Json::parse("[2147483647]");  // INT_MAX
        ConversionOutput<fl::tuple<int>> conv = convertArgs<void(int)>(args);
        CHECK(conv.result.ok());
        CHECK_EQ(fl::get<0>(conv.tuple), 2147483647);
    }

    SUBCASE("uint8_t argument") {
        Json args = Json::parse("[255]");
        ConversionOutput<fl::tuple<uint8_t>> conv = convertArgs<void(uint8_t)>(args);
        CHECK(conv.result.ok());
        CHECK_EQ(fl::get<0>(conv.tuple), 255);
    }

    SUBCASE("uint8_t overflow - warning or error") {
        Json args = Json::parse("[300]");  // > 255
        ConversionOutput<fl::tuple<uint8_t>> conv = convertArgs<void(uint8_t)>(args);
        // Could be warning (truncation) or error depending on implementation
        // At minimum, should not silently succeed
        if (conv.result.ok()) {
            CHECK(conv.result.hasWarning());
        }
    }
}

#endif // FASTLED_ENABLE_JSON

