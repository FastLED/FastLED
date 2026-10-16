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
`redefinition of 'X'` / `redefines default argument` **only if clang opened a
header from disk although the PCH already holds it**: clang failed to match the
on-disk file to the PCH copy, so the header was ingested twice.

- `#pragma once` dedupes by **file identity**. If identity matching fails, the
  second copy is parsed, and the first redefinition stops the build. This is
  the early warning; it is doing its job.
- An `#ifndef` include guard dedupes by **macro**. The macro is defined in the
  PCH, so the second copy is still read from disk but its body is skipped.
  The build passes, but the identity bug is hidden. **Never "fix" a PCH
  redefinition by adding include guards** (see #2325, where that masked a
  Windows spelling problem).

So the question is always: *why did clang fail to match this header to its PCH
copy?* In FastLED's history there are two answers.

| Mechanism | How it looks in the probe | Past cases |
|---|---|---|
| **Path spelling drift.** The PCH recorded `.../src/fl/x.h`; the TU opened `...\src\fl/x.h`, `src/fl/../fl/x.h`, a different case or a different root. | `tu` shows the header re-opened with **DIFFERENT spelling** | #2685 / zccache#619 (Windows: clang joined an `-I` root and an include with `\`); #3813 (spelling changed but the PCH cache key could not see it) |
| **Stale PCH accepted.** An input's mtime is newer than the PCH (touch, `git checkout`, formatter, revert-an-edit, cache or snapshot restore). With `-fpch-validate-input-files-content`, clang accepts the PCH because the content matches, but then fails to match the file. | `inputs` lists the header under "newer than the PCH", and `tu` shows it re-opened with the **same spelling** | #4839 / #4843 (touched header); #2995 / zccache#710 (meson snapshot restore resurrected a stale PCH); #4437 (staleness check skipped on header edits) |

Both mechanisms land in the same place: a staleness guard in `ci/compile_pch.py`
or the build plumbing that is **blind to one kind of change**. Previous misses
were content-hash only (#3813, #4839), no compiler flags in the key (#3813),
checks gated on `build.ninja` mtime (#4437), and snapshot restores (#2995). When
you find a new case, the fix is almost always "make the guard see this kind of
change", not "turn validation off".

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
   identical" signature.

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
   which emit `\` on Windows, see #2685). Same spelling plus stale means find
   which guard should have rebuilt the PCH and didn't.

4. **Only now** clean (`bash test --clean`) and confirm the failure is gone.
   Then reproduce it deliberately (for example `touch` the header and rerun the
   probe) to prove you found the trigger. Restore the mtime afterwards
   (`touch -d "<original stat time>" <file>`).

## Where things live (Linux/macOS host build)

- **PCH build:** the ninja `CUSTOM_COMMAND_DEP` `tests/test_pch.h.pch` runs
  `ci/compile_pch.py` with `ctc-clang++` directly. It is **not** wrapped by
  zccache. The flags include `-Werror=invalid-pch -verify-pch
  -fpch-validate-input-files-content -fpch-instantiate-templates`.
- **Consumers:** the `cpp_COMPILER` rule is prefixed with zccache by
  `inject_zccache_wrapping` (`ci/meson/meson_markers.py`). On **Windows** the
  wrapping is stripped (`_ZCCACHE_WINDOWS_TARGET_RULES = ()`, #3126), so
  Windows host compiles do not go through zccache.
- **Guards:** `ci/compile_pch.py` (`invalidate_stale_pch`, `inputs_newer_than`,
  `.pch.input_hash`, `.pch.flags_hash`, `.d.cache`), run before every compile
  (#4437). There is a fail-safe that purges and retries once on this error
  signature (#4843), and it prints `STALE PCH DETECTED`. If you see that line,
  a guard missed something: find it rather than relying on the retry.
- **Recorded deps:** ninja keeps the PCH depfile in its own database. Read it
  with `FL_AGENT_ALLOW_ALL_CMDS=1 bash -c 'cd .build/meson-quick && ninja -t
  deps tests/test_pch.h.pch'` (read-only). `tests/test_pch.h.d.cache` is
  `compile_pch.py`'s copy.

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
- A temporary `touch` or edit-and-revert of a PCH header is itself the stale
  trigger. Restore the mtime, or expect one PCH rebuild.
- `-fsyntax-only` with the clang-tool-chain config leaves `-Wl,` flags unused.
  Pass `-Wno-unused-command-line-argument`, as the PCH build does.
