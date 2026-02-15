// Demo of new fl::printf features: octal, width, and flags
#include "fl/stl/ostream.h"
#include "fl/stl/stdio.h"
#include "fl/stl/string.h"

int main() {
    char buf[128];

    fl::cout << "=== New fl::printf Features Demo ===" << fl::endl << fl::endl;

    // Octal format
    fl::cout << "1. Octal Format (%o):" << fl::endl;
    fl::snprintf(buf, sizeof(buf), "   8 in octal: %o", 8);
    fl::cout << "   " << buf << fl::endl;
    fl::snprintf(buf, sizeof(buf), "   64 in octal: %o", 64);
    fl::cout << "   " << buf << fl::endl;
    fl::snprintf(buf, sizeof(buf), "   255 in octal: %o", 255);
    fl::cout << "   " << buf << fl::endl;
    fl::cout << fl::endl;

    // Width specifier
    fl::cout << "2. Width Specifier:" << fl::endl;
    fl::snprintf(buf, sizeof(buf), "   Right-aligned: [%5d]", 42);
    fl::cout << "   " << buf << fl::endl;
    fl::snprintf(buf, sizeof(buf), "   String width: [%10s]", "test");
    fl::cout << "   " << buf << fl::endl;
    fl::cout << fl::endl;

    // Zero-padding (most important for embedded)
    fl::cout << "3. Zero-Padding (Critical for hex formatting):" << fl::endl;
    fl::snprintf(buf, sizeof(buf), "   Hex byte: %02x", 0x0F);
    fl::cout << "   " << buf << fl::endl;
    fl::snprintf(buf, sizeof(buf), "   Uppercase: %02X", 0x0F);
    fl::cout << "   " << buf << fl::endl;
    fl::snprintf(buf, sizeof(buf), "   4-digit hex: %04x", 0x12);
    fl::cout << "   " << buf << fl::endl;
    fl::snprintf(buf, sizeof(buf), "   8-digit hex: %08x", 0xDEADBEEF);
    fl::cout << "   " << buf << fl::endl;
    fl::snprintf(buf, sizeof(buf), "   5-digit decimal: %05d", 42);
    fl::cout << "   " << buf << fl::endl;
    fl::cout << fl::endl;

    // Left-align
    fl::cout << "4. Left-Align Flag (-):" << fl::endl;
    fl::snprintf(buf, sizeof(buf), "   Left-aligned: [%-5d]", 42);
    fl::cout << "   " << buf << fl::endl;
    fl::snprintf(buf, sizeof(buf), "   Left string: [%-10s]", "test");
    fl::cout << "   " << buf << fl::endl;
    fl::cout << fl::endl;

    // Plus sign
    fl::cout << "5. Plus Sign Flag (+):" << fl::endl;
    fl::snprintf(buf, sizeof(buf), "   Positive: %+d", 42);
    fl::cout << "   " << buf << fl::endl;
    fl::snprintf(buf, sizeof(buf), "   Negative: %+d", -42);
    fl::cout << "   " << buf << fl::endl;
    fl::snprintf(buf, sizeof(buf), "   Zero: %+d", 0);
    fl::cout << "   " << buf << fl::endl;
    fl::cout << fl::endl;

    // Space flag
    fl::cout << "6. Space Flag ( ):" << fl::endl;
    fl::snprintf(buf, sizeof(buf), "   Positive: [% d]", 42);
    fl::cout << "   " << buf << fl::endl;
    fl::snprintf(buf, sizeof(buf), "   Negative: [% d]", -42);
    fl::cout << "   " << buf << fl::endl;
    fl::cout << fl::endl;

    // Alternate form
    fl::cout << "7. Alternate Form Flag (#):" << fl::endl;
    fl::snprintf(buf, sizeof(buf), "   Hex with 0x: %#x", 0x2A);
    fl::cout << "   " << buf << fl::endl;
    fl::snprintf(buf, sizeof(buf), "   Uppercase: %#X", 0x2A);
    fl::cout << "   " << buf << fl::endl;
    fl::snprintf(buf, sizeof(buf), "   Octal with 0: %#o", 8);
    fl::cout << "   " << buf << fl::endl;
    fl::snprintf(buf, sizeof(buf), "   Zero (no prefix): %#x", 0);
    fl::cout << "   " << buf << fl::endl;
    fl::cout << fl::endl;

    // Combined features
    fl::cout << "8. Combined Features:" << fl::endl;
    fl::snprintf(buf, sizeof(buf), "   Hash + zero-pad: %#06x", 0x2A);
    fl::cout << "   " << buf << fl::endl;
    fl::snprintf(buf, sizeof(buf), "   Plus + width: %+5d", 42);
    fl::cout << "   " << buf << fl::endl;
    fl::snprintf(buf, sizeof(buf), "   Left + width: [%-8d]", 42);
    fl::cout << "   " << buf << fl::endl;
    fl::snprintf(buf, sizeof(buf), "   Long unsigned: %08lx", 0xDEADBEEFUL);
    fl::cout << "   " << buf << fl::endl;
    fl::cout << fl::endl;

    // Real-world example: MD5/hash formatting
    fl::cout << "9. Real-World Example (MD5 Hash Formatting):" << fl::endl;
    unsigned char hash[16] = {
        0x5d, 0x41, 0x40, 0x2a, 0xbc, 0x4b, 0x2a, 0x76,
        0xb9, 0x71, 0x9d, 0x91, 0x10, 0x17, 0xc5, 0x92
    };
    fl::string hash_str;
    for (int i = 0; i < 16; i++) {
        fl::snprintf(buf, sizeof(buf), "%02x", hash[i]);
        hash_str += buf;
    }
    fl::cout << "   MD5: " << hash_str.c_str() << fl::endl;

    return 0;
}
