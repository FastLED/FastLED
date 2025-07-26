#pragma once

#include "fl/string.h"

namespace fl {

/**
 * @brief Removes all whitespace characters from a JSON string to create a compact version
 * 
 * This function creates a compact version of a JSON string by removing all whitespace
 * characters (spaces, tabs, newlines, carriage returns) while preserving the JSON structure.
 * 
 * @param jsonStr The input JSON string that may contain whitespace
 * @return A new fl::string with all whitespace removed
 */
fl::string compactJsonString(const char* jsonStr);

/**
 * @brief Overload for fl::string input
 */
inline fl::string compactJsonString(const fl::string& jsonStr) {
    return compactJsonString(jsonStr.c_str());
}

} // namespace fl