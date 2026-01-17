from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
ESP32 Stack Trace Decoder

Decodes ESP32 crash dumps using addr2line. Can process:
- MEPC/RA registers from RISC-V crashes
- Backtrace addresses from esp_backtrace_print()
- Stack memory dumps

Usage:
    python decode_esp32_backtrace.py <elf_file> [--build-info <build_info.json>] <addresses...>

    Or pipe crash log:
    cat crash.log | python decode_esp32_backtrace.py <elf_file> [--build-info <build_info.json>]

Example:
    python decode_esp32_backtrace.py .pio/build/dev/firmware.elf 0x42002a3c 0x42001234
    python decode_esp32_backtrace.py firmware.elf --build-info build_info.json < crash.log
"""

import json
import re
import subprocess
import sys
from pathlib import Path
from typing import Any, Optional, cast


def find_addr2line_from_build_info(build_info_path: Path) -> Optional[Path]:
    """Find addr2line tool from build_info.json.

    Args:
        build_info_path: Path to build_info.json file

    Returns:
        Path to addr2line tool or None if not found
    """
    try:
        with open(build_info_path, "r") as f:
            build_info: dict[str, Any] = json.load(f)

        # Build info has structure: { "board_name": { "aliases": { "addr2line": "path" } } }
        # Get the first (and usually only) board entry
        for board_name, board_data in build_info.items():
            if not isinstance(board_data, dict):
                continue
            if "aliases" not in board_data:
                continue
            aliases = cast(dict[str, Any], board_data["aliases"])
            if not isinstance(aliases, dict):
                continue
            addr2line_path = cast(Optional[str], aliases.get("addr2line"))
            if addr2line_path and isinstance(addr2line_path, str):
                path = Path(addr2line_path)
                if path.exists():
                    print(
                        f"Using addr2line from build_info.json: {path}",
                        file=sys.stderr,
                    )
                    return path
                else:
                    print(
                        f"Warning: addr2line path from build_info.json does not exist: {path}",
                        file=sys.stderr,
                    )

        print("Warning: No addr2line path found in build_info.json", file=sys.stderr)
        return None

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(
            f"Warning: Failed to load addr2line from build_info.json: {e}",
            file=sys.stderr,
        )
        return None


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


def decode_addresses(
    elf_file: Path, addresses: list[str], build_info_path: Optional[Path] = None
) -> None:
    """Decode addresses using addr2line.

    Args:
        elf_file: Path to ELF file
        addresses: List of addresses to decode
        build_info_path: Optional path to build_info.json for finding addr2line
    """
    # Try build_info.json first if provided
    addr2line = None
    if build_info_path:
        addr2line = find_addr2line_from_build_info(build_info_path)

    # Fall back to default search
    if not addr2line:
        addr2line = find_addr2line()

    if not addr2line:
        print("Error: Could not find addr2line tool", file=sys.stderr)
        print("Make sure PlatformIO ESP32 toolchain is installed", file=sys.stderr)
        print("Or provide --build-info with path to build_info.json", file=sys.stderr)
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


def extract_addresses_from_crash_log(log: str) -> list[str]:
    """Extract addresses from ESP32 crash log."""
    addresses: list[str] = []

    # Extract MEPC (RISC-V program counter)
    mepc_match = re.search(r"MEPC\s*:\s*(0x[0-9a-fA-F]+)", log)
    if mepc_match:
        addresses.append(mepc_match.group(1))

    # Extract RA (return address)
    ra_match = re.search(r"RA\s*:\s*(0x[0-9a-fA-F]+)", log)
    if ra_match:
        addresses.append(ra_match.group(1))

    # Extract backtrace addresses from ESP32 memory regions:
    # 0x3C - Flash DROM, 0x3F - DRAM, 0x40 - IRAM, 0x42 - Flash IROM, 0x50 - RTC
    backtrace_pattern = r"0x(?:3[CF]|4[02]|50)[0-9a-fA-F]{6}"
    backtrace_addrs: list[str] = re.findall(backtrace_pattern, log)
    addresses.extend(backtrace_addrs[:10])  # Limit to first 10 addresses

    # Remove duplicates while preserving order
    seen: set[str] = set()
    unique_addresses: list[str] = []
    for addr in addresses:
        if addr not in seen:
            seen.add(addr)
            unique_addresses.append(addr)

    return unique_addresses


def main():
    import argparse

    parser = argparse.ArgumentParser(
        description="Decode ESP32 crash dumps using addr2line",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    %(prog)s firmware.elf 0x42002a3c 0x42001234
    %(prog)s firmware.elf --build-info build_info.json 0x42002a3c
    cat crash.log | %(prog)s firmware.elf --build-info build_info.json
        """,
    )
    parser.add_argument("elf_file", type=Path, help="Path to firmware.elf file")
    parser.add_argument(
        "--build-info",
        type=Path,
        help="Path to build_info.json for finding addr2line tool",
    )
    parser.add_argument(
        "addresses",
        nargs="*",
        help="Addresses to decode (or pipe crash log to stdin)",
    )

    args = parser.parse_args()

    # Check if addresses provided on command line
    if args.addresses:
        decode_addresses(args.elf_file, args.addresses, args.build_info)
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
        decode_addresses(args.elf_file, addresses, args.build_info)


if __name__ == "__main__":
    main()
