/* eslint-disable no-console */
/* eslint-disable import/prefer-default-export */
/* eslint-disable no-restricted-syntax */
/* eslint-disable max-len */
/* eslint-disable guard-for-in */

/**
 * Configuration constants
 */
const AUDIO_SAMPLE_BLOCK_SIZE = 512;  // Matches i2s read size on esp32c3
const MAX_AUDIO_BUFFER_LIMIT = 10;    // Maximum number of audio buffers to accumulate

/**
 * Audio processor types
 */
const AUDIO_PROCESSOR_TYPES = {
  SCRIPT_PROCESSOR: 'script_processor',
  AUDIO_WORKLET: 'audio_worklet'
};

// Default processor type - can be easily changed here
const DEFAULT_PROCESSOR_TYPE = AUDIO_PROCESSOR_TYPES.SCRIPT_PROCESSOR;

/**
 * TIMESTAMP IMPLEMENTATION:
 * Audio sample timestamps are relative to the start of the audio file, not absolute time.
 * Priority order:
 * 1. audioElement.currentTime - Preferred: gives playback position in audio file (seconds → milliseconds)
 * 2. audioContext.currentTime - Fallback: high-precision audio context time (seconds → milliseconds)  
 * 3. performance.now() - Final fallback: high-resolution system time relative to page load
 * This ensures consistent timing that's meaningful for audio synchronization.
 */

/**
 * Abstract base class for audio processors
 * Provides a common interface for different audio processing implementations
 */
class AudioProcessor {
  constructor(audioContext, sampleCallback) {
    this.audioContext = audioContext;
    this.sampleCallback = sampleCallback;
    this.isProcessing = false;
  }

  /**
   * Initialize the audio processor
   * @param {MediaElementAudioSourceNode} source - Audio source node
   * @returns {Promise<void>}
   */
  async initialize(source) {
    throw new Error('initialize() must be implemented by subclass');
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
   * Get the processor type
   * @returns {string}
   */
  getType() {
    throw new Error('getType() must be implemented by subclass');
  }
}

/**
 * ScriptProcessor-based audio processor (legacy but widely supported)
 */
class ScriptProcessorAudioProcessor extends AudioProcessor {
  constructor(audioContext, sampleCallback) {
    super(audioContext, sampleCallback);
    this.scriptNode = null;
    this.sampleBuffer = new Int16Array(AUDIO_SAMPLE_BLOCK_SIZE);
  }

  async initialize(source) {
    // Create script processor node
    this.scriptNode = this.audioContext.createScriptProcessor(AUDIO_SAMPLE_BLOCK_SIZE, 1, 1);
    
    // Set up audio processing callback
    this.scriptNode.onaudioprocess = (audioProcessingEvent) => {
      if (!this.isProcessing) return;
      
      // Get input data from the left channel
      const inputBuffer = audioProcessingEvent.inputBuffer;
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
  }

  convertAudioSamples(inputData, sampleBuffer) {
    for (let i = 0; i < inputData.length; i++) {
      // Convert from float32 (-1.0 to 1.0) to int16 range (-32768 to 32767)
      sampleBuffer[i] = Math.floor(inputData[i] * 32767);
    }
  }

  getTimestamp() {
    // Use AudioContext.currentTime as primary source for ScriptProcessor
    return Math.floor(this.audioContext.currentTime * 1000);
  }

  cleanup() {
    super.cleanup();
    if (this.scriptNode) {
      this.scriptNode.onaudioprocess = null;
      this.scriptNode.disconnect();
      this.scriptNode = null;
    }
  }

  getType() {
    return AUDIO_PROCESSOR_TYPES.SCRIPT_PROCESSOR;
  }
}

/**
 * AudioWorklet-based audio processor (modern, runs on audio thread)
 * Currently contains stubs - not fully implemented yet
 */
class AudioWorkletAudioProcessor extends AudioProcessor {
  constructor(audioContext, sampleCallback) {
    super(audioContext, sampleCallback);
    this.workletNode = null;
    this.isWorkletLoaded = false;
    console.warn('🎵 AudioWorklet is not implemented yet!');
  }

  async initialize(source) {
    console.warn('🎵 AudioWorklet is not implemented yet!');
    
    // Stub implementation - just connect source to destination for passthrough
    source.connect(this.audioContext.destination);
    
    // Log that this is a stub
    console.log('🎵 AudioWorklet stub: Using passthrough connection (no processing)');
  }

  start() {
    super.start();
    console.warn('🎵 AudioWorklet is not implemented yet!');
    console.log('🎵 AudioWorklet stub: Processing "started" (no actual processing)');
  }

  stop() {
    super.stop();
    console.warn('🎵 AudioWorklet is not implemented yet!');
    console.log('🎵 AudioWorklet stub: Processing "stopped" (no actual processing)');
  }

  cleanup() {
    super.cleanup();
    console.warn('🎵 AudioWorklet is not implemented yet!');
    console.log('🎵 AudioWorklet stub: Cleanup complete (no actual cleanup needed)');
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
   * @returns {string}
   */
  static getBestProcessorType() {
    if (this.isAudioWorkletSupported()) {
      return AUDIO_PROCESSOR_TYPES.AUDIO_WORKLET;
    }
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
  constructor(processorType = DEFAULT_PROCESSOR_TYPE) {
    this.processorType = processorType;
    this.initializeGlobalAudioData();
  }
  
  /**
   * Set up the global audio data storage if it doesn't exist
   */
  initializeGlobalAudioData() {
    if (!window.audioData) {
      console.log("Initializing global audio data storage");
      window.audioData = {
        audioContexts: {},  // Store audio contexts by ID
        audioSamples: {},   // Store current audio samples by ID
        audioBuffers: {},   // Store optimized audio buffer storage by ID
        audioProcessors: {},// Store audio processors by ID
        hasActiveSamples: false
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
   * Set up audio analysis for a given audio element
   * @param {HTMLAudioElement} audioElement - The audio element to analyze
   * @returns {Object} Audio analysis components
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
      
      console.log(`🎵 Audio analysis setup complete for ${audioId} using ${audioComponents.processor.getType()}`);
      
      return audioComponents;
    } catch (error) {
      console.error('🎵 Failed to setup audio analysis:', error);
      throw error;
    }
  }
  
  /**
   * Create audio context and processing components
   * @param {HTMLAudioElement} audioElement - The audio element to analyze
   * @returns {Object} Created audio components
   */
  async createAudioComponents(audioElement) {
    // Create audio context with browser compatibility
    const AudioContext = window.AudioContext || window.webkitAudioContext;
    const audioContext = new AudioContext();
    
    // Create audio source
    const source = audioContext.createMediaElementSource(audioElement);
    source.connect(audioContext.destination); // Connect to output
    
    // Create sample callback for the processor
    const sampleCallback = (sampleBuffer, timestamp) => {
      this.handleAudioSamples(sampleBuffer, timestamp, audioElement);
    };
    
    // Create audio processor using the factory
    const processor = AudioProcessorFactory.create(this.processorType, audioContext, sampleCallback);
    
    // Initialize the processor
    await processor.initialize(source);
    
    return {
      audioContext,
      source,
      processor
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
    window.audioData.audioBuffers[audioId] = new AudioBufferStorage(audioId);
    
    // Create a placeholder for current samples (for backward compatibility)
    window.audioData.audioSamples[audioId] = new Int16Array(AUDIO_SAMPLE_BLOCK_SIZE);
  }
  
  /**
   * Start audio playback and handle errors
   * @param {HTMLAudioElement} audioElement - The audio element to play
   */
  startAudioPlayback(audioElement) {
    audioElement.play().catch(err => {
      console.error("Error playing audio:", err);
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

    // Debug logging (sample ~1% of audio blocks to avoid console spam)
    if (Math.random() < 0.01) {
      const processor = window.audioData.audioProcessors[audioId];
      const processorType = processor ? processor.getType() : 'unknown';
      const timestampSource = audioElement && !audioElement.paused && audioElement.currentTime >= 0 
        ? 'audioElement.currentTime' 
        : audioContext && audioContext.currentTime >= 0 
        ? 'audioContext.currentTime' 
        : 'performance.now()';
      console.log(`🎵 Audio ${audioId} (${processorType}): ${timestamp}ms (source: ${timestampSource})`);
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
    const { uploadButton, audioInput } = this.createFileSelectionComponents(element);
    
    // Set up file selection handler
    this.setupFileSelectionHandler(uploadButton, audioInput, controlDiv);
    
    // Add components to the container
    controlDiv.appendChild(uploadButton);
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
    // Create a custom upload button that matches other UI elements
    const uploadButton = document.createElement('button');
    uploadButton.textContent = 'Select Audio File';
    uploadButton.className = 'audio-upload-button';
    uploadButton.id = `upload-btn-${element.id}`;
    
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
    
    return { uploadButton, audioInput };
  }
  
  /**
   * Set up the file selection handler
   * @param {HTMLButtonElement} uploadButton - The upload button
   * @param {HTMLInputElement} audioInput - The file input
   * @param {HTMLElement} controlDiv - The control container
   */
  setupFileSelectionHandler(uploadButton, audioInput, controlDiv) {
    audioInput.addEventListener('change', (event) => {
      const file = event.target.files[0];
      if (file) {
        // Create object URL for the selected file
        const url = URL.createObjectURL(file);
        
        // Update UI to show selected file
        this.updateButtonText(uploadButton, file);
        
        // Set up audio playback
        const audio = this.createOrUpdateAudioElement(controlDiv);
        this.cleanupPreviousAudioContext(audioInput.id);
        
        // Configure and play the audio
        this.configureAudioPlayback(audio, url, controlDiv);
        
        // Add processing indicator
        this.updateAudioProcessingIndicator(controlDiv);
      }
    });
  }
  
  /**
   * Update button text to show selected file name
   * @param {HTMLButtonElement} button - The upload button
   * @param {File} file - The selected audio file
   */
  updateButtonText(button, file) {
    button.textContent = file.name.length > 20 
      ? file.name.substring(0, 17) + '...' 
      : file.name;
  }
  
  /**
   * Create or update the audio element
   * @param {HTMLElement} container - The control container
   * @returns {HTMLAudioElement} The audio element
   */
  createOrUpdateAudioElement(container) {
    let audio = container.querySelector('audio');
    if (!audio) {
      audio = document.createElement('audio');
      audio.controls = true;
      audio.className = 'audio-player';
      container.appendChild(audio);
    }
    return audio;
  }
  
  /**
   * Clean up any previous audio context and buffer storage
   * @param {string} inputId - The ID of the audio input
   */
  cleanupPreviousAudioContext(inputId) {
    // Clean up audio processor
    if (window.audioData?.audioProcessors?.[inputId]) {
      const processor = window.audioData.audioProcessors[inputId];
      console.log(`🎵 Cleaning up ${processor.getType()} processor for ${inputId}`);
      processor.cleanup();
      delete window.audioData.audioProcessors[inputId];
    }
    
    // Clean up audio context
    if (window.audioData?.audioContexts?.[inputId]) {
      try {
        window.audioData.audioContexts[inputId].close();
      } catch (e) {
        console.warn('Error closing previous audio context:', e);
      }
      delete window.audioData.audioContexts[inputId];
    }
    
    // Clean up buffer storage with proper memory cleanup
    if (window.audioData?.audioBuffers?.[inputId]) {
      const bufferStorage = window.audioData.audioBuffers[inputId];
      const stats = bufferStorage.getStats();
      bufferStorage.clear();
      delete window.audioData.audioBuffers[inputId];
    }
    
    // Clean up sample references
    if (window.audioData?.audioSamples?.[inputId]) {
      delete window.audioData.audioSamples[inputId];
    }
  }
  
  /**
   * Configure and play the audio
   * @param {HTMLAudioElement} audio - The audio element
   * @param {string} url - The audio file URL
   * @param {HTMLElement} container - The control container
   */
  configureAudioPlayback(audio, url, container) {
    // Set source and loop
    audio.src = url;
    audio.loop = true;
    
    // Initialize audio analysis before playing
    const analysisSetup = this.setupAudioAnalysis(audio);
    
    // Try to play the audio (may be blocked by browser policies)
    audio.play().then(() => {
      console.log("Audio playback started successfully");
    }).catch(err => {
      console.error("Error starting audio playback:", err);
      this.createFallbackPlayButton(audio, container);
    });
  }
  
  /**
   * Create a fallback play button when autoplay is blocked
   * @param {HTMLAudioElement} audio - The audio element
   * @param {HTMLElement} container - The control container
   */
  createFallbackPlayButton(audio, container) {
    const playButton = document.createElement('button');
    playButton.textContent = "Play Audio";
    playButton.className = "audio-play-button";
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
}

/**
 * Create a global instance of AudioManager
 */
const audioManager = new AudioManager();

/**
 * Make setupAudioAnalysis available globally
 * @param {HTMLAudioElement} audioElement - The audio element to analyze
 * @returns {Object} Audio analysis components
 */
window.setupAudioAnalysis = function(audioElement) {
  return audioManager.setupAudioAnalysis(audioElement);
};

/**
 * Get audio buffer statistics for all active audio inputs (debugging)
 * @returns {Object} Statistics for all audio buffers
 */
window.getAudioBufferStats = function() {
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
    activeStreams: acc.activeStreams + 1
  }), { totalBufferCount: 0, totalSamples: 0, totalMemoryKB: 0, activeStreams: 0 });
  
  return {
    individual: stats,
    totals: totals,
    limit: {
      maxBuffers: MAX_AUDIO_BUFFER_LIMIT,
      description: `Limited to ${MAX_AUDIO_BUFFER_LIMIT} audio buffers to prevent accumulation during engine freezes`
    }
  };
};

/**
 * Audio Buffer Storage Manager
 * Simple buffer storage with limiting to prevent accumulation during engine freezes
 */
class AudioBufferStorage {
  constructor(audioId) {
    this.audioId = audioId;
    this.buffers = [];  // Simple array of audio buffers
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
      console.log(`*** Audio ${this.audioId}: Dropping oldest buffer (limit: ${MAX_AUDIO_BUFFER_LIMIT}) ***`);
    }
    
    // Add new buffer
    this.buffers.push({
      samples: Array.from(sampleBuffer), // Convert to regular array for JSON serialization
      timestamp: timestamp
    });
    this.totalSamples += sampleBuffer.length;
  }

  /**
   * Get all buffered samples in the format expected by the backend
   * @returns {Array} Array of sample objects with timestamp
   */
  getAllSamples() {
    return this.buffers.map(buffer => ({
      samples: buffer.samples,
      timestamp: buffer.timestamp
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
      memoryEstimateKB: this._estimateMemoryUsage()
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
