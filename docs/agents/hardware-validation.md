# Hardware Validation

## Live Device Testing (AI Agents)
**PRIMARY TOOL: `bash validate`** - Hardware-in-the-loop validation framework:

The `bash validate` command is the **preferred entry point** for AI agents doing live device testing. It provides a complete validation framework with pre-configured expect/fail patterns designed for hardware testing.

### MANDATORY: Driver Selection Required

You **must** specify at least one LED driver to test using one of these flags:
- `--parlio` - Test parallel I/O driver
- `--rmt` - Test RMT (Remote Control) driver
- `--spi` - Test SPI driver
- `--uart` - Test UART driver
- `--i2s` - Test I2S LCD_CAM driver (ESP32-S3 only)
- `--all` - Test all drivers (equivalent to `--parlio --rmt --spi --uart --i2s`)

### Usage Examples
```bash
# Test specific driver (MANDATORY - must specify at least one)
bash validate --parlio                    # Auto-detect environment
bash validate esp32s3 --parlio            # Specify esp32s3 environment
bash validate --rmt
bash validate --spi
bash validate --uart
bash validate --i2s                       # ESP32-S3 only

# Test multiple drivers
bash validate --parlio --rmt              # Auto-detect environment
bash validate esp32dev --parlio --rmt     # Specify esp32dev environment
bash validate --spi --uart

# Test all drivers
bash validate --all                       # Auto-detect environment
bash validate esp32s3 --all               # Specify esp32s3 environment

# Combined with other options
bash validate --parlio --skip-lint
bash validate esp32s3 --rmt --timeout 120
bash validate --all --skip-lint --timeout 180
```

### Common Options
- `--skip-lint` - Skip linting for faster iteration
- `--timeout <seconds>` - Custom timeout (default: 120s)
- `--help` - See all options

### Strip Size Configuration
Configure LED strip sizes for validation testing via JSON-RPC:
```bash
# Use strip size presets
bash validate --parlio --strip-sizes small         # 100, 500 LEDs
bash validate --rmt --strip-sizes medium           # 300, 1000 LEDs
bash validate --spi --strip-sizes large            # 500, 3000 LEDs

# Use custom strip sizes (comma-separated LED counts)
bash validate --parlio --strip-sizes 100,300       # Test with 100 and 300 LED strips
bash validate --rmt --strip-sizes 100,300,1000     # Test with 100, 300, and 1000 LED strips
bash validate --i2s --strip-sizes 500              # Test with single 500 LED strip

# Combined configuration
bash validate --all --strip-sizes 100,500,3000
```

**Strip Size Presets:**
- `tiny` - 10, 100 LEDs
- `small` - 100, 500 LEDs (default)
- `medium` - 300, 1000 LEDs
- `large` - 500, 3000 LEDs
- `xlarge` - 1000, 5000 LEDs (high-memory devices only)

### Lane Configuration
Configure number of lanes for validation testing via JSON-RPC:
```bash
# Test with specific lane count
bash validate --parlio --lanes 2                   # Test with exactly 2 lanes
bash validate --i2s --lanes 4                      # Test with exactly 4 lanes

# Test with lane range
bash validate --rmt --lanes 1-4                    # Test with 1 to 4 lanes (tests all combinations)
bash validate --spi --lanes 2-8                    # Test with 2 to 8 lanes

# Set per-lane LED counts (NEW)
bash validate --i2s --lane-counts 100,200,300      # 3 lanes with 100, 200, 300 LEDs per lane
bash validate --parlio --lane-counts 50,100        # 2 lanes with 50 and 100 LEDs per lane

# Combined with strip sizes
bash validate --i2s --lanes 2 --strip-sizes 100,300  # 2 lanes, strips of 100 and 300 LEDs
```

**Default:** 1-8 lanes (firmware default)

### Color Pattern Configuration
Configure custom RGB color pattern for validation testing:
```bash
# Custom color patterns (hex RGB format)
bash validate --parlio --color-pattern ff00aa      # Pink color (RGB: 255, 0, 170)
bash validate --rmt --color-pattern 0x00ff00       # Green color (RGB: 0, 255, 0)
bash validate --i2s --color-pattern 112233         # Dark blue (RGB: 17, 34, 51)

# Combined with lane configuration
bash validate --parlio --lane-counts 100,200 --color-pattern ff0000  # 2 lanes, red color
```

**Note:** Custom color patterns require firmware support via the `setSolidColor` RPC command. This command may need to be implemented in the firmware if not already available.

### Error Handling
If you run `bash validate` without specifying a driver, you'll get a helpful error message:
```
ERROR: No LED driver specified. You must specify at least one driver to test.

Available driver options:
  --parlio    Test parallel I/O driver
  --rmt       Test RMT (Remote Control) driver
  --spi       Test SPI driver
  --uart      Test UART driver
  --i2s       Test I2S LCD_CAM driver (ESP32-S3 only)
  --all       Test all drivers

Example commands:
  bash validate --parlio
  bash validate esp32s3 --parlio
  bash validate --rmt --spi
  bash validate --all
```

### What bash validate Does
1. **Lint** - Catches ISR errors and code quality issues (skippable with --skip-lint)
2. **Compile** - Builds for attached device
3. **Upload** - Flashes firmware to device
4. **Monitor** - Validates output with smart pattern matching
5. **Report** - Exit 0 (success) or 1 (failure) with detailed feedback

### Runtime Driver Selection
Driver selection happens at runtime via JSON-RPC (no recompilation needed). You can instantly switch between drivers or test multiple drivers without rebuilding firmware.

### Why bash validate instead of bash debug
- Pre-configured patterns catch hardware failures automatically
- Designed for automated testing and AI feedback loops
- Prevents false positives from ISR hangs and device crashes
- Comprehensive test matrix (48 test cases across drivers/lanes/sizes)
- Runtime driver selection (no recompilation needed)

**When to use `bash debug` (advanced):**
For custom sketches requiring manual pattern configuration. See Device Debug section below.

## Device Debug (Manual Testing)
`bash debug <sketch>` - Low-level tool for custom device workflows:

**Simple example:**
```bash
bash debug MySketch --expect "READY"
```

**Complex example:**
```bash
bash debug MySketch --expect "INIT OK" --expect "TEST PASS" --fail-on "PANIC" --timeout 2m
```

For full documentation, see `uv run ci/debug_attached.py --help`

## JSON-RPC Validation Protocol (Advanced)

**JSON-RPC AS A SCRIPTING LANGUAGE**: The validation system uses JSON-RPC as a **scripting language** for hardware testing with fail-fast semantics:

### Fail-Fast Model
- All test orchestration happens via JSON-RPC commands and responses
- Text output (FL_PRINT, FL_WARN) is for human diagnostics ONLY
- Python code never greps serial output for control flow decisions
- Tests exit immediately on pass/fail (no timeout waiting)

### Example JSON-RPC Test Script
```python
from ci.rpc_client import RpcClient

with RpcClient("/dev/ttyUSB0") as client:
    # Pre-flight check
    result = client.send("testGpioConnection", args=[1, 2])
    if not result["connected"]:
        exit(1)  # Fail-fast: hardware not connected

    # Configure and run test in single call (NEW consolidated format)
    result = client.send("runTest", args={
        "drivers": ["PARLIO", "RMT"],
        "laneRange": {"min": 1, "max": 4},
        "stripSizes": [100, 300]
    })
    exit(0 if result["success"] else 1)
```

### EXTENSIBLE TESTING
The validation system uses bidirectional JSON-RPC over serial for hardware-in-the-loop testing. This protocol enables AI agents to:
- Test LED drivers without recompilation (runtime driver selection)
- Add new test functionality by extending the JSON-RPC API
- Configure variable lane sizes and test patterns programmatically

### Python Agent (`ci/validation_agent.py`)
```python
from validation_agent import ValidationAgent, TestConfig

# Connect to device
with ValidationAgent("COM18") as agent:
    # Health check
    agent.ping()  # Returns: {timestamp, uptimeMs, frameCounter}

    # Get available drivers
    drivers = agent.get_drivers()  # Returns: ["PARLIO", "RMT", "SPI", "UART"]

    # Configure test: 100 LEDs, 1 lane, PARLIO driver
    config = TestConfig(
        driver="PARLIO",
        lane_sizes=[100],  # Per-lane LED counts
        pattern="MSB_LSB_A",
        iterations=1
    )
    agent.configure(config)

    # Run test and get results
    result = agent.run_test()
    print(f"Passed: {result.passed_tests}/{result.total_tests}")

    # Reset between tests
    agent.reset()
```

### JSON-RPC Commands
Sent over serial with `REMOTE:` response prefix:

| Command | Args | Description |
|---------|------|-------------|
| `ping` | - | Health check, returns timestamp and uptime |
| `drivers` | - | List available LED drivers with enabled status |
| `getState` | - | Query current device state and configuration |
| `runTest` | `{drivers, laneRange?, stripSizes?}` | **NEW**: Configure and execute test in single call with named arguments (recommended) |
| `setDrivers` | `[driver_names...]` | Legacy: Set which drivers to test (use `runTest` with config instead) |
| `setLaneRange` | `min, max` | Legacy: Set lane count range (use `runTest` with config instead) |
| `setStripSizes` | `[sizes...]` | Legacy: Set strip sizes array (use `runTest` with config instead) |
| `configure` | `{driver, laneSizes, pattern, iterations, shortStripSize?, longStripSize?, testSmallStrips?, testLargeStrips?}` | Set up test parameters (extended with strip size config) |
| `setLaneSizes` | `[sizes...]` | Set per-lane LED counts directly |
| `setLedCount` | `count` | Set uniform LED count for all lanes |
| `setPattern` | `name` | Set test pattern (MSB_LSB_A, SOLID_RGB, etc.) |
| `setShortStripSize` | `size` | Set short strip LED count |
| `setLongStripSize` | `size` | Set long strip LED count |
| `setStripSizeValues` | `short, long` | Set both strip sizes at once |
| `reset` | - | Reset device state |

### Consolidated `runTest` with Named Arguments (recommended)
```json
// Device emits ready event after setup:
RESULT: {"type":"ready","ready":true,"setupTimeMs":2340,"testCases":48,"drivers":3}

// NEW: Single call with config (replaces setDrivers + setLaneRange + setStripSizes + runTest)
// Send:
{"function":"runTest","args":{"drivers":["PARLIO","RMT"],"laneRange":{"min":2,"max":4},"stripSizes":[100,300]}}

// Receive (streaming JSONL):
REMOTE: {"success":true,"streamMode":true}
RESULT: {"type":"test_start","testCases":16}
RESULT: {"type":"test_complete","passed":true,"totalTests":64,"passedTests":64,"durationMs":8230}
```

### Legacy Multi-Call Format (still supported)
```json
// Send (4 separate calls):
{"function":"setDrivers","args":["PARLIO"]}
{"function":"setLaneRange","args":[2,4]}
{"function":"setStripSizes","args":[[100,300]]}
{"function":"runTest","args":[]}

// Receive:
REMOTE: {"success":true,"streamMode":true}
RESULT: {"type":"test_start","driver":"PARLIO","totalLeds":100}
RESULT: {"type":"test_complete","passed":true,"totalTests":4,"passedTests":4,"durationMs":2830}
```

### Variable Lane Configurations
```python
# Uniform: all lanes same size
config = TestConfig.uniform("RMT", led_count=100, lane_count=4, pattern="MSB_LSB_A")

# Asymmetric: different sizes per lane
config = TestConfig(driver="PARLIO", lane_sizes=[300, 200, 100, 50], pattern="SOLID_RGB")
```

### Strip Size Configuration
```python
# Option 1: Individual RPC commands
agent.set_strip_size_values(short=100, long=500)  # Set both sizes at once
agent.set_strip_sizes_enabled(small=True, large=True)  # Enable both strip sizes

# Option 2: Via TestConfig
config = TestConfig(
    driver="PARLIO",
    lane_sizes=[100, 100],
    pattern="MSB_LSB_A",
    short_strip_size=300,      # Override default short strip size
    long_strip_size=1000,      # Override default long strip size
    test_small_strips=True,    # Enable small strip testing
    test_large_strips=True,    # Enable large strip testing (requires sufficient memory)
)
agent.configure(config)
```

### Extending the Protocol
The JSON-RPC architecture allows agents to easily add new test functionality:
1. Add new RPC handler in `examples/Validation/ValidationRemote.cpp`
2. Add corresponding Python method in `ci/validation_agent.py`
3. No firmware recompilation needed for client-side extensions

**Files**:
- `ci/validation_agent.py` - Python ValidationAgent class
- `ci/validation_loop.py` - Test orchestration with CLI
- `examples/Validation/ValidationRemote.cpp` - Firmware RPC handlers

## Package Installation Daemon Management
`bash daemon <command>` - Manage the singleton daemon that handles PlatformIO package installations:

**Commands:**
- `bash daemon status` - Show daemon status and health
- `bash daemon stop` - Stop the daemon gracefully
- `bash daemon logs` - View daemon log file (last 50 lines)
- `bash daemon logs-tail` - Follow daemon logs in real-time (Ctrl+C to exit)
- `bash daemon start` - Start the daemon (usually automatic)
- `bash daemon restart` - Stop and start the daemon
- `bash daemon clean` - Remove all daemon state (force fresh start)

**What is the daemon:**
The package installation daemon is a singleton background process that ensures PlatformIO package installations complete atomically, even if an agent is interrupted mid-download. It prevents package corruption by:
- Surviving agent termination (daemon continues independently)
- Preventing concurrent package installations system-wide
- Providing progress feedback to waiting clients
- Auto-shutting down after 12 hours of inactivity

**Note:** The daemon starts automatically when needed by `bash compile` or `bash debug`. Manual management is typically not required.
