#!/usr/bin/env python3
"""
Source-file attribution: groups every code symbol in firmware.bin to its
originating source file via addr2line, then sums bytes per file/directory.

This complements bin_bloat_analysis.py (which categorizes by name regex).
"""

from __future__ import annotations

import re
import subprocess
from collections import defaultdict
from pathlib import Path


TC = Path("/Users/zacharyvorhies/.platformio/packages/toolchain-xtensa-esp-elf/bin")
NM = TC / "xtensa-esp32s3-elf-nm"
ADDR2LINE = TC / "xtensa-esp32s3-elf-addr2line"
READELF = TC / "xtensa-esp32s3-elf-readelf"

ELF = Path(
    "/Users/zacharyvorhies/dev/1/.build/pio/esp32s3/.fbuild/build/esp32s3/release/firmware.elf"
)


def loadable_progbits_ranges() -> list[tuple[int, int, str]]:
    """Sections with LOAD flag — these are what get written into firmware.bin."""
    out_objdump = subprocess.check_output(
        [str(TC / "xtensa-esp32s3-elf-objdump"), "-h", str(ELF)], text=True
    )
    ranges: list[tuple[int, int, str]] = []
    lines = out_objdump.splitlines()
    i = 0
    while i < len(lines):
        m = re.match(
            r"\s*\d+\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+[0-9a-f]+\s+[0-9a-f]+",
            lines[i],
        )
        if m and i + 1 < len(lines):
            name, size_hex, vma_hex = m.groups()
            flags = lines[i + 1]
            size = int(size_hex, 16)
            addr = int(vma_hex, 16)
            if size > 0 and "LOAD" in flags:
                ranges.append((addr, addr + size, name))
            i += 2
        else:
            i += 1
    return ranges


def in_loaded(addr: int, ranges: list[tuple[int, int, str]]) -> str | None:
    for lo, hi, name in ranges:
        if lo <= addr < hi:
            return name
    return None


def parse_nm() -> list[tuple[int, int, str, str]]:
    out = subprocess.check_output(
        [str(NM), "--print-size", "--size-sort", "--radix=d", str(ELF)], text=True
    )
    syms: list[tuple[int, int, str, str]] = []
    for line in out.splitlines():
        parts = line.split(maxsplit=3)
        if len(parts) < 4:
            continue
        try:
            syms.append((int(parts[0]), int(parts[1]), parts[2], parts[3]))
        except ValueError:
            continue
    return syms


def addr2line_batch(addrs: list[int]) -> dict[int, str]:
    if not addrs:
        return {}
    inp = "\n".join(f"0x{a:x}" for a in addrs)
    proc = subprocess.run(
        [str(ADDR2LINE), "-e", str(ELF)],
        input=inp,
        capture_output=True,
        text=True,
        check=True,
    )
    out_lines = proc.stdout.splitlines()
    res: dict[int, str] = {}
    for a, ln in zip(addrs, out_lines):
        # strip line:col
        f = ln.strip()
        f = re.sub(r":\d+(?::\d+)?$", "", f)
        f = re.sub(r" \(discriminator \d+\)", "", f)
        res[a] = f
    return res


# Map filename → high-level category
FL_DRIVER_FILES = {
    "channel_driver_lcd_spi": "FL/Channel/LCD_SPI",
    "channel_driver_lcd_clockless": "FL/Channel/LCD_SPI",
    "lcd_spi": "FL/Channel/LCD_SPI",
    "channel_driver_rmt": "FL/Channel/RMT",
    "rmt_5": "FL/Channel/RMT",
    "rmt5": "FL/Channel/RMT",
    "rmt_memory_manager": "FL/Channel/RMT",
    "rmt_buffer_pool": "FL/Channel/RMT",
    "rmt_engine": "FL/Channel/RMT",
    "channel_driver_rmt_rx": "FL/Channel/RMT_RX",
    "rmt_rx": "FL/Channel/RMT_RX",
    "channel_driver_i2s": "FL/Channel/I2S",
    "i2s_lcd_cam": "FL/Channel/I2S",
    "channel_driver_i2s_spi": "FL/Channel/I2S_SPI",
    "i2s_spi": "FL/Channel/I2S_SPI",
    "channel_driver_uart": "FL/Channel/UART",
    "uart_peripheral": "FL/Channel/UART",
    "channel_driver_spi": "FL/Channel/SPI",
    "channel_engine_spi": "FL/Channel/SPI",
    "spi_channel": "FL/Channel/SPI",
    "spi_hw_manager": "FL/Channel/SPI",
    "channel_driver_parlio": "FL/Channel/PARLIO",
    "parlio_engine": "FL/Channel/PARLIO",
    "channel_driver_lcd_cam": "FL/Channel/LCD_CAM",
    "channel_driver_ble": "FL/Channel/BLE",
    "channel_driver_gpio_isr_rx": "FL/Channel/GPIO_ISR_RX",
    "channel_driver_cled": "FL/Channel/CLED",
    "channel.cpp": "FL/ChannelManager",
    "manager.cpp": "FL/ChannelManager",
    "channel_manager": "FL/ChannelManager",
    "_build.cpp": "FL/ChannelManager",
}


def categorize(file_str: str, mangled: str) -> str:
    f = file_str.lower()
    # Only attribute to FastLED if path is actually under FastLED's source tree
    is_fastled_path = (
        "/dev/1/src/" in f or "/fastled/src/" in f or "/fl/" in f or "fastled" in f
    )
    if is_fastled_path:
        for needle, cat in FL_DRIVER_FILES.items():
            if needle in f:
                return cat
    if is_fastled_path:
        if "/task/" in f or "task/executor" in f or "coroutine" in f:
            return "FL/Task"
        if "/audio/" in f:
            return "FL/Audio"
        if "/net/" in f or "/http/" in f or "websock" in f:
            return "FL/Net"
        if (
            "pixel" in f
            or "wave" in f
            or "color" in f
            or "gamma" in f
            or "lib8tion" in f
        ):
            return "FL/Pixel"
        if "/platforms/" in f and "esp32" in f:
            return "FL/Platforms-ESP32"
        return "FL/Other"
    if mangled.startswith(("_ZN2fl", "fl::")):
        # Sometimes addr2line gives '??' but mangled is fl::*
        return "FL/Other(no-debug-info)"
    # ESP-IDF and Arduino
    if "esp-idf" in f or "esp_idf" in f:
        return "ESP-IDF"
    if "arduino" in f:
        return "Arduino"
    if "freertos" in f:
        return "FreeRTOS"
    if "lwip" in f:
        return "LWIP"
    if "newlib" in f or "libc" in f or "gcc" in f:
        return "libc/libgcc"
    if f.startswith("??"):
        return "Unknown(no-debug)"
    return "Other"


def main() -> int:
    ranges = loadable_progbits_ranges()
    print("Loaded PROGBITS+ALLOC sections:")
    for lo, hi, n in ranges:
        print(f"  {n:<22} {hi - lo:>10,} bytes  [0x{lo:08x}-0x{hi:08x})")
    total = sum(hi - lo for lo, hi, _ in ranges)
    print(f"  TOTAL bin-shipped (PROGBITS+ALLOC): {total:,}")

    syms = parse_nm()
    in_bin = [(a, s, t, m) for a, s, t, m in syms if in_loaded(a, ranges)]
    print(f"\nnm symbols that fall in shipped sections: {len(in_bin):,}")
    print(f"Sum of nm symbol sizes: {sum(s for _, s, _, _ in in_bin):,}")

    # addr2line batch
    addrs = [a for a, _, _, _ in in_bin]
    print("Running addr2line for source attribution...")
    a2l = addr2line_batch(addrs)

    # Group by file
    by_cat: dict[str, int] = defaultdict(int)
    by_file: dict[str, int] = defaultdict(int)
    by_file_list: dict[str, list[tuple[int, str]]] = defaultdict(list)
    for a, sz, t, m in in_bin:
        f = a2l.get(a, "??")
        cat = categorize(f, m)
        by_cat[cat] += sz
        by_file[f] += sz
        by_file_list[f].append((sz, m))

    bin_attributed = sum(by_cat.values())
    print(f"\n=== Bytes by CATEGORY (source-file attribution) ===")
    print(f"  attributed total: {bin_attributed:,}")
    for cat, sz in sorted(by_cat.items(), key=lambda kv: -kv[1]):
        pct = 100.0 * sz / total
        print(f"  {cat:<28} {sz:>10,}  {pct:>5.1f}% of bin")

    print("\n=== Top 40 source files (by bytes contributed to bin) ===")
    for f, sz in sorted(by_file.items(), key=lambda kv: -kv[1])[:40]:
        print(f"  {sz:>8,}  {f}")

    # FL drivers detail
    print("\n=== Top FL/Channel/* files (driver-level breakdown) ===")
    fl_files = [
        (f, sz) for f, sz in by_file.items() if categorize(f, "").startswith("FL/")
    ]
    for f, sz in sorted(fl_files, key=lambda kv: -kv[1])[:30]:
        print(f"  {sz:>8,}  {f}  ({categorize(f, '')})")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
