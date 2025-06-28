# Audio Reactive Visualizations

This example demonstrates various audio-reactive visualization modes using FastLED. It processes real-time audio input and creates stunning visual effects synchronized to music.

## Features

### Visualization Modes

1. **Spectrum Bars** - Classic frequency spectrum analyzer with vertical bars
2. **Radial Spectrum** - Circular frequency visualization radiating from center
3. **Waveform** - Real-time audio waveform display
4. **VU Meter** - Traditional volume unit meter with RMS and peak levels
5. **Matrix Rain** - Audio-reactive digital rain effect
6. **Fire Effect** - Flame simulation that responds to audio intensity
7. **Plasma Wave** - Animated plasma patterns modulated by audio

### Audio Processing

- **Real-time FFT Analysis** - Frequency spectrum analysis
- **Beat Detection** - Automatic beat detection with adjustable sensitivity
- **Auto Gain Control** - Automatically adjusts to varying audio levels
- **Noise Floor Filtering** - Removes background noise
- **Peak Detection** - Tracks audio peaks with smoothing

### Visual Controls

- **7 Color Palettes** - Rainbow, Heat, Ocean, Forest, Party, Lava, Cloud
- **Brightness Control** - Adjustable LED brightness
- **Fade Speed** - Control trail/persistence effects
- **Mirror Mode** - Create symmetrical displays
- **Beat Flash** - Visual feedback on beat detection

## Hardware Requirements

- **Controller**: ESP32, Teensy, or other platform with sufficient memory
- **LED Matrix**: 100x100 pixels (10,000 LEDs total)
- **Memory**: Requires `SKETCH_HAS_LOTS_OF_MEMORY` (not suitable for Arduino UNO)
- **Audio Input**: Microphone or line-in audio source

## Wiring

```
LED Matrix:
- Data Pin: GPIO 3 (configurable via LED_PIN)
- Power: 5V (ensure adequate power supply for 10,000 LEDs!)
- Ground: Common ground with controller

Audio Input:
- Follow your platform's audio input configuration
```

## Configuration

### Display Settings
```cpp
#define WIDTH 100          // Matrix width
#define HEIGHT 100         // Matrix height
#define LED_PIN 3          // Data pin for LEDs
#define LED_TYPE WS2812B   // LED chipset
#define COLOR_ORDER GRB    // Color order
```

### Audio Settings
```cpp
#define SAMPLE_RATE 44100  // Audio sample rate
#define FFT_SIZE 512       // FFT size for frequency analysis
```

## UI Controls

### Master Controls
- **Enable Audio** - Toggle audio processing on/off
- **Visualization Mode** - Select from 7 different modes

### Audio Controls
- **Audio Gain** - Manual gain adjustment (0.1 - 5.0)
- **Noise Floor** - Background noise threshold (0.0 - 1.0)
- **Auto Gain** - Enable automatic gain control

### Visual Controls
- **Brightness** - LED brightness (0 - 255)
- **Fade Speed** - Trail effect speed (0 - 255)
- **Color Palette** - Choose color scheme
- **Mirror Mode** - Enable symmetrical display

### Beat Detection
- **Beat Detection** - Enable/disable beat detection
- **Beat Sensitivity** - Adjust detection threshold (0.5 - 3.0)
- **Beat Flash** - Enable visual flash on beats

## Usage

1. Upload the sketch to your controller
2. Connect your LED matrix
3. Provide audio input (microphone or line-in)
4. Use the web UI to control visualizations
5. Select different modes and adjust parameters in real-time

## Performance Tips

- This example requires significant processing power
- Reduce matrix size if experiencing lag
- Disable beat detection for lower CPU usage
- Use simpler visualization modes on slower processors

## Customization

### Adding New Visualizations

1. Add your mode name to the `visualMode` dropdown
2. Create a new `drawYourMode()` function
3. Add a case in the main switch statement
4. Implement your visualization logic

### Modifying Color Palettes

Edit the `getCurrentPalette()` function to add custom palettes:
```cpp
case 7: return YourCustomPalette_p;
```

### Adjusting Matrix Size

For different matrix sizes, modify:
```cpp
#define WIDTH your_width
#define HEIGHT your_height
```

## Memory Usage

This example uses approximately:
- 30KB for LED buffer (100x100 RGB)
- 2KB for FFT data
- 1KB for audio buffers
- Additional memory for effect buffers

## Troubleshooting

- **No visualization**: Check audio input and "Enable Audio" setting
- **Choppy animation**: Reduce matrix size or disable some features
- **No beat detection**: Increase beat sensitivity or check audio levels
- **Dim display**: Increase brightness or check power supply
- **Compilation error**: Ensure platform has sufficient memory

## Credits

This example demonstrates the audio processing capabilities of FastLED, including FFT analysis, beat detection, and various visualization techniques suitable for LED art installations, music visualizers, and interactive displays.
