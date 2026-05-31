# Hardware AutoResearch

## Live Device Testing (AI Agents)
**PRIMARY TOOL: `bash autoresearch`** - Hardware-in-the-loop autoresearch framework:

The `bash autoresearch` command is the **preferred entry point** for AI agents doing live device testing. It provides a complete autoresearch framework with pre-configured expect/fail patterns designed for hardware testing.

### MANDATORY: Driver Selection Required

You **must** specify at least one LED driver to test using one of these flags:
- `--parlio` - Test parallel I/O driver
- `--rmt` - Test RMT (Remote Control) driver
- `--spi` - Test SPI driver
- `--uart` - Test UART driver
- `--i2s` - Test I2S LCD_CAM driver (ESP32-S3 only)
- `--lcd-rgb` - Test LCD RGB driver (ESP32-P4 only)
- `--all` - Test all drivers (equivalent to `--parlio --rmt --spi --uart --i2s --lcd-rgb`)
- `--parallel` - Test multiple drivers simultaneously (requires 2+ drivers)

### Usage Examples
```bash
# Test specific driver (MANDATORY - must specify at least one)
bash autoresearch --parlio                    # Auto-detect environment
bash autoresearch esp32s3 --parlio            # Specify esp32s3 environment
bash autoresearch --rmt
bash autoresearch --spi
bash autoresearch --uart
bash autoresearch --i2s                       # ESP32-S3 only
bash autoresearch --lcd-rgb                   # ESP32-P4 only

# Test multiple drivers (sequentially)
bash autoresearch --parlio --rmt              # Auto-detect environment
bash autoresearch esp32dev --parlio --rmt     # Specify esp32dev environment
bash autoresearch --spi --uart

# Test multiple drivers in parallel (simultaneously on device)
bash autoresearch --parlio --lcd-rgb --parallel --lanes 1    # PARLIO + LCD_RGB parallel on ESP32-P4
bash autoresearch --parlio --rmt --parallel                  # PARLIO + RMT parallel

# Test all drivers
bash autoresearch --all                       # Auto-detect environment
bash autoresearch esp32s3 --all               # Specify esp32s3 environment

# Combined with other options
bash autoresearch --parlio --skip-lint
bash autoresearch esp32s3 --rmt --timeout 120
bash autoresearch --all --skip-lint --timeout 180
```

### Common Options
- `--skip-lint` - Skip linting for faster iteration
- `--timeout <seconds>` - Custom timeout (default: 120s)
- `--help` - See all options

### Build Backend
`bash autoresearch` uses fbuild for all board compiles. Do not use board-specific PlatformIO fallback paths for compatibility issues; file board build compatibility problems at https://github.com/FastLED/fbuild/issues.

### Strip Size Configuration
Configure LED strip sizes for autoresearch testing via JSON-RPC:
```bash
# Use strip size presets
bash autoresearch --parlio --strip-sizes small         # 100, 500 LEDs
bash autoresearch --rmt --strip-sizes medium           # 300, 1000 LEDs
bash autoresearch --spi --strip-sizes large            # 500, 3000 LEDs

# Use custom strip sizes (comma-separated LED counts)
bash autoresearch --parlio --strip-sizes 100,300       # Test with 100 and 300 LED strips
bash autoresearch --rmt --strip-sizes 100,300,1000     # Test with 100, 300, and 1000 LED strips
bash autoresearch --i2s --strip-sizes 500              # Test with single 500 LED strip

# Combined configuration
bash autoresearch --all --strip-sizes 100,500,3000
```

**Strip Size Presets:**
- `tiny` - 10, 100 LEDs
- `small` - 100, 500 LEDs (default)
- `medium` - 300, 1000 LEDs
- `large` - 500, 3000 LEDs
- `xlarge` - 1000, 5000 LEDs (high-memory devices only)

### Lane Configuration
Configure number of lanes for autoresearch testing via JSON-RPC:
```bash
# Test with specific lane count
bash autoresearch --parlio --lanes 2                   # Test with exactly 2 lanes
bash autoresearch --i2s --lanes 4                      # Test with exactly 4 lanes

# Test with lane range
bash autoresearch --rmt --lanes 1-4                    # Test with 1 to 4 lanes (tests all combinations)
bash autoresearch --spi --lanes 2-8                    # Test with 2 to 8 lanes

# Set per-lane LED counts (NEW)
bash autoresearch --i2s --lane-counts 100,200,300      # 3 lanes with 100, 200, 300 LEDs per lane
bash autoresearch --parlio --lane-counts 50,100        # 2 lanes with 50 and 100 LEDs per lane

# Combined with strip sizes
bash autoresearch --i2s --lanes 2 --strip-sizes 100,300  # 2 lanes, strips of 100 and 300 LEDs
```

**Default:** 1-8 lanes (firmware default)

### Color Pattern Configuration
Configure custom RGB color pattern for autoresearch testing:
```bash
# Custom color patterns (hex RGB format)
bash autoresearch --parlio --color-pattern ff00aa      # Pink color (RGB: 255, 0, 170)
bash autoresearch --rmt --color-pattern 0x00ff00       # Green color (RGB: 0, 255, 0)
bash autoresearch --i2s --color-pattern 112233         # Dark blue (RGB: 17, 34, 51)

# Combined with lane configuration
bash autoresearch --parlio --lane-counts 100,200 --color-pattern ff0000  # 2 lanes, red color
```

**Note:** Custom color patterns require firmware support via the `setSolidColor` RPC command. This command may need to be implemented in the firmware if not already available.

### Error Handling
If you run `bash autoresearch` without specifying a driver, you'll get a helpful error message:
```
ERROR: No LED driver specified. You must specify at least one driver to test.

Available driver options:
  --parlio    Test parallel I/O driver
  --rmt       Test RMT (Remote Control) driver
  --spi       Test SPI driver
  --uart      Test UART driver
  --i2s       Test I2S LCD_CAM driver (ESP32-S3 only)
  --lcd-rgb   Test LCD RGB driver (ESP32-P4 only)
  --all       Test all drivers

Example commands:
  bash autoresearch --parlio
  bash autoresearch esp32s3 --parlio
  bash autoresearch --rmt --spi
  bash autoresearch --all
```

### What bash autoresearch Does
1. **Lint** - Catches ISR errors and code quality issues (skippable with --skip-lint)
2. **Compile** - Builds for attached device
3. **Upload** - Flashes firmware to device
4. **Monitor** - AutoResearch output with smart pattern matching
5. **Report** - Exit 0 (success) or 1 (failure) with detailed feedback

### Runtime Driver Selection
Driver selection happens at runtime via JSON-RPC (no recompilation needed). You can instantly switch between drivers or test multiple drivers without rebuilding firmware.

### Interpreting Output Cutoff as Device Crash
**DIRECTIVE:** If the serial output is abruptly cut off mid-test (no `TEST_COMPLETED_EXIT_OK` or `TEST_COMPLETED_EXIT_ERROR`), **assume the device crashed**. This applies even when there is no stack trace or panic message — some crashes (e.g., hardware watchdog, power brownout, DMA corruption) produce no diagnostic output.

**When a crash is suspected without a stack trace:**
1. Investigate whether a stack trace dumper / crash handler is implemented for the target platform
2. If no crash handler exists, attempt to implement one (e.g., ESP32 `esp_system` panic handler, custom `abort()` hook)
3. If implementing a crash handler, leave a comment in `platformio.ini` documenting the investigation outcome
4. Re-run the failing test to capture the stack trace
5. If the crash handler cannot be implemented, document the limitation in the test results

### Why bash autoresearch instead of bash debug
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

## JSON-RPC AutoResearch Protocol (Advanced)

**JSON-RPC AS A SCRIPTING LANGUAGE**: The autoresearch system uses JSON-RPC as a **scripting language** for hardware testing with fail-fast semantics:

### Fail-Fast Model
- All test orchestration happens via JSON-RPC commands and responses
- Text output (FL_PRINT, FL_WARN) is for human diagnostics ONLY
- Python code never greps serial output for control flow decisions
- Tests exit immediately on pass/fail (no timeout waiting)

### Example JSON-RPC Test Script
```python
from ci.rpc_client import RpcClient

with RpcClient("/dev/ttyUSB0") as client:
    # Pre-flight check (testGpioConnection expects [txPin, rxPin])
    result = client.send("testGpioConnection", args=[1, 2])
    if not result["connected"]:
        exit(1)  # Fail-fast: hardware not connected

    # Run a single-driver test (ASYNC — final response arrives via sendAsyncResponse)
    result = client.send("runSingleTest", args={
        "driver": "PARLIO",
        "laneSizes": [100],
        "pattern": "MSB_LSB_A",
    })
    exit(0 if result["success"] else 1)
```

### EXTENSIBLE TESTING
The autoresearch system uses bidirectional JSON-RPC over serial for hardware-in-the-loop testing. This protocol enables AI agents to:
- Test LED drivers without recompilation (runtime driver selection)
- Add new test functionality by extending the JSON-RPC API
- Configure variable lane sizes and test patterns programmatically

### Discovering the live RPC surface
Two device-side discovery rails are always available — prefer these over any hand-written method list, since the firmware is the source of truth:

- `rpc.discover` (built-in) — returns the full OpenRPC schema for every bound method.
- `help` (no args) — returns a flat manifest array of `{name, phase, args, returns, description}`.

```python
from ci.rpc_client import RpcClient

with RpcClient("/dev/ttyUSB0") as client:
    for fn in client.send("help"):
        print(f"{fn['phase']:>22}  {fn['name']:<28}  {fn['args']}")
```

### JSON-RPC Commands (current `bind()` registrations)

Source of truth: `examples/AutoResearch/AutoResearchRemote.cpp` `mRemote->bind("...")` calls. Re-derive this table from `help` / `rpc.discover` rather than memorizing it.

**Phase 1 — Status & basic control**

| Command | Args | Returns |
|---------|------|---------|
| `status` | `[]` | `{ready, pinTx, pinRx}` |
| `ping` | `[]` | `{success, message:"pong", timestamp, uptimeMs}` |
| `drivers` | `[]` | `[{name, priority, enabled}, ...]` |
| `debugTest` | any | `{success, received: <args>}` (echoes args; used to validate RPC plumbing) |
| `setDebug` | `[bool]` | `{success}` (toggle verbose logging) |
| `help` | `[]` | `[{name, phase, args, returns, description}, ...]` |

**Phase 2 — Pin configuration & GPIO probe**

| Command | Args | Returns |
|---------|------|---------|
| `getPins` | `[]` | `{success, txPin, rxPin, defaults:{txPin,rxPin}, platform}` |
| `setTxPin` | `[pin]` | `{success, txPin}` |
| `setRxPin` | `[pin]` | `{success, rxPin}` |
| `setPins` | `[txPin, rxPin]` | `{success, txPin, rxPin}` |
| `testGpioConnection` | `[txPin, rxPin]` | `{success, connected, rxWhenTxLow, rxWhenTxHigh, message}` |
| `findConnectedPins` | `[]` | `{success, candidates:[{txPin,rxPin}, ...]}` |

**Phase 3 — Tests (ASYNC; final response via `sendAsyncResponse`)**

| Command | Args | Returns |
|---------|------|---------|
| `runSingleTest` | `{driver, laneSizes, pattern?, timing?, iterations?}` | `{success, passed, totalTests, passedTests, duration_ms, driver, laneCount, laneSizes, pattern, firstFailure?}` |
| `runParallelTest` | `{drivers:[{driver, laneSizes}, ...], pattern?, iterations?, timing?}` | `{success, passed, duration_ms, show_duration_us, drivers:[...], rx_validation_attempted, rx_validation_passed}` |

**Phase 4 — Utility & benchmarks**

| Command | Args | Description |
|---------|------|-------------|
| `testSimd` | `[]` | Run comprehensive SIMD validation suite |
| `testSimdBenchmark` | `[{iterations}?]` | Benchmark float vs s16x16 vs s16x16x4 multiply |
| `wave8ExpandBenchmark` | `[]` | Wave8 expansion ns/byte micro-bench (#2526) |
| `parlioEncodeBenchmark` | `[]` | Full PARLIO encode_us bench (#2539) |
| `parlioStreamValidate` | `[...]` | PARLIO stream validation against reference |
| `testAsync` | `[]` | Async framework smoke test |
| `testNoSerial` | `[]` | Minimal RPC plumbing check (no Serial writes) |

**Phase 5 — Networking / OTA / BLE / decode**

`startNetServer`, `startNetClient`, `runNetClientTest`, `runNetLoopback`, `stopNet`, `startOta`, `stopOta`, `startBle`, `stopBle`, `bleStatus`, `decodeFile(data:bytes, ext:string)`.

**Phase 6 — Coroutine framework tests**

`testCoroutineBasic`, `testCoroutineStop`, `testCoroutineConcurrent`, `testCoroutineAwait`, `testCoroutineAwaitError`, `testCoroutinePromiseCallbacks`, `testCoroutinePromiseCatchCallback`, `testCoroutineChainedAwait`, `testCoroutineAll`.

**Not bound (historical):** `runTest`, `setDrivers`, `setLaneRange`, `setStripSizes`, `configure`, `setLaneSizes`, `setLedCount`, `setPattern`, `setShortStripSize`, `setLongStripSize`, `setStripSizeValues`, `getState`, `reset`. These names appear in the firmware-side `help` manifest as historical hints but currently return `-32601 Method not found`. Use `runSingleTest` / `runParallelTest` with the inline config shape above.

### `runSingleTest` example (streaming JSONL)
```json
// Device emits ready event after setup:
RESULT: {"type":"ready","ready":true,"setupTimeMs":2340,"testCases":48,"drivers":3}

// Send:
{"function":"runSingleTest","args":{"driver":"PARLIO","laneSizes":[100,100],"pattern":"MSB_LSB_A"}}

// Receive (streaming JSONL):
REMOTE: {"success":true,"streamMode":true}
RESULT: {"type":"test_start","driver":"PARLIO","totalLeds":200}
RESULT: {"type":"test_complete","passed":true,"totalTests":4,"passedTests":4,"durationMs":2830}
```

### Variable Lane Configurations
```python
# Uniform: all lanes same size — pass identical entries
client.send("runSingleTest", args={"driver": "RMT", "laneSizes": [100]*4, "pattern": "MSB_LSB_A"})

# Asymmetric: different sizes per lane
client.send("runSingleTest", args={"driver": "PARLIO", "laneSizes": [300, 200, 100, 50], "pattern": "SOLID_RGB"})
```

### Extending the Protocol
The JSON-RPC architecture allows agents to easily add new test functionality:
1. Add a `mRemote->bind("yourMethod", ...)` registration in `examples/AutoResearch/AutoResearchRemote.cpp`.
2. Add a matching entry to the `help` handler (same file) so the manifest stays self-describing.
3. Client code can then call it via `rpc_client.send("yourMethod", ...)` — no Python wrapper required.

**Files**:
- `examples/AutoResearch/AutoResearchRemote.cpp` — firmware RPC handlers (source of truth)
- `ci/rpc_client.py` — minimal `send(method, args)` client used in examples above
- `ci/autoresearch_agent.py` / `ci/autoresearch_loop.py` — higher-level wrapper. NOTE: parts of this wrapper still call legacy RPC names (`configure`, `runTest`, `getState`, `reset`) that are no longer bound — prefer `rpc_client` until the wrapper is reconciled.

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
