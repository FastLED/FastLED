// src/fl/channels/detail/validation/result_formatter.cpp.hpp
//
// Result formatting implementation

#include "result_formatter.h"
#include "fl/stl/sstream.h"
#include "fl/log.h"

// DriverTestResult is now defined in fl/channels/validation.h

namespace fl {
namespace validation {

inline string formatSummaryTable(const fl::vector<fl::DriverTestResult>& driver_results) {
    fl::sstream ss;
    ss << "\n╔════════════════════════════════════════════════════════════════╗\n";
    ss << "║ DRIVER VALIDATION SUMMARY                                      ║\n";
    ss << "╠════════════════════════════════════════════════════════════════╣\n";
    ss << "║ Driver       │ Status      │ Tests Passed │ Total Tests       ║\n";
    ss << "╠══════════════╪═════════════╪══════════════╪═══════════════════╣\n";

    for (fl::size i = 0; i < driver_results.size(); i++) {
        const auto& result = driver_results[i];
        const char* status;
        if (result.skipped) {
            status = "SKIPPED    ";
        } else if (result.allPassed()) {
            status = "PASS ✓     ";
        } else if (result.anyFailed()) {
            status = "FAIL ✗     ";
        } else {
            status = "NO TESTS   ";
        }

        // Build table row
        ss << "║ ";

        // Driver name (12 chars, left-aligned)
        fl::string driver_name = result.driver_name;
        if (driver_name.length() > 12) {
            driver_name = driver_name.substr(0, 12);
        }
        ss << driver_name;
        for (size_t j = driver_name.length(); j < 12; j++) {
            ss << " ";
        }
        ss << " │ " << status << " │ ";

        // Tests passed (12 chars, left-aligned)
        if (result.skipped) {
            ss << "-";
            for (int j = 1; j < 12; j++) ss << " ";
        } else {
            fl::string passed = fl::to_string(result.passed_tests);
            ss << passed;
            for (size_t j = passed.length(); j < 12; j++) ss << " ";
        }
        ss << " │ ";

        // Total tests (17 chars, left-aligned)
        if (result.skipped) {
            ss << "-";
            for (int j = 1; j < 17; j++) ss << " ";
        } else {
            fl::string total = fl::to_string(result.total_tests);
            ss << total;
            for (size_t j = total.length(); j < 17; j++) ss << " ";
        }
        ss << " ║\n";
    }

    ss << "╚══════════════╧═════════════╧══════════════╧═══════════════════╝";
    return ss.str();
}

inline void printSummaryTable(const fl::vector<fl::DriverTestResult>& driver_results) {
    FL_WARN(formatSummaryTable(driver_results).c_str());
}

}  // namespace validation
}  // namespace fl
