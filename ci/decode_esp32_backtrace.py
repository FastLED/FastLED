#!/usr/bin/env python3
"""
ESP32 Stack Trace Decoder

Decodes ESP32 crash dumps using addr2line. Can process:
- MEPC/RA registers from RISC-V crashes
- Backtrace addresses from esp_backtrace_print()
- Stack memory dumps

Usage:
    python decode_esp32_backtrace.py <elf_file> <addresses...>

    Or pipe crash log:
    cat crash.log | python decode_esp32_backtrace.py <elf_file>

Example:
    python decode_esp32_backtrace.py .pio/build/dev/firmware.elf 0x42002a3c 0x42001234
"""

import re
import subprocess
import sys
from pathlib import Path
from typing import List, Optional


def find_addr2line() -> Optional[Path]:
    """Find the ESP32 addr2line tool."""
    # Common PlatformIO toolchain locations
    home = Path.home()

    # Try RISC-V (ESP32-C3, C6, etc.)
    riscv_paths = [
        home
        / ".platformio/packages/toolchain-riscv32-esp/bin/riscv32-esp-elf-addr2line",
        home
        / ".platformio/packages/toolchain-riscv32-esp/bin/riscv32-esp-elf-addr2line.exe",
    ]

    for path in riscv_paths:
        if path.exists():
            return path

    # Try Xtensa (ESP32, ESP32-S2, ESP32-S3)
    xtensa_paths = [
        home
        / ".platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-addr2line",
        home
        / ".platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-addr2line.exe",
        home
        / ".platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-addr2line",
        home
        / ".platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-addr2line.exe",
    ]

    for path in xtensa_paths:
        if path.exists():
            return path

    return None


def decode_addresses(elf_file: Path, addresses: List[str]) -> None:
    """Decode addresses using addr2line."""
    addr2line = find_addr2line()
    if not addr2line:
        print("Error: Could not find addr2line tool", file=sys.stderr)
        print("Make sure PlatformIO ESP32 toolchain is installed", file=sys.stderr)
        return

    if not elf_file.exists():
        print(f"Error: ELF file not found: {elf_file}", file=sys.stderr)
        return

    if not addresses:
        print("No addresses to decode", file=sys.stderr)
        return

    # Run addr2line
    cmd = [str(addr2line), "-e", str(elf_file), "-f", "-C"] + addresses

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        print("\n=== Decoded Stack Trace ===")

        lines = result.stdout.strip().split("\n")
        for i in range(0, len(lines), 2):
            if i + 1 < len(lines):
                func = lines[i]
                loc = lines[i + 1]
                addr = addresses[i // 2] if i // 2 < len(addresses) else "?"
                print(f"{addr}: {func}")
                print(f"         at {loc}")

    except subprocess.CalledProcessError as e:
        print(f"Error running addr2line: {e}", file=sys.stderr)


def extract_addresses_from_crash_log(log: str) -> List[str]:
    """Extract addresses from ESP32 crash log."""
    addresses: List[str] = []

    # Extract MEPC (RISC-V program counter)
    mepc_match = re.search(r"MEPC\s*:\s*(0x[0-9a-fA-F]+)", log)
    if mepc_match:
        addresses.append(mepc_match.group(1))

    # Extract RA (return address)
    ra_match = re.search(r"RA\s*:\s*(0x[0-9a-fA-F]+)", log)
    if ra_match:
        addresses.append(ra_match.group(1))

    # Extract backtrace addresses (0x42xxxxxx pattern for ESP32-C3)
    backtrace_pattern = r"0x[4][0-9a-fA-F]{7}"
    backtrace_addrs: List[str] = re.findall(backtrace_pattern, log)
    addresses.extend(backtrace_addrs[:10])  # Limit to first 10 addresses

    # Remove duplicates while preserving order
    seen: set[str] = set()
    unique_addresses: List[str] = []
    for addr in addresses:
        if addr not in seen:
            seen.add(addr)
            unique_addresses.append(addr)

    return unique_addresses


def main():
    if len(sys.argv) < 2:
        print(
            "Usage: decode_esp32_backtrace.py <elf_file> [addresses...]",
            file=sys.stderr,
        )
        print(
            "   or: cat crash.log | decode_esp32_backtrace.py <elf_file>",
            file=sys.stderr,
        )
        sys.exit(1)

    elf_file = Path(sys.argv[1])

    # Check if addresses provided on command line
    if len(sys.argv) > 2:
        addresses = sys.argv[2:]
        decode_addresses(elf_file, addresses)
    else:
        # Read from stdin
        if sys.stdin.isatty():
            print("Error: No addresses provided and stdin is empty", file=sys.stderr)
            print(
                "Provide addresses as arguments or pipe crash log to stdin",
                file=sys.stderr,
            )
            sys.exit(1)

        log = sys.stdin.read()
        addresses = extract_addresses_from_crash_log(log)

        if not addresses:
            print("Error: No addresses found in crash log", file=sys.stderr)
            sys.exit(1)

        print(f"Found {len(addresses)} addresses to decode")
        decode_addresses(elf_file, addresses)


if __name__ == "__main__":
    main()
