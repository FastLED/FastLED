/**
 * FastLED Audio Manager Module
 *
 * Comprehensive audio processing system for FastLED WebAssembly applications.
 * Provides real-time audio analysis, file handling, and integration with LED effects.
 *
 * Key features:
 * - Multiple audio processor support (AudioWorklet and ScriptProcessor)
 * - Real-time audio sample processing and buffering
 * - Audio file upload and playback management
 * - Cross-browser compatibility with fallbacks
 * - High-precision timing and synchronization
 * - Memory-efficient sample storage and retrieval
 * - Debug logging and diagnostics
 * - Automatic processor selection based on browser capabilities
 *
 * Supported audio formats:
 * - MP3, WAV, OGG, AAC (browser-dependent)
 * - Real-time microphone input
 * - HTML audio element playback
 *
 * Processor types:
 * - AudioWorklet: Modern, high-performance (runs on audio thread)
 * - ScriptProcessor: Legacy fallback (runs on main thread)
 *
 * @module AudioManager
 */

/* eslint-disable no-console */
/* eslint-disable no-restricted-syntax */
/* eslint-disable max-len */
/* eslint-disable guard-for-in */

/**
 * Audio sample block size configuration
 * Must match i2s read size on ESP32-C3 for compatibility
 * @constant {number}
 */
const AUDIO_SAMPLE_BLOCK_SIZE = 512;

/**
 * Maximum number of audio buffers to accumulate before cleanup
 * Prevents excessive memory usage during long playback sessions
 * @constant {number}
 */
const MAX_AUDIO_BUFFER_LIMIT = 10;

/**
 * Debug configuration for audio processing
 * Controls logging frequency to prevent console spam while maintaining visibility
 * @constant {Object}
 */
const AUDIO_DEBUG = {
  /** @type {boolean} Enable/disable verbose debugging */
  enabled: false,
  /** @type {number} How often to log sample processing (0.1% of the time) */
  sampleRate: 0.001,
  /** @type {number} How often to log buffer operations (10% of the time) */
  bufferRate: 0.1,
  /** @type {number} How often to log worklet debug messages (0.01% of the time) */
  workletRate: 0.0001,
};

/**
 * Audio processor type constants
 * Defines available audio processing implementations
 * @constant {Object}
 */
const AUDIO_PROCESSOR_TYPES = {
  /** @type {string} Legacy ScriptProcessor (main thread) */
  SCRIPT_PROCESSOR: 'script_processor',
  /** @type {string} Modern AudioWorklet (audio thread) */
  AUDIO_WORKLET: 'audio_worklet',
};

/**
 * TIMESTAMP IMPLEMENTATION DOCUMENTATION:
 *
 * Audio sample timestamps are relative to the start of the audio file, not absolute time.
 * This ensures consistent timing that's meaningful for audio synchronization.
 *
 * Priority order for timestamp sources:
 * 1. audioElement.currentTime - Preferred: gives playback position in audio file (seconds → milliseconds)
 * 2. audioContext.currentTime - Fallback: high-precision audio context time (seconds → milliseconds)
 * 3. performance.now() - Final fallback: high-resolution system time relative to page load
 *
 * This approach ensures that audio-visual synchronization remains accurate regardless
 * of when playback starts or system performance variations.
 */

/**
 * Abstract base class for audio processors
 * Provides a common interface for different audio processing implementations
 * Enables polymorphic usage of ScriptProcessor and AudioWorklet implementations
 */
class AudioProcessor {
  /**
   * Creates a new AudioProcessor instance
   * @param {AudioContext} audioContext - Web Audio API context
   * @param {Function} sampleCallback - Callback function for processed audio samples
   */
  constructor(audioContext, sampleCallback) {
    /** @type {AudioContext} Web Audio API context */
    this.audioContext = audioContext;

    /** @type {Function} Callback for processed audio samples */
    this.sampleCallback = sampleCallback;

    /** @type {boolean} Whether audio processing is currently active */
    this.isProcessing = false;
  }

  /**
   * Initialize the audio processor
   * @abstract
   * @param {MediaElementAudioSourceNode | MediaStreamAudioSourceNode} [_source] - Optional audio source node
   * @returns {Promise<void>}
   */
  initialize(_source) {
    // Base class method - returns rejected promise since it must be implemented by subclass
    return Promise.reject(new Error('initialize() must be implemented by subclass'));
  }

  /**
   * Start audio processing
   */
  start() {
    this.isProcessing = true;
  }

  /**
   * Stop audio processing
   */
  stop() {
    this.isProcessing = false;
  }

  /**
   * Clean up resources
   */
  cleanup() {
    this.stop();
  }

  /**
   * Get the processor type identifier
   * @abstract
   * @returns {string} Processor type string
   */
  getType() {
    throw new Error('getType() must be implemented by subclass');
  }
}

/**
 * ScriptProcessor-based audio processor (legacy but widely supported)
 * Uses the deprecated ScriptProcessorNode for broad browser compatibility
 * Runs on the main thread which can cause performance issues but works everywhere
 */
class ScriptProcessorAudioProcessor extends AudioProcessor {
  /**
   * Creates a new ScriptProcessorAudioProcessor instance
   * @param {AudioContext} audioContext - Web Audio API context
   * @param {Function} sampleCallback - Callback function for processed audio samples
   */
  constructor(audioContext, sampleCallback) {
    super(audioContext, sampleCallback);

    /** @type {ScriptProcessorNode|null} The ScriptProcessor node */
    this.scriptNode = null;

    /** @type {Int16Array} Buffer for converting audio samples to int16 format */
    this.sampleBuffer = new Int16Array(AUDIO_SAMPLE_BLOCK_SIZE);
  }

  /**
   * Initialize the ScriptProcessor node and audio processing chain
   * @param {MediaElementAudioSourceNode | MediaStreamAudioSourceNode} source - Audio source node to connect
   * @returns {Promise<void>}
   */
  initialize(source) {
    // Create script processor node - returns promise for base class compatibility
    this.scriptNode = this.audioContext.createScriptProcessor(AUDIO_SAMPLE_BLOCK_SIZE, 1, 1);

    // Set up audio processing callback
    this.scriptNode.onaudioprocess = (audioProcessingEvent) => {
      if (!this.isProcessing) return;

      // Get input data from the left channel
      const { inputBuffer } = audioProcessingEvent;
      const inputData = inputBuffer.getChannelData(0);

      // Convert float32 audio data to int16 range
      this.convertAudioSamples(inputData, this.sampleBuffer);

      // Get timestamp
      const timestamp = this.getTimestamp();

      // Call the sample callback
      this.sampleCallback(this.sampleBuffer, timestamp);
    };

    // Connect nodes
    source.connect(this.scriptNode);
    this.scriptNode.connect(this.audioContext.destination);

    // Return resolved promise for interface compatibility
    return Promise.resolve();
  }

  /**
   * Convert audio samples from float32 to int16 format
   * @param {Float32Array} inputData - Input audio data in float32 format (-1.0 to 1.0)
   * @param {Int16Array} sampleBuffer - Output buffer for int16 samples (-32768 to 32767)
   */
  convertAudioSamples(inputData, sampleBuffer) {
    for (let i = 0; i < inputData.length; i++) {
      // Convert from float32 (-1.0 to 1.0) to int16 range (-32768 to 32767)
      sampleBuffer[i] = Math.floor(inputData[i] * 32767);
    }
  }

  /**
   * Get current timestamp for audio synchronization
   * @returns {number} Timestamp in milliseconds
   */
  getTimestamp() {
    // Use AudioContext.currentTime as primary source for ScriptProcessor
    return Math.floor(this.audioContext.currentTime * 1000);
  }

  /**
   * Clean up ScriptProcessor resources
   */
  cleanup() {
    super.cleanup();
    if (this.scriptNode) {
      this.scriptNode.onaudioprocess = null;
      this.scriptNode.disconnect();
      this.scriptNode = null;
    }
  }

  /**
   * Get the processor type identifier
   * @returns {string} Processor type
   */
  getType() {
    return AUDIO_PROCESSOR_TYPES.SCRIPT_PROCESSOR;
  }
}

/**
 * AudioWorklet-based audio processor (modern, runs on audio thread)
 * Provides better performance and timing consistency than ScriptProcessor
 */
class AudioWorkletAudioProcessor extends AudioProcessor {
  constructor(audioContext, sampleCallback) {
    super(audioContext, sampleCallback);
    this.workletNode = null;
    this.isWorkletLoaded = false;
    console.log('🎵 AudioWorklet processor created');
  }

  /**
   * Initialize the AudioWorklet processor and audio processing chain
   * @param {MediaElementAudioSourceNode | MediaStreamAudioSourceNode} source - Audio source node to connect
   * @returns {Promise<void>}
   */
  async initialize(source) {
    try {
      // Load the AudioWorklet module if not already loaded
      if (!this.isWorkletLoaded) {
        // Try different possible paths for the AudioWorklet processor
        const possiblePaths = [
          './audio_worklet_processor.js',
          'audio_worklet_processor.js',
          '../audio_worklet_processor.js',
          'src/platforms/wasm/compiler/modules/audio_worklet_processor.js',
        ];

        let loadSuccess = false;
        const diagnosticInfo = [];

        for (const path of possiblePaths) {
          try {
            // deno-lint-ignore no-await-in-loop
            await this.audioContext.audioWorklet.addModule(path);
            console.log(`🎵 ✅ AudioWorklet module loaded successfully from: ${path}`);
            loadSuccess = true;
            break;
          } catch (pathError) {
            // Collect detailed diagnostic information
            const diagnostic = {
              path,
              error: pathError.message,
              errorName: pathError.name,
              errorType: this.diagnoseAudioWorkletError(pathError, path),
            };
            diagnosticInfo.push(diagnostic);

            console.warn(`🎵 ❌ Failed to load AudioWorklet from ${path}:`, pathError.message);
            console.warn(`🎵 🔍 Error type: ${diagnostic.errorType}`);
          }
        }

        // If all paths failed, show diagnostic information
        if (!loadSuccess) {
          console.log('🎵 📊 AudioWorklet Loading Diagnostic Report');
          diagnosticInfo.forEach((info, index) => {
            console.log(`🎵 📁 Attempt ${index + 1}: ${info.path}`);
            console.log(`🎵    Error: ${info.error}`);
            console.log(`🎵    Type: ${info.errorType}`);
          });
          console.log('🎵 💡 Check browser Network tab for specific HTTP status codes');
        }

        if (!loadSuccess) {
          // Provide a summary of the diagnostic information
          const errorTypes = [...new Set(diagnosticInfo.map((d) => d.errorType))];
          const detailedError = new Error(`
🎵 AudioWorklet module could not be loaded from any path.

Diagnosed error types: ${errorTypes.join(', ')}

Run these diagnostic commands in the browser console:
  window.testAudioWorkletPath()              - Test all paths
  window.getAudioWorkletEnvironmentInfo()    - Check environment

Quick fixes to try:
1. Copy audio_worklet_processor.js to the same directory as this page
2. Check browser Network tab for 404 or CORS errors
3. Ensure you're using http:// or https:// (not file://)
4. Verify web server is serving .js files correctly

The system will automatically fall back to ScriptProcessor.`);

          throw detailedError;
        }

        this.isWorkletLoaded = true;
      }

      // Create the AudioWorklet node
      this.workletNode = new AudioWorkletNode(this.audioContext, 'fastled-audio-processor', {
        numberOfInputs: 1,
        numberOfOutputs: 1,
        outputChannelCount: [1],
        processorOptions: {
          sampleRate: this.audioContext.sampleRate,
        },
      });

      // Set up message handling from the worklet
      this.workletNode.port.onmessage = (event) => {
        this.handleWorkletMessage(event.data);
      };

      // Handle worklet errors
      this.workletNode.onprocessorerror = (error) => {
        console.error('🎵 AudioWorklet processor error:', error);
      };

      // Connect nodes: source -> worklet -> destination
      source.connect(this.workletNode);
      this.workletNode.connect(this.audioContext.destination);
    } catch (error) {
      console.error('🎵 Failed to initialize AudioWorklet processor:', error);

      // Provide helpful error messages for common issues
      if (error.name === 'NotSupportedError') {
        console.error('🎵 AudioWorklet is not supported in this browser');
      } else if (error.message.includes('audio_worklet_processor.js')) {
        console.error('🎵 Could not load audio_worklet_processor.js - check file path');
      }

      throw error;
    }
  }

  /**
   * Handle messages from the AudioWorklet
   * @param {Object} data - Message data from worklet
   */
  handleWorkletMessage(data) {
    const { type, samples, timestamp } = data;

    switch (type) {
      case 'audioData':
        if (this.isProcessing && samples && samples.length > 0) {
          // Convert samples array back to Int16Array for compatibility
          const sampleBuffer = new Int16Array(samples);

          // Call the sample callback with enhanced timestamp
          const enhancedTimestamp = this.enhanceTimestamp(timestamp);
          this.sampleCallback(sampleBuffer, enhancedTimestamp);
        }
        break;

      case 'error':
        console.error('🎵 AudioWorklet reported error:', data.message);
        break;

      case 'debug':
        // Only log debug messages when debugging is enabled
        if (AUDIO_DEBUG.enabled && Math.random() < AUDIO_DEBUG.workletRate) {
          console.log('🎵 AudioWorklet debug:', data.message);
        }
        break;

      default:
        console.warn('🎵 Unknown message type from AudioWorklet:', type);
    }
  }

  /**
   * Enhance the timestamp from AudioWorklet with additional context
   * @param {number} workletTimestamp - Timestamp from AudioWorklet (audioContext.currentTime)
   * @returns {number} Enhanced timestamp in milliseconds
   */
  enhanceTimestamp(workletTimestamp) {
    // AudioWorklet provides high-precision AudioContext.currentTime
    // Convert from seconds to milliseconds for consistency
    return Math.floor(workletTimestamp);
  }

  start() {
    super.start();
    if (this.workletNode) {
      console.log('🎵 Starting AudioWorklet processing');
      this.workletNode.port.postMessage({
        type: 'start',
        timestamp: this.audioContext.currentTime,
      });
    }
  }

  stop() {
    super.stop();
    if (this.workletNode) {
      console.log('🎵 Stopping AudioWorklet processing');
      this.workletNode.port.postMessage({
        type: 'stop',
        timestamp: this.audioContext.currentTime,
      });
    }
  }

  /**
   * Send configuration to the AudioWorklet
   * @param {Object} config - Configuration object
   */
  sendConfig(config) {
    if (this.workletNode) {
      console.log('🎵 Sending config to AudioWorklet:', config);
      this.workletNode.port.postMessage({
        type: 'config',
        data: config,
        timestamp: this.audioContext.currentTime,
      });
    }
  }

  cleanup() {
    super.cleanup();

    if (this.workletNode) {
      console.log('🎵 Cleaning up AudioWorklet processor');

      try {
        // Stop processing
        this.workletNode.port.postMessage({ type: 'stop' });

        // Clear message handler
        this.workletNode.port.onmessage = null;
        this.workletNode.onprocessorerror = null;

        // Disconnect the node
        this.workletNode.disconnect();

        console.log('🎵 AudioWorklet cleanup completed');
      } catch (error) {
        console.warn('🎵 Error during AudioWorklet cleanup:', error);
      }

      this.workletNode = null;
    }
  }

  /**
   * Diagnose the type of AudioWorklet loading error
   * @param {Error} error - The error that occurred
   * @param {string} [_path] - Optional path that failed to load
   * @returns {string} Error type description
   */
  diagnoseAudioWorkletError(error, _path) {
    const errorMsg = error.message.toLowerCase();
    const errorName = error.name;

    // Common error patterns and their likely causes
    if (errorMsg.includes('cors') || errorMsg.includes('cross-origin')) {
      return 'CORS_ERROR - Cross-origin request blocked';
    }

    if (errorMsg.includes('404') || errorMsg.includes('not found')) {
      return 'PATH_ERROR - File not found (404)';
    }

    if (errorMsg.includes('network') || errorMsg.includes('fetch')) {
      return 'NETWORK_ERROR - Network request failed';
    }

    if (errorMsg.includes('syntax') || errorMsg.includes('parse')) {
      return 'SYNTAX_ERROR - JavaScript syntax error in worklet file';
    }

    if (errorMsg.includes('security') || errorMsg.includes('insecure')) {
      return 'SECURITY_ERROR - Security restriction (HTTPS required?)';
    }

    if (errorMsg.includes('mime') || errorMsg.includes('content-type')) {
      return 'MIME_ERROR - Incorrect MIME type (should be application/javascript)';
    }

    if (errorName === 'TypeError') {
      return 'TYPE_ERROR - Likely a path resolution or module loading issue';
    }

    if (errorName === 'AbortError') {
      return 'ABORT_ERROR - Request was aborted (timeout or manual cancel)';
    }

    if (errorMsg.includes('unable to load') || errorMsg.includes('failed to load')) {
      return 'LOAD_ERROR - Generic loading failure (check network tab)';
    }

    return `UNKNOWN_ERROR - ${errorName}: Check browser console and network tab`;
  }

  getType() {
    return AUDIO_PROCESSOR_TYPES.AUDIO_WORKLET;
  }
}

/**
 * Factory for creating audio processors
 */
class AudioProcessorFactory {
  /**
   * Create an audio processor of the specified type
   * @param {string} type - Processor type
   * @param {AudioContext} audioContext - Audio context
   * @param {Function} sampleCallback - Callback for audio samples
   * @returns {AudioProcessor}
   */
  static create(type, audioContext, sampleCallback) {
    switch (type) {
      case AUDIO_PROCESSOR_TYPES.SCRIPT_PROCESSOR:
        return new ScriptProcessorAudioProcessor(audioContext, sampleCallback);
      case AUDIO_PROCESSOR_TYPES.AUDIO_WORKLET:
        return new AudioWorkletAudioProcessor(audioContext, sampleCallback);
      default:
        console.warn(`Unknown audio processor type: ${type}, falling back to ScriptProcessor`);
        return new ScriptProcessorAudioProcessor(audioContext, sampleCallback);
    }
  }

  /**
   * Check if AudioWorklet is supported
   * @returns {boolean}
   */
  static isAudioWorkletSupported() {
    return 'audioWorklet' in AudioContext.prototype;
  }

  /**
   * Get the best available processor type
   * Note: This only checks API support, not actual file availability
   * @returns {string}
   */
  static getBestProcessorType() {
    if (this.isAudioWorkletSupported()) {
      return AUDIO_PROCESSOR_TYPES.AUDIO_WORKLET;
    }
    return AUDIO_PROCESSOR_TYPES.SCRIPT_PROCESSOR;
  }

  /**
   * Get a conservative processor type that's most likely to work
   * @returns {string}
   */
  static getReliableProcessorType() {
    // For now, always return ScriptProcessor as it's more reliable
    // Can be changed to return AUDIO_WORKLET when file loading is more robust
    return AUDIO_PROCESSOR_TYPES.SCRIPT_PROCESSOR;
  }
}

/**
 * Audio Manager class to handle audio processing and UI
 */
export class AudioManager {
  /**
   * Initialize the AudioManager and set up global audio data storage
   */
  constructor(processorType = null) {
    // Auto-select the best processor type if not specified
    if (processorType === null) {
      processorType = AudioProcessorFactory.getBestProcessorType();
      console.log(`🎵 Auto-selected audio processor: ${processorType}`);
      console.log(
        '🎵 (Will automatically fallback to ScriptProcessor if AudioWorklet fails to load)',
      );
    }

    this.processorType = processorType;
    this.initializeGlobalAudioData();
  }

  /**
   * Set up the global audio data storage if it doesn't exist
   */
  initializeGlobalAudioData() {
    if (!window.audioData) {
      console.log('Initializing global audio data storage');
      window.audioData = {
        audioContexts: {}, // Store audio contexts by ID
        audioSamples: {}, // Store current audio samples by ID
        audioBuffers: {}, // Store optimized audio buffer storage by ID
        audioProcessors: {}, // Store audio processors by ID
        audioSources: {}, // Store MediaElementSourceNodes by ID
        mediaStreams: {}, // Store MediaStreams for microphone capture by ID
        hasActiveSamples: false,
        frequencyData: new Float32Array(0), // Store frequency analysis data
        timeData: new Float32Array(0), // Store time domain data
        volume: 0 // Store current volume level
      };
    }
  }

  /**
   * Set the processor type for new audio setups
   * @param {string} type - Processor type
   */
  setProcessorType(type) {
    if (Object.values(AUDIO_PROCESSOR_TYPES).includes(type)) {
      this.processorType = type;
      console.log(`🎵 Audio processor type set to: ${type}`);
    } else {
      console.warn(`🎵 Invalid processor type: ${type}`);
    }
  }

  /**
   * Get current processor type
   * @returns {string}
   */
  getProcessorType() {
    return this.processorType;
  }

  /**
   * Check if AudioWorklet is supported in this browser
   * @returns {boolean}
   */
  isAudioWorkletSupported() {
    return AudioProcessorFactory.isAudioWorkletSupported();
  }

  /**
   * Get the best available processor type for this browser
   * @returns {string}
   */
  getBestProcessorType() {
    return AudioProcessorFactory.getBestProcessorType();
  }

  /**
   * Switch to AudioWorklet if supported, otherwise fall back to ScriptProcessor
   * @returns {boolean} True if switched to AudioWorklet, false if fell back to ScriptProcessor
   */
  useAudioWorkletIfSupported() {
    if (this.isAudioWorkletSupported()) {
      this.setProcessorType(AUDIO_PROCESSOR_TYPES.AUDIO_WORKLET);
      return true;
    }
    console.warn('🎵 AudioWorklet not supported, using ScriptProcessor');
    this.setProcessorType(AUDIO_PROCESSOR_TYPES.SCRIPT_PROCESSOR);
    return false;
  }

  /**
   * Get processor capabilities and status
   * @returns {Object} Capabilities object
   */
  getCapabilities() {
    return {
      currentProcessor: this.processorType,
      audioWorkletSupported: this.isAudioWorkletSupported(),
      bestAvailable: this.getBestProcessorType(),
      availableTypes: Object.values(AUDIO_PROCESSOR_TYPES),
    };
  }

  /**
   * Set up audio analysis for a given audio element
   * @param {HTMLAudioElement} audioElement - The audio element to analyze
   * @returns {Promise<Object>} Audio analysis components
   */
  async setupAudioAnalysis(audioElement) {
    try {
      // Create and configure the audio context and nodes
      const audioComponents = await this.createAudioComponents(audioElement);

      // Get the audio element's input ID and store references
      const audioId = audioElement.parentNode.querySelector('input').id;
      this.storeAudioReferences(audioId, audioComponents);

      // Start audio processing
      audioComponents.processor.start();

      // Start audio playback
      this.startAudioPlayback(audioElement);

      if (AUDIO_DEBUG.enabled) {
        console.log(
          `🎵 Audio analysis setup complete for ${audioId} using ${audioComponents.processor.getType()}`,
        );
      }

      return audioComponents;
    } catch (error) {
      console.error('🎵 Failed to setup audio analysis:', error);
      throw error;
    }
  }

  /**
   * Create audio context and processing components with automatic fallback
   * @param {HTMLAudioElement} audioElement - The audio element to analyze
   * @returns {Promise<Object>} Created audio components
   */
  async createAudioComponents(audioElement) {
    // Create audio context with browser compatibility
    const AudioContext = window.AudioContext || window.webkitAudioContext;
    const audioContext = new AudioContext();

    if (AUDIO_DEBUG.enabled) {
      console.log(`🎵 Creating new AudioContext (state: ${audioContext.state})`);
    }

    // Create audio source - handle both file-based and stream-based audio
    let source;
    if (audioElement.srcObject && audioElement.srcObject instanceof MediaStream) {
      // For microphone streams, create MediaStreamAudioSourceNode
      source = audioContext.createMediaStreamSource(audioElement.srcObject);
    } else {
      // For file-based audio, create MediaElementAudioSourceNode
      source = audioContext.createMediaElementSource(audioElement);
      source.connect(audioContext.destination); // Connect to output (only for file-based)
    }

    // Create sample callback for the processor
    const sampleCallback = (sampleBuffer, timestamp) => {
      this.handleAudioSamples(sampleBuffer, timestamp, audioElement);
    };

    // Try to create and initialize the preferred processor with fallback
    let processor = null;

    try {
      // First attempt: Try preferred processor type
      processor = AudioProcessorFactory.create(this.processorType, audioContext, sampleCallback);
      await processor.initialize(source);
      console.log(`🎵 Audio processor initialized: ${processor.getType()}`);
    } catch (processorError) {
      console.warn(`🎵 Failed to initialize ${this.processorType}:`, processorError.message);

      // If AudioWorklet failed, try fallback to ScriptProcessor
      if (this.processorType === AUDIO_PROCESSOR_TYPES.AUDIO_WORKLET) {
        try {
          console.log(`🎵 Falling back to ${AUDIO_PROCESSOR_TYPES.SCRIPT_PROCESSOR}`);
          processor = AudioProcessorFactory.create(
            AUDIO_PROCESSOR_TYPES.SCRIPT_PROCESSOR,
            audioContext,
            sampleCallback,
          );
          await processor.initialize(source);
          console.log(`🎵 Successfully using ${processor.getType()} processor`);

          // Update the AudioManager's processor type for future uses
          this.processorType = AUDIO_PROCESSOR_TYPES.SCRIPT_PROCESSOR;
        } catch (fallbackError) {
          console.error('🎵 Both processors failed:', fallbackError);
          throw new Error(
            `Failed to initialize any audio processor. AudioWorklet error: ${processorError.message}, ScriptProcessor error: ${fallbackError.message}`,
          );
        }
      } else {
        // If ScriptProcessor itself failed, re-throw the error
        throw processorError;
      }
    }

    // Handle media element connection errors separately (these aren't processor-specific)
    if (!processor) {
      const error = new Error('No audio processor could be created');
      console.error('🎵 Failed to create audio components:', error);

      // Check for common connection issues
      if (error.name === 'InvalidStateError' && error.message.includes('already connected')) {
        console.error(
          '🎵 The audio element is still connected to a previous MediaElementSourceNode.',
        );
        console.error('🎵 This usually means the cleanup process did not complete properly.');
        console.error('🎵 Try pausing the audio and waiting a moment before switching tracks.');
      }

      throw error;
    }

    console.log(`🎵 Audio components created successfully using ${processor.getType()}`);

    return {
      audioContext,
      source,
      processor,
    };
  }

  /**
   * Handle audio samples from the processor
   * @param {Int16Array} sampleBuffer - Audio samples
   * @param {number} timestamp - Sample timestamp
   * @param {HTMLAudioElement} audioElement - Audio element
   */
  handleAudioSamples(sampleBuffer, timestamp, audioElement) {
    // Store samples if audio is playing
    if (!audioElement.paused) {
      const audioId = audioElement.parentNode.querySelector('input').id;
      this.storeAudioSamples(sampleBuffer, audioId, audioElement);
      this.updateProcessingIndicator();
    }
  }

  /**
   * Store audio references in the global audio data object
   * @param {string} audioId - The ID of the audio input
   * @param {Object} components - Audio components to store
   */
  storeAudioReferences(audioId, components) {
    window.audioData.audioContexts[audioId] = components.audioContext;
    window.audioData.audioProcessors[audioId] = components.processor;
    window.audioData.audioSources[audioId] = components.source; // Store source for cleanup
    window.audioData.audioBuffers[audioId] = new AudioBufferStorage(audioId);

    // Create a placeholder for current samples (for backward compatibility)
    window.audioData.audioSamples[audioId] = new Int16Array(AUDIO_SAMPLE_BLOCK_SIZE);
  }

  /**
   * Start audio playback and handle errors
   * @param {HTMLAudioElement} audioElement - The audio element to play
   */
  startAudioPlayback(audioElement) {
    audioElement.play().catch((err) => {
      console.error('Error playing audio:', err);
    });
  }

  /**
   * Store a copy of the current audio samples
   * @param {Int16Array} sampleBuffer - Buffer containing current samples
   * @param {string} audioId - The ID of the audio input
   * @param {HTMLAudioElement} audioElement - The audio element (for timestamp)
   */
  storeAudioSamples(sampleBuffer, audioId, audioElement) {
    const bufferCopy = new Int16Array(sampleBuffer);

    // Update the current samples reference for backward compatibility
    if (window.audioData.audioSamples[audioId]) {
      window.audioData.audioSamples[audioId].set(bufferCopy);
    }

    // Get timestamp with priority order
    let timestamp = 0;
    const audioContext = window.audioData.audioContexts[audioId];

    if (audioElement && !audioElement.paused && audioElement.currentTime >= 0) {
      // Use audio element's currentTime (seconds) converted to milliseconds
      timestamp = Math.floor(audioElement.currentTime * 1000);
    } else if (audioContext && audioContext.currentTime >= 0) {
      // Fallback to AudioContext.currentTime
      timestamp = Math.floor(audioContext.currentTime * 1000);
    } else {
      // Final fallback: use performance.now()
      timestamp = Math.floor(performance.now());
    }

    // Debug logging (controlled by AUDIO_DEBUG settings)
    if (AUDIO_DEBUG.enabled && Math.random() < AUDIO_DEBUG.sampleRate) {
      const processor = window.audioData.audioProcessors[audioId];
      const processorType = processor ? processor.getType() : 'unknown';
      console.log(`🎵 Audio ${audioId} (${processorType}): Processing samples`);
    }

    // Use optimized buffer storage system
    const bufferStorage = window.audioData.audioBuffers[audioId];
    bufferStorage.addSamples(bufferCopy, timestamp);
    window.audioData.hasActiveSamples = true;
  }

  /**
   * Update the UI to indicate audio is being processed
   */
  updateProcessingIndicator() {
    const label = document.getElementById('canvas-label');
    if (label) {
      label.textContent = 'Audio: Processing';
      if (!label.classList.contains('show-animation')) {
        label.classList.add('show-animation');
      }
    }
  }

  /**
   * Create an audio field UI element
   * @param {Object} element - Element configuration
   * @returns {HTMLElement} The created audio control
   */
  createAudioField(element) {
    // Create the main container and label
    const controlDiv = this.createControlContainer(element);

    // Create file selection components
    const { uploadButton, micButton, audioInput, buttonContainer } = this.createFileSelectionComponents(element);

    // Set up file selection handler
    this.setupFileSelectionHandler(uploadButton, audioInput, controlDiv);

    // Set up microphone capture handler
    this.setupMicrophoneHandler(micButton, controlDiv);

    // Add components to the container
    controlDiv.appendChild(buttonContainer);
    controlDiv.appendChild(audioInput);

    return controlDiv;
  }

  /**
   * Create the main control container with label
   * @param {Object} element - Element configuration
   * @returns {HTMLElement} The control container
   */
  createControlContainer(element) {
    const controlDiv = document.createElement('div');
    controlDiv.className = 'ui-control audio-control';

    const labelValueContainer = document.createElement('div');
    labelValueContainer.style.display = 'flex';
    labelValueContainer.style.justifyContent = 'space-between';
    labelValueContainer.style.width = '100%';

    const label = document.createElement('label');
    label.textContent = element.name;
    label.htmlFor = `audio-${element.id}`;

    labelValueContainer.appendChild(label);
    controlDiv.appendChild(labelValueContainer);

    return controlDiv;
  }

  /**
   * Create file selection button and input
   * @param {Object} element - Element configuration
   * @returns {Object} The created components
   */
  createFileSelectionComponents(element) {
    // Create button container for both buttons
    const buttonContainer = document.createElement('div');
    buttonContainer.className = 'audio-button-container';
    buttonContainer.style.display = 'flex';
    buttonContainer.style.gap = '8px';
    buttonContainer.style.marginTop = '5px';

    // Create a custom upload button that matches other UI elements
    const uploadButton = document.createElement('button');
    uploadButton.textContent = '📁 Audio File';
    uploadButton.className = 'audio-upload-button';
    uploadButton.id = `upload-btn-${element.id}`;
    uploadButton.title = 'Select audio file from device';

    // Create microphone button
    const micButton = document.createElement('button');
    micButton.textContent = '🎤 Microphone';
    micButton.className = 'audio-mic-button';
    micButton.id = `mic-btn-${element.id}`;
    micButton.title = 'Capture audio from microphone';

    // Hidden file input
    const audioInput = document.createElement('input');
    audioInput.type = 'file';
    audioInput.id = `audio-${element.id}`;
    audioInput.accept = 'audio/*';
    audioInput.style.display = 'none';

    // Connect button to file input
    uploadButton.addEventListener('click', () => {
      audioInput.click();
    });

    // Add buttons to container
    buttonContainer.appendChild(uploadButton);
    buttonContainer.appendChild(micButton);

    return { uploadButton, micButton, audioInput, buttonContainer };
  }

  /**
   * Set up the file selection handler
   * @param {HTMLButtonElement} uploadButton - The upload button
   * @param {HTMLInputElement} audioInput - The file input
   * @param {HTMLElement} controlDiv - The control container
   */
  setupFileSelectionHandler(uploadButton, audioInput, controlDiv) {
    audioInput.addEventListener('change', async (event) => {
      const file = event.target.files[0];
      if (file) {
        try {
          // Create object URL for the selected file
          const url = URL.createObjectURL(file);

          // Update UI to show selected file
          this.updateButtonText(uploadButton, file);

          // Clean up previous audio context BEFORE setting up new audio
          await this.cleanupPreviousAudioContext(audioInput.id);

          // Small delay to ensure cleanup is complete
          await new Promise((resolve) => { setTimeout(resolve, 100); });

          // Set up audio playback with fresh audio element
          const audio = this.createOrUpdateAudioElement(controlDiv);

          // Configure and play the audio
          await this.configureAudioPlayback(audio, url, controlDiv);

          // Add processing indicator
          this.updateAudioProcessingIndicator(controlDiv);
        } catch (error) {
          console.error('🎵 Error during audio file selection:', error);
          // Show error to user
          this.showAudioError(controlDiv, 'Failed to load audio file. Please try again.');
        }
      }
    });
  }

  /**
   * Set up the microphone capture handler
   * @param {HTMLButtonElement} micButton - The microphone button
   * @param {HTMLElement} controlDiv - The control container
   */
  setupMicrophoneHandler(micButton, controlDiv) {
    let isCapturing = false;

    micButton.addEventListener('click', async () => {
      if (!isCapturing) {
        // Start microphone capture
        try {
          await this.startMicrophoneCapture(micButton, controlDiv);
          isCapturing = true;
        } catch (error) {
          console.error('🎤 Failed to start microphone capture:', error);
          this.showAudioError(controlDiv, 'Failed to access microphone. Please check permissions.');
        }
      } else {
        // Stop microphone capture
        await this.stopMicrophoneCapture(micButton, controlDiv);
        isCapturing = false;
      }
    });
  }

  /**
   * Start microphone capture
   * @param {HTMLButtonElement} micButton - The microphone button
   * @param {HTMLElement} controlDiv - The control container
   */
  async startMicrophoneCapture(micButton, controlDiv) {
    try {
      // Request microphone access
      const stream = await navigator.mediaDevices.getUserMedia({
        audio: {
          echoCancellation: true,
          noiseSuppression: true,
          autoGainControl: true,
          sampleRate: 44100
        }
      });

      // Update button state
      micButton.textContent = '🛑 Stop Recording';
      micButton.className = 'audio-mic-button recording';
      micButton.title = 'Stop microphone recording';

      // Get the audio input ID from the container
      const audioInput = controlDiv.querySelector('input[type="file"]');
      const audioId = audioInput ? audioInput.id : 'unknown';

      // Clean up any previous audio context
      await this.cleanupPreviousAudioContext(audioId);

      // Small delay to ensure cleanup is complete
      await new Promise((resolve) => { setTimeout(resolve, 100); });

      // Create audio element for the stream
      const audio = this.createStreamAudioElement(controlDiv, stream);

      // Set up audio processing for the stream
      await this.setupAudioAnalysis(audio);

      // Update UI to show recording state
      this.updateAudioProcessingIndicator(controlDiv);

      // Store the stream for cleanup
      this.storeMediaStream(audioId, stream);

      console.log('🎤 Microphone capture started successfully');
    } catch (error) {
      console.error('🎤 Error starting microphone capture:', error);
      throw error;
    }
  }

  /**
   * Stop microphone capture
   * @param {HTMLButtonElement} micButton - The microphone button
   * @param {HTMLElement} controlDiv - The control container
   */
  async stopMicrophoneCapture(micButton, controlDiv) {
    try {
      // Get the audio input ID from the container
      const audioInput = controlDiv.querySelector('input[type="file"]');
      const audioId = audioInput ? audioInput.id : 'unknown';

      // Stop the media stream
      const stream = this.getStoredMediaStream(audioId);
      if (stream) {
        stream.getTracks().forEach(track => track.stop());
        this.clearStoredMediaStream(audioId);
      }

      // Clean up audio context
      await this.cleanupPreviousAudioContext(audioId);

      // Remove audio element
      const existingAudio = controlDiv.querySelector('audio');
      if (existingAudio) {
        existingAudio.pause();
        existingAudio.srcObject = null;
        controlDiv.removeChild(existingAudio);
      }

      // Reset button state
      micButton.textContent = '🎤 Microphone';
      micButton.className = 'audio-mic-button';
      micButton.title = 'Capture audio from microphone';

      // Remove processing indicator
      const existingIndicator = controlDiv.querySelector('.audio-indicator');
      if (existingIndicator) {
        controlDiv.removeChild(existingIndicator);
      }

      console.log('🎤 Microphone capture stopped');
    } catch (error) {
      console.error('🎤 Error stopping microphone capture:', error);
    }
  }

  /**
   * Create an audio element for the media stream
   * @param {HTMLElement} container - The control container
   * @param {MediaStream} stream - The media stream
   * @returns {HTMLAudioElement} The audio element
   */
  createStreamAudioElement(container, stream) {
    // Remove any existing audio element first
    const existingAudio = container.querySelector('audio');
    if (existingAudio) {
      existingAudio.pause();
      existingAudio.srcObject = null;
      container.removeChild(existingAudio);
    }

    // Create new audio element for the stream
    const audio = document.createElement('audio');
    audio.controls = false; // Hide controls for microphone stream
    audio.muted = true; // Mute to prevent feedback
    audio.className = 'audio-player stream';
    audio.srcObject = stream;

    // Get the audio input ID from the container
    const audioInput = container.querySelector('input[type="file"]');
    const audioId = audioInput ? audioInput.id : 'unknown';
    audio.setAttribute('data-audio-id', audioId);

    container.appendChild(audio);

    // Start playing the stream (muted)
    audio.play().catch(err => {
      console.warn('🎤 Could not auto-play stream (this is normal):', err);
    });

    return audio;
  }

  /**
   * Store a media stream for later cleanup
   * @param {string} audioId - The audio ID
   * @param {MediaStream} stream - The media stream
   */
  storeMediaStream(audioId, stream) {
    if (!window.audioData.mediaStreams) {
      window.audioData.mediaStreams = {};
    }
    window.audioData.mediaStreams[audioId] = stream;
  }

  /**
   * Get a stored media stream
   * @param {string} audioId - The audio ID
   * @returns {MediaStream|null} The stored stream or null
   */
  getStoredMediaStream(audioId) {
    return window.audioData.mediaStreams?.[audioId] || null;
  }

  /**
   * Clear a stored media stream
   * @param {string} audioId - The audio ID
   */
  clearStoredMediaStream(audioId) {
    if (window.audioData.mediaStreams?.[audioId]) {
      delete window.audioData.mediaStreams[audioId];
    }
  }

  /**
   * Update button text to show selected file name
   * @param {HTMLButtonElement} button - The upload button
   * @param {File} file - The selected audio file
   */
  updateButtonText(button, file) {
    button.textContent = file.name.length > 20 ? `${file.name.substring(0, 17)}...` : file.name;
  }

  /**
   * Create or update the audio element
   * @param {HTMLElement} container - The control container
   * @returns {HTMLAudioElement} The audio element
   */
  createOrUpdateAudioElement(container) {
    // Get the audio input ID from the container
    const audioInput = container.querySelector('input[type="file"]');
    const audioId = audioInput ? audioInput.id : 'unknown';

    // Remove any existing audio element first
    const existingAudio = container.querySelector('audio');
    if (existingAudio) {
      existingAudio.pause();
      existingAudio.currentTime = 0;
      existingAudio.src = '';
      existingAudio.load();
      container.removeChild(existingAudio);
    }

    // Always create a fresh audio element
    const audio = document.createElement('audio');
    audio.controls = true;
    audio.className = 'audio-player';
    audio.setAttribute('data-audio-id', audioId); // Track which input this belongs to
    container.appendChild(audio);

    return audio;
  }

  /**
   * Clean up any previous audio context and buffer storage
   * @param {string} inputId - The ID of the audio input
   * @returns {Promise<void>}
   */
  async cleanupPreviousAudioContext(inputId) {
    if (AUDIO_DEBUG.enabled) {
      console.log(`🎵 Starting cleanup for ${inputId}`);
    }

    // Clean up audio processor first (this disconnects nodes)
    if (window.audioData?.audioProcessors?.[inputId]) {
      const processor = window.audioData.audioProcessors[inputId];
      if (AUDIO_DEBUG.enabled) {
        console.log(`🎵 Cleaning up ${processor.getType()} processor for ${inputId}`);
      }
      processor.cleanup();
      delete window.audioData.audioProcessors[inputId];
    }

    // Clean up MediaElementSourceNode
    if (window.audioData?.audioSources?.[inputId]) {
      try {
        const source = window.audioData.audioSources[inputId];
        source.disconnect();
      } catch (e) {
        console.warn('Error disconnecting MediaElementSourceNode:', e);
      }
      delete window.audioData.audioSources[inputId];
    }

    // Clean up audio context and wait for it to close
    if (window.audioData?.audioContexts?.[inputId]) {
      try {
        const context = window.audioData.audioContexts[inputId];
        await context.close();
      } catch (e) {
        console.warn('Error closing previous audio context:', e);
      }
      delete window.audioData.audioContexts[inputId];
    }

    // Clean up buffer storage with proper memory cleanup
    if (window.audioData?.audioBuffers?.[inputId]) {
      const bufferStorage = window.audioData.audioBuffers[inputId];
      bufferStorage.clear();
      delete window.audioData.audioBuffers[inputId];
    }

    // Clean up sample references
    if (window.audioData?.audioSamples?.[inputId]) {
      delete window.audioData.audioSamples[inputId];
    }

    // Clean up media streams
    if (window.audioData?.mediaStreams?.[inputId]) {
      const stream = window.audioData.mediaStreams[inputId];
      stream.getTracks().forEach(track => track.stop());
      delete window.audioData.mediaStreams[inputId];
    }

    // Clean up any lingering audio elements in the DOM that might be associated with this ID
    const audioElements = document.querySelectorAll(
      `#audio-${inputId}, audio[data-audio-id="${inputId}"]`,
    );
    audioElements.forEach(/** @param {HTMLAudioElement} audio */(audio) => {
      audio.pause();
      audio.src = '';
      audio.load();
    });
  }

  /**
   * Configure and play the audio
   * @param {HTMLAudioElement} audio - The audio element
   * @param {string} url - The audio file URL
   * @param {HTMLElement} container - The control container
   */
  async configureAudioPlayback(audio, url, container) {
    // Set source and loop
    audio.src = url;
    audio.loop = true;

    try {
      // Initialize audio analysis before playing
      await this.setupAudioAnalysis(audio);

      // Try to play the audio (may be blocked by browser policies)
      await audio.play();
    } catch (err) {
      console.error('🎵 Error during audio playback setup:', err);
      this.createFallbackPlayButton(audio, container);
      throw err; // Re-throw so the caller can handle it
    }
  }

  /**
   * Create a fallback play button when autoplay is blocked
   * @param {HTMLAudioElement} audio - The audio element
   * @param {HTMLElement} container - The control container
   */
  createFallbackPlayButton(audio, container) {
    const playButton = document.createElement('button');
    playButton.textContent = 'Play Audio';
    playButton.className = 'audio-play-button';
    playButton.onclick = () => {
      audio.play();
    };
    container.appendChild(playButton);
  }

  /**
   * Update the audio processing indicator
   * @param {HTMLElement} container - The control container
   */
  updateAudioProcessingIndicator(container) {
    // Create new indicator
    const audioIndicator = document.createElement('div');
    audioIndicator.className = 'audio-indicator';
    audioIndicator.textContent = 'Audio samples ready';

    // Replace any existing indicator
    const existingIndicator = container.querySelector('.audio-indicator');
    if (existingIndicator) {
      container.removeChild(existingIndicator);
    }

    container.appendChild(audioIndicator);
  }

  /**
   * Show an error message in the audio control container
   * @param {HTMLElement} container - The control container
   * @param {string} message - Error message to display
   */
  showAudioError(container, message) {
    // Create error indicator
    const errorIndicator = document.createElement('div');
    errorIndicator.className = 'audio-error';
    errorIndicator.textContent = message;
    errorIndicator.style.color = 'red';
    errorIndicator.style.fontSize = '12px';
    errorIndicator.style.marginTop = '5px';

    // Replace any existing indicator
    const existingIndicator = container.querySelector('.audio-indicator, .audio-error');
    if (existingIndicator) {
      container.removeChild(existingIndicator);
    }

    container.appendChild(errorIndicator);
  }
}

/**
 * Create a global instance of AudioManager
 */
const audioManager = new AudioManager();

/**
 * Make setupAudioAnalysis available globally
 * @param {HTMLAudioElement} audioElement - The audio element to analyze
 * @returns {Promise<Object>} Audio analysis components
 */
window.setupAudioAnalysis = async function (audioElement) {
  return await audioManager.setupAudioAnalysis(audioElement);
};

/**
 * Get audio processor capabilities and status (debugging utility)
 * @returns {Object} Capabilities and status information
 */
window.getAudioCapabilities = function () {
  const capabilities = audioManager.getCapabilities();
  console.log('🎵 Audio Engine Capabilities:', capabilities);
  return capabilities;
};

/**
 * Switch audio processor type (debugging utility)
 * @param {string} type - Processor type ('script_processor' or 'audio_worklet')
 * @returns {boolean} True if switch was successful
 */
window.setAudioProcessor = function (type) {
  try {
    audioManager.setProcessorType(type);
    console.log(`🎵 Audio processor switched to: ${type}`);
    return true;
  } catch (error) {
    console.error('🎵 Failed to switch audio processor:', error);
    return false;
  }
};

/**
 * Use the best available audio processor (debugging utility)
 * @returns {string} The processor type that was selected
 */
window.useBestAudioProcessor = function () {
  const isWorklet = audioManager.useAudioWorkletIfSupported();
  const selected = audioManager.getProcessorType();
  console.log(`🎵 Selected best audio processor: ${selected} (AudioWorklet: ${isWorklet})`);
  return selected;
};

/**
 * Force AudioWorklet mode (for testing - will fallback automatically if it fails)
 * @returns {string} The processor type that was set
 */
window.forceAudioWorklet = function () {
  audioManager.setProcessorType(AUDIO_PROCESSOR_TYPES.AUDIO_WORKLET);
  console.log('🎵 Forced AudioWorklet mode (with automatic ScriptProcessor fallback)');
  return audioManager.getProcessorType();
};

/**
 * Force ScriptProcessor mode (for compatibility testing)
 * @returns {string} The processor type that was set
 */
window.forceScriptProcessor = function () {
  audioManager.setProcessorType(AUDIO_PROCESSOR_TYPES.SCRIPT_PROCESSOR);
  console.log('🎵 Forced ScriptProcessor mode');
  return audioManager.getProcessorType();
};

/**
 * Enable audio debug logging (for troubleshooting)
 * @param {boolean} enabled - Whether to enable debug logging
 */
window.setAudioDebug = function (enabled = true) {
  AUDIO_DEBUG.enabled = enabled;
  console.log(`🎵 Audio debug logging ${enabled ? 'enabled' : 'disabled'}`);
  return AUDIO_DEBUG.enabled;
};

/**
 * Get current audio debug settings
 * @returns {Object} Current debug configuration
 */
window.getAudioDebugSettings = function () {
  console.log('🎵 Audio Debug Settings:', AUDIO_DEBUG);
  return AUDIO_DEBUG;
};

/**
 * Test AudioWorklet path loading (diagnostic tool)
 * @param {string} customPath - Optional custom path to test
 * @returns {Promise<boolean>} True if path loads successfully
 */
window.testAudioWorkletPath = async function (customPath = null) {
  console.log('🎵 🔍 Testing AudioWorklet Path Loading...');

  const AudioContext = window.AudioContext || window.webkitAudioContext;
  const testContext = new AudioContext();

  const pathsToTest = customPath ? [customPath] : [
    './audio_worklet_processor.js',
    'audio_worklet_processor.js',
    '../audio_worklet_processor.js',
    'src/platforms/wasm/compiler/modules/audio_worklet_processor.js',
  ];

  for (const path of pathsToTest) {
    try {
      console.log(`🎵 🧪 Testing path: ${path}`);

      // First, try a simple fetch to see if the file exists
      try {
        // deno-lint-ignore no-await-in-loop
        const fetchResponse = await fetch(path);
        console.log(`🎵    📡 Fetch status: ${fetchResponse.status} ${fetchResponse.statusText}`);

        if (!fetchResponse.ok) {
          console.log(`🎵    ❌ Fetch failed: HTTP ${fetchResponse.status}`);
          continue;
        }

        console.log('🎵    ✅ File exists and is accessible');
      } catch (fetchError) {
        console.log(`🎵    ❌ Fetch error: ${fetchError.message}`);
        continue;
      }

      // Now try loading as AudioWorklet module
      // deno-lint-ignore no-await-in-loop
      await testContext.audioWorklet.addModule(path);
      console.log('🎵    🎵 ✅ AudioWorklet module loaded successfully!');

      testContext.close();
      return true;
    } catch (error) {
      console.log(`🎵    🎵 ❌ AudioWorklet loading failed: ${error.message}`);
    }
  }

  console.log('🎵 💡 Check browser Network tab for 404s or CORS errors');
  testContext.close();
  return false;
};

/**
 * Get information about the current page and potential AudioWorklet issues
 * @returns {Object} Environment information
 */
window.getAudioWorkletEnvironmentInfo = function () {
  const info = {
    currentURL: window.location.href,
    protocol: window.location.protocol,
    host: window.location.host,
    pathname: window.location.pathname,
    isSecureContext: self.isSecureContext,
    audioWorkletSupported: 'audioWorklet'
      in (window.AudioContext || window.webkitAudioContext).prototype,
    userAgent: navigator.userAgent,
  };

  console.log('🎵 🌍 AudioWorklet Environment Information');
  console.log('🎵 Current URL:', info.currentURL);
  console.log('🎵 Protocol:', info.protocol);
  console.log('🎵 AudioWorklet API Supported:', info.audioWorkletSupported);

  // Check for common issues
  if (info.protocol === 'file:') {
    console.log('🎵 ⚠️  Running from file:// protocol - AudioWorklet may not work');
    console.log('🎵 💡 Solution: Use a local web server (python -m http.server, etc.)');
  }

  if (!info.audioWorkletSupported) {
    console.log('🎵 ❌ AudioWorklet API not supported in this browser');
  }

  return info;
};

/**
 * Get audio buffer statistics for all active audio inputs (debugging)
 * @returns {Object} Statistics for all audio buffers
 */
window.getAudioBufferStats = function () {
  if (!window.audioData || !window.audioData.audioBuffers) {
    return { error: 'No audio data available' };
  }

  const stats = {};
  for (const [audioId, bufferStorage] of Object.entries(window.audioData.audioBuffers)) {
    if (bufferStorage && typeof bufferStorage.getStats === 'function') {
      stats[audioId] = bufferStorage.getStats();
    }
  }

  // Calculate totals
  const totals = Object.values(stats).reduce((acc, stat) => ({
    totalBufferCount: acc.totalBufferCount + stat.bufferCount,
    totalSamples: acc.totalSamples + stat.totalSamples,
    totalMemoryKB: acc.totalMemoryKB + stat.memoryEstimateKB,
    activeStreams: acc.activeStreams + 1,
  }), {
    totalBufferCount: 0, totalSamples: 0, totalMemoryKB: 0, activeStreams: 0,
  });

  return {
    individual: stats,
    totals,
    limit: {
      maxBuffers: MAX_AUDIO_BUFFER_LIMIT,
      description:
        `Limited to ${MAX_AUDIO_BUFFER_LIMIT} audio buffers to prevent accumulation during engine freezes`,
    },
  };
};

/**
 * Audio Buffer Storage Manager
 * Simple buffer storage with limiting to prevent accumulation during engine freezes
 */
class AudioBufferStorage {
  constructor(audioId) {
    this.audioId = audioId;
    this.buffers = []; // Simple array of audio buffers
    this.totalSamples = 0;
  }

  /**
   * Add audio samples to the buffer with automatic limiting
   * @param {Int16Array} sampleBuffer - Raw audio samples
   * @param {number} timestamp - Sample timestamp
   */
  addSamples(sampleBuffer, timestamp) {
    // Enforce buffer limit to prevent excessive accumulation during engine freezes
    while (this.buffers.length >= MAX_AUDIO_BUFFER_LIMIT) {
      const removedBuffer = this.buffers.shift();
      this.totalSamples -= removedBuffer.samples.length;
      // Only log buffer drops when debugging is enabled or occasionally for important info
      if (AUDIO_DEBUG.enabled && Math.random() < AUDIO_DEBUG.bufferRate) {
        console.log(
          `🎵 Audio ${this.audioId}: Dropping oldest buffer (limit: ${MAX_AUDIO_BUFFER_LIMIT})`,
        );
      }
    }

    // Add new buffer
    this.buffers.push({
      samples: Array.from(sampleBuffer), // Convert to regular array for JSON serialization
      timestamp,
    });
    this.totalSamples += sampleBuffer.length;
  }

  /**
   * Get all buffered samples in the format expected by the backend
   * @returns {Array} Array of sample objects with timestamp
   */
  getAllSamples() {
    return this.buffers.map((buffer) => ({
      samples: buffer.samples,
      timestamp: buffer.timestamp,
    }));
  }

  /**
   * Get the current number of buffered blocks
   * @returns {number} Number of audio blocks
   */
  getBufferCount() {
    return this.buffers.length;
  }

  /**
   * Get total number of samples across all buffers
   * @returns {number} Total sample count
   */
  getTotalSamples() {
    return this.totalSamples;
  }

  /**
   * Clear all buffered data
   */
  clear() {
    this.buffers = [];
    this.totalSamples = 0;
  }

  /**
   * Get storage statistics for debugging
   * @returns {Object} Storage statistics
   */
  getStats() {
    return {
      bufferCount: this.getBufferCount(),
      totalSamples: this.totalSamples,
      storageType: 'simple',
      memoryEstimateKB: this._estimateMemoryUsage(),
    };
  }

  /**
   * Estimate memory usage in KB
   * @private
   */
  _estimateMemoryUsage() {
    // Regular array = ~8 bytes per sample + object overhead
    return ((this.totalSamples * 8) + (this.buffers.length * 50)) / 1024;
  }
}
