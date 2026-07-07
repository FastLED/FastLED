# Hardware AutoResearch

## 🚨 Port-scan BEFORE claiming a board is "not attached" (REQUIRED)

**Never state "board X is not attached" / "static-verified only, target board absent" without first enumerating what is physically connected in the same session.** Multiple boards are often attached at once. Enumerate with fbuild's native scanner (NOT raw pyserial):

```bash
fbuild port scan
```

Two rows per port — OS identity plus a `└─ vendor / product` line resolved from the FastLED boards DB:

```
COM11     10C4:EA60    Silicon Labs CP210x USB to UART Bridge (COM11)    ser=0254F7A1
          └─ Arduino / NodeMCU 0.9 (ESP-12 Module)       # ESP32-WROOM-class
COM25     303A:1001    USB Serial Device (COM25)         ser=…
          └─ Espressif Systems / WEMOS LOLIN S3          # ESP32-S3
COM10     1FC9:0132    USB Serial Device (COM10)         ser=0B03400A
          └─ NXP Semiconductors / LPC-Link2 CMSIS-DAP    # LPC845-BRK
```

Do NOT reason about attachment from memory or from "which board this task is about." Read the scan. Full rule + history (two incidents where boards were wrongly declared absent): `agents/docs/cpp-standards.md` → "Check if the target hardware might already be attached (REQUIRED)".

## Live Device Testing (AI Agents)
**PRIMARY TOOL: `bash autoresearch`** - Hardware-in-the-loop autoresearch framework:

The `bash autoresearch` command is the **preferred entry point** for AI agents doing live device testing. It provides a complete autoresearch framework with pre-configured expect/fail patterns designed for hardware testing.

### Agent Runbook: Evidence to AutoResearch Docs

Use this workflow when updating AutoResearch agent documentation from prior
Codex sessions, issue history, or hardware bring-up notes. The goal is to turn
messy chat history into verified command examples and agent rules without
copying stale or hallucinated syntax into the repo.

**Source of truth order:**

1. Current repo code: `autoresearch`, `ci/autoresearch/args.py`,
   `ci/autoresearch/phases.py`, `ci/autoresearch/runner.py`.
2. Firmware harness: `examples/AutoResearch/AutoResearch.ino`,
   `examples/AutoResearch/AutoResearchRemote.cpp`,
   `examples/AutoResearch/AutoResearchLowMemory.h`, and driver-specific
   headers under `examples/AutoResearch/`.
3. Repo docs: this file, `CLAUDE.md`, `agents/docs/build-system.md`,
   `src/fl/channels/README.md`, and platform READMEs such as
   `src/platforms/arm/lpc/README.md`.
4. MCU facts: prefer https://github.com/FastLED/datasheets, then vendor
   primary manuals. Separate datasheet facts from what the local jumper wiring
   and AutoResearch output actually proved.
5. Chat logs and issue comments are evidence pointers only. Re-check every
   command, flag, RPC name, board name, and wiring claim against the sources
   above before documenting it.

Reference links for agents:

- [CLAUDE.md](../../CLAUDE.md)
- [hardware-autoresearch.md](hardware-autoresearch.md)
- [autoresearch wrapper](../../autoresearch)
- [ci/autoresearch/args.py](../../ci/autoresearch/args.py)
- [ci/autoresearch/phases.py](../../ci/autoresearch/phases.py)
- [examples/AutoResearch](../../examples/AutoResearch)
- [fl/channels README](../../src/fl/channels/README.md)
- [LPC platform README](../../src/platforms/arm/lpc/README.md)
- [FastLED/datasheets](https://github.com/FastLED/datasheets)

**Mining Codex chat logs for evidence:**

Local Codex archives commonly live under:

```text
C:\Users\niteris\.codex\sessions\YYYY\MM\DD\rollout-*.jsonl
C:\Users\niteris\.codex\history.jsonl
C:\Users\niteris\.codex\logs_2.sqlite
C:\Users\niteris\.codex\goals_1.sqlite
C:\Users\niteris\.codex\memories_1.sqlite
```

Do a deterministic reduction before asking any model to read logs:

```bash
rg -i -l "autoresearch|auto research|fbuild port scan|esp32|esp32s3|esp32c6|lpc845|lpc8|teensy|rpc|function binding|cmd args|fault-emit|dma-uart|uart" C:\Users\niteris\.codex\sessions
```

Normalize hit windows into indexed records so reviewers can cite exact
messages:

```json
{
  "log_id": "2026-07-06T13-31-14",
  "path": "C:/Users/niteris/.codex/sessions/...jsonl",
  "messages": [
    {"idx": 0, "line": 1, "role": "user", "text": "..."},
    {"idx": 1, "line": 2, "role": "assistant", "text": "..."}
  ]
}
```

Lower-cost agents may classify shards, but they must be gatekeepers, not final
doc authors. Prompt them to return strict JSON with `answer: "yes" | "no"` as
the first decision, plus `attention_indices`, exact commands seen,
boards/families, flags, files/docs, user corrections, and open questions. Send
only the `yes` records and low-confidence `no` records to GPT-5.5 for synthesis.
GPT-5.5 must verify parser syntax and firmware bindings against the repo before
writing docs.

Gatekeeper prompt template:

```text
You are a binary relevance classifier for FastLED AutoResearch documentation.

For this chat log, answer YES only if it contains evidence useful for improving
AutoResearch agent documentation, including:
- bash autoresearch command shapes
- board bringup
- ESP32/LPC/Teensy/generic board behavior
- fbuild port scan
- AutoResearch RPC/function bindings
- command-line args or mode flags
- user corrections about how agents should use AutoResearch

Answer NO if it only mentions unrelated coding work, generic FastLED work,
or hardware work without AutoResearch documentation value.

Return strict JSON only:
{
  "answer": "yes" | "no",
  "confidence": 0.0-1.0,
  "attention_indices": [message idx numbers],
  "reason": "one short sentence",
  "summary_for_gpt55": "2-4 sentence evidence summary, empty if no",
  "commands_seen": [],
  "boards_or_families": [],
  "flags_seen": [],
  "files_or_docs_mentioned": [],
  "user_corrections": [],
  "open_questions": []
}
```

### Board Bring-up Workflow

For a new board or a newly attached board, prove the transport before proving a
driver:

1. Run `fbuild port scan` and record the board identity, VID/PID, serial number,
   and port. Do not claim a board is absent without this scan.
2. Confirm the board has an entry in `ci/boards.py` or can be detected by the
   AutoResearch/fbuild path. If the deploy backend is missing, file the gap in
   FastLED/fbuild; do not add direct flash-tool calls.
3. Run GPIO-only AutoResearch first. With no driver flag, current
   `bash autoresearch` runs pin discovery plus toggle-capture where supported.
4. For constrained boards that use low-memory mode, prove the minimal RPC/serial
   path before adding driver benches. On LPC845 this is the echo bring-up.
5. Add one driver/API validation mode at a time, then rerun with the exact
   command that will appear in issue/PR closeout.

Canonical command shapes:

```bash
# Generic / auto-detected board: GPIO-only bring-up.
fbuild port scan
bash autoresearch --upload-port <port> --timeout 120s

# Generic known environment.
bash autoresearch <environment> --upload-port <port> --timeout 120s

# ESP32 family examples.
bash autoresearch esp32s3 --parlio --upload-port <port> --timeout 120s
bash autoresearch esp32s3 --lcd --lanes 2 --upload-port <port> --timeout 120s
bash autoresearch esp32p4 --lcd-rgb --upload-port <port> --timeout 120s
bash autoresearch esp32c6 --parlio --upload-port <port> --timeout 120s
bash autoresearch esp32s3 --net --upload-port <port> --timeout 120s
bash autoresearch esp32s3 --ota --upload-port <port> --timeout 120s
bash autoresearch esp32s3 --ble --upload-port <port> --timeout 120s

# LPC low-memory bring-up and targeted validation.
bash autoresearch lpc845 --upload-port <port> --timeout 120s --skip-lint
bash autoresearch lpc845 --pin-toggle-rx --upload-port <port> --timeout 120s --skip-lint
bash autoresearch lpc845 --ws2812-loopback --upload-port <port> --timeout 120s --skip-lint
bash autoresearch lpc845 --uart --upload-port <port> --timeout 120s --skip-lint
bash autoresearch lpc845 --dma-uart --upload-port <port> --timeout 120s --skip-lint
bash autoresearch lpc845 --fault-emit-test --upload-port <port> --timeout 120s --skip-lint

# Teensy 4.x bring-up and driver validation.
bash autoresearch teensy41 --upload-port <port> --timeout 120s
bash autoresearch teensy41 --object-fled --tx-pin <tx> --rx-pin <rx> --timeout 120s
bash autoresearch teensy41 --flex-io --tx-pin <tx> --rx-pin <rx> --timeout 120s
```

`<environment>` is the optional positional argument parsed by
`ci/autoresearch/args.py`; `--env <environment>` is the equivalent named form.
Use the positional form in docs unless the task specifically needs `--env`.

### Validation Levels

Pick the narrowest validation level that proves the claim:

- **Board transport:** `fbuild port scan` plus GPIO-only AutoResearch or
  low-memory echo. This proves build, deploy, reset, serial, and minimal RPC.
- **Basic features:** use existing special modes such as `--simd`,
  `--coroutine`, `--ieee754`, `--net`, `--ota`, `--ble`, or a direct
  `RpcClient` call to a bound method listed by `help` / `rpc.discover`.
- **Driver behavior:** use `bash autoresearch <board> --<driver>` and let the
  host generate JSON-RPC `runSingleTest` / `runParallelTest` commands.
- **Public FastLED API behavior:** route through the user-facing API under test
  (`FastLED.addLeds`, channels config, `Bus::AUTO`, explicit `Bus::UART`,
  etc.). Raw peripheral benches such as `--dma-uart` are supporting evidence,
  not replacements for public API validation.
- **Failure/safety behavior:** use dedicated modes such as
  `--fault-emit-test`; capture decisive `FAULT:`, `REMOTE:`, or `RESULT:` lines.

Closeout evidence must include the command, board, port, wiring, decisive
output, commit/PR URL, and issue URL. Stale chat comments, CI compile-only
results, and old serial buffers do not close hardware issues when a board can
be tested locally.

### Driver Development and New Modes

Add new AutoResearch functionality in the harness, not as a one-off sketch or
raw serial probe.

Firmware side:

1. For full-memory boards, add or reuse a handler in
   `examples/AutoResearch/AutoResearchRemote.cpp` or a helper included from
   `examples/AutoResearch/AutoResearch.ino`.
2. Register the method with `mRemote->bind("methodName", ...)` and update the
   `help` manifest in the same file so agents can discover it.
3. For low-memory boards, add the smallest possible binding in
   `examples/AutoResearch/AutoResearchLowMemory.h`, guarded by the relevant
   compile-time macro so unrelated benches stay out of flash.
4. Keep output machine-readable. Prefer JSON-RPC responses and `RESULT:` JSONL;
   do not add free-form `Serial.print` control paths.

Host side:

1. Add a command-line flag in `ci/autoresearch/args.py`.
2. Wire the flag in `ci/autoresearch/phases.py`, including any required staged
   build defines passed to `synthesise_autoresearch_project`.
3. Put reusable bench code in `ci/autoresearch/test_*.py` or a small helper
   module under `ci/autoresearch/`.
4. Use `ci.rpc_client.RpcClient` over the fbuild-backed serial interface. Do
   not open raw `pyserial` ports in bench or acceptance code.
5. Add or update parser/unit tests when the mode changes CLI behavior.

For channels-API or bus-selection work, read `src/fl/channels/README.md` before
choosing the test surface. Example: on LPC845, `--uart` validates the UART DMA
clockless loopback through the normal FastLED LED API; `--dma-uart` validates
the lower-level async UART TX DMA bench.

### Low-Memory vs Full-Memory Strategy

`examples/AutoResearch/AutoResearch.ino` is shared by constrained and rich
targets. It auto-enables `FASTLED_AUTORESEARCH_LOW_MEMORY` when
`FL_PLATFORM_HAS_LARGE_MEMORY == 0`.

Low-memory boards, such as LPC845/LPC804:

- Keep one bench per build. Flash/RAM budget may not fit SPI DMA, UART DMA,
  SCT/PWM DMA, fault tests, and WS2812 loopback together.
- Gate each feature with a macro such as `FASTLED_AUTORESEARCH_FAULT_TEST`,
  `FASTLED_AUTORESEARCH_LPC_UART_DMA`, `FASTLED_LPC_SPI_DMA`, or
  `FASTLED_LPC_PWM_DMA`.
- Use `--skip-lint` and `--quiet` when the target is constrained and the task
  is hardware validation rather than broad lint feedback.
- Prefer echo, pin-toggle, byte-match loopback, and focused fault tests over a
  full driver matrix.
- Treat silence as possible crash/reset/USART routing failure. First prove
  board, port, firmware freshness, and wiring; then add or use fault/crash
  diagnostics.

Full-memory boards, such as ESP32-family and Teensy 4.x:

- Use the full RPC harness and `AutoResearchRemote.cpp` bindings.
- Validate multiple drivers, lanes, strip sizes, custom colors, networking,
  OTA, BLE, decode, SIMD, and coroutine modes when relevant.
- Larger cases such as `--strip-sizes large` or `xlarge` belong here, not on
  LPC8xx-class parts.
- Driver development should converge on public API validation once raw
  peripheral probes have explained the hardware behavior.

### Choose GPIO-only or a Test Mode

Use no driver flag for GPIO-only board bring-up, or specify one or more flags
to validate a driver, subsystem, or special mode:
- `(none)` - GPIO-only mode: pin discovery + toggle capture where supported
- `--parlio` - Test parallel I/O driver
- `--rmt` - Test RMT (Remote Control) driver
- `--spi` - Test SPI driver
- `--uart` - Test UART driver
- `--lcd` - Test LCD_CLOCKLESS driver (ESP32-S3 only)
- `--lcd-spi` - Test LCD_SPI driver (ESP32-S3 only)
- `--lcd-rgb` - Test LCD RGB driver (ESP32-P4 only)
- `--object-fled` - Test ObjectFLED DMA driver (Teensy 4.x only)
- `--flex-io` - Test FlexIO clockless driver (Teensy 4.x only)
- `--flexio [0|1]` - Deprecated compatibility alias for `--flex-io`; default
  `0`, pass `1` to enable, emits a warning
- `--lpuart` - Deprecated compatibility alias for `--uart`; emits a warning
- `--all` - Test all implemented drivers for the selected board
- `--parallel` - Test multiple drivers simultaneously (requires 2+ drivers)
- `--ieee754` - Special mode: run integer IEEE 754 decimal codec verification without LED driver loopback
- `--simd`, `--coroutine`, `--net`, `--ota`, `--ble`, `--decode` - Special
  feature validation modes

### Usage Examples
```bash
# Test a specific driver, or omit driver flags for GPIO-only bring-up
bash autoresearch --parlio                    # Auto-detect environment
bash autoresearch esp32s3 --parlio            # Specify esp32s3 environment
bash autoresearch --rmt
bash autoresearch --spi
bash autoresearch --uart
bash autoresearch --lcd                       # ESP32-S3 LCD_CLOCKLESS
bash autoresearch --lcd-rgb                   # ESP32-P4 only
bash autoresearch lpc845 --ieee754            # LPC low-memory IEEE 754 codec check
                                              # (lpc845brk still works as a deprecated alias — #3220)

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

### LPC845 Fault-Emit Validation

Use this mode when validating the LPC845 crash diagnostics tracked by
[FastLED #3302](https://github.com/FastLED/FastLED/issues/3302). It builds the
low-memory AutoResearch sketch with `FASTLED_AUTORESEARCH_FAULT_TEST=1`, flashes
the attached LPC845, deliberately triggers OOM and stack-blow RPCs, and asserts
that the device emits a `FAULT:` diagnostic before reset.

```bash
fbuild port scan
bash autoresearch lpc845 --fault-emit-test --upload-port COM10 --timeout 120s --skip-lint
```

The test intentionally crashes the target twice. A pass means both triggers
emitted `FAULT:` diagnostics and the harness re-flashed between triggers. If it
fails with no `FAULT:` line, treat the LPC safety nets as regressed: consult
[FastLED #3300](https://github.com/FastLED/FastLED/issues/3300),
[FastLED #3302](https://github.com/FastLED/FastLED/issues/3302), and
[ArduinoCore-LPC8xx #30](https://github.com/zackees/ArduinoCore-LPC8xx/pull/30)
before debugging unrelated AutoResearch transport layers.

### Common Options
- `--skip-lint` - Skip linting for faster iteration
- `--timeout <seconds>` - Custom timeout (default: 120s)
- `--help` - See all options

### Build Backend
`bash autoresearch` uses fbuild for all board compiles. Do not use board-specific PlatformIO fallback paths for compatibility issues; file board build compatibility problems at https://github.com/FastLED/fbuild/issues.

### 🚨 Deploy Backend: fbuild ONLY

`bash autoresearch` MUST NOT invoke flash tools directly. The build → deploy → run sequence is:

1. `fbuild build --environment <env>` — fbuild produces `firmware.bin`.
2. `fbuild deploy --environment <env>` — fbuild flashes the board.
3. autoresearch opens the VCOM and runs the RPC bench.

Steps 1 and 2 are both fbuild's responsibility. autoresearch is a bench-orchestration tool; it is not a flasher. If fbuild lacks a deployer for your target, file the gap at https://github.com/FastLED/fbuild/issues and let autoresearch fail loudly — do **not** add a `pyocd`/`lpc21isp`/`esptool`/etc. call from autoresearch as a "temporary" fallback. The wedge story on 2026-07-01 (LPC-Link2 CMSIS-DAP v1 firmware hanging pyocd's Windows HID for 8 minutes) is exactly the class of bug that happens when deployment is scattered instead of centralised. Full rule: `agents/docs/build-system.md` → "Deployment (flash / upload) is fbuild's job — ALWAYS". The nxplpc deployer (lpc21isp UART ISP path) shipped in FastLED/fbuild#595 and was refined in #923 + #928; `bash autoresearch lpc845 ...` flows through it via `fbuild deploy` and never touches CMSIS-DAP HID.

### 🚨 Device serial: fbuild's Rust monitor ONLY — NEVER raw pyserial

**Never open a device serial port with `pyserial` (`import serial; serial.Serial(...)`) in autoresearch, bench runners, or any CI code.** All device serial I/O goes through fbuild's native (Rust) serial monitor:

```python
from ci.rpc_client import RpcClient
from ci.util.serial_interface import create_serial_interface

iface = create_serial_interface(port)          # FbuildSerialAdapter (Rust) by default
client = RpcClient(port, serial_interface=iface)
await client.connect()
resp = await client.send("echo", args=[42])    # mandatory JSON-RPC id correlation
```

`create_serial_interface(...)` returns the fbuild-backed `FbuildSerialAdapter` unless `use_pyserial=True` is *explicitly* passed (which nothing in autoresearch should do). `RpcClient` on top of it gives correct framing, mandatory request-id correlation, retries, crash-trace decoding, and reconnect.

**Why the rule exists** (silicon-diagnosed 2026-07-04): the LPC845 SPI/UART bench runners were hand-rolling raw `pyserial` request/response loops. On Windows pyserial these were unreliable — replies were dropped roughly one-per-session (`in_waiting` under-reports, `reset_input_buffer()` before a write races the reply, byte-at-a-time reads straddle the port timeout). The firmware answered every RPC correctly; the *pyserial transport* lost them. Routing through `RpcClient`/fbuild's Rust monitor — the same path `ble.py`, `decode.py`, `driver_sweep.py`, and the coroutine tests already use — makes device serial deterministic.

The only sanctioned pyserial touch is fbuild's own `PySerialAdapter` fallback inside `ci/util/serial_interface.py` (selected only via `use_pyserial=True`) and the DTR-reset helper there. Application/bench code must go through `create_serial_interface` + `RpcClient`. See also `agents/docs/python-standards.md` → "Device serial".

### Synthesised `platformio.ini` (no root dependency)

Since #3281, `bash autoresearch` synthesises its own `.build/pio/<board>/platformio.ini` from `ci/boards.py` before launching fbuild — the same pattern `bash compile` already uses. **Root `./platformio.ini` is NOT consulted in the default path.** The staged tree under `.build/pio/<board>/` contains:

- `platformio.ini` — generated from `Board.to_platformio_ini()` for the resolved board.
- `src/sketch/` — populated by copying `examples/AutoResearch/`.

Board selection happens in this order:
1. Positional environment (`bash autoresearch esp32c6 ...`) — synthesised immediately at parse time.
2. `--env <name>` — same as positional.
3. `--lcd` / `--lcd-spi` / `--lcd-rgb` — implies `esp32s3` / `esp32p4`; synthesised immediately.
4. Auto-detect from attached USB device — synthesis deferred until after `detect_attached_chip()`.

**Legacy escape hatch (deprecated):** `--use-root-platformio-ini` re-enables the old behavior of reading root `./platformio.ini` instead of synthesising. It is not allowed for Teensy AutoResearch acceptance or any ObjectFLED/FlexIO bring-up run. Use only for non-Teensy diagnostic comparisons when you have local edits to root `./platformio.ini` that the synthesised file misses — in which case the correct fix is to move those edits into `ci/boards.py`.

```bash
# Default (synthesised):
bash autoresearch esp32c6 --parlio

# Legacy (consumes root ./platformio.ini, deprecated):
bash autoresearch esp32c6 --parlio --use-root-platformio-ini
```

For Teensy 4.x validation, always use the synthesised fbuild path:

```bash
bash autoresearch teensy41 --object-fled --tx-pin 8 --rx-pin 9 --timeout 120
```

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
bash autoresearch --lcd --strip-sizes 500              # Test with single 500 LED strip

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
bash autoresearch --lcd --lanes 4                      # Test with exactly 4 lanes

# Test with lane range
bash autoresearch --rmt --lanes 1-4                    # Test with 1 to 4 lanes (tests all combinations)
bash autoresearch --spi --lanes 2-8                    # Test with 2 to 8 lanes

# Set per-lane LED counts (NEW)
bash autoresearch --lcd --lane-counts 100,200,300      # 3 lanes with 100, 200, 300 LEDs per lane
bash autoresearch --parlio --lane-counts 50,100        # 2 lanes with 50 and 100 LEDs per lane

# Combined with strip sizes
bash autoresearch --lcd --lanes 2 --strip-sizes 100,300  # 2 lanes, strips of 100 and 300 LEDs
```

**Default:** 1-8 lanes (firmware default)

### Color Pattern Configuration
Configure custom RGB color pattern for autoresearch testing:
```bash
# Custom color patterns (hex RGB format)
bash autoresearch --parlio --color-pattern ff00aa      # Pink color (RGB: 255, 0, 170)
bash autoresearch --rmt --color-pattern 0x00ff00       # Green color (RGB: 0, 255, 0)
bash autoresearch --lcd --color-pattern 112233         # Dark blue (RGB: 17, 34, 51)

# Combined with lane configuration
bash autoresearch --parlio --lane-counts 100,200 --color-pattern ff0000  # 2 lanes, red color
```

**Note:** Custom color patterns require firmware support via the `setSolidColor` RPC command. This command may need to be implemented in the firmware if not already available.

### Error Handling
If you run `bash autoresearch` without specifying a driver or special mode, it
runs GPIO-only board bring-up. Use this intentionally for first contact with a
new board or when validating the build/deploy/serial/RPC path before a driver
bench.

```bash
bash autoresearch                       # Auto-detect device, GPIO-only bring-up
bash autoresearch teensy41              # Known environment, GPIO-only bring-up
bash autoresearch lpc845 --upload-port COM10 --timeout 120s --skip-lint
```

Use a driver or special-mode flag when the claim is about a specific subsystem:

```bash
bash autoresearch esp32s3 --parlio
bash autoresearch --rmt --spi
bash autoresearch --all
bash autoresearch lpc845 --uart --upload-port COM10 --timeout 120s --skip-lint
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
| `findConnectedPins` | `[]` or `[{startPin, endPin, autoApply}]` (all optional) | `{success, found, txPin, rxPin, autoApplied, testedPairs}` (or `{error, message}` on invalid range) |

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

## FlexIO RX Backend (Teensy 4 — opt-in)

A second RX capture backend exists for the Teensy 4.x family alongside the default FlexPWM path. It uses the iMXRT1062 **FLEXIO1** peripheral with a 16-bit timer in edge-restart mode and an eDMA shifter for raw inter-edge tick capture. Brought up across PRs #2765 (skeleton), #2766 (FLEXIO1 hardware), #2767 (square-wave bench), and #2769 (ObjectFLED loopback) per the [#2764](https://github.com/FastLED/FastLED/issues/2764) plan.

### Selecting the backend

Two opt-in paths — both leave `RxBackend::PLATFORM_DEFAULT` resolving to FlexPWM, so existing AutoResearch flows are untouched:

```cpp
// C++ from a sketch — explicit RxBackend
fl::RxChannelConfig cfg(rx_pin, fl::RxBackend::FLEXIO);
auto rx = fl::RxChannel::create(cfg);
```

Raw pyserial RPC scripts are diagnostics only. They are not acceptance criteria for Teensy ObjectFLED/FlexIO bring-up, and they must not be open while `bash autoresearch` owns the port.

### Pin support

Phase 1B ships with a minimal pin map (`kFlexIo1Pins[]` in `src/platforms/arm/teensy/teensy4_common/rx_flexio_channel.cpp.hpp`). The first entry covers the dev-bench GPIO 3↔4 jumper used to validate the bring-up:

| Teensy pin | FLEXIO1 input | IOMUXC `SW_MUX_CTL` | IOMUXC `SW_PAD_CTL` |
|---|---|---|---|
| 4 | `FLEXIO1_FLEXIO06` (ALT4) | `0x401F802C` | `0x401F821C` |

Adding a pin: append a row mapping its Teensy digital number to its FLEXIO1 input index and the matching IOMUXC offsets. The pad config is set to ALT4 + 100 kΩ pull-up keeper + hysteresis for clean edge detection.

### Peripheral resource budget

| Resource | Use |
|---|---|
| FLEXIO1 Shifter 0 | Receive-mode, PINSEL = mapped FLEXIO1 input |
| FLEXIO1 Timer 0 | 16-bit counter, TRGSEL = `2*pin+1`, TIMRST = trigger-rising |
| eDMA channel | Auto-allocated by `DMAChannel` |
| `DMAMUX_SOURCE_FLEXIO1_REQUEST0` | DMA request driven by Shifter 0 status flag |
| FLEXIO1 functional clock | PLL3_PFD3 (480 MHz) / PRED=2 / PODF=2 → ~120 MHz (~8.3 ns/tick) |
| FlexIO **2** | Untouched — owned by the existing FastLED WS2812 TX driver (`flexio_driver.cpp.hpp`) |
| FlexIO **3** | Untouched — free for future use |

### Verification

- **`flexioRxBenchmark`** — diagnostic RPC that drives a square wave via `analogWriteFrequency` and reports per-period mean / sigma / min / max nanoseconds. The raw script is useful for investigation only; acceptance must be routed through canonical `bash autoresearch`.
- **`flexioObjectFledTest`** — diagnostic RPC that drives a WS2812 pattern via `Bus::FLEX_IO` slot 0 and decodes the captured edge stream against `TIMING_WS2812B_V5`. The raw `ci/autoresearch/test_flexio_rx_objectfled.py` script is not an acceptance path; use it only when deliberately debugging outside a canonical run, and close it before `bash autoresearch`.

`RxBackend::PLATFORM_DEFAULT` switching from FlexPWM to FlexIO on Teensy 4 is intentionally **deferred** to a follow-up PR — keeps the bench-validation surface small and the existing FlexPWM-based AutoResearch flows untouched until both backends are equally exercised.

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
