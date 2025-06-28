# Audio Reactive Visualization Example

This example demonstrates advanced audio reactive visualization capabilities using FastLED. It processes real-time audio input and creates stunning visual effects synchronized to music.

## Features

### Visualization Modes

1. **Spectrum Analyzer** - Classic frequency spectrum display with customizable colors
2. **Waveform** - Real-time audio waveform visualization
3. **VU Meter** - Traditional volume unit meter with RMS and peak indicators
4. **Spectrogram** - Scrolling frequency analysis over time
5. **Combined** - Split-screen showing both spectrum and waveform
6. **Reactive Patterns** - Dynamic patterns that respond to audio energy and beats

### Audio Processing

- **Real-time FFT** - Fast Fourier Transform for frequency analysis
- **Beat Detection** - Automatic beat detection with adjustable sensitivity
- **Auto Gain Control (AGC)** - Automatically adjusts to varying audio levels
- **Noise Floor Filtering** - Removes background noise for cleaner visuals
- **Attack/Decay/Sustain** - Professional audio envelope controls

### Visual Controls

- **Multiple Color Palettes** - Heat, Rainbow, Ocean, Forest, Lava, Cloud, Party
- **Mirror Mode** - Creates symmetrical displays
- **Brightness Control** - Adjustable LED brightness
- **Fade Effects** - Smooth transitions with adjustable fade time
- **Color Animation** - Animated color cycling with speed control
- **Smoothing** - Optional smoothing for less jittery displays

### Advanced Features

- **Frequency Band Analysis** - 8-band frequency analyzer for detailed audio analysis
- **FFT Smoothing** - Temporal smoothing of frequency data
- **Logarithmic Scale** - Optional log scale for frequency display
- **Freeze Frame** - Pause the visualization at any moment
- **Frame Advance** - Step through frozen frames

## UI Controls

### Main Controls
- **Enable Audio Reactive Mode** - Master on/off switch for audio processing
- **Visualization Mode** - Dropdown to select visualization type

### Audio Processing Group
- **Fade Time** - How quickly levels decay (0-4 seconds)
- **Attack Time** - How quickly levels rise (0-4 seconds)
- **Output Smoothing** - Final output smoothing (0-2 seconds)
- **Audio Gain** - Manual gain adjustment (0.1-5.0)
- **Noise Floor** - Background noise threshold (-80 to -20 dB)

### Visual Controls Group
- **Fade to Black** - Trail/persistence effect (0-50)
- **Brightness** - LED brightness (0-255)
- **Color Speed** - Animation speed (0.1-5.0)
- **Color Palette** - Choose from 7 palettes
- **Mirror Mode** - Enable symmetrical display
- **Smoothing** - Enable temporal smoothing

### FFT Controls Group
- **Min Frequency** - Lower frequency bound (20-1000 Hz)
- **Max Frequency** - Upper frequency bound (1000-20000 Hz)
- **Logarithmic Scale** - Use log scale for frequency
- **FFT Smoothing** - Smoothing factor (0-0.95)

### Advanced Controls Group
- **Freeze Frame** - Pause visualization
- **Advance Frame** - Step forward when frozen
- **Beat Detection** - Enable beat detection
- **Beat Sensitivity** - Beat detection threshold (0.5-3.0)
- **Auto Gain Control** - Enable automatic gain adjustment

## Hardware Setup

### LED Configuration
- Default: 128x128 LED matrix (16,384 LEDs)
- Downscaled to 64x64 for output (4,096 LEDs)
- Data pin: GPIO 3 (configurable)
- LED type: WS2812B (Neopixel)

### Audio Input
The example uses the FastLED audio system which can accept input from:
- Microphone (real-time audio capture)
- Audio file playback
- System audio (on supported platforms)

## Usage

1. **Basic Operation**
   - Upload the sketch to your controller
   - Connect your LED matrix
   - Provide audio input
   - Use the web UI to control visualization

2. **Optimizing for Your Setup**
   - Adjust the noise floor if visualization is too sensitive/insensitive
   - Use AGC for varying audio levels
   - Tune beat sensitivity for your music style
   - Experiment with different color palettes and speeds

3. **Performance Tips**
   - Reduce matrix size for slower controllers
   - Disable smoothing for more responsive display
   - Use simpler visualization modes for lower CPU usage

## Code Structure

### Main Components

1. **Audio Processing Pipeline**
   ```cpp
   AudioSample → FFT → Band Analysis → Beat Detection → Visualization
   ```

2. **Visualization Functions**
   - `drawSpectrumAnalyzer()` - Frequency spectrum bars
   - `drawWaveform()` - Audio waveform display
   - `drawVUMeter()` - Volume meter visualization
   - `drawSpectrogram()` - Time-frequency plot
   - `drawReactivePatterns()` - Beat-reactive patterns

3. **Audio Analysis Classes**
   - `MaxFadeTracker` - Smooth peak tracking with attack/decay
   - `BeatDetector` - Energy-based beat detection
   - `FrequencyBandAnalyzer` - 8-band frequency analysis

## Customization

### Adding New Visualizations
1. Create a new draw function
2. Add it to the visualization mode dropdown
3. Add a case in the main switch statement

### Modifying Color Palettes
Edit the `getCurrentPalette()` function to add custom palettes.

### Adjusting Frequency Bands
Modify the `FrequencyBandAnalyzer` constructor to change band boundaries.

## Troubleshooting

- **No visualization**: Check audio input and ensure "Enable Audio Reactive Mode" is on
- **Too dim/bright**: Adjust brightness control
- **Choppy animation**: Increase smoothing or reduce matrix size
- **No beat detection**: Adjust beat sensitivity or check audio levels
- **Visualization too sensitive**: Increase noise floor value

## Memory Requirements

This example requires significant memory for:
- Framebuffer: 128×128×3 = 49,152 bytes
- LED buffer: 64×64×3 = 12,288 bytes
- Audio buffers and FFT data

Platforms with limited memory may need to reduce the matrix size.
