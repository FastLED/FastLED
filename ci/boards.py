# dataclasses

import json
from copy import deepcopy
from dataclasses import dataclass
from typing import Any


# An open source version of the esp-idf 5.1 platform for the ESP32 that
# gives esp32 boards the same build environment as the Arduino 2.3.1+.

# Set to a specific release, we may want to update this in the future.
ESP32_IDF_5_1_PIOARDUINO = "https://github.com/pioarduino/platform-espressif32/releases/download/51.03.04/platform-espressif32.zip"

# TODO: Upgrade toolkit to 5.3
ESP32_IDF_5_3_PIOARDUINO = "https://github.com/pioarduino/platform-espressif32/releases/download/53.03.10/platform-espressif32.zip"
ESP32_IDF_5_4_PIOARDUINO = "https://github.com/pioarduino/platform-espressif32/releases/download/54.03.20/platform-espressif32.zip"
ESP32_IDF_5_5_PIOARDUINO = "https://github.com/pioarduino/platform-espressif32/releases/download/55.03.34/platform-espressif32.zip"
ESP32_IDF_5_5_1_PIOARDUINO = "https://github.com/pioarduino/platform-espressif32/releases/download/55.03.35/platform-espressif32.zip"
ESP32_IDF_LATEST_PIOARDUINO = ESP32_IDF_5_5_1_PIOARDUINO

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
ALL: list["Board"] = []


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
    build_unflags: list[str] | None = None  # New: unflag options
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
    extra_scripts: list[str] | None = (
        None  # Custom build scripts to run (e.g., ['pre:script.py'] for pre-build hooks)
    )

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

        # Handle build_unflags
        if self.build_unflags:
            for uf in self.build_unflags:
                options.append(f"build_unflags={uf}")

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

    @property
    def memory_class(self) -> str:
        """Return memory classification: 'low' or 'high'.

        Based on sketch_macros.h SKETCH_HAS_LOTS_OF_MEMORY definitions.
        Low-memory boards include AVR, Teensy LC/3.0/3.1, STM32F1, ESP8266, Renesas UNO R4.
        """
        # Low-memory board list (matches sketch_macros.h)
        low_memory_boards = {
            "uno",
            "nano",
            "nano_every",
            "leonardo",
            "attiny85",
            "attiny88",
            "attiny1604",
            "attiny4313",
            "attiny1616",
            "teensylc",
            "teensy30",
            "teensy31",
            "stm32f103c8",
            "stm32f103cb",
            "stm32f103tb",
            "esp8266",
            "uno_r4_wifi",
            "uno_r4_minima",
        }

        # Low-memory platform families
        low_memory_platforms = {
            "atmelavr",  # AVR classic
            "atmelmegaavr",  # AVR modern (mega)
            "renesas-ra",  # Renesas (UNO R4)
        }

        # Check board name first
        if self.board_name in low_memory_boards:
            return "low"

        # Check platform
        if self.platform and self.platform in low_memory_platforms:
            return "low"

        # Default to high memory
        return "high"

    @property
    def platform_family(self) -> str:
        """Return simplified platform family name for filtering.

        Maps PlatformIO platform names to simplified family names like 'avr', 'esp32', 'teensy'.
        """
        if not self.platform:
            return "unknown"

        platform_lower = self.platform.lower()

        # Extract platform family from various URL formats
        if "atmelavr" in platform_lower or "atmelmegaavr" in platform_lower:
            return "avr"
        elif "espressif32" in platform_lower or "esp32" in platform_lower:
            return "esp32"
        elif "espressif8266" in platform_lower or "esp8266" in platform_lower:
            return "esp8266"
        elif "teensy" in platform_lower:
            return "teensy"
        elif "ststm32" in platform_lower or "stm32" in platform_lower:
            return "stm32"
        elif "atmelsam" in platform_lower or "sam" in platform_lower:
            return "sam"
        elif "raspberrypi" in platform_lower or "rp2040" in platform_lower:
            return "rp"
        elif "nordicnrf52" in platform_lower or "nrf52" in platform_lower:
            return "nrf52"
        elif "renesas" in platform_lower:
            return "renesas"
        elif "native" in platform_lower:
            return "native"
        elif "apollo" in platform_lower:
            return "apollo3"
        elif "siliconlabs" in platform_lower or "efm32" in platform_lower:
            return "efm32"
        else:
            # Return platform as-is if no mapping found
            return self.platform

    def get_mcu_target(self) -> str | None:
        """Return the MCU target identifier for filtering.

        Returns board_build_mcu if set, otherwise derives from board name or platform.
        """
        # Use explicit board_build_mcu if set
        if self.board_build_mcu:
            return self.board_build_mcu.upper()

        # Derive from board name for common patterns
        board_lower = self.board_name.lower()

        # AVR boards
        if board_lower == "uno" or board_lower == "nano":
            return "ATMEGA328P"
        elif board_lower == "leonardo" or "leonardo" in board_lower:
            return "ATMEGA32U4"
        elif board_lower.startswith("attiny"):
            return self.board_name.upper()  # ATtiny85, ATtiny88, etc.
        elif board_lower == "nano_every":
            return "ATMEGA4809"

        # ESP32 boards
        elif board_lower.startswith("esp32"):
            if "s3" in board_lower:
                return "ESP32S3"
            elif "s2" in board_lower:
                return "ESP32S2"
            elif "c3" in board_lower:
                return "ESP32C3"
            elif "c2" in board_lower:
                return "ESP32C2"
            elif "c5" in board_lower:
                return "ESP32C5"
            elif "c6" in board_lower:
                return "ESP32C6"
            elif "h2" in board_lower:
                return "ESP32H2"
            elif "p4" in board_lower:
                return "ESP32P4"
            else:
                return "ESP32"

        # ESP8266
        elif "esp8266" in board_lower or "esp01" in board_lower:
            return "ESP8266"

        # Teensy boards
        elif board_lower == "teensylc":
            return "MKL26Z64"
        elif board_lower == "teensy30":
            return "MK20DX128"
        elif board_lower == "teensy31":
            return "MK20DX256"
        elif board_lower == "teensy40":
            return "IMXRT1062"
        elif board_lower == "teensy41":
            return "IMXRT1062"

        # STM32 boards
        elif "stm32f103c8" in board_lower:
            return "STM32F103C8"
        elif "stm32f103cb" in board_lower:
            return "STM32F103CB"
        elif "stm32f103tb" in board_lower:
            return "STM32F103TB"
        elif "stm32f411ce" in board_lower:
            return "STM32F411CE"
        elif "stm32h747xi" in board_lower:
            return "STM32H747XI"

        # SAM boards
        elif "sam3x8e_due" in board_lower:
            return "SAM3X8E"
        elif "samd21g18a" in board_lower:
            return "SAMD21G18A"
        elif "samd51j19a" in board_lower:
            return "SAMD51J19A"
        elif "samd51p20a" in board_lower:
            return "SAMD51P20A"

        # RP2040/RP2350
        elif "pico" in board_lower:
            if "2" in board_lower:
                return "RP2350"
            return "RP2040"

        # NRF52 boards
        elif "nrf52" in board_lower:
            return "NRF52840"

        # UNO R4
        elif "uno_r4" in board_lower:
            return "RA4M1"

        # Return None if we can't determine the target
        return None

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

        # Merge board's extra_scripts with parameter extra_scripts
        all_extra_scripts: list[str] = []
        if self.extra_scripts:
            all_extra_scripts.extend(self.extra_scripts)
        if extra_scripts:
            all_extra_scripts.extend(extra_scripts)
        if all_extra_scripts:
            lines.append(f"extra_scripts = {' '.join(all_extra_scripts)}")

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

        # Build-time flags and unflags ---------------------------------------
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

        if self.build_unflags:
            # PlatformIO accepts multiple *build_unflags* separated by spaces.
            # Emit a single line for readability.
            lines.append(f"build_unflags = {' '.join(self.build_unflags)}")

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
    build_unflags=[
        "-mwindows",  # Remove GUI subsystem flag if present
        "-Wl,-Map,firmware.map",  # Remove GCC/ld map file flag (incompatible with zig's lld linker)
        "-fopt-info-all",  # Remove GCC-only optimization report flag (incompatible with Clang)
    ],
    extra_scripts=[
        "pre:../../../ci/native/zig_toolchain_override.py",  # Override toolchain with zig's Clang (runs BEFORE library compilation - CRITICAL for ABI compatibility)
    ],
)

SAM3X8E_DUE = Board(
    board_name="sam3x8e_due",
    real_board_name="due",
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

STM32H747XI_GIGA = Board(
    board_name="stm32h747xi",
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
    platform=ESP32_IDF_5_5_1_PIOARDUINO,
)

ESP32_C6_DEVKITC_1 = Board(
    board_name="esp32c6",
    real_board_name="esp32-c6-devkitc-1",
    platform=ESP32_IDF_5_5_PIOARDUINO,
    board_build_flash_size="4MB",  # ESP32-C6FH4 actual flash size confirmed by esptool
)

ESP32_S3_DEVKITC_1 = Board(
    board_name="esp32s3",
    real_board_name="esp32-s3-devkitc-1",
    platform=ESP32_IDF_5_4_PIOARDUINO,
    framework="arduino",
    board_build_flash_size="4MB",  # Set to 4MB for QEMU compatibility (default is 8MB)
    board_partitions="default.csv",  # Use default 4MB partition table for QEMU compatibility
    build_unflags=["-DFASTLED_RMT5=0", "-DFASTLED_RMT5"],
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
)

ADA_FEATHER_NRF52840_SENSE = Board(
    board_name="adafruit_feather_nrf52840_sense",
    platform="nordicnrf52",
)

XIAOBLESENSE_ADAFRUIT_NRF52 = Board(
    board_name="xiaoblesense_adafruit",
    platform="https://github.com/maxgerhardt/platform-nordicnrf52",
    platform_needs_install=True,  # Install platform package to get the boards
)

# Alias: handle common misspelling without the trailing 't'
XIAOBLESENSE_ADAFRUI_ALIAS = Board(
    board_name="xiaoblesense_adafrui",  # missing 't'
    real_board_name="xiaoblesense_adafruit",  # map to the correct board name
    platform="https://github.com/maxgerhardt/platform-nordicnrf52",
    platform_needs_install=True,
)

XIAOBLESENSE_NRF52 = Board(
    board_name="xiaoblesense",
    real_board_name="xiaoble_adafruit",
    platform="https://github.com/maxgerhardt/platform-nordicnrf52",
    platform_needs_install=True,
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
    ],
    board_build_core="nRF5",  # Ensure correct core directory
)

RPI_PICO = Board(
    board_name="rp2040",
    real_board_name="rpipico",
    platform="https://github.com/maxgerhardt/platform-raspberrypi.git",
    platform_needs_install=True,  # Install platform package to get the boards
    platform_packages="framework-arduinopico@https://github.com/earlephilhower/arduino-pico.git",
    framework="arduino",
    board_build_core="earlephilhower",
    board_build_filesystem_size="0.5m",
)

RPI_PICO2 = Board(
    board_name="rp2350",
    real_board_name="rpipico2",
    platform="https://github.com/maxgerhardt/platform-raspberrypi.git",
    platform_needs_install=True,  # Install platform package to get the boards
    platform_packages="framework-arduinopico@https://github.com/earlephilhower/arduino-pico.git",
    framework="arduino",
    board_build_core="earlephilhower",
    board_build_filesystem_size="0.5m",
)

STM32F103C8_BLUEPILL = Board(
    board_name="stm32f103c8",
    real_board_name="bluepill_f103c8",
    platform="ststm32",
)

# maple_mini_b20
STM32F103CB_MAPLEMINI = Board(
    board_name="stm32f103cb",
    real_board_name="maple_mini_b20",
    platform="ststm32",
)

STM32F103TB_TINYSTM = Board(
    board_name="stm32f103tb",
    real_board_name="hy_tinystm103tb",
    platform="ststm32",
)

ATTINY88 = Board(
    board_name="attiny88",
    platform="atmelavr",
)

# ATtiny1604
ATTINY1604 = Board(
    board_name="ATtiny1604",
    platform="atmelmegaavr",
)

# attiny4313
ATTINY4313 = Board(
    board_name="attiny4313",
    platform="atmelavr",
)

ATTINY1616 = Board(
    board_name="ATtiny1616",
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

ATMEGA32U4_LEONARDO = Board(
    board_name="leonardo",
    real_board_name="leonardo",
    platform="atmelavr",
    framework="arduino",
)

ATMEGA8A = Board(
    board_name="atmega8a",
    real_board_name="ATmega8",  # PlatformIO board ID (supports ATmega8/A)
    platform="atmelavr",
    framework="arduino",
    board_build_mcu="atmega8a",  # Override to specifically target ATmega8A
    lib_ignore=[
        "SoftwareSerial"
    ],  # ATmega8A lacks PCMSK registers needed by SoftwareSerial
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

# Seeed XIAO ESP32S3 board – same platform, needs FASTLED_RMT5 macro removal
XIAO_ESP32S3 = Board(
    board_name="seeed_xiao_esp32s3",
    real_board_name="seeed_xiao_esp32s3",
    platform=ESP32_IDF_5_4_PIOARDUINO,
    board_build_flash_size="4MB",  # Set to 4MB for QEMU compatibility (default is 8MB)
    board_partitions="default.csv",  # Use default 4MB partition table for QEMU compatibility
    defines=None,
    build_unflags=["-DFASTLED_RMT5=0", "-DFASTLED_RMT5"],
)

# STM32F4 Black Pill board - addresses GitHub issue #726
STM32F411CE_BLACKPILL = Board(
    board_name="stm32f411ce",
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
SAMD21G18A_FEATHER = Board(
    board_name="samd21",
    real_board_name="adafruit_feather_m0",
    platform="atmelsam",
    framework="arduino",
)

SAMD21G18A_ZERO = Board(
    board_name="samd21_zero",
    real_board_name="zeroUSB",
    platform="atmelsam",
    framework="arduino",
)

# SAMD51 boards (Cortex-M4F @ 120 MHz)
SAMD51J19A_FEATHER_M4 = Board(
    board_name="samd51j",
    real_board_name="adafruit_feather_m4",
    platform="atmelsam",
    framework="arduino",
    lib_ignore=["I2S"],  # I2S library has SAMD51 compatibility issues
    defines=[
        "FASTLED_USES_ARDUINO_AUDIO_INPUT=0",  # Disable Arduino audio (I2S not available)
    ],
)

SAMD51P20A_GRANDCENTRAL = Board(
    board_name="samd51p",
    real_board_name="adafruit_grand_central_m4",
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
    board: Board | None = None
    if no_project_options:
        board = Board(board_name=board_name, add_board_to_all=False)
    elif board_name in _BOARD_MAP:
        # Direct match on board_name (primary lookup)
        board = _BOARD_MAP[board_name]
    else:
        # Try reverse lookup by real_board_name (alias support)
        board = None  # type: ignore[assignment]
        for candidate in ALL:
            if candidate.real_board_name == board_name:
                board = candidate
                break

        if board is None:
            # No match found - create generic board without special overrides
            # Assume platformio will know what to do with it
            board = Board(board_name=board_name, add_board_to_all=False)

    assert board is not None
    return board.clone()
