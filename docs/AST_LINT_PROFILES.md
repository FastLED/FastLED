# AST lint with real ESP SDKs

The host stub profile cannot see code removed by ESP preprocessor guards.
The ESP profiles supply real SDK headers, target settings, and optional SD
dependencies to Clang. Declaration counts describe semantic analysis coverage;
they do not measure runtime test coverage.

## Setup

From the repository root, install the declared Python dependencies and provision
the SDK environments explicitly:

```sh
uv sync
uv run python -m ci.tools.ast_profiles --provision
uv run zccache start
```

Provisioning calls the installed, pinned fbuild's `install --json` and
`build --release -t compiledb` commands. It downloads missing SDKs and libraries
and generates real compiler commands without compiling or linking firmware.
Provisioning also installs the bundled target clang-query and records the
existing host query's actual implicit include paths. Ordinary lint loads these
environments and never provisions dependencies.

The native result cache requires the matching zccache daemon to be running.
`zccache start` is idempotent; repeat it after restarting the machine before
running lint. A missing daemon is an error, not an uncached fallback.

CI restores SDKs and tools before installation, and restores the per-revision
AST result cache before a daemon can start. SDK provisioning uses fbuild's
`FBUILD_CACHE_DIR` override to keep only these profile dependencies in
`.cache/ast-sdk`. Native builds retain their normal package cache. The tool cache
contains the complete `clang` and `clang_extra` installations, excluding download
archives and unrelated runtimes. The result cache stores dependency manifests
and zccache's self-contained `kv/exec-probe-v1` files; it excludes daemon sockets,
PID files, locks, and logs. Stable SDK cache keys are separate from per-revision
result keys so source edits do not duplicate the SDK archive.

To provision or inspect one SDK environment:

```sh
uv run python -m ci.tools.ast_profiles --provision --profile esp32-idf4
uv run python -m ci.tools.ast_profiles --profile esp32-idf4
```

Run the normal C++ lint entry point after provisioning:

```sh
bash lint --cpp
```

The dedicated matrix entry point is useful when investigating semantic coverage:

```sh
uv run python -m ci.tools.ast_matrix
```

The required profiles are declared in `ci/tools/ast_profiles.py`:

| Profile | Board | Pinned platform | Target |
| --- | --- | --- | --- |
| `esp32-idf4` | `esp32dev` | `espressif32@4.4.0`, Arduino 2.0.3 / IDF 4 | `xtensa-esp32-elf` |
| `esp32-idf5` | `esp32dev` | pioarduino `55.03.311`, Arduino 3.3.11 / IDF 5 | `xtensa-esp32-elf` |
| `esp32c6-idf5` | `esp32-c6-devkitc-1` | pioarduino `55.03.35`, Arduino 3.3.5 / IDF 5 | `riscv32-esp-elf` |

The C6 profile exercises RISC-V platform selection alongside the classic
Xtensa ESP32. These three SDK environments support separate Arduino SD and
SdFat analysis configurations. SdFat is pinned to registry package
`greiman/SdFat@2.3.1`. SD, FS, and SPI come from the selected Arduino SDK;
they must not be declared as independent registry packages.

## Source and configuration provenance

Each environment lives in `.build/ast-profiles/<profile>/`:

- `platformio.ini` declares the pinned SDK and library requirements.
- `src/main.cpp` selects the actual SDK libraries during dependency discovery.
- `packages.json` records fbuild's resolved package identities and available
  archive checksums.
- `.fbuild/build/release/compile_commands.raw.json` contains the real target
  compiler commands, before fbuild translates them for clangd.
- `ast-environment.json` records the target, adapted arguments, include paths,
  compiler identity, removed flags, SDK/configuration identity files, and
  `derivation_inputs` content hashes.
- `compiler-predefines.txt` records the real target compiler's predefined
  macros, including its integer typedef spellings.

The shared `.build/ast-profiles/target-clang-query.json` records the resolved
bundled binary, package version, and LLVM version. `host-environment.json`
records the existing host query command and its implicit include roots.
Normal lint does not start a compiler to rediscover these prerequisites.
The loader verifies SHA256 hashes of the inputs used to derive each environment:
compiler/query binaries, raw compilation commands, response files, predefined
type macros, package receipts, configuration, and the profile adapter source.
Changing these inputs requires explicit reprovisioning. Reusing old arguments
after invalidating only the analysis cache would still analyze stale settings.

The staging project contains no copied FastLED sources. Analysis prepends the
current checkout's `src` include path and parses its owned translation units.
Changes in the checkout therefore cannot be hidden by an old staged library.

The adapter removes only known GCC code-generation and build-output switches.
It retains target ABI and architecture arguments, SDK definitions, include
order, and the SDK's C++ language standard. It expands response files and
normalizes paths relative to each original compiler command.

GCC and Clang can choose different C++ types for the same fixed-width alias.
For example, the ESP C6 GCC toolchain defines `int32_t` using `long int`, while
Clang's default is `int`; both occupy 32 bits, but they select different
overloads. The adapter supplies GCC's emitted integer type macros, including
the least/fast aliases, to preserve the SDK's type choices. It does not replace
Clang's compiler-version or architecture feature macros.

Builtin types are verified separately. The selected Xtensa GCC compilers use
unsigned 16-bit `wchar_t`, so the adapter supplies `-fshort-wchar`; changing
`__WCHAR_TYPE__` alone would leave Clang's builtin type incorrect. Provisioning
compiles assertions for builtin sizes, signedness, and key typedef identities
before saving an environment. An unsupported ABI or failed assertion stops
provisioning.

The real compiler reports its implicit C++ include directories using empty
standard input. Clang receives these paths together with `-nostdinc` and
`-nostdinc++`, so host standard-library discovery does not silently replace
the target headers. Target provisioning and target queries also remove host
include injection variables from their process environment. Host queries retain
their configured environment. Unknown flags remain visible to Clang and can
fail the analysis instead of being silently discarded.

`dependency_roots` records SDK header-tree ancestors needed for quoted relative
includes into sibling components. These roots come from fbuild package receipts.
They expand dependency validation without adding compiler search directories or
changing include precedence.

## Reports, caching, and measurement

The matrix writes `.build/ast-coverage.json` by default. Choose another artifact
with `--report`; `--workers` bounds concurrent jobs. `--memory-budget-mb` and
`--worker-memory-mb` estimate a scheduling limit, not an enforced memory ceiling.

```sh
uv run python -m ci.tools.ast_matrix --workers 2 --report .build/ast-warm.json
```

The report lists exact jobs, profile versions and arguments, expected backend
definitions, findings with profile attribution, declaration locations, missing
expectations, parse failures, and known uncovered configurations. It records
analysis time, parse count, cache hits and hit rate, and coverage per profile and
union. `elapsed_seconds` excludes report aggregation and writing; measure the
complete CLI externally when comparing end-to-end wall time.
`snapshot_verified` records the final input audit after all jobs finish; edits
during analysis invalidate the run even when individual jobs were cache hits.

Completed per-unit results use the native `zccache.exec_cached` binding. Local
manifests retain discovered dependencies; analysis results and eviction remain
zccache's responsibility. Keys validate source/header bytes, tool and compiler
identity, full arguments, generated configuration, and include availability.
Previously absent headers and changed symlink targets invalidate affected
profiles. Cached violations remain violations. Failed or incomplete parses are
not stored as successful results. An unchanged warm invocation should report
zero parses and performs no downloads or firmware builds.

`memory_measurement` records aggregate sampled RSS for the current process and
its descendants, using a 100 millisecond polling interval with a final sample at shutdown.
The peak includes overlapping workers and the parent Python process. It excludes
independent already-running daemons; shared resident pages can be counted once
per process, and processes shorter than a sampling interval can be missed.
Unavailable process observations are counted. Treat this as sampled process-tree
RSS, not unique physical memory or a hard resource limit.

For performance comparisons, keep the revision, machine, worker count, and
provisioned SDKs fixed. Retain separate reports for cold analysis, an unchanged
warm run, one translation-unit edit, and a shared-header edit. Use isolated cache
state for the cold run through zccache's supported cache configuration. Restore
edits before drawing final coverage conclusions. Record the edit paths and cache
state alongside the JSON artifacts; a prior exploratory run does not establish
the final performance budget.

## Troubleshooting

**Missing profile or SDK:** rerun the explicit provisioning command. Missing
required headers, compiler binaries, raw compilation databases, or environment
manifests are failures. Do not substitute a stub profile for a missing ESP SDK.

**Changed platform pin or checkout path:** provision again. Absolute compiler
and SDK paths belong to the machine that generated the manifest. Reusing an
SDK package cache is supported; copying generated databases between machines
without regenerating them is not.

**Changed include environment:** reprovision the host environment after changing
its include injection variables, such as `CPATH`, `CPLUS_INCLUDE_PATH`,
`C_INCLUDE_PATH`, `SDKROOT`, or `INCLUDE`. Target compiler discovery and queries
strip these variables, including Nix and Objective-C include injection settings,
so a host SDK cannot silently enter target analysis.

**Changed derivation input:** reprovision after changes to the compiler/query
binary, raw database, response file, predefined type macros, platform
configuration, package receipt, or profile adapter. Normal source/header edits
only invalidate the affected semantic results and do not require provisioning.

**Wrong SDK major:** check the resolved `esp_idf_version.h` and `packages.json`.
The loader validates the selected IDF major rather than trusting the profile
name alone.

**Missing SD or SdFat backend:** inspect the actual include paths for `SD.h`,
`FS.h`, `SPI.h`, and `SdFat.h`. Presence assertions require the selected backend
definition to exist in the AST. A successful parse with zero backend matches
does not prove coverage. Header case matters on Linux; fake compatibility
headers would conceal real selection bugs.

**Clang reports an unsupported target or option:** use the project-supported
bundled Clang toolchain. The package's LLVM 21.1.5 clang-query successfully
parsed both Xtensa IDF 4 and RISC-V C6 SDK translation units during validation.
A system clang-query can lack target support; a system wrapper can also inject
host include paths despite explicit target arguments. Investigate the raw command and the
adapter's recorded removals before changing flags; do not remove ABI switches
or SDK macros merely to obtain a successful parse.

**A previously untested annotation fails:** fix the owned declaration and its
matching declarations/definitions. Required semantic coverage must not use a
generated baseline to grandfather missing exception specifications.

## Coverage limits and adding profiles

The validated matrix contains 138 jobs: 33 stub jobs and 35 for each SDK
(34 owned translation units plus the separate SdFat configuration). It observed
21,894 distinct declaration expansion locations across 1,236 owned source files.
ESP adds 2,592 locations beyond the stub's 19,302. The C6 profile adds 192
locations beyond the combined stub and Xtensa profiles, which justifies keeping
the RISC-V configuration. These counts identify parsed declarations, not executed
code or a percentage of all conditional source branches.

A compiler AST describes one preprocessed configuration. Seeing a file in an
include trace does not prove that every branch in that file was parsed. The
same header can expose different declarations under SD, SdFat, chip, driver,
and SDK-version guards. Compare normalized declaration locations and expected
backend definitions, keeping parse failures and uncovered configurations
distinct from successful semantic coverage.

To add a platform, declare a pinned `EspProfile`, provision it using fbuild,
verify its real headers and target settings, and add the owned source units
and expected implementation definitions to the analysis matrix. Demonstrate
the additional declarations it exposes and measure its cost before making it
required. Do not combine mutually exclusive chip macros in one translation
unit. A raw-source parser may help discover gaps, but it does not replace
compiler validation of types, inheritance, and exception contracts.
