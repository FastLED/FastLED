# External MCP Servers, Plugins & Community Skills

## Overview

Research conducted 2026-03-20 evaluating the ecosystem of MCP servers, Claude Code plugins, and community skills for embedded C++ (FastLED) development.

---

## Recommended Plugins (Tier 1 — Adopt)

### 1. clangd-lsp (Official Anthropic)

**Install:** `/plugin install clangd-lsp@claude-plugins-official`

C/C++ language server integration providing: `goToDefinition`, `goToImplementation`, `hover`, `documentSymbol`, `findReferences`, `workspaceSymbol`, `prepareCallHierarchy`, `incomingCalls`, `outgoingCalls`.

**Requirements:**
- `clangd` installed (via LLVM, winget, apt, brew)
- `compile_commands.json` in project root (generate via meson: `meson setup builddir && cp builddir/compile_commands.json .`)

**Value:** Dramatically improves symbol navigation across FastLED's template-heavy code, 1700+ line SPI driver, and platform abstraction layers.

### 2. Trail of Bits: testing-handbook-skills

**Install:** `/plugin marketplace add trailofbits/skills`

15 sub-skills: `address-sanitizer`, `harness-writing`, `fuzzing-obstacles`, `coverage-analysis`, `libfuzzer`. ASan is directly applicable to C++ unit tests. Fuzzing harnesses could test LED encoding paths (wave8, RMT).

### 3. Trail of Bits: sharp-edges

Identifies error-prone APIs and "footgun" designs. FastLED's public API (CRGB, CLEDController, XYMap) is used by thousands — finding misuse-prone patterns prevents user bugs.

---

## Recommended Plugins (Tier 2 — Worth Trying)

### 4. Trail of Bits: static-analysis

CodeQL and Semgrep skills for C/C++. Could find buffer overflows, integer overflow, and null pointer issues in SPI/RMT driver code.

### 5. code-review (Official Anthropic)

**Install:** `/plugin install code-review@claude-plugins-official`

Multi-agent PR review: launches 4 parallel agents (2x CLAUDE.md compliance, 1x bug detection, 1x git history analysis). Filters to confidence >= 80. More sophisticated than single-pass review.

### 6. obra/superpowers (Selective)

**Install:** `/plugin marketplace add obra/superpowers-marketplace`

101K+ stars. Cherry-pick `systematic-debugging` and `verification-before-completion` skills only. Full install may conflict with existing CLAUDE.md workflow.

---

## MCP Servers Evaluated

### Serial Port (Best Available)

**es617/serial-mcp-server** — Python, cross-platform, MIT license
- Non-blocking serial with stateful sessions
- DTR/RTS control, hex/base64 formats, protocol specs
- Install: `pip install serial-mcp-server && claude mcp add serial -- serial_mcp`
- **Verdict: OPTIONAL.** Useful for interactive serial debugging but `bash autoresearch` already handles this.

### Arduino (Not Applicable)

**hardware-mcp/arduino-mcp-server** — 21 tools wrapping `arduino-cli`
- **Verdict: NOT RECOMMENDED.** FastLED development builds use fbuild and the repository's maintained wrappers, not arduino-cli.

### Embedded Debugging

**Adancurusul/embedded-debugger-mcp** — 60 stars, Rust, probe-rs debugging
- **Verdict: NOT RELEVANT.** JTAG/SWD probe-based for ARM Cortex-M. ESP32 uses different debug infrastructure.

### fbuild MCP

fbuild provides its own MCP server through `fbuild mcp`, so a separate
PlatformIO integration is unnecessary.

### On-Device MCP (ESP32 as server)

**quantumnic/mcpd**, **solnera/esp32-mcpserver** — MCU-hosted MCP servers
- **Verdict: NOT RELEVANT.** Wrong paradigm — we need to develop firmware, not control hardware from LLM.

---

## Community Skills Assessment

### Most Valuable (from Trail of Bits)

| Skill | Applicability | What it Does |
|-------|--------------|--------------|
| testing-handbook-skills | HIGH | ASan, fuzzing, coverage analysis for C++ |
| sharp-edges | HIGH | Error-prone API detection |
| static-analysis | MEDIUM-HIGH | CodeQL/Semgrep for C/C++ |
| variant-analysis | MEDIUM | Find similar vulnerability patterns across codebase |
| differential-review | MEDIUM | Security-focused PR review with git history |
| property-based-testing | MEDIUM | Property-based testing methodology |

### Ecosystem Gaps (No Community Solution)

These capabilities have no community skill and FastLED's custom tools are superior:

1. **C++ Embedded Memory Safety** — ISR volatile checking, DMA alignment, stack depth analysis
2. **LED Protocol Timing Analysis** — SPI/RMT clock verification, T1/T2/T3 validation
3. **PlatformIO/Meson Build Optimization** — Cross-compilation matrix, unity build tuning
4. **C++ API Documentation** — Doxygen-aware header documentation generation

FastLED's existing skills (`/memory-audit`, `/timing-analysis`, `/platform-port`) already cover these gaps better than anything available in the community.

---

## LSP Integration Notes

### Setting Up clangd for FastLED

1. Generate compile_commands.json:
   ```bash
   bash test --cpp  # This creates a meson build directory
   cp .build/host_tests/compile_commands.json .
   ```

2. Install clangd plugin:
   ```
   /plugin install clangd-lsp@claude-plugins-official
   ```

3. Optional: Create `.clangd` config for project-specific settings:
   ```yaml
   CompileFlags:
     Add: [-std=c++17]
   Diagnostics:
     ClangTidy:
       Add: [bugprone-*, performance-*, modernize-*]
   ```

### tweakcc (Fallback)

If LSP has issues on Windows: `npx tweakcc --apply` from Piebald-AI/tweakcc patches Claude Code's LSP support.

---

## Summary Decision Matrix

| Tool | Action | Reason |
|------|--------|--------|
| clangd-lsp | **ADOPT** | C++ symbol navigation is a force multiplier |
| Trail of Bits testing-handbook | **ADOPT** | ASan/fuzzing directly applicable |
| Trail of Bits sharp-edges | **ADOPT** | API quality for public library |
| Trail of Bits static-analysis | **TRY** | CodeQL/Semgrep for vulnerability scanning |
| code-review plugin | **TRY** | Multi-agent review with confidence scoring |
| superpowers (selective) | **TRY** | systematic-debugging, verification skills |
| serial-mcp-server | **OPTIONAL** | Only if interactive serial debugging needed |
| arduino-mcp-server | **SKIP** | Wrong build system (arduino-cli vs fbuild) |
| embedded-debugger-mcp | **SKIP** | Wrong debug infrastructure (probe-rs vs ESP tools) |
| rohitg00 toolkit | **REFERENCE** | Use as discovery catalog, not direct install |
