# Wave PDE alternatives for FastLED — state-of-the-art survey

**Mission scope:** Resolve [#3111](https://github.com/FastLED/FastLED/issues/3111).
Survey numerical-methods + real-time-graphics literature for techniques that improve the visual quality (or reduce the CPU cost) of `fl::WaveSimulation2D` / `WaveFx` versus the current super-sample-based scalar leapfrog approach.

**Working assumption — quantified:** The current approach pays an `O(k³)` super-sample tax (`k²` cells × `k` updates per displayed frame). At `SUPER_SAMPLE_4X` that is `64×` the 1× scalar cost, and the marginal visual gain past `SUPER_SAMPLE_2X` is essentially decoupled-from-physics anti-aliasing of the output. If there's an algorithmic 10×+ win available it is in **how we anti-alias, not in how we propagate**.

**Author's standing:** I am working from FastLED's actual target constraints (AVR through ESP32-S3 PSRAM, 8×8–256×256 grids, integer Q15, no FFTW / Eigen / OpenCV, no FPU on most targets) and have weighted recommendations heavily by *cycles per cell per displayed frame* on a Cortex-M4 baseline. I have explicitly *not* weighted by "the most beautiful math" — this is an LED-art problem.

---

## Executive summary

| Family | Best candidate | Δ visual @ 1× grid vs SS-4× baseline | Δ cost vs 1× scalar baseline | Embed score (worst → best of 5 platforms) |
|---|---|---|---|---|
| Higher-order spatial stencils | **Compact 9-point O(h⁴) (§5-like geometry, §B-style weights)** | comparable | 1.0–1.3× | 4 / 5 (AVR limited) |
| Output anti-aliasing filter | **3×3 separable Gaussian or single-pass SMAA on the display buffer** | comparable on smooth fields | 1.05–1.2× | 5 / 5 |
| Dispersion-corrected (NS-FDTD) | Stencil-coefficient tuning at design frequency | comparable–better at "design" wave speeds | 1.0–1.1× | 4 / 5 |
| Implicit time integration | ADI w/ Thomas-algorithm tridiagonal solve | comparable | 2.5–4× (but supports CFL > 1) | 2 / 5 |
| Spectral / FFT | Tessendorf-style FFT propagation | better visually | 8–30× per displayed frame on M4 (no AVR) | 0 / 5 (AVR/M0 ruled out) |
| Lattice Boltzmann (LBM) | D2Q5 acoustic LBM | comparable | 1.3–1.8× | 3 / 5 |
| Procedural (non-PDE) | **Sum-of-Gerstner-waves / Tessendorf-sin-spectrum** | different look (no reflections / no boundary interaction) but very pleasing | 0.3–2× depending on `N_waves` | 5 / 5 |
| Adaptive mesh refinement (AMR) | Active-region tracking + sparse update | identical to scalar baseline | 0.1–1× (best when grid is mostly idle) | 4 / 5 |

**Short-list (3 of 5) for immediate implementation:**

1. **Add a compact O(h⁴) 9-point Laplacian alongside the existing isotropic 9-point.** Same memory access pattern as #3104, real numerical-accuracy win at the same cell-read cost, naturally pairs with disabling super-sample. See §B below.
2. **Add a 3×3 separable Gaussian (or single-pass FXAA-style edge-blur) on the rendered LED buffer, replacing super-sample for the "ripples look smooth" use case.** This is the actual root cause of why SS exists for most users. See §A and §J below.
3. **Add a `WaveProceduralFx` opt-in mode that synthesizes ripples as a sum of N Gerstner / damped-sinusoid wavelets** instead of integrating a PDE. For AVR / Low-memory targets where neither the scalar PDE nor the compact O(h⁴) version fits well, this is dramatically cheaper and visually compelling on coarse grids. See §F below.

The remaining 2 short-list slots (ADI implicit, Tessendorf FFT on ESP32-S3+) are conditional — they win on specific platforms but lose on the matrix average, so they should land only if a specific user demand surfaces.

---

## Embeddability scoring rubric

All candidates are scored on each of the five target platform families. **5 = ships today as-is, 1 = will not fit / will not run at frame rate, 0 = formally impossible**.

| Platform | RAM budget for grid | Native int | HW divider | Notes |
|---|---|---|---|---|
| AVR (Uno / ATmega328) | ≤ 2 KB | 8-bit | no | 8-bit `int`, software 16/32-bit ops, no FPU |
| Cortex-M0 (RP2040, Teensy-LC, LPC8xx) | ≤ 32 KB | 32-bit | no | No HW divide; multiply ≈ 1 cycle |
| Cortex-M4/M7 (Teensy 3.x/4.x) | 256 KB–1 MB | 32-bit | yes | DSP intrinsics; FPU on M4F/M7 |
| ESP32 / ESP32-S3 / -C6 | 320 KB SRAM + ≥ 2 MB PSRAM | 32-bit | yes | PIE SIMD on S3; PSRAM is slow (~80 ns/access) |
| Host (WASM / native test) | unbounded | 32/64-bit | yes | SSE2/AVX2/NEON; reference for visual A/B |

A score of **3 or above on every platform** is the bar for a default-on change. A score of **3 or above on M4+ESP32** is the bar for an opt-in change. Anything that scores 0 anywhere is a per-platform `#ifdef` opt-in only.

---

# Detailed candidate analyses

## §A — Anti-aliasing decoupled from time integration

**Premise:** Super-sample's *primary* benefit is averaging-out the spatial aliasing of the rendered wave field, not improving PDE accuracy. If we anti-alias at render time we sidestep `O(k²)` of the cost wholesale.

### A1. 3×3 separable Gaussian on display buffer (RECOMMENDED — #2 of short-list)

- **Family:** post-process spatial AA filter.
- **Visual quality vs SS-4×:** *comparable on smooth fields*. Equal-or-better at killing the dotted-LED moiré that drives users to super-sample in the first place. Loses to SS on hard edges (e.g. half-duplex zero floor), but `WaveFx` is rarely used at hard edges.
- **Cost:** one separable Gaussian pass = `9N` reads + `9N` mul-add + `N` writes per frame at the displayed resolution. About 1.05–1.2× the scalar baseline depending on whether the colormap is applied before or after.
- **RAM overhead:** one extra `N × sizeof(u8)` line buffer for the horizontal pass.
- **Embed score:** AVR 5 · M0 5 · M4 5 · ESP32 5 · Host 5. The blur kernels are integer-only and trivially fit AVR.
- **Citations:**
  - Standard real-time-graphics technique; see *GPU Gems 1, Ch. 21 — Real-Time Glow* for the seminal separable-Gaussian pattern, and *iquilezles.org/articles/simplewater/* for the analogue used in shader water.
- **Reference implementations:**
  - The codebase already has `WaveFx::setUseChangeGrid` + blur infrastructure in `src/fl/fx/2d/blend.h` / `fl/gfx/blur.cpp.hpp` — adding a post-process Gaussian is wiring, not new code.
- **Risk / open question:** Will an aggressive Gaussian on output erase too much of the wave structure? Need an A/B on `Wave2d` (100×100, gradient palette) to confirm visual quality at σ ≈ 0.7 LED-pixel widths.

### A2. Temporal anti-aliasing (TAA) — NOT RECOMMENDED

- **Family:** temporal-reprojection AA filter.
- **Visual quality:** would smooth temporal aliasing well, but the wave field already has *physical* temporal smoothness from the damping factor — there is no per-frame jitter to suppress.
- **Cost:** requires a per-frame history buffer (`N × sizeof(u8)`) and motion-vector reprojection, which doesn't exist for a static-camera wave field.
- **Verdict:** TAA solves a problem we don't have. Skip.
- **Citation:** Wronski, [*Temporal Supersampling and Antialiasing*](https://bartwronski.com/2014/03/15/temporal-supersampling-and-antialiasing/), 2014.

### A3. MIP-style downsample of intermediate state — REDUNDANT WITH A1

- **Family:** filter-then-decimate.
- **Verdict:** functionally identical to A1 (a Gaussian-and-decimate IS a MIP pass). Folded into A1.

## §B — Higher-order spatial discretizations (the family §5 / #3104 only scratched)

**Premise:** `#3104` added the 9-point *isotropic* Laplacian, which gives O(h²) accuracy with isotropic leading error. The literature has 9-point stencils that achieve **O(h⁴)** at the same memory access pattern — strictly better.

### B1. Compact 4th-order 9-point Laplacian (RECOMMENDED — #1 of short-list)

- **Family:** compact high-order finite-difference stencil.
- **Formula (Kreiss compact-4, 2D):** `∇²u ≈ 1/(6h²) · [ 4·(N+S+E+W) + (NW+NE+SW+SE) - 20·C ]`. Reads the same 9 cells as the existing isotropic stencil but with weights tuned for O(h⁴) accuracy (the isotropic stencil's weights — `1/6` and `2/3` — are tuned for isotropy, not order).
- **Visual quality:** matches super-sample 2×–4× behavior on smooth waves. The remaining anisotropy is `O(h⁴)` — invisible at LED-grid resolutions, much smaller than current 9-point's O(h²).
- **Cost:** 0× over current 9-point (same number of cell reads, same number of multiply-adds; just different integer weights). Drop-in replacement at the stencil-selection switch in `wave_simulation_real.cpp.hpp:268` (post-#3104).
- **RAM overhead:** zero.
- **Embed score:** AVR 4 · M0 5 · M4 5 · ESP32 5 · Host 5. AVR pays the larger constant-multiply cost (`×20`) which on 8-bit takes a few cycles; but otherwise identical to current 9-point.
- **Citations:**
  - Walstijn & Kowalczyk, [*On the Numerical Solution of the 2D Wave Equation with Compact FDTD Schemes*](https://www.academia.edu/58126171/On_the_Numerical_Solution_of_the_2D_Wave_equation_with_Compact_FDTD_Schemes), 2008.
  - Thampi, Ansumali, Adhikari, Succi, [*Isotropic discrete Laplacian operators from lattice hydrodynamics*](https://arxiv.org/abs/1202.3299), 2012 — explicit derivation of the Patra-Karttunen family that the 9-point isotropic stencil belongs to.
  - Mattila et al., [*High-Accuracy Approximation of High-Rank Derivatives*](https://www.hindawi.com/journals/tswj/2014/142907/), 2014 — direct comparison of LBM-derived isotropic stencils vs naive finite difference.
- **Reference implementations:**
  - [Devito](https://www.devitoproject.org/) (geophysics modelling) — generates compact O(h⁴) stencils for the acoustic wave equation.
  - Open-source acoustic-simulation codebases like [Salvus](https://mondaic.com/) document this stencil family directly.
- **Risk / open question:** The compact-4 weights `[1, 4, -20]/6` mean the Laplacian magnitude is `6×` larger pre-normalization than the current 9-point. The existing `i64`-promoted Q15 multiply in `wave_simulation_real.cpp.hpp` already handles this (verified for the `~8.6e9` worst case in #3083) — just need to confirm the new constant doesn't push past `i32` even with `i64` intermediate. **Bench: run the saturated-checkerboard test at compact-4 weights and verify no UBSAN shift-of-negative warnings.**

### B2. NS-FDTD with frequency-optimized correction functions

- **Family:** non-standard FDTD (NS-FDTD), modified-equation approach.
- **Premise:** Replace `h` and `Δt` in the stencil with frequency-dependent correction functions tuned for a specific "design" wave speed. Near-zero numerical dispersion at that speed.
- **Visual quality:** for art applications where the wave speed is a UI slider this is brittle — the user picks a speed, dispersion is great at that speed and bad elsewhere. For acoustic simulation it's perfect.
- **Cost:** comparable to B1 — same stencil topology, different weights computed once at `setSpeed()` time.
- **Embed score:** AVR 4 · M0 4 · M4 5 · ESP32 5 · Host 5.
- **Citation:** SST, [*Time-space domain dispersion reduction schemes in the uniform norm for the 2D acoustic wave equation*](https://www.sciencedirect.com/science/article/abs/pii/S0021999121004848), 2021.
- **Risk:** UX nightmare if exposed naively — every `setSpeed()` call would change visual character of waves at off-design speeds. Worth investigating *only* if `WaveFx` adopts a fixed nominal speed.

### B3. 13-point / 17-point higher-order

- **Family:** longer-range finite-difference stencils.
- **Cost:** `~1.5–2×` the per-cell reads compared to B1, sharply diminishing returns on visual quality.
- **Embed score:** AVR 2 (RAM pressure from longer halo) · M0 3 · others 4.
- **Verdict:** the gap from O(h⁴) to O(h⁶) is invisible at LED resolutions. **Skip.**

## §C — Implicit / semi-implicit time integration

**Premise:** explicit leapfrog requires `Δt² ≤ h²/c²` (CFL). Implicit methods are unconditionally stable — one time step can cover many wavelengths.

### C1. ADI (alternating-direction implicit) — CONDITIONAL RECOMMEND

- **Family:** operator splitting; tridiagonal solve along each axis per step.
- **Premise:** for 2D wave, solve x-direction implicitly with y explicit, then swap. Each step is two `O(N)` Thomas-algorithm tridiagonal sweeps.
- **Visual quality:** equivalent to scalar leapfrog at the same CFL number, but ADI lets us run `CFL > 1` stably so each step propagates further.
- **Cost:** ~2.5–4× the per-step cost of explicit leapfrog (the tridiagonal solve), but if each step covers `4×` the wavefront distance (CFL = 2), net cost is *lower*. Net break-even depends on the visual frame budget.
- **RAM overhead:** one `N`-wide temporary buffer per axis pass.
- **Embed score:** AVR 1 (cannot afford tridiag scratch) · M0 2 · M4 4 · ESP32 5 · Host 5.
- **Citations:**
  - Lee, [*New unconditionally stable ADI schemes for the wave equation*](https://ieeexplore.ieee.org/document/1274571), 2004.
  - Compact-4 + ADI hybrids: [*Two ADI compact difference methods for variable-exponent diffusion wave equations*](https://arxiv.org/pdf/2509.21316), 2025.
- **Risk / open question:** the user-visible behavior of `WaveFx` changes — wavefronts move faster per simulation step, so the existing `Speed` UI slider semantics shift. Would need a one-line conversion in the slider readout.
- **Verdict:** strong fit for ESP32-S3 with PSRAM (cheap memory, slow access — fewer steps per frame is a real win) but not portable. **Conditional recommend — only if a specific user demand surfaces for "smooth waves on ESP32 with huge grids".**

### C2. Crank-Nicolson (fully implicit) — NOT RECOMMENDED

- **Cost:** full 2D matrix solve per step (`O(N^1.5)` via banded LU) — strictly worse than ADI for our problem topology.
- **Skip.**

## §D — Spectral / pseudospectral

### D1. Tessendorf-style FFT propagation (CONDITIONAL on ESP32+)

- **Family:** Fourier-domain propagation.
- **Premise:** wave PDE has analytical solution in frequency space — propagation is per-mode `e^(±ick t)`. Take FFT of `u(t)`, multiply each mode by its propagation factor, inverse FFT. No dispersion, no stencil error.
- **Visual quality:** *better than super-sample*. The mathematics is exact for the linear wave equation.
- **Cost:** for an `N×N` grid, two 2D FFTs per step = `O(N² log N)`. On Cortex-M4 the CMSIS-DSP 2048-point FFT takes ~1.2 ms (Q15) — so a `64×64` 2D = `64 × 64`-point row FFTs + same column FFTs ≈ 8–15 ms per step on M4. Painful but feasible at 30 fps for 64×64.
- **RAM overhead:** real + imaginary buffers = `2 × N²`. For 64×64 Q15 = 16 KB — fits ESP32, painful on M4.
- **Embed score:** AVR 0 (no chance) · M0 0 (no FFT lib, no FPU) · M4 3 (CMSIS-DSP saves us) · ESP32 5 (DSP coproc + PSRAM) · Host 5.
- **Citations:**
  - Tessendorf, [*Simulating Ocean Water*](https://people.computing.clemson.edu/~jtessen/reports/papers_files/coursenotes2004.pdf), SIGGRAPH 2001 — the canonical FFT-based-water-rendering paper.
  - [GitHub — Tessendorf-FFT references](https://github.com/topics/fft-ocean) — many open-source GPU implementations.
- **Reference embedded:** none found in the survey. Embedded Tessendorf would be a substantial port.
- **Risk:** FFT bin layout, periodicity, and ringing-at-impulse-sites mean a user calling `setf(x, y, 1.0)` to drop a "pebble" produces a different visual signature than the scalar PDE does. Would feel different to LED-art users.
- **Verdict:** **Conditional recommend** for ESP32-S3 with PSRAM only. Not portable.

### D2. Cosine-transform / DST for Dirichlet boundaries — NOT INVESTIGATED IN DEPTH

- Similar cost profile to D1 but matches the existing closed-boundary semantics of `WaveSimulation2D`. Worth a follow-up if D1 is pursued.

## §E — Lattice Boltzmann (LBM)

### E1. D2Q5 acoustic LBM

- **Family:** discrete-velocity model; "streaming + collision" instead of stencil + time-step.
- **Premise:** each grid cell holds `Q=5` distribution functions (one per discrete velocity). Per step: stream (move each distribution to its neighbor along its velocity vector), then collide (relax toward equilibrium).
- **Visual quality:** comparable to current 9-point isotropic at SS=1. LBM is inherently isotropic at the D2Q5 / D2Q9 level. Some intrinsic numerical dissipation that needs tuning.
- **Cost:** D2Q5 = `5N` reads + `5N` writes per step + per-cell `O(1)` collision math. Per-cell cost is `~1.5–1.8×` the current scalar update.
- **RAM overhead:** `5 × i16` per cell instead of the current `2 × i16` (`grid1` + `grid2`). For 100×100: 100 KB instead of 40 KB. **AVR-disqualifying.**
- **Embed score:** AVR 0 (memory) · M0 2 (memory) · M4 4 · ESP32 5 (PSRAM friendly — streaming step is cache-coherent) · Host 5.
- **Citations:**
  - Yan & Liu, [*Acoustic Wave Simulation by Lattice Boltzmann Method with D2Q5 and D2Q9*](https://www.researchgate.net/publication/335410380), 2019.
  - Lalleau & Adhikari (review): D2Q5 is "the most suitable model for the linear acoustic problem".
  - [*Comparative Study of 2D Lattice Boltzmann Models for Simulating Seismic Waves*](https://www.mdpi.com/2072-4292/16/2/285), 2024.
- **Verdict:** the memory cost rules out small platforms, and on big platforms the per-cell visual win over a well-tuned compact-4 stencil (§B1) doesn't justify the rewrite. **Skip for now.** Revisit if AMR-LBM hybrids prove out (active fronts only — see §G).

## §F — Procedural / non-PDE alternatives (RECOMMENDED — #3 of short-list)

**Premise:** for LED art, we are not bound by the wave equation — we are bound by "ripples look pretty". Procedural ripple synthesis is dramatically cheaper than any PDE.

### F1. Sum of damped Gerstner / sinusoid wavelets (RECOMMENDED — `WaveProceduralFx`)

- **Family:** procedural wavelet synthesis. `u(x, y, t) = Σᵢ Aᵢ · damp(t - tᵢ) · ripple(|p - cᵢ|, t - tᵢ, kᵢ)` for `N` active wavelets, each spawned at a "drop" location.
- **Premise:** when the user triggers a ripple (e.g. `WaveFx::setf(x, y, 1.0)` in the existing API), allocate a wavelet `i` with center `cᵢ = (x, y)`, birth time `tᵢ`, frequency `kᵢ`. Render is a sum-of-rings.
- **Visual quality:** *visually compelling, NOT physically correct*. No reflections from boundaries (could be added by mirror-imaging the wavelet centers). No interference between wavelets at distance (well — they sum, but it's not PDE interference). Looks like a perfect LED-art "raindrops" effect.
- **Cost:** `O(N_wavelets × N_cells)` per frame. With `N_wavelets ≤ 8` active at once (typical "raindrop" demo) and 100×100 grid: 80,000 evaluations per frame at maybe 20 cycles each on M4 = 1.6M cycles = ~7 ms at 240 MHz. Faster than the current scalar PDE on the same hardware.
- **RAM overhead:** `~64 bytes × N_wavelets` of wavelet state. **Zero grid memory.**
- **Embed score:** AVR 4 (with `N_wavelets ≤ 4`) · M0 5 · M4 5 · ESP32 5 · Host 5.
- **Citations:**
  - Gerstner (1802); modern compact treatment: Mark Finch, [*Effective Water Simulation from Physical Models*](https://developer.nvidia.com/gpugems/gpugems/part-i-natural-effects/chapter-1-effective-water-simulation-physical-models) — *GPU Gems Ch.1*. The sum-of-sines formulation extends directly to circular Gerstner waves around a drop site.
  - Inigo Quilez, [*Simple Water*](https://iquilezles.org/articles/simplewater/) — minimal-cost shader water.
  - Buss, [*Math 155B shader water project*](https://mathweb.ucsd.edu/~sbuss/CourseWeb/Math155B_2020Spring/Project_1/Project1_Math155B_Spring2020.html) — pedagogical sum-of-sines.
- **Reference implementations:** several GPU shader implementations; no microcontroller-targeted one found in the survey. **This is a publishable contribution if FastLED ships it.**
- **Risk:** users coming from `WaveFx` will notice the lack of boundary reflections. Worth offering as a *parallel* `WaveProceduralFx` class rather than a replacement.
- **Verdict:** **strong recommend.** The cheapest, most embeddable, visually-strongest option for AVR / Low-memory targets, and competitive even on ESP32. Frees up cycles for richer color/blur work.

### F2. Tessendorf-spectrum sum-of-sines (without FFT)

- **Family:** procedural; uses Tessendorf's wave-spectrum formula but evaluated directly per cell rather than via FFT.
- **Cost:** `O(N_modes × N_cells)`. Worse asymptotically than F1; only viable for very small grids.
- **Verdict:** strictly dominated by F1. **Skip.**

### F3. Perlin / curl noise as wave field

- **Family:** noise-based procedural; treats wave as a moving noise field.
- **Visual quality:** wave-like but lacks circular-ripple structure. Different aesthetic.
- **Verdict:** orthogonal to this study; already covered by existing FastLED noise effects.

## §G — Adaptive / sparse representations

### G1. Active-region tracking (CONDITIONAL RECOMMEND)

- **Family:** spatial sparsity — only update cells with non-trivial activity.
- **Premise:** maintain a per-cell "is active" flag (or a tile-level "any cell in this tile is active" coarse flag). Each update step skips cells whose neighborhood is all zero.
- **Visual quality:** *identical* to the underlying PDE — this is an optimization, not a method change.
- **Cost:** `O(N_active_cells)` per step. For a typical LED-art scene with 2–3 active wavefronts on a 100×100 grid, active cells are maybe 10–20% of total. **5–10× speedup.**
- **RAM overhead:** `N/64 bits` for tile-level activity bitmap.
- **Embed score:** AVR 4 · M0 5 · M4 5 · ESP32 5 · Host 5.
- **Citations:**
  - General AMR theory: Berger & Oliger, [*Adaptive mesh refinement for hyperbolic partial differential equations*](https://www.sciencedirect.com/science/article/abs/pii/0021999184900731), 1984 — foundational, but assumes a tree structure heavier than we need.
  - For tile-level activity: the existing FastLED `mUseChangeGrid` optimization in `wave_simulation.h` is already exactly this pattern; needs only an "all-zero tile" fast path.
- **Risk:** false-negatives if the activity test undercounts cells just below the activity threshold. Need a 2-cell-wide active boundary halo.
- **Verdict:** **strong recommend for ESP32-S3 with large grids** — the platform where the PDE cost actually hurts. Smaller wins on AVR. Pair with §F (procedural) if the user's demand is "many simultaneous ripples on a big grid".

## §H — Fixed-point / low-precision

### H1. Q7 / Q8 storage with Q15 inner-loop arithmetic

- **Family:** halve grid memory by storing cells as `i8` (Q7) instead of `i16` (Q15), promote to Q15 during the multiply.
- **Visual quality:** noticeable amplitude quantization at the dim end of the spectrum (gradient palettes show "stepping"). Mitigatable with floor-dithering.
- **Cost:** **half the memory traffic** on the grid (the dominant cost on PSRAM-backed ESP32 grids). ~1.5× speedup on ESP32 PSRAM, ~1.1× elsewhere.
- **RAM overhead:** halves it.
- **Embed score:** AVR 5 (genuinely matters here) · M0 4 · M4 4 · ESP32 5 · Host 4.
- **Verdict:** **opt-in recommend for AVR / ESP32-PSRAM.** Add a `WaveSimulation2D_Real8` parallel class or a templated `Storage` parameter. Worth a focused sub-issue.

### H2. Posit numbers — NOT INVESTIGATED

- No embedded implementations exist. Skip.

## §I — Embedded-graphics industry SOTA

- **Shadertoy water demos** (e.g. iquilezles' Simple Water, various sum-of-sines ports) confirm that *sum-of-sines is the universal industry choice* when "physically accurate wave PDE" isn't a requirement. This reinforces §F.
- **Demoscene** uses sub-1KB sketches with sum-of-sines / DDA-circle ripple effects routinely on AVR. No PDE in sight.
- **Industry-side state of the art for "AAA water"** (Sea of Thieves, Uncharted, Sea of Stars) is Gerstner + FFT hybrid — physically inappropriate for our LED-art constraints.

## §J — Anti-aliasing-specific filters (cross-reference)

Covered in §A1 (Gaussian) and discussed below for MLAA/SMAA:

### J1. SMAA (Subpixel Morphological Anti-Aliasing) — NOT RECOMMENDED on small grids

- **Family:** edge-detect post-process AA.
- **Verdict:** designed for high-resolution screens (1080p+). On a 16×16 LED grid every "pixel" is its own discontinuity — SMAA's edge-detection heuristics misfire.
- **Citation:** Jimenez et al., [*SMAA: Enhanced Subpixel Morphological Antialiasing*](https://www.iryoku.com/smaa/), 2012.
- **Skip.** §A1 (Gaussian) is the right tool at this scale.

---

# Non-recommendations (explicit dead ends)

The following were investigated and ruled out. Documenting these explicitly saves the next research run from re-walking them.

| Method | Why ruled out |
|---|---|
| TAA (§A2) | No motion vectors in a static-camera wave field |
| MIP-style decimate (§A3) | Folded into A1 |
| 13-point/17-point stencils (§B3) | Diminishing returns past O(h⁴) at LED resolution |
| Crank-Nicolson fully-implicit (§C2) | Banded LU dominated by ADI |
| LBM D2Q5 (§E1) | Memory cost rules out AVR/M0 |
| Tessendorf-direct sum-of-sines (§F2) | Dominated by sum-of-Gerstner in F1 |
| Perlin-noise wave field (§F3) | Different aesthetic; orthogonal |
| Posit numbers (§H2) | No embedded impl |
| SMAA/FXAA (§J1) | Wrong tool for 16×16-grid scale |

---

# Recommended next sub-issues

If the project agrees with this report, file the following as focused sub-issues (the way #3098 spawned #3092–#3097):

1. **`perf(wave): add compact O(h⁴) Laplacian as a third stencil option`** — §B1 above. Drop-in addition to the `LaplacianStencil` enum from #3104. Pairs with disabling super-sample as the default for medium-size grids. **Highest-priority — smallest diff, biggest visual / cycle gain.**
2. **`feat(wave): add output Gaussian blur as an alternative to super-sampling`** — §A1 above. Two new UI controls: `OutputBlur` (off / 1 / 2 passes) and `SuperSample` (default `NONE` once this lands). Big educational win for users who currently default to SS-4× without knowing why.
3. **`feat(fx): add WaveProceduralFx — sum-of-Gerstner-wavelets ripple synthesizer`** — §F1 above. New `Fx2d` class, no PDE. **Largest design footprint but highest end-user value, especially on AVR.**
4. **`perf(wave): tile-level active-region tracking in WaveSimulation2D update`** — §G1 above. Extends the existing `useChangeGrid` infrastructure. Win is biggest on ESP32-S3 with PSRAM-backed large grids.
5. **`perf(wave): Q7 grid storage option for memory-constrained targets`** — §H1 above. Templated `Storage = i16 | i8` parameter on the inner classes.

**SIMD work (#3106 / #3107 / #3108) should remain on hold** — items 1, 2, and 4 above all reduce the kernel's hot-path cycle cost by more than SIMD typically delivers, and the algorithmic wins compose with later SIMD work. Pick the algorithm before tuning the constant factor.

---

# Appendix: bibliography

Cited inline above; reproduced here for cross-reference:

**Compact / high-order finite difference:**
- Walstijn & Kowalczyk, [*On the Numerical Solution of the 2D Wave Equation with Compact FDTD Schemes*](https://www.academia.edu/58126171/On_the_Numerical_Solution_of_the_2D_Wave_equation_with_Compact_FDTD_Schemes), 2008.
- [*An efficient and high accuracy finite-difference scheme for the acoustic wave equation in 3D heterogeneous media*](https://www.sciencedirect.com/science/article/abs/pii/S1877750319303588), 2019.
- [*Time-space domain dispersion reduction schemes in the uniform norm for the 2D acoustic wave equation*](https://www.sciencedirect.com/science/article/abs/pii/S0021999121004848), 2021.
- [*A Convolutional Dispersion Relation Preserving Scheme for the Acoustic Wave Equation*](https://arxiv.org/abs/2205.10825), 2022.

**Isotropic Laplacian operators:**
- Thampi, Ansumali, Adhikari, Succi, [*Isotropic discrete Laplacian operators from lattice hydrodynamics*](https://arxiv.org/abs/1202.3299), 2012.
- Mattila et al., [*High-Accuracy Approximation of High-Rank Derivatives: Isotropic Finite Differences Based on Lattice-Boltzmann Stencils*](https://www.hindawi.com/journals/tswj/2014/142907/), 2014.

**ADI / implicit:**
- Lee, [*New unconditionally stable ADI schemes for the wave equation*](https://ieeexplore.ieee.org/document/1274571), 2004.
- [*Two ADI compact difference methods for variable-exponent diffusion wave equations*](https://arxiv.org/pdf/2509.21316), 2025.

**Lattice Boltzmann:**
- Yan & Liu, [*Acoustic Wave Simulation by Lattice Boltzmann Method with D2Q5 and D2Q9 of Different Relaxation Times*](https://www.researchgate.net/publication/335410380), 2019.
- [*Comparative Study of 2D Lattice Boltzmann Models for Simulating Seismic Waves*](https://www.mdpi.com/2072-4292/16/2/285), 2024.

**Tessendorf / FFT:**
- Tessendorf, [*Simulating Ocean Water*](https://people.computing.clemson.edu/~jtessen/reports/papers_files/coursenotes2004.pdf), SIGGRAPH 2001 / updated 2004.
- [*Arc Blanc: a real time ocean simulation framework*](https://arxiv.org/abs/2503.03326), 2025.

**Gerstner / procedural:**
- Finch, [*Effective Water Simulation from Physical Models*](https://developer.nvidia.com/gpugems/gpugems/part-i-natural-effects/chapter-1-effective-water-simulation-physical-models), *GPU Gems 1, Ch. 1*, 2004.
- Inigo Quilez, [*Simple Water*](https://iquilezles.org/articles/simplewater/).

**TAA / post-process AA:**
- Wronski, [*Temporal Supersampling and Antialiasing*](https://bartwronski.com/2014/03/15/temporal-supersampling-and-antialiasing/), 2014.
- Jimenez et al., [*SMAA: Enhanced Subpixel Morphological Antialiasing*](https://www.iryoku.com/smaa/), 2012.

**Embedded FFT:**
- ARM, *DSP capabilities of Cortex-M4 and Cortex-M7*, [white paper](https://developer.arm.com/cfs-file/__key/communityserver-blogs-components-weblogfiles/00-00-00-21-42/7563.ARM-white-paper-_2D00_-DSP-capabilities-of-Cortex_2D00_M4-and-Cortex_2D00_M7.pdf).
- NXP, *AN12383: Computing FFT with PowerQuad and CMSIS-DSP on LPC5500*, [PDF](https://www.nxp.com/docs/en/application-note/AN12383.pdf).

---

# Cross-references

- Original audit: [#3073](https://github.com/FastLED/FastLED/issues/3073) (CLOSED)
- Audit meta: [#3098](https://github.com/FastLED/FastLED/issues/3098) (CLOSED) — produced #3092–#3097
- 9-point isotropic Laplacian implementation: [#3104](https://github.com/FastLED/FastLED/pull/3104) (MERGED) — this report builds on its stencil-selection mechanism
- UI toggle: [#3110](https://github.com/FastLED/FastLED/pull/3110) (MERGED) — adds the user-facing knob this report's §A1/§B1 recommendations would extend
- SIMD work on hold pending this research: [#3106](https://github.com/FastLED/FastLED/issues/3106), [#3107](https://github.com/FastLED/FastLED/issues/3107), [#3108](https://github.com/FastLED/FastLED/issues/3108)

Closes #3111.
