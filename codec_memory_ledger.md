# MP3 codec memory ledger

<!-- codec-memory-ledger:v1 -->

This ledger is generated from the identical host-native rig for both MP3
backends. `working-ram` is persistent decoder state plus algorithm scratch and
the selected backend's stream staging buffer; it is the Phase 1 acceptance
metric. `pipeline-peak` additionally includes the caller-owned 4,608-byte PCM
output. The same rig records Helix at 27,952 bytes of working RAM, clarifying
that the tracker's historical "~24 KB" Helix baseline omitted its 4,096-byte
stream buffer. The minimp3 replacement is gated at or below 24 KiB.

Measurements use Clang 18, `-Os`, x86-64 Linux ELF, and the
`l3-hecommon.bit` decode fixture. The gate rejects increases over 2%.

`minimp3-fixed` is the `MINIMP3_FIXED_POINT` build. It carries no
`stack-watermark-observed` or `massif-codec-peak` row: those come from the
live watermark rig, which drives the production decoder wrapper and therefore
sees whichever backend the build selected. Its stack budget is enforced by the
compiler-derived `stack-callgraph` figure instead, which is the fail-closed
metric the 2 KiB gate actually uses.

The fixed-point build costs 128 bytes more scratch than the float build --
scalefactor gains are carried as mantissa plus exponent rather than as a
single float -- and 3,021 more bytes of text. It uses *less* decode stack
(1,920 against 2,032), because its polyphase back-end has no vector code paths
to keep live.

## Summary

| backend | metric | bytes-or-count |
| --- | --- | ---: |
| helix | decoder-state | 18392 |
| helix | scratch | 5464 |
| helix | stream-buffer | 4096 |
| helix | pcm-output | 4608 |
| helix | codec-core | 23856 |
| helix | working-ram | 27952 |
| helix | pipeline-peak | 32560 |
| helix | allocation-count | 10 |
| helix | stack-max-frame | 424 |
| helix | stack-callgraph | 552 |
| helix | stack-watermark-observed | 1736 |
| helix | static-tables | 12744 |
| helix | object-text | 40281 |
| helix | object-data | 664 |
| helix | object-bss | 0 |
| minimp3-float | decoder-state | 11276 |
| minimp3-float | scratch | 7808 |
| minimp3-float | stream-buffer | 4096 |
| minimp3-float | pcm-output | 4608 |
| minimp3-float | codec-core | 19084 |
| minimp3-float | working-ram | 23180 |
| minimp3-float | pipeline-peak | 27788 |
| minimp3-float | allocation-count | 4 |
| minimp3-float | stack-max-frame | 1208 |
| minimp3-float | stack-callgraph | 2032 |
| minimp3-float | stack-watermark-observed | 2056 |
| minimp3-float | static-tables | 7878 |
| minimp3-float | object-text | 23160 |
| minimp3-float | object-data | 0 |
| minimp3-float | object-bss | 0 |
| helix | massif-codec-peak | 32560 |
| minimp3-float | massif-codec-peak | 27788 |
| minimp3-fixed | decoder-state | 11276 |
| minimp3-fixed | scratch | 7936 |
| minimp3-fixed | stream-buffer | 4096 |
| minimp3-fixed | pcm-output | 4608 |
| minimp3-fixed | codec-core | 19212 |
| minimp3-fixed | working-ram | 23308 |
| minimp3-fixed | pipeline-peak | 27916 |
| minimp3-fixed | allocation-count | 4 |
| minimp3-fixed | stack-max-frame | 1480 |
| minimp3-fixed | stack-callgraph | 1920 |
| minimp3-fixed | static-tables | 8077 |
| minimp3-fixed | object-text | 26181 |
| minimp3-fixed | object-data | 0 |
| minimp3-fixed | object-bss | 0 |

Minimp3-float working RAM is 23,180 bytes (1,396 bytes below 24 KiB), both static
call-graph estimates are at or below 2 KiB, and minimp3-float's 7,878 bytes of
static tables are below Helix +20% (15,292 bytes).

The live watermark is painted below the worker's current stack pointer and is
a conservative observation that includes the audit call boundary and its
256-byte safety guard. The 2 KiB acceptance gate uses the compiler-derived,
optimized decoder callgraph; every reachable emitted function must have a
stack-usage record or the audit fails closed.

`stack-watermark-observed` is recorded but **not regression-gated**, which now
matches what the paragraph above always said the acceptance gate was. It is not
reproducible across CI runs: helix has measured 1736 and 3336 on identical
decoder code, each value exact and one of them reproduced on a re-run, and 1816
locally, while `minimp3-float` measures 2056 in every environment tried. The
scan reports the deepest byte *anything* disturbed rather than the deepest byte
the decoder used, so one excursion below the real frame moves it by kilobytes.
The rows below are the values last observed, kept so the audit still requires
the metric to be emitted. FastLED#4106 tracks making it reproducible or
host-keying it the way `codec_cpu_trend.json` keys its baselines.

## Static table inventory

| backend | stage | symbol | bytes |
| --- | --- | --- | ---: |
| helix | header | bitsPerSlotTab | 6 |
| helix | huffman | quadTabOffset | 8 |
| helix | huffman | quadTabMaxBits | 8 |
| helix | header | sideBytesTab | 12 |
| helix | stereo | ISFIIP | 16 |
| helix | header | samplesPerFrameTab | 18 |
| helix | scalefactor | preTab | 22 |
| helix | scalefactor | SFLenTab | 32 |
| helix | header | samplerateTab | 36 |
| helix | transform | c18 | 36 |
| helix | stereo | ISFMpeg1 | 56 |
| helix | stereo | csa | 64 |
| helix | scalefactor | NRTab | 72 |
| helix | huffman | quadTable | 80 |
| helix | transform | coef32 | 124 |
| helix | huffman | huffTabOffset | 128 |
| helix | transform | dcttab | 192 |
| helix | huffman | huffTabLookup | 256 |
| helix | stereo | ISFMpeg2 | 256 |
| helix | header | bitrateTab | 270 |
| helix | header | slotTab | 270 |
| helix | transform | imdctWin | 576 |
| helix | header | sfBandTable | 666 |
| helix | synthesis | polyCoef | 1056 |
| helix | huffman | huffTable | 8484 |
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
