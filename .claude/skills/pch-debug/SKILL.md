---
name: pch-debug
description: Debug precompiled-header (PCH) failures in FastLED host builds. Use whenever a build or test fails with "redefinition of", "template parameter redefines default argument", "previous definition is here" or "has been modified since the precompiled header ... was built" in a header that test_pch.h pulls in; whenever `bash test` only works after `--clean`; whenever an agent suspects a stale PCH, a path-spelling mismatch, #pragma once not deduplicating, zccache serving bad objects, or test_pch.h.pch, -include-pch, -fpch-validate-input-files-content, compile_pch.py or .d.cache are involved. Finds the mechanism with evidence before anything is cleaned or changed.
argument-hint: [failing test or TU path, optional]
---

# Debugging FastLED PCH failures

Arguments: $ARGUMENTS

## The rules

**Do not run `bash test --clean` first.** It destroys the stale PCH, the
sidecars and the mtimes, which are the only evidence. Probe first, then clean.

**Never disable PCH timestamps. `-fno-pch-timestamp` (or `-Xclang
-fno-pch-timestamp`) is banned in FastLED, permanently.** It makes the PCH
record mtime 0, so clang stops checking input timestamps. A touched or
restored header then no longer fails, which looks like a fix, but it is unsafe
and it masks problems: the staleness guard that should have rebuilt the PCH is
still broken, and you have only silenced the signal that told you so. FastLED
keeps strict PCH settings (`-Werror=invalid-pch`, `-verify-pch`, timestamps
on). Fix the guard that missed the change; never weaken the validation. The
same goes for anything else that relaxes PCH validation to make an error go
away.

`bash lint` enforces this (`ci/lint/pch_strict_flags.py`):
`-fno-pch-timestamp` may not appear anywhere in the build surface. Every PCH
producer must pass `-Werror=invalid-pch`, `-verify-pch` and
`-fpch-validate-input-files-content`, and every `-include-pch` consumer must
pass `-Werror=invalid-pch` and `-fpch-validate-input-files-content`. Do not add
an exemption to make a PCH error or a lint failure go away.

## What the symptom means

The test PCH (`.build/meson-<mode>/tests/test_pch.h.pch`) already contains
FastLED.h and everything it includes. A consumer TU fails with
`redefinition of 'X'` / `redefines default argument` **only if clang included
a header from disk although the PCH already holds it**.

### How clang decides (clang 21.x, `clang/lib/Serialization/ASTReader.cpp`)

For every input header, the PCH records **spelling, size, mtime** and an
**xxh3 content hash** (`INPUT_FILE` / `INPUT_FILE_HASH` records; see them with
`llvm-bcanalyzer`). The mtime that matters is **the header's mtime as recorded
inside the PCH**, compared with the header's mtime now. The PCH file's own
mtime plays no part. Clang uses the pair in **two independent places**:

1. **"Is the PCH valid?"** (`ASTReader::getInputFile`). Compare each input's
   current size and mtime with the record:
   - size differs: invalid, `has been modified ... size changed`;
   - mtime differs: invalid, `mtime changed (was A, now B)`. **But with
     `-fpch-validate-input-files-content`**, clang then hashes the file and
     compares it with the stored xxh3. If they match, the change is forgiven
     and the PCH is accepted.
2. **"Have I already included this header?"** (`HeaderFileInfoTrait`). When
   the preprocessor reaches `#include "fl/stl/malloc.h"`, it looks up that
   header's saved state in the PCH (included, `#pragma once`). The lookup key
   is the file's **current** `(size, mtime)`: `ComputeHash` hashes exactly
   those two, and `EqualKey` rejects on a size or mtime mismatch. Only then
   does it compare names: an identical absolute name, or both names
   resolving to the same file through clang's FileManager.

The content flag relaxes check 1 and **not** check 2. A byte-identical header
whose mtime no longer equals the recorded one is therefore half-accepted: the
PCH loads, the lookup misses, clang no longer knows the header is `#pragma
once` and already included, and it parses it again. Hence the redefinition.

| Case | Check 1 (valid?) | Check 2 (lookup) | Result |
|---|---|---|---|
| fresh PCH | pass | hit | OK |
| header mtime ≠ recorded, content identical, strict flags (no content flag) | fail | (n/a) | clear "mtime changed" error |
| same, **with** `-fpch-validate-input-files-content` (FastLED today) | pass (hash) | **miss** (mtime) | `redefinition` |
| same, plus an `#ifndef` guard | pass | miss | "works": the guard macro from the PCH skips the body, so the bug is **hidden** |
| same, but the **PCH file** touched newer | pass | **miss** | `redefinition`: the PCH's own mtime is in neither check |
| content edited | fail | (n/a) | clear "content/size changed" error |
| spelling the consumer cannot resolve to the recorded file | pass | **miss** (names don't resolve to one file) | `redefinition` |

So:
- `#pragma once` failing is the early warning; it is doing its job. An
  `#ifndef` include guard dedupes by macro and **hides** the problem. **Never
  "fix" a PCH redefinition by adding include guards** (as #2325 did, masking a
  Windows spelling problem).
- **The trigger is always a header whose current `(size, mtime)` or
  resolvable name differs from what the PCH recorded for it.** Not "newer
  than the PCH": an mtime moved **backwards** (a restore, `touch -d`, an
  archive extract) breaks the lookup just the same.
- **Touching the PCH file fixes nothing** (it is in neither check), and it
  makes the stale PCH look fresh to the build tool and our guards. Don't.
- **Valid fixes** rebuild the PCH on content changes or supply unchanged
  headers through zccache's private VFS snapshots with the recorded timestamps.
  Never restore a shared source header's timestamp: this can hide a real edit
  from another build directory's ordinary objects. Every PCH producer and
  consumer must run through zccache. Disabling timestamps remains banned.

### Mechanisms in FastLED's history

| Mechanism | How it looks in the probe | Past cases |
|---|---|---|
| **Header mtime differs from the record.** Touch, `git checkout`, formatter, edit-and-revert, a cache or snapshot restore. | `inputs` lists it under "size/mtime differ from the PCH record", usually **same size, different mtime**; `tu` shows it re-opened under the **same spelling** | #4839 / #4843 (touched header); #2995 / zccache#710 (snapshot restore); #4437 (staleness check skipped on header edits) |
| **Path spelling drift.** The PCH recorded one spelling and the consumer's spelling does not resolve to the same file (seen on Windows: `src\fl/stl/cstdio.h`). | `tu` shows the header re-opened with **DIFFERENT spelling** | #2685 / zccache#619; #3813 (the PCH cache key could not see a changed spelling) |

Both land in the same place: a staleness guard in `ci/compile_pch.py` or the
build plumbing that is **blind to one kind of change**. Previous misses were
content-hash only (#3813, #4839), no compiler flags in the key (#3813), checks
gated on the build file's mtime (#4437), and snapshot restores (#2995). The
current guard compares dependency content and compiler flags before reuse.
zccache supplies the recorded input metadata through private snapshots. When
you find a new case, check that the consumer actually runs through zccache and
that the snapshot content still matches the source; never turn validation off.

## Procedure

Run from the repo root, against the build dir that failed (`--build-dir
.build/meson-debug` for debug builds).

1. **Dump what the PCH recorded and check it for staleness:**
   ```bash
   uv run python .claude/skills/pch-debug/pch_probe.py inputs
   ```
   This reports odd spellings in `src/` or `tests/`, files recorded under two
   spellings, stale inputs and missing inputs. It reads the PCH with
   `llvm-bcanalyzer -dump` when available (on PATH, or `LLVM_BCANALYZER=`;
   on NixOS it comes with `llvmPackages.llvm`). That gives every input's
   recorded spelling, **size and mtime**, the exact values clang validates
   against, without clang's validation, which refuses the very PCH you are
   debugging. Same size with a different mtime is the "touched, content
   identical" signature, which breaks check 2 above. The probe reports any
   difference from the record, whether the mtime moved forwards or backwards.

   Without the tool it falls back to `clang -module-file-info`, which fails on
   a stale PCH. The probe then prints clang's refusal (it names the stale
   input and `mtime changed (was A, now B)`) and recovers the paths from the
   PCH bytes.

   To read the records by hand:
   `llvm-bcanalyzer -dump <pch> | grep -A1 'INPUT_FILE '`. In each
   `<INPUT_FILE ... op1=SIZE op2=MTIME .../> blob data = 'PATH'`, the blob is
   the spelling the PCH recorded.

2. **Probe the failing TU exactly as the build compiles it:**
   ```bash
   uv run python .claude/skills/pch-debug/pch_probe.py tu tests/fl/stl/adl_barrier.cpp
   uv run python .claude/skills/pch-debug/pch_probe.py tu adl_barrier --zccache .venv/bin/zccache
   ```
   This takes the TU's entry from `compile_commands.json`, runs it with `-H
   -fsyntax-only` and lists every project header the TU re-opened from disk
   although the PCH has it, with the TU spelling next to the PCH spelling. Run
   it with and without `--zccache`. If only the zccache run differs, the cache
   layer is involved; capture both outputs.

3. **Classify** using the table above. DIFFERENT spelling means trace where
   that spelling is produced (`-I` flags in `build.ninja` /
   `compile_commands.json`, `ci/meson/path_normalization.py`, meson `/` joins,
   which emit `\` on Windows, see #2685). Same spelling with a size/mtime
   mismatch means find which guard should have rebuilt the PCH and didn't, or
   what moved the header's mtime (including backwards).

4. **Recover without changing source timestamps.** Confirm the consumer
   runs through the pinned zccache. Its private input snapshots preserve the
   size and timestamp the PCH recorded while validating current source content.
   If that path fails, capture zccache diagnostics and rebuild the affected PCH.
   Never use `touch -d` on shared headers to make a PCH load: it can hide real
   edits from ordinary objects in other build directories.

5. **Only now** clean (`bash test --clean`) and confirm the failure is gone.
   Then reproduce it deliberately on a temporary fixture to prove you found the
   trigger without modifying shared source timestamps.

## Where things live (Linux/macOS host build)

- **PCH build:** the ninja `CUSTOM_COMMAND_DEP` `tests/test_pch.h.pch` runs
  `ci/compile_pch.py` with zccache wrapping `ctc-clang++`, as does the
  FastLED PCH producer. The flags include `-Werror=invalid-pch -verify-pch
  -fpch-validate-input-files-content -fpch-instantiate-templates`.
- **Consumers:** the `cpp_COMPILER` rule is prefixed with zccache by
  `inject_zccache_wrapping` (`ci/meson/meson_markers.py`) on every native host. Windows link rules
  remain bare because of daemon IPC failures during parallel DLL links (#3126).
- **Guards:** `ci/compile_pch.py` (`invalidate_stale_pch`, `.pch.input_hash`,
  `.pch.flags_hash`, `.d.cache`), run before every compile
  (#4437). A bounded fail-safe purges and retries once on Clang redefinitions
  or an explicit zccache rebuild refusal naming a PCH in this build. It prints
  `STALE PCH DETECTED` and also migrates legacy chains missing zccache import
  provenance. Repeated failures need investigation.
- **Recorded deps:** ninja keeps the PCH depfile in its own database. Read it
  with `FL_AGENT_ALLOW_ALL_CMDS=1 bash -c 'cd .build/meson-quick && ninja -t
  deps tests/test_pch.h.pch'` (read-only). `tests/test_pch.h.d.cache` is
  `compile_pch.py`'s copy.
- **Ordinary native objects:** Ninja sees source timestamps unchanged by
  zccache. Source-content signatures additionally guard early test-result,
  fingerprint and Ninja skip caches against timestamp-preserving edits.
  The native input guard invalidates objects when changed content is hidden
  by preserved timestamps, including partial builds; it never rewrites sources.

## Minimal repros: what they can and cannot prove

A three-file repro (`a.h` with `#pragma once`, a `pch.h` that includes it, and
`t.cpp`) proves compiler behaviour **only if** it uses the bare compiler binary:
- FastLED's compiler is `~/.clang-tool-chain/clang/<os>/<arch>/bin/clang++`.
  Pass `--no-default-config`, because on NixOS clang-tool-chain auto-loads a
  `clang++.cfg` whose `-Wl,` flags make `-x c++-header ... -o x.pch` fail with
  "cannot specify -o when generating multiple output files".
- Do not use `/run/current-system/sw/bin/clang++` on NixOS: it is a bash
  wrapper that injects `-fmacro-prefix-map`, `--gcc-toolchain` and similar.
- Use `-nostdinc` so no system paths are involved.

Even then, a minimal repro only shows how clang reacts to *one* trigger. It
does not show which trigger fired in the real build: that is what the `tu` probe
is for. **Do not file upstream compiler bugs from a minimal repro alone**, and
never without asking the maintainer first. FastLED's past PCH failures were
build-plumbing problems (path spelling, cache keys, snapshot restores) that
clang merely exposed.

## Pitfalls seen in practice

- Machine load (load average over 100) turns 20 s test watchdogs into fake
  failures. That is not a PCH problem; the probe's rc=0 tells them apart.
- A temporary `touch` or edit-and-revert tests snapshot handling. Verify the
  source timestamp stays as the edit left it and the zccache consumer compiles.
- `-fsyntax-only` with the clang-tool-chain config leaves `-Wl,` flags unused.
  Pass `-Wno-unused-command-line-argument`, as the PCH build does.
