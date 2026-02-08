/// @file rpc.cpp
/// @brief Tests for RPC system - TypedRpc and RpcFactory

#include "fl/json.h"
#include "fl/rpc.h"

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

// =============================================================================
// TEST SUITE: RpcFactory - Minimal Unit Tests (from TASK.md)
// =============================================================================

FL_TEST_CASE("RpcFactory - register+bind+call") {
    RpcFactory rpc;

    RpcFn<int(int, int)> addFn = [](int a, int b) -> int { return a + b; };
    bool registered = rpc.method<int(int, int)>("add", addFn);
    FL_CHECK(registered);

    RpcFn<int(int, int)> boundFn = rpc.bind<int(int, int)>("add");
    FL_CHECK(boundFn);

    int result = boundFn(2, 3);
    FL_CHECK_EQ(result, 5);
}

FL_TEST_CASE("RpcFactory - wrong signature bind fails") {
    RpcFactory rpc;

    RpcFn<int(int, int)> addFn = [](int a, int b) -> int { return a + b; };
    rpc.method<int(int, int)>("add", addFn);

    fl::optional<RpcFn<double(double, double)>> wrongBind =
        rpc.try_bind<double(double, double)>("add");

    FL_CHECK_FALSE(wrongBind.has_value());
}

FL_TEST_CASE("RpcFactory - void return") {
    RpcFactory rpc;

    bool flag = false;
    RpcFn<void()> pingFn = [&flag]() { flag = !flag; };
    bool registered = rpc.method<void()>("ping", pingFn);
    FL_CHECK(registered);

    RpcFn<void()> boundPing = rpc.bind<void()>("ping");
    FL_CHECK(boundPing);

    FL_CHECK_FALSE(flag);
    boundPing();
    FL_CHECK(flag);
    boundPing();
    FL_CHECK_FALSE(flag);
}

FL_TEST_CASE("RpcFactory - transport parity") {
    RpcFactory rpc;

    RpcFn<int(int, int)> addFn = [](int a, int b) -> int { return a + b; };
    rpc.method<int(int, int)>("add", addFn);

    Json request = Json::parse(R"({"method": "add", "params": [6, 7], "id": 1})");
    FL_REQUIRE(request.is_object());

    Json response = rpc.handle(request);

    FL_CHECK(response.is_object());
    FL_CHECK(response.contains("result"));
    FL_CHECK(response.contains("id"));

    int resultValue = response["result"].as_int().value_or(-1);
    FL_CHECK_EQ(resultValue, 13);

    int idValue = response["id"].as_int().value_or(-1);
    FL_CHECK_EQ(idValue, 1);
}

// =============================================================================
// TEST SUITE: Additional RpcFactory Tests
// =============================================================================

FL_TEST_CASE("RpcFactory - bind non-existent method returns empty") {
    RpcFactory rpc;

    RpcFn<int(int, int)> boundFn = rpc.bind<int(int, int)>("nonexistent");
    FL_CHECK_FALSE(boundFn);
}

FL_TEST_CASE("RpcFactory - try_bind non-existent method returns nullopt") {
    RpcFactory rpc;

    auto result = rpc.try_bind<int(int, int)>("nonexistent");
    FL_CHECK_FALSE(result.has_value());
}

FL_TEST_CASE("RpcFactory - duplicate registration same signature updates") {
    RpcFactory rpc;

    int callCount = 0;
    RpcFn<void()> fn1 = [&callCount]() { callCount += 1; };
    RpcFn<void()> fn2 = [&callCount]() { callCount += 10; };

    FL_CHECK(rpc.method<void()>("counter", fn1));
    FL_CHECK(rpc.method<void()>("counter", fn2));

    auto bound = rpc.bind<void()>("counter");
    bound();
    FL_CHECK_EQ(callCount, 10);
}

FL_TEST_CASE("RpcFactory - duplicate registration different signature fails") {
    RpcFactory rpc;

    RpcFn<int(int)> fn1 = [](int x) -> int { return x; };
    RpcFn<int(int, int)> fn2 = [](int a, int b) -> int { return a + b; };

    FL_CHECK(rpc.method<int(int)>("func", fn1));
    FL_CHECK_FALSE(rpc.method<int(int, int)>("func", fn2));
}

FL_TEST_CASE("RpcFactory - has method check") {
    RpcFactory rpc;

    FL_CHECK_FALSE(rpc.has("test"));
    rpc.method<void()>("test", []() {});
    FL_CHECK(rpc.has("test"));
}

FL_TEST_CASE("RpcFactory - handle with missing method") {
    RpcFactory rpc;

    Json request = Json::parse(R"({"method": "unknown", "params": [], "id": 1})");
    Json response = rpc.handle(request);

    FL_CHECK(response.contains("error"));
    auto errorCode = response["error"]["code"].as_int().value_or(0);
    FL_CHECK_EQ(errorCode, -32601);  // Method not found
}

FL_TEST_CASE("RpcFactory - handle with invalid params") {
    RpcFactory rpc;

    rpc.method<int(int)>("square", [](int x) -> int { return x * x; });

    Json request = Json::parse(R"({"method": "square", "params": [1, 2], "id": 1})");
    Json response = rpc.handle(request);

    FL_CHECK(response.contains("error"));
    auto errorCode = response["error"]["code"].as_int().value_or(0);
    FL_CHECK_EQ(errorCode, -32602);  // Invalid params
}

FL_TEST_CASE("RpcFactory - handle_maybe with notification") {
    RpcFactory rpc;

    bool called = false;
    rpc.method<void()>("notify", [&called]() { called = true; });

    Json notification = Json::parse(R"({"method": "notify", "params": []})");
    auto result = rpc.handle_maybe(notification);

    FL_CHECK_FALSE(result.has_value());
    FL_CHECK(called);
}

FL_TEST_CASE("RpcFactory - handle_maybe with request") {
    RpcFactory rpc;

    rpc.method<int(int)>("double", [](int x) -> int { return x * 2; });

    Json request = Json::parse(R"({"method": "double", "params": [5], "id": 1})");
    auto result = rpc.handle_maybe(request);

    FL_CHECK(result.has_value());
    if (result.has_value()) {
        FL_CHECK_EQ(result.value()["result"].as_int().value_or(0), 10);
    }
}

FL_TEST_CASE("RpcFactory - string return type") {
    RpcFactory rpc;

    RpcFn<fl::string(fl::string)> greetFn = [](fl::string name) -> fl::string {
        return "Hello, " + name + "!";
    };
    rpc.method<fl::string(fl::string)>("greet", greetFn);

    auto bound = rpc.bind<fl::string(fl::string)>("greet");
    fl::string directResult = bound("World");
    FL_CHECK_EQ(directResult, "Hello, World!");

    Json request = Json::parse(R"({"method": "greet", "params": ["Alice"], "id": 1})");
    Json response = rpc.handle(request);
    FL_CHECK_EQ(response["result"].as_string().value_or(""), "Hello, Alice!");
}

FL_TEST_CASE("RpcFactory - transport with type coercion warning") {
    RpcFactory rpc;

    rpc.method<int(int)>("square", [](int x) -> int { return x * x; });

    Json request = Json::parse(R"({"method": "square", "params": [3.7], "id": 1})");
    Json response = rpc.handle(request);

    FL_CHECK(response.contains("result"));
    int resultValue = response["result"].as_int().value_or(-1);
    FL_CHECK_EQ(resultValue, 9);  // 3 * 3 (truncated from 3.7)

    FL_CHECK(response.contains("warnings"));
    FL_CHECK(response["warnings"].is_array());
    FL_CHECK(response["warnings"].size() > 0);
}

// =============================================================================
// TEST SUITE: Typedef Flexibility
// =============================================================================

using IntBinaryOp = fl::function<int(int, int)>;
using StringTransform = fl::function<fl::string(fl::string)>;
using VoidCallback = fl::function<void()>;
using IntUnaryOp = RpcFn<int(int)>;

FL_TEST_CASE("RpcFactory - typedef flexibility with fl::function alias") {
    RpcFactory rpc;

    IntBinaryOp multiply = [](int a, int b) -> int { return a * b; };
    FL_CHECK(rpc.method<int(int, int)>("multiply", multiply));

    RpcFn<int(int, int)> bound = rpc.bind<int(int, int)>("multiply");
    FL_CHECK(bound);
    FL_CHECK_EQ(bound(4, 5), 20);

    IntBinaryOp boundAsAlias = rpc.bind<int(int, int)>("multiply");
    FL_CHECK(boundAsAlias);
    FL_CHECK_EQ(boundAsAlias(6, 7), 42);
}

FL_TEST_CASE("RpcFactory - direct lambda registration without explicit wrapper") {
    RpcFactory rpc;

    FL_CHECK(rpc.method<int(int, int)>("subtract", [](int a, int b) { return a - b; }));

    int offset = 100;
    FL_CHECK(rpc.method<int(int)>("add_offset", [offset](int x) { return x + offset; }));

    auto subtractFn = rpc.bind<int(int, int)>("subtract");
    FL_CHECK_EQ(subtractFn(10, 3), 7);

    auto offsetFn = rpc.bind<int(int)>("add_offset");
    FL_CHECK_EQ(offsetFn(5), 105);
}

FL_TEST_CASE("RpcFactory - explicit fl::function type") {
    RpcFactory rpc;

    fl::function<int(int, int)> divFn = [](int a, int b) -> int {
        return b != 0 ? a / b : 0;
    };
    FL_CHECK(rpc.method<int(int, int)>("divide", divFn));

    fl::function<int(int, int)> boundDiv = rpc.bind<int(int, int)>("divide");
    FL_CHECK(boundDiv);
    FL_CHECK_EQ(boundDiv(20, 4), 5);
    FL_CHECK_EQ(boundDiv(10, 0), 0);
}

FL_TEST_CASE("RpcFactory - mixed typedef styles in same registry") {
    RpcFactory rpc;

    RpcFn<int(int)> sqFn = [](int x) -> int { return x * x; };
    FL_CHECK(rpc.method<int(int)>("square", sqFn));

    fl::function<int(int)> cubeFn = [](int x) -> int { return x * x * x; };
    FL_CHECK(rpc.method<int(int)>("cube", cubeFn));

    IntUnaryOp negateFn = [](int x) -> int { return -x; };
    FL_CHECK(rpc.method<int(int)>("negate", negateFn));

    FL_CHECK(rpc.method<int(int)>("double", [](int x) { return x * 2; }));

    FL_CHECK_EQ(rpc.bind<int(int)>("square")(3), 9);
    FL_CHECK_EQ(rpc.bind<int(int)>("cube")(2), 8);
    FL_CHECK_EQ(rpc.bind<int(int)>("negate")(5), -5);
    FL_CHECK_EQ(rpc.bind<int(int)>("double")(7), 14);

    fl::function<int(int)> boundSquare = rpc.bind<int(int)>("square");
    FL_CHECK_EQ(boundSquare(4), 16);

    IntUnaryOp boundCube = rpc.bind<int(int)>("cube");
    FL_CHECK_EQ(boundCube(3), 27);
}

FL_TEST_CASE("RpcFactory - void callbacks with different typedefs") {
    RpcFactory rpc;

    int counter = 0;

    VoidCallback incrementFn = [&counter]() { counter++; };
    FL_CHECK(rpc.method<void()>("increment", incrementFn));

    RpcFn<void()> decrementFn = [&counter]() { counter--; };
    FL_CHECK(rpc.method<void()>("decrement", decrementFn));

    FL_CHECK(rpc.method<void()>("reset", [&counter]() { counter = 0; }));

    fl::function<void()> doubleCounter = [&counter]() { counter *= 2; };
    FL_CHECK(rpc.method<void()>("double_counter", doubleCounter));

    counter = 5;
    rpc.bind<void()>("increment")();
    FL_CHECK_EQ(counter, 6);

    rpc.bind<void()>("decrement")();
    FL_CHECK_EQ(counter, 5);

    rpc.bind<void()>("double_counter")();
    FL_CHECK_EQ(counter, 10);

    rpc.bind<void()>("reset")();
    FL_CHECK_EQ(counter, 0);
}

FL_TEST_CASE("RpcFactory - string transform with typedef alias") {
    RpcFactory rpc;

    StringTransform upperFn = [](fl::string s) -> fl::string {
        fl::string result;
        for (size_t i = 0; i < s.size(); ++i) {
            char c = s[i];
            if (c >= 'a' && c <= 'z') {
                c = c - 'a' + 'A';
            }
            result += c;
        }
        return result;
    };
    FL_CHECK(rpc.method<fl::string(fl::string)>("upper", upperFn));

    RpcFn<fl::string(fl::string)> boundUpper = rpc.bind<fl::string(fl::string)>("upper");
    FL_CHECK_EQ(boundUpper("hello"), "HELLO");

    fl::function<fl::string(fl::string)> boundUpper2 = rpc.bind<fl::string(fl::string)>("upper");
    FL_CHECK_EQ(boundUpper2("world"), "WORLD");

    StringTransform boundUpper3 = rpc.bind<fl::string(fl::string)>("upper");
    FL_CHECK_EQ(boundUpper3("test"), "TEST");
}

FL_TEST_CASE("RpcFactory - try_bind with different typedef styles") {
    RpcFactory rpc;

    IntBinaryOp addFn = [](int a, int b) -> int { return a + b; };
    FL_CHECK(rpc.method<int(int, int)>("add", addFn));

    auto opt1 = rpc.try_bind<int(int, int)>("add");
    FL_CHECK(opt1.has_value());
    FL_CHECK_EQ(opt1.value()(3, 4), 7);

    if (opt1.has_value()) {
        RpcFn<int(int, int)> fn1 = opt1.value();
        fl::function<int(int, int)> fn2 = opt1.value();
        IntBinaryOp fn3 = opt1.value();

        FL_CHECK_EQ(fn1(1, 2), 3);
        FL_CHECK_EQ(fn2(2, 3), 5);
        FL_CHECK_EQ(fn3(3, 4), 7);
    }
}

FL_TEST_CASE("RpcFactory - JSON transport works regardless of registration typedef") {
    RpcFactory rpc;

    IntBinaryOp pow2Fn = [](int base, int exp) -> int {
        int result = 1;
        for (int i = 0; i < exp; ++i) result *= base;
        return result;
    };
    FL_CHECK(rpc.method<int(int, int)>("pow", pow2Fn));

    fl::function<fl::string(fl::string, fl::string)> concatFn =
        [](fl::string a, fl::string b) -> fl::string { return a + b; };
    FL_CHECK(rpc.method<fl::string(fl::string, fl::string)>("concat", concatFn));

    Json req1 = Json::parse(R"({"method": "pow", "params": [2, 8], "id": 1})");
    Json resp1 = rpc.handle(req1);
    FL_CHECK_EQ(resp1["result"].as_int().value_or(0), 256);

    Json req2 = Json::parse(R"({"method": "concat", "params": ["foo", "bar"], "id": 2})");
    Json resp2 = rpc.handle(req2);
    FL_CHECK_EQ(resp2["result"].as_string().value_or(""), "foobar");
}

// =============================================================================
// TEST SUITE: Schema Generation - OpenRPC Format
// =============================================================================

FL_TEST_CASE("RpcFactory - schema generation basic") {
    RpcFactory rpc;

    FL_SUBCASE("empty registry returns valid schema") {
        Json schema = rpc.schema();
        FL_CHECK(schema.is_object());
        FL_CHECK(schema.contains("openrpc"));
        FL_CHECK(schema.contains("info"));
        FL_CHECK(schema.contains("methods"));
        FL_CHECK_EQ(schema["openrpc"].as_string().value_or(""), "1.3.2");
        FL_CHECK(schema["methods"].is_array());
        FL_CHECK_EQ(schema["methods"].size(), 0);
    }

    FL_SUBCASE("single method schema") {
        rpc.method("add", [](int a, int b) -> int { return a + b; });

        Json schema = rpc.schema();
        FL_CHECK_EQ(schema["methods"].size(), 1);

        Json method = schema["methods"][0];
        FL_CHECK_EQ(method["name"].as_string().value_or(""), "add");
        FL_CHECK(method["params"].is_array());
        FL_CHECK_EQ(method["params"].size(), 2);
        FL_CHECK(method.contains("result"));
    }

    FL_SUBCASE("void return method has no result") {
        bool called = false;
        rpc.method("ping", [&called]() { called = true; });

        Json methods = rpc.methods();
        FL_CHECK_EQ(methods.size(), 1);

        Json method = methods[0];
        FL_CHECK_EQ(method["name"].as_string().value_or(""), "ping");
        FL_CHECK(method["params"].is_array());
        FL_CHECK_EQ(method["params"].size(), 0);
        FL_CHECK_FALSE(method.contains("result"));
    }
}

FL_TEST_CASE("RpcFactory - parameter schema types") {
    RpcFactory rpc;

    FL_SUBCASE("integer parameters") {
        rpc.method("int_fn", [](int x) -> int { return x; });

        Json methods = rpc.methods();
        Json param = methods[0]["params"][0];
        FL_CHECK_EQ(param["schema"]["type"].as_string().value_or(""), "integer");
    }

    FL_SUBCASE("float parameters") {
        rpc.method("float_fn", [](float x) -> float { return x; });

        Json methods = rpc.methods();
        Json param = methods[0]["params"][0];
        FL_CHECK_EQ(param["schema"]["type"].as_string().value_or(""), "number");
    }

    FL_SUBCASE("bool parameters") {
        rpc.method("bool_fn", [](bool x) -> bool { return x; });

        Json methods = rpc.methods();
        Json param = methods[0]["params"][0];
        FL_CHECK_EQ(param["schema"]["type"].as_string().value_or(""), "boolean");
    }

    FL_SUBCASE("string parameters") {
        rpc.method("string_fn", [](fl::string x) -> fl::string { return x; });

        Json methods = rpc.methods();
        Json param = methods[0]["params"][0];
        FL_CHECK_EQ(param["schema"]["type"].as_string().value_or(""), "string");
    }

    FL_SUBCASE("mixed parameters") {
        rpc.method("mixed", [](int a, float b, fl::string c) -> bool { return a > 0; });

        Json methods = rpc.methods();
        Json params = methods[0]["params"];
        FL_CHECK_EQ(params.size(), 3);
        FL_CHECK_EQ(params[0]["schema"]["type"].as_string().value_or(""), "integer");
        FL_CHECK_EQ(params[1]["schema"]["type"].as_string().value_or(""), "number");
        FL_CHECK_EQ(params[2]["schema"]["type"].as_string().value_or(""), "string");
    }
}

FL_TEST_CASE("RpcFactory - result schema types") {
    RpcFactory rpc;

    FL_SUBCASE("integer result") {
        rpc.method("int_result", []() -> int { return 42; });

        Json methods = rpc.methods();
        Json result = methods[0]["result"];
        FL_CHECK_EQ(result["type"].as_string().value_or(""), "integer");
    }

    FL_SUBCASE("float result") {
        rpc.method("float_result", []() -> float { return 3.14f; });

        Json methods = rpc.methods();
        Json result = methods[0]["result"];
        FL_CHECK_EQ(result["type"].as_string().value_or(""), "number");
    }

    FL_SUBCASE("bool result") {
        rpc.method("bool_result", []() -> bool { return true; });

        Json methods = rpc.methods();
        Json result = methods[0]["result"];
        FL_CHECK_EQ(result["type"].as_string().value_or(""), "boolean");
    }

    FL_SUBCASE("string result") {
        rpc.method("string_result", []() -> fl::string { return "hello"; });

        Json methods = rpc.methods();
        Json result = methods[0]["result"];
        FL_CHECK_EQ(result["type"].as_string().value_or(""), "string");
    }
}

FL_TEST_CASE("RpcFactory - multiple methods schema") {
    RpcFactory rpc;

    rpc.method("add", [](int a, int b) -> int { return a + b; });
    rpc.method("greet", [](fl::string name) -> fl::string { return "Hello " + name; });
    rpc.method("ping", []() {});

    Json schema = rpc.schema("Test API", "2.0.0");

    FL_CHECK_EQ(schema["info"]["title"].as_string().value_or(""), "Test API");
    FL_CHECK_EQ(schema["info"]["version"].as_string().value_or(""), "2.0.0");
    FL_CHECK_EQ(schema["methods"].size(), 3);
    FL_CHECK_EQ(rpc.count(), 3);
}

FL_TEST_CASE("Rpc alias works") {
    // Test that Rpc is an alias for RpcFactory
    fl::Rpc rpc;

    auto add = rpc.method("add", [](int a, int b) -> int { return a + b; });
    FL_CHECK_EQ(add(2, 3), 5);

    Json schema = rpc.schema();
    FL_CHECK(schema.is_object());
    FL_CHECK_EQ(rpc.count(), 1);
}

// =============================================================================
// TEST SUITE: Named Parameters
// =============================================================================

FL_TEST_CASE("RpcFactory - named parameters in schema") {
    RpcFactory rpc;

    FL_SUBCASE("named params via method_with builder") {
        auto add = rpc.method_with("add", [](int a, int b) -> int { return a + b; })
            .params({"left", "right"})
            .done();

        FL_CHECK_EQ(add(2, 3), 5);

        Json methods = rpc.methods();
        FL_CHECK_EQ(methods.size(), 1);

        Json params = methods[0]["params"];
        FL_CHECK_EQ(params.size(), 2);
        FL_CHECK_EQ(params[0]["name"].as_string().value_or(""), "left");
        FL_CHECK_EQ(params[1]["name"].as_string().value_or(""), "right");
    }

    FL_SUBCASE("partial named params uses defaults for rest") {
        rpc.method_with("func", [](int a, int b, int c) -> int { return a + b + c; })
            .params({"first"})  // Only first param named
            .done();

        Json methods = rpc.methods();
        Json params = methods[0]["params"];
        FL_CHECK_EQ(params[0]["name"].as_string().value_or(""), "first");
        FL_CHECK_EQ(params[1]["name"].as_string().value_or(""), "arg1");
        FL_CHECK_EQ(params[2]["name"].as_string().value_or(""), "arg2");
    }
}

// =============================================================================
// TEST SUITE: Method Descriptions
// =============================================================================

FL_TEST_CASE("RpcFactory - method descriptions in schema") {
    RpcFactory rpc;

    rpc.method_with("calculate", [](int x) -> int { return x * 2; })
        .description("Doubles the input value")
        .done();

    Json methods = rpc.methods();
    FL_CHECK_EQ(methods.size(), 1);
    FL_CHECK(methods[0].contains("description"));
    FL_CHECK_EQ(methods[0]["description"].as_string().value_or(""), "Doubles the input value");
}

// =============================================================================
// TEST SUITE: Tags (OpenRPC Grouping)
// =============================================================================

FL_TEST_CASE("RpcFactory - tags for method grouping") {
    RpcFactory rpc;

    FL_SUBCASE("single tag") {
        rpc.method_with("led.setBrightness", [](int b) { (void)b; })
            .tags({"led"})
            .done();

        Json methods = rpc.methods();
        FL_CHECK(methods[0].contains("tags"));
        FL_CHECK(methods[0]["tags"].is_array());
        FL_CHECK_EQ(methods[0]["tags"].size(), 1);
        FL_CHECK_EQ(methods[0]["tags"][0]["name"].as_string().value_or(""), "led");
    }

    FL_SUBCASE("multiple tags") {
        rpc.method_with("led.setColor", [](int r, int g, int b) { (void)r; (void)g; (void)b; })
            .tags({"led", "color"})
            .done();

        Json methods = rpc.methods();
        FL_CHECK_EQ(methods[0]["tags"].size(), 2);
        FL_CHECK_EQ(methods[0]["tags"][0]["name"].as_string().value_or(""), "led");
        FL_CHECK_EQ(methods[0]["tags"][1]["name"].as_string().value_or(""), "color");
    }

    FL_SUBCASE("tags() returns unique tag names") {
        rpc.method_with("led.on", []() {})
            .tags({"led", "control"})
            .done();
        rpc.method_with("led.off", []() {})
            .tags({"led", "control"})
            .done();
        rpc.method_with("system.status", []() -> fl::string { return "ok"; })
            .tags({"system"})
            .done();

        auto tagList = rpc.tags();
        FL_CHECK_EQ(tagList.size(), 3);  // led, control, system - unique
    }
}

// =============================================================================
// TEST SUITE: Namespaced Methods (Dot Notation)
// =============================================================================

FL_TEST_CASE("RpcFactory - namespaced methods with dot notation") {
    RpcFactory rpc;

    rpc.method("led.setBrightness", [](int b) { (void)b; });
    rpc.method("led.getStatus", []() -> fl::string { return "on"; });
    rpc.method("system.reboot", []() {});

    FL_CHECK(rpc.has("led.setBrightness"));
    FL_CHECK(rpc.has("led.getStatus"));
    FL_CHECK(rpc.has("system.reboot"));

    // JSON-RPC transport works with namespaced methods
    Json request = Json::parse(R"({"method": "led.getStatus", "params": [], "id": 1})");
    Json response = rpc.handle(request);
    FL_CHECK_EQ(response["result"].as_string().value_or(""), "on");
}

// =============================================================================
// TEST SUITE: rpc.discover Built-in
// =============================================================================

FL_TEST_CASE("RpcFactory - rpc.discover built-in") {
    RpcFactory rpc;

    FL_SUBCASE("discover disabled by default") {
        rpc.method("add", [](int a, int b) -> int { return a + b; });

        Json request = Json::parse(R"({"method": "rpc.discover", "params": [], "id": 1})");
        Json response = rpc.handle(request);

        // Should return error - method not found
        FL_CHECK(response.contains("error"));
    }

    FL_SUBCASE("discover enabled returns schema") {
        rpc.enableDiscover("My API", "2.0.0");
        rpc.method("add", [](int a, int b) -> int { return a + b; });

        Json request = Json::parse(R"({"method": "rpc.discover", "params": [], "id": 1})");
        Json response = rpc.handle(request);

        FL_CHECK(response.contains("result"));
        Json schema = response["result"];

        FL_CHECK_EQ(schema["openrpc"].as_string().value_or(""), "1.3.2");
        FL_CHECK_EQ(schema["info"]["title"].as_string().value_or(""), "My API");
        FL_CHECK_EQ(schema["info"]["version"].as_string().value_or(""), "2.0.0");
        FL_CHECK(schema["methods"].is_array());
        FL_CHECK_EQ(schema["methods"].size(), 1);
    }
}

// =============================================================================
// TEST SUITE: Full Fluent API Example
// =============================================================================

FL_TEST_CASE("RpcFactory - full fluent API example") {
    RpcFactory rpc;
    rpc.enableDiscover("LED Controller API", "1.0.0");

    // Register methods with full metadata
    auto setBrightness = rpc.method_with("led.setBrightness", [](int brightness) {
            (void)brightness;
        })
        .params({"brightness"})
        .description("Set LED brightness (0-255)")
        .tags({"led", "control"})
        .done();

    auto getStatus = rpc.method_with("led.getStatus", []() -> fl::string {
            return "active";
        })
        .description("Get current LED status")
        .tags({"led", "status"})
        .done();

    // Simple method without extra metadata still works
    rpc.method("system.ping", []() {});

    // Verify everything works
    FL_CHECK(setBrightness);
    FL_CHECK(getStatus);
    FL_CHECK_EQ(getStatus(), "active");

    // Verify schema has all metadata
    Json schema = rpc.schema();
    FL_CHECK_EQ(schema["methods"].size(), 3);

    // Find led.setBrightness in methods
    bool found = false;
    for (fl::size i = 0; i < schema["methods"].size(); ++i) {
        if (schema["methods"][i]["name"].as_string().value_or("") == "led.setBrightness") {
            Json method = schema["methods"][i];
            FL_CHECK_EQ(method["params"][0]["name"].as_string().value_or(""), "brightness");
            FL_CHECK_EQ(method["description"].as_string().value_or(""), "Set LED brightness (0-255)");
            FL_CHECK_EQ(method["tags"].size(), 2);
            found = true;
            break;
        }
    }
    FL_CHECK(found);
}

#endif // FASTLED_ENABLE_JSON
