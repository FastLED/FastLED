#pragma once

#ifdef memcpy_P
#undef memcpy_P
#endif
#define memcpy_P(dest, src, n) memcpy(dest, src, n)