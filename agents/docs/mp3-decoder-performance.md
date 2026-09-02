# MP3 decoder performance: minimp3 vs the retired Helix backend

## Summary

FastLED replaced libhelix_mp3 with minimp3 in FastLED#4056, for licensing
reasons: Helix is RPSL/RCSL, minimp3 is CC0. minimp3 wins on memory and on
licence. On speed the picture splits, and the half that ships is the slow half:

**Use Callgrind instruction counts, not cycles.** They are deterministic, so
they do not move with machine load -- and this file has already been wrong once
because a cycle measurement was taken on a loaded box. As a cross-check, the
Helix count measured in 2026-09 came out 0.18% from the baseline recorded a year
earlier on different hardware.

Scalar (`-DMINIMP3_NO_SIMD`), `-O2` with inlining, five-file corpus, 892 frames:

Ratios below are `backend / helix` **instructions executed**, so higher is
worse. 1.59x means minimp3-fixed does 59% *more* work than Helix, not that it is
faster.

| build | instructions | vs Helix | | ships on |
|---|---|---|---|---|
| minimp3-float | 202,832,660 | 0.91x | **9% fewer instructions** | hosts with an FPU |
| minimp3-fixed | 353,592,466 | 1.59x | **59% MORE instructions** | every embedded target |
| helix | 222,939,814 | 1.00x | (reference) | -- |

The configuration matters more than it looks. At `-O1 -fno-inline` (the audit's
flags) the same comparison gives 1.42x by instructions and 1.18x by cycles.
Helix gains more from inlining than minimp3 does -- 2.07x against 1.86x -- so
the gap *widens* with optimisation, and the `-fno-inline` number understates
what ships. Quote the `-O2` scalar figure.

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

## Withdrawn: the first cycle measurement

The first pass at this used wall-clock cycles and produced 2.41x / 1.59x. Both
numbers are withdrawn. The run was taken at load average 12, with SIMD enabled
on the minimp3 side and none on Helix's (its acceleration is ARM assembly),
which is three independent errors in one measurement.

It is recorded here because the failure mode is instructive: every absolute
figure in that run was inflated about 3x, *including for binaries that had not
been rebuilt*, and that was visible in the data at the time. A three-fold shift
in an untouched binary is a machine-state problem, not a code problem.

Use Callgrind. It cannot fail this way.

## Accuracy, for context

The speed gap is not free -- minimp3-fixed buys real accuracy with it:

| decoder | PSNR on `l3-hecommon.bit` |
|---|---|
| minimp3-fixed (ships) | **123.24 dB** |
| helix | 102.87 dB |
| ISO limited-accuracy floor | 60 dB |

minimp3-fixed is **20.37 dB better than Helix** and holds 63 dB of margin over
the ISO floor. A 16-bit MAC variant was measured at 44.37 dB -- 15 dB *below*
the floor -- so that particular trade is closed (FastLED#4108).

Any speed work has to keep 123.24 dB, or state plainly what it is spending and
why.

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

## Where the gap is: saturation, not DSP

Callgrind attribution, scalar `-O2`, the numbers that should drive any
optimisation work:

| minimp3-fixed | | helix | |
|---|---|---|---|
| `mp3d_synth_granule` | **201,839,372 (57.1%)** | PolyphaseStereo (asm + hpp) | 66,139,380 (29.7%) |
| ...of which one line | **86,547,296 (24.5%)** | `ClipToShort` + `clip_2n_helper` | ~26M |
| `mp3dec_decode_frame_r` | 57,394,378 (16.2%) | IMDCT | 30,288,816 (13.6%) |
| `L3_imdct36` | 43,809,800 (12.4%) | FDCT32 | 26,169,120 (11.7%) |
| **total** | **353,592,466** | **total** | **222,939,814** |

The whole gap is 130.7M instructions. Synthesis alone differs by 135.7M, so
**synthesis is the gap** -- everything else roughly cancels.

And inside it, one line of C:

    if (value > MP3D_SAT_MAX) return MP3D_SAT_MAX;   // 86,547,296 -- 24.5%

`mp3d_sat64` saturates every intermediate in the polyphase. Helix does not: it
truncates via `MULSHIFT32` in the inner loop and clips once, at output, in
`ClipToShort`. That is a design difference, not a micro-optimisation, and it is
where roughly two thirds of the excess lives.

Note this is *not* the clamp FastLED#4127 fixed. #4127 corrected the clamp's
*bounds* (it was pinning to +/-1.0 instead of the int32 range) and was worth 81
dB of accuracy. The cost here is the clamp being evaluated per intermediate at
all. Any change must keep #4127's bounds and its ISO conformance -- the 83-vector
suite exists precisely to catch a regression there.

## On-device: the host understates the gap by 2.5x

Measured on an ESP32-C6 at 160 MHz, both decoders compiled into one firmware and
timed in the same run with no reflash between them
(`bash autoresearch esp32c6 --mp3`):

```
Layer III only -- minimp3 142647 us, helix 35742 us
  ->  minimp3-fixed is 3.99x helix (299% slower)  [both decoded 8 frames]
```

| | host x86-64 | **ESP32-C6 (riscv32)** |
|---|---|---|
| minimp3-fixed vs Helix | 1.59x | **3.99x** |

**This is why the host number could not be trusted.** Nothing about the host
profile hinted at 4x. The likely cause is the same one FastLED#4133 ran into:
minimp3-fixed's arithmetic is full of int64 -- `mp3d_sat64`, the polyphase
accumulators, `mp3d_mulshift` -- and an int64 operation is a register pair plus
a carry chain on a 32-bit core while being free on x86-64. Helix was written for
32-bit MCUs and uses `MULSHIFT32`, a 32x32 taking the high half, throughout.

### Two measurement traps hit while getting this number

Both produced confident, wrong answers first. Recording them because the shape
recurs:

**Helix does not decode Layer I.** The first on-device run reported 4.32x, timing
minimp3 over 11 frames against Helix over 8 -- Helix silently skipped the Layer I
fixture, since it is an MP3 (Layer III) decoder. The comparison now runs Layer
III only, on both sides, and the runner *suppresses the ratio entirely* if the
two did not decode the same frame and sample counts. A ratio against a decoder
that bailed out early is worse than no ratio, because it looks like a result.

**Timing without checking the work.** The first version called
`helix.decode(...)` with a no-op callback and timed it. That measures nothing if
the decoder returns early, and it did. Always report what a timing actually
decoded alongside the timing.

Residual bias, quantified rather than waved away: minimp3's leg computes the
fixture FNV-1a while Helix's callback is empty. That is roughly 74k operations
against a 142 ms measurement -- about 0.3%, and it biases *against* minimp3, so
the 3.99x is if anything slightly generous to it.

## The saturation defends exactly one malformed stream

Follow-up measurement, because the Callgrind finding above was taken on x86-64
and the obvious fix -- int64 accumulation -- is the thing FastLED#4133 proved
costs +78% on 32-bit. Two questions had to be settled before touching anything:
does the cost exist on the target, and does the saturation ever do anything?

**Does it cost on RISC-V?** Yes. `mp3d_DCT_II` compiled for riscv32-esp at
`-Os`, scalar, with the butterfly saturation stripped as an experiment:

| riscv32 `mp3d_DCT_II` | instructions |
|---|---|
| with saturation (ships) | **373** |
| without | **252** |
| | **-121, -32.4%** |

So it is not an x86 artefact. A third of that function is clamping.

**Does the clamp ever fire?** Almost never, and never on anything valid.
Instrumented `mp3d_sat64` counting actual clamps, not calls:

| corpus | calls | clamps | rate |
|---|---|---|---|
| 83 ISO conformance vectors | 154,115,265 | 4,609 | 0.003% |
| repo corpus (real music) | 82,537,927 | **0** | 0% |

And every one of those 4,609 comes from a single file:

    l3-nonstandard-big-iscf.bit    4609 clamps
    (all other 82 vectors)            0

That is the malformed intensity-stereo vector -- the same stream that found the
polyphase UB in FastLED#4133. Its scalefactors drive the DSP far outside
anything a conformant encoder produces.

### What this reframes

The trade is not accuracy versus speed. On conformant input, saturating and not
saturating produce **identical output** -- the branch is never taken. The trade
is *malformed-input robustness* versus about a third of the DCT-32 on every
32-bit target.

That robustness is not worthless, and the reason is in minimp3.h's own comment:
without the clamp a wrap produces "a sign-flipped full-scale sample -- an
audible bang instead of a bounded clip". For a decoder fed untrusted bytes that
is a real property, not a theoretical one.

So the options are better defined than "HQ vs Fast":

1. **Bound the input, not every intermediate.** The vector is `big-iscf`: large
   intensity-stereo scalefactors. If the DCT's *input* is bounded at
   dequantisation, the butterfly network cannot overflow and needs no internal
   clamping at all. Most promising, and it fixes the cause rather than the
   symptom.
2. **Clamp once per stage, not per operation.** Growth is at most 2x per
   butterfly stage, so one clamp per stage bounds the network with roughly an
   eighth of the current clamp count.
3. **Cheaper clamp.** Already tried and reverted: a branchless 32-bit form cost
   6,265 bytes of text (+24%) for ~3% of decode time. Do not repeat without
   cross-compiled numbers.
4. **Accept it.** 32% of one function, defending against untrusted input, is a
   defensible price -- but it should be a decision, not an accident.

Whatever is chosen, `l3-nonstandard-big-iscf` is the regression test, and
FastLED#4127's clamp *bounds* must not move.

## Host Callgrind finds candidates; it cannot rank them

Use host Callgrind to locate hot code — it is deterministic and takes 9 seconds
against the device's 59. Do **not** use it to choose between two candidate
changes. On this decoder it has now been wrong about the ranking in both
direction and magnitude:

| change | host | ESP32-C6 |
|---|---|---|
| `FL_NO_INLINE` on `mp3d_synth` | **-4.8%** | ~0% (inside measurement drift) |
| polyphase lane-pair restructure | **+3.2%** (a regression) | **-9.7%** |

Acting on the host ranking would have shipped the keyword and discarded the
restructure. Both wrong.

Earlier rounds transferred cleanly (-20/-6/-11% host became -28/-4.6/-4.2%
device), which is what made the heuristic look safe. It is not: what transfers
is *direction on changes that remove work*, and what does not transfer is
anything whose cost is register pressure or spill traffic. x86-64 has registers
to spare and a 32-bit RISC-V core does not, so a change that only helps by
keeping values out of memory is invisible on the host, and a change that helps
the host's scheduler can be neutral on the device.

The clearest instance: the polyphase kept four lanes' accumulators in
`int64_t a[4], b[4]`. At `-Os` that array is a stack slot -- a variable index
must be memory -- so every tap-lane paid eight memory operations of pure spill.
Rewriting two lanes at a time as *named scalars* took the function from 313
load/stores to 188 against an unchanged 195 multiplies. The host, where those
values live in registers either way, called it a regression.

Practical rule: iterate on host, confirm on device, and let the device settle
any comparison closer than about 5%. `ci/codec_cpu/report.py --baseline HEAD
--runs 2` does both in one command; device spread is 0.00% across runs, so small
deltas there are real.

Related: `-O3` on the polyphase is worse than hand-unrolling (gcc emits 395
memory operations against a hand-written 188) but *does* pay on the DCT-32 and
IMDCT -- which host Callgrind at `-O2` structurally cannot observe at all.

## Disable SIMD when tracing on the host

**Host profiling must pass `-DMINIMP3_NO_SIMD`.** `ci/codec_cpu/audit.py`
already does, in `_common_compile_flags()`. That is deliberate, not incidental
-- do not "fix" it.

The targets this decoder is slow on have no SIMD. ESP32-C6, C3 and H2 are
scalar RISC-V; minimp3's vector kernels exist only for SSE4.1 and NEON. A host
run with SIMD enabled measures a path those parts never execute, and it flatters
minimp3 specifically, because the vectorised polyphase and IMDCT are exactly the
kernels that dominate the profile.

It also breaks a Helix comparison in both directions at once. Helix's
hand-written acceleration is ARM assembly (`real/arm/asmpoly_gcc.S`) and does
not participate on x86 at all, so enabling minimp3's x86 SIMD pits an
accelerated minimp3 against a scalar Helix on a host that resembles neither
target. That mistake was made here once: the 1.59x figure this file used to
carry came from an `-O2`, SIMD-on run, and it predicts nothing about a C6.

- **Scalar, `-DMINIMP3_NO_SIMD`** -- for anything meant to say something about
  embedded performance. This is what the audit gates on.
- **SIMD enabled** -- only for "how fast is the host build", a different
  question and not what #4139 is about.

If a future host has no SIMD path to disable the flag is a no-op; the invariant
is that the audit measures the scalar kernels.

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
