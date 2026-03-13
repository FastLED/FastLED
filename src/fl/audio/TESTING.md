# Audio Detector Testing Guide

This document describes how to test FastLED audio detectors end-to-end, from synthetic unit tests through real-world MP3 validation.

## Development Workflow: Tune, Then Delete

**The MP3 test files are development-time calibration tools, NOT permanent test fixtures.**

The workflow for tuning audio detectors is:

1. **Create real-world MP3 test fixtures** (see Tier 2 below) to calibrate scoring parameters against real audio
2. **Run both synthetic and MP3 tests** side-by-side while tuning parameters
3. **Transfer learned thresholds into synthetic tests** -- adjust synthetic signal generators and scoring parameters until synthetic tests reproduce the discrimination behavior observed on real audio
4. **Delete the MP3 files** once synthetic tests are stable and cover the same detection/false-positive boundaries
5. **Commit only the synthetic tests** -- the repository should never contain MP3 files for voice detection

**Why?** MP3 files are large (~300 KB each), make the repo bloated, and create brittle tests tied to specific recordings. Synthetic tests are deterministic, fast, tiny, and reproducible across all platforms. The real audio is a calibration instrument, not a regression test.

**If re-tuning is needed later**, re-create the MP3 fixtures from the documented sources (see "How the Test Audio Was Created" below), tune, then delete again.

## Overview

Audio detector testing has two tiers:

1. **Synthetic signals** -- deterministic, reproducible, fast. Validates spectral feature math and scoring logic.
2. **Real-world audio** -- MP3 files with known structure (e.g., guitar-only region, voice+guitar region). Used during development to calibrate parameters against real recordings. **Deleted before commit.**

These tiers have fundamentally different feature distributions in CQ (Constant-Q) space. Scoring parameters tuned for synthetic signals will not produce high detection rates on real audio, and vice versa. Keep expectations separate during calibration.

## Tier 1: Synthetic Signal Testing (Permanent)

### Signal Generators

Located in `tests/fl/audio/test_helpers.h`:

| Generator | Purpose | Key Parameters |
|-----------|---------|----------------|
| `makeSample()` | Pure sine wave | freq, timestamp, amplitude |
| `makeMultiHarmonic()` | Harmonic series with geometric decay | f0, numHarmonics, decay |
| `makeSyntheticVowel()` | Additive synthesis with Gaussian formant envelopes | f0, f1Freq, f2Freq |
| `makeWhiteNoise()` | Deterministic PRNG noise | timestamp, amplitude |
| `makeChirp()` | Linear frequency sweep | startFreq, endFreq |

### Test Patterns

Each test feeds 10 frames through the detector at 44100 Hz sample rate with 128-bin CQ FFT:

```cpp
VocalDetector detector;
detector.setSampleRate(44100);

for (int frame = 0; frame < 10; ++frame) {
    auto sample = makeSyntheticVowel(150.0f, 700.0f, 1200.0f, frame * 23, 16000.0f);
    auto ctx = fl::make_shared<AudioContext>(sample);
    ctx->setSampleRate(44100);
    ctx->getFFT(128);
    detector.update(ctx);
}

FL_CHECK(detector.isVocal());
FL_CHECK_GE(detector.getConfidence(), 0.6f);
```

### Expected Results (Synthetic)

| Signal | isVocal | Confidence |
|--------|---------|------------|
| Pure sine 440 Hz | false | <= 0.5 |
| Multi-harmonic (8 harmonics, 0.7 decay) | false | <= 0.5 |
| Two unrelated sines (440 + 1750 Hz) | false | <= 0.5 |
| White noise | false | <= 0.4 |
| Chirp (200-2000 Hz) | false | <= 0.5 |
| Guitar-like broadband | false | < 0.40 |
| Vowel /a/ (f0=150, F1=700, F2=1200) | true | >= 0.55 |
| Vowel /i/ (f0=200, F1=350, F2=2700) | -- | >= 0.55 |

The /i/ vowel is at the edge of what 128-bin CQ can resolve (F1=350 Hz, F2=2700 Hz produces weak formant peaks). The vowel /a/ threshold was relaxed from 0.60 to 0.55 during real-audio tuning because the broadened scoring parameters slightly reduce synthetic vowel confidence.

### Amplitude Sweep

Test that spectral features are amplitude-invariant by running the same signal at multiple levels (~-36 to ~0 dBFS). Confidence values for the same signal type should be within 0.15 of each other across levels.

## Tier 2: Real-World Audio Calibration (Development Only)

**These MP3 files are created during tuning and deleted before commit.**

### Step 1: Source Audio Samples

Use freely licensed recordings from archive.org or similar. Document the source in `tests/data/codec/ground_truth.txt` (also development-only).

Requirements:
- **Instrumental track** (e.g., acoustic guitar): should have clear solo section and no vocals
- **Vocal track**: speech or singing with natural gaps between phrases
- Both should be available as MP3 or WAV

Example sources used:
- Guitar: `archive.org/details/AcousticGuitarSound` (Rob Angelitis, CC BY-NC-SA 3.0)
- Voice: `archive.org/details/vocal-pearls_20220524` (track 30)

### Step 2: Establish Ground Truth

Analyze each raw source file with ffmpeg and SoX to record baseline characteristics.

**ffmpeg volume detection:**
```bash
ffmpeg -i source.mp3 -af volumedetect -f null /dev/null
```

**ffmpeg EBU R128 loudness:**
```bash
ffmpeg -i source.mp3 -af loudnorm=print_format=json -f null /dev/null
```

**ffmpeg silence detection:**
```bash
ffmpeg -i source.mp3 -af silencedetect=noise=-30dB:d=0.5 -f null /dev/null
```

**SoX stats** (requires WAV -- SoX on Windows often lacks MP3 support):
```bash
ffmpeg -i source.mp3 source.wav
static_sox source.wav -n stat
static_sox source.wav -n stats
```

Record peak dBFS, RMS dBFS, crest factor, silence regions, and duration in `ground_truth.txt`.

### Step 3: Trim, Normalize, and Mix

**Trim silence and normalize to -10 dBFS peak:**
```bash
# Trim to desired duration
static_sox input.wav trimmed.wav trim 0 20

# Normalize (gain to reach target peak)
static_sox trimmed.wav normalized.wav gain -n -10
```

**Mix voice into instrument track with delay:**
```bash
ffmpeg -i guitar_normalized.wav -i voice_normalized.wav \
  -filter_complex "[1]adelay=5000|5000[delayed];[0][delayed]amix=inputs=2:normalize=0" \
  -ac 1 mixed.wav
```

The `normalize=0` parameter is critical -- without it, `amix` divides amplitude by the number of inputs, halving the signal level.

### Step 4: Create Level Variants

Create mixes at different voice-to-instrument ratios:

```bash
# Voice at 0 dB relative to guitar (equal level)
ffmpeg -i guitar.wav -i voice.wav \
  -filter_complex "[1]adelay=5000|5000,volume=0dB[v];[0][v]amix=inputs=2:normalize=0" \
  -ac 1 mix_0dB.wav

# Voice at -10 dB relative to guitar
ffmpeg -i guitar.wav -i voice.wav \
  -filter_complex "[1]adelay=5000|5000,volume=-10dB[v];[0][v]amix=inputs=2:normalize=0" \
  -ac 1 mix_minus10dB.wav

# Voice at -20 dB (very quiet -- barely audible)
# Voice at +10 dB (voice dominates)
```

Encode to MP3 at 128 kbps for test fixtures (~300 KB per 20s file):
```bash
ffmpeg -i mix_0dB.wav -b:a 128k -ar 44100 -ac 1 voice_music_0dB.mp3
```

### Step 5: Write Temporary C++ Tests

Use the MP3 decoder to load test fixtures and run the detector frame-by-frame during calibration:

```cpp
fl::vector<fl::AudioSample> loadAndDecodeMp3(const char* path) {
    fl::setTestFileSystemRoot("tests/data");
    fl::FileSystem fs;
    if (!fs.beginSd(0)) return {};
    fl::FileHandlePtr file = fs.openRead(path);
    if (!file || !file->valid()) return {};
    fl::size file_size = file->size();
    fl::vector<fl::u8> mp3_data;
    mp3_data.resize(file_size);
    file->read(mp3_data.data(), file_size);
    file->close();
    fl::third_party::Mp3HelixDecoder decoder;
    if (!decoder.init()) return {};
    return decoder.decodeToAudioSamples(mp3_data.data(), mp3_data.size());
}
```

Divide the audio into time regions with known content and count detection rates per region:

```cpp
// For a 20s file with voice at t=5-15s:
//   0-4s:   guitar only (expect low false positive rate)
//   6-14s:  voice+guitar (measure detection rate)
//   16-20s: guitar tail (expect low false positive rate)
```

Use `i * 1152 / sampleRate` to convert MP3 frame index to seconds (MP3 frames are 1152 samples).

### Step 6: Set Appropriate Thresholds

**Do not require 100% detection.** Voice detection on mixed audio is inherently imperfect, especially at low voice levels. Use these guidelines during calibration:

| Voice Level | Detection Rate | False Positive Rate |
|-------------|---------------|-------------------|
| +10 dB (dominant) | >= 60% | <= 15% |
| 0 dB (equal) | >= 10% | <= 20% |
| -10 dB (quiet) | >= 5% | <= 20% |
| -20 dB (very quiet) | not asserted | <= 20% |

### Step 7: Transfer to Synthetic Tests and Clean Up

Once calibration is complete:
1. Ensure synthetic tests cover the same detection boundaries
2. Delete all MP3 files from `tests/data/codec/voice_music_*.mp3`
3. Delete `tests/data/codec/ground_truth.txt`
4. Delete any raw source files from `tmp/`
5. Remove MP3-based test cases from the C++ test file
6. Verify all remaining synthetic tests pass: `bash test vocal`

## Tier 3: Sample Rate Invariance Testing

### Purpose

Verify that audio detectors are not hardcoded to 44100 Hz. The CQ kernel adapts to different sample rates via `f/fs` in kernel generation, and the default frequency range (fmin=174.6 Hz, fmax=4698.3 Hz) is well within Nyquist for 22050 Hz (Nyquist = 11025 Hz). This tier confirms that detection rates remain equivalent across sample rates.

### Method

During calibration, downsample existing MP3 test fixtures from 44100 Hz to 22050 Hz using ffmpeg, then run the same detector on both versions and compare per-region statistics.

**Resampling command:**
```bash
ffmpeg -i tests/data/codec/voice_music_0dB.mp3 -ar 22050 -b:a 64k tests/data/codec/voice_music_0dB_22k.mp3
```

For the permanent test suite, use synthetic signals at different sample rates to verify sample-rate invariance without MP3 dependencies.

### Expected Results

- Detection and false positive rates should be within 30 percentage points of the 44100 Hz baseline
- MP3 frame size is 1152 samples regardless of sample rate, so frame-to-time conversion must use the actual sample rate: `frameIndex * 1152 / sampleRate`

Calibration results with the tuned detector:
- 44 kHz: detection=14%, FP=12%
- 22 kHz: detection=40%, FP=0%
- Rate difference: 25.5pp detection, 11.7pp FP (both within 30pp limit)

## The Synthetic-vs-Real Gap

CQ-space spectral features have very different distributions for synthetic vs real audio:

| Feature | Synthetic Vowel | Real Voice+Guitar |
|---------|----------------|-------------------|
| Centroid (normalized) | ~0.50 | ~0.22 |
| Rolloff | ~0.65 | ~0.25 |
| Formant ratio | 0.5 - 0.9 | ~0.15 |
| Flatness | ~0.44 | 0.45 - 0.55 |
| Harmonic density | 60 - 100 | 43 - 58 |

This gap exists because:
- Guitar occupies the same harmonic/formant frequency ranges as voice in CQ space
- Real audio has background noise, reverberation, and recording artifacts
- Additive synthesis vowels have idealized formant peaks that real voice does not

Widening scoring parameters to accept real audio feature ranges will cause false positives on synthetic non-vocal signals (e.g., multi-harmonic instrument sounds). Keep the two test tiers separate with independent expectations during calibration.

## Optimization: Temporal Spectral Variance

### The Problem

Per-frame spectral features alone cannot discriminate voice from guitar in a 0dB mix. Every single-frame feature we tried (centroid, rolloff, formant ratio, harmonic density, vocal presence ratio, spectral flux) had nearly identical distributions between guitar-only and voice+guitar regions:

| Feature | Guitar-only | Voice+Guitar | Separation |
|---------|------------|-------------|------------|
| Centroid | 0.214 | 0.214 | 0% |
| Rolloff | 0.225 | 0.236 | 5% |
| Formant | 0.162 | 0.139 | ~0% |
| Density | 51.6 | 48.3 | 6% |
| Spectral flux | 0.0605 | 0.0587 | 3% |

The only feature with any separation was **spectral flatness** (guitar=0.484, voice=0.458 -- a 5.4% difference). By centering a very sharp scoring peak at 0.46 with +/-0.05 width, this difference could be exploited for ~14% detection rate with ~18% false positive rate.

### The Temporal Variance Idea

Instead of looking at single frames, compare each frame's spectrum against a smoothed history. The insight: **vocal cords vibrate at resonant frequencies that modulate the spectrum frame-to-frame** (vibrato, formant transitions, consonant/vowel boundaries). Guitar harmonics are more steady-state -- the string vibrates at a fixed fundamental with slowly decaying overtones.

The algorithm:
1. Maintain a per-bin exponential moving average (EMA) of the CQ spectrum
2. Each frame, compute `|current_bin - smoothed_bin| / smoothed_bin` for each active bin
3. Average across all active bins -> **spectral variance** (mean relative deviation)
4. Higher variance = more spectral instability = more likely voice

### Implementation: `SpectralVariance` Filter

The temporal spectral variance accumulator was generalized into a reusable filter in `fl/filter.h`:

```cpp
// fl/filter.h -- Multi-channel EMA with per-bin relative deviation
SpectralVariance<float> sv(0.2f);     // alpha=0.2, ~5-frame window

void processFrame(fl::span<const float> fftBins) {
    float variance = sv.update(fftBins);  // mean per-bin relative change
    // variance > threshold -> spectral content is changing (voice, transients)
    // variance ~ 0 -> steady-state signal (sustained instrument, noise)
}
```

The filter lives in `fl/detail/filter/spectral_variance_impl.h` with the public API in `fl/filter.h` alongside all other filters. It accepts any `span<const T>` -- not limited to audio/FFT bins.

Parameters:
- **alpha** (default 0.2): EMA tracking speed. Lower = more history, detects slower changes
- **floor** (default 1e-4): Minimum value for active channel. Bins below floor are excluded

### Results on Real Audio

With temporal spectral variance applied as a confidence **boost** (not a base score), the guitar-only false positive rate dropped from 18% to 12% while maintaining 14% detection rate:

| Metric | Before Variance | After Variance |
|--------|----------------|---------------|
| Detection rate (voice, 6-14s) | 14% | 14% |
| False positive rate (guitar, 0-4s) | 18% | 12% |
| All synthetic tests | PASS (18/18) | PASS (20/20) |

The variance is applied as a multiplicative boost rather than an additive score because synthetic single-frame tests produce zero variance (each frame is identical). Making it multiplicative means:
- Synthetic tests: variance = 0 -> boost = 1.0 (no effect) -> existing scoring preserved
- Real audio: guitar variance ~0.74 -> boost ~1.0 (neutral); voice variance ~1.01 -> boost ~1.06 (small lift)

### Why a Boost, Not a Primary Feature

The temporal variance has correct **direction** (voice > guitar) but the magnitude is modest:
- Guitar: 0.744 +/- 1.098
- Voice+guitar: 1.009 +/- 2.110

The standard deviations are larger than the means, so frame-to-frame variance is noisy. Using it as a high-weight primary feature breaks synthetic tests and produces unstable detection. As a multiplicative boost, it consistently reduces false positives without disrupting the flatness-based primary discrimination.

### Feature Distribution in Real Audio (0dB Voice+Guitar Mix)

For reference, the complete per-region feature distributions from `voice_music_0dB.mp3`:

```
guitar-only (0-4s) (n=154):
  centroid=0.214+/-0.050  rolloff=0.225+/-0.097  formant=0.162+/-0.118
  flatness=0.484+/-0.143  density=51.6+/-17.3  presence=0.015+/-0.057
  flux=0.0605+/-0.0285  variance=0.7444+/-1.0976

voice+guitar (6-14s) (n=306):
  centroid=0.214+/-0.068  rolloff=0.236+/-0.139  formant=0.139+/-0.147
  flatness=0.458+/-0.110  density=48.3+/-19.5  presence=0.025+/-0.078
  flux=0.0587+/-0.0294  variance=1.0091+/-2.1098

tail (16+s) (n=156):
  centroid=0.237+/-0.045  rolloff=0.271+/-0.068  formant=0.139+/-0.128
  flatness=0.594+/-0.194  density=49.8+/-12.4  presence=0.005+/-0.012
  flux=0.0593+/-0.0378  variance=6.6077+/-48.6560
```

### Feature Distribution in Real Audio (3-Way Voice+Guitar+Drums Mix)

3-way calibration uses guitar + jazz drums (looped 4s `jazzy_percussion.mp3`) + voice.
Drums dramatically alter feature distributions: jitter, ACF irregularity, and zcCV are all
elevated even without voice. Key finding: **loud voice (+10dB) has LOWER detection than
quiet voice (-10dB)** because loud voice adds periodicity that reduces ACF irregularity.

```
3-way backing (guitar+drums, 0-4s) (n=77, stride-2):
  flatness=0.633  formant=0.665  presence=0.126
  jitter=0.598    acfIrr=0.673   zcCV=1.121
  variance=0.932  confidence=0.464

3-way voice+all +10dB (6-14s) (n=153, stride-2):
  flatness=0.576  formant=0.356  presence=0.026
  jitter=0.571    acfIrr=0.653   zcCV=1.321
  variance=0.844  confidence=0.525

3-way voice+all 0dB (6-14s) (n=153, stride-2):
  flatness=0.610  formant=0.375  presence=0.029
  jitter=0.587    acfIrr=0.756   zcCV=1.458
  variance=0.738  confidence=0.543

3-way voice+all -10dB (6-14s) (n=153, stride-2):
  flatness=0.632  formant=0.422  presence=0.036
  jitter=0.586    acfIrr=0.844   zcCV=1.584
  variance=0.722  confidence=0.575
```

Detection rates (3-way, stride-2 sampling):

| Variant | Detection | FP Rate | Target |
|---------|-----------|---------|--------|
| +10dB | 27.5% | 0% | >=25%, <=15% |
| 0dB | 25.5% | 0% | >=10%, <=20% |
| -10dB | 56.2% | 0% | >=5%, <=20% |
| -20dB | 35.3% | 0% | FP<=20% |

Key differences from 2-way (voice+guitar only):
- **Drums inflate time-domain features**: jitter 0.60+ (was 0.23-0.35), ACF irreg 0.67+ (was 0.40-0.82)
- **Presence ratio inverts**: drums add 2-4kHz energy → backing presence (0.126) > voice presence (0.026)
- **Formant ratio inverts**: voice F1 energy dilutes F2/F1 ratio → backing formant (0.665) > voice formant (0.356)
- **ACF boost threshold raised**: 0.60 → 0.75 (above drum baseline 0.69)
- **Jitter boost threshold raised**: 0.25 → 0.55 (drums push jitter to 0.60)
- **zcCV neutral zone widened**: penalty start 0.80 → 1.50 (voice region zcCV 1.3-1.6)

## How the Test Audio Was Created

This section documents the exact procedure for recreating MP3 calibration fixtures if re-tuning is needed in the future.

### 1. Source Audio

Open-source recordings from archive.org:

- **Guitar**: Acoustic guitar recording from `archive.org/details/AcousticGuitarSound` (Rob Angelitis, CC BY-NC-SA 3.0) -- track 03, 20+ seconds of clean fingerpicking
- **Voice**: Vocal recording from `archive.org/details/vocal-pearls_20220524` (track 30, "Love from Sweet Gina") -- singing with natural phrasing and gaps
- **Drums**: Jazz percussion (`tests/data/codec/jazzy_percussion.mp3`) -- 4-second complex jazz beat, looped to 20s for 3-way mixes

### 2. Trim and Normalize

Each source was trimmed to the desired duration and normalized to consistent peak levels using SoX:

```bash
# Trim guitar to 20 seconds
static_sox guitar_raw.wav guitar_trimmed.wav trim 0 20

# Trim voice to 10 seconds (will be overlaid on guitar)
static_sox voice_raw.wav voice_trimmed.wav trim 0 10

# Normalize both to -10 dBFS peak
static_sox guitar_trimmed.wav guitar_norm.wav gain -n -10
static_sox voice_trimmed.wav voice_norm.wav gain -n -10
```

### 3. Mix with Offset

The voice track was mixed into the guitar track with a 5-second delay using ffmpeg's `adelay` filter:

```
Time:  0s ---- 5s ---- 15s ---- 20s
Guitar: ████████████████████████████
Voice:        ██████████████
              ^ adelay=5s
```

```bash
ffmpeg -i guitar_norm.wav -i voice_norm.wav \
  -filter_complex "[1]adelay=5000|5000[delayed];[0][delayed]amix=inputs=2:normalize=0" \
  -ac 1 voice_music_0dB.wav
```

Key ffmpeg flags:
- `adelay=5000|5000`: Delay voice by 5 seconds (both channels, though output is mono)
- `amix=inputs=2:normalize=0`: Mix without amplitude normalization (preserves levels)
- `-ac 1`: Force mono output

### 3b. 3-Way Mix (Voice+Guitar+Drums)

For 3-way calibration, drums are looped and mixed with guitar as the backing track:

```bash
# Loop drums to 20s, trim, normalize
ffmpeg -stream_loop 4 -i drums_raw.mp3 -t 20 -ar 44100 -ac 1 drums_loop.wav
static_sox drums_loop.wav drums_norm.wav gain -n -10

# 3-way mix: guitar+drums backing, voice delayed 5s
ffmpeg -i guitar_norm.wav -i drums_norm.wav -i voice_norm.wav \
  -filter_complex "[0][1]amix=inputs=2:normalize=0[back];[2]adelay=5000|5000,volume=0dB[v];[back][v]amix=inputs=2:normalize=0" \
  -ac 1 mix_3way_0dB.wav
```

**Critical**: Both `amix` filters must use `normalize=0` to prevent amplitude halving.

### 4. Level Variants

Multiple mixes at different voice-to-guitar ratios:

```bash
# 0 dB (equal level -- hardest for detection)
ffmpeg -i guitar.wav -i voice.wav \
  -filter_complex "[1]adelay=5000|5000,volume=0dB[v];[0][v]amix=inputs=2:normalize=0" \
  -ac 1 mix_0dB.wav

# -10 dB (voice 10 dB quieter than guitar)
# -20 dB (voice barely audible)
# +10 dB (voice dominates)
```

### 5. Encode to MP3

Encoded at 128 kbps mono for compact test fixtures (~300 KB per 20s file):

```bash
ffmpeg -i mix_0dB.wav -b:a 128k -ar 44100 -ac 1 voice_music_0dB.mp3
```

A 22 kHz variant was created for sample rate invariance testing:
```bash
ffmpeg -i voice_music_0dB.mp3 -ar 22050 -b:a 64k voice_music_0dB_22k.mp3
```

### 6. Test Region Layout

The C++ test divides the audio by time to isolate regions with known content:

| Region | Time Range | Content | Expected |
|--------|-----------|---------|----------|
| Guitar-only | 0 - 4s | Guitar solo, no voice | Low FP rate (<= 20%) |
| Transition | 4 - 6s | Voice fading in | Not measured |
| Voice+guitar | 6 - 14s | Both playing | Detection rate >= 10% |
| Transition | 14 - 16s | Voice fading out | Not measured |
| Tail | 16 - 20s | Guitar solo, post-voice | Not measured (noisy) |

The 2-second transition gaps avoid measuring frames where voice is partially present.

## Example Directive: Improving an Audio Detector

Use this template when asking an AI agent to enhance a detector. Copy the structure, fill in specifics for your detector, and paste as a standalone improvement plan.

### Template

```markdown
# Improve <DetectorName> with <Feature1>, <Feature2>, and <SignalType> Test Signals

## Problem

The <DetectorName> uses <N> spectral features (<list>). This has blind spots:

1. **<False positive scenario>** — describe a signal that incorrectly scores high.
2. **<Missing discrimination>** — describe what structural property of the target
   signal is not being checked (e.g., harmonic structure, temporal modulation).
3. **Test suite only uses <signal type>**, so these weaknesses aren't validated.

## Changes

### 1. Add synthetic signal generators to test helpers

**File:** `tests/fl/audio/test_helpers.h`

For each new signal type, add an inline generator function in the
`fl::audio::test` namespace. Each generator must:
- Return `AudioSample`
- Accept a `u32 timestamp` parameter
- Use deterministic parameters (fixed seeds for PRNG) for reproducibility
- Stay within Nyquist limits

Example generators to consider:
| Generator | When to add |
|-----------|-------------|
| `makeMultiHarmonic(f0, N, decay)` | Testing harmonic-rich non-target signals |
| `makeSyntheticVowel(f0, f1, f2)` | Testing formant-based vocal detection |
| `makeWhiteNoise()` | Flat spectrum baseline |
| `makeChirp(startFreq, endFreq)` | Sweeping tones |
| `makePulse(freq, dutyCycle)` | Percussive/transient signals |
| `makeFMSynth(carrier, modulator, depth)` | Complex non-harmonic signals |

### 2. Add new spectral features

**File:** `src/fl/audio/detectors/<detector>.h` (declarations)
**File:** `src/fl/audio/detectors/<detector>.cpp.hpp` (implementation)

For each new feature:
- Add `float mFeatureName = 0.0f;` private member
- Add `float calculateFeatureName(const FFTBins& fft);` private method
- Add `float getFeatureName() const { return mFeatureName; }` public accessor
- Reset in `reset()`
- Compute in `update()` before `calculateRawConfidence()`

### 3. Update confidence scoring

Update `calculateRawConfidence()` with weighted combination. Guidelines:
- All weights must sum to 1.0
- Reduce existing feature weights proportionally when adding new ones
- Use piecewise scoring functions: ramp up / peak / decay down
- Document the expected range and scoring curve for each feature

### 4. Add test cases

**File:** `tests/fl/audio/detectors/<detector>.cpp`

Each test feeds 10 frames (lets EMA settle), then asserts on `isDetected()`
and `getConfidence()`. Include:
- **True positives**: signals the detector should catch
- **True negatives**: signals the detector should reject
- **Edge cases**: signals at the boundary of detection

### 5. Real-world calibration (Tier 2)

Follow the Tier 2 procedure in this document:
1. Source real audio samples (CC0/public domain)
2. Establish ground truth with ffmpeg/SoX analysis
3. Trim, normalize, mix at multiple levels
4. Run detector, measure detection rate and false positive rate
5. Transfer learned thresholds back to synthetic tests
6. Delete MP3 files before commit

## Implementation Order

1. Test helpers (no dependencies, enables TDD)
2. Header declarations (new members, method signatures, accessors)
3. Implementation (feature computation, scoring, wiring)
4. Synthetic tests
5. `bash test <detector>` — iterate until all tests pass
6. Real-world calibration (Tier 2) if synthetic tests alone are insufficient
7. `bash lint`

## Verification

bash test <detector>       # Detector-specific tests
bash test --cpp            # Full C++ test suite (no regressions)
bash lint                  # Code formatting
```

### Worked Example: VocalDetector Enhancement

This section is the complete directive that was used to improve the VocalDetector with spectral flatness, harmonic density, and expanded synthetic test signals. It serves as the canonical example of the template above.

#### Problem

The VocalDetector uses 3 spectral features (centroid, rolloff, formant ratio) averaged equally. This has blind spots:

1. **Single sine at the right frequency scores well on all three metrics.** A pure 800 Hz sine has centroid near mid-range, rolloff in the vocal range, and can produce a plausible F2/F1 ratio if it lands near the F1 band.
2. **No check for harmonic structure.** Human voice is rich in harmonics (F0 + overtones). A pure tone or white noise lacks this structure but isn't penalized.
3. **No check for spectral shape.** Voice has moderate spectral complexity (not pure tone, not flat noise). The current features don't distinguish these cases.
4. **Test suite only uses single-sine signals**, so these weaknesses aren't validated.

**Prerequisite:** FFT log-spacing fix (ISSUE_FFT_LOG.md) is complete. All bin-to-frequency conversions now use the CQ log-spaced formula. The `estimateFormantRatio()` function correctly maps Hz ranges to CQ bin indices.

#### Changes

##### 1. Synthetic signal generators added to test helpers

**File:** `tests/fl/audio/test_helpers.h`

Four functions added to the `fl::audio::test` namespace (with `#include "fl/stl/random.h"`):

**`makeMultiHarmonic`** — Fundamental + overtones with geometric amplitude decay. Tests harmonic-rich signals that aren't voice (e.g., instruments).

```cpp
inline AudioSample makeMultiHarmonic(float f0, int numHarmonics, float decay,
                                      u32 timestamp, float amplitude = 16000.0f,
                                      int count = 512, float sampleRate = 44100.0f);
```

**`makeSyntheticVowel`** — Additive synthesis with Gaussian formant envelopes on harmonics. Simplest physically-motivated voice model: harmonic series with 1/h rolloff shaped by two resonance peaks.

```cpp
inline AudioSample makeSyntheticVowel(float f0, float f1Freq, float f2Freq,
                                       u32 timestamp, float amplitude = 16000.0f,
                                       int count = 512, float sampleRate = 44100.0f);
```

Key parameters: `f1Bw = 150.0f` (F1 bandwidth), `f2Bw = 200.0f` (F2 bandwidth), `maxFreq = min(4000, sampleRate/2)`.

**`makeWhiteNoise`** — Deterministic noise using `fl::fl_random` with fixed seed (12345). Same output every run.

```cpp
inline AudioSample makeWhiteNoise(u32 timestamp, float amplitude = 16000.0f,
                                   int count = 512);
```

**`makeChirp`** — Linear frequency sweep with phase accumulation. Tests that a sweeping tone doesn't trigger vocal detection.

```cpp
inline AudioSample makeChirp(float startFreq, float endFreq, u32 timestamp,
                              float amplitude = 16000.0f, int count = 512,
                              float sampleRate = 44100.0f);
```

##### 2. Two new spectral features

**File:** `src/fl/audio/detectors/vocal.h` (declarations), `src/fl/audio/detectors/vocal.cpp.hpp` (implementation)

**Spectral Flatness** = geometric_mean / arithmetic_mean of bin magnitudes. Range [0, 1]. Pure tone → 0, white noise → 1, voice typically 0.2-0.5. Computed via log-domain trick using `bins_db`: `ln(bins_raw[i]) = bins_db[i] * 0.1151293f`, avoiding per-bin `logf()` calls.

**Harmonic Density** = count of bins with energy above 10% of peak magnitude. Voice has moderate density (several prominent harmonics). Pure tone has very few (1-2). Noise has many. Returns raw count; scoring function handles the mapping.

##### 3. Improved formant detection

Added local-maxima verification to `estimateFormantRatio()`: F1 and F2 peaks must be >= 1.5x the average energy in their respective search ranges. Prevents white noise (uniform spectrum) from scoring well on formant ratio.

##### 4. Five-feature weighted confidence scoring

| Feature | Weight | Scoring |
|---------|--------|---------|
| Centroid | 0.15 | Peak at 0.5 normalized (unchanged) |
| Rolloff | 0.15 | Peak at 0.65 (unchanged) |
| Formant Ratio | 0.25 | Peak at 1.4 (unchanged formula) |
| Spectral Flatness | 0.25 | Piecewise: <0.1 ramp to 0.3; 0.1-0.6 peak at 0.35; >0.6 decay to 0 |
| Harmonic Density | 0.20 | Piecewise: <4 ramp to 0.3; 4-16 ramp to 1.0; >16 decay to 0 |

Weights sum to 1.0.

##### 5. Seven new test cases

**File:** `tests/fl/audio/detectors/vocal.cpp`

| Test Name | Signal | Expected |
|-----------|--------|----------|
| `single sine 440Hz is not vocal` | `makeSample(440)` | NOT vocal, confidence <= 0.5 |
| `multi-harmonic is not vocal` | `makeMultiHarmonic(220, 8, 0.7)` | NOT vocal, confidence <= 0.5 |
| `vowel ah is vocal` | `makeSyntheticVowel(150, 700, 1200)` | IS vocal, confidence >= 0.6 |
| `vowel ee is vocal` | `makeSyntheticVowel(200, 350, 2700)` | IS vocal, confidence >= 0.55 |
| `white noise is not vocal` | `makeWhiteNoise()` | NOT vocal, confidence <= 0.4 |
| `chirp is not vocal` | `makeChirp(200, 2000)` | NOT vocal, confidence <= 0.5 |
| `two unrelated sines not vocal` | mix of 440 Hz + 1750 Hz | NOT vocal, confidence <= 0.5 |

**Note on "ee" vowel:** Uses F1=350 Hz (instead of textbook 300 Hz) because 300 Hz may fall below the F1 detection range at 128-bin CQ resolution. The /i/ vowel is at the edge of what 128-bin CQ can resolve (F1=350 Hz, F2=2700 Hz produces weak formant peaks). Threshold relaxed from 0.60 to 0.55 during real-audio tuning.

##### 6. Real-world MP3 calibration (Tier 2)

Full ground truth analysis procedure with ffmpeg and SoX, including:
- Volume detection, EBU R128 loudness, silence detection, per-frame spectral stats
- SoX stat/stats/spectrogram analysis
- Ground truth report compilation in `tests/data/codec/ground_truth.txt`
- Feature validation: FastLED detector output must be within 20% of ffmpeg `aspectralstats` ground truth

Level variants created:

| Variant | Voice Relative to Guitar | Detection Rate | False Positive Rate |
|---------|--------------------------|---------------|-------------------|
| +10 dB (dominant) | >= 60% | <= 15% |
| 0 dB (equal) | >= 10% | <= 20% |
| -10 dB (quiet) | >= 5% | <= 20% |
| -20 dB (very quiet) | not asserted | <= 20% |

##### 7. Results and lessons learned

- Detection rate (voice, 6-14s): 14%
- False positive rate (guitar, 0-4s): 12% (down from 18% after adding temporal spectral variance)
- All synthetic tests: PASS (20/20)
- 44 kHz detection=14%, FP=12%; 22 kHz detection=40%, FP=0% (within 30pp tolerance)

Key lessons (see "The Synthetic-vs-Real Gap" section above for details):
- Synthetic and real audio have very different feature distributions in CQ space
- Widening scoring parameters for real audio causes synthetic false positives
- Temporal features (cross-frame variance) provide discrimination that single-frame features cannot
- A multiplicative boost preserves synthetic test behavior while improving real-audio discrimination

#### Risk Assessment

| Risk | Likelihood | Mitigation |
|------|-----------|------------|
| "ee" vowel fails (F1 near edge of range) | Medium | Use F1=350 Hz; widen F1 range if needed |
| Existing tests break with new weights | Low | Old features keep same formulas, just lower total weight (0.33 -> 0.15-0.25) |
| Spectral flatness constants need tuning | Medium | 10 frames of settling + iterating on thresholds |
| Harmonic density bin count thresholds (4-16) need tuning for CQ bins | Medium | CQ log-spacing concentrates bins at lower frequencies; may need to adjust to 3-12 |
| Guitar false positives in MP3 test | High | Guitar has harmonic structure — formant ratio + spectral flatness must differentiate |
| -10 dB voice detection too low | Medium | At -10 dB relative, voice features are masked by guitar. Relax threshold or accept lower detection rate |
| MP3 compression artifacts affect features | Low | 128 kbps MP3 preserves spectral features below 16 kHz |

## Future Work

Additional temporal features that could further improve real-world detection:

- **Pitch tracking** -- voice has characteristic vibrato and pitch contour patterns
- **Onset detection** -- vocal onsets differ from plucked/struck instrument onsets
- **Modulation analysis** -- voice has amplitude and frequency modulation patterns distinct from instruments
- **Multi-frame variance windowing** -- use a sliding window of variance values to detect sustained vocal activity vs transient spectral changes

## Running Tests

```bash
# Run all vocal detector tests
bash test vocal

# Run with verbose output to see feature distributions
bash test vocal --verbose

# Run full test suite
bash test
```

All synthetic tests are in `tests/fl/audio/detectors/vocal.cpp`.
