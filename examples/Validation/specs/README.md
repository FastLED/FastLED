# Validation JSON Spec Files

This directory contains JSON specification files for automated LED validation testing.

## Available Specs

### quick_test.json
**Purpose:** Fast validation check with minimal configuration
**Use case:** Quick smoke test, CI pre-commit checks
**Config:** PARLIO driver, 2 lanes @ 100 LEDs each, 1 iteration

```bash
bash validate examples/Validation/specs/quick_test.json
```

### full_matrix.json
**Purpose:** Comprehensive multi-driver validation with various lane configurations
**Use case:** Full regression testing, release validation
**Config:** All drivers (PARLIO, RMT, SPI, UART), 9 lane configurations, 4 patterns, 3 iterations
**Duration:** ~15-30 minutes depending on hardware

```bash
bash validate examples/Validation/specs/full_matrix.json
```

### edge_cases.json
**Purpose:** Stress test edge cases and unusual configurations
**Use case:** Driver robustness testing, buffer management validation
**Config:** PARLIO + RMT, extreme asymmetry (1 LED vs 300 LEDs), 5 iterations

```bash
bash validate examples/Validation/specs/edge_cases.json
```

## JSON Spec Format

```json
{
  "version": 1,
  "description": "Human-readable description",
  "drivers": ["PARLIO", "RMT"],
  "laneConfigs": [
    [100],           // Single lane, 100 LEDs
    [100, 100],      // Two lanes, 100 LEDs each
    [300, 200, 100]  // Three lanes, asymmetric sizes
  ],
  "patterns": ["MSB_LSB_A", "SOLID_RGB"],
  "iterations": 3,
  "options": {
    "timeout": 120,
    "stopOnFirstFailure": false,
    "verbose": false
  }
}
```

### Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `version` | int | Yes | Spec format version (currently `1`) |
| `description` | string | No | Human-readable test suite description |
| `drivers` | string[] | Yes | Drivers to test: `"PARLIO"`, `"RMT"`, `"SPI"`, `"UART"` |
| `laneConfigs` | int[][] | Yes | Array of lane size configurations |
| `patterns` | string[] | No | Test patterns (default: `["MSB_LSB_A"]`) |
| `iterations` | int | No | Iterations per config (default: `1`) |
| `options.timeout` | int | No | Timeout in seconds (default: `60`) |
| `options.stopOnFirstFailure` | bool | No | Stop on first failure (default: `false`) |
| `options.verbose` | bool | No | Verbose output (default: `false`) |

## Lane Configurations

Each entry in `laneConfigs` is an array specifying LED count per lane:

- `[100]` - Single lane with 100 LEDs
- `[100, 100]` - Two lanes, 100 LEDs each (uniform)
- `[300, 200, 100, 50]` - Four lanes with decreasing LED counts (asymmetric)
- `[1, 300]` - Edge case: 1 LED on lane 0, 300 LEDs on lane 1

## Available Patterns

- `MSB_LSB_A` - R=0xF0, G=0x0F, B=0xAA (MSB/LSB bit testing)
- `MSB_LSB_B` - R=0x55, G=0xFF, B=0x00
- `MSB_LSB_C` - R=0x0F, G=0xAA, B=0xF0
- `SOLID_RGB` - Solid alternating R/G/B pixels
- `GRADIENT` - Smooth gradient across strip
- `ALL_WHITE` - All LEDs white (255,255,255)
- `ALL_BLACK` - All LEDs off (0,0,0)

## Output Format

Results are output as JSONL (JSON Lines) with `RESULT: ` prefix:

```
RESULT: {"type":"test_start","timestamp":1705500000,"spec":"./quick_test.json","totalConfigs":1}
RESULT: {"type":"config_start","index":0,"driver":"PARLIO","laneSizes":[100,100],"pattern":"MSB_LSB_A","totalLeds":200}
RESULT: {"type":"config_result","index":0,"driver":"PARLIO","laneSizes":[100,100],"pattern":"MSB_LSB_A","passed":true,"durationMs":150}
RESULT: {"type":"test_complete","timestamp":1705500010,"totalConfigs":1,"passed":1,"failed":0,"success":true}
```

### Parsing Results

**Filter just results:**
```bash
bash validate specs/quick_test.json 2>&1 | grep "^RESULT: " | sed 's/^RESULT: //'
```

**Parse with jq:**
```bash
bash validate specs/quick_test.json 2>&1 | grep "^RESULT: " | sed 's/^RESULT: //' | \
  jq 'select(.type == "test_complete")'
```

**Check for failures:**
```bash
bash validate specs/quick_test.json 2>&1 | grep "^RESULT: " | sed 's/^RESULT: //' | \
  jq 'select(.type == "config_result" and .passed == false)'
```

## Creating Custom Specs

To create a new spec file:

1. Copy an existing spec file as a template
2. Modify `drivers`, `laneConfigs`, `patterns` as needed
3. Adjust `iterations` and `timeout` based on test complexity
4. Save with `.json` extension
5. Run: `bash validate path/to/your_spec.json`

### Example: Custom Spec for PARLIO Only

```json
{
  "version": 1,
  "description": "PARLIO-specific validation with 8 lanes",
  "drivers": ["PARLIO"],
  "laneConfigs": [
    [100, 100, 100, 100, 100, 100, 100, 100]
  ],
  "patterns": ["MSB_LSB_A", "SOLID_RGB"],
  "iterations": 2
}
```
