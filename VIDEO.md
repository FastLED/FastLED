# Video Recording Feature for FastLED Web App

## Implementation Status: ✅ COMPLETE

### Overview
A video recording feature has been implemented for the FastLED web application in `src/platforms/wasm/compiler/`. This feature allows users to capture both the canvas animations and audio output to create high-quality video recordings.

## Features Implemented

### 1. Recording Button UI
- **Location**: Upper right corner of the canvas
- **Visual States**:
  - **Idle**: Gray circular button with a record icon (⚫)
  - **Recording**: Red pulsing button with a stop icon (⬛)
- **Animation**: Smooth pulse effect when recording

### 2. Video Capture Specifications
- **Format**: WebM with VP9 video codec and Opus audio codec
- **Fallback Formats**: VP8, WebM, MP4 (auto-detected based on browser support)
- **Video Bitrate**: 10 Mbps (high quality)
- **Audio Bitrate**: 128 kbps
- **Frame Rate**: 30 FPS default (configurable to 60 FPS)
- **Resolution**: Captures at canvas native resolution

### 3. Audio Capture
- **Source**: Web Audio API AudioContext
- **Integration**: Automatically captures audio if AudioContext is available
- **MP3 Player Integration**:
  - Captures audio from the existing MP3 player module
  - Audio streams from MP3 playback are routed through the AudioContext
  - Mixed audio (synthesized + MP3) is captured in the recording
- **Audio Mixing**: Combines all audio sources (synthesized sounds + MP3 playback) into a single stream
- **Fallback**: Records video-only if audio is unavailable
- **Sync**: Audio and video are automatically synchronized

### 4. User Interaction
- **Start Recording**: Click the record button or press `Ctrl+R` / `Cmd+R`
- **Stop Recording**: Click the button again (now showing stop icon)
- **File Save**: Automatic download prompt when recording stops
- **Filename Format**: `fastled-recording-YYYY-MM-DD-HH-MM-SS.webm`

## Technical Implementation

### Files Created/Modified

1. **`modules/video_recorder.js`** (NEW)
   - Core VideoRecorder class
   - MediaRecorder API integration
   - Canvas stream capture
   - Audio context integration with MP3 player support
   - Mixed audio capture (MP3 + synthesized sounds)
   - File save functionality

2. **`index.html`** (MODIFIED)
   - Added record button HTML structure
   - SVG icons for record/stop states

3. **`index.css`** (MODIFIED)
   - Record button styling
   - Animation effects (pulse, hover)
   - State transitions

4. **`index.js`** (MODIFIED)
   - Video recorder initialization
   - Button event handling
   - Keyboard shortcut support
   - Global debug functions

## API Documentation

### MediaRecorder API Usage

```javascript
// Canvas capture
canvas.captureStream(fps) // Creates MediaStream from canvas

// Audio capture with MP3 mixing
audioContext.createMediaStreamDestination() // Creates audio destination node

// MP3 player audio routing
mp3Player.connect(audioDestination) // Route MP3 audio to recording destination
synthesizer.connect(audioDestination) // Route synthesized audio to destination

// Combined stream with mixed audio
new MediaStream([
  ...videoStream.getVideoTracks(),
  ...audioDestination.stream.getAudioTracks() // Contains mixed audio from all sources
])

// Recording with mixed audio
mediaRecorder = new MediaRecorder(stream, {
  mimeType: 'video/webm;codecs=vp9,opus',
  videoBitsPerSecond: 10000000,
  audioBitsPerSecond: 128000  // Captures all mixed audio sources
})
```

### Key Design Decisions

1. **File Save Timing**: Files are saved AFTER recording stops (not when it starts)
   - Ensures precise start/stop timing for A/V sync
   - User sees save dialog after recording completes
   - No filename prompt interrupts recording start

2. **High Bitrate Choice**: 10 Mbps video bitrate
   - Ensures high quality capture
   - Suitable for 60 FPS content
   - Can be compressed later if needed

3. **WebM/VP9 Format**: Primary codec choice
   - Best browser support for high-quality video
   - Good compression efficiency
   - Supports both video and audio tracks

## Usage Instructions

### For Users
1. Click the gray record button in the upper-right corner of the canvas
2. Button turns red and pulses to indicate recording
3. Perform your LED animation
4. Click the red button to stop recording
5. Browser will prompt to save the video file

### For Developers

#### Global Functions (Console Access)
```javascript
// Get recorder instance
window.getVideoRecorder()

// Start recording programmatically
window.startVideoRecording()

// Stop recording programmatically
window.stopVideoRecording()
```

#### Keyboard Shortcuts
- `Ctrl+R` / `Cmd+R`: Toggle recording on/off

## Browser Compatibility

- ✅ Chrome/Chromium (Full support)
- ✅ Firefox (Full support)
- ✅ Edge (Full support)
- ⚠️ Safari (Limited codec support, uses fallback)

## Future Enhancements (Optional)

- [ ] Settings menu for quality presets
- [ ] Frame rate selector (30/60 FPS toggle)
- [ ] Recording timer display
- [ ] Pause/resume functionality
- [ ] Custom filename input
- [ ] Multiple format export options
- [ ] Recording preview before save
- [ ] Audio level meters for MP3 and synthesized audio
- [ ] Separate audio track controls (mute individual sources)
- [ ] Audio normalization for mixed sources

## Troubleshooting

### No Audio in Recording
- Check if browser has microphone permissions
- Verify AudioContext is initialized
- Some browsers require user interaction to start AudioContext
- Ensure MP3 player audio nodes are properly connected to the recording destination
- Verify that audio mixing is working (check browser console for audio node errors)

### Recording Button Not Appearing
- Check browser console for errors
- Verify canvas element exists with ID "myCanvas"
- Ensure JavaScript modules are loading correctly

### Low Quality Video
- Browser may be using fallback codec
- Check available disk space
- Try different browser for VP9 support

## Testing Checklist

- [x] Record button appears in correct position
- [x] Button changes color when recording
- [x] Pulse animation works during recording
- [x] Canvas content is captured correctly
- [x] Audio is captured (when available)
- [x] File saves with correct timestamp
- [x] Keyboard shortcut works
- [x] Multiple start/stop cycles work correctly
- [x] Memory is properly released after recording