#include "fl/audio_reactive.h"
#include "fl/math.h"
#include "fl/slice.h"
#include <math.h>

namespace fl {

// Global instance
AudioReactive Audio;

AudioReactive::AudioReactive() 
    : mFFTBins(16)  // Initialize with 16 frequency bins
{
    // No internal buffers needed
}

AudioReactive::~AudioReactive() = default;

void AudioReactive::begin(const AudioConfig& config) {
    setConfig(config);
    
    // Reset state
    mCurrentData = AudioData{};
    mSmoothedData = AudioData{};
    mLastProcessTime = 0;
    mLastBeatTime = 0;
    mPreviousVolume = 0.0f;
    mAGCMultiplier = 1.0f;
    mMaxSample = 0.0f;
    mAverageLevel = 0.0f;
}

void AudioReactive::setConfig(const AudioConfig& config) {
    mConfig = config;
}

void AudioReactive::processSample(AudioSample& sample, uint32_t currentTimeMs) {
    if (!sample.isValid()) {
        return; // Invalid sample, ignore
    }
    
    // Only process if enough time has passed
    if (currentTimeMs - mLastProcessTime >= PROCESS_INTERVAL) {
        // Process the AudioSample directly
        processFFT(sample);
        updateVolumeAndPeak(sample);
        detectBeat(currentTimeMs);
        applyGain();
        applyScaling();
        smoothResults();
        
        mCurrentData.timestamp = currentTimeMs;
        mLastProcessTime = currentTimeMs;
    }
}

void AudioReactive::update(uint32_t currentTimeMs) {
    // This method now only handles timing-based updates without new sample data
    // The main processing happens in processSample()
    
    // Just apply smoothing if enough time has passed
    if (currentTimeMs - mLastProcessTime >= PROCESS_INTERVAL) {
        smoothResults();
        mCurrentData.timestamp = currentTimeMs;
        mLastProcessTime = currentTimeMs;
    }
}

void AudioReactive::processFFT(AudioSample& sample) {
    // Get PCM data from AudioSample
    const auto& pcmData = sample.pcm();
    if (pcmData.empty()) return;
    
    // Use AudioSample's built-in FFT capability
    sample.fft(&mFFTBins);
    
    // Map FFT bins to frequency channels using WLED-compatible mapping
    mapFFTBinsToFrequencyChannels();
}

void AudioReactive::mapFFTBinsToFrequencyChannels() {
    // Copy FFT results to frequency bins array
    for (int i = 0; i < 16; ++i) {
        if (i < static_cast<int>(mFFTBins.bins_raw.size())) {
            mCurrentData.frequencyBins[i] = mFFTBins.bins_raw[i];
        } else {
            mCurrentData.frequencyBins[i] = 0.0f;
        }
    }
    
    // Apply pink noise compensation (from WLED)
    for (int i = 0; i < 16; ++i) {
        mCurrentData.frequencyBins[i] *= PINK_NOISE_COMPENSATION[i];
    }
    
    // Find dominant frequency
    float maxMagnitude = 0.0f;
    int maxBin = 0;
    for (int i = 0; i < 16; ++i) {
        if (mCurrentData.frequencyBins[i] > maxMagnitude) {
            maxMagnitude = mCurrentData.frequencyBins[i];
            maxBin = i;
        }
    }
    
    // Convert bin index to approximate frequency
    // Rough approximation based on WLED frequency mapping
    const float binCenterFrequencies[16] = {
        64.5f,   // Bin 0: 43-86 Hz
        107.5f,  // Bin 1: 86-129 Hz  
        172.5f,  // Bin 2: 129-216 Hz
        258.5f,  // Bin 3: 216-301 Hz
        365.5f,  // Bin 4: 301-430 Hz
        495.0f,  // Bin 5: 430-560 Hz
        689.0f,  // Bin 6: 560-818 Hz
        969.0f,  // Bin 7: 818-1120 Hz
        1270.5f, // Bin 8: 1120-1421 Hz
        1658.0f, // Bin 9: 1421-1895 Hz
        2153.5f, // Bin 10: 1895-2412 Hz
        2713.5f, // Bin 11: 2412-3015 Hz
        3359.5f, // Bin 12: 3015-3704 Hz
        4091.5f, // Bin 13: 3704-4479 Hz
        5792.5f, // Bin 14: 4479-7106 Hz
        8182.5f  // Bin 15: 7106-9259 Hz
    };
    
    mCurrentData.dominantFrequency = binCenterFrequencies[maxBin];
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
    for (int16_t pcmSample : pcmData) {
        float absSample = (pcmSample < 0) ? -pcmSample : pcmSample;
        maxSample = (maxSample > absSample) ? maxSample : absSample;
    }
    
    // Scale to 0-255 range (approximately)
    mCurrentData.volumeRaw = rms / 128.0f;  // Rough scaling
    mCurrentData.volume = mCurrentData.volumeRaw;
    
    // Peak detection
    mCurrentData.peak = maxSample / 32768.0f * 255.0f;
    
    // Update AGC tracking
    if (mConfig.agcEnabled) {
        // Simple AGC - track maximum level
        if (maxSample > mMaxSample) {
            mMaxSample = maxSample;
        } else {
            mMaxSample *= 0.999f; // Slow decay
        }
        
        // Update AGC multiplier
        if (mMaxSample > 1000.0f) {
            float targetLevel = 16384.0f; // Half of full scale
            mAGCMultiplier = targetLevel / mMaxSample;
            mAGCMultiplier = (mAGCMultiplier < 0.1f) ? 0.1f : ((mAGCMultiplier > 10.0f) ? 10.0f : mAGCMultiplier);
        }
    }
}

void AudioReactive::detectBeat(uint32_t currentTimeMs) {
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
    
    // Update previous volume for next comparison
    mPreviousVolume = currentVolume * 0.8f + mPreviousVolume * 0.2f; // Smooth
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
    
    // Apply AGC if enabled
    if (mConfig.agcEnabled) {
        mCurrentData.volume *= mAGCMultiplier;
        mCurrentData.volumeRaw *= mAGCMultiplier;
        mCurrentData.peak *= mAGCMultiplier;
        
        for (int i = 0; i < 16; ++i) {
            mCurrentData.frequencyBins[i] *= mAGCMultiplier;
        }
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
    // Simple smoothing between current and smoothed data
    const float smoothingFactor = 0.8f;
    
    mSmoothedData.volume = mSmoothedData.volume * smoothingFactor + 
                          mCurrentData.volume * (1.0f - smoothingFactor);
    mSmoothedData.volumeRaw = mSmoothedData.volumeRaw * smoothingFactor + 
                             mCurrentData.volumeRaw * (1.0f - smoothingFactor);
    mSmoothedData.peak = mSmoothedData.peak * smoothingFactor + 
                        mCurrentData.peak * (1.0f - smoothingFactor);
    
    for (int i = 0; i < 16; ++i) {
        mSmoothedData.frequencyBins[i] = mSmoothedData.frequencyBins[i] * smoothingFactor + 
                                        mCurrentData.frequencyBins[i] * (1.0f - smoothingFactor);
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

uint8_t AudioReactive::volumeToScale255() const {
    float vol = (mCurrentData.volume < 0.0f) ? 0.0f : ((mCurrentData.volume > 255.0f) ? 255.0f : mCurrentData.volume);
    return static_cast<uint8_t>(vol);
}

CRGB AudioReactive::volumeToColor(const CRGBPalette16& /* palette */) const {
    uint8_t index = volumeToScale255();
    // Simplified color palette lookup 
    return CRGB(index, index, index);  // For now, return grayscale
}

uint8_t AudioReactive::frequencyToScale255(uint8_t binIndex) const {
    if (binIndex >= 16) return 0;
    
    float value = (mCurrentData.frequencyBins[binIndex] < 0.0f) ? 0.0f : 
                  ((mCurrentData.frequencyBins[binIndex] > 255.0f) ? 255.0f : mCurrentData.frequencyBins[binIndex]);
    return static_cast<uint8_t>(value);
}

// Helper methods
float AudioReactive::mapFrequencyBin(int fromBin, int toBin) {
    if (fromBin < 0 || toBin >= static_cast<int>(mFFTBins.size()) || fromBin > toBin) {
        return 0.0f;
    }
    
    float sum = 0.0f;
    for (int i = fromBin; i <= toBin; ++i) {
        if (i < static_cast<int>(mFFTBins.bins_raw.size())) {
            sum += mFFTBins.bins_raw[i];
        }
    }
    
    return sum / static_cast<float>(toBin - fromBin + 1);
}

float AudioReactive::computeRMS(const fl::vector<int16_t>& samples) {
    if (samples.empty()) return 0.0f;
    
    float sumSquares = 0.0f;
    for (const auto& sample : samples) {
        float f = static_cast<float>(sample);
        sumSquares += f * f;
    }
    
    return sqrtf(sumSquares / samples.size());
}

} // namespace fl
