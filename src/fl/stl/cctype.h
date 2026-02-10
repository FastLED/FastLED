#pragma once

///////////////////////////////////////////////////////////////////////////////
// FastLED Character Classification Functions (cctype.h equivalents)
//
// Provides character classification utilities similar to C <ctype.h>
// but adapted for embedded platforms and FastLED's needs.
//
// All functions are in the fl:: namespace.
///////////////////////////////////////////////////////////////////////////////

namespace fl {

/// @brief Check if character is whitespace (space, tab, newline, carriage return)
/// @param c The character to check
/// @return true if c is a whitespace character, false otherwise
inline bool isspace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/// @brief Check if character is a decimal digit (0-9)
/// @param c The character to check
/// @return true if c is a digit, false otherwise
inline bool isdigit(char c) {
    return c >= '0' && c <= '9';
}

/// @brief Convert character to lowercase
/// @param c The character to convert
/// @return The lowercase version of c (if c is uppercase), otherwise c unchanged
inline char tolower(char c) {
    return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c;
}

/// @brief Convert character to uppercase
/// @param c The character to convert
/// @return The uppercase version of c (if c is lowercase), otherwise c unchanged
inline char toupper(char c) {
    return (c >= 'a' && c <= 'z') ? (c - ('a' - 'A')) : c;
}

} // namespace fl
