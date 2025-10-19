#include "fl/cstring.h"

namespace fl {

void* memfill(void* ptr, int value, fl::size num) noexcept {
    return __builtin_memset(ptr, value, num);
}

void* memcopy(void* dst, const void* src, fl::size num) noexcept {
    return __builtin_memcpy(dst, src, num);
}

void* memmove(void* dst, const void* src, fl::size num) noexcept {
    return __builtin_memmove(dst, src, num);
}

const char* strstr(const char* haystack, const char* needle) noexcept {
    // Manual implementation without cstring
    if (!needle || needle[0] == '\0') {
        return haystack;
    }
    for (; *haystack; ++haystack) {
        const char* h = haystack;
        const char* n = needle;
        while (*h && *n && *h == *n) {
            ++h;
            ++n;
        }
        if (*n == '\0') {
            return haystack;
        }
    }
    return nullptr;
}

int strncmp(const char* s1, const char* s2, fl::size n) noexcept {
    for (fl::size i = 0; i < n; ++i) {
        if (s1[i] != s2[i]) {
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        }
        if (s1[i] == '\0') {
            return 0;
        }
    }
    return 0;
}

} // namespace fl
