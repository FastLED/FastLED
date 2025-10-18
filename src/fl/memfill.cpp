#include "fl/memfill.h"
#include <cstring>  // ok include

namespace fl {

void* memfill_impl(void* ptr, int value, fl::size num) {
    return memset(ptr, value, num);
}

void* memcopy_impl(void* dst, const void* src, fl::size num) {
    return memcpy(dst, src, num);
}

fl::size strlen(const char* str) {
    return ::strlen(str);
}

void* memmove_impl(void* dst, const void* src, fl::size num) {
    return memmove(dst, src, num);
}

int strcmp(const char* s1, const char* s2) {
    return ::strcmp(s1, s2);
}

const char* strstr(const char* haystack, const char* needle) {
    return ::strstr(haystack, needle);
}

int strncmp(const char* s1, const char* s2, fl::size n) {
    return ::strncmp(s1, s2, n);
}

void* memmove(void* dst, const void* src, fl::size num) {
    return ::memmove(dst, src, num);
}

} // namespace fl
