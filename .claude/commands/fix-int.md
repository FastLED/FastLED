---
description: Fix integer type definitions for specified platform
argument-hint: <platform>
---

Research and fix integer type definitions (fl::i8, fl::u8, fl::i16, fl::u16, fl::i32, fl::u32, fl::i64, fl::u64) for the specified platform.

Platform: $1

Use the 'fix-int-agent' sub-agent to research the correct primitive type mappings for the target platform and apply fixes to platform-specific int header files.

IMPORTANT:
- Do NOT modify fl/int.h or fl/stdint.h
- Only modify files in src/platforms/**/int*.h
- If the task requires modifying protected files, halt and report
