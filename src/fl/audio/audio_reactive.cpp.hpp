#include "fl/audio/audio_reactive.h"
#include "fl/audio/audio_processor.h"
#include "fl/audio/detectors/musical_beat_detector.h"
#include "fl/audio/detectors/multiband_beat_detector.h"
#include "fl/audio/mic_response_data.h"
#include "fl/stl/math.h"
#include "fl/stl/span.h"
#include "fl/stl/int.h"
#include "fl/stl/unique_ptr.h"  // For unique_ptr

namespace fl {

AudioReactive::AudioReactive()
    : mConfig{}, mFFTBins(16)  // Initialize with 16 frequency bins
{
    // Initialize enhanced beat detection components
    mSpectralFluxDetector = fl::make_unique<SpectralFluxDetector>();
    mPerceptualWeighting = fl::make_unique<PerceptualWeighting>();

    // Initialize previous magnitudes array to zero
    for (fl::size i = 0; i < mPreviousMagnitudes.size(); ++i) {
        mPreviousMagnitudes[i] = 0.0f;
    }
}

AudioReactive::~AudioReactive() = default;

void AudioReactive::begin(const AudioReactiveConfig& config) {
    setConfig(config);

    // Reset state
    mCurrentData = AudioData{};
    mSmoothedData = AudioData{};
    mLastBeatTime = 0;
    mPreviousVolume = 0.0f;

    // Configure signal conditioning components
    SignalConditionerConfig scConfig;
    scConfig.enableDCRemoval = config.enableSignalConditioning;
    scConfig.enableSpikeFilter = config.enableSignalConditioning;
    scConfig.enableNoiseGate = config.noiseGate && config.enableSignalConditioning;
    mSignalConditioner.configure(scConfig);
    mSignalConditioner.reset();

    NoiseFloorTrackerConfig nfConfig;
    nfConfig.enabled = config.enableNoiseFloorTracking;
    mNoiseFloorTracker.configure(nfConfig);
    mNoiseFloorTracker.reset();

    // Configure frequency bin mapper (obligatory - fixes hardcoded sample rate)
    FrequencyBinMapperConfig fbmConfig;
    fbmConfig.mode = FrequencyBinMode::Bins16;
    fbmConfig.sampleRate = config.sampleRate;
    fbmConfig.useLogSpacing = config.enableLogBinSpacing;
    fbmConfig.minFrequency = 20.0f;
    fbmConfig.maxFrequency = static_cast<float>(config.sampleRate) / 2.0f;  // Nyquist
    // fftBinCount will be set when we know the FFT size (after first processSample)
    fbmConfig.fftBinCount = 256;  // Default, overridden when actual FFT size known
    mFrequencyBinMapper.configure(fbmConfig);

    // Compute pink noise compensation gains from bin centers.
    // CQ bins span linearly from fmin to fmax (default: 174.6-4698.3 Hz).
    {
        const float fmin = FFT_Args::DefaultMinFrequency();
        const float fmax = FFT_Args::DefaultMaxFrequency();
        float binCenters[16];
        for (int i = 0; i < 16; ++i) {
            float t = static_cast<float>(i) / 15.0f;
            binCenters[i] = fmin + (fmax - fmin) * t;
        }
        computePinkNoiseGains(binCenters, 16, mPinkNoiseGains);
        mPinkNoiseComputed = true;
    }

    // Reset enhanced beat detection components
    if (mSpectralFluxDetector) {
        mSpectralFluxDetector->reset();
        mSpectralFluxDetector->setThreshold(config.spectralFluxThreshold);
    }

    // Configure musical beat detection (Phase 3 middleware - lazy creation)
    if (config.enableMusicalBeatDetection) {
        if (!mMusicalBeatDetector) {
            mMusicalBeatDetector = fl::make_unique<MusicalBeatDetector>();
        }
        MusicalBeatDetectorConfig mbdConfig;
        mbdConfig.minBPM = config.musicalBeatMinBPM;
        mbdConfig.maxBPM = config.musicalBeatMaxBPM;
        mbdConfig.minBeatConfidence = config.musicalBeatConfidence;
        mbdConfig.sampleRate = config.sampleRate;
        mbdConfig.samplesPerFrame = 512;  // Typical FFT frame size
        mMusicalBeatDetector->configure(mbdConfig);
        mMusicalBeatDetector->reset();
    } else {
        mMusicalBeatDetector.reset();
    }

    if (config.enableMultiBandBeats) {
        if (!mMultiBandBeatDetector) {
            mMultiBandBeatDetector = fl::make_unique<MultiBandBeatDetector>();
        }
        MultiBandBeatDetectorConfig mbbdConfig;
        mbbdConfig.bassThreshold = config.bassThreshold;
        mbbdConfig.midThreshold = config.midThreshold;
        mbbdConfig.trebleThreshold = config.trebleThreshold;
        mMultiBandBeatDetector->configure(mbbdConfig);
        mMultiBandBeatDetector->reset();
    } else {
        mMultiBandBeatDetector.reset();
    }

    // Configure spectral equalizer (optional - lazy creation)
    if (config.enableSpectralEqualizer) {
        if (!mSpectralEqualizer) {
            mSpectralEqualizer = fl::make_unique<SpectralEqualizer>();
        }
        SpectralEqualizerConfig seConfig;
        seConfig.curve = EqualizationCurve::AWeighting;
        seConfig.numBands = 16;
        mSpectralEqualizer->configure(seConfig);
    } else {
        mSpectralEqualizer.reset();
    }

    // Reset previous magnitudes
    for (fl::size i = 0; i < mPreviousMagnitudes.size(); ++i) {
        mPreviousMagnitudes[i] = 0.0f;
    }

    // Reset internal AudioProcessor if it exists
    if (mAudioProcessor) {
        mAudioProcessor->setSampleRate(config.sampleRate);
        mAudioProcessor->reset();
    }
}

void AudioReactive::setConfig(const AudioReactiveConfig& config) {
    mConfig = config;
}

void AudioReactive::processSample(const AudioSample& sample) {
    if (!sample.isValid()) {
        return; // Invalid sample, ignore
    }

    // Extract timestamp from the AudioSample
    fl::u32 currentTimeMs = sample.timestamp();

    // Phase 1: Signal conditioning pipeline
    AudioSample processedSample = sample;

    // Step 1: Signal conditioning (DC removal, spike filtering, noise gate)
    if (mConfig.enableSignalConditioning) {
        processedSample = mSignalConditioner.processSample(processedSample);
        if (!processedSample.isValid()) {
            return; // Signal was completely filtered out
        }
    }

    // Step 2: Noise floor tracking (update tracker, but don't modify signal)
    if (mConfig.enableNoiseFloorTracking) {
        float rms = processedSample.rms();
        mNoiseFloorTracker.update(rms);
    }

    // Process the conditioned AudioSample - timing is gated by sample availability
    processFFT(processedSample);
    updateVolumeAndPeak(processedSample);

    // Enhanced processing pipeline
    applySpectralEqualization();
    calculateBandEnergies();

    // Apply pink noise compensation AFTER band energy calculation
    // so that bassEnergy/midEnergy/trebleEnergy reflect actual spectral content
    if (mPinkNoiseComputed) {
        for (int i = 0; i < 16; ++i) {
            mCurrentData.frequencyBins[i] *= mPinkNoiseGains[i];
        }
    }

    updateSpectralFlux();

    // Enhanced beat detection (includes original)
    detectBeat(currentTimeMs);
    detectEnhancedBeats(currentTimeMs);

    // Apply perceptual weighting if enabled
    applyPerceptualWeighting();

    applyGain();
    applyScaling();
    smoothResults();

    mCurrentData.timestamp = currentTimeMs;

    // Forward to internal AudioProcessor for detector-based polling getters
    if (mAudioProcessor) {
        mAudioProcessor->update(sample);
    }
}

void AudioReactive::update(fl::u32 currentTimeMs) {
    // This method handles updates without new sample data
    // Just apply smoothing and update timestamp
    smoothResults();
    mCurrentData.timestamp = currentTimeMs;
}

void AudioReactive::processFFT(const AudioSample& sample) {
    // Get PCM data from AudioSample
    const auto& pcmData = sample.pcm();
    if (pcmData.empty()) return;
    
    // Use AudioSample's built-in FFT capability
    sample.fft(&mFFTBins);
    
    // Map FFT bins to frequency channels using WLED-compatible mapping
    mapFFTBinsToFrequencyChannels();
}

void AudioReactive::mapFFTBinsToFrequencyChannels() {
    // AudioSample::fft() returns CQ-kernel bins that are already
    // frequency-mapped (linearly spaced from fmin to fmax). Copy them
    // directly instead of re-mapping through FrequencyBinMapper, which
    // incorrectly treats CQ bins as raw DFT bins.
    fl::span<const float> rawBins = mFFTBins.raw();
    if (rawBins.empty()) {
        for (int i = 0; i < 16; ++i) {
            mCurrentData.frequencyBins[i] = 0.0f;
        }
        return;
    }

    // Copy CQ bins directly to frequency bins (already frequency-mapped)
    for (int i = 0; i < 16; ++i) {
        if (i < static_cast<int>(rawBins.size())) {
            mCurrentData.frequencyBins[i] = rawBins[i];
        } else {
            mCurrentData.frequencyBins[i] = 0.0f;
        }
    }

    // Note: Pink noise compensation is applied later in processSample(),
    // AFTER band energies are calculated from the raw CQ bins.
    // This ensures bassEnergy/midEnergy/trebleEnergy reflect actual
    // spectral content, not display-oriented compensation.

    // Find dominant frequency bin
    float maxMagnitude = 0.0f;
    int maxBin = 0;
    for (int i = 0; i < 16; ++i) {
        if (mCurrentData.frequencyBins[i] > maxMagnitude) {
            maxMagnitude = mCurrentData.frequencyBins[i];
            maxBin = i;
        }
    }

    // CQ bins span linearly from fmin to fmax (default: 174.6-4698.3 Hz)
    const float fmin = FFT_Args::DefaultMinFrequency();
    const float fmax = FFT_Args::DefaultMaxFrequency();
    const float deltaF = (fmax - fmin) / 16.0f;
    float dominantFreqStart = fmin + static_cast<float>(maxBin) * deltaF;
    mCurrentData.dominantFrequency = dominantFreqStart + deltaF * 0.5f;
    mCurrentData.magnitude = maxMagnitude;
}

void AudioReactive::updateVolumeAndPeak(const AudioSample& sample) {
    // Get PCM data from AudioSample
    const auto& pcmData = sample.pcm();
    if (pcmData.empty()) {
        mCurrentData.volume = 0.0f;
        mCurrentData.volumeRaw = 0.0f;
        mCurrentData.peak = 0.0f;
        return;
    }
    
    // Use AudioSample's built-in RMS calculation
    float rms = sample.rms();
    
    // Calculate peak from PCM data
    float maxSample = 0.0f;
    for (fl::i16 pcmSample : pcmData) {
        float absSample = (pcmSample < 0) ? -pcmSample : pcmSample;
        maxSample = (maxSample > absSample) ? maxSample : absSample;
    }
    
    // Scale to 0-255 range (approximately)
    mCurrentData.volumeRaw = rms / 128.0f;  // Rough scaling
    mCurrentData.volume = mCurrentData.volumeRaw;
    
    // Peak detection
    mCurrentData.peak = maxSample / 32768.0f * 255.0f;
}

void AudioReactive::detectBeat(fl::u32 currentTimeMs) {
    // Need minimum time since last beat
    if (currentTimeMs - mLastBeatTime < BEAT_COOLDOWN) {
        mCurrentData.beatDetected = false;
        return;
    }
    
    // Simple beat detection based on volume increase
    float currentVolume = mCurrentData.volume;
    
    // Beat detected if volume significantly increased
    if (currentVolume > mPreviousVolume + mVolumeThreshold && 
        currentVolume > 5.0f) {  // Minimum volume threshold
        mCurrentData.beatDetected = true;
        mLastBeatTime = currentTimeMs;
    } else {
        mCurrentData.beatDetected = false;
    }
    
    // Update previous volume for next comparison using attack/decay
    float beatAttackRate = mConfig.attack / 255.0f * 0.5f + 0.1f;   // 0.1 to 0.6
    float beatDecayRate = mConfig.decay / 255.0f * 0.3f + 0.05f;    // 0.05 to 0.35
    
    if (currentVolume > mPreviousVolume) {
        // Rising volume - use attack rate (faster tracking)
        mPreviousVolume = mPreviousVolume * (1.0f - beatAttackRate) + currentVolume * beatAttackRate;
    } else {
        // Falling volume - use decay rate (slower tracking)
        mPreviousVolume = mPreviousVolume * (1.0f - beatDecayRate) + currentVolume * beatDecayRate;
    }
}

void AudioReactive::applyGain() {
    // Apply gain setting (0-255 maps to 0.0-2.0 multiplier)
    float gainMultiplier = static_cast<float>(mConfig.gain) / 128.0f;

    mCurrentData.volume *= gainMultiplier;
    mCurrentData.volumeRaw *= gainMultiplier;
    mCurrentData.peak *= gainMultiplier;

    for (int i = 0; i < 16; ++i) {
        mCurrentData.frequencyBins[i] *= gainMultiplier;
    }
}

void AudioReactive::applyScaling() {
    // Apply scaling mode to frequency bins
    for (int i = 0; i < 16; ++i) {
        float value = mCurrentData.frequencyBins[i];
        
        switch (mConfig.scalingMode) {
            case 1: // Logarithmic scaling
                if (value > 1.0f) {
                    value = logf(value) * 20.0f; // Scale factor
                } else {
                    value = 0.0f;
                }
                break;
                
            case 2: // Linear scaling (no change)
                // value remains as-is
                break;
                
            case 3: // Square root scaling
                if (value > 0.0f) {
                    value = sqrtf(value) * 8.0f; // Scale factor
                } else {
                    value = 0.0f;
                }
                break;
                
            case 0: // No scaling
            default:
                // value remains as-is
                break;
        }
        
        mCurrentData.frequencyBins[i] = value;
    }
}

void AudioReactive::smoothResults() {
    // Attack/decay smoothing - different rates for rising vs falling values
    // Convert attack/decay times to smoothing factors
    // Shorter times = less smoothing (faster response)
    float attackFactor = 1.0f - (mConfig.attack / 255.0f * 0.9f);  // Range: 0.1 to 1.0
    float decayFactor = 1.0f - (mConfig.decay / 255.0f * 0.95f);   // Range: 0.05 to 1.0
    
    // Apply attack/decay smoothing to volume
    if (mCurrentData.volume > mSmoothedData.volume) {
        // Rising - use attack time (faster response)
        mSmoothedData.volume = mSmoothedData.volume * (1.0f - attackFactor) + 
                              mCurrentData.volume * attackFactor;
    } else {
        // Falling - use decay time (slower response)
        mSmoothedData.volume = mSmoothedData.volume * (1.0f - decayFactor) + 
                              mCurrentData.volume * decayFactor;
    }
    
    // Apply attack/decay smoothing to volumeRaw
    if (mCurrentData.volumeRaw > mSmoothedData.volumeRaw) {
        mSmoothedData.volumeRaw = mSmoothedData.volumeRaw * (1.0f - attackFactor) + 
                                 mCurrentData.volumeRaw * attackFactor;
    } else {
        mSmoothedData.volumeRaw = mSmoothedData.volumeRaw * (1.0f - decayFactor) + 
                                 mCurrentData.volumeRaw * decayFactor;
    }
    
    // Apply attack/decay smoothing to peak
    if (mCurrentData.peak > mSmoothedData.peak) {
        mSmoothedData.peak = mSmoothedData.peak * (1.0f - attackFactor) + 
                            mCurrentData.peak * attackFactor;
    } else {
        mSmoothedData.peak = mSmoothedData.peak * (1.0f - decayFactor) + 
                            mCurrentData.peak * decayFactor;
    }
    
    // Apply attack/decay smoothing to frequency bins
    for (int i = 0; i < 16; ++i) {
        if (mCurrentData.frequencyBins[i] > mSmoothedData.frequencyBins[i]) {
            // Rising - use attack time
            mSmoothedData.frequencyBins[i] = mSmoothedData.frequencyBins[i] * (1.0f - attackFactor) + 
                                            mCurrentData.frequencyBins[i] * attackFactor;
        } else {
            // Falling - use decay time  
            mSmoothedData.frequencyBins[i] = mSmoothedData.frequencyBins[i] * (1.0f - decayFactor) + 
                                            mCurrentData.frequencyBins[i] * decayFactor;
        }
    }
    
    // Copy non-smoothed values
    mSmoothedData.beatDetected = mCurrentData.beatDetected;
    mSmoothedData.dominantFrequency = mCurrentData.dominantFrequency;
    mSmoothedData.magnitude = mCurrentData.magnitude;
    mSmoothedData.timestamp = mCurrentData.timestamp;
}

const AudioData& AudioReactive::getData() const {
    return mCurrentData;
}

const AudioData& AudioReactive::getSmoothedData() const {
    return mSmoothedData;
}

float AudioReactive::getVolume() const {
    return mCurrentData.volume;
}

float AudioReactive::getBass() const {
    // Average of bins 0-1 (sub-bass and bass)
    return (mCurrentData.frequencyBins[0] + mCurrentData.frequencyBins[1]) / 2.0f;
}

float AudioReactive::getMid() const {
    // Average of bins 6-7 (midrange around 1kHz)
    return (mCurrentData.frequencyBins[6] + mCurrentData.frequencyBins[7]) / 2.0f;
}

float AudioReactive::getTreble() const {
    // Average of bins 14-15 (high frequencies)
    return (mCurrentData.frequencyBins[14] + mCurrentData.frequencyBins[15]) / 2.0f;
}

bool AudioReactive::isBeat() const {
    return mCurrentData.beatDetected;
}

bool AudioReactive::isBassBeat() const {
    return mCurrentData.bassBeatDetected;
}

bool AudioReactive::isMidBeat() const {
    return mCurrentData.midBeatDetected;
}

bool AudioReactive::isTrebleBeat() const {
    return mCurrentData.trebleBeatDetected;
}

float AudioReactive::getSpectralFlux() const {
    return mCurrentData.spectralFlux;
}

float AudioReactive::getBassEnergy() const {
    return mCurrentData.bassEnergy;
}

float AudioReactive::getMidEnergy() const {
    return mCurrentData.midEnergy;
}

float AudioReactive::getTrebleEnergy() const {
    return mCurrentData.trebleEnergy;
}

fl::u8 AudioReactive::volumeToScale255() const {
    float vol = (mCurrentData.volume < 0.0f) ? 0.0f : ((mCurrentData.volume > 255.0f) ? 255.0f : mCurrentData.volume);
    return static_cast<fl::u8>(vol);
}

CRGB AudioReactive::volumeToColor(const CRGBPalette16& /* palette */) const {
    fl::u8 index = volumeToScale255();
    // Simplified color palette lookup 
    return CRGB(index, index, index);  // For now, return grayscale
}

fl::u8 AudioReactive::frequencyToScale255(fl::u8 binIndex) const {
    if (binIndex >= 16) return 0;
    
    float value = (mCurrentData.frequencyBins[binIndex] < 0.0f) ? 0.0f : 
                  ((mCurrentData.frequencyBins[binIndex] > 255.0f) ? 255.0f : mCurrentData.frequencyBins[binIndex]);
    return static_cast<fl::u8>(value);
}

// Enhanced beat detection methods
void AudioReactive::calculateBandEnergies() {
    span<const float> bins(mCurrentData.frequencyBins, 16);
    mCurrentData.bassEnergy = mFrequencyBinMapper.getBassEnergy(bins);
    mCurrentData.midEnergy = mFrequencyBinMapper.getMidEnergy(bins);
    mCurrentData.trebleEnergy = mFrequencyBinMapper.getTrebleEnergy(bins);
}

void AudioReactive::applySpectralEqualization() {
    if (!mConfig.enableSpectralEqualizer) {
        return;
    }

    // Apply spectral EQ in-place on the frequency bins
    float equalizedBins[16];
    span<const float> inputSpan(mCurrentData.frequencyBins, 16);
    span<float> outputSpan(equalizedBins, 16);
    mSpectralEqualizer->apply(inputSpan, outputSpan);

    // Copy back
    for (int i = 0; i < 16; ++i) {
        mCurrentData.frequencyBins[i] = equalizedBins[i];
    }
}

void AudioReactive::updateSpectralFlux() {
    if (!mSpectralFluxDetector) {
        mCurrentData.spectralFlux = 0.0f;
        return;
    }
    
    // Calculate spectral flux from current and previous frequency bins
    mCurrentData.spectralFlux = mSpectralFluxDetector->calculateSpectralFlux(
        mCurrentData.frequencyBins, 
        mPreviousMagnitudes.data()
    );
    
    // Update previous magnitudes for next frame
    for (int i = 0; i < 16; ++i) {
        mPreviousMagnitudes[i] = mCurrentData.frequencyBins[i];
    }
}

void AudioReactive::detectEnhancedBeats(fl::u32 currentTimeMs) {
    // Reset beat flags
    mCurrentData.bassBeatDetected = false;
    mCurrentData.midBeatDetected = false;
    mCurrentData.trebleBeatDetected = false;

    // Skip if enhanced beat detection is disabled
    if (!mConfig.enableSpectralFlux && !mConfig.enableMultiBand &&
        !mConfig.enableMusicalBeatDetection && !mConfig.enableMultiBandBeats) {
        return;
    }

    // Need minimum time since last beat for enhanced detection too
    if (currentTimeMs - mLastBeatTime < BEAT_COOLDOWN) {
        return;
    }

    // Spectral flux-based onset detection (preliminary)
    bool onsetDetected = false;
    float onsetStrength = 0.0f;

    if (mConfig.enableSpectralFlux && mSpectralFluxDetector) {
        onsetDetected = mSpectralFluxDetector->detectOnset(
            mCurrentData.frequencyBins,
            mPreviousMagnitudes.data()
        );

        if (onsetDetected) {
            onsetStrength = mSpectralFluxDetector->calculateSpectralFlux(
                mCurrentData.frequencyBins,
                mPreviousMagnitudes.data()
            );
        }
    }

    // Phase 3: Musical beat detection - validates onsets as true musical beats
    if (mConfig.enableMusicalBeatDetection) {
        mMusicalBeatDetector->processSample(onsetDetected, onsetStrength);

        if (mMusicalBeatDetector->isBeat()) {
            // This is a validated musical beat, not just a random onset
            mCurrentData.beatDetected = true;
            mLastBeatTime = currentTimeMs;
        }
    } else if (onsetDetected) {
        // Fall back to simple onset detection if musical beat detection disabled
        mCurrentData.beatDetected = true;
        mLastBeatTime = currentTimeMs;
    }

    // Phase 3: Multi-band beat detection - per-frequency beat tracking
    if (mConfig.enableMultiBandBeats) {
        mMultiBandBeatDetector->detectBeats(mCurrentData.frequencyBins);

        mCurrentData.bassBeatDetected = mMultiBandBeatDetector->isBassBeat();
        mCurrentData.midBeatDetected = mMultiBandBeatDetector->isMidBeat();
        mCurrentData.trebleBeatDetected = mMultiBandBeatDetector->isTrebleBeat();
    } else if (mConfig.enableMultiBand) {
        // Fall back to simple threshold-based detection if multi-band disabled
        // Bass beat detection (bins 0-1)
        if (mCurrentData.bassEnergy > mConfig.bassThreshold) {
            mCurrentData.bassBeatDetected = true;
        }

        // Mid beat detection (bins 6-7)
        if (mCurrentData.midEnergy > mConfig.midThreshold) {
            mCurrentData.midBeatDetected = true;
        }

        // Treble beat detection (bins 14-15)
        if (mCurrentData.trebleEnergy > mConfig.trebleThreshold) {
            mCurrentData.trebleBeatDetected = true;
        }
    }
}

void AudioReactive::applyPerceptualWeighting() {
    // Apply perceptual weighting if available
    if (mPerceptualWeighting) {
        mPerceptualWeighting->applyAWeighting(mCurrentData);
        
        // Apply loudness compensation with reference level of 50.0f
        mPerceptualWeighting->applyLoudnessCompensation(mCurrentData, 50.0f);
    }
}

// Helper methods
float AudioReactive::mapFrequencyBin(int fromBin, int toBin) {
    if (fromBin < 0 || toBin >= static_cast<int>(mFFTBins.bands()) || fromBin > toBin) {
        return 0.0f;
    }
    
    float sum = 0.0f;
    for (int i = fromBin; i <= toBin; ++i) {
        if (i < static_cast<int>(mFFTBins.raw().size())) {
            sum += mFFTBins.raw()[i];
        }
    }
    
    return sum / static_cast<float>(toBin - fromBin + 1);
}

float AudioReactive::computeRMS(const fl::vector<fl::i16>& samples) {
    if (samples.empty()) return 0.0f;
    
    float sumSquares = 0.0f;
    for (const auto& sample : samples) {
        float f = static_cast<float>(sample);
        sumSquares += f * f;
    }
    
    return sqrtf(sumSquares / samples.size());
}

// SpectralFluxDetector implementation
SpectralFluxDetector::SpectralFluxDetector() 
    : mFluxThreshold(0.1f)
#if SKETCH_HAS_LOTS_OF_MEMORY
    , mHistoryIndex(0)
#endif
{
    // Initialize previous magnitudes to zero
    for (fl::size i = 0; i < mPreviousMagnitudes.size(); ++i) {
        mPreviousMagnitudes[i] = 0.0f;
    }
    
#if SKETCH_HAS_LOTS_OF_MEMORY
    // Initialize flux history to zero
    for (fl::size i = 0; i < mFluxHistory.size(); ++i) {
        mFluxHistory[i] = 0.0f;
    }
#endif
}

SpectralFluxDetector::~SpectralFluxDetector() = default;

void SpectralFluxDetector::reset() {
    for (fl::size i = 0; i < mPreviousMagnitudes.size(); ++i) {
        mPreviousMagnitudes[i] = 0.0f;
    }
    
#if SKETCH_HAS_LOTS_OF_MEMORY
    for (fl::size i = 0; i < mFluxHistory.size(); ++i) {
        mFluxHistory[i] = 0.0f;
    }
    mHistoryIndex = 0;
#endif
}

bool SpectralFluxDetector::detectOnset(const float* currentBins, const float* /* previousBins */) {
    float flux = calculateSpectralFlux(currentBins, mPreviousMagnitudes.data());
    
#if SKETCH_HAS_LOTS_OF_MEMORY
    // Store flux in history for adaptive threshold calculation
    mFluxHistory[mHistoryIndex] = flux;
    mHistoryIndex = (mHistoryIndex + 1) % mFluxHistory.size();
    
    float adaptiveThreshold = calculateAdaptiveThreshold();
    return flux > adaptiveThreshold;
#else
    // Simple fixed threshold for memory-constrained platforms
    return flux > mFluxThreshold;
#endif
}

float SpectralFluxDetector::calculateSpectralFlux(const float* currentBins, const float* previousBins) {
    float flux = 0.0f;
    
    // Calculate spectral flux as sum of positive differences
    for (int i = 0; i < 16; ++i) {
        float diff = currentBins[i] - previousBins[i];
        if (diff > 0.0f) {
            flux += diff;
        }
    }
    
    // Update previous magnitudes for next calculation
    for (int i = 0; i < 16; ++i) {
        mPreviousMagnitudes[i] = currentBins[i];
    }
    
    return flux;
}

void SpectralFluxDetector::setThreshold(float threshold) {
    mFluxThreshold = threshold;
}

float SpectralFluxDetector::getThreshold() const {
    return mFluxThreshold;
}

#if SKETCH_HAS_LOTS_OF_MEMORY
float SpectralFluxDetector::calculateAdaptiveThreshold() {
    // Calculate moving average of flux history
    float sum = 0.0f;
    for (fl::size i = 0; i < mFluxHistory.size(); ++i) {
        sum += mFluxHistory[i];
    }
    float average = sum / mFluxHistory.size();
    
    // Adaptive threshold is base threshold plus some multiple of recent average
    return mFluxThreshold + (average * 0.5f);
}
#endif

// BeatDetectors implementation  
BeatDetectors::BeatDetectors()
    : mBassEnergy(0.0f), mMidEnergy(0.0f), mTrebleEnergy(0.0f)
    , mPreviousBassEnergy(0.0f), mPreviousMidEnergy(0.0f), mPreviousTrebleEnergy(0.0f)
{
}

BeatDetectors::~BeatDetectors() = default;

void BeatDetectors::reset() {
#if SKETCH_HAS_LOTS_OF_MEMORY
    bass.reset();
    mid.reset(); 
    treble.reset();
#else
    combined.reset();
#endif
    
    mBassEnergy = 0.0f;
    mMidEnergy = 0.0f;
    mTrebleEnergy = 0.0f;
    mPreviousBassEnergy = 0.0f;
    mPreviousMidEnergy = 0.0f;
    mPreviousTrebleEnergy = 0.0f;
}

void BeatDetectors::detectBeats(const float* frequencyBins, AudioData& audioData) {
    // Calculate current band energies
    mBassEnergy = (frequencyBins[0] + frequencyBins[1]) / 2.0f;
    mMidEnergy = (frequencyBins[6] + frequencyBins[7]) / 2.0f;
    mTrebleEnergy = (frequencyBins[14] + frequencyBins[15]) / 2.0f;
    
#if SKETCH_HAS_LOTS_OF_MEMORY
    // Use separate detectors for each band
    audioData.bassBeatDetected = bass.detectOnset(&mBassEnergy, &mPreviousBassEnergy);
    audioData.midBeatDetected = mid.detectOnset(&mMidEnergy, &mPreviousMidEnergy);
    audioData.trebleBeatDetected = treble.detectOnset(&mTrebleEnergy, &mPreviousTrebleEnergy);
#else
    // Use simple threshold detection for memory-constrained platforms
    audioData.bassBeatDetected = (mBassEnergy > mPreviousBassEnergy * 1.3f) && (mBassEnergy > 0.1f);
    audioData.midBeatDetected = (mMidEnergy > mPreviousMidEnergy * 1.25f) && (mMidEnergy > 0.08f);
    audioData.trebleBeatDetected = (mTrebleEnergy > mPreviousTrebleEnergy * 1.2f) && (mTrebleEnergy > 0.05f);
#endif
    
    // Update previous energies
    mPreviousBassEnergy = mBassEnergy;
    mPreviousMidEnergy = mMidEnergy;
    mPreviousTrebleEnergy = mTrebleEnergy;
}

void BeatDetectors::setThresholds(float bassThresh, float midThresh, float trebleThresh) {
#if SKETCH_HAS_LOTS_OF_MEMORY
    bass.setThreshold(bassThresh);
    mid.setThreshold(midThresh);
    treble.setThreshold(trebleThresh);
#else
    combined.setThreshold((bassThresh + midThresh + trebleThresh) / 3.0f);
#endif
}

// PerceptualWeighting implementation
PerceptualWeighting::PerceptualWeighting()
#if SKETCH_HAS_LOTS_OF_MEMORY
    : mHistoryIndex(0)
#endif
{
#if SKETCH_HAS_LOTS_OF_MEMORY
    // Initialize loudness history to zero
    for (fl::size i = 0; i < mLoudnessHistory.size(); ++i) {
        mLoudnessHistory[i] = 0.0f;
    }
    // Suppress unused warning until mHistoryIndex is implemented
    (void)mHistoryIndex;
#endif
}

PerceptualWeighting::~PerceptualWeighting() = default;

void PerceptualWeighting::applyAWeighting(AudioData& data) const {
    // Apply A-weighting coefficients to frequency bins
    for (int i = 0; i < 16; ++i) {
        data.frequencyBins[i] *= A_WEIGHTING_COEFFS[i];
    }
}

void PerceptualWeighting::applyLoudnessCompensation(AudioData& data, float referenceLevel) const {
    // Calculate current loudness level
    float currentLoudness = data.volume;
    
    // Calculate compensation factor based on difference from reference
    float compensationFactor = 1.0f;
    if (currentLoudness < referenceLevel) {
        // Boost quiet signals
        compensationFactor = 1.0f + (referenceLevel - currentLoudness) / referenceLevel * 0.3f;
    } else if (currentLoudness > referenceLevel * 1.5f) {
        // Slightly reduce very loud signals  
        compensationFactor = 1.0f - (currentLoudness - referenceLevel * 1.5f) / (referenceLevel * 2.0f) * 0.2f;
    }
    
    // Apply compensation to frequency bins
    for (int i = 0; i < 16; ++i) {
        data.frequencyBins[i] *= compensationFactor;
    }
    
#if SKETCH_HAS_LOTS_OF_MEMORY
    // Store in history for future adaptive compensation (not implemented yet)
    // This would be used for more sophisticated dynamic range compensation
#endif
}

void AudioReactive::setGain(float gain) {
    ensureAudioProcessor().setGain(gain);
}

float AudioReactive::getGain() const {
    if (mAudioProcessor) {
        return mAudioProcessor->getGain();
    }
    return 1.0f;
}

// Signal conditioning stats accessors
const SignalConditioner::Stats& AudioReactive::getSignalConditionerStats() const {
    return mSignalConditioner.getStats();
}

const NoiseFloorTracker::Stats& AudioReactive::getNoiseFloorStats() const {
    return mNoiseFloorTracker.getStats();
}

bool AudioReactive::isSpectralEqualizerEnabled() const {
    return mConfig.enableSpectralEqualizer;
}

const SpectralEqualizer::Stats& AudioReactive::getSpectralEqualizerStats() const {
    return mSpectralEqualizer->getStats();
}

// ----- Polling Getter Forwarding (via internal AudioProcessor) -----

AudioProcessor& AudioReactive::ensureAudioProcessor() {
    if (!mAudioProcessor) {
        mAudioProcessor = fl::make_unique<AudioProcessor>();
        mAudioProcessor->setSampleRate(mConfig.sampleRate);
    }
    return *mAudioProcessor;
}

float AudioReactive::getVocalConfidence() { return ensureAudioProcessor().getVocalConfidence(); }
float AudioReactive::getBeatConfidence() { return ensureAudioProcessor().getBeatConfidence(); }
float AudioReactive::getBPM() { return ensureAudioProcessor().getBPM(); }
float AudioReactive::getEnergyLevel() { return ensureAudioProcessor().getEnergy(); }
float AudioReactive::getPeakLevel() { return ensureAudioProcessor().getPeakLevel(); }
float AudioReactive::getBassLevel() { return ensureAudioProcessor().getBassLevel(); }
float AudioReactive::getMidLevel() { return ensureAudioProcessor().getMidLevel(); }
float AudioReactive::getTrebleLevel() { return ensureAudioProcessor().getTrebleLevel(); }
bool AudioReactive::isSilent() { return ensureAudioProcessor().isSilent(); }
u32 AudioReactive::getSilenceDuration() { return ensureAudioProcessor().getSilenceDuration(); }
float AudioReactive::getTransientStrength() { return ensureAudioProcessor().getTransientStrength(); }
float AudioReactive::getDynamicTrend() { return ensureAudioProcessor().getDynamicTrend(); }
bool AudioReactive::isCrescendo() { return ensureAudioProcessor().isCrescendo(); }
bool AudioReactive::isDiminuendo() { return ensureAudioProcessor().isDiminuendo(); }
float AudioReactive::getPitchConfidence() { return ensureAudioProcessor().getPitchConfidence(); }
float AudioReactive::getPitchHz() { return ensureAudioProcessor().getPitch(); }
float AudioReactive::getTempoConfidence() { return ensureAudioProcessor().getTempoConfidence(); }
float AudioReactive::getTempoBPM() { return ensureAudioProcessor().getTempoBPM(); }
float AudioReactive::getBuildupIntensity() { return ensureAudioProcessor().getBuildupIntensity(); }
float AudioReactive::getBuildupProgress() { return ensureAudioProcessor().getBuildupProgress(); }
float AudioReactive::getDropImpact() { return ensureAudioProcessor().getDropImpact(); }
bool AudioReactive::isKick() { return ensureAudioProcessor().isKick(); }
bool AudioReactive::isSnare() { return ensureAudioProcessor().isSnare(); }
bool AudioReactive::isHiHat() { return ensureAudioProcessor().isHiHat(); }
bool AudioReactive::isTom() { return ensureAudioProcessor().isTom(); }
u8 AudioReactive::getCurrentNote() { return ensureAudioProcessor().getCurrentNote(); }
float AudioReactive::getNoteVelocity() { return ensureAudioProcessor().getNoteVelocity(); }
float AudioReactive::getNoteConfidence() { return ensureAudioProcessor().getNoteConfidence(); }
float AudioReactive::getDownbeatConfidence() { return ensureAudioProcessor().getDownbeatConfidence(); }
float AudioReactive::getMeasurePhase() { return ensureAudioProcessor().getMeasurePhase(); }
u8 AudioReactive::getCurrentBeatNumber() { return ensureAudioProcessor().getCurrentBeatNumber(); }
float AudioReactive::getBackbeatConfidence() { return ensureAudioProcessor().getBackbeatConfidence(); }
float AudioReactive::getBackbeatStrength() { return ensureAudioProcessor().getBackbeatStrength(); }
float AudioReactive::getChordConfidence() { return ensureAudioProcessor().getChordConfidence(); }
float AudioReactive::getKeyConfidence() { return ensureAudioProcessor().getKeyConfidence(); }
float AudioReactive::getMoodArousal() { return ensureAudioProcessor().getMoodArousal(); }
float AudioReactive::getMoodValence() { return ensureAudioProcessor().getMoodValence(); }

} // namespace fl
