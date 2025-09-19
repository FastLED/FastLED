/**
 * FastLED Video Recorder Module
 *
 * Captures canvas content and audio to create video recordings.
 * Supports high-quality video with configurable frame rates and audio sync.
 *
 * @module VideoRecorder
 */

/* eslint-disable no-console */

/**
 * Default video recording configuration
 * @constant {Object}
 */
const DEFAULT_VIDEO_CONFIG = {
  /** @type {string} Video MIME type for recording */
  videoCodec: 'video/mp4;codecs=h264,aac',
  /** @type {number} Target video bitrate in Mbps */
  videoBitrate: 10,
  /** @type {string} Audio codec for recording */
  audioCodec: 'aac',
  /** @type {number} Target audio bitrate in kbps */
  audioBitrate: 128,
  /** @type {number} Default frame rate */
  fps: 30,
};

/**
 * Fallback MIME types if H.264/MP4 is not supported
 * @constant {string[]}
 */
const FALLBACK_MIME_TYPES = [
  'video/webm;codecs=vp9,opus',
  'video/webm;codecs=vp8,opus',
  'video/webm;codecs=vp9',
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
    const {
      canvas, audioContext, fps, onStateChange, settings,
    } = options;

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

    /** @type {number} Frame counter for logging */
    this.frameCounter = 0;

    /** @type {number} Recording start time */
    this.recordingStartTime = 0;

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
    // Validate canvas and context
    if (!this.canvas) {
      throw new Error('Canvas element is null or undefined');
    }

    // Don't try to read canvas content since graphics manager might be using WebGL
    // The video recorder will capture whatever is being rendered to the canvas
    console.log('Video recorder creating media stream from canvas...');

    // Get video stream from canvas
    const videoStream = this.canvas.captureStream(this.fps);

    console.log('Canvas dimensions:', this.canvas.width, 'x', this.canvas.height);
    console.log('Video stream tracks:', videoStream.getVideoTracks().length);
    console.log('Video stream settings:', videoStream.getVideoTracks()[0]?.getSettings());

    // Validate that we have video tracks
    const videoTracks = videoStream.getVideoTracks();
    if (videoTracks.length === 0) {
      throw new Error('Canvas captureStream() did not produce any video tracks');
    }

    // Check if video tracks are enabled and not muted
    videoTracks.forEach((track, index) => {
      console.log(`Video track ${index}:`, {
        enabled: track.enabled,
        muted: track.muted,
        readyState: track.readyState,
        kind: track.kind,
        id: track.id
      });
      if (track.readyState === 'ended') {
        console.warn(`Video track ${index} is already ended`);
      }
    });

    // If no audio context, return video-only stream
    if (!this.audioContext) {
      console.log('No audio context, using video-only stream');
      return videoStream;
    }

    try {
      // Create audio destination node for capturing audio
      this.audioDestination = this.audioContext.createMediaStreamDestination();

      // Try to connect audio sources if they exist
      // Look for common audio node patterns
      if (this.audioContext.destination && this.audioContext.destination.connect) {
        // Some browsers allow connecting from destination
        try {
          // Create a gain node to route audio properly
          const gainNode = this.audioContext.createGain();
          gainNode.connect(this.audioDestination);
          console.log('Audio routing established via gain node');
        } catch (e) {
          console.warn('Could not establish audio routing:', e);
        }
      }

      // Check if we have audio tracks
      const audioTracks = this.audioDestination.stream.getAudioTracks();
      if (audioTracks.length === 0) {
        console.warn('No audio tracks available, recording video only');
        return videoStream;
      }

      // Combine video and audio tracks
      const combinedStream = new MediaStream([
        ...videoStream.getVideoTracks(),
        ...audioTracks,
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
        audioBitsPerSecond: this.settings.audioBitrate * 1000, // Convert kbps to bps
      };

      // Remove mimeType if empty (use browser default)
      if (!options.mimeType) {
        delete options.mimeType;
      }

      // Try to create MediaRecorder instance with options first
      try {
        this.mediaRecorder = new MediaRecorder(this.stream, options);
        console.log('MediaRecorder created with options:', options);
      } catch (optionsError) {
        console.warn('MediaRecorder failed with options, trying without options:', optionsError);
        try {
          // Fallback: create without options
          this.mediaRecorder = new MediaRecorder(this.stream);
          console.log('MediaRecorder created without options (browser default)');
        } catch (basicError) {
          console.error('MediaRecorder creation failed entirely:', basicError);
          throw basicError;
        }
      }

      // Reset recorded chunks and frame counter
      this.recordedChunks = [];
      this.frameCounter = 0;
      this.recordingStartTime = performance.now();

      // Set up event handlers
      this.mediaRecorder.onstart = () => {
        console.log('MediaRecorder start event fired');
        this.isRecording = true;
        // Notify state change
        if (this.onStateChange) {
          this.onStateChange(true);
        }
        // Force a frame update to ensure we get data
        this.requestFrameUpdate();
      };

      this.mediaRecorder.ondataavailable = (event) => {
        console.log('Data available:', event.data.size, 'bytes');
        if (event.data.size > 0) {
          this.recordedChunks.push(event.data);
          this.frameCounter++;
          console.log(`frame ${this.frameCounter}`);
        }
      };

      this.mediaRecorder.onstop = () => {
        console.log('MediaRecorder stopped, total chunks:', this.recordedChunks.length);
        console.log('Total data size:', this.recordedChunks.reduce((sum, chunk) => sum + chunk.size, 0), 'bytes');
        this.saveRecording();
      };

      this.mediaRecorder.onerror = (event) => {
        console.error('MediaRecorder error:', event.error);
        this.isRecording = false;
        if (this.onStateChange) {
          this.onStateChange(false);
        }
        this.stopRecording();
      };

      // Log MediaRecorder configuration
      console.log('MediaRecorder options:', options);
      console.log('MediaRecorder state:', this.mediaRecorder.state);
      console.log('Stream active:', this.stream.active);
      console.log('Stream tracks:', this.stream.getTracks().map((t) => ({ kind: t.kind, enabled: t.enabled, muted: t.muted })));

      // Start recording with explicit timeslice to ensure data collection
      try {
        // Try different timeslice values - some browsers are picky
        const timeslice = 1000; // Default: 1 second
        try {
          this.mediaRecorder.start(timeslice);
          console.log(`MediaRecorder.start(${timeslice}) called successfully`);
        } catch (timesliceError) {
          console.warn(`MediaRecorder.start(${timeslice}) failed, trying without timeslice:`, timesliceError);
          this.mediaRecorder.start(); // Try without timeslice
          console.log('MediaRecorder.start() called successfully (no timeslice)');
        }

        // Add a timeout to ensure we detect if recording fails to start
        setTimeout(() => {
          if (this.mediaRecorder && this.mediaRecorder.state !== 'recording') {
            console.error('MediaRecorder failed to transition to recording state:', this.mediaRecorder.state);
            this.isRecording = false;
            if (this.onStateChange) {
              this.onStateChange(false);
            }
          }
        }, 500);

      } catch (startError) {
        console.error('MediaRecorder.start() failed:', startError);
        this.isRecording = false;
        if (this.onStateChange) {
          this.onStateChange(false);
        }
        return false;
      }

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
        this.stream.getTracks().forEach((track) => track.stop());
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

    try {
      // Create a blob from recorded chunks
      const blob = new Blob(this.recordedChunks, {
        type: this.selectedMimeType || 'video/webm',
      });

      // Determine file extension from MIME type
      let extension = '.webm'; // default
      if (this.selectedMimeType) {
        if (this.selectedMimeType.includes('mp4')) {
          extension = '.mp4';
        } else if (this.selectedMimeType.includes('webm')) {
          extension = '.webm';
        }
      }

      // Generate filename with timestamp
      const timestamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, -5);
      const filename = `fastled-recording-${timestamp}${extension}`;

      // Log recording statistics
      const recordingDuration = (performance.now() - this.recordingStartTime) / 1000;
      console.log(`Recording completed: ${this.frameCounter} frames in ${recordingDuration.toFixed(1)}s`);
      console.log(`Average FPS: ${(this.frameCounter / recordingDuration).toFixed(1)}`);
      console.log(`File size: ${(blob.size / 1024 / 1024).toFixed(2)} MB`);

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
        if (document.body.contains(a)) {
          document.body.removeChild(a);
        }
        URL.revokeObjectURL(url);
      }, 100);

      console.log(`Recording saved as ${filename}`);
    } catch (error) {
      console.error('Failed to save recording:', error);
    }
  }

  /**
   * Toggles recording state
   * @returns {boolean} New recording state
   */
  toggleRecording() {
    if (this.isRecording) {
      this.stopRecording();
      return false;
    }
    this.startRecording();
    return true;
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
   * Forces a frame update to trigger canvas content capture
   * This helps ensure MediaRecorder gets data when recording starts
   */
  requestFrameUpdate() {
    try {
      // Try to request a frame update if available
      if (window.requestAnimationFrame) {
        window.requestAnimationFrame(() => {
          console.log('Frame update requested to trigger canvas capture');
          // If we have access to a render function, call it
          if (window.updateCanvas && typeof window.updateCanvas === 'function') {
            try {
              // Pass empty frame data with screenMap for canvas refresh during recording
              const emptyFrameData = Object.assign([], {
                screenMap: window.screenMap || undefined
              });
              window.updateCanvas(emptyFrameData);
            } catch (e) {
              console.warn('updateCanvas() call failed:', e);
            }
          }
        });
      }
    } catch (e) {
      console.warn('requestFrameUpdate failed:', e);
    }
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
    try {
      if (this.isRecording) {
        this.stopRecording();
      }

      // Clean up media recorder
      if (this.mediaRecorder) {
        this.mediaRecorder.ondataavailable = null;
        this.mediaRecorder.onstop = null;
        this.mediaRecorder.onerror = null;
        this.mediaRecorder = null;
      }

      // Clean up stream
      if (this.stream) {
        this.stream.getTracks().forEach((track) => {
          track.stop();
        });
        this.stream = null;
      }

      // Clean up audio destination
      if (this.audioDestination) {
        this.audioDestination.disconnect();
        this.audioDestination = null;
      }

      // Clear recorded chunks
      this.recordedChunks = [];

      // Clear references
      this.canvas = null;
      this.audioContext = null;
      this.onStateChange = null;
    } catch (error) {
      console.error('Error during VideoRecorder disposal:', error);
    }
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
