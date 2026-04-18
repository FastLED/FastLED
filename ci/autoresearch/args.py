"""Argument parsing for the autoresearch test runner."""

import argparse
from dataclasses import dataclass
from pathlib import Path


@dataclass
class Args:
    """Parsed command-line arguments for autoresearch test runner."""

    # Positional argument
    environment_positional: str | None

    # Driver selection flags
    parlio: bool
    rmt: bool
    spi: bool
    uart: bool
    lcd: bool
    lcd_spi: bool
    lcd_rgb: bool
    object_fled: bool
    all: bool
    simd: bool
    coroutine: bool

    # Standard options
    environment: str | None
    verbose: bool
    skip_lint: bool
    upload_port: str | None
    timeout: str
    project_dir: Path

    # Pattern overrides
    no_expect: bool
    no_fail_on: bool
    expect_keywords: list[str] | None
    fail_keywords: list[str] | None

    # Pin configuration
    tx_pin: int | None
    rx_pin: int | None
    auto_discover_pins: bool

    # Build system selection
    use_fbuild: bool
    no_fbuild: bool
    clean: bool
    skip_schema: bool
    quiet: bool

    # Strip size configuration
    strip_sizes: str | None

    # Lane configuration
    lanes: str | None

    # Per-lane LED counts
    lane_counts: str | None

    # Color pattern
    color_pattern: str | None

    # Legacy API testing
    legacy: bool

    # Chipset selection
    chipset: str

    # Network autoresearch modes
    net_server: bool
    net_client: bool
    net: bool

    # OTA autoresearch mode
    ota: bool

    # BLE autoresearch mode
    ble: bool

    # Parallel driver testing
    parallel: bool

    # Decode autoresearch mode (host-only, no device needed)
    decode: str | None

    # Multi-frame capture — number of back-to-back show()/capture cycles per pattern.
    # None = driver-default (SPI → 2, others → 1). Explicit value overrides.
    # See issues #2254 and #2288 (ESP32-S3 SPI second-frame degradation).
    frames: int | None

    @staticmethod
    def parse_args() -> "Args":
        """Parse command-line arguments and return Args dataclass instance."""
        parser = argparse.ArgumentParser(
            description="FastLED AutoResearch Test Runner with JSON-RPC support",
            formatter_class=argparse.RawDescriptionHelpFormatter,
            epilog="""
Examples:
  %(prog)s --parlio                    # Auto-detect env, test only PARLIO driver
  %(prog)s esp32s3 --parlio            # Test esp32s3, PARLIO driver
  %(prog)s --rmt --spi                 # Test RMT and SPI drivers
  %(prog)s --all                       # Test all drivers
  %(prog)s --parlio --skip-lint        # Skip linting for faster iteration
  %(prog)s --rmt --timeout 120         # Custom timeout (default: 60s)
  %(prog)s --lcd --lanes 2 --strip-sizes 100,300  # Test LCD_CLOCKLESS with 2 lanes, strips of 100 and 300 LEDs
  %(prog)s --lcd-spi --strip-sizes 100,500   # Test LCD_SPI (APA102) on ESP32-S3
  %(prog)s --parlio --lanes 1-4        # Test PARLIO with 1-4 lanes
  %(prog)s --rmt --strip-sizes small   # Test RMT with 'small' preset (100/500 LEDs)
  %(prog)s --lcd --lane-counts 100,200,300  # Test LCD_CLOCKLESS with 3 lanes (100, 200, 300 LEDs per lane)
  %(prog)s --parlio --color-pattern 0xff00aa  # Test PARLIO with custom color pattern (RGB hex)
  %(prog)s --help                      # Show this help message

Driver Selection (JSON-RPC):
  Driver selection happens at runtime via JSON-RPC (no recompilation needed).
  You can instantly switch between drivers without rebuilding firmware.

  MANDATORY: You MUST specify at least one driver flag:
    --parlio       Test only PARLIO driver
    --rmt          Test only RMT driver
    --spi          Test only SPI driver
    --uart         Test only UART driver
    --lcd          Test only LCD_CLOCKLESS driver (ESP32-S3 only, replaces misnamed I2S)
    --lcd-spi      Test only LCD_SPI driver (ESP32-S3 only, APA102/SK9822)
    --lcd-rgb      Test only LCD RGB driver (ESP32-P4 only)
    --object-fled  Test only ObjectFLED DMA driver (Teensy 4.x)
    --all          Test all drivers

Strip Size Configuration:
  Configure LED strip sizes for autoresearch testing via JSON-RPC:
    --strip-sizes <preset or custom>   Set strip sizes (preset or comma-separated counts)

  Presets (shortcuts for common configurations):
    tiny    - 10, 100 LEDs
    small   - 100, 500 LEDs (default)
    medium  - 300, 1000 LEDs
    large   - 500, 3000 LEDs
    xlarge  - 1000, 5000 LEDs (high-memory devices only)

  Custom arrays (comma-separated LED counts):
    --strip-sizes 100,300         Test with 100 and 300 LED strips
    --strip-sizes 100,300,1000    Test with 100, 300, and 1000 LED strips
    --strip-sizes 500             Test with single 500 LED strip

Lane Configuration:
  Configure number of lanes for autoresearch testing via JSON-RPC:
    --lanes <N or MIN-MAX>   Set lane count or range
    --lane-counts <LED1,LED2,...>  Set per-lane LED counts (comma-separated)

  Examples:
    --lanes 2          Test with exactly 2 lanes
    --lanes 1-4        Test with 1 to 4 lanes (tests all combinations)
    --lane-counts 100,200,300  3 lanes with 100, 200, 300 LEDs per lane
    Default: 1-8 lanes (firmware default)

Color Pattern Configuration:
  Configure custom color pattern for autoresearch testing:
    --color-pattern <HEX>  Set RGB color pattern (hex format: RRGGBB or 0xRRGGBB)

  Examples:
    --color-pattern ff00aa      Custom color (pink)
    --color-pattern 0x00ff00    Custom color (green)

Exit Codes:
  0   Success (all patterns found, no failures)
  1   Failure (missing patterns, ERROR detected, or compilation/upload failed)
  130 User interrupt (Ctrl+C)

See Also:
  - examples/AutoResearch/AutoResearch.ino - AutoResearch sketch documentation
  - CLAUDE.md Section "Live Device Testing (AI Agents)" - Usage guidelines
        """,
        )

        # Positional argument for environment
        parser.add_argument(
            "environment_positional",
            nargs="?",
            help="PlatformIO environment to build (e.g., esp32s3, esp32dev). If omitted, auto-detects from attached device.",
        )

        # Driver selection flags
        driver_group = parser.add_argument_group(
            "Driver Selection (optional — omit for GPIO-only mode)"
        )
        driver_group.add_argument(
            "--parlio",
            action="store_true",
            help="Test only PARLIO driver",
        )
        driver_group.add_argument(
            "--rmt",
            action="store_true",
            help="Test only RMT driver",
        )
        driver_group.add_argument(
            "--spi",
            action="store_true",
            help="Test only SPI driver",
        )
        driver_group.add_argument(
            "--uart",
            action="store_true",
            help="Test only UART driver",
        )
        driver_group.add_argument(
            "--lcd",
            action="store_true",
            help="Test only LCD_CLOCKLESS driver (ESP32-S3, clockless via LCD_CAM I80; replaces misnamed I2S)",
        )
        driver_group.add_argument(
            "--lcd-spi",
            action="store_true",
            help="Test only LCD_SPI driver (ESP32-S3, APA102/SK9822 via LCD_CAM I80)",
        )
        driver_group.add_argument(
            "--lcd-rgb",
            action="store_true",
            help="Test only LCD RGB driver (ESP32-P4 only)",
        )
        driver_group.add_argument(
            "--object-fled",
            action="store_true",
            help="Test only ObjectFLED DMA driver (Teensy 4.x only)",
        )
        driver_group.add_argument(
            "--all",
            action="store_true",
            help="Test all drivers (equivalent to --parlio --rmt --spi --uart --lcd --lcd-spi --lcd-rgb --object-fled)",
        )
        driver_group.add_argument(
            "--simd",
            action="store_true",
            help="Test SIMD operations only (no LED drivers)",
        )
        driver_group.add_argument(
            "--coroutine",
            action="store_true",
            help="Test coroutine/task creation, stop, and await (no LED drivers needed)",
        )
        driver_group.add_argument(
            "--parallel",
            action="store_true",
            help="Test multiple drivers in parallel (e.g., --parlio --lcd-rgb --parallel --lanes 1)",
        )

        # Network autoresearch modes
        net_group = parser.add_argument_group(
            "Network AutoResearch (ESP32 WiFi + HTTP)",
            "Test ESP32 WiFi soft AP and HTTP server/client functionality.",
        )
        net_group.add_argument(
            "--net-server",
            action="store_true",
            help="ESP32 starts WiFi AP + HTTP server; host connects and validates endpoints",
        )
        net_group.add_argument(
            "--net-client",
            action="store_true",
            help="ESP32 starts WiFi AP; host starts HTTP server; ESP32 fetches from host",
        )
        net_group.add_argument(
            "--net",
            action="store_true",
            help="Self-contained loopback test: ESP32 starts HTTP server, then GETs localhost (no WiFi needed)",
        )

        # OTA autoresearch mode
        ota_group = parser.add_argument_group(
            "OTA AutoResearch (ESP32 WiFi + OTA HTTP)",
            "Test ESP32 OTA (Over-The-Air) firmware update web interface.",
        )
        ota_group.add_argument(
            "--ota",
            action="store_true",
            help="ESP32 starts WiFi AP + OTA HTTP server; host validates auth and update endpoints",
        )

        # BLE autoresearch mode
        ble_group = parser.add_argument_group(
            "BLE AutoResearch (ESP32 BLE GATT)",
            "Test ESP32 BLE GATT server with JSON-RPC ping/pong over BLE.",
        )
        ble_group.add_argument(
            "--ble",
            action="store_true",
            help="ESP32 starts BLE GATT server; host connects via Bleak and validates ping/pong",
        )

        # Decode autoresearch mode (host-only or device)
        decode_group = parser.add_argument_group(
            "Decode AutoResearch",
            "Test codec decoding of a local file or URL. "
            "Without --env: runs host-only C++ test. "
            "With --env: sends file to device via JSON-RPC for on-device decoding.",
        )
        decode_group.add_argument(
            "--decode",
            type=str,
            default=None,
            metavar="PATH_OR_URL",
            help="Decode a media file (local path or URL). Supported: .mp4 .mpeg .mpg .gif .jpg .jpeg .mp3 .webp",
        )

        # Standard options
        parser.add_argument(
            "--env",
            "-e",
            dest="environment",
            help="PlatformIO environment to build (optional, auto-detect if not provided)",
        )
        parser.add_argument(
            "--verbose",
            "-v",
            action="store_true",
            help="Enable verbose output",
        )
        parser.add_argument(
            "--skip-lint",
            action="store_true",
            help="Skip C++ linting phase (faster iteration, but may miss ISR errors)",
        )
        parser.add_argument(
            "--upload-port",
            "-p",
            help="Serial port to use for upload and monitoring (e.g., /dev/ttyUSB0, COM3)",
        )
        parser.add_argument(
            "--timeout",
            "-t",
            type=str,
            default="60",
            help="Timeout for monitor phase. Supports: plain number (seconds), '120s', '2m', '5000ms' (default: 60)",
        )
        parser.add_argument(
            "--project-dir",
            "-d",
            type=Path,
            default=Path.cwd(),
            help="PlatformIO project directory (default: current directory)",
        )

        # Pattern overrides (for advanced usage)
        pattern_group = parser.add_argument_group(
            "Pattern Overrides (advanced)",
            "Override default autoresearch patterns. Use with caution.",
        )
        pattern_group.add_argument(
            "--no-expect",
            action="store_true",
            help="Clear default --expect patterns (use custom only)",
        )
        pattern_group.add_argument(
            "--no-fail-on",
            action="store_true",
            help="Clear default --fail-on pattern (use custom only)",
        )
        pattern_group.add_argument(
            "--expect",
            "-x",
            action="append",
            dest="expect_keywords",
            help="Add custom regex pattern that must be matched by timeout for exit 0 (can be specified multiple times)",
        )
        pattern_group.add_argument(
            "--fail-on",
            "-f",
            action="append",
            dest="fail_keywords",
            help="Add custom regex pattern that triggers immediate termination + exit 1 (can be specified multiple times)",
        )

        # Pin configuration
        pin_group = parser.add_argument_group(
            "Pin Configuration",
            "Override default TX/RX pins (platform-specific defaults otherwise).",
        )
        pin_group.add_argument(
            "--tx-pin",
            type=int,
            help="Override TX pin number (FastLED driver output)",
        )
        pin_group.add_argument(
            "--rx-pin",
            type=int,
            help="Override RX pin number (RMT receiver input)",
        )
        pin_group.add_argument(
            "--auto-discover-pins",
            action="store_true",
            default=True,
            help="Auto-discover connected pin pairs (default: True)",
        )
        pin_group.add_argument(
            "--no-auto-discover-pins",
            action="store_true",
            help="Disable auto-discovery of connected pins",
        )

        # Build system selection
        parser.add_argument(
            "--use-fbuild",
            action="store_true",
            help="Use fbuild for compile and upload instead of PlatformIO (default for esp32s3 and esp32c6)",
        )
        parser.add_argument(
            "--no-fbuild",
            action="store_true",
            help="Force PlatformIO even for esp32s3 (disables fbuild default)",
        )
        parser.add_argument(
            "--clean",
            action="store_true",
            help="Clean build before compiling (removes cached build artifacts)",
        )
        parser.add_argument(
            "--skip-schema",
            action="store_true",
            help="Skip RPC schema validation (saves 2-5s per cycle)",
        )
        parser.add_argument(
            "--quiet",
            "-q",
            action="store_true",
            help="Minimal output for AI agents (~15 lines). Verbose output goes to .autoresearch_last.log",
        )

        # Strip size configuration
        strip_size_group = parser.add_argument_group(
            "Strip Size Configuration",
            "Configure LED strip sizes for autoresearch testing.",
        )
        strip_size_group.add_argument(
            "--strip-sizes",
            type=str,
            metavar="PRESET or SIZE1,SIZE2,...",
            help="Strip sizes: preset name (tiny/small/medium/large/xlarge) OR comma-separated LED counts (e.g., '100,300,1000')",
        )

        # Lane configuration
        lane_group = parser.add_argument_group(
            "Lane Configuration",
            "Configure number of lanes for autoresearch testing.",
        )
        lane_group.add_argument(
            "--lanes",
            type=str,
            metavar="N or MIN-MAX",
            help="Lane count: single number (e.g., '2') or range (e.g., '1-4'). Default: 1-8",
        )
        lane_group.add_argument(
            "--lane-counts",
            type=str,
            metavar="LED1,LED2,...",
            help="Per-lane LED counts (comma-separated, e.g., '100,200,300' for 3 lanes with different counts)",
        )

        # Color pattern configuration
        color_group = parser.add_argument_group(
            "Color Pattern Configuration",
            "Configure custom color pattern for autoresearch testing.",
        )
        color_group.add_argument(
            "--color-pattern",
            type=str,
            metavar="HEX",
            help="RGB color pattern in hex format (e.g., 'ff00aa' or '0x00ff00')",
        )

        # Legacy API testing
        parser.add_argument(
            "--legacy",
            action="store_true",
            help="Test using legacy template addLeds API (WS2812B<PIN>) instead of Channel API. Single-lane only, pin must be 0-8.",
        )

        # Chipset selection
        parser.add_argument(
            "--chipset",
            choices=["ws2812", "ucs7604"],
            default="ws2812",
            help="Chipset timing to use for autoresearch (default: ws2812). ucs7604 uses UCS7604-800KHZ timing with 16-bit encoding.",
        )

        # Multi-frame capture (second-frame degradation detection)
        parser.add_argument(
            "--frames",
            type=int,
            default=None,
            metavar="N",
            help="Consecutive back-to-back show()/capture cycles per pattern (1-16). "
            "Exposes second-frame degradation bugs (e.g., SPI driver zeroing its "
            "DMA buffer after first show). Default: 2 for --spi, 1 otherwise. "
            "See issues #2254, #2288.",
        )

        parsed = parser.parse_args()

        # Convert argparse.Namespace to Args dataclass
        return Args(
            environment_positional=parsed.environment_positional,
            parlio=parsed.parlio,
            rmt=parsed.rmt,
            spi=parsed.spi,
            uart=parsed.uart,
            lcd=parsed.lcd,
            lcd_spi=parsed.lcd_spi,
            lcd_rgb=parsed.lcd_rgb,
            object_fled=parsed.object_fled,
            all=parsed.all,
            simd=parsed.simd,
            coroutine=parsed.coroutine,
            environment=parsed.environment,
            verbose=parsed.verbose,
            skip_lint=parsed.skip_lint,
            upload_port=parsed.upload_port,
            timeout=parsed.timeout,
            project_dir=parsed.project_dir,
            no_expect=parsed.no_expect,
            no_fail_on=parsed.no_fail_on,
            expect_keywords=parsed.expect_keywords,
            fail_keywords=parsed.fail_keywords,
            tx_pin=parsed.tx_pin,
            rx_pin=parsed.rx_pin,
            auto_discover_pins=parsed.auto_discover_pins
            and not parsed.no_auto_discover_pins,
            use_fbuild=parsed.use_fbuild,
            no_fbuild=parsed.no_fbuild,
            clean=parsed.clean,
            skip_schema=parsed.skip_schema,
            quiet=parsed.quiet,
            strip_sizes=parsed.strip_sizes,
            lanes=parsed.lanes,
            lane_counts=parsed.lane_counts,
            color_pattern=parsed.color_pattern,
            legacy=parsed.legacy,
            chipset=parsed.chipset,
            net_server=parsed.net_server,
            net_client=parsed.net_client,
            net=parsed.net,
            ota=parsed.ota,
            ble=parsed.ble,
            parallel=parsed.parallel,
            decode=parsed.decode,
            frames=parsed.frames,
        )
