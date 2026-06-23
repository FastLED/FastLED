/// @file runtime_rpc_binding.hpp
/// @brief Host-side coverage for the non-template RuntimeRpcBinding path
///        (FastLED #3246 Tier 1A). Host targets compile with
///        FL_PLATFORM_HAS_LARGE_MEMORY = 1 so Rpc::registerMethod routes
///        through TypedInvoker by default; these tests build a
///        RuntimeRpcBinding directly via the factory and exercise the
///        invoke() body that ships on LPC845 / AVR / similar.
// ok cpp include

#include "test.h"

#include "fl/stl/json.h"
#include "fl/stl/string.h"
#include "fl/stl/function.h"
#include "fl/stl/tuple.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "fl/remote/rpc/runtime_rpc_binding.h"
#include "fl/remote/rpc/type_conversion_result.h"

using namespace fl;
using fl::detail::makeRuntimeRpcBinding;
using fl::detail::RuntimeRpcBinding;

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("RuntimeRpcBinding - returns scalar result through type-erased invoke") {
    FL_SUBCASE("int(int,int) sums args and encodes result as JSON int") {
        fl::function<int(int, int)> add = [](int a, int b) { return a + b; };
        fl::shared_ptr<RuntimeRpcBinding> binding = makeRuntimeRpcBinding(add);

        json args = json::parse("[5, 7]");
        fl::tuple<TypeConversionResult, json> out = binding->invoke(args);
        TypeConversionResult result = fl::get<0>(out);
        json value = fl::get<1>(out);

        FL_CHECK(result.ok());
        FL_CHECK_FALSE(result.hasError());
        FL_CHECK_EQ(value.as_int().value_or(0), 12);
    }

    FL_SUBCASE("fl::string(int,int,int,int) -- the LPC845 hot signature from #3246") {
        // The signature whose TypedRpcBinding<...>::invokeWithReturn measured
        // 2,142 B on LPC845-BRK. The behavioral parity is verified here; the
        // flash-savings claim is checked separately via `bash bloat lpc845brk`.
        fl::function<fl::string(int, int, int, int)> fmt =
            [](int a, int b, int c, int d) {
                return fl::to_string(static_cast<fl::i64>(a + b + c + d));
            };
        fl::shared_ptr<RuntimeRpcBinding> binding = makeRuntimeRpcBinding(fmt);

        json args = json::parse("[1, 2, 3, 4]");
        fl::tuple<TypeConversionResult, json> out = binding->invoke(args);
        TypeConversionResult result = fl::get<0>(out);
        json value = fl::get<1>(out);

        FL_CHECK(result.ok());
        FL_CHECK_EQ(value.as_string().value_or(fl::string()), fl::string("10"));
    }

    FL_SUBCASE("float(float) preserves precision") {
        fl::function<float(float)> doubleIt = [](float x) { return x * 2.0f; };
        fl::shared_ptr<RuntimeRpcBinding> binding = makeRuntimeRpcBinding(doubleIt);

        json args = json::parse("[3.5]");
        fl::tuple<TypeConversionResult, json> out = binding->invoke(args);
        TypeConversionResult result = fl::get<0>(out);
        json value = fl::get<1>(out);

        FL_CHECK(result.ok());
        float v = value.as_float().value_or(0.0f);
        FL_CHECK(v > 6.99f && v < 7.01f);
    }

    FL_SUBCASE("bool(bool) round-trips correctly") {
        fl::function<bool(bool)> negate = [](bool x) { return !x; };
        fl::shared_ptr<RuntimeRpcBinding> binding = makeRuntimeRpcBinding(negate);

        json args_true = json::parse("[true]");
        json value_true = fl::get<1>(binding->invoke(args_true));
        FL_CHECK_EQ(value_true.as_bool().value_or(true), false);

        json args_false = json::parse("[false]");
        json value_false = fl::get<1>(binding->invoke(args_false));
        FL_CHECK_EQ(value_false.as_bool().value_or(false), true);
    }
}

FL_TEST_CASE("RuntimeRpcBinding - void return type") {
    FL_SUBCASE("void(int) executes side effect, returns null json") {
        int sink = 0;
        int* sinkPtr = &sink;
        fl::function<void(int)> consume = [sinkPtr](int v) { *sinkPtr = v; };
        fl::shared_ptr<RuntimeRpcBinding> binding = makeRuntimeRpcBinding(consume);

        json args = json::parse("[42]");
        fl::tuple<TypeConversionResult, json> out = binding->invoke(args);
        TypeConversionResult result = fl::get<0>(out);
        json value = fl::get<1>(out);

        FL_CHECK(result.ok());
        FL_CHECK(value.is_null());
        FL_CHECK_EQ(sink, 42);
    }

    FL_SUBCASE("void() -- zero-arg signature") {
        bool fired = false;
        bool* firedPtr = &fired;
        fl::function<void()> ping = [firedPtr]() { *firedPtr = true; };
        fl::shared_ptr<RuntimeRpcBinding> binding = makeRuntimeRpcBinding(ping);

        json args = json::parse("[]");
        fl::tuple<TypeConversionResult, json> out = binding->invoke(args);
        TypeConversionResult result = fl::get<0>(out);
        json value = fl::get<1>(out);

        FL_CHECK(result.ok());
        FL_CHECK(value.is_null());
        FL_CHECK(fired);
    }
}

FL_TEST_CASE("RuntimeRpcBinding - error paths surface through TypeConversionResult") {
    FL_SUBCASE("non-array args yields error") {
        fl::function<int(int)> identity = [](int x) { return x; };
        fl::shared_ptr<RuntimeRpcBinding> binding = makeRuntimeRpcBinding(identity);

        json args = json::parse("{\"a\": 1}");  // object, not array
        TypeConversionResult result = fl::get<0>(binding->invoke(args));
        FL_CHECK(result.hasError());
        FL_CHECK(result.errorMessage().find("array") != fl::string::npos);
    }

    FL_SUBCASE("arg count mismatch yields error") {
        fl::function<int(int, int)> add = [](int a, int b) { return a + b; };
        fl::shared_ptr<RuntimeRpcBinding> binding = makeRuntimeRpcBinding(add);

        json args = json::parse("[1]");  // only one of two
        TypeConversionResult result = fl::get<0>(binding->invoke(args));
        FL_CHECK(result.hasError());
        FL_CHECK(result.errorMessage().find("expected 2") != fl::string::npos);
    }
}

FL_TEST_CASE("RuntimeRpcBinding - string argument storage") {
    FL_SUBCASE("fl::string(fl::string) round-trips") {
        fl::function<fl::string(fl::string)> shout = [](fl::string s) {
            return s + fl::string("!");
        };
        fl::shared_ptr<RuntimeRpcBinding> binding = makeRuntimeRpcBinding(shout);

        json args = json::parse(R"(["hi"])");
        json value = fl::get<1>(binding->invoke(args));
        FL_CHECK_EQ(value.as_string().value_or(fl::string()), fl::string("hi!"));
    }

    FL_SUBCASE("int(const char*) uses ConstCharPtrWrapper storage adapter") {
        fl::function<int(const char*)> measure =
            [](const char* p) { return static_cast<int>(fl::string(p).size()); };
        fl::shared_ptr<RuntimeRpcBinding> binding = makeRuntimeRpcBinding(measure);

        json args = json::parse(R"(["abcde"])");
        json value = fl::get<1>(binding->invoke(args));
        FL_CHECK_EQ(value.as_int().value_or(0), 5);
    }
}

FL_TEST_CASE("RuntimeRpcBinding - binding lifetime tears down user_fn cleanly") {
    // RuntimeRpcBinding owns the heap-allocated fl::function<Sig> through
    // mUserFn + mDestroyUserFn. Dropping the shared_ptr must run the per-Sig
    // destroy fn (cleanly tearing down the captured lambda). ASAN/LSAN in
    // debug mode catches any leak here.
    fl::function<int(int)> fn = [](int x) { return x + 1; };

    {
        fl::shared_ptr<RuntimeRpcBinding> binding = makeRuntimeRpcBinding(fn);
        json args = json::parse("[10]");
        FL_CHECK_EQ(fl::get<1>(binding->invoke(args)).as_int().value_or(0), 11);
        // binding goes out of scope -> destroy_user_fn runs
    }

    FL_CHECK(true);
}

} // FL_TEST_FILE
