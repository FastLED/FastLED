# dataclasses

import json
from copy import deepcopy
from dataclasses import dataclass
from typing import Any, ClassVar, Dict, List, Set


# An open source version of the esp-idf 5.1 platform for the ESP32 that
# gives esp32 boards the same build environment as the Arduino 2.3.1+.

# Set to a specific release, we may want to update this in the future.
ESP32_IDF_5_1_PIOARDUINO = "https://github.com/pioarduino/platform-espressif32/releases/download/51.03.04/platform-espressif32.zip"

# TODO: Upgrade toolkit to 5.3
ESP32_IDF_5_3_PIOARDUINO = "https://github.com/pioarduino/platform-espressif32/releases/download/53.03.10/platform-espressif32.zip"
ESP32_IDF_5_4_PIOARDUINO = "https://github.com/pioarduino/platform-espressif32/releases/download/54.03.20/platform-espressif32.zip"
ESP32_IDF_5_5_PIOARDUINO = "https://github.com/pioarduino/platform-espressif32/releases/download/55.03.30-2/platform-espressif32.zip"
ESP32_IDF_LATEST_PIOARDUINO = ESP32_IDF_5_5_PIOARDUINO

ESP32_IDF_LATEST_PIOARDUINO = (
    "https://github.com/pioarduino/platform-espressif32.git#develop"
)
ESP32_IDF_4_4_LATEST = (
    "https://github.com/platformio/platform-espressif32/archive/refs/tags/v4.4.0.zip"
)
APOLLO3_2_2_0 = "https://github.com/nigelb/platform-apollo3blue"
# Top of trunk.
# ESP32_IDF_5_1_PIOARDUINO = "https://github.com/pioarduino/platform-espressif32"

# Old fork that we were using
# ESP32_IDF_5_1_PIOARDUINO = "https://github.com/zackees/platform-espressif32#Arduino/IDF5"

# ALL will be auto populated in the Board constructor whenever a
# board is defined.
ALL: List["Board"] = []


class Auto:
    pass


@dataclass
class Board:
    board_name: str
    real_board_name: str | None = None
    platform: str | None = None
    platform_needs_install: bool = False
    use_pio_run: bool = (
        False  # some platforms like esp32-c2-devkitm-1 will only work with pio run
    )
    platform_packages: str | None = None
    framework: str | None = None
    board_build_mcu: str | None = None
    board_build_core: str | None = None
    board_build_filesystem_size: str | None = None
    board_build_flash_size: str | None = (
        None  # Flash size for ESP32 boards (e.g., '4MB')
    )
    build_flags: list[str] | None = None  # Reserved for future use.
    defines: list[str] | None = None
    customsdk: str | None = None
    board_partitions: str | None = None  # Reserved for future use.
    no_board_spec: bool = (
        False  # For platforms like 'native' that don't need a board specification
    )
    add_board_to_all: bool = True
    lib_compat_mode: str | None = (
        None  # Library compatibility mode (e.g., 'off' for native platform)
    )
    lib_ldf_mode: str | None = (
        None  # Library Dependency Finder mode (e.g., 'chain+' for enhanced dependency finding)
    )
    lib_ignore: list[str] | None = (
        None  # Libraries to ignore during compilation (e.g., ['I2S'] for UNO R4 WiFi)
    )

    # Class-level mappings for platform/MCU classification (O(1) lookups)
    _LOW_MEMORY_BOARDS: ClassVar[Set[str]] = {
        # AVR ATmega
        "uno",  # ATmega328P
        "nano",  # ATmega328P
        "nano_every",  # ATmega4809
        "yun",  # ATmega32U4
        # AVR ATtiny
        "attiny85",  # __AVR_ATtiny85__
        "attiny88",  # __AVR_ATtiny88__
        "attiny4313",  # ARDUINO_attinyxy4
        "attiny1616",  # ARDUINO_attinyxy6
        # Teensy low-memory
        "teensylc",  # ARDUINO_TEENSYLC
        "teensy30",  # ARDUINO_TEENSY30
        "teensy31",  # __MK20DX256__ (256KB RAM)
        # STM32F1
        "bluepill",  # STM32F103C8
        "bluepill_f103cb",  # STM32F103CB
        "maple_mini",  # STM32F1
        "hy_tinystm103tb",  # STM32F1
        # ESP8266
        "esp8266",
        # Renesas UNO R4
        "uno_r4_wifi",
    }

    _LOW_MEMORY_PLATFORMS: ClassVar[Set[str]] = {
        "avr",  # All AVR boards
        "renesas-ra",  # Renesas UNO R4
    }

    _PLATFORM_PATTERNS: ClassVar[List[tuple[str, str]]] = [
        # (pattern, platform_family) - order matters, check most specific first
        ("espressif32", "esp32"),
        ("esp32", "esp32"),
        ("esp8266", "esp8266"),
        ("raspberrypi", "rp2040"),
        ("rp2040", "rp2040"),
        ("teensy", "teensy"),
        ("ststm32", "stm32"),
        ("stm32", "stm32"),
        ("atmelsam", "arm"),
        ("renesas-ra", "renesas"),
        ("renesas", "renesas"),
        ("nrf52", "nrf52"),
        ("atmelavr", "avr"),
        ("avr", "avr"),
        ("native", "native"),
        ("stub", "native"),
    ]

    _MCU_PATTERNS: ClassVar[List[tuple[str, str]]] = [
        # (board_name_pattern, mcu_target) - most specific patterns first
        # ESP32 variants
        ("esp32_p4", "ESP32P4"),
        ("esp32p4", "ESP32P4"),
        ("esp32s3", "ESP32S3"),
        ("esp32_s3", "ESP32S3"),
        ("esp32s2", "ESP32S2"),
        ("esp32_s2", "ESP32S2"),
        ("esp32c6", "ESP32C6"),
        ("esp32_c6", "ESP32C6"),
        ("esp32c5", "ESP32C5"),
        ("esp32_c5", "ESP32C5"),
        ("esp32c3", "ESP32C3"),
        ("esp32_c3", "ESP32C3"),
        ("esp32c2", "ESP32C2"),
        ("esp32_c2", "ESP32C2"),
        ("esp32h2", "ESP32H2"),
        ("esp32_h2", "ESP32H2"),
        ("esp32", "ESP32"),
        # ESP8266
        ("esp8266", "ESP8266"),
        # Teensy variants
        ("teensy41", "Teensy41"),
        ("teensy40", "Teensy40"),
        ("teensy31", "Teensy31"),
        ("teensy30", "Teensy30"),
        ("teensy_lc", "TeensyLC"),
        ("teensylc", "TeensyLC"),
        # STM32 variants
        ("bluepill_f103cb", "STM32F103CB"),
        ("bluepill", "STM32F103C8"),
        ("stm32f1", "STM32F1"),
        # AVR variants
        ("uno", "ATmega328P"),
        ("nano", "ATmega328P"),
        ("nano_every", "ATmega4809"),
        ("attiny85", "ATtiny85"),
        ("attiny88", "ATtiny88"),
        ("attiny4313", "ATtiny4313"),
        ("attiny1616", "ATtiny1616"),
        # RP2040 variants
        ("rp2350", "RP2350"),
        ("rp2040", "RP2040"),
        ("pico2", "RP2350"),
        ("rpi_pico2", "RP2350"),
        ("xrp", "RP2350"),
        ("rpipico2", "RP2350"),
        ("pico", "RP2040"),
        ("rpi_pico", "RP2040"),
        ("rpipico", "RP2040"),
        # ARM variants
        ("due", "ATSAM3X8E"),
        ("giga_r1", "STM32H747"),
        # NRF52 variants
        ("nrf52840", "NRF52840"),
        # Renesas
        ("uno_r4_wifi", "RA4M1"),
        ("uno_r4", "RA4M1"),
    ]

    _BOARD_TO_PLATFORM_FAMILY: ClassVar[Dict[str, str]] = {
        # Direct board name to platform family mapping (fastest for exact matches)
        "esp32dev": "esp32",
        "esp32c2": "esp32",
        "esp32c3": "esp32",
        "esp32c5": "esp32",
        "esp32c6": "esp32",
        "esp32s2": "esp32",
        "esp32s3": "esp32",
        "esp32h2": "esp32",
        "esp32p4": "esp32",
        "esp8266": "esp8266",
        "uno": "avr",
        "nano": "avr",
        "nano_every": "avr",
        "attiny85": "avr",
        "attiny88": "avr",
        "attiny4313": "avr",
        "attiny1616": "avr",
        "yun": "avr",
        "teensylc": "teensy",
        "teensy30": "teensy",
        "teensy31": "teensy",
        "teensy40": "teensy",
        "teensy41": "teensy",
        "bluepill": "stm32",
        "bluepill_f103cb": "stm32",
        "maple_mini": "stm32",
        "hy_tinystm103tb": "stm32",
        "blackpill": "stm32",
        "rpipico": "rp",
        "rpipico2": "rp",
        "rp2040": "rp",
        "rp2350": "rp",
        "due": "arm",
        "digix": "arm",
        "zero": "arm",
        "adafruit_feather_m0": "arm",
        "adafruit_feather_m4": "arm",
        "adafruit_grand_central_m4": "arm",
        "giga_r1": "arm",
        "nrf52840_dk": "nrf52",
        "adafruit_feather_nrf52840_sense": "nrf52",
        "xiaoblesense": "nrf52",
        "uno_r4_wifi": "renesas",
        "native": "native",
        "web": "native",
    }

    def __post_init__(self) -> None:
        # Check if framework is set, warn and auto-set to arduino if missing (except for native/stub platforms)
        if self.framework is None and not self._is_native_or_stub_platform():
            self.framework = "arduino"

        # Auto-detect and exclude problematic libraries based on environmental signals
        self._auto_detect_library_exclusions()

        ALL.append(self)

    def _is_native_or_stub_platform(self) -> bool:
        """Check if this is a native or stub platform that doesn't need a framework"""
        # Check for native platform
        if self.platform and (
            "native" in self.platform.lower() or "stub" in self.platform.lower()
        ):
            return True

        # Check for stub build flags
        if self.build_flags:
            for flag in self.build_flags:
                if "FASTLED_STUB" in flag or "PLATFORM_NATIVE" in flag:
                    return True

        # Check no_board_spec flag (typically used for native platforms)
        if self.no_board_spec:
            return True

        return False

    def _auto_detect_library_exclusions(self) -> None:
        """Automatically detect and exclude libraries known to be broken on this platform."""

        # Check for I2S library problems based on environmental signals
        if self._should_ignore_i2s_library():
            if not self.lib_ignore:
                self.lib_ignore = []
            if "I2S" not in self.lib_ignore:
                self.lib_ignore.append("I2S")

    def _should_ignore_i2s_library(self) -> bool:
        """Detect if I2S library should be ignored based on platform characteristics."""

        # Known problematic platforms - Renesas RA family
        if self.platform == "renesas-ra":
            return True

        # Known problematic board names
        problematic_boards = ["uno_r4_wifi", "uno_r4_minima"]
        if self.board_name in problematic_boards:
            return True

        # Check for specific defines that indicate I2S problems
        if self.defines:
            problematic_defines = [
                "ARDUINO_UNOR4_WIFI",
                "ARDUINO_UNOR4_MINIMA",
                "ARDUINO_ARCH_RENESAS",
                "ARDUINO_ARCH_RENESAS_UNO",
                "_RENESAS_RA_",
                "ARDUINO_FSP",
            ]
            for define in self.defines:
                # Check if define contains any problematic patterns
                if any(prob_define in define for prob_define in problematic_defines):
                    return True

        return False

    @property
    def memory_class(self) -> str:
        """Return memory class for this board: 'low' or 'high'.

        Used by @filter directives to determine platform memory constraints.
        Returns 'low' for memory-constrained platforms, 'high' for others.

        Memory classification based on C++ sketch_macros.h SKETCH_HAS_LOTS_OF_MEMORY:
        - Low-memory: AVR, STM32F1, ESP8266, Teensy LC/3.0/3.1, Renesas UNO R4
        - High-memory: ESP32 family, RP2040, ARM (Due, Feather), Teensy 4.x, NRF52
        """
        # Check board name first (O(1) set lookup, highest priority)
        if self.board_name in self._LOW_MEMORY_BOARDS:
            return "low"

        # Check platform family (O(1) set lookup)
        if self.platform:
            platform_lower = self.platform.lower()
            for low_mem_platform in self._LOW_MEMORY_PLATFORMS:
                if low_mem_platform in platform_lower:
                    return "low"

        # Check for specific MCU indicators
        if self.board_build_mcu:
            mcu_lower = self.board_build_mcu.lower()
            if any(low_mcu in mcu_lower for low_mcu in ["avr", "stm32f1", "atmega328"]):
                return "low"

        # Default to high-memory for most modern boards
        return "high"

    @property
    def platform_family(self) -> str:
        """Return platform family for this board.

        Used by @filter directives for platform-level filtering.
        Examples: 'esp32', 'esp8266', 'avr', 'rp2040', 'teensy', 'stm32', 'arm', 'native'
        """
        if not self.board_name and not self.platform:
            return "unknown"

        # Try exact board name lookup first (O(1) dict lookup)
        board_lower = self.board_name.lower()
        if board_lower in self._BOARD_TO_PLATFORM_FAMILY:
            return self._BOARD_TO_PLATFORM_FAMILY[board_lower]

        # Check platform string with pattern matching
        if self.platform:
            platform_lower = self.platform.lower()
            for pattern, family in self._PLATFORM_PATTERNS:
                if pattern in platform_lower:
                    return family

        # Fallback: check board name patterns
        for pattern, family in self._PLATFORM_PATTERNS:
            if pattern in board_lower:
                return family

        # Default fallback
        return "unknown"

    def get_mcu_target(self) -> str | None:
        """Return specific MCU target for this board (e.g., 'ESP32S3', 'ATmega328P').

        Used by @filter directives for MCU-specific filtering.
        Returns None if target cannot be determined.
        """
        board_lower = self.board_name.lower()

        # Check patterns in order (most specific first)
        for pattern, target in self._MCU_PATTERNS:
            if pattern in board_lower:
                return target

        # No specific target for unknown boards
        return None

    def clone(self) -> "Board":
        out = Board(
            board_name=self.board_name,
            add_board_to_all=False,
        )
        for field_name, field_info in self.__dataclass_fields__.items():
            field_value: Any = getattr(self, field_name)
            # Create deep copy for mutable types to avoid shared references
            if isinstance(field_value, (list, dict)):
                field_value = deepcopy(field_value)  # type: ignore[misc]
            setattr(out, field_name, field_value)
        return out

    def get_real_board_name(self) -> str:
        return self.real_board_name if self.real_board_name else self.board_name

    def to_dictionary(self) -> dict[str, list[str]]:
        out: dict[str, list[str]] = {}
        if self.real_board_name:
            out[self.board_name] = [f"board={self.real_board_name}"]
        options = out.setdefault(self.board_name, [])

        if self.platform:
            options.append(f"platform={self.platform}")

        if self.platform_needs_install:
            options.append("platform_needs_install=true")
        if self.platform_packages:
            options.append(f"platform_packages={self.platform_packages}")
        if self.framework:
            options.append(f"framework={self.framework}")
        if self.board_build_core:
            options.append(f"board_build.core={self.board_build_core}")
        if self.board_build_mcu:
            options.append(f"board_build.mcu={self.board_build_mcu}")
        if self.board_build_filesystem_size:
            options.append(
                f"board_build.filesystem_size={self.board_build_filesystem_size}"
            )
        if self.board_build_flash_size:
            options.append(f"board_build.flash_size={self.board_build_flash_size}")
        if self.defines:
            for define in self.defines:
                options.append(f"build_flags=-D{define}")

        # Handle explicit build_flags (added for native host compilation and other special cases)
        if self.build_flags:
            for bf in self.build_flags:
                options.append(f"build_flags={bf}")

        if self.customsdk:
            options.append(f"custom_sdkconfig={self.customsdk}")

        # Add board-specific build cache directory pointing via symlink directive
        # here = Path(__file__).parent
        # project_root = here.parent.parent  # Move from ci/util/ to project root
        # cache_dir = project_root / ".pio_cache" / self.board_name
        # absolute_cache_dir = cache_dir.resolve()
        # options.append(f"build_cache_dir=symlink://{absolute_cache_dir}")

        return out

    def __repr__(self) -> str:
        json_str = json.dumps(self.to_dictionary(), indent=4, sort_keys=True)
        return json_str

    def __hash__(self) -> int:
        data_str = self.__repr__()
        return hash(data_str)

    def to_platformio_ini(
        self,
        additional_defines: list[str] | None = None,
        additional_include_dirs: list[str] | None = None,
        additional_libs: list[str] | None = None,
        include_platformio_section: bool = False,
        core_dir: str | None = None,
        packages_dir: str | None = None,
        project_root: str | None = None,
        build_cache_dir: str | None = None,
        extra_scripts: list[str] | None = None,
    ) -> str:
        """Return a `platformio.ini` snippet representing this board.

        The output is suitable for directly appending to a *platformio.ini* file
        and follows the same semantics used by the PlatformIO CLI.  Only
        parameters understood by PlatformIO are emitted – internal helper
        fields like ``platform_needs_install`` and ``use_pio_run`` are **not**
        included because they are consumed exclusively by the build helpers in
        the *ci/* folder and would be ignored (or flagged as errors) by
        PlatformIO itself.

        Args:
            example: Example name for dynamic build flags (currently unused)
            additional_defines: Additional defines to merge with board defines
            additional_include_dirs: Additional include directories to merge with build flags
            include_platformio_section: Whether to include [platformio] section with core_dir/packages_dir
            core_dir: PlatformIO core directory path
            packages_dir: PlatformIO packages directory path
            project_root: FastLED project root for lib_deps symlink
        """
        lines: list[str] = []

        # Optional [platformio] section
        if include_platformio_section:
            lines.append("[platformio]")
            if build_cache_dir:
                lines.append(f"build_cache_dir = {build_cache_dir}")
            if core_dir:
                lines.append(f"core_dir = {core_dir}")
            if packages_dir:
                lines.append(f"packages_dir = {packages_dir}")
            lines.append("")

        # Section header
        lines.append(f"[env:{self.board_name}]")
        if extra_scripts:
            lines.append(f"extra_scripts = {' '.join(extra_scripts)}")

        # Board identifier (skip for platforms that don't need board specification)
        if not self.no_board_spec:
            lines.append(f"board = {self.get_real_board_name()}")

        # Optional parameters -------------------------------------------------
        if self.platform:
            lines.append(f"platform = {self.platform}")

        if self.platform_packages:
            lines.append(f"platform_packages = {self.platform_packages}")

        if self.framework:
            lines.append(f"framework = {self.framework}")

        if self.board_build_core:
            lines.append(f"board_build.core = {self.board_build_core}")

        if self.board_build_mcu:
            lines.append(f"board_build.mcu = {self.board_build_mcu}")

        if self.board_build_filesystem_size:
            lines.append(
                f"board_build.filesystem_size = {self.board_build_filesystem_size}"
            )

        if self.board_build_flash_size:
            lines.append(f"board_build.flash_size = {self.board_build_flash_size}")
            # Also set upload flash size to override board defaults
            lines.append(f"board_upload.flash_size = {self.board_build_flash_size}")

        # Force DIO flash mode for ESP32 boards to ensure QEMU compatibility
        # QEMU doesn't support QIO flash mode which requires setting the QIE bit
        if self.board_name.startswith("esp32") or (
            self.real_board_name and self.real_board_name.startswith("esp32")
        ):
            lines.append("board_build.flash_mode = dio")
            lines.append("board_upload.flash_mode = dio")

        if self.board_partitions:
            lines.append(f"board_build.partitions = {self.board_partitions}")

        # Build-time flags -------------------------------------------------
        build_flags_elements: list[str] = []
        if self.defines:
            build_flags_elements.extend(f"-D{define}" for define in self.defines)

        # Add additional defines
        if additional_defines:
            build_flags_elements.extend(f"-D{define}" for define in additional_defines)

        # Use static build flags
        if self.build_flags:
            build_flags_elements.extend(self.build_flags)

        # Add additional include directories
        if additional_include_dirs:
            build_flags_elements.extend(
                f"-I{include_dir}" for include_dir in additional_include_dirs
            )

        if build_flags_elements:
            # Put each build flag on a separate line for better readability and debugging
            lines.append("build_flags =")
            for flag in build_flags_elements:
                lines.append(f"    {flag}")

        # Custom ESP-IDF sdkconfig override (ESP32-family boards)
        if self.customsdk:
            lines.append(f"custom_sdkconfig = {self.customsdk}")

        # Library compatibility mode (for platforms like native that need special handling)
        if self.lib_compat_mode:
            lines.append(f"lib_compat_mode = {self.lib_compat_mode}")

        # Library Dependency Finder mode (for enhanced dependency finding)
        if self.lib_ldf_mode:
            lines.append(f"lib_ldf_mode = {self.lib_ldf_mode}")

        # Libraries to ignore during compilation
        if self.lib_ignore:
            if len(self.lib_ignore) == 1:
                lines.append(f"lib_ignore = {self.lib_ignore[0]}")
            else:
                lines.append("lib_ignore =")
                for lib in self.lib_ignore:
                    lines.append(f"    {lib}")

        # Add FastLED-specific configurations if project_root is provided
        if project_root:
            # Only add default lib_ldf_mode if board doesn't specify its own
            current_config = "\n".join(lines)
            if "lib_ldf_mode" not in current_config:
                lines.append("lib_ldf_mode = chain")
            lines.append("lib_archive = true")

            # Build lib_deps with additional libs only (FastLED is copied to lib/FastLED)
            # PlatformIO supports multiple lib_deps entries on separate lines
            if additional_libs:
                # Format as multi-line lib_deps for proper PlatformIO parsing
                if len(additional_libs) == 1:
                    lines.append(f"lib_deps = {additional_libs[0]}")
                else:
                    lines.append("lib_deps =")
                    for entry in additional_libs:
                        lines.append(f"    {entry}")

        return "\n".join(lines) + "\n"


# [env:sparkfun_xrp_controller]
# platform = https://github.com/maxgerhardt/platform-raspberrypi
# board = sparkfun_xrp_controller
# framework = arduino
# lib_deps = fastled/FastLED @ ^3.9.16


WEBTARGET = Board(
    board_name="web",
)

# Native host compilation target using PlatformIO's "native" platform.
# This allows compiling FastLED for the host machine (Linux/macOS/Windows)
# which is useful for CI compile-tests and static analysis.  We replicate
# the build flags present in ci/native/platformio.ini so that the same
# stub implementation and main-file inclusion are used.

NATIVE = Board(
    board_name="native",
    platform="platformio/native",
    no_board_spec=True,  # Native platform doesn't need a board specification
    lib_compat_mode="off",  # Disable library compatibility checking for native platform
    build_flags=[
        "-DFASTLED_STUB_IMPL",  # Enable stub platform implementations
        "-DFASTLED_USE_STUB_ARDUINO",  # Enable Arduino stub implementations
        "-DPLATFORM_NATIVE",  # Enable native platform stub compilation
        "-std=c++17",
        "-I../../../src/platforms/stub",  # Include path for Arduino.h and other stub headers (relative to project dir)
        "-I../../../src",  # Include path for FastLED.h and other source headers (relative to project dir)
    ],
)

DUE = Board(
    board_name="due",
    platform="atmelsam",
)


SPARKFUN_XRP_CONTROLLER_2350B = Board(
    board_name="sparkfun_xrp_controller",
    platform="https://github.com/maxgerhardt/platform-raspberrypi",
    platform_needs_install=True,
)

APOLLO3_RED_BOARD = Board(
    board_name="apollo3_red",
    real_board_name="SparkFun_RedBoard_Artemis_ATP",
    platform=APOLLO3_2_2_0,
    platform_packages="framework-arduinoapollo3@https://github.com/sparkfun/Arduino_Apollo3#v2.2.0",
    platform_needs_install=True,
)

APOLLO3_SPARKFUN_THING_PLUS_EXPLORABLE = Board(
    board_name="apollo3_thing_explorable",
    real_board_name="SparkFun_Thing_Plus_expLoRaBLE",
    platform=APOLLO3_2_2_0,
    platform_packages="framework-arduinoapollo3@https://github.com/sparkfun/Arduino_Apollo3#v2.2.0",
    platform_needs_install=True,
)

ESP32DEV = Board(
    board_name="esp32dev",
    platform=ESP32_IDF_5_3_PIOARDUINO,
)

# TODO: esp32dev_qemu remove when possible, we don't want an extra board definition unless we need it
# Specialized board configuration optimized for QEMU testing
ESP32DEV_QEMU = Board(
    board_name="esp32dev_qemu",
    real_board_name="esp32dev",
    platform=ESP32_IDF_5_3_PIOARDUINO,
    build_flags=[
        # Essential QEMU compatibility - disable PSRAM
        "-DBOARD_HAS_PSRAM=false",
        "-DARDUINO_BOARD_HAS_PSRAM=false",
        # Ensure UART0 is used for serial output
        "-DARDUINO_USB_CDC_ON_BOOT=0",
        # Basic debug level for serial output
        "-DCORE_DEBUG_LEVEL=1",
    ],
    defines=[
        "FASTLED_ESP32_FLASH_LOCK=0",  # Disable flash locking for QEMU
        "QEMU_BUILD=1",  # Flag to indicate QEMU build
    ],
)

ESP32DEV_IDF3_3 = Board(
    board_name="esp32dev_idf33",
    real_board_name="esp32dev",
    platform="espressif32@1.11.2",
)

ESP32DEV_IDF4_4 = Board(
    board_name="esp32dev_idf44",
    real_board_name="esp32dev",
    platform=ESP32_IDF_4_4_LATEST,
)

GIGA_R1 = Board(
    board_name="giga_r1",
    platform="ststm32",
    framework="arduino",
    real_board_name="giga_r1_m7",
)

# ESP8266 = Board(
#     board_name="esp8266",
#     platform=ESP32_IDF_5_1_PIOARDUINO,
# )

# ESP32-C2: Use Arduino framework only (not "arduino, espidf")
# The dual framework mode causes PlatformIO to use ESP-IDF's component-based build system,
# which does not automatically discover and compile .cpp files in example subdirectories
# (e.g., examples/Codec/codec_processor.cpp, examples/Downscale/src/xypaths.cpp).
# This resulted in linking errors with "undefined reference" to functions defined in those files.
# Arduino-only mode uses PlatformIO's standard source discovery which correctly compiles
# all .cpp files copied to the build directory.
# See: GitHub Actions run 18448215424 - ESP32-C2 linking failures for Codec, Downscale, FxWave2d
ESP32_C2_DEVKITM_1 = Board(
    board_name="esp32c2",
    real_board_name="esp32-c2-devkitm-1",
    platform="https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip",
    framework="arduino",  # IMPORTANT: Do not add "espidf" - see comment above
)

ESP32_C3_DEVKITM_1 = Board(
    board_name="esp32c3",
    real_board_name="esp32-c3-devkitm-1",
    platform=ESP32_IDF_5_3_PIOARDUINO,
)

ESP32_C5_DEVKITC_1 = Board(
    board_name="esp32c5",
    real_board_name="esp32-c5-devkitc-1",
    platform=ESP32_IDF_5_5_PIOARDUINO,
)

ESP32_C6_DEVKITC_1 = Board(
    board_name="esp32c6",
    real_board_name="esp32-c6-devkitc-1",
    platform=ESP32_IDF_5_3_PIOARDUINO,
)

ESP32_S3_DEVKITC_1 = Board(
    board_name="esp32s3",
    real_board_name="esp32-s3-devkitc-1",
    platform=ESP32_IDF_5_4_PIOARDUINO,
    framework="arduino",
    board_build_flash_size="4MB",  # Set to 4MB for QEMU compatibility (default is 8MB)
    board_partitions="default.csv",  # Use default 4MB partition table for QEMU compatibility
)

ESP32_S2_DEVKITM_1 = Board(
    board_name="esp32s2",
    real_board_name="lolin_s2_mini ",
    board_build_mcu="esp32s2",
    platform=ESP32_IDF_5_3_PIOARDUINO,
)

ESP32_UPESY_WROOM = Board(
    board_name="upesy_wroom",
    real_board_name="upesy_wroom",
    platform="espressif32",
)

ESP32H2 = Board(
    board_name="esp32h2",
    real_board_name="esp32-h2-devkitm-1",
    platform_needs_install=True,  # Install platform package to get the boards
    platform=ESP32_IDF_5_3_PIOARDUINO,
)

ESP32_P4 = Board(
    board_name="esp32p4",
    real_board_name="esp32-p4-evboard",
    platform_needs_install=True,  # Install platform package to get the boards
    platform="https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip",
    # Use default framework (arduino,espidf) without specifying one explicitly
    # The pioarduino platform has known issues with ESP32-P4 when using Arduino-only mode
    # See: platform-espressif32 builder/frameworks/arduino.py has unhandled None values for FRAMEWORK_DIR and FRAMEWORK_LIB_DIR
)

ADA_FEATHER_NRF52840_SENSE = Board(
    board_name="adafruit_feather_nrf52840_sense",
    platform="nordicnrf52",
    defines=[
        "FASTLED_ALL_PINS_HARDWARE_SPI",
        "FORCE_ALL_PINS_VALID",
    ],
)

XIAOBLESENSE_ADAFRUIT_NRF52 = Board(
    board_name="xiaoblesense_adafruit",
    platform="https://github.com/maxgerhardt/platform-nordicnrf52",
    platform_needs_install=True,  # Install platform package to get the boards
    defines=[
        "FASTLED_ALL_PINS_HARDWARE_SPI",
        "FASTLED_NRF52_SUPPRESS_UNTESTED_BOARD_WARNING",
    ],
)

# Alias: handle common misspelling without the trailing 't'
XIAOBLESENSE_ADAFRUI_ALIAS = Board(
    board_name="xiaoblesense_adafrui",  # missing 't'
    real_board_name="xiaoblesense_adafruit",  # map to the correct board name
    platform="https://github.com/maxgerhardt/platform-nordicnrf52",
    platform_needs_install=True,
    defines=[
        "FASTLED_ALL_PINS_HARDWARE_SPI",
        "FASTLED_NRF52_SUPPRESS_UNTESTED_BOARD_WARNING",
    ],
)

XIAOBLESENSE_NRF52 = Board(
    board_name="xiaoblesense",
    real_board_name="xiaoble_adafruit",
    platform="https://github.com/maxgerhardt/platform-nordicnrf52",
    platform_needs_install=True,
    defines=[
        "FASTLED_ALL_PINS_HARDWARE_SPI",
    ],
)

# Correct nRF52840 DK board definition
# The Nordic nRF52840 DK is directly supported by the default PlatformIO
# `nordicnrf52` platform under the board name `nrf52840_dk`, so we don't
# need a custom platform package or extra installation steps.  Point the
# Board definition at the stock platform and use the canonical board name.
# This fixes compilation failures introduced during the build-system
# migration where the board was temporarily mapped to the XIAO variant.
NRF52840 = Board(
    board_name="nrf52840_dk",
    real_board_name="nrf52840_dk_adafruit",  # Use Adafruit BSP variant which includes full Nordic SDK headers
    platform="nordicnrf52",
    framework="arduino",
    platform_needs_install=False,
    platform_packages="framework-arduinoadafruitnrf52@^1.10601.0",
    defines=[
        "FASTLED_USE_COMPILE_TESTS=0",
        "FASTLED_ALL_PINS_HARDWARE_SPI",
    ],
    board_build_core="nRF5",  # Ensure correct core directory
)

RPI_PICO = Board(
    board_name="rpipico",
    platform="https://github.com/maxgerhardt/platform-raspberrypi.git",
    platform_needs_install=True,  # Install platform package to get the boards
    platform_packages="framework-arduinopico@https://github.com/earlephilhower/arduino-pico.git",
    framework="arduino",
    board_build_core="earlephilhower",
    board_build_filesystem_size="0.5m",
)

RPI_PICO2 = Board(
    board_name="rpipico2",
    real_board_name="rpipico",  # Use the existing Pico board definition until PlatformIO adds native Pico 2 support
    platform="https://github.com/maxgerhardt/platform-raspberrypi.git",
    platform_needs_install=True,  # Install platform package to get the boards
    platform_packages="framework-arduinopico@https://github.com/earlephilhower/arduino-pico.git",
    framework="arduino",
    board_build_core="earlephilhower",
    board_build_filesystem_size="0.5m",
)

# Aliases for new RPXXXX naming convention (board_name -> new standardized name mapping)
# These maintain backward compatibility with existing workflows and CI configurations
RP2040 = Board(
    board_name="rp2040",
    real_board_name="rpipico",
    platform="https://github.com/maxgerhardt/platform-raspberrypi.git",
    platform_needs_install=True,
    platform_packages="framework-arduinopico@https://github.com/earlephilhower/arduino-pico.git",
    framework="arduino",
    board_build_core="earlephilhower",
    board_build_filesystem_size="0.5m",
)

RP2350 = Board(
    board_name="rp2350",
    real_board_name="rpipico2",
    platform="https://github.com/maxgerhardt/platform-raspberrypi.git",
    platform_needs_install=True,
    platform_packages="framework-arduinopico@https://github.com/earlephilhower/arduino-pico.git",
    framework="arduino",
    board_build_core="earlephilhower",
    board_build_filesystem_size="0.5m",
)

BLUEPILL = Board(
    board_name="bluepill",
    real_board_name="bluepill_f103c8",
    platform="ststm32",
)

# STM32F103CB variant with 128KB flash (double the C8's 64KB)
# Fully compatible with BluePill F103C8 but with more flash memory
# Supports Roger Clark STM32 core and STM32duino
BLUEPILL_F103CB = Board(
    board_name="bluepill_f103cb",
    real_board_name="bluepill_f103cb",
    platform="ststm32",
)

# maple_mini_b20
MAPLE_MINI = Board(
    board_name="maple_mini",
    real_board_name="maple_mini_b20",
    platform="ststm32",
)

HY_TINYSTM103TB = Board(
    board_name="hy_tinystm103tb",
    platform="ststm32",
)

ATTINY88 = Board(
    board_name="attiny88",
    platform="atmelavr",
)

# ATtiny1604
ATTINY1604 = Board(
    board_name="attiny1604",
    platform="atmelmegaavr",
)

# attiny4313
ATTINY4313 = Board(
    board_name="attiny4313",
    platform="atmelavr",
)

ATTINY1616 = Board(
    board_name="attiny1616",
    platform="atmelmegaavr",
)

UNO_R4_WIFI = Board(
    board_name="uno_r4_wifi",
    platform="renesas-ra",
    # I2S library auto-excluded due to missing r_i2s_api.h header in Arduino Renesas framework
)

NANO_EVERY = Board(
    board_name="nano_every",
    platform="atmelmegaavr",
)

ESP32DEV_I2S = Board(
    board_name="esp32dev_i2s",
    real_board_name="esp32dev",
    platform=ESP32_IDF_4_4_LATEST,
)

ESP32S3_RMT51 = Board(
    board_name="esp32rmt_51",
    real_board_name="esp32-s3-devkitc-1",
    platform_needs_install=True,
    platform=ESP32_IDF_5_3_PIOARDUINO,
    defines=[
        "FASTLED_RMT5=1",
    ],
)

# Teensy boards
TEENSY_LC = Board(
    board_name="teensylc",
    platform="teensy",
    framework="arduino",
)

TEENSY30 = Board(
    board_name="teensy30",
    platform="teensy",
    framework="arduino",
)

TEENSY31 = Board(
    board_name="teensy31",
    platform="teensy",
    framework="arduino",
)

TEENSY40 = Board(
    board_name="teensy40",
    platform="teensy",
    framework="arduino",
)

TEENSY41 = Board(
    board_name="teensy41",
    platform="teensy",
    framework="arduino",
)

# Basic Arduino boards
UNO = Board(
    board_name="uno",
    platform="atmelavr",
    framework="arduino",
)

YUN = Board(
    board_name="yun",
    platform="atmelavr",
    framework="arduino",
)

DIGIX = Board(
    board_name="digix",
    real_board_name="due",  # Digix is Arduino Due compatible
    platform="atmelsam",
    framework="arduino",
)

# ESP8266 boards
ESP8266 = Board(
    board_name="esp8266",
    real_board_name="esp01",
    platform="espressif8266",
    framework="arduino",
)

# ATtiny boards
ATTINY85 = Board(
    board_name="attiny85",
    platform="atmelavr",
    framework="arduino",
)

# Seeed XIAO ESP32S3 board – same platform
XIAO_ESP32S3 = Board(
    board_name="seeed_xiao_esp32s3",
    real_board_name="seeed_xiao_esp32s3",
    platform=ESP32_IDF_5_4_PIOARDUINO,
    board_build_flash_size="4MB",  # Set to 4MB for QEMU compatibility (default is 8MB)
    board_partitions="default.csv",  # Use default 4MB partition table for QEMU compatibility
)

# STM32F4 Black Pill board - addresses GitHub issue #726
BLACKPILL = Board(
    board_name="blackpill",
    real_board_name="blackpill_f411ce",
    platform="ststm32",
)

# Silicon Labs MGM240S boards (Arduino Nano Matter, SparkFun Thing Plus Matter)
# Uses Silicon Labs EFM32 platform with Arduino framework support
# Based on EFR32MG24 SoC with ARM Cortex-M33 @ 78MHz
MGM240S = Board(
    board_name="mgm240",
    real_board_name="sparkfun_thingplusmatter",
    platform="https://github.com/maxgerhardt/platform-siliconlabsefm32/archive/refs/heads/silabs-arduino.zip",
    platform_needs_install=True,
    framework="arduino",
)

# SAMD21 boards (Cortex-M0+ @ 48 MHz)
ADAFRUIT_FEATHER_M0 = Board(
    board_name="adafruit_feather_m0",
    platform="atmelsam",
    framework="arduino",
)

ARDUINO_ZERO = Board(
    board_name="zero",
    real_board_name="zeroUSB",
    platform="atmelsam",
    framework="arduino",
)

# SAMD51 boards (Cortex-M4F @ 120 MHz)
ADAFRUIT_FEATHER_M4 = Board(
    board_name="adafruit_feather_m4",
    platform="atmelsam",
    framework="arduino",
    lib_ignore=["I2S"],  # I2S library has SAMD51 compatibility issues
    defines=[
        "FASTLED_USES_ARDUINO_AUDIO_INPUT=0",  # Disable Arduino audio (I2S not available)
    ],
)

ADAFRUIT_GRAND_CENTRAL_M4 = Board(
    board_name="adafruit_grand_central_m4",
    platform="atmelsam",
    framework="arduino",
    lib_ignore=["I2S"],  # I2S library has SAMD51 compatibility issues
    defines=[
        "FASTLED_USES_ARDUINO_AUDIO_INPUT=0",  # Disable Arduino audio (I2S not available)
    ],
)


def _make_board_map(boards: list[Board]) -> dict[str, Board]:
    # make board map, but assert on duplicate board names
    board_map: dict[str, Board] = {}
    for board in boards:
        assert board.board_name not in board_map, (
            f"Duplicate board name: {board.board_name}"
        )
        board_map[board.board_name] = board
    return board_map


_BOARD_MAP: dict[str, Board] = _make_board_map(ALL)


def create_board(board_name: str, no_project_options: bool = False) -> Board:
    if no_project_options:
        # Create a minimal board without project options
        board = Board(board_name=board_name, add_board_to_all=False)
    else:
        # Try exact match first, then case-insensitive match
        if board_name in _BOARD_MAP:
            board = _BOARD_MAP[board_name]
        else:
            # Try case-insensitive match
            board_name_lower = board_name.lower()
            found_board = None
            for registered_name, registered_board in _BOARD_MAP.items():
                if registered_name.lower() == board_name_lower:
                    found_board = registered_board
                    break

            if found_board:
                board = found_board
            else:
                # Empty board without any special overrides, assume platformio will know what to do with it
                board = Board(board_name=board_name, add_board_to_all=False)
    return board.clone()
