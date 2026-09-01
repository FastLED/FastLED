# MP3 decoder performance: minimp3 vs the retired Helix backend

## Summary

FastLED replaced libhelix_mp3 with minimp3 in FastLED#4056, for licensing
reasons: Helix is RPSL/RCSL, minimp3 is CC0. minimp3 wins on memory and on
licence. On speed the picture splits, and the half that ships is the slow half:

| build | vs Helix | ships on |
|---|---|---|
| minimp3-float | **0.60x** (40% faster) | hosts with an FPU |
| minimp3-fixed | **1.59x** (59% slower) | every embedded target |

`minimp3-fixed` is the default everywhere -- the choice is made in
`minimp3.h`, not by a build flag -- so the configuration FastLED actually ships
to microcontrollers is materially slower than the decoder it replaced.

On an ESP32-C6 at 160 MHz that lands at **1.51x real time** for one stereo
44.1 kHz stream (`bash autoresearch esp32c6 --mp3`). Helix would have been near
2.4x. The difference is the headroom left for LED output, network and UI.

## The gate does not cover this

`codec_cpu_trend.json` is the CPU baseline store. Its `backends` (codegen)
section has both minimp3 builds. Its **`host_baselines` section has only
`minimp3-float`.**

`ci/codec_cpu/audit.py` declares `BACKENDS = ("minimp3-float",
"minimp3-fixed")` and measures both, so `minimp3-fixed` produces host cycle and
Callgrind numbers on every run -- and there is nothing recorded to compare them
against. The fixed-point decoder can regress on host CPU indefinitely without
tripping anything.

Helix, by contrast, *did* have full host baselines on all three CI hosts. They
were deleted with the backend in `6990cdc3e4`.

There is also no `codec_cpu_ledger.md`, despite
`.github/workflows/mp3_cpu_audit.yml` path-filtering on it. It has never
existed. Do not conclude "Helix was never profiled" from grepping it -- that
mistake was made once already; `grep -c` on a missing file returns 0.

## Historical Helix numbers

Recovered from `git show 6990cdc3e4^:codec_cpu_trend.json`. Callgrind, AMD EPYC
7763, clang 18, the five-file corpus, 892 frames:

| backend | instructions | branches | branch misses |
|---|---|---|---|
| helix | 461,845,281 | 8,294,798 | 453,948 |
| minimp3-float | 261,532,799 | 22,461,967 | 555,920 |

Helix's hot functions:

| instructions | function |
|---|---|
| 453,016,441 | `MP3Decode` (whole decode) |
| 301,950,654 | `Subband` |
| 192,621,420 | `PolyphaseStereo` |
| 101,035,762 | `IMDCT` |
| 96,979,680 | `MADD64` |
| 81,616,669 | `IMDCT36` |

Helix target codegen, also recovered (instructions / inner-loop):

| target | dct32 | polyphase |
|---|---|---|
| cortex-m0plus | 885 / 498 | 2015 / 1179 |
| cortex-m4 | 547 / 536 | 679 / 372 |
| riscv32-esp | 15 / 0 | 776 / 0 |
| xtensa-esp32 | 551 / 117 | 2075 / 1251 |

Do not read these against minimp3's current polyphase figures (75-195
instructions) as if they were the same measurement. Helix's polyphase is one
large unrolled routine; minimp3's `mp3d_synth_pair` is small and called many
times. Static size does not order the two -- dynamic instruction count does,
and that is the table above.

## Fresh measurement, 2026-09-01

Helix reconstructed from `6990cdc3e4^` into a scratch directory and driven by a
harness copying `ci/codec_cpu/driver.cpp` verbatim: same five-file corpus, same
`__builtin_readcyclecounter`, same FNV-1a checksum, same flags. Only the decoder
differs. All three decode 892 frames -- if that count diverges the comparison is
void. Median of 30 runs, pinned to one core.

| backend | audit flags | vs Helix | production flags | vs Helix |
|---|---|---|---|---|
| minimp3-float | 163,146,708 | 0.60x | 39,078,828 | 0.60x |
| minimp3-fixed | 660,438,396 | 2.41x | 102,928,770 | **1.59x** |
| helix | 274,149,774 | 1.00x | 64,732,068 | 1.00x |

*Audit flags* are `audit.py`'s: `-O1 -fno-inline -DMINIMP3_NO_SIMD`.
*Production flags* are `-O2` with inlining and SIMD on. `-fno-inline` inflates
the fixed-point gap from 1.59x to 2.41x; it does not create it. Measuring only
under audit flags would have overstated the problem by half, and measuring only
the float build would have missed it entirely.

**Helix was not restored to the tree and must not be.** The measurement is
reproducible from git history; the RPSL/RCSL obligations are the entire reason
the code was removed.

## Where to look for the slowdown

Synthesis is the hot kernel in both decoders. From the operation ledger,
minimp3-fixed performs 26,434,368 multiplies in synthesis against 6,014,848 in
the IMDCT -- and Helix spends 301,950,654 instructions in `Subband` plus
192,621,420 in `PolyphaseStereo`, the same stage under different names. Any
serious attempt starts there.

Per-frame host medians (`uv run python ci/codec_cpu/audit.py --host`),
minimp3-float:

| stage | ns/frame |
|---|---|
| synthesis | 20,598 |
| huffman | 7,373 |
| imdct | 3,440 |
| antialias | 962 |
| scalefactors | 599 |

The fixed build's huffman stage is notably worse than the float build's
(12,134 vs 7,373 ns/frame) despite huffman being integer work in both, which is
a concrete lead.

## Reproducing

Helix is not vendored and cannot be. To rebuild the harness:

1. `git ls-tree -r --name-only 6990cdc3e4^ -- src/third_party/libhelix_mp3`,
   extracting each file with `git show` into a scratch directory.
2. Compile the library through its own `_build.cpp.hpp`. It needs
   `fl::third_party::Mp3MemoryAllocate` / `Mp3MemoryFree` and `fl::memcpy` /
   `memmove` / `memset` shimmed onto the C runtime -- the same shim
   `ci/codec_conformance/harness.cpp` uses for minimp3. Note the header puts
   its typedefs at global scope but its functions in `fl::third_party`.
3. Drive it with `ci/codec_cpu/driver.cpp`'s corpus, cycle counter and
   checksum. Helix needs `MP3FindSyncWord` before each `MP3Decode`; minimp3
   scans for sync itself.

## Caveats

- Host x86-64, not an embedded target. 32-bit targets behave very differently
  for this decoder: FastLED#4133's first fix cost +78% instructions on armv7m,
  riscv32 and i386, and exactly 0% on x86-64.
- Helix ships hand-written ARM assembly (`real/arm/asmpoly_gcc.S`) that does not
  participate on x86. On a Cortex-M the Helix side would likely be *better* than
  measured here, widening the gap rather than narrowing it.
- PCM checksums differ between all three decoders, as expected. They are
  different implementations held to a shared 60 dB ISO floor, not to
  bit-identity with each other.
