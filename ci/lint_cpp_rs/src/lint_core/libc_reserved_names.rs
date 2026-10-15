// Reserved libc function names for LibcMirrorChecker and the libc back-stop
// coverage pass (FastLED #4834 / #4838). This table is the C library's surface,
// written out by hand; it must never be derived from the names fl mirrors today,
// so that a new mirror is caught the day it is added.

/// libc functions that take a pointer argument (or none), so an fl-typed
/// argument can drag `fl` into ADL. Every fl function with one of these names
/// must be `template <typename FlLibcTag = void> FASTLED_FORCE_INLINE`.
const LIBC_RESERVED_NAMES: &[&str] = &[
    // C11 <string.h>
    "memchr", "memcmp", "memcpy", "memmove", "memset", "strcat", "strchr",
    "strcmp", "strcoll", "strcpy", "strcspn", "strerror", "strlen", "strncat",
    "strncmp", "strncpy", "strpbrk", "strrchr", "strspn", "strstr", "strtok",
    "strxfrm",
    // C11 <stdlib.h>
    "atof", "atoi", "atol", "atoll", "strtod", "strtof", "strtold", "strtol",
    "strtoll", "strtoul", "strtoull", "rand", "srand", "aligned_alloc",
    "calloc", "free", "malloc", "realloc", "abort", "atexit", "at_quick_exit",
    "exit", "_Exit", "quick_exit", "getenv", "system", "bsearch", "qsort",
    "mblen", "mbtowc", "wctomb", "mbstowcs", "wcstombs",
    // <strings.h>
    "bcmp", "bcopy", "bzero", "index", "rindex", "strcasecmp", "strncasecmp",
    "strcasecmp_l", "strncasecmp_l",
    // POSIX / newlib / glibc / BSD extensions
    "strdup", "strndup", "strnlen", "memccpy", "mempcpy", "memmem", "memrchr",
    "strlcpy", "strlcat", "strtok_r", "strerror_r", "strsep", "stpcpy",
    "stpncpy", "strsignal", "strcasestr", "strchrnul", "posix_memalign",
    "memalign", "valloc", "pvalloc", "reallocarray", "setenv", "unsetenv",
    "putenv", "random", "srandom", "rand_r", "mkstemp", "mkdtemp", "realpath",
    "qsort_r", "explicit_bzero", "rawmemchr", "strverscmp", "strcoll_l",
    "strxfrm_l", "strerror_l", "secure_getenv", "clearenv", "mktemp", "mkostemp",
    "ecvt", "fcvt", "gcvt", "arc4random", "arc4random_buf",
    "arc4random_uniform", "getsubopt", "memset_explicit",
];

/// libc functions with only arithmetic parameters. ADL never reaches `fl` for
/// them, but `using namespace fl;` still makes a plain `fl::div(int, int)` tie
/// with `::div`. A template of any shape loses that tie, so these only have to
/// be templates (`fl::abs` is a generic `template <typename T>` math function).
const LIBC_ARITHMETIC_NAMES: &[&str] = &[
    "abs", "labs", "llabs", "div", "ldiv", "lldiv", "ffs", "ffsl", "ffsll",
    "imaxabs", "imaxdiv",
];
