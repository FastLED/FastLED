#pragma once

/// @file memfill.h
/// Memory filling and copying utilities

#include "fl/stl/stdint.h"

namespace fl {

/// Fill memory with a value (wrapper for memset)
/// @param ptr Pointer to the memory area
/// @param value The byte value to fill with
/// @param num Number of bytes to fill
/// @return Pointer to the memory area
void* memfill(void* ptr, int value, fl::size num) noexcept;

/// Copy memory (wrapper for memcpy)
/// @param dst Destination pointer
/// @param src Source pointer
/// @param num Number of bytes to copy
/// @return Pointer to the destination
void* memcopy(void* dst, const void* src, fl::size num) noexcept;

/// Move memory (handles overlapping regions, wrapper for memmove)
/// @param dst Destination pointer
/// @param src Source pointer
/// @param num Number of bytes to move
/// @return Pointer to the destination
void* memmove(void* dst, const void* src, fl::size num) noexcept;

/// Find substring in string
/// @param haystack The string to search in
/// @param needle The substring to search for
/// @return Pointer to the beginning of the substring, or nullptr if not found
const char* strstr(const char* haystack, const char* needle) noexcept;

/// Compare n bytes of two strings
/// @param s1 First string
/// @param s2 Second string
/// @param n Number of bytes to compare
/// @return 0 if equal, negative if s1<s2, positive if s1>s2
int strncmp(const char* s1, const char* s2, fl::size n) noexcept;

}  // namespace fl
