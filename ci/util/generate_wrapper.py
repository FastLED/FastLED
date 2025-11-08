#!/usr/bin/env python3
"""
Generate wrapper .cpp files for Arduino sketch examples.
This script creates a simple wrapper that includes the .ino file and provides main().
"""

import sys


def main():
    if len(sys.argv) != 4:
        print(
            "Usage: generate_wrapper.py <output_file> <example_name> <ino_file>",
            file=sys.stderr,
        )
        sys.exit(1)

    output_file = sys.argv[1]
    example_name = sys.argv[2]
    ino_file = sys.argv[3]

    content = f"""// Auto-generated wrapper for example: {example_name}
// This file includes the Arduino sketch and provides main()

#include "{ino_file}"
#include "fl/stub_main.hpp"
"""

    with open(output_file, "w") as f:
        f.write(content)

    print(f"Generated wrapper: {output_file}")


if __name__ == "__main__":
    main()
