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

Minimp3-float working RAM is 23,180 bytes (1,396 bytes below 24 KiB), both static
call-graph estimates are at or below 2 KiB, and minimp3-float's 7,878 bytes of
static tables are below Helix +20% (15,292 bytes).

The live watermark is painted below the worker's current stack pointer and is
a conservative observation that includes the audit call boundary and its
256-byte safety guard. The 2 KiB acceptance gate uses the compiler-derived,
optimized decoder callgraph; every reachable emitted function must have a
stack-usage record or the audit fails closed.

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

The compiler-generated `.LCPI*` literals have zero standalone symbol size and
are excluded; their bytes are already attributed to the functions that own
them by the ELF object. All named read-only tables emitted by `llvm-nm
--print-size --size-sort --demangle` are itemized above.
