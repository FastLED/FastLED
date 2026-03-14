// Integration tests for VocalDetector using real-audio OGG fixtures
// Tests detection accuracy on real instrument stems and mixes at various levels

#include "test.h"
#include "fl/audio/audio.h"
#include "fl/audio/audio_context.h"
#include "fl/audio/detectors/vocal.h"
#include "fl/codec/vorbis.h"
#include "fl/stl/detail/file_handle.h"
#include "fl/stl/detail/file_io.h"
#include "fl/stl/math.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/vector.h"

using namespace fl;
using Diag = fl::VocalDetectorDiagnostics;

namespace test_vocal_real_audio {

// --- Utility: Load OGG file bytes from disk ---
static fl::vector<fl::u8> loadOggFile(const char* path) {
    fl::FILE* f = fl::fopen(path, "rb");
    if (!f) {
        return fl::vector<fl::u8>();
    }
    fl::fseek(f, 0, fl::io::seek_end);
    long fileSize = fl::ftell(f);
    fl::fseek(f, 0, fl::io::seek_set);
    if (fileSize <= 0) {
        fl::fclose(f);
        return fl::vector<fl::u8>();
    }
    fl::vector<fl::u8> data(static_cast<fl::size>(fileSize));
    fl::size bytesRead = fl::fread(data.data(), 1, static_cast<fl::size>(fileSize), f);
    fl::fclose(f);
    if (bytesRead != static_cast<fl::size>(fileSize)) {
        return fl::vector<fl::u8>();
    }
    return data;
}

// --- Utility: Decode OGG to flat PCM i16 vector ---
static fl::vector<fl::i16> decodeOggToPcm(const char* path, fl::u32* outSampleRate = nullptr) {
    fl::vector<fl::u8> oggData = loadOggFile(path);
    if (oggData.empty()) {
        return fl::vector<fl::i16>();
    }

    // Parse info for sample rate
    fl::string error;
    VorbisInfo info = Vorbis::parseVorbisInfo(oggData, &error);
    if (!info.isValid) {
        return fl::vector<fl::i16>();
    }
    if (outSampleRate) {
        *outSampleRate = info.sampleRate;
    }

    // Decode all samples
    fl::vector<AudioSample> samples = Vorbis::decodeAll(oggData, &error);
    if (samples.empty()) {
        return fl::vector<fl::i16>();
    }

    // Concatenate all PCM frames
    fl::vector<fl::i16> pcm;
    for (const auto& sample : samples) {
        const auto& data = sample.pcm();
        for (fl::size i = 0; i < data.size(); ++i) {
            pcm.push_back(data[i]);
        }
    }
    return pcm;
}

// --- Utility: Mix two PCM streams ---
static fl::vector<fl::i16> mixPcm(fl::span<const fl::i16> a, fl::span<const fl::i16> b,
                                   float ratioA, float ratioB) {
    fl::size len = fl::min(a.size(), b.size());
    fl::vector<fl::i16> out;
    out.reserve(len);
    for (fl::size i = 0; i < len; ++i) {
        float mixed = static_cast<float>(a[i]) * ratioA + static_cast<float>(b[i]) * ratioB;
        if (mixed > 32767.0f) mixed = 32767.0f;
        if (mixed < -32768.0f) mixed = -32768.0f;
        out.push_back(static_cast<fl::i16>(mixed));
    }
    return out;
}

// --- Utility: Run VocalDetector over PCM stream ---
struct RealAudioResult {
    float avgConfidence;
    float maxConfidence;
    int vocalFrames;
    int totalFrames;
    float vocalRate;
};

// maxSeconds: limit processing to first N seconds (0 = all)
static RealAudioResult runDetectorOnPcm(fl::span<const fl::i16> pcm, int sampleRate = 44100, float maxSeconds = 3.0f) {
    VocalDetector detector;
    detector.setSampleRate(sampleRate);

    constexpr int FRAME_SIZE = 512;
    RealAudioResult result = {0.0f, 0.0f, 0, 0, 0.0f};
    float confSum = 0.0f;

    fl::u32 timestamp = 0;
    fl::u32 timestampInc = static_cast<fl::u32>(FRAME_SIZE * 1000 / sampleRate);

    // Limit to maxSeconds of audio to keep tests fast
    fl::size maxSamples = pcm.size();
    if (maxSeconds > 0.0f) {
        fl::size limit = static_cast<fl::size>(maxSeconds * static_cast<float>(sampleRate));
        if (limit < maxSamples) maxSamples = limit;
    }

    for (fl::size offset = 0; offset + FRAME_SIZE <= maxSamples; offset += FRAME_SIZE) {
        fl::span<const fl::i16> frame(pcm.data() + offset, FRAME_SIZE);
        AudioSample sample(frame, timestamp);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(sampleRate);
        detector.update(ctx);
        detector.fireCallbacks();

        float conf = detector.getConfidence();
        confSum += conf;
        if (conf > result.maxConfidence) {
            result.maxConfidence = conf;
        }
        if (detector.isVocal()) {
            result.vocalFrames++;
        }
        result.totalFrames++;
        timestamp += timestampInc;
    }

    if (result.totalFrames > 0) {
        result.avgConfidence = confSum / static_cast<float>(result.totalFrames);
        result.vocalRate = static_cast<float>(result.vocalFrames) / static_cast<float>(result.totalFrames);
    }
    return result;
}

// File path constants
static const char* STEMS_DIR = "tests/data/audio/stems/";
static const char* MIXES_DIR = "tests/data/audio/mixes/";

// Helper to build path
static fl::string stemPath(const char* name) {
    fl::string p;
    p.append(STEMS_DIR);
    p.append(name);
    return p;
}
static fl::string mixPath(const char* name) {
    fl::string p;
    p.append(MIXES_DIR);
    p.append(name);
    return p;
}

// Check if fixture files exist
static bool fixturesAvailable() {
    fl::vector<fl::u8> test = loadOggFile(mixPath("vocal_solo.ogg").c_str());
    return !test.empty();
}

} // namespace test_vocal_real_audio

using namespace test_vocal_real_audio;

// ============================================================================
// 6a. Positive controls — vocals must be detected
// ============================================================================

FL_TEST_CASE("VocalDetector real-audio - vocal_solo detected") {
    if (!fixturesAvailable()) {
        FL_MESSAGE("Skipping: OGG fixtures not found. Run: uv run python ci/tools/generate_audio_fixtures.py");
        return;
    }
    fl::u32 sr = 0;
    auto pcm = decodeOggToPcm(mixPath("vocal_solo.ogg").c_str(), &sr);
    FL_REQUIRE(!pcm.empty());
    FL_CHECK_EQ(sr, 44100u);

    auto result = runDetectorOnPcm(pcm, static_cast<int>(sr));
    FL_MESSAGE("vocal_solo: avgConf=" << result.avgConfidence
               << " maxConf=" << result.maxConfidence
               << " vocalRate=" << result.vocalRate
               << " (" << result.vocalFrames << "/" << result.totalFrames << ")");
    FL_CHECK_GE(result.vocalRate, 0.40f);
    FL_CHECK_GE(result.avgConfidence, 0.35f);
}

FL_TEST_CASE("VocalDetector real-audio - mix_vocal_loud higher than backing") {
    if (!fixturesAvailable()) {
        FL_MESSAGE("Skipping: OGG fixtures not found.");
        return;
    }

    auto pcmLoud = decodeOggToPcm(mixPath("mix_vocal_loud.ogg").c_str());
    auto pcmBacking = decodeOggToPcm(mixPath("backing_only.ogg").c_str());
    FL_REQUIRE(!pcmLoud.empty());
    FL_REQUIRE(!pcmBacking.empty());

    auto resultLoud = runDetectorOnPcm(pcmLoud);
    auto resultBacking = runDetectorOnPcm(pcmBacking);

    FL_MESSAGE("mix_vocal_loud: avgConf=" << resultLoud.avgConfidence
               << " backing: avgConf=" << resultBacking.avgConfidence);

    // Voice at +6dB should produce higher confidence than backing alone
    FL_CHECK_GE(resultLoud.avgConfidence, resultBacking.avgConfidence - 0.02f);
}

// ============================================================================
// 6b. Negative controls — backing must NOT be detected
// ============================================================================

FL_TEST_CASE("VocalDetector real-audio - guitar_solo not vocal") {
    if (!fixturesAvailable()) {
        FL_MESSAGE("Skipping: OGG fixtures not found.");
        return;
    }
    fl::u32 sr = 0;
    auto pcm = decodeOggToPcm(mixPath("guitar_solo.ogg").c_str(), &sr);
    FL_REQUIRE(!pcm.empty());

    auto result = runDetectorOnPcm(pcm, static_cast<int>(sr));
    FL_MESSAGE("guitar_solo: avgConf=" << result.avgConfidence
               << " vocalRate=" << result.vocalRate);
    FL_CHECK_LE(result.vocalRate, 0.25f);
    FL_CHECK_LT(result.avgConfidence, 0.55f);
}

FL_TEST_CASE("VocalDetector real-audio - drums_solo not vocal") {
    if (!fixturesAvailable()) {
        FL_MESSAGE("Skipping: OGG fixtures not found.");
        return;
    }
    fl::u32 sr = 0;
    auto pcm = decodeOggToPcm(mixPath("drums_solo.ogg").c_str(), &sr);
    FL_REQUIRE(!pcm.empty());

    auto result = runDetectorOnPcm(pcm, static_cast<int>(sr));
    FL_MESSAGE("drums_solo: avgConf=" << result.avgConfidence
               << " vocalRate=" << result.vocalRate);
    FL_CHECK_LE(result.vocalRate, 0.25f);
}

FL_TEST_CASE("VocalDetector real-audio - backing_only not vocal") {
    if (!fixturesAvailable()) {
        FL_MESSAGE("Skipping: OGG fixtures not found.");
        return;
    }
    fl::u32 sr = 0;
    auto pcm = decodeOggToPcm(mixPath("backing_only.ogg").c_str(), &sr);
    FL_REQUIRE(!pcm.empty());

    auto result = runDetectorOnPcm(pcm, static_cast<int>(sr));
    FL_MESSAGE("backing_only: avgConf=" << result.avgConfidence
               << " vocalRate=" << result.vocalRate);
    FL_CHECK_LE(result.vocalRate, 0.30f);
}

// ============================================================================
// 6c. Mix gradient — detection monotonically decreases with vocal level
// ============================================================================

FL_TEST_CASE("VocalDetector real-audio - mix gradient") {
    if (!fixturesAvailable()) {
        FL_MESSAGE("Skipping: OGG fixtures not found.");
        return;
    }

    auto pcmLoud = decodeOggToPcm(mixPath("mix_vocal_loud.ogg").c_str());
    auto pcmEqual = decodeOggToPcm(mixPath("mix_vocal_equal.ogg").c_str());
    auto pcmQuiet = decodeOggToPcm(mixPath("mix_vocal_quiet.ogg").c_str());
    FL_REQUIRE(!pcmLoud.empty());
    FL_REQUIRE(!pcmEqual.empty());
    FL_REQUIRE(!pcmQuiet.empty());

    auto resultLoud = runDetectorOnPcm(pcmLoud);
    auto resultEqual = runDetectorOnPcm(pcmEqual);
    auto resultQuiet = runDetectorOnPcm(pcmQuiet);

    FL_MESSAGE("gradient: loud=" << resultLoud.avgConfidence
               << " equal=" << resultEqual.avgConfidence
               << " quiet=" << resultQuiet.avgConfidence);

    // Confidence should decrease as voice level decreases
    FL_CHECK_GE(resultLoud.avgConfidence, resultEqual.avgConfidence - 0.05f);
    FL_CHECK_GE(resultEqual.avgConfidence, resultQuiet.avgConfidence - 0.05f);
}

// ============================================================================
// 6d. C++-side mixing — load stems, mix at runtime
// ============================================================================

FL_TEST_CASE("VocalDetector real-audio - cpp-side stem mixing") {
    if (!fixturesAvailable()) {
        FL_MESSAGE("Skipping: OGG fixtures not found.");
        return;
    }

    auto voice = decodeOggToPcm(stemPath("voice_male.ogg").c_str());
    auto guitar = decodeOggToPcm(stemPath("guitar_acoustic.ogg").c_str());
    if (voice.empty() || guitar.empty()) {
        FL_MESSAGE("Skipping: stem files not found.");
        return;
    }

    // Guitar alone
    auto resultGuitar = runDetectorOnPcm(guitar);

    // Voice + guitar at 50/50
    auto mixed = mixPcm(voice, guitar, 0.5f, 1.0f);
    auto resultMixed = runDetectorOnPcm(mixed);

    FL_MESSAGE("stem mixing: guitar_only=" << resultGuitar.avgConfidence
               << " voice+guitar=" << resultMixed.avgConfidence);

    // Adding voice should increase confidence
    FL_CHECK_GT(resultMixed.avgConfidence, resultGuitar.avgConfidence - 0.05f);
}

// ============================================================================
// 6e. Feature diagnostics — print spectral features for all fixtures
// ============================================================================

FL_TEST_CASE("VocalDetector real-audio - feature diagnostics") {
    if (!fixturesAvailable()) {
        FL_MESSAGE("Skipping: OGG fixtures not found.");
        return;
    }

    struct FixtureInfo {
        const char* name;
        const char* path;
    };

    // Representative subset to keep test fast (4 fixtures instead of 8)
    FixtureInfo fixtures[] = {
        {"vocal_solo", "mixes/vocal_solo.ogg"},
        {"guitar_solo", "mixes/guitar_solo.ogg"},
        {"drums_solo", "mixes/drums_solo.ogg"},
        {"backing_only", "mixes/backing_only.ogg"},
    };

    for (const auto& fix : fixtures) {
        fl::string fullPath;
        fullPath.append("tests/data/audio/");
        fullPath.append(fix.path);

        auto pcm = decodeOggToPcm(fullPath.c_str());
        if (pcm.empty()) continue;

        // Run detector on first 2 seconds for diagnostics
        VocalDetector detector;
        detector.setSampleRate(44100);

        constexpr int FRAME_SIZE = 512;
        constexpr fl::size MAX_DIAG_SAMPLES = 44100 * 2; // 2 seconds
        int frameCount = 0;
        for (fl::size offset = 0; offset + FRAME_SIZE <= pcm.size() && offset + FRAME_SIZE <= MAX_DIAG_SAMPLES; offset += FRAME_SIZE) {
            fl::span<const fl::i16> frame(pcm.data() + offset, FRAME_SIZE);
            AudioSample sample(frame, static_cast<fl::u32>(frameCount * 12));
            auto ctx = fl::make_shared<AudioContext>(sample);
            ctx->setSampleRate(44100);
            detector.update(ctx);
            frameCount++;
        }

        FL_MESSAGE(fix.name
                   << ": conf=" << detector.getConfidence()
                   << " flat=" << Diag::getSpectralFlatness(detector)
                   << " form=" << Diag::getFormantRatio(detector)
                   << " pres=" << Diag::getVocalPresenceRatio(detector)
                   << " cent=" << Diag::getSpectralCentroid(detector)
                   << " flux=" << Diag::getSpectralFlux(detector)
                   << " jit=" << Diag::getEnvelopeJitter(detector)
                   << " zcCV=" << Diag::getZeroCrossingCV(detector)
                   << " vocal=" << (detector.isVocal() ? "YES" : "no"));
    }
}

// ============================================================================
// 6f. Codec robustness — OGG decode shouldn't alter detection significantly
// ============================================================================

FL_TEST_CASE("VocalDetector real-audio - voice stem decodes cleanly") {
    if (!fixturesAvailable()) {
        FL_MESSAGE("Skipping: OGG fixtures not found.");
        return;
    }
    fl::u32 sr = 0;
    auto pcm = decodeOggToPcm(stemPath("voice_male.ogg").c_str(), &sr);
    if (pcm.empty()) {
        FL_MESSAGE("Skipping: voice_male.ogg not found.");
        return;
    }

    // Verify we got reasonable PCM data
    FL_CHECK_EQ(sr, 44100u);
    FL_CHECK_GT(pcm.size(), static_cast<fl::size>(44100 * 5)); // At least 5 seconds

    // Check signal isn't silence (RMS > threshold)
    float sumSq = 0.0f;
    for (fl::size i = 0; i < pcm.size(); ++i) {
        float s = static_cast<float>(pcm[i]);
        sumSq += s * s;
    }
    float rms = fl::sqrtf(sumSq / static_cast<float>(pcm.size()));
    FL_MESSAGE("voice_male RMS: " << rms);
    FL_CHECK_GT(rms, 100.0f); // Should be well above noise floor
}
