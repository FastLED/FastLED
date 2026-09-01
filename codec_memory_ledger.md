# MP3 codec memory ledger

<!-- codec-memory-ledger:v1 -->

This ledger is generated from the identical host-native rig for both minimp3
builds. `working-ram` is persistent decoder state plus algorithm scratch and
the stream staging buffer; it is the Phase 1 acceptance metric. `pipeline-peak`
additionally includes the caller-owned 4,608-byte PCM output. Both builds are
gated at or below 24 KiB.

The retired Helix backend measured 27,952 bytes of working RAM on this same rig
-- the tracker's historical "~24 KB" Helix baseline omitted its 4,096-byte
stream buffer. Its rows were deleted with the backend (FastLED#4056); the one
number that outlived it is the 15,292-byte static-table budget below, which is
Helix's 12,744 bytes of tables +20%.

Measurements use Clang 18, `-Os`, x86-64 Linux ELF, and the
`l3-hecommon.bit` decode fixture. The gate rejects increases over 2%.

`minimp3-fixed` is the `MINIMP3_FIXED_POINT` build, which is what ships;
`minimp3-float` is the `MINIMP3_FLOAT_POINT` reference build the fixed-vs-float
gates compare against. Every audit translation unit pins its variant
explicitly, because the header's default is now fixed point and an unpinned TU
would silently measure the other build.

`minimp3-fixed` carries no `stack-watermark-observed` or `massif-codec-peak`
row: those come from the live watermark rig, which is pinned to the float build
so that it and the object it links agree on `mp3dec_scratch_t`'s size. The
fixed build's stack budget is enforced by the compiler-derived
`stack-callgraph` figure instead, which is the fail-closed metric the 2 KiB
gate actually uses.

The fixed-point build costs 128 bytes more scratch than the float build --
scalefactor gains are carried as mantissa plus exponent rather than as a
single float -- and 3,021 more bytes of text. It uses less decode stack than
the float build (896 against 1,200), because its polyphase back-end has no
vector code paths to keep live.

Both figures dropped by about 1 KB in FastLED#4116, which moved
`L12_scale_info` off the stack. Layer I/II scale info is ~1090 bytes in the
fixed build and used to sit inside `mp3dec_decode_frame_r`, where it was almost
the whole 1,480-byte frame; the fixed decoder was running at 94% of its 2 KiB
budget, and the figure moved with the compiler's inlining decisions rather than
with anything in the decoder. It now shares arena storage with Layer III's
`maindata` through a union -- a frame is one layer or the other, never both --
so `scratch` and `working-ram` do not move and the saving is free. The budget
now sits at 44% for fixed and 59% for float.

## Summary

| backend | metric | bytes-or-count |
| --- | --- | ---: |
| minimp3-float | decoder-state | 11276 |
| minimp3-float | scratch | 7808 |
| minimp3-float | stream-buffer | 4096 |
| minimp3-float | pcm-output | 4608 |
| minimp3-float | codec-core | 19084 |
| minimp3-float | working-ram | 23180 |
| minimp3-float | pipeline-peak | 27788 |
| minimp3-float | allocation-count | 4 |
| minimp3-float | stack-max-frame | 824 |
| minimp3-float | stack-callgraph | 1200 |
| minimp3-float | stack-watermark-observed | 1224 |
| minimp3-float | static-tables | 7878 |
| minimp3-float | object-text | 23160 |
| minimp3-float | object-data | 0 |
| minimp3-float | object-bss | 0 |
| minimp3-float | massif-codec-peak | 27788 |
| minimp3-fixed | decoder-state | 11276 |
| minimp3-fixed | scratch | 7936 |
| minimp3-fixed | stream-buffer | 4096 |
| minimp3-fixed | pcm-output | 4608 |
| minimp3-fixed | codec-core | 19212 |
| minimp3-fixed | working-ram | 23308 |
| minimp3-fixed | pipeline-peak | 27916 |
| minimp3-fixed | allocation-count | 4 |
| minimp3-fixed | stack-max-frame | 456 |
| minimp3-fixed | stack-callgraph | 896 |
| minimp3-fixed | static-tables | 8077 |
| minimp3-fixed | object-text | 26181 |
| minimp3-fixed | object-data | 0 |
| minimp3-fixed | object-bss | 0 |

Minimp3-float working RAM is 23,180 bytes (1,396 bytes below 24 KiB), both static
call-graph estimates are at or below 2 KiB, and minimp3-float's 7,878 bytes of
static tables are below the 15,292-byte budget (the retired Helix baseline
+20%), as are minimp3-fixed's 8,077.

The live watermark is painted below the worker's current stack pointer and is
a conservative observation that includes the audit call boundary and its
256-byte safety guard. The 2 KiB acceptance gate uses the compiler-derived,
optimized decoder callgraph; every reachable emitted function must have a
stack-usage record or the audit fails closed.

`stack-watermark-observed` is gated like every other metric again, because the
measurement is now reproducible. It was demoted to informational in FastLED#4105
because it landed on exact-but-different values on different CI runners while the
decoder was byte-identical (1736 and 3336 for the retired Helix backend, each
exact, one reproduced on a re-run).

FastLED#4106 found the cause by instrumenting rather than inferring. The harness
forwarded the decoder's `memcpy`/`memmove`/`memset` to glibc, so glibc's frames
sat inside the measured window -- and those differ between glibc builds and CPU
dispatch paths. Swapping them for plain loops moves the figure from 2952 to 2056
and pins it: five runs identical, and unchanged under `GLIBC_TUNABLES` settings
that select different `mem*` implementations. It was also measuring the wrong
thing, since no MCU links glibc's vectorised `memcpy` and the 2 KiB budget is an
MCU budget.

The two independent instruments now corroborate each other: the compiler-derived
callgraph says 2032 and the live watermark says 2056, a 24-byte gap that is the
audit call boundary. Before the fix they disagreed by 664 bytes.

## Static table inventory

| backend | stage | symbol | bytes |
| --- | --- | --- | ---: |
| minimp3-float | layer1-2 | g_alloc_L1 | 3 |
| minimp3-float | layer1-2 | g_alloc_L2M1_lowrate | 6 |
| minimp3-float | layer1-2 | g_alloc_L2M2 | 9 |
| minimp3-float | scalefactor | g_preamp | 10 |
| minimp3-float | header | g_hz | 12 |
| minimp3-float | layer1-2 | g_alloc_L2M1 | 12 |
| minimp3-float | huffman | tab33 | 16 |
| minimp3-float | dequantization | g_expfrac | 16 |
| minimp3-float | scalefactor | g_scfc_decode | 16 |
| minimp3-float | transform | g_twid3 | 24 |
| minimp3-float | scalefactor | g_mod | 24 |
| minimp3-float | huffman | tab32 | 28 |
| minimp3-float | huffman | g_linbits | 32 |
| minimp3-float | stereo | g_pan | 56 |
| minimp3-float | huffman | tabindex | 64 |
| minimp3-float | transform | g_aa | 64 |
| minimp3-float | transform | g_twid9 | 72 |
| minimp3-float | scalefactor | g_scf_partitions | 84 |
| minimp3-float | header | halfrate | 90 |
| minimp3-float | layer1-2 | g_bitalloc_code_tab | 92 |
| minimp3-float | transform | g_sec | 96 |
| minimp3-float | transform | g_mdct_window | 144 |
| minimp3-float | scalefactor | g_scf_long | 184 |
| minimp3-float | layer1-2 | g_deq_L12 | 216 |
| minimp3-float | scalefactor | g_scf_mixed | 320 |
| minimp3-float | scalefactor | g_scf_short | 320 |
| minimp3-float | dequantization | g_pow43 | 580 |
| minimp3-float | synthesis | g_win | 960 |
| minimp3-float | huffman | tabs | 4328 |
| minimp3-fixed | layer1-2 | g_alloc_L1 | 3 |
| minimp3-fixed | layer1-2 | g_alloc_L2M1_lowrate | 6 |
| minimp3-fixed | layer1-2 | g_alloc_L2M2 | 9 |
| minimp3-fixed | scalefactor | g_preamp | 10 |
| minimp3-fixed | layer1-2 | g_alloc_L2M1 | 12 |
| minimp3-fixed | header | g_hz | 12 |
| minimp3-fixed | dequantization | g_expfrac_q30 | 16 |
| minimp3-fixed | scalefactor | g_scfc_decode | 16 |
| minimp3-fixed | huffman | tab33 | 16 |
| minimp3-fixed | scalefactor | g_mod | 24 |
| minimp3-fixed | transform | g_twid3_q30 | 24 |
| minimp3-fixed | huffman | tab32 | 28 |
| minimp3-fixed | transform | g_aa_ca_q31 | 32 |
| minimp3-fixed | transform | g_aa_cs_q31 | 32 |
| minimp3-fixed | huffman | g_linbits | 32 |
| minimp3-fixed | layer1-2 | g_deq_L12_exp | 54 |
| minimp3-fixed | stereo | g_pan_q30 | 56 |
| minimp3-fixed | huffman | tabindex | 64 |
| minimp3-fixed | transform | g_mdct_window_normal_q30 | 72 |
| minimp3-fixed | transform | g_mdct_window_stop_q30 | 72 |
| minimp3-fixed | transform | g_twid9_q30 | 72 |
| minimp3-fixed | scalefactor | g_scf_partitions | 84 |
| minimp3-fixed | header | halfrate | 90 |
| minimp3-fixed | layer1-2 | g_bitalloc_code_tab | 92 |
| minimp3-fixed | transform | g_sec_q27 | 96 |
| minimp3-fixed | dequantization | g_pow43_exp | 145 |
| minimp3-fixed | scalefactor | g_scf_long | 184 |
| minimp3-fixed | layer1-2 | g_deq_L12_mant | 216 |
| minimp3-fixed | scalefactor | g_scf_mixed | 320 |
| minimp3-fixed | scalefactor | g_scf_short | 320 |
| minimp3-fixed | dequantization | g_pow43_mant | 580 |
| minimp3-fixed | synthesis | g_win | 960 |
| minimp3-fixed | huffman | tabs | 4328 |

The compiler-generated `.LCPI*` literals have zero standalone symbol size and
are excluded; their bytes are already attributed to the functions that own
them by the ELF object. All named read-only tables emitted by `llvm-nm
--print-size --size-sort --demangle` are itemized above.
