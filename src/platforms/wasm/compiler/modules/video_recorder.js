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
 * @typedef {Object} FrameInfo
 * @property {ImageData} imageData - The image data
 * @property {number} timestamp - Frame timestamp
 * @property {number} frameNumber - Frame number
 * @property {number} width - Frame width
 * @property {number} height - Frame height
 * @property {boolean} [fromWebGL] - Whether from WebGL
 */

/**
 * Default video recording configuration optimized for fastest encoding and MP4 compatibility
 * @constant {Object}
 */
const DEFAULT_VIDEO_CONFIG = {
  /** @type {string} Video MIME type for recording - H.264 baseline for fastest encoding with MP4 compatibility */
  videoCodec: 'video/mp4;codecs=avc1.42E01E',
  /** @type {number} Target video bitrate in Mbps - higher for speed over compression */
  videoBitrate: 8,
  /** @type {string} Audio codec for recording */
  audioCodec: 'aac',
  /** @type {number} Target audio bitrate in kbps */
  audioBitrate: 128,
  /** @type {number} Default frame rate */
  fps: 60,
  /** @type {number} Maximum frame buffer size in frames - extremely large buffer to prevent drops */
  maxBufferFrames: 10000,
  /** @type {number} Buffer size in MB for frame storage */
  maxBufferSizeMB: 500,
};

/**
 * Codec priority ordered by encoding speed (fastest to slowest)
 * Prefers MP4 containers when same codec is available in both MP4 and WebM
 * Optimized for real-time recording performance and universal compatibility
 * @constant {string[]}
 */
const SPEED_OPTIMIZED_CODECS = [
  // Fastest codecs - prioritize MP4 containers for compatibility
  'video/mp4;codecs=avc1.42E01E',    // H.264 baseline profile: Fastest H.264 in MP4
  'video/mp4;codecs=h264',           // H.264: Fast and widely supported in MP4
  'video/mp4;codecs=h264,aac',       // H.264 with AAC in MP4
  'video/webm;codecs=vp8',           // VP8: Fast encoding, WebRTC standard (WebM only)
  'video/mp4',                       // MP4 container auto-select

  // Medium speed codecs - balance of speed and quality
  'video/webm;codecs=vp9',           // VP9: Better compression but slower (WebM only)
  'video/webm;codecs=h264',          // H.264 in WebM container (fallback)
  'video/webm',                      // WebM auto-select

  // Slower codecs - better compression but much slower encoding
  'video/webm;codecs=av1',           // AV1: Best compression, slowest encoding (WebM only)
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

    console.log('VideoRecorder constructor called with options:', options);

    /** @type {HTMLCanvasElement} */
    this.canvas = canvas;

    /** @type {AudioContext|null} */
    this.audioContext = audioContext || null;

    /** @type {Object} Current recording settings */
    this.settings = { ...DEFAULT_VIDEO_CONFIG, ...settings };
    console.log('Final merged settings:', this.settings);

    /** @type {number} */
    this.fps = fps !== undefined ? fps : this.settings.fps;
    console.log('VideoRecorder fps set to:', this.fps, 'from fps param:', fps, 'settings.fps:', this.settings.fps);

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

    /** @type {string} User's preferred format for file extension */
    this.preferredFormat = this.determinePreferredFormat();

    /** @type {number} Frame counter for logging */
    this.frameCounter = 0;

    /** @type {number} Recording start time */
    this.recordingStartTime = 0;

    // === FRAME BUFFER SYSTEM ===
    /** @type {Array<FrameInfo>} Massive frame buffer to prevent any drops */
    this.frameBuffer = [];

    /** @type {number} Current buffer memory usage in bytes */
    this.bufferMemoryUsage = 0;

    /** @type {number} Maximum frames to buffer */
    this.maxBufferFrames = this.settings.maxBufferFrames || 10000;

    /** @type {number} Maximum buffer size in bytes */
    this.maxBufferSize = (this.settings.maxBufferSizeMB || 500) * 1024 * 1024;

    /** @type {boolean} Buffer processing flag */
    this.isProcessingBuffer = false;

    /** @type {number} Frames dropped due to extreme memory pressure */
    this.droppedFrames = 0;

    /** @type {number} Total frames captured */
    this.capturedFrames = 0;

    /** @type {Worker|null} Background worker for frame processing */
    this.frameWorker = null;

    /** @type {boolean} Flag to track if we're in emergency buffer mode */
    this.emergencyBufferMode = false;

    console.log('VideoRecorder initialized with settings:', this.settings);
    console.log('Selected MIME type:', this.selectedMimeType);
    console.log(`Extreme buffer system: ${this.maxBufferFrames} frames, ${(this.maxBufferSize / 1024 / 1024).toFixed(0)}MB capacity`);
  }

  /**
   * Determines the user's preferred format for file naming
   * @returns {string} Preferred format ('mp4' or 'webm')
   */
  determinePreferredFormat() {
    const mimeType = this.settings.videoCodec;
    console.log('Determining preferred format from videoCodec:', mimeType);
    if (mimeType && mimeType.includes('mp4')) {
      console.log('Preferred format: mp4');
      return 'mp4';
    }
    console.log('Preferred format: webm');
    return 'webm';
  }

  /**
   * Gets a user-friendly display name for the current codec
   * @returns {string} Human-readable codec name
   */
  getCodecDisplayName() {
    const codec = this.settings.videoCodec;

    // Map codec MIME types to user-friendly names
    if (codec.includes('avc1.42E01E')) {
      return 'H.264 Baseline MP4';
    } else if (codec.includes('h264,aac')) {
      return 'H.264+AAC MP4';
    } else if (codec.includes('h264') && codec.includes('mp4')) {
      return 'H.264 MP4';
    } else if (codec.includes('h264') && codec.includes('webm')) {
      return 'H.264 WebM';
    } else if (codec.includes('vp8')) {
      return 'VP8 WebM';
    } else if (codec.includes('vp9')) {
      return 'VP9 WebM';
    } else if (codec.includes('av1')) {
      return 'AV1 WebM';
    } else if (codec.includes('mp4')) {
      return 'MP4 Auto';
    } else if (codec.includes('webm')) {
      return 'WebM Auto';
    }

    // Fallback to the actual MIME type if no match
    return codec || 'Default';
  }

  /**
   * Selects the best available MIME type for recording, optimized for encoding speed
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

    // Try speed-optimized codecs in order of encoding speed (fastest first)
    console.log('Configured codec not supported, trying speed-optimized fallbacks...');
    for (const speedOptimizedType of SPEED_OPTIMIZED_CODECS) {
      if (MediaRecorder.isTypeSupported(speedOptimizedType)) {
        console.log(`Using speed-optimized fallback: ${speedOptimizedType}`);
        return speedOptimizedType;
      }
    }

    // Final fallback to basic WebM
    if (MediaRecorder.isTypeSupported('video/webm')) {
      console.log('Using basic WebM as final fallback');
      return 'video/webm';
    }

    // Use browser default if nothing else works
    console.warn('No MIME types supported, using browser default');
    return '';
  }

  /**
   * Captures a frame directly from the canvas and adds it to the buffer
   * This bypasses MediaRecorder completely for maximum reliability
   */
  captureFrameToBuffer() {
    if (!this.isRecording || !this.canvas) {
      return;
    }

    try {
      // Check buffer limits before capturing
      if (this.frameBuffer.length >= this.maxBufferFrames) {
        console.warn(`Frame buffer at max capacity (${this.maxBufferFrames} frames), enabling emergency mode`);
        this.emergencyBufferMode = true;
        this.processBufferEmergency();
        return;
      }

      if (this.bufferMemoryUsage >= this.maxBufferSize) {
        console.warn(`Buffer memory at max (${(this.bufferMemoryUsage / 1024 / 1024).toFixed(1)}MB), enabling emergency mode`);
        this.emergencyBufferMode = true;
        this.processBufferEmergency();
        return;
      }

      // Get 2D context for frame capture
      const ctx = this.canvas.getContext('2d');
      if (!ctx) {
        // If no 2D context available, try WebGL readPixels
        this.captureWebGLFrame();
        return;
      }

      // Capture frame as ImageData
      const imageData = ctx.getImageData(0, 0, this.canvas.width, this.canvas.height);

      // Add timestamp for precise timing
      const frameInfo = {
        imageData: imageData,
        timestamp: performance.now(),
        frameNumber: this.capturedFrames,
        width: this.canvas.width,
        height: this.canvas.height
      };

      // Add to buffer
      this.frameBuffer.push(frameInfo);
      this.capturedFrames++;

      // Update memory usage (ImageData size = width * height * 4 bytes per pixel)
      const frameSize = imageData.width * imageData.height * 4;
      this.bufferMemoryUsage += frameSize;

      // Log buffer status every 100 frames
      if (this.capturedFrames % 100 === 0) {
        console.log(`Frame buffer: ${this.frameBuffer.length} frames, ${(this.bufferMemoryUsage / 1024 / 1024).toFixed(1)}MB`);
      }

      // NOTE: Buffer processing disabled - using native canvas.captureStream() instead
      // Manual buffering is no longer needed and only wastes CPU cycles

    } catch (error) {
      console.error('Error capturing frame to buffer:', error);
      this.droppedFrames++;
    }
  }

  /**
   * Captures frame from WebGL canvas using readPixels
   */
  captureWebGLFrame() {
    try {
      const gl = this.canvas.getContext('webgl') || this.canvas.getContext('webgl2');
      if (!gl) {
        console.warn('No WebGL context available for frame capture');
        return;
      }

      const width = this.canvas.width;
      const height = this.canvas.height;

      // Read pixels from WebGL framebuffer
      const pixels = new Uint8ClampedArray(width * height * 4);
      gl.readPixels(0, 0, width, height, gl.RGBA, gl.UNSIGNED_BYTE, pixels);

      // Create ImageData from WebGL pixels
      const imageData = new ImageData(pixels, width, height);

      // Add to buffer with timing info
      const frameInfo = {
        imageData: imageData,
        timestamp: performance.now(),
        frameNumber: this.capturedFrames,
        width: width,
        height: height,
        fromWebGL: true
      };

      this.frameBuffer.push(frameInfo);
      this.capturedFrames++;
      this.bufferMemoryUsage += pixels.length;

    } catch (error) {
      console.error('Error capturing WebGL frame:', error);
      this.droppedFrames++;
    }
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

    // Get video stream from canvas with exact frame rate
    // Use exact fps - canvas.captureStream() is hardware-accelerated and non-blocking
    console.log('Calling canvas.captureStream() with fps:', this.fps);
    const videoStream = this.canvas.captureStream(this.fps);

    console.log('Canvas dimensions:', this.canvas.width, 'x', this.canvas.height);
    console.log('Video stream tracks:', videoStream.getVideoTracks().length);
    const videoTrackSettings = videoStream.getVideoTracks()[0]?.getSettings();
    console.log('Video stream settings:', videoTrackSettings);
    console.log('🎬 FINAL FPS SENT TO MEDIA RECORDER:', videoTrackSettings?.frameRate || 'unknown');

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
   * Processes the frame buffer asynchronously to encode frames into video
   * Optimized for startup performance to reduce frame drops
   */
  async processBufferAsync() {
    if (this.isProcessingBuffer || this.frameBuffer.length === 0) {
      return;
    }

    this.isProcessingBuffer = true;

    try {
      // Use smaller batch sizes during startup to reduce blocking
      const isStartup = this.frameCounter < 60; // First 1 second at 60fps
      const baseBatchSize = isStartup ? 3 : 10;
      const processingDelay = isStartup ? 8 : 0; // Longer delays during startup

      while (this.frameBuffer.length > 0 && this.isRecording) {
        // Adaptive batch sizing based on buffer fill level
        const bufferFillRatio = this.frameBuffer.length / this.maxBufferFrames;
        let batchSize = baseBatchSize;

        if (bufferFillRatio > 0.8) {
          // Aggressive processing when buffer is nearly full
          batchSize = Math.min(20, this.frameBuffer.length);
        } else if (bufferFillRatio > 0.5) {
          // Moderate processing when buffer is half full
          batchSize = Math.min(15, this.frameBuffer.length);
        }

        batchSize = Math.min(batchSize, this.frameBuffer.length);
        const frameBatch = this.frameBuffer.splice(0, batchSize);

        for (const frameInfo of frameBatch) {
          await this.encodeFrameFromBuffer(frameInfo);

          // Update memory usage
          const frameSize = frameInfo.imageData.width * frameInfo.imageData.height * 4;
          this.bufferMemoryUsage = Math.max(0, this.bufferMemoryUsage - frameSize);
        }

        // Yield control back to main thread between batches with adaptive delays
        await new Promise((resolve) => {
          setTimeout(() => resolve(), processingDelay);
        });
      }

      // Check if we can exit emergency mode
      if (this.emergencyBufferMode && this.frameBuffer.length < this.maxBufferFrames * 0.5) {
        console.log('Exiting emergency buffer mode');
        this.emergencyBufferMode = false;
      }

    } catch (error) {
      console.error('Error processing frame buffer:', error);
    } finally {
      this.isProcessingBuffer = false;

      // Continue processing if there are still frames and we're recording
      if (this.frameBuffer.length > 0 && this.isRecording) {
        // Dynamic scheduling based on buffer pressure
        const nextProcessDelay = this.frameBuffer.length > this.maxBufferFrames * 0.7 ? 8 : 16;
        setTimeout(() => this.processBufferAsync(), nextProcessDelay);
      }
    }
  }

  /**
   * Encodes a single frame from the buffer into the video stream
   * @param {FrameInfo} frameInfo - Frame information with ImageData and timing
   */
  async encodeFrameFromBuffer(frameInfo) {
    try {
      if (!this.mediaRecorder || !this.stream) {
        return;
      }

      // Create a temporary canvas for this frame
      const tempCanvas = document.createElement('canvas');
      tempCanvas.width = frameInfo.width;
      tempCanvas.height = frameInfo.height;

      const tempCtx = tempCanvas.getContext('2d');
      if (!tempCtx) {
        console.warn('Could not get 2D context for frame encoding');
        return;
      }

      // Draw the frame data to the temporary canvas
      tempCtx.putImageData(frameInfo.imageData, 0, 0);

      // Get a stream from this frame and add it to our video stream
      // Note: This is a simplified approach - in a full implementation,
      // we would need to properly handle the MediaRecorder's dataavailable events

      this.frameCounter++;

      if (this.frameCounter % 60 === 0) {
        console.log(`Encoded frame ${this.frameCounter} from buffer (${this.frameBuffer.length} frames remaining)`);
      }

    } catch (error) {
      console.error('Error encoding frame from buffer:', error);
    }
  }

  /**
   * Emergency buffer processing - aggressively processes frames when buffer is full
   */
  processBufferEmergency() {
    if (!this.emergencyBufferMode) {
      return;
    }

    try {
      console.log('Emergency buffer processing: dropping oldest frames to prevent memory overflow');

      // Drop 10% of oldest frames to make room
      const framesToDrop = Math.floor(this.frameBuffer.length * 0.1);
      const droppedFrames = this.frameBuffer.splice(0, framesToDrop);

      // Update memory usage
      for (const frame of droppedFrames) {
        const frameSize = frame.imageData.width * frame.imageData.height * 4;
        this.bufferMemoryUsage = Math.max(0, this.bufferMemoryUsage - frameSize);
      }

      this.droppedFrames += framesToDrop;
      console.warn(`Emergency: Dropped ${framesToDrop} frames. Total dropped: ${this.droppedFrames}`);

    } catch (error) {
      console.error('Error in emergency buffer processing:', error);
    }
  }

  /**
   * Starts the frame capture loop that continuously captures frames at the target FPS
   * Uses a more efficient timing approach to reduce startup frame drops
   */
  startFrameCapture() {
    if (!this.isRecording) {
      return;
    }

    const captureInterval = 1000 / this.fps; // Milliseconds per frame
    let lastCaptureTime = performance.now(); // Use performance.now() for better precision
    let frameSkips = 0;

    const captureLoop = (currentTime) => {
      if (!this.isRecording) {
        return; // Exit loop if recording stopped
      }

      const deltaTime = currentTime - lastCaptureTime;

      // Maintain consistent frame rate with adaptive timing to prevent startup drops
      if (deltaTime >= captureInterval * 0.95) { // 5% tolerance for timing jitter
        this.captureFrameToBuffer();
        lastCaptureTime = currentTime - (deltaTime % captureInterval); // Correct for drift
        frameSkips = 0;
      } else if (deltaTime < captureInterval * 0.5) {
        // If we're way ahead of schedule, skip to prevent overloading
        frameSkips++;
        if (frameSkips > 2) {
          // Reset timing if we're consistently too fast
          lastCaptureTime = currentTime;
          frameSkips = 0;
        }
      }

      // Schedule next capture with priority timing
      if (this.isRecording) {
        requestAnimationFrame(captureLoop);
      }
    };

    // Start capture loop immediately - no delay needed
    requestAnimationFrame(captureLoop);
    console.log(`Frame capture loop started at ${this.fps} FPS (${captureInterval.toFixed(1)}ms per frame)`);
  }

  /**
   * Finalizes buffer processing - ensures all buffered frames are processed before stopping
   * @returns {Promise<void>}
   */
  async finalizeBufferProcessing() {
    console.log(`Finalizing buffer processing: ${this.frameBuffer.length} frames remaining`);

    // Set a reasonable timeout to prevent hanging
    const timeoutMs = 30000; // 30 seconds
    const startTime = Date.now();

    while (this.frameBuffer.length > 0 && (Date.now() - startTime) < timeoutMs) {
      if (!this.isProcessingBuffer) {
        await this.processBufferAsync();
      }

      // Short delay to prevent tight loop
      await new Promise((resolve) => {
        setTimeout(() => resolve(), 100);
      });
    }

    if (this.frameBuffer.length > 0) {
      console.warn(`Buffer finalization timed out with ${this.frameBuffer.length} frames remaining`);
    } else {
      console.log('Buffer finalization completed successfully');
    }

    // Log final statistics
    this.logBufferStatistics();
  }

  /**
   * Logs comprehensive buffer statistics
   */
  logBufferStatistics() {
    const totalFrames = this.capturedFrames;
    const droppedFrames = this.droppedFrames;
    const successRate = totalFrames > 0 ? ((totalFrames - droppedFrames) / totalFrames * 100).toFixed(2) : 0;
    const memoryUsedMB = (this.bufferMemoryUsage / 1024 / 1024).toFixed(2);

    console.log('=== FRAME BUFFER STATISTICS ===');
    console.log(`Total frames captured: ${totalFrames}`);
    console.log(`Frames dropped: ${droppedFrames}`);
    console.log(`Success rate: ${successRate}%`);
    console.log(`Buffer memory used: ${memoryUsedMB}MB`);
    console.log(`Frames in buffer: ${this.frameBuffer.length}`);
    console.log(`Emergency mode activated: ${this.emergencyBufferMode ? 'Yes' : 'No'}`);
    console.log('==============================');
  }

  /**
   * Gets current buffer status for monitoring
   * @returns {Object} Buffer status information
   */
  getBufferStatus() {
    return {
      framesInBuffer: this.frameBuffer.length,
      maxBufferFrames: this.maxBufferFrames,
      bufferFillPercent: (this.frameBuffer.length / this.maxBufferFrames * 100).toFixed(1),
      memoryUsageMB: (this.bufferMemoryUsage / 1024 / 1024).toFixed(2),
      maxMemoryMB: (this.maxBufferSize / 1024 / 1024).toFixed(0),
      memoryFillPercent: (this.bufferMemoryUsage / this.maxBufferSize * 100).toFixed(1),
      capturedFrames: this.capturedFrames,
      droppedFrames: this.droppedFrames,
      successRate: this.capturedFrames > 0 ? ((this.capturedFrames - this.droppedFrames) / this.capturedFrames * 100).toFixed(2) : 0,
      emergencyMode: this.emergencyBufferMode,
      isProcessingBuffer: this.isProcessingBuffer
    };
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

      // Encoding speed optimizations based on codec type
      if (this.selectedMimeType.includes('avc1.42E01E')) {
        // H.264 Baseline profile - optimized for fastest encoding
        options.videoKeyFrameIntervalDuration = 2000; // Less frequent keyframes for speed
        options.bitsPerSecond = options.videoBitsPerSecond; // No cap, baseline is fast
      } else if (this.selectedMimeType.includes('h264') || this.selectedMimeType.includes('avc1')) {
        // H.264 standard optimizations for fast encoding
        options.videoKeyFrameIntervalDuration = 2000; // Less frequent keyframes for speed
      } else if (this.selectedMimeType.includes('vp8')) {
        // VP8 optimizations for fast encoding
        options.bitsPerSecond = options.videoBitsPerSecond; // Don't cap VP8, it's already fast
        // VP8 benefits from slightly higher bitrates for speed vs compression trade-off
      } else if (this.selectedMimeType.includes('vp9')) {
        // VP9 is slower, so limit bitrate to help encoding speed
        options.bitsPerSecond = Math.min(options.videoBitsPerSecond, 6000000); // Cap at 6Mbps
      } else if (this.selectedMimeType.includes('av1')) {
        // AV1 is slowest, so significantly limit bitrate for speed
        options.bitsPerSecond = Math.min(options.videoBitsPerSecond, 4000000); // Cap at 4Mbps
      }

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
        console.log('MediaRecorder start event fired - using native canvas.captureStream()');
        this.isRecording = true;
        // Initialize minimal counters (no manual buffering needed)
        this.droppedFrames = 0;
        this.capturedFrames = 0;

        // Note: Frame capture is handled by canvas.captureStream() - no manual capture needed
        console.log('Using native canvas.captureStream() for optimal performance');

        // Notify state change
        if (this.onStateChange) {
          this.onStateChange(true);
        }
        console.log('Native canvas.captureStream() recording initialized successfully');
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

        // No buffer processing needed with canvas.captureStream() - save directly
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

      // Start recording with optimized timeslice for native canvas.captureStream()
      try {
        // Use fixed reasonable timeslices to avoid browser batching issues
        let timeslice = 100; // 100ms chunks = 10 chunks per second, good balance
        if (this.selectedMimeType.includes('avc1.42E01E') || this.selectedMimeType.includes('vp8') || this.selectedMimeType.includes('h264')) {
          timeslice = 100; // Fast codecs - consistent 100ms chunks
        } else if (this.selectedMimeType.includes('av1')) {
          timeslice = 200; // Slower codecs - larger chunks to reduce encoding pressure
        }
        try {
          this.mediaRecorder.start(timeslice);
          console.log(`MediaRecorder.start(${timeslice}ms chunks) called successfully for optimized performance`);
        } catch (timesliceError) {
          console.warn(`MediaRecorder.start(${timeslice}) failed, trying without timeslice:`, timesliceError);
          this.mediaRecorder.start(); // Try without timeslice
          console.log('MediaRecorder.start() called successfully (no timeslice)');
        }

        // MediaRecorder state monitoring handled by onstart/onerror events

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

      // Determine file extension from user's preferred format
      const extension = this.preferredFormat === 'mp4' ? '.mp4' : '.webm';

      // Debug logging for filename generation
      console.log('Saving recording with settings:', {
        originalVideoCodec: this.settings.videoCodec,
        selectedMimeType: this.selectedMimeType,
        preferredFormat: this.preferredFormat,
        extension: extension
      });

      // Generate filename with timestamp
      const timestamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, -5);
      const filename = `fastled-recording-${timestamp}${extension}`;

      // Log comprehensive recording statistics
      const recordingDuration = (performance.now() - this.recordingStartTime) / 1000;
      const bufferStats = this.getBufferStatus();

      console.log('=== RECORDING COMPLETED ===');
      console.log(`Duration: ${recordingDuration.toFixed(1)}s`);
      console.log(`MediaRecorder frames: ${this.frameCounter}`);
      console.log(`Buffer frames captured: ${bufferStats.capturedFrames}`);
      console.log(`Frames dropped: ${bufferStats.droppedFrames}`);
      console.log(`Success rate: ${bufferStats.successRate}%`);
      console.log(`Average FPS: ${(this.frameCounter / recordingDuration).toFixed(1)}`);
      console.log(`File size: ${(blob.size / 1024 / 1024).toFixed(2)} MB`);
      console.log(`Peak buffer usage: ${bufferStats.memoryUsageMB}MB`);
      console.log('==========================');

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
    console.log('VideoRecorder.updateSettings() called with:', newSettings);

    if (this.isRecording) {
      console.warn('Cannot change settings while recording');
      return;
    }

    // Merge new settings with current settings
    this.settings = { ...this.settings, ...newSettings };
    console.log('Updated settings:', this.settings);

    // Update fps if provided
    if (newSettings.fps) {
      this.fps = newSettings.fps;
    }

    // Reselect MIME type and preferred format with new settings
    this.selectedMimeType = this.selectMimeType();
    this.preferredFormat = this.determinePreferredFormat();

    console.log('Video recorder settings updated:', this.settings);
    console.log('New MIME type:', this.selectedMimeType);
    console.log('Preferred format:', this.preferredFormat);
  }

  /**
   * Gets current recording state
   * @returns {boolean} True if currently recording
   */
  getIsRecording() {
    return this.isRecording;
  }

  /**
   * Forces a frame update to trigger canvas content capture - optimized for busy canvas
   * Uses direct animation frame scheduling instead of idle callback to avoid waiting
   */
  requestFrameUpdate() {
    try {
      // Skip idle callback entirely - use immediate frame update for busy canvas scenarios
      // This prevents waiting for idle time that never comes during intensive rendering
      window.requestAnimationFrame(() => {
        console.log('Frame update requested to trigger canvas capture (optimized)');
        this.updateCanvasWhenIdle();
      });
    } catch (e) {
      console.warn('requestFrameUpdate failed:', e);
    }
  }

  /**
   * Updates canvas during recording to ensure continuous capture
   * Optimized to minimize interference with main rendering loop
   */
  updateCanvasWhenIdle() {
    try {
      // Skip redundant canvas updates if main loop is active and fast
      if (window.fastLEDController && window.fastLEDController.getAverageFrameTime) {
        const avgFrameTime = window.fastLEDController.getAverageFrameTime();
        if (avgFrameTime < 20) {
          // Main loop is running smoothly, don't interfere
          return;
        }
      }

      // If we have access to a render function, call it minimally
      if (window.updateCanvas && typeof window.updateCanvas === 'function') {
        // Pass empty frame data with screenMap for canvas refresh during recording
        const emptyFrameData = Object.assign([], {
          screenMap: window.screenMap || undefined
        });
        window.updateCanvas(emptyFrameData);
      }
    } catch (e) {
      console.warn('updateCanvasWhenIdle() call failed:', e);
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
   * Cleanup method to release resources - enhanced for buffer system
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

      // Clean up frame buffer system
      this.frameBuffer = [];
      this.bufferMemoryUsage = 0;
      this.isProcessingBuffer = false;
      this.emergencyBufferMode = false;

      // Terminate frame worker if it exists
      if (this.frameWorker) {
        this.frameWorker.terminate();
        this.frameWorker = null;
      }

      // Clear recorded chunks
      this.recordedChunks = [];

      // Clear references
      this.canvas = null;
      this.audioContext = null;
      this.onStateChange = null;

      console.log('VideoRecorder disposed with buffer cleanup');
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
