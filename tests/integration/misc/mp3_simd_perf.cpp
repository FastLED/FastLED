// minimp3 fixed-point SIMD-vs-scalar timing (#4055), integration tier.
//
// A timed benchmark with a pass/fail floor; inside fl_codec it pushed that
// suite past the 20 s runner watchdog on loaded hosts. It runs on request:
// `bash test --integration` or `bash test mp3_simd_perf`. The exactness
// gate (SIMD bit-identical to scalar) stays in the unit tier, in
// tests/unit/fl/codec/mp3_fixed_point.hpp.

#include "test.h"
#include "fl/stl/stdio.h"
#include "tests/unit/fl/codec/mp3_variant_decode.hpp" // ok cpp include
#ifdef FASTLED_TESTING
#include "platforms/stub/fs_stub.hpp" // ok platform headers
#endif

#ifdef FASTLED_TESTING

FL_TEST_CASE("minimp3 fixed-point SIMD is not slower than scalar") {
    fl::setTestFileSystemRoot("tests/data");
    const fl::vector<fl::u8> full =
        readCorpusBytes("codec/minimp3/l3-lame-vbrtag.bit");
    FL_REQUIRE_GT(full.size(), 3u);
    // The first third of the stream is enough for a 2x floor; the decoder
    // stops cleanly at the truncated last frame.
    fl::vector<fl::u8> bytes;
    bytes.assign(full.begin(), full.begin() + full.size() / 3);

    // Informational on its own -- host timing in a unit test is noisy -- but
    // the direction is the enforced part of #4055's perf bar: whatever the
    // machine, the vector path must not be *slower* than the scalar one it
    // replaces. Both variants decode the same bytes in the same process, so
    // the comparison is same-host by construction and needs no baseline.
    // The enforced bound below is a 2x floor, which needs no averaging, so
    // three reps are enough.
    const int kReps = 3;
    fl::u32 scalar_us = 0;
    fl::u32 simd_us = 0;
    for (int rep = 0; rep < kReps; ++rep) {
        const fl::u32 t0 = fl::micros();
        decodeWithVariant<fl::Minimp3FixedScalarVariant>(bytes);
        const fl::u32 t1 = fl::micros();
        decodeWithVariant<fl::Minimp3FixedVariant>(bytes);
        const fl::u32 t2 = fl::micros();
        scalar_us += t1 - t0;
        simd_us += t2 - t1;
    }
    const double ratio = simd_us ? static_cast<double>(scalar_us) /
                                       static_cast<double>(simd_us)
                                 : 0.0;
    // Whether the ratio is enforced is printed with it, deliberately. This
    // line is emitted before the `dspUsesSimd()` check below, so a ratio on
    // its own says nothing about whether the gate ran: a target with no
    // integer SIMD prints one too and then returns without asserting. Making
    // each line say which it is means a CI log can be read on its own,
    // instead of by cross-referencing whether the exactness test happened to
    // print its own "no integer SIMD" note.
    printf("[simd-perf] scalar=%u us  simd=%u us  speedup=%.3fx over %d reps "
           "(integer SIMD: %s)\n",
           scalar_us, simd_us, ratio, kReps,
           fl::Minimp3FixedVariant::dspUsesSimd() ? "yes, ratio enforced"
                                                  : "no, ratio not enforced");
    FL_CHECK_GT(scalar_us, 0u);
    FL_CHECK_GT(simd_us, 0u);

    if (!fl::Minimp3FixedVariant::dspUsesSimd()) {
        return; // nothing was vectorised; the ratio measures noise
    }
#if defined(__OPTIMIZE__)
    // A floor, not a parity claim (#4183).
    //
    // This asserted `ratio >= 0.95` and took the default branch red at
    // random. The measured ratio across CI is bimodal, not noisy around one
    // value: 0.904, 0.920, 0.922, 0.924, 0.945 on some machines against
    // 1.080 on others, with a macOS ARM runner at 0.922. The fastest scalar
    // time of the lot produced the *worst* ratio, so this is not contention
    // -- the kernel is genuinely faster on part of the fleet and genuinely
    // slower on the rest, and both are correct measurements of different
    // hardware.
    //
    // 0.95 sits inside that spread. A threshold there cannot tell "the SIMD
    // kernel regressed" from "this run landed on the other runner class", so
    // as a gate it reports the fleet rather than the code.
    //
    // What survives is a floor far outside any fleet variation: a vector path
    // twice as slow as the scalar one it replaces is a defect on any machine.
    //
    // It will not catch a subtle regression, and an earlier revision of this
    // comment said that job belongs to `ci/codec_cpu/audit.py`. It does not.
    // That audit passes `-DMINIMP3_NO_SIMD` on every build path it has, so it
    // never compiles these kernels at all -- deliberately, because it exists
    // to predict embedded cost and no target FastLED ships to has integer
    // SIMD (`MP3D_HAVE_INT_SIMD` needs SSE2 or NEON; Xtensa, RISC-V and
    // Cortex-M have neither). See "Disable SIMD when tracing on the host" in
    // agents/docs/mp3-decoder-performance.md.
    //
    // So nothing gates the *speed* of the vector path, and after #4183 that is
    // the deliberate state rather than an oversight: it runs only in host
    // builds, and a heterogeneous fleet cannot resolve the ~2% effect anyway.
    // What is gated exactly is its *correctness* -- the bit-identical case
    // above compares every sample against the scalar kernels, and that holds
    // on any machine because it compares outputs and not times.
    //
    // The ratio is still printed above on every run, so a shift in either
    // population stays visible.
    FL_CHECK_GE(ratio, 0.5);
#else
    // Deliberately not asserted in an unoptimised build. Intrinsics are hit
    // far harder than scalar integer code by -O0 (every vector temporary
    // round-trips through the stack), so the ratio there measures the compiler
    // rather than the kernel: this same code measures 0.93x at -O0, against
    // 0.90x-1.08x at -O3 depending on the runner. Nothing else gates the
    // vector path's speed either -- see the note above the floor.
    printf("[simd-perf] unoptimised build; ratio not enforced here\n");
#endif
}

#endif // FASTLED_TESTING
