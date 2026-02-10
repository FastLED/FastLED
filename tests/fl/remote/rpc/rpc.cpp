/// @file rpc.cpp
/// @brief Tests for RPC system - TypedRpc and RpcFactory

#include "fl/json.h"
#include "fl/remote/rpc/rpc.h"

#include "test.h"

#if FASTLED_ENABLE_JSON

using namespace fl;

// =============================================================================
// TEST SUITE: TypeConversionResult - Warning/Error System
// =============================================================================

FL_TEST_CASE("TypeConversionResult - Basic Structure") {
    FL_SUBCASE("Success result has no warnings or errors") {
        TypeConversionResult result = TypeConversionResult::success();
        FL_CHECK(result.ok());
        FL_CHECK_FALSE(result.hasWarning());
        FL_CHECK_FALSE(result.hasError());
        FL_CHECK(result.warnings().empty());
        FL_CHECK(result.errorMessage().empty());
    }

    FL_SUBCASE("Warning result indicates type promotion") {
        TypeConversionResult result = TypeConversionResult::warning("float 3.14 truncated to int 3");
        FL_CHECK(result.ok());  // Warnings don't prevent success
        FL_CHECK(result.hasWarning());
        FL_CHECK_FALSE(result.hasError());
        FL_CHECK_EQ(result.warnings().size(), 1);
        FL_CHECK_EQ(result.warnings()[0], "float 3.14 truncated to int 3");
    }

    FL_SUBCASE("Error result indicates critical mismatch") {
        TypeConversionResult result = TypeConversionResult::error("cannot convert object to int");
        FL_CHECK_FALSE(result.ok());
        FL_CHECK_FALSE(result.hasWarning());
        FL_CHECK(result.hasError());
        FL_CHECK_EQ(result.errorMessage(), "cannot convert object to int");
    }

    FL_SUBCASE("Multiple warnings can be accumulated") {
        TypeConversionResult result = TypeConversionResult::success();
        result.addWarning("arg 0: string '123' converted to int");
        result.addWarning("arg 1: float 2.5 truncated to int 2");
        FL_CHECK(result.ok());
        FL_CHECK(result.hasWarning());
        FL_CHECK_EQ(result.warnings().size(), 2);
    }
}

// =============================================================================
// TEST SUITE: JsonArgConverter - Type Extraction from fl::function
// =============================================================================

FL_TEST_CASE("JsonArgConverter - Extract types from function signature") {
    FL_SUBCASE("void() - no arguments") {
        using Converter = JsonArgConverter<void()>;
        FL_CHECK_EQ(Converter::argCount(), 0);
    }

    FL_SUBCASE("void(int) - single int argument") {
        using Converter = JsonArgConverter<void(int)>;
        FL_CHECK_EQ(Converter::argCount(), 1);
    }

    FL_SUBCASE("void(int, float, string) - multiple arguments") {
        using Converter = JsonArgConverter<void(int, float, fl::string)>;
        FL_CHECK_EQ(Converter::argCount(), 3);
    }

    FL_SUBCASE("int(float) - with return type") {
        using Converter = JsonArgConverter<int(float)>;
        FL_CHECK_EQ(Converter::argCount(), 1);
    }
}

// =============================================================================
// TEST SUITE: JSON to Typed Args Conversion - Strict Type Matching
// =============================================================================

FL_TEST_CASE("JsonArgConverter - Exact type matches (no warnings)") {
    FL_SUBCASE("int argument from JSON integer") {
        Json args = Json::parse("[42]");
        auto conv = JsonArgConverter<void(int)>::convert(args);
        auto argsTuple = fl::get<0>(conv);
        auto result = fl::get<1>(conv);
        FL_CHECK(result.ok());
        FL_CHECK_FALSE(result.hasWarning());
        FL_CHECK_EQ(fl::get<0>(argsTuple), 42);
    }

    FL_SUBCASE("float argument from JSON number") {
        Json args = Json::parse("[3.14]");
        auto conv = JsonArgConverter<void(float)>::convert(args);
        auto argsTuple = fl::get<0>(conv);
        auto result = fl::get<1>(conv);
        FL_CHECK(result.ok());
        FL_CHECK_FALSE(result.hasWarning());
        float val = fl::get<0>(argsTuple);
        FL_CHECK(val > 3.13f);
        FL_CHECK(val < 3.15f);
    }

    FL_SUBCASE("string argument from JSON string") {
        Json args = Json::parse(R"(["hello"])");
        auto conv = JsonArgConverter<void(fl::string)>::convert(args);
        auto argsTuple = fl::get<0>(conv);
        auto result = fl::get<1>(conv);
        FL_CHECK(result.ok());
        FL_CHECK_FALSE(result.hasWarning());
        FL_CHECK_EQ(fl::get<0>(argsTuple), "hello");
    }

    FL_SUBCASE("bool argument from JSON boolean") {
        Json args = Json::parse("[true]");
        auto conv = JsonArgConverter<void(bool)>::convert(args);
        auto argsTuple = fl::get<0>(conv);
        auto result = fl::get<1>(conv);
        FL_CHECK(result.ok());
        FL_CHECK_FALSE(result.hasWarning());
        FL_CHECK_EQ(fl::get<0>(argsTuple), true);
    }

    FL_SUBCASE("multiple arguments of same type") {
        Json args = Json::parse("[1, 2, 3]");
        auto conv = JsonArgConverter<void(int, int, int)>::convert(args);
        auto argsTuple = fl::get<0>(conv);
        auto result = fl::get<1>(conv);
        FL_CHECK(result.ok());
        FL_CHECK_FALSE(result.hasWarning());
        FL_CHECK_EQ(fl::get<0>(argsTuple), 1);
        FL_CHECK_EQ(fl::get<1>(argsTuple), 2);
        FL_CHECK_EQ(fl::get<2>(argsTuple), 3);
    }

    FL_SUBCASE("multiple arguments of different types") {
        Json args = Json::parse(R"([42, 3.14, "test", true])");
        auto conv = JsonArgConverter<void(int, float, fl::string, bool)>::convert(args);
        auto argsTuple = fl::get<0>(conv);
        auto result = fl::get<1>(conv);
        FL_CHECK(result.ok());
        FL_CHECK_FALSE(result.hasWarning());
        FL_CHECK_EQ(fl::get<0>(argsTuple), 42);
        float val = fl::get<1>(argsTuple);
        FL_CHECK(val > 3.13f);
        FL_CHECK(val < 3.15f);
        FL_CHECK_EQ(fl::get<2>(argsTuple), "test");
        FL_CHECK_EQ(fl::get<3>(argsTuple), true);
    }
}

FL_TEST_CASE("JsonArgConverter - Type promotions with warnings") {
    FL_SUBCASE("float to int - truncation warning") {
        Json args = Json::parse("[3.7]");
        auto conv = JsonArgConverter<void(int)>::convert(args);
        auto argsTuple = fl::get<0>(conv);
        auto result = fl::get<1>(conv);
        FL_CHECK(result.ok());
        FL_CHECK(result.hasWarning());
        FL_CHECK_EQ(fl::get<0>(argsTuple), 3);
        FL_CHECK(result.warnings()[0].find("truncat") != fl::string::npos);
    }

    FL_SUBCASE("int to float - precision warning for large values") {
        Json args = Json::parse("[16777217]");  // 2^24 + 1, beyond float precision
        auto conv = JsonArgConverter<void(float)>::convert(args);
        auto result = fl::get<1>(conv);
        FL_CHECK(result.ok());
        // May or may not warn depending on implementation
    }

    FL_SUBCASE("string '123' to int - parse warning") {
        Json args = Json::parse(R"(["123"])");
        auto conv = JsonArgConverter<void(int)>::convert(args);
        auto argsTuple = fl::get<0>(conv);
        auto result = fl::get<1>(conv);
        FL_CHECK(result.ok());
        FL_CHECK(result.hasWarning());
        FL_CHECK_EQ(fl::get<0>(argsTuple), 123);
    }

    FL_SUBCASE("string '3.14' to float - parse warning") {
        Json args = Json::parse(R"(["3.14"])");
        auto conv = JsonArgConverter<void(float)>::convert(args);
        auto argsTuple = fl::get<0>(conv);
        auto result = fl::get<1>(conv);
        FL_CHECK(result.ok());
        FL_CHECK(result.hasWarning());
        float val = fl::get<0>(argsTuple);
        FL_CHECK(val > 3.13f);
        FL_CHECK(val < 3.15f);
    }

    FL_SUBCASE("bool to int - implicit conversion warning") {
        Json args = Json::parse("[true]");
        auto conv = JsonArgConverter<void(int)>::convert(args);
        auto argsTuple = fl::get<0>(conv);
        auto result = fl::get<1>(conv);
        FL_CHECK(result.ok());
        FL_CHECK(result.hasWarning());
        FL_CHECK_EQ(fl::get<0>(argsTuple), 1);
    }

    FL_SUBCASE("int to bool - implicit conversion warning") {
        Json args = Json::parse("[1]");
        auto conv = JsonArgConverter<void(bool)>::convert(args);
        auto argsTuple = fl::get<0>(conv);
        auto result = fl::get<1>(conv);
        FL_CHECK(result.ok());
        FL_CHECK(result.hasWarning());
        FL_CHECK_EQ(fl::get<0>(argsTuple), true);
    }

    FL_SUBCASE("int 0 to bool - warning") {
        Json args = Json::parse("[0]");
        auto conv = JsonArgConverter<void(bool)>::convert(args);
        auto argsTuple = fl::get<0>(conv);
        auto result = fl::get<1>(conv);
        FL_CHECK(result.ok());
        FL_CHECK(result.hasWarning());
        FL_CHECK_EQ(fl::get<0>(argsTuple), false);
    }

    FL_SUBCASE("string 'true' to bool - parse warning") {
        Json args = Json::parse(R"(["true"])");
        auto conv = JsonArgConverter<void(bool)>::convert(args);
        auto argsTuple = fl::get<0>(conv);
        auto result = fl::get<1>(conv);
        FL_CHECK(result.ok());
        FL_CHECK(result.hasWarning());
        FL_CHECK_EQ(fl::get<0>(argsTuple), true);
    }

    FL_SUBCASE("int to string - stringify warning") {
        Json args = Json::parse("[42]");
        auto conv = JsonArgConverter<void(fl::string)>::convert(args);
        auto argsTuple = fl::get<0>(conv);
        auto result = fl::get<1>(conv);
        FL_CHECK(result.ok());
        FL_CHECK(result.hasWarning());
        FL_CHECK_EQ(fl::get<0>(argsTuple), "42");
    }
}

FL_TEST_CASE("JsonArgConverter - Type errors (critical mismatches)") {
    FL_SUBCASE("object to int - error") {
        Json args = Json::parse(R"([{"key": "value"}])");
        auto conv = JsonArgConverter<void(int)>::convert(args);
        auto result = fl::get<1>(conv);
        FL_CHECK_FALSE(result.ok());
        FL_CHECK(result.hasError());
        FL_CHECK(result.errorMessage().find("object") != fl::string::npos);
    }

    FL_SUBCASE("array to int - error") {
        Json args = Json::parse("[[1, 2, 3]]");
        auto conv = JsonArgConverter<void(int)>::convert(args);
        auto result = fl::get<1>(conv);
        FL_CHECK_FALSE(result.ok());
        FL_CHECK(result.hasError());
    }

    FL_SUBCASE("null to int - error") {
        Json args = Json::parse("[null]");
        auto conv = JsonArgConverter<void(int)>::convert(args);
        auto result = fl::get<1>(conv);
        FL_CHECK_FALSE(result.ok());
        FL_CHECK(result.hasError());
    }

    FL_SUBCASE("unparseable string to int - error") {
        Json args = Json::parse(R"(["not_a_number"])");
        auto conv = JsonArgConverter<void(int)>::convert(args);
        auto result = fl::get<1>(conv);
        FL_CHECK_FALSE(result.ok());
        FL_CHECK(result.hasError());
    }

    FL_SUBCASE("wrong argument count - too few") {
        Json args = Json::parse("[1]");
        auto conv = JsonArgConverter<void(int, int)>::convert(args);
        auto result = fl::get<1>(conv);
        FL_CHECK_FALSE(result.ok());
        FL_CHECK(result.hasError());
        FL_CHECK(result.errorMessage().find("argument") != fl::string::npos);
    }

    FL_SUBCASE("wrong argument count - too many") {
        Json args = Json::parse("[1, 2, 3]");
        auto conv = JsonArgConverter<void(int)>::convert(args);
        auto result = fl::get<1>(conv);
        FL_CHECK_FALSE(result.ok());
        FL_CHECK(result.hasError());
    }

    FL_SUBCASE("non-array args - error") {
        Json args = Json::parse("42");
        auto conv = JsonArgConverter<void(int)>::convert(args);
        auto result = fl::get<1>(conv);
        FL_CHECK_FALSE(result.ok());
        FL_CHECK(result.hasError());
        FL_CHECK(result.errorMessage().find("array") != fl::string::npos);
    }
}

// =============================================================================
// TEST SUITE: TypedRpcBinding - Function Invocation with Type Safety
// =============================================================================

FL_TEST_CASE("TypedRpcBinding - Invoke function with typed arguments") {
    FL_SUBCASE("void function with no arguments") {
        bool called = false;
        fl::function<void()> fn = [&called]() { called = true; };

        TypedRpcBinding<void()> binding(fn);
        Json args = Json::parse("[]");

        TypeConversionResult result = binding.invoke(args);
        FL_CHECK(result.ok());
        FL_CHECK(called);
    }

    FL_SUBCASE("void function with single int argument") {
        int received = 0;
        fl::function<void(int)> fn = [&received](int x) { received = x; };

        TypedRpcBinding<void(int)> binding(fn);
        Json args = Json::parse("[42]");

        TypeConversionResult result = binding.invoke(args);
        FL_CHECK(result.ok());
        FL_CHECK_EQ(received, 42);
    }

    FL_SUBCASE("void function with multiple arguments") {
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
        FL_CHECK(result.ok());
        FL_CHECK_EQ(a, 1);
        FL_CHECK(b > 2.4f);
        FL_CHECK(b < 2.6f);
        FL_CHECK_EQ(c, "test");
    }

    FL_SUBCASE("function with return value - int") {
        fl::function<int(int, int)> fn = [](int x, int y) -> int { return x + y; };

        TypedRpcBinding<int(int, int)> binding(fn);
        Json args = Json::parse("[10, 20]");

        auto resultTuple = binding.invokeWithReturn(args);
        auto result = fl::get<0>(resultTuple);
        auto returnVal = fl::get<1>(resultTuple);
        FL_CHECK(result.ok());
        FL_CHECK_EQ(returnVal.as_int().value_or(0), 30);
    }

    FL_SUBCASE("function with return value - string") {
        fl::function<fl::string(fl::string, int)> fn = [](fl::string prefix, int count) -> fl::string {
            fl::string result = prefix;
            for (int i = 0; i < count; i++) result += "!";
            return result;
        };

        TypedRpcBinding<fl::string(fl::string, int)> binding(fn);
        Json args = Json::parse(R"(["hello", 3])");

        auto resultTuple = binding.invokeWithReturn(args);
        auto result = fl::get<0>(resultTuple);
        auto returnVal = fl::get<1>(resultTuple);
        FL_CHECK(result.ok());
        FL_CHECK(returnVal.is_string());
        FL_CHECK_EQ(returnVal.as_string().value_or(""), "hello!!!");
    }

    FL_SUBCASE("invocation with type promotion warning") {
        int received = 0;
        fl::function<void(int)> fn = [&received](int x) { received = x; };

        TypedRpcBinding<void(int)> binding(fn);
        Json args = Json::parse("[3.7]");  // float -> int

        TypeConversionResult result = binding.invoke(args);
        FL_CHECK(result.ok());
        FL_CHECK(result.hasWarning());
        FL_CHECK_EQ(received, 3);
    }

    FL_SUBCASE("invocation with type error") {
        fl::function<void(int)> fn = [](int x) { (void)x; };

        TypedRpcBinding<void(int)> binding(fn);
        Json args = Json::parse(R"([{"key": "value"}])");  // object -> int

        TypeConversionResult result = binding.invoke(args);
        FL_CHECK_FALSE(result.ok());
        FL_CHECK(result.hasError());
    }
}

// =============================================================================
// TEST SUITE: Edge Cases and Special Values
// =============================================================================

FL_TEST_CASE("JsonArgConverter - Edge cases") {
    FL_SUBCASE("empty argument list") {
        Json args = Json::parse("[]");
        auto conv = JsonArgConverter<void()>::convert(args);
        auto result = fl::get<1>(conv);
        FL_CHECK(result.ok());
    }

    FL_SUBCASE("negative integers") {
        Json args = Json::parse("[-42]");
        auto conv = JsonArgConverter<void(int)>::convert(args);
        auto argsTuple = fl::get<0>(conv);
        auto result = fl::get<1>(conv);
        FL_CHECK(result.ok());
        FL_CHECK_EQ(fl::get<0>(argsTuple), -42);
    }

    FL_SUBCASE("negative float") {
        Json args = Json::parse("[-3.14]");
        auto conv = JsonArgConverter<void(float)>::convert(args);
        auto argsTuple = fl::get<0>(conv);
        auto result = fl::get<1>(conv);
        FL_CHECK(result.ok());
        float val = fl::get<0>(argsTuple);
        FL_CHECK(val < -3.13f);
        FL_CHECK(val > -3.15f);
    }

    FL_SUBCASE("zero values") {
        Json args = Json::parse("[0, 0.0, false]");
        auto conv = JsonArgConverter<void(int, float, bool)>::convert(args);
        auto argsTuple = fl::get<0>(conv);
        auto result = fl::get<1>(conv);
        FL_CHECK(result.ok());
        FL_CHECK_EQ(fl::get<0>(argsTuple), 0);
        float val = fl::get<1>(argsTuple);
        FL_CHECK(val > -0.001f);
        FL_CHECK(val < 0.001f);
        FL_CHECK_EQ(fl::get<2>(argsTuple), false);
    }

    FL_SUBCASE("empty string") {
        Json args = Json::parse(R"([""])");
        auto conv = JsonArgConverter<void(fl::string)>::convert(args);
        auto argsTuple = fl::get<0>(conv);
        auto result = fl::get<1>(conv);
        FL_CHECK(result.ok());
        FL_CHECK_EQ(fl::get<0>(argsTuple), "");
    }

    FL_SUBCASE("string with special characters") {
        Json args = Json::parse(R"(["hello\nworld\t!"])");
        auto conv = JsonArgConverter<void(fl::string)>::convert(args);
        auto argsTuple = fl::get<0>(conv);
        auto result = fl::get<1>(conv);
        FL_CHECK(result.ok());
        FL_CHECK_EQ(fl::get<0>(argsTuple), "hello\nworld\t!");
    }

    FL_SUBCASE("large integer") {
        Json args = Json::parse("[2147483647]");  // INT_MAX
        auto conv = JsonArgConverter<void(int)>::convert(args);
        auto argsTuple = fl::get<0>(conv);
        auto result = fl::get<1>(conv);
        FL_CHECK(result.ok());
        FL_CHECK_EQ(fl::get<0>(argsTuple), 2147483647);
    }

    FL_SUBCASE("uint8_t argument") {
        Json args = Json::parse("[255]");
        auto conv = JsonArgConverter<void(uint8_t)>::convert(args);
        auto argsTuple = fl::get<0>(conv);
        auto result = fl::get<1>(conv);
        FL_CHECK(result.ok());
        FL_CHECK_EQ(fl::get<0>(argsTuple), 255);
    }

    FL_SUBCASE("uint8_t overflow - warning or error") {
        Json args = Json::parse("[300]");  // > 255
        auto conv = JsonArgConverter<void(uint8_t)>::convert(args);
        auto result = fl::get<1>(conv);
        // Could be warning (truncation) or error depending on implementation
        // At minimum, should not silently succeed
        if (result.ok()) {
            FL_CHECK(result.hasWarning());
        }
    }
}

#endif // FASTLED_ENABLE_JSON
