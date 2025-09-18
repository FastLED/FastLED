/**
 * FastLED Video Recorder Module
 *
 * Captures canvas content and audio to create video recordings.
 * Supports high-quality video with configurable frame rates and audio sync.
 *
 * @module VideoRecorder
 */

/* eslint-disable no-console */
/* eslint-disable import/prefer-default-export */

/**
 * Default video recording configuration
 * @constant {Object}
 */
const DEFAULT_VIDEO_CONFIG = {
  /** @type {string} Video MIME type for recording */
  videoCodec: 'video/webm;codecs=vp9',
  /** @type {number} Target video bitrate in Mbps */
  videoBitrate: 10,
  /** @type {string} Audio codec for recording */
  audioCodec: 'opus',
  /** @type {number} Target audio bitrate in kbps */
  audioBitrate: 128,
  /** @type {number} Default frame rate */
  fps: 30,
};

/**
 * Fallback MIME types if VP9 is not supported
 * @constant {string[]}
 */
const FALLBACK_MIME_TYPES = [
  'video/webm;codecs=vp8,opus',
  'video/webm;codecs=vp8',
  'video/webm',
  'video/mp4',
];

/**
 * Video Recorder class for capturing canvas and audio
 */
export class VideoRecorder {
  /**
   * Creates a new VideoRecorder instance
   * @param {Object} options - Configuration options
   * @param {HTMLCanvasElement} options.canvas - The canvas element to record
   * @param {AudioContext} [options.audioContext] - Audio context for capturing audio
   * @param {number} [options.fps] - Frame rate for recording (overrides settings.fps if provided)
   * @param {Function} [options.onStateChange] - Callback for state changes
   * @param {Object} [options.settings] - Recording settings (codec, bitrate, etc.)
   */
  constructor(options) {
    const { canvas, audioContext, fps, onStateChange, settings } = options;

    /** @type {HTMLCanvasElement} */
    this.canvas = canvas;

    /** @type {AudioContext|null} */
    this.audioContext = audioContext || null;

    /** @type {Object} Current recording settings */
    this.settings = { ...DEFAULT_VIDEO_CONFIG, ...settings };

    /** @type {number} */
    this.fps = fps || this.settings.fps;

    /** @type {MediaRecorder|null} */
    this.mediaRecorder = null;

    /** @type {MediaStream|null} */
    this.stream = null;

    /** @type {Array<Blob>} */
    this.recordedChunks = [];

    /** @type {boolean} */
    this.isRecording = false;

    /** @type {Function|null} */
    this.onStateChange = onStateChange || null;

    /** @type {MediaStreamAudioDestinationNode|null} */
    this.audioDestination = null;

    /** @type {string} */
    this.selectedMimeType = this.selectMimeType();

    console.log('VideoRecorder initialized with settings:', this.settings);
    console.log('Selected MIME type:', this.selectedMimeType);
  }

  /**
   * Selects the best available MIME type for recording
   * @returns {string} Selected MIME type
   */
  selectMimeType() {
    // Start with the configured video codec
    const mimeType = this.settings.videoCodec;

    // Log what we're starting with
    console.log('Original video codec setting:', mimeType);

    // Try the configured MIME type first
    if (MediaRecorder.isTypeSupported(mimeType)) {
      console.log('Using configured MIME type:', mimeType);
      return mimeType;
    }

    // Try fallback MIME types
    for (const fallbackType of FALLBACK_MIME_TYPES) {
      if (MediaRecorder.isTypeSupported(fallbackType)) {
        console.log('Using fallback MIME type:', fallbackType);
        return fallbackType;
      }
    }

    // Try extremely basic WebM
    if (MediaRecorder.isTypeSupported('video/webm')) {
      console.log('Using basic WebM');
      return 'video/webm';
    }

    // Use default if nothing else works
    console.warn('No MIME types supported, using browser default');
    return '';
  }

  /**
   * Creates a combined media stream with video from canvas and audio
   * @returns {MediaStream} Combined media stream
   */
  createMediaStream() {
    // Ensure canvas has some content by drawing a test pattern if it's blank
    const ctx = this.canvas.getContext('2d');
    const imageData = ctx.getImageData(0, 0, this.canvas.width, this.canvas.height);
    const hasContent = imageData.data.some(pixel => pixel !== 0);

    if (!hasContent) {
      console.warn('Canvas appears to be blank, drawing test pattern');
      ctx.fillStyle = '#ff0000';
      ctx.fillRect(0, 0, this.canvas.width / 2, this.canvas.height / 2);
      ctx.fillStyle = '#00ff00';
      ctx.fillRect(this.canvas.width / 2, 0, this.canvas.width / 2, this.canvas.height / 2);
      ctx.fillStyle = '#0000ff';
      ctx.fillRect(0, this.canvas.height / 2, this.canvas.width / 2, this.canvas.height / 2);
      ctx.fillStyle = '#ffff00';
      ctx.fillRect(this.canvas.width / 2, this.canvas.height / 2, this.canvas.width / 2, this.canvas.height / 2);
    }

    // Get video stream from canvas
    const videoStream = this.canvas.captureStream(this.fps);

    console.log('Canvas dimensions:', this.canvas.width, 'x', this.canvas.height);
    console.log('Canvas has content:', hasContent);
    console.log('Video stream tracks:', videoStream.getVideoTracks().length);
    console.log('Video stream settings:', videoStream.getVideoTracks()[0]?.getSettings());

    // If no audio context, return video-only stream
    if (!this.audioContext) {
      console.log('No audio context, using video-only stream');
      return videoStream;
    }

    try {
      // Create audio destination node for capturing audio
      this.audioDestination = this.audioContext.createMediaStreamDestination();

      // Connect all audio sources to the destination
      // This assumes audio is being routed through the main gain node
      // You may need to connect specific audio nodes here
      const gainNode = this.audioContext.gain || this.audioContext.createGain();
      gainNode.connect(this.audioDestination);

      // Combine video and audio tracks
      const combinedStream = new MediaStream([
        ...videoStream.getVideoTracks(),
        ...this.audioDestination.stream.getAudioTracks(),
      ]);

      console.log('Combined stream - Video tracks:', combinedStream.getVideoTracks().length);
      console.log('Combined stream - Audio tracks:', combinedStream.getAudioTracks().length);

      return combinedStream;
    } catch (error) {
      console.error('Error creating audio stream, using video-only:', error);
      return videoStream;
    }
  }

  /**
   * Starts recording the canvas and audio
   * @returns {boolean} True if recording started successfully
   */
  startRecording() {
    if (this.isRecording) {
      console.warn('Recording already in progress');
      return false;
    }

    try {
      // Create the media stream
      this.stream = this.createMediaStream();

      // Configure MediaRecorder options
      const options = {
        mimeType: this.selectedMimeType,
        videoBitsPerSecond: this.settings.videoBitrate * 1000000, // Convert Mbps to bps
        audioBitsPerSecond: this.settings.audioBitrate * 1000,    // Convert kbps to bps
      };

      // Remove mimeType if empty (use browser default)
      if (!options.mimeType) {
        delete options.mimeType;
      }

      // Create MediaRecorder instance
      this.mediaRecorder = new MediaRecorder(this.stream, options);

      // Reset recorded chunks
      this.recordedChunks = [];

      // Set up event handlers
      this.mediaRecorder.ondataavailable = (event) => {
        console.log('Data available:', event.data.size, 'bytes');
        if (event.data.size > 0) {
          this.recordedChunks.push(event.data);
        }
      };

      this.mediaRecorder.onstop = () => {
        console.log('MediaRecorder stopped, total chunks:', this.recordedChunks.length);
        console.log('Total data size:', this.recordedChunks.reduce((sum, chunk) => sum + chunk.size, 0), 'bytes');
        this.saveRecording();
      };

      this.mediaRecorder.onerror = (event) => {
        console.error('MediaRecorder error:', event.error);
        this.stopRecording();
      };

      // Log MediaRecorder configuration
      console.log('MediaRecorder options:', options);
      console.log('MediaRecorder state:', this.mediaRecorder.state);
      console.log('Stream active:', this.stream.active);
      console.log('Stream tracks:', this.stream.getTracks().map(t => ({ kind: t.kind, enabled: t.enabled, muted: t.muted })));

      // Start recording with explicit timeslice to ensure data collection
      this.mediaRecorder.start(1000); // Request data every 1 second
      this.isRecording = true;

      // Notify state change
      if (this.onStateChange) {
        this.onStateChange(true);
      }

      console.log('Recording started');
      return true;
    } catch (error) {
      console.error('Failed to start recording:', error);
      return false;
    }
  }

  /**
   * Stops the current recording
   * @returns {boolean} True if recording was stopped
   */
  stopRecording() {
    if (!this.isRecording || !this.mediaRecorder) {
      console.warn('No recording in progress');
      return false;
    }

    try {
      // Stop the media recorder
      this.mediaRecorder.stop();
      this.isRecording = false;

      // Stop all tracks in the stream
      if (this.stream) {
        this.stream.getTracks().forEach(track => track.stop());
        this.stream = null;
      }

      // Disconnect audio destination
      if (this.audioDestination) {
        this.audioDestination.disconnect();
        this.audioDestination = null;
      }

      // Notify state change
      if (this.onStateChange) {
        this.onStateChange(false);
      }

      console.log('Recording stopped');
      return true;
    } catch (error) {
      console.error('Error stopping recording:', error);
      return false;
    }
  }

  /**
   * Saves the recorded video to a file
   */
  saveRecording() {
    if (this.recordedChunks.length === 0) {
      console.warn('No recorded data to save');
      return;
    }

    // Create a blob from recorded chunks
    const blob = new Blob(this.recordedChunks, {
      type: this.selectedMimeType || 'video/webm',
    });

    // Generate filename with timestamp
    const timestamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, -5);
    const filename = `fastled-recording-${timestamp}.webm`;

    // Create download link
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.style.display = 'none';
    a.href = url;
    a.download = filename;

    // Trigger download
    document.body.appendChild(a);
    a.click();

    // Cleanup
    setTimeout(() => {
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
    }, 100);

    console.log(`Recording saved as ${filename}`);
  }

  /**
   * Toggles recording state
   * @returns {boolean} New recording state
   */
  toggleRecording() {
    if (this.isRecording) {
      this.stopRecording();
      return false;
    } else {
      this.startRecording();
      return true;
    }
  }

  /**
   * Sets the frame rate for recording
   * @param {number} fps - Frame rate (e.g., 24, 25, 30, 50, 60)
   */
  setFrameRate(fps) {
    if (this.isRecording) {
      console.warn('Cannot change frame rate while recording');
      return;
    }
    this.fps = fps;
    this.settings.fps = fps;
    console.log(`Frame rate set to ${fps} FPS`);
  }

  /**
   * Updates recording settings
   * @param {Object} newSettings - New settings to apply
   */
  updateSettings(newSettings) {
    if (this.isRecording) {
      console.warn('Cannot change settings while recording');
      return;
    }

    // Merge new settings with current settings
    this.settings = { ...this.settings, ...newSettings };

    // Update fps if provided
    if (newSettings.fps) {
      this.fps = newSettings.fps;
    }

    // Reselect MIME type with new settings
    this.selectedMimeType = this.selectMimeType();

    console.log('Video recorder settings updated:', this.settings);
    console.log('New MIME type:', this.selectedMimeType);
  }

  /**
   * Gets current recording state
   * @returns {boolean} True if currently recording
   */
  getIsRecording() {
    return this.isRecording;
  }

  /**
   * Test recording with minimal configuration to verify basic functionality
   * @returns {boolean} True if test can proceed
   */
  testRecording() {
    console.log('Testing MediaRecorder capabilities...');

    // Test basic WebM support
    const basicWebM = MediaRecorder.isTypeSupported('video/webm');
    console.log('Basic WebM support:', basicWebM);

    // Test VP8 support
    const vp8 = MediaRecorder.isTypeSupported('video/webm;codecs=vp8');
    console.log('VP8 support:', vp8);

    // Test VP9 support
    const vp9 = MediaRecorder.isTypeSupported('video/webm;codecs=vp9');
    console.log('VP9 support:', vp9);

    // Test with basic canvas stream
    try {
      const testStream = this.canvas.captureStream(15);
      console.log('Canvas stream created successfully');
      console.log('Stream tracks:', testStream.getTracks().length);
      return true;
    } catch (error) {
      console.error('Canvas stream creation failed:', error);
      return false;
    }
  }

  /**
   * Cleanup method to release resources
   */
  dispose() {
    if (this.isRecording) {
      this.stopRecording();
    }
    this.canvas = null;
    this.audioContext = null;
    this.onStateChange = null;
  }
}

/**
 * MediaRecorder API Documentation for reference:
 *
 * The MediaRecorder interface of the MediaStream Recording API provides
 * functionality to easily record media. It is created using the
 * MediaRecorder() constructor.
 *
 * Key methods:
 * - start(): Begins recording media
 * - stop(): Stops recording, triggers 'dataavailable' and 'stop' events
 * - pause(): Pauses the recording
 * - resume(): Resumes a paused recording
 *
 * Key events:
 * - dataavailable: Fired periodically with recorded data
 * - stop: Fired when recording stops
 * - error: Fired when an error occurs
 *
 * Canvas capture:
 * - canvas.captureStream(fps): Creates a MediaStream from canvas content
 * - Captures canvas at specified frame rate
 * - Automatically includes any canvas updates
 *
 * Audio capture:
 * - AudioContext.createMediaStreamDestination(): Creates audio destination
 * - Can combine multiple audio sources
 * - Syncs with video timeline automatically
 *
 * File save timing:
 * - File is saved AFTER recording stops (not when it starts)
 * - This ensures precise start/stop timing for sync purposes
 * - User sees save dialog after recording completes
 */