#pragma once

#include "fl/int.h"
#include "fl/json.h"
#include "fl/stl/stdio.h"

/// Helper for building profile test results in the format expected by ci/profile_runner.py
///
/// The profile runner expects JSON output with this exact structure:
/// ```
/// PROFILE_RESULT:{
///   "variant": "string",       // e.g., "baseline", "optimized", "simd"
///   "target": "string",        // Function/feature name
///   "total_calls": int,        // Number of calls executed
///   "total_time_ns": int64,    // Total time in nanoseconds
///   "ns_per_call": double,     // Average time per call
///   "calls_per_sec": double    // Throughput (calls/second)
/// }
/// ```
///
/// Usage example:
/// ```cpp
/// int main(int argc, char *argv[]) {
///     bool json_output = (argc > 1 && fl::strcmp(argv[1], "baseline") == 0);
///
///     u32 t0 = ::micros();
///     benchmark_function(CALLS);
///     u32 t1 = ::micros();
///
///     if (json_output) {
///         ProfileResultBuilder::print_result("baseline", "sincos32", CALLS, t1 - t0);
///     }
/// }
/// ```
class ProfileResultBuilder {
public:
    /// Simple one-line result printing for common case
    ///
    /// @param variant Variant name (e.g., "baseline", "optimized")
    /// @param target Function/feature being profiled
    /// @param total_calls Number of function calls executed
    /// @param elapsed_us Total elapsed time in microseconds
    static void print_result(const char* variant, const char* target,
                           int total_calls, fl::u32 elapsed_us) {
        fl::i64 elapsed_ns = static_cast<fl::i64>(elapsed_us) * 1000LL;
        double ns_per_call = static_cast<double>(elapsed_ns) / total_calls;
        double calls_per_sec = 1e9 / ns_per_call;

        fl::Json result = fl::Json::object();
        result.set("variant", variant);
        result.set("target", target);
        result.set("total_calls", total_calls);
        result.set("total_time_ns", elapsed_ns);
        result.set("ns_per_call", ns_per_call);
        result.set("calls_per_sec", calls_per_sec);

        fl::printf("PROFILE_RESULT:%s\n", result.to_string().c_str());
    }

    /// Constructor for building custom results (e.g., comparison tests)
    ProfileResultBuilder(const char* variant, const char* target)
        : m_result(fl::Json::object()) {
        m_result.set("variant", variant);
        m_result.set("target", target);
    }

    /// Add timing data (automatically calculates ns_per_call and calls_per_sec)
    void add_timing(int total_calls, fl::u32 elapsed_us) {
        fl::i64 elapsed_ns = static_cast<fl::i64>(elapsed_us) * 1000LL;
        double ns_per_call = static_cast<double>(elapsed_ns) / total_calls;
        double calls_per_sec = 1e9 / ns_per_call;

        m_result.set("total_calls", total_calls);
        m_result.set("total_time_ns", elapsed_ns);
        m_result.set("ns_per_call", ns_per_call);
        m_result.set("calls_per_sec", calls_per_sec);
    }

    /// Add custom field to JSON (for comparison/analysis tests)
    void set(const char* key, const fl::Json& value) {
        m_result.set(key, value);
    }

    void set(const char* key, const char* value) {
        m_result.set(key, value);
    }

    void set(const char* key, int value) {
        m_result.set(key, value);
    }

    void set(const char* key, fl::i64 value) {
        m_result.set(key, value);
    }

    void set(const char* key, double value) {
        m_result.set(key, value);
    }

    /// Print the result with PROFILE_RESULT: prefix
    void print() const {
        fl::printf("PROFILE_RESULT:%s\n", m_result.to_string().c_str());
    }

    /// Get the JSON object (for further manipulation)
    fl::Json& json() {
        return m_result;
    }

private:
    fl::Json m_result;
};
