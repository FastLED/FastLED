// src/fl/channels/detail/validation/result_formatter.h
//
// Result formatting utilities for validation testing

#pragma once

#include "fl/stl/vector.h"
#include "fl/stl/string.h"

namespace fl {

// Forward declarations
struct DriverTestResult;

namespace validation {

/// @brief Format driver validation results as a summary table
/// @param driver_results Vector of driver test results
/// @return Formatted table string
string formatSummaryTable(const fl::vector<fl::DriverTestResult>& driver_results);

/// @brief Print driver validation summary table to log
/// @param driver_results Vector of driver test results
void printSummaryTable(const fl::vector<fl::DriverTestResult>& driver_results);

}  // namespace validation
}  // namespace fl
