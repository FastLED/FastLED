/// @file frequency_bin_mapper.cpp.hpp
/// @brief Implementation of FrequencyBinMapper for FFT bin to frequency channel mapping

#include "fl/fx/audio/frequency_bin_mapper.h"
#include "fl/stl/math.h"
#include "fl/math_macros.h"
#include "fl/compiler_control.h"

namespace fl {

FrequencyBinMapper::FrequencyBinMapper() {
    configure(FrequencyBinMapperConfig());
}

FrequencyBinMapper::FrequencyBinMapper(const FrequencyBinMapperConfig& config) {
    configure(config);
}

FrequencyBinMapper::~FrequencyBinMapper() = default;

void FrequencyBinMapper::configure(const FrequencyBinMapperConfig& config) {
    mConfig = config;

    // Reset stats
    mStats = Stats();

    // Calculate frequency boundaries for output bins
    calculateBinBoundaries();

    // Calculate FFT bin to frequency bin mappings
    calculateBinMappings();
}

void FrequencyBinMapper::calculateBinBoundaries() {
    const size numBins = static_cast<size>(mConfig.mode);

    // Allocate space for bin boundaries (numBins + 1 edges)
    mBinFrequencies.clear();
    mBinFrequencies.reserve(numBins + 1);

    if (mConfig.useLogSpacing) {
        calculateLogFrequencies();
    } else {
        calculateLinearFrequencies();
    }
}

void FrequencyBinMapper::calculateLogFrequencies() {
    const size numBins = static_cast<size>(mConfig.mode);
    const float logMin = fl::logf(mConfig.minFrequency);
    const float logMax = fl::logf(mConfig.maxFrequency);
    const float logStep = (logMax - logMin) / static_cast<float>(numBins);

    // Calculate logarithmically-spaced bin edges
    for (size i = 0; i <= numBins; ++i) {
        float logFreq = logMin + static_cast<float>(i) * logStep;
        float freq = fl::expf(logFreq);
        mBinFrequencies.push_back(freq);
    }
}

void FrequencyBinMapper::calculateLinearFrequencies() {
    const size numBins = static_cast<size>(mConfig.mode);
    const float step = (mConfig.maxFrequency - mConfig.minFrequency) / static_cast<float>(numBins);

    // Calculate linearly-spaced bin edges
    for (size i = 0; i <= numBins; ++i) {
        float freq = mConfig.minFrequency + static_cast<float>(i) * step;
        mBinFrequencies.push_back(freq);
    }
}

void FrequencyBinMapper::calculateBinMappings() {
    const size numBins = static_cast<size>(mConfig.mode);

    mBinMappings.clear();
    mBinMappings.reserve(numBins);

    // For each output frequency bin, determine which FFT bins contribute
    for (size i = 0; i < numBins; ++i) {
        float minFreq = mBinFrequencies[i];
        float maxFreq = mBinFrequencies[i + 1];

        // Convert frequencies to FFT bin indices
        float startBinFloat = frequencyToFFTBin(minFreq);
        float endBinFloat = frequencyToFFTBin(maxFreq);

        // Round to integer FFT bin indices
        u32 startBin = static_cast<u32>(startBinFloat);
        u32 endBin = static_cast<u32>(fl::ceilf(endBinFloat));

        // Clamp to valid FFT bin range
        if (startBin >= mConfig.fftBinCount) {
            startBin = mConfig.fftBinCount - 1;
        }
        if (endBin > mConfig.fftBinCount) {
            endBin = mConfig.fftBinCount;
        }

        // Ensure at least one FFT bin per output bin
        if (endBin <= startBin) {
            endBin = startBin + 1;
        }

        BinMapping mapping;
        mapping.startBin = startBin;
        mapping.endBin = endBin;
        mBinMappings.push_back(mapping);
    }
}

float FrequencyBinMapper::frequencyToFFTBin(float frequency) const {
    // FFT bin index = (frequency / sampleRate) * fftSize
    // fftSize = fftBinCount * 2 (FFT produces fftSize/2 bins)
    const float fftSize = static_cast<float>(mConfig.fftBinCount) * 2.0f;
    return (frequency / static_cast<float>(mConfig.sampleRate)) * fftSize;
}

void FrequencyBinMapper::mapBins(span<const float> fftBins, span<float> outputBins) const {
    const size numBins = static_cast<size>(mConfig.mode);

    // Validate output buffer size
    if (outputBins.size() < numBins) {
        FL_WARN("FrequencyBinMapper: output buffer too small (" << outputBins.size()
                << " < " << numBins << ")");
        return;
    }

    // Track maximum magnitude for stats
    float maxMag = 0.0f;
    u32 fftBinsUsed = 0;

    // Map FFT bins to frequency bins by averaging
    for (size i = 0; i < numBins; ++i) {
        const BinMapping& mapping = mBinMappings[i];

        float sum = 0.0f;
        u32 count = 0;

        // Average FFT bins in this range
        for (u32 j = mapping.startBin; j < mapping.endBin && j < fftBins.size(); ++j) {
            sum += fftBins[j];
            ++count;
            ++fftBinsUsed;

            if (fftBins[j] > maxMag) {
                maxMag = fftBins[j];
            }
        }

        // Calculate average
        if (count > 0) {
            outputBins[i] = sum / static_cast<float>(count);
        } else {
            outputBins[i] = 0.0f;
        }
    }

    // Update stats (mutable in const method for statistics)
    const_cast<FrequencyBinMapper*>(this)->mStats.binMappingCount++;
    const_cast<FrequencyBinMapper*>(this)->mStats.lastFFTBinsUsed = fftBinsUsed;
    const_cast<FrequencyBinMapper*>(this)->mStats.maxMagnitude = maxMag;
}

float FrequencyBinMapper::getBassEnergy(span<const float> frequencyBins) const {
    if (frequencyBins.size() < BASS_BIN_END) {
        return 0.0f;
    }

    // Average bins 0-1 (bass range)
    float sum = 0.0f;
    for (size i = BASS_BIN_START; i < BASS_BIN_END; ++i) {
        sum += frequencyBins[i];
    }
    return sum / static_cast<float>(BASS_BIN_END - BASS_BIN_START);
}

float FrequencyBinMapper::getMidEnergy(span<const float> frequencyBins) const {
    if (frequencyBins.size() < MID_BIN_END) {
        return 0.0f;
    }

    // Average bins 6-7 (mid range)
    float sum = 0.0f;
    for (size i = MID_BIN_START; i < MID_BIN_END; ++i) {
        sum += frequencyBins[i];
    }
    return sum / static_cast<float>(MID_BIN_END - MID_BIN_START);
}

float FrequencyBinMapper::getTrebleEnergy(span<const float> frequencyBins) const {
    if (frequencyBins.size() < TREBLE_BIN_END) {
        return 0.0f;
    }

    // Average bins 14-15 (treble range)
    float sum = 0.0f;
    for (size i = TREBLE_BIN_START; i < TREBLE_BIN_END; ++i) {
        sum += frequencyBins[i];
    }
    return sum / static_cast<float>(TREBLE_BIN_END - TREBLE_BIN_START);
}

FrequencyBinMapper::FrequencyRange FrequencyBinMapper::getBinFrequencyRange(size binIndex) const {
    FrequencyRange range = {0.0f, 0.0f};

    if (binIndex >= mBinFrequencies.size() - 1) {
        return range;
    }

    range.minFreq = mBinFrequencies[binIndex];
    range.maxFreq = mBinFrequencies[binIndex + 1];
    return range;
}

} // namespace fl
