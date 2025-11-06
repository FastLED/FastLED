---
description: Review RISC-V assembly code for correctness and best practices
---

Review RISC-V assembly code for correctness, adherence to conventions, and best practices.

Use the 'riscv-review-agent' sub-agent to perform expert-level RISC-V assembly review with comprehensive ISA knowledge.

The agent will:
- Search for RISC-V assembly code in git changes (inline asm or .S files)
- Perform deep analysis across 8 categories (instructions, registers, memory, control flow, CSRs, directives, common issues, platform-specific)
- Reference the comprehensive RISC-V Assembly Language Programmer Manual (risc-v-asm-manual.txt)
- Provide detailed, actionable feedback with fixes and specification references

## What Gets Reviewed

- **Instruction correctness**: Valid syntax, proper operands, immediate ranges
- **Register conventions**: ABI compliance, saved vs temporary registers, stack alignment
- **Memory access**: Alignment requirements, sign extension, offset limits
- **Control flow**: Branches, jumps, function calls, returns
- **CSR operations**: Privilege level access, atomic semantics, field types
- **Assembler directives**: Sections, alignment, symbol visibility, data placement
- **Common issues**: Misalignment, stack corruption, clobbered registers, illegal immediates
- **Platform-specific**: RV32I vs RV64I, extensions (M/A/F/D/C), ABI variants

## Expected Output

The agent provides structured feedback:
- ✅ Correct patterns identified
- ⚠️ Warnings with recommendations
- ❌ Errors with fixes and manual references
- Summary with overall assessment (PASS/FAIL)
