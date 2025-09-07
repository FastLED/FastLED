
This was a working example of the audio initialization that i know works:


src\platforms\esp\32\audio\example.cpp

This class wrapper does not:

src\platforms\esp\32\audio\i2s_audio_idf5.hpp

We have the same pins mapping. Find out what we are doing wrong and dump report here after this line

## Analysis Report

**Problem:** The class wrapper `IDF5_I2S_Audio` in `idf5_i2s_std.hpp` has incorrect I2S configuration compared to the working example.

### Key Configuration Differences:

#### 1. Slot Configuration Mismatch (CRITICAL)
- **Working example** (`example.cpp:62`): `slot_mask = I2S_STD_SLOT_RIGHT`
- **Class wrapper** (`idf5_i2s_std.hpp:104`): `slot_mask = I2S_STD_SLOT_LEFT` ❌

- **Working example** (`example.cpp:60`): `slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT`
- **Class wrapper** (`idf5_i2s_std.hpp:102`): `slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO` ❌

- **Working example** (`example.cpp:63`): `ws_width = 32`
- **Class wrapper** (`idf5_i2s_std.hpp:105`): `ws_width = I2S_DATA_BIT_WIDTH_16BIT` ❌

#### 2. Missing GPIO Pull-down Configuration (CRITICAL)
- **Working example** (`example.cpp:114`): `gpio_set_pull_mode(PIN_IS2_SD, GPIO_PULLDOWN_ONLY)`
- **Class wrapper**: Missing this essential hardware setup ❌

#### 3. Missing Power Management (IMPORTANT)  
- **Working example** (`example.cpp:88-94`): APB frequency lock for stable I2S clocking
- **Class wrapper**: No power management ❌

#### 4. DMA Buffer Size Calculation
- **Working example**: Uses `AUDIO_SAMPLES_PER_DMA_BUFFER` directly
- **Class wrapper**: Uses `I2S_AUDIO_BUFFER_LEN / 2` which may not match the expected frame size

### Root Cause:
The INMP441 microphone expects right-channel slot data with 32-bit slot width, but the class wrapper is configured for left-channel with auto/16-bit width.
