# Xtensa Assembly Code Review

Perform comprehensive manual review of Xtensa assembly code in FastLED ESP32 platforms for correctness, ABI compliance, safety, and performance.

## What This Does

Launches the **code_review_asm_xtensa-agent** to manually review Xtensa assembly code using expert knowledge of:
- Xtensa ISA (LX6/LX7 instruction sets)
- Windowed vs Call0 ABI requirements
- ESP32 hardware constraints (alignment, MMIO, IRAM)
- Common assembly bugs and optimization patterns

## When to Use

Run this after making changes to:
- Inline assembly (`__asm__`, `asm volatile`)
- Standalone assembly files (`.S`, `.s`)
- ESP32 platform-specific code in `src/platforms/esp/`
- Interrupt handlers or timing-critical LED code

## What Gets Checked

### Critical Errors (Must Fix)
- **E001**: Invalid `movi` for 32-bit addresses (assembler error)
- **E002**: Wrong return instruction in interrupts (`ret`/`retw` instead of `rfi`)
- **E003**: Missing `memw` memory barriers for MMIO writes
- **E004**: Windowed ABI used in interrupt handlers (corrupts context)
- **E005**: Unaligned memory access (alignment exceptions)
- **E006**: Missing `IRAM_ATTR` on interrupt handlers (flash crashes)
- **E007**: Mixed ABI usage in same function (register corruption)

### Performance Warnings
- **W001**: Load-use hazards (pipeline stalls)
- **W002**: Excessive loop overhead (small loops)
- **W003**: Deep loop nesting (3+ levels)
- **W004**: High register pressure (>12 live registers)
- **W005**: Long dependency chains (5+ sequential deps)

### Optimization Opportunities
- **O001**: Inefficient array indexing (use `addx4/addx8`)
- **O002**: Predictable branches (use conditional moves)
- **O003**: Loop unrolling opportunities
- **O004**: Code Density narrow instructions (`.n` variants)

## Example Usage

```bash
# Review Xtensa assembly changes
/code_review_asm_xtensa
```

The agent will:
1. Find all changed files with assembly code
2. Extract and analyze each assembly block
3. Check against Xtensa ISA rules and ESP32 constraints
4. Report errors, warnings, and optimization suggestions

## Output Format

```
## Xtensa Assembly Review

### Files Analyzed
- src/platforms/esp/esp32_clockless.cpp (3 asm blocks)
- src/platforms/esp/interrupt_handler.S (1 file)

### Critical Errors (Must Fix)
[Detailed error reports with file:line, code snippets, and fixes]

### Performance Warnings
[Warning reports with cycle impact and recommendations]

### Optimization Opportunities
[Suggestions with estimated performance improvements]

### Summary
- Total: X errors, Y warnings, Z optimizations
- Action required: [Fix errors immediately / All clear]
```

## Key Review Points

### ABI Compliance
- **Windowed ABI**: Normal functions use `entry`/`retw` with window rotation
- **Call0 ABI**: Interrupts use manual stack management, `ret`/`rfi` returns

### Memory Safety
- **Alignment**: `l32i/s32i` requires 4-byte, `l16ui/s16i` requires 2-byte alignment
- **MMIO barriers**: Write to `0x3FF00000-0x3FFFFFFF` must be followed by `memw`
- **IRAM placement**: Interrupt handlers need `IRAM_ATTR` attribute

### Register Usage
- **Windowed ABI**: `a0`=retaddr, `a1`=SP, `a2-a7`=args, window rotates on call
- **Call0 ABI**: Manual preservation, caller-saved `a0,a2-a11,a15`, callee-saved `a12-a14`

### Shift Operations
- **SAR register**: Must set with `ssl`/`ssr` before `sll`/`srl`/`sra`
- **Immediate shifts**: Use `slli`/`srli`/`srai` to avoid SAR dependency

## Follow-Up Actions

After review:
1. **Fix all E-level errors** - These will crash or fail to compile
2. **Investigate W-level warnings** - May indicate bugs or performance issues
3. **Consider O-level optimizations** - Balance clarity vs performance gains
4. **Re-compile and test**: `uv run ci/ci-compile.py esp32dev --examples <Name>`

## Resources

- **Agent definition**: `.claude/agents/code_review_asm_xtensa-agent.md`
- **ISA reference**: `Xtensa.txt` (Espressif Xtensa ISA Overview)
- **ESP32 docs**: https://docs.espressif.com/projects/esp-idf/en/latest/
