# MP3 decoder performance: minimp3 vs the retired Helix backend

## Measuring a change: one command

```
bash mp3measure
```

Host Callgrind instruction counts against the last commit, an ESP32-C6
autoresearch run with the Helix ratio, the riscv32 `.text` delta, and PSNR.
`--gates` adds conformance, sanitizers and lint; `--skip-device` when no board
is attached. Step output goes to log files, so the report stays readable.

**Optimisation work happens in one file:
`src/third_party/minimp3/minimp3_synth_fixed.h`** -- the fixed-point synthesis
back-end (DCT-32, polyphase, integer SIMD). It is 57% of a decode and holds the
one stage where this decoder still loses to Helix on RISC-V. It is textually
included from `minimp3.h` at a single point inside
`#if MINIMP3_HAVE_FIXED_POINT` and does not compile standalone.

PSNR is checked on every run, not behind `--gates`: fixed-point work can "win"
by breaking the arithmetic, and an opt-in tripwire is one a loop will skip. The
device checksum (`0xc6b632ab`) covers the same risk on hardware.

A serial timeout is retried automatically. The board re-enumerates on reset, so
a back-to-back run can open the port the previous run left behind and hear
nothing; that is the link failing, not the decoder.

## Where it stands today

ESP32-C6 at 160 MHz, both decoders in one firmware, timed in the same run
(`bash mp3measure`):

| | minimp3-fixed | Helix | ratio |
|---|---|---|---|
| Layer III decode | **39,839 us** | 35,284 us | **1.130x** |

Down from 4.0x when this work started. Accuracy is unchanged throughout:
PSNR 123.24 dB on `l3-hecommon`, device checksum `0xc6b632ab`.

Two cautions on that ratio. Helix's leg is untouched code and still moves
0.6-0.7% flash to flash, so it is a usable drift control but not a fixed
reference -- against the leg measured at the start of this round (34,812 us)
the same firmware reads 1.144x. And the *host* now reads 1.024x against Helix
where it used to read 1.000x: several of the changes that pay on a 32-bit
target cost instructions on x86-64. That is the expected direction, not a
regression to chase.

## Summary

FastLED replaced libhelix_mp3 with minimp3 in FastLED#4056, for licensing
reasons: Helix is RPSL/RCSL, minimp3 is CC0. minimp3 wins on memory and on
licence. On speed the picture splits, and the half that ships is the slow half:

**Use Callgrind instruction counts, not cycles.** They are deterministic, so
they do not move with machine load -- and this file has already been wrong once
because a cycle measurement was taken on a loaded box. As a cross-check, the
Helix count measured in 2026-09 came out 0.18% from the baseline recorded a year
earlier on different hardware.

**These figures are historical.** They were taken at `-O2` when
`callgrind.py` compiled at that level, against the decoder as it stood before
any of the optimisation work below. The script now compiles at `-Os`, which is
what ships, so a fresh run will not reproduce these numbers and is not meant
to -- see "Where it stands today" for the current ones. They are kept because
they are the measurement the whole effort started from.

Scalar (`-DMINIMP3_NO_SIMD`), `-O2` with inlining, five-file corpus, 892
frames, decoder as of 2026-08:

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
what ships. At the time these were taken, the figure to quote was the `-O2`
scalar one. **That guidance has since changed:** `callgrind.py` compiles at
`-Os` now, because `-Os` is what FastLED ships and measuring the level that
ships is worth more than measuring the level that flatters. Quote the `-Os`
scalar figure, and quote the *device* number for any speed claim.

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

Callgrind attribution, scalar `-O2` (historical; the script is `-Os` now),
the numbers that should drive any
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

## Making the 32-bit cost visible: `text_size.py --compare`

Host Callgrind reads **1.024x** against Helix on the 892-frame corpus while an
ESP32-C6 reads **1.130x**. That difference is not noise and it is not the
corpus -- it is work that only exists on a 32-bit target: 32x32->64 multiplies
that need a `mul`/`mulh` pair, and values that live in registers on x86-64 and
in the frame on riscv32. Callgrind cannot see any of it.

    uv run python ci/codec_cpu/text_size.py --compare

builds *both* decoders for *both* ISAs from the same sources and prints, per
stage, how much each one inflates going from x86-64 to riscv32. Two details
make the numbers mean anything, and both were wrong in the first version:

- **Same compiler family on both sides** (`esp-elf-g++` against host `g++`).
  Against host *clang* the tool reported minimp3's DCT-32 at 798 riscv32
  instructions to 217 on x86-64 and called it a 3.7x ISA penalty. It is 1.59x.
  g++ fully unrolls the DCT-32's inner loops -- 798 riscv32 instructions
  contain two backward branches -- and clang at `-Os` does not. Most of that
  ratio was one compiler against another.
- **Inline-aware attribution.** Both decoders inline heavily at `-Os`;
  a symbol table compares two different partitions of the same program. The
  tool disassembles, asks `addr2line -i` for each address's inline stack, and
  charges the bytes to the innermost frame that names a stage. minimp3's
  `fl::math` helpers are transparent, because Helix spells the same operations
  as macros and counting minimp3's separately would score a language choice as
  an algorithmic difference.

What it says today (riscv32 instructions, `-Os`, scalar):

| stage | mp3 rv32 | infl | hx rv32 | infl | mp3/hx |
|---|---|---|---|---|---|
| polyphase | 1,278 | 1.85x | 1,820 | 2.09x | 0.70x |
| dct32 | 742 | 1.03x | 710 | 1.05x | **1.05x** |
| imdct | 692 | 1.36x | 1,158 | 1.02x | 0.60x |
| bitstream | 1,649 | 0.88x | 1,784 | 1.03x | 0.92x |
| dequant | 453 | 1.42x | 599 | 1.06x | 0.76x |
| whole TU | 5,579 | **1.17x** | 7,197 | **1.18x** | 0.78x |

minimp3 no longer inflates faster than Helix from host to target. It used to:
1.24x against 1.18x, with `dct32` alone at 1.50x -- and closing that stage is
what moved the whole-TU figure to 1.17x.

## The DCT-32 gap was the multiply lowering, not the spill

`dct32` was the one stage where minimp3 lost to Helix on riscv32 -- 876
instructions to 710, a ratio of 1.23x where every other stage sat at 0.60-0.92x.
Two numbers looked like the cause and only one of them was:

| | minimp3 `dct32`, before | Helix `dct32` |
|---|---|---|
| memory ops | 257 | 275 |
| ...of which sp/s0 (frame traffic) | **174** | **47** |
| multiplies (`mul`+`mulh`) | **89** | **44** |

**The frame traffic was a red herring, and worth spelling out because the
metric will mislead the next reader the same way.** Three things were going on:

- About 87 of `mp3d_DCT_II`'s 147 frame accesses are its *prologue*, where gcc
  constant-folds `g_sec_q27` into `lui`/`addi` immediates and spills them to
  stack slots outside the k loop. That runs once per call against 18 iterations
  of ~1,140 instructions -- 0.4% of the function, dynamically.
- The rest is `t[4][8]`, the transpose scratch between pass 1 (which writes
  columns) and pass 2 (which reads rows). Those 32 values have to be
  materialised somewhere. Helix pays exactly the same traffic to its `buf[32]`
  -- but `buf` is a *pointer argument*, so its loads and stores are addressed
  off `a0` and the sp/s0 counter does not attribute them as frame traffic at
  all. Total memory ops, which is the honest comparison, were 257 to 275: this
  decoder was already doing slightly *less*.
- Both decoders execute exactly **80 multiplies per 32-point DCT**, and their
  non-multiply code came to 427 instructions against 439 -- within 3%.

So the entire gap was in how one multiply lowers. Helix spends two instructions
on each -- `mulh`, then a shift to undo the coefficient's Q format -- and this
decoder spent nine:

    mul  mulh  lui  add  sltu  add  srli  slli  or

Seven of those exist only to round at bit `Bits` rather than at bit 32, which
needs the low half of the product, the carry out of adding 2^(Bits-1) to it,
and a 64-bit funnel shift. Helix does not round anywhere; it truncates, and
that is where its 20 dB of accuracy went.

`mp3d_mulshift_k` rounds at bit 32 -- which `mulh` does for free -- and lands
on the same bits. Writing a compile-time coefficient as `Coef = W*2^Bits + F`:

    floor((v*Coef + 2^(Bits-1)) / 2^Bits)
      = v*W + floor((v*(F << (32-Bits)) + 2^31) / 2^32)
      = v*W + mulh(v, F32) + (lo(v*F32) >> 31)

`F32` is kept as a *signed* int32 -- most of these coefficients have a
fractional part above a half, so the Q32 form has its top bit set; reading that
bit pattern as negative subtracts 2^32, and the missing 1 goes back on the
integer side as `W+1`. Spelling it unsigned instead makes gcc emit
`srai/mul/mul/mulhu/add` and hands the whole win back.

Per coefficient on riscv32 `-Os`, excluding the `lui`/`addi` that materialises
it (both forms pay that): 8 instructions become 4 where W is 0, 5 where W is 1,
7 where W is 3 or 10. `mp3d_DCT_II`'s dynamic count per 32-point transform went
**1,140 -> 894 (-21.6%)**, and the stage to 1.05x of Helix.

The requirement is that the coefficient be a constant *expression*, not merely
a constant the compiler knows the value of -- only then can W and F32 be
computed at compile time. That is why pass 1 is spelled out as eight macro
invocations with `i` as a template argument and `g_sec_q27` is `constexpr`.

Bit-exactness here is proved rather than argued: the identity was checked
against `mp3d_mulshift` for all 2^32 int32 inputs against each of the 33
coefficients the DCT instantiates -- 141,733,920,768 pairs, zero mismatches.
The device checksum and the PSNR are unchanged, and they are the tripwire if a
constant is ever transcribed wrong.

**What is left, and it is not much.** The multiply column still reads 94 to
Helix's 44, because the rounding bit needs the low half and so every multiply
is still a `mul`/`mulh` pair where Helix uses one `mulh`. Removing that means
truncating, which is the 20 dB. The remaining candidate is the *general*
`fl::math::mul_shift_round32`, still 9 instructions at its 20 non-DCT call
sites (IMDCT, antialias, stereo); the same low-half-rounding identity takes it
to 7 without needing a compile-time coefficient:

    (hi << (32-S)) + (((lo >> (S-1)) + 1) >> 1)

That lives in `src/platforms/int_asm.h` behind a pinned codegen test, so it is
a separate piece of work from anything in the synthesis file.

`mp3d_synth_granule` -- which is where the DCT-32 lands -- is 17.0% of host Ir,
so the host does see the stage; what it could not see is that the stage cost
disproportionately more on the target.

Read all of this as a **bound, not a measurement**. Static instruction counts
say where the target does structurally more work; they do not say what that
work costs. A change removing 15 of 25 riscv32 instructions once delivered 4.6%
on device because the branches it removed were predictable. `device_profile.py`
decides.

The Helix half builds through `ci/codec_cpu/helix_codegen.cpp`, which is
benchmark-only: RPSL/RCSL-licensed, not part of the library, not linked by any
firmware.

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
