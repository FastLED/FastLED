// Quick test to see what %lu produces
#include "fl/stl/stdio.h"
#include "fl/stl/iostream.h"
#include "fl/stl/cstdio.h"

int main() {
    char buf[128];

    // Test %lu
    fl::snprintf(buf, sizeof(buf), "Value: %lu", 12345UL);
    fl::cout << "Result: '" << buf << "'" << fl::endl;

    // Test %llu
    fl::snprintf(buf, sizeof(buf), "Value: %llu", 18446744073709551615ULL);
    fl::cout << "Result: '" << buf << "'" << fl::endl;

    // Test %u for comparison
    fl::snprintf(buf, sizeof(buf), "Value: %u", 12345U);
    fl::cout << "Result: '" << buf << "'" << fl::endl;

    return 0;
}
