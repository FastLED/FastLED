// g++ --std=c++11 test.cpp

#include "test.h"

#include "fl/audio.h"
#include "fl/fft.h"
#include <vector>

using namespace fl;

TEST_CASE("AudioSample default constructor") {
    AudioSample sample;
    CHECK(!sample.isValid());
    CHECK(!sample);
    CHECK(sample.size() == 0);
}

TEST_CASE("AudioSample with data") {
    // Create sample with simple PCM data
    std::vector<int16_t> pcm_data = {100, -200, 300, -400, 500};
    
    AudioSampleImplPtr impl = AudioSampleImplPtr::New();
    impl->assign(pcm_data.begin(), pcm_data.end());
    
    AudioSample sample(impl);
    
    CHECK(sample.isValid());
    CHECK(sample);
    CHECK(sample.size() == 5);
    CHECK(sample.pcm().size() == 5);
    CHECK(sample[0] == 100);
    CHECK(sample[1] == -200);
    CHECK(sample[2] == 300);
    CHECK(sample[3] == -400);
    CHECK(sample[4] == 500);
}

TEST_CASE("AudioSample zero crossing factor") {
    // Test with alternating positive/negative values (many zero crossings)
    std::vector<int16_t> alternating = {100, -100, 200, -200, 300, -300, 400, -400, 500, -500};
    
    AudioSampleImplPtr impl = AudioSampleImplPtr::New();
    impl->assign(alternating.begin(), alternating.end());
    
    AudioSample sample(impl);
    
    // Should have high zero crossing factor (9 crossings out of 9 possible)
    float zcf = sample.zcf();
    CHECK(zcf >= 0.9f); // Expecting close to 1.0
    
    // Test with all positive values (no zero crossings)
    std::vector<int16_t> all_positive = {100, 200, 300, 400, 500};
    
    AudioSampleImplPtr impl2 = AudioSampleImplPtr::New();
    impl2->assign(all_positive.begin(), all_positive.end());
    
    AudioSample sample2(impl2);
    
    // Should have zero crossing factor of 0
    float zcf2 = sample2.zcf();
    CHECK(zcf2 == 0.0f);
}

TEST_CASE("AudioSample copy constructor and assignment") {
    std::vector<int16_t> pcm_data = {100, -200, 300};
    
    AudioSampleImplPtr impl = AudioSampleImplPtr::New();
    impl->assign(pcm_data.begin(), pcm_data.end());
    
    AudioSample sample1(impl);
    AudioSample sample2(sample1); // Copy constructor
    AudioSample sample3;
    sample3 = sample1; // Assignment operator
    
    CHECK(sample2.isValid());
    CHECK(sample3.isValid());
    CHECK(sample2.size() == 3);
    CHECK(sample3.size() == 3);
    CHECK(sample1 == sample2);
    CHECK(sample1 == sample3);
}

TEST_CASE("AudioSample equality operators") {
    std::vector<int16_t> pcm_data = {100, -200};
    
    AudioSampleImplPtr impl1 = AudioSampleImplPtr::New();
    impl1->assign(pcm_data.begin(), pcm_data.end());
    
    AudioSampleImplPtr impl2 = AudioSampleImplPtr::New();
    impl2->assign(pcm_data.begin(), pcm_data.end());
    
    AudioSample sample1(impl1);
    AudioSample sample2(impl1); // Same impl
    AudioSample sample3(impl2); // Different impl, same data
    AudioSample sample4; // Invalid
    
    CHECK(sample1 == sample2);
    CHECK(sample1 != sample4);
    CHECK(sample4 != sample1);
}

TEST_CASE("SoundLevelMeter basic functionality") {
    SoundLevelMeter meter(33.0, 0.0);
    
    // Test with some sample data
    std::vector<int16_t> samples = {1000, -1000, 2000, -2000, 3000, -3000};
    
    meter.processBlock(samples.data(), samples.size());
    
    // Basic checks - values should be reasonable
    double dbfs = meter.getDBFS();
    double spl = meter.getSPL();
    
    CHECK(dbfs <= 0.0); // DBFS should be <= 0
    CHECK(spl >= 0.0);  // SPL should be reasonable
    
    // Test setFloorSPL - should work without crashing
    meter.setFloorSPL(40.0);
    
    // Test resetFloor - should work without crashing
    meter.resetFloor();
}

TEST_CASE("SoundLevelMeter with slice") {
    SoundLevelMeter meter;
    
    std::vector<int16_t> samples = {500, -500, 1000, -1000};
    fl::Slice<const int16_t> slice(samples.data(), samples.size());
    
    meter.processBlock(slice);
    
    // Should process without issues
    double dbfs = meter.getDBFS();
    CHECK(dbfs <= 0.0);
}

TEST_CASE("AudioSample FFT integration") {
    // Create a simple sine wave
    std::vector<int16_t> sine_wave;
    const int n = 256;
    for (int i = 0; i < n; ++i) {
        float angle = 2.0f * 3.14159f * i / n;
        sine_wave.push_back(int16_t(10000 * sin(angle)));
    }
    
    AudioSampleImplPtr impl = AudioSampleImplPtr::New();
    impl->assign(sine_wave.begin(), sine_wave.end());
    
    AudioSample sample(impl);
    
    FFTBins bins(16);
    sample.fft(&bins);
    
    // FFT should run without crashing and produce reasonable output
    CHECK(bins.bins_raw.size() == 16);
    CHECK(bins.bins_db.size() == 16);
}
