#!/usr/bin/env python3
"""
ESP32-S3 .bin-level bloat analysis.

Filters ELF symbols to only those in segments that ship in firmware.bin
(LOAD/PROGBITS sections), then attributes bytes to source areas via demangled
names + addr2line cross-reference.
"""

from __future__ import annotations

import json
import re
import subprocess
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path


TOOLCHAIN = Path(
    "/Users/zacharyvorhies/.platformio/packages/toolchain-xtensa-esp-elf/bin"
)
NM = TOOLCHAIN / "xtensa-esp32s3-elf-nm"
ADDR2LINE = TOOLCHAIN / "xtensa-esp32s3-elf-addr2line"
CXXFILT = TOOLCHAIN / "xtensa-esp32s3-elf-c++filt"
READELF = TOOLCHAIN / "xtensa-esp32s3-elf-readelf"

ELF = Path(
    "/Users/zacharyvorhies/dev/1/.build/pio/esp32s3/.fbuild/build/esp32s3/release/firmware.elf"
)


@dataclass
class Section:
    name: str
    addr: int
    size: int
    flags: str

    @property
    def end(self) -> int:
        return self.addr + self.size

    @property
    def is_loadable(self) -> bool:
        # Sections that contribute bytes to firmware.bin: PROGBITS with A (alloc) flag
        # AND non-zero size.
        return "A" in self.flags and self.size > 0


@dataclass
class Symbol:
    addr: int
    size: int
    type: str
    mangled: str
    demangled: str
    section: str | None = None
    source_file: str | None = None


def parse_sections() -> list[Section]:
    out = subprocess.check_output([str(READELF), "-SW", str(ELF)], text=True)
    sections: list[Section] = []
    for line in out.splitlines():
        m = re.match(
            r"\s*\[\s*\d+\]\s+(\S+)\s+\S+\s+([0-9a-f]+)\s+[0-9a-f]+\s+([0-9a-f]+)\s+\S+\s+([A-Za-z]*)",
            line,
        )
        if not m:
            continue
        name, addr_hex, size_hex, flags = m.groups()
        addr = int(addr_hex, 16)
        size = int(size_hex, 16)
        sections.append(Section(name=name, addr=addr, size=size, flags=flags))
    return sections


def loaded_section_for(addr: int, sections: list[Section]) -> Section | None:
    for s in sections:
        if not s.is_loadable:
            continue
        # PROGBITS only
        if "W" in s.flags and s.name.startswith(".dram0.dummy"):
            continue
        if s.addr <= addr < s.end:
            return s
    return None


def parse_nm_symbols() -> list[tuple[int, int, str, str]]:
    """Returns (addr, size, type, mangled_name)."""
    out = subprocess.check_output(
        [str(NM), "--print-size", "--size-sort", "--radix=d", str(ELF)],
        text=True,
    )
    syms: list[tuple[int, int, str, str]] = []
    for line in out.splitlines():
        parts = line.split(maxsplit=3)
        if len(parts) < 4:
            continue
        try:
            addr = int(parts[0])
            size = int(parts[1])
            t = parts[2]
            name = parts[3]
            syms.append((addr, size, t, name))
        except ValueError:
            continue
    return syms


def batch_demangle(names: list[str]) -> dict[str, str]:
    if not names:
        return {}
    proc = subprocess.run(
        [str(CXXFILT)],
        input="\n".join(names),
        capture_output=True,
        text=True,
        check=True,
    )
    out_lines = proc.stdout.splitlines()
    return dict(zip(names, out_lines))


def batch_addr2line(addrs: list[int]) -> dict[int, str]:
    if not addrs:
        return {}
    inp = "\n".join(f"0x{a:x}" for a in addrs)
    proc = subprocess.run(
        [str(ADDR2LINE), "-e", str(ELF), "-f", "-C", "-s"],
        input=inp,
        capture_output=True,
        text=True,
        check=True,
    )
    lines = proc.stdout.splitlines()
    # addr2line -f outputs two lines per addr: function, file:line
    result: dict[int, str] = {}
    for i, addr in enumerate(addrs):
        idx = i * 2 + 1
        if idx < len(lines):
            file_line = lines[idx].strip()
            result[addr] = file_line
    return result


# Categorization rules. Order matters: first match wins.
CATEGORIES: list[tuple[str, list[str]]] = [
    # --- FastLED Channel API (the suspected bloat source) ---
    (
        "FastLED::ChannelManager",
        [
            r"\bfl::ChannelManager\b",
            r"\bfl::Channel\b(?!Driver)(?!Engine)",
        ],
    ),
    (
        "FastLED::Channel/PARLIO",
        [
            r"\bfl::ChannelDriverPARLIO",
            r"\bfl::ChannelDriverParlio",
            r"\bfl::ParlioEngine",
            r"\bfl::PARLIO",
            r"\bfl::Parlio",
        ],
    ),
    (
        "FastLED::Channel/RMT",
        [
            r"\bfl::ChannelDriverRmt",
            r"\bfl::Rmt5",
            r"\bfl::ChannelEngineRMT",
            r"\bfl::RmtMemoryManager",
            r"\bfl::detail::Rmt",
        ],
    ),
    ("FastLED::Channel/RMT_RX", [r"\bfl::Rmt5Rx", r"\bfl::ChannelDriverRmtRx"]),
    (
        "FastLED::Channel/I2S",
        [
            r"\bfl::ChannelDriverI2S",
            r"\bfl::ChannelEngineI2S",
            r"\bfl::I2sLcdCam",
            r"\bfl::detail::I2s",
        ],
    ),
    ("FastLED::Channel/I2S_SPI", [r"\bfl::ChannelDriverI2SSpi", r"\bfl::I2sSpi"]),
    (
        "FastLED::Channel/LCD_SPI",
        [
            r"\bfl::ChannelDriverLcdSpi",
            r"\bfl::LcdSpi",
            r"\bfl::detail::LcdSpi",
            r"\bfl::ChannelDriverLcdClockless",
        ],
    ),
    ("FastLED::Channel/LCD_CAM", [r"\bfl::ChannelDriverLcdCam", r"\bfl::LcdCam"]),
    (
        "FastLED::Channel/SPI",
        [
            r"\bfl::ChannelDriverSpi(?!Hw)",
            r"\bfl::ChannelEngineSpi",
            r"\bfl::SpiChannel",
            r"\bfl::detail::SpiChannel",
            r"\bfl::SpiHwManager",
            r"\bfl::SpiHwBase",
            r"\bfl::SpiChannelEngineAdapter",
        ],
    ),
    (
        "FastLED::Channel/UART",
        [
            r"\bfl::ChannelDriverUart",
            r"\bfl::ChannelEngineUART",
            r"\bfl::UartPeripheral",
        ],
    ),
    ("FastLED::Channel/BLE", [r"\bfl::ChannelDriverBle", r"\bfl::Ble"]),
    (
        "FastLED::Channel/GPIO_ISR_RX",
        [r"\bfl::ChannelDriverGpioIsrRx", r"\bfl::GpioIsrRx"],
    ),
    ("FastLED::Channel/CLED", [r"\bfl::ChannelDriverCled", r"\bfl::Cled"]),
    ("FastLED::Channel/Platforms", [r"\bfl::platforms::"]),
    # --- FastLED async/coroutine machinery ---
    (
        "FastLED::Task/Coroutine",
        [
            r"\bfl::task\b",
            r"\bfl::Coroutine",
            r"\bfl::Promise",
            r"\bfl::Executor",
            r"\bfl::pumpCoroutines",
            r"\bfl::async\b",
        ],
    ),
    ("FastLED::Audio", [r"\bfl::audio\b", r"\bfl::AudioFFT", r"\bfl::AudioReactive"]),
    (
        "FastLED::Net/HTTP",
        [
            r"\bfl::Http",
            r"\bfl::WebSocket",
            r"\bfl::Url",
            r"\bfl::tcp",
            r"\bfl::Sock",
            r"\bfl::Net",
        ],
    ),
    (
        "FastLED::Pixel/Color",
        [
            r"\bfl::PixelController",
            r"\bfl::CRGB",
            r"\bfl::wave\d",
            r"\bfl::ColorPalette",
            r"\bfl::HsvToRgb",
        ],
    ),
    (
        "FastLED::Container",
        [
            r"\bfl::vector",
            r"\bfl::span",
            r"\bfl::shared_ptr",
            r"\bfl::weak_ptr",
            r"\bfl::unique_ptr",
            r"\bfl::function",
            r"\bfl::basic_string",
            r"\bfl::HashMap",
            r"\bfl::Singleton",
            r"\bfl::optional",
        ],
    ),
    ("FastLED::Other", [r"\bfl::"]),
    # --- ESP-IDF / Arduino Core ---
    (
        "ESP-IDF::WiFi",
        [r"^wifi_", r"\bwifi", r"^esp_wifi", r"^ieee80211", r"^net80211"],
    ),
    (
        "ESP-IDF::Bluetooth/BLE",
        [
            r"^bt_",
            r"\bble_",
            r"^esp_ble",
            r"^bluedroid",
            r"^l2cap",
            r"^gatts?_",
            r"^smp_",
            r"^attp_",
        ],
    ),
    (
        "ESP-IDF::LWIP/Network",
        [
            r"^lwip_",
            r"^tcp_",
            r"^udp_",
            r"^ip[46]?_",
            r"^netif_",
            r"^pbuf_",
            r"^sockets_",
            r"^dns_",
            r"^dhcp",
        ],
    ),
    (
        "ESP-IDF::FreeRTOS",
        [
            r"^vTask",
            r"^xTask",
            r"^pvTask",
            r"^uxTask",
            r"^xQueue",
            r"^vQueue",
            r"^xSemaphore",
            r"^xEvent",
            r"^prvIdle",
            r"^port_",
        ],
    ),
    (
        "ESP-IDF::ESPSystem",
        [r"^esp_", r"^xt_", r"^Cache_", r"^sys_", r"^startup_", r"^do_global_"],
    ),
    (
        "ESP-IDF::HAL/Driver",
        [
            r"^gpio_",
            r"^uart_",
            r"^spi_",
            r"^i2c_",
            r"^i2s_",
            r"^rmt_",
            r"^rtc_",
            r"^gdma_",
            r"^ledc_",
            r"^pcnt_",
            r"^mcpwm_",
            r"^twai_",
            r"^touch_",
            r"^adc_",
            r"^dac_",
            r"^esp_lcd",
            r"^lcd_",
            r"^parlio_",
        ],
    ),
    (
        "ESP-IDF::FlashStorage",
        [r"^spi_flash", r"^esp_partition", r"^nvs_", r"^esp_flash", r"^esp_image"],
    ),
    (
        "ESP-IDF::Crypto/mbedTLS",
        [r"^mbedtls_", r"^mpi_", r"^sha\d", r"^aes_", r"^esp_crypto"],
    ),
    (
        "Arduino::Core",
        [
            r"^Arduino",
            r"^_Z\w*Hardware",
            r"^Print\b",
            r"^Stream\b",
            r"^Wire\b",
            r"^Serial",
            r"^_Z\w*WiFi",
            r"^_Z\w*HardwareSerial",
        ],
    ),
    (
        "libc/libstdc++",
        [
            r"^_dtoa_",
            r"^_svfprintf",
            r"^_vfprintf",
            r"^_malloc",
            r"^_calloc",
            r"^memcpy",
            r"^memset",
            r"^strn?cpy",
            r"^printf",
            r"^puts$",
            r"^_Z\w*std",
            r"^__cxa",
            r"^operator new",
            r"^operator delete",
        ],
    ),
    ("Misc(other ESP-IDF)", [r"^\w+_init$", r"^\w+_register"]),
]

CAT_REGEX: list[tuple[str, list[re.Pattern[str]]]] = [
    (cat, [re.compile(p) for p in pats]) for cat, pats in CATEGORIES
]


def categorize(demangled: str, mangled: str) -> str:
    for cat, pats in CAT_REGEX:
        for p in pats:
            if p.search(demangled) or p.search(mangled):
                return cat
    return "Unknown"


def main() -> int:
    print("Loading sections...")
    sections = parse_sections()
    loadable = [s for s in sections if s.is_loadable]
    total_loaded = sum(s.size for s in loadable)
    print(
        f"  Found {len(loadable)} loadable sections, total loadable bytes = {total_loaded:,}"
    )
    for s in sorted(loadable, key=lambda x: -x.size):
        print(
            f"    {s.name:<22}  size={s.size:>8,}  addr=0x{s.addr:08x}  flags={s.flags}"
        )

    print("\nLoading nm symbols...")
    raw = parse_nm_symbols()
    print(f"  {len(raw):,} symbols with size from nm")

    print("Demangling...")
    mangled_names = list({m for _, _, _, m in raw})
    demangled_map = batch_demangle(mangled_names)

    print("Filtering to loadable-section symbols...")
    in_bin: list[Symbol] = []
    skipped = 0
    for addr, size, t, mangled in raw:
        sec = loaded_section_for(addr, sections)
        if sec is None:
            skipped += 1
            continue
        in_bin.append(
            Symbol(
                addr=addr,
                size=size,
                type=t,
                mangled=mangled,
                demangled=demangled_map.get(mangled, mangled),
                section=sec.name,
            )
        )
    print(
        f"  {len(in_bin):,} symbols in bin, {skipped:,} skipped (debug-only or unaddressed)"
    )

    bin_total = sum(s.size for s in in_bin)
    print(f"  Sum of nm symbol sizes in bin sections: {bin_total:,} bytes")

    # ===== By section =====
    print("\n=== Top symbols by SECTION (showing where bytes go) ===")
    by_sec: dict[str, int] = defaultdict(int)
    for s in in_bin:
        by_sec[s.section or "?"] += s.size
    for sec, size in sorted(by_sec.items(), key=lambda kv: -kv[1]):
        print(f"  {sec:<22} {size:>8,}")

    # ===== By category =====
    by_cat: dict[str, int] = defaultdict(int)
    by_cat_count: dict[str, int] = defaultdict(int)
    by_cat_section: dict[tuple[str, str], int] = defaultdict(int)
    cat_examples: dict[str, list[tuple[int, str]]] = defaultdict(list)

    for s in in_bin:
        cat = categorize(s.demangled, s.mangled)
        by_cat[cat] += s.size
        by_cat_count[cat] += 1
        by_cat_section[(cat, s.section or "?")] += s.size
        cat_examples[cat].append((s.size, s.demangled))

    print("\n=== Bloat by CATEGORY (sums of nm symbol sizes that ship in bin) ===")
    print(f"  {'CATEGORY':<32} {'BYTES':>10}  {'%bin':>6}  {'#syms':>6}")
    for cat, size in sorted(by_cat.items(), key=lambda kv: -kv[1]):
        pct = 100.0 * size / bin_total if bin_total else 0
        print(f"  {cat:<32} {size:>10,}  {pct:>5.1f}%  {by_cat_count[cat]:>6}")

    # FastLED detail breakdown
    print("\n=== FastLED-only detail (Channel API drivers, etc.) ===")
    fl_total = sum(v for k, v in by_cat.items() if k.startswith("FastLED"))
    print(f"  FastLED total (sum across FastLED:: categories): {fl_total:,} bytes")
    print(f"  {'CATEGORY':<32} {'BYTES':>10}  {'%fl':>6}")
    for cat, size in sorted(by_cat.items(), key=lambda kv: -kv[1]):
        if not cat.startswith("FastLED"):
            continue
        pct = 100.0 * size / fl_total if fl_total else 0
        print(f"  {cat:<32} {size:>10,}  {pct:>5.1f}%")

    # Top 30 symbols overall (in bin)
    print("\n=== Top 30 individual symbols in bin ===")
    print(f"  {'BYTES':>8}  {'SECTION':<18}  SYMBOL")
    for s in sorted(in_bin, key=lambda x: -x.size)[:30]:
        d = s.demangled.replace("fl::", "fl::").replace(", ", ",")
        if len(d) > 110:
            d = d[:107] + "..."
        print(f"  {s.size:>8,}  {s.section:<18}  {d}")

    # Top symbols per FastLED category
    print("\n=== Top 5 symbols per FastLED category ===")
    for cat in sorted(by_cat, key=lambda k: -by_cat[k]):
        if not cat.startswith("FastLED"):
            continue
        print(f"\n  [{cat}]  total={by_cat[cat]:,}")
        for size, name in sorted(cat_examples[cat], key=lambda kv: -kv[0])[:5]:
            n = name.replace(", ", ",")
            if len(n) > 110:
                n = n[:107] + "..."
            print(f"    {size:>8,}  {n}")

    # ESP-IDF top symbols (sanity check the categorization)
    print("\n=== Top 5 'Unknown' (suggests new category needed) ===")
    for size, name in sorted(cat_examples.get("Unknown", []), key=lambda kv: -kv[0])[
        :5
    ]:
        print(f"    {size:>8,}  {name}")

    # Save JSON
    out = {
        "bin_total_loaded": total_loaded,
        "nm_in_bin_total": bin_total,
        "by_section": dict(sorted(by_sec.items(), key=lambda kv: -kv[1])),
        "by_category": dict(sorted(by_cat.items(), key=lambda kv: -kv[1])),
        "fastled_total": fl_total,
        "top_symbols": [
            {"size": s.size, "section": s.section, "name": s.demangled}
            for s in sorted(in_bin, key=lambda x: -x.size)[:200]
        ],
    }
    Path("/tmp/bin_bloat.json").write_text(json.dumps(out, indent=2))
    print("\nSaved JSON to /tmp/bin_bloat.json")
    return 0


if __name__ == "__main__":
    sys.exit(main())
