/* eslint-disable no-console */
/* eslint-disable import/prefer-default-export */
/* eslint-disable no-restricted-syntax */
/* eslint-disable max-len */
/* eslint-disable guard-for-in */

/**
 * Configuration constants
 */
const AUDIO_SAMPLE_BLOCK_SIZE = 512;  // Matches i2s read size on esp32c3
const AUDIO_STACK_BUFFER_THRESHOLD = 5;  // Use stack storage for < 5 buffers, heap for >= 5

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
 * Audio Manager class to handle audio processing and UI
 */
export class AudioManager {
  /**
   * Initialize the AudioManager and set up global audio data storage
   */
  constructor() {
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
        hasActiveSamples: false
      };
    }
  }

  /**
   * Set up audio analysis for a given audio element
   * @param {HTMLAudioElement} audioElement - The audio element to analyze
   * @returns {Object} Audio analysis components
   */
  setupAudioAnalysis(audioElement) {
    // Create and configure the audio context and nodes
    const audioComponents = this.createAudioComponents(audioElement);
    
    // Get the audio element's input ID and store references
    const audioId = audioElement.parentNode.querySelector('input').id;
    this.storeAudioReferences(audioId, audioComponents);
    
    // Set up the audio processing callback
    this.setupAudioProcessing(audioComponents.scriptNode, audioComponents.sampleBuffer, audioElement, audioId);
    
    // Start audio playback
    this.startAudioPlayback(audioElement);
    
    return audioComponents;
  }
  
  /**
   * Create audio context and processing components
   * @param {HTMLAudioElement} audioElement - The audio element to analyze
   * @returns {Object} Created audio components
   */
  createAudioComponents(audioElement) {
    // Create audio context with browser compatibility
    const AudioContext = window.AudioContext || window.webkitAudioContext;
    const audioContext = new AudioContext();
    
    // Create script processor node for raw sample access
    // Note: ScriptProcessorNode is deprecated but still widely supported
    const scriptNode = audioContext.createScriptProcessor(AUDIO_SAMPLE_BLOCK_SIZE, 1, 1);
    
    // Create a buffer to store the latest samples as Int16Array
    const sampleBuffer = new Int16Array(AUDIO_SAMPLE_BLOCK_SIZE);
    
    // Connect audio element to script processor
    const source = audioContext.createMediaElementSource(audioElement);
    source.connect(audioContext.destination); // Connect to output
    source.connect(scriptNode); // Also connect to script processor
    scriptNode.connect(audioContext.destination); // Connect script processor to output
    
    return {
      audioContext,
      scriptNode,
      source,
      sampleBuffer
    };
  }
  
  /**
   * Store audio references in the global audio data object
   * @param {string} audioId - The ID of the audio input
   * @param {Object} components - Audio components to store
   */
  storeAudioReferences(audioId, components) {
    window.audioData.audioContexts[audioId] = components.audioContext;
    window.audioData.audioSamples[audioId] = components.sampleBuffer;
    window.audioData.audioBuffers[audioId] = new AudioBufferStorage(audioId);
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
   * Set up the audio processing callback
   * @param {ScriptProcessorNode} scriptNode - The script processor node
   * @param {Int16Array} sampleBuffer - Buffer to store audio samples
   * @param {HTMLAudioElement} audioElement - The audio element being processed
   * @param {string} audioId - The ID of the audio input
   */
  setupAudioProcessing(scriptNode, sampleBuffer, audioElement, audioId) {
    scriptNode.onaudioprocess = (audioProcessingEvent) => {
      // Get input data from the left channel
      const inputBuffer = audioProcessingEvent.inputBuffer;
      const inputData = inputBuffer.getChannelData(0);
      
      // Convert float32 audio data to int16 range
      this.convertAudioSamples(inputData, sampleBuffer);
      
      // Store samples if audio is playing
      if (!audioElement.paused) {
        this.storeAudioSamples(sampleBuffer, audioId);
        this.updateProcessingIndicator();
      }
    };
  }
  
  /**
   * Convert float32 audio samples to int16 format
   * @param {Float32Array} inputData - Raw audio data (-1.0 to 1.0)
   * @param {Int16Array} sampleBuffer - Target buffer for converted samples
   */
  convertAudioSamples(inputData, sampleBuffer) {
    for (let i = 0; i < inputData.length; i++) {
      // Convert from float32 (-1.0 to 1.0) to int16 range (-32768 to 32767)
      sampleBuffer[i] = Math.floor(inputData[i] * 32767);
    }
  }
  
  /**
   * Store a copy of the current audio samples
   * @param {Int16Array} sampleBuffer - Buffer containing current samples
   * @param {string} audioId - The ID of the audio input
   */
  storeAudioSamples(sampleBuffer, audioId) {
    const bufferCopy = new Int16Array(sampleBuffer);
    
    // Get timestamp relative to start of audio file (preferred method)
    let timestamp = 0;
    
    // Try to get the audio element for this ID to use its currentTime
    const audioContext = window.audioData.audioContexts[audioId];
    const audioElement = document.querySelector(`#audio-${audioId}`);
    
    if (audioElement && !audioElement.paused && audioElement.currentTime >= 0) {
      // Use audio element's currentTime (seconds) converted to milliseconds
      // This gives us time relative to the start of the audio file
      timestamp = Math.floor(audioElement.currentTime * 1000);
    } else if (audioContext && audioContext.currentTime >= 0) {
      // Fallback to AudioContext.currentTime (high-resolution audio time)
      // This is relative to when the audio context was created
      timestamp = Math.floor(audioContext.currentTime * 1000);
    } else {
      // Final fallback: use performance.now() for high-resolution system time
      // This is relative to when the page loaded (better than Date.now())
      timestamp = Math.floor(performance.now());
    }

    // Debug logging (sample ~1% of audio blocks to avoid console spam)
    if (Math.random() < 0.01) {
      const timestampSource = audioElement && !audioElement.paused && audioElement.currentTime >= 0 
        ? 'audioElement.currentTime' 
        : audioContext && audioContext.currentTime >= 0 
        ? 'audioContext.currentTime' 
        : 'performance.now()';
      console.log(`Audio timestamp for ${audioId}: ${timestamp}ms (source: ${timestampSource})`);
    }

    // Use optimized buffer storage system
    const bufferStorage = window.audioData.audioBuffers[audioId];
    const prevBufferCount = bufferStorage.getBufferCount();
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
      //console.log(`*** Cleaning up audio ${inputId}: ${stats.bufferCount} blocks, ${stats.memoryEstimateKB.toFixed(1)}KB freed ***`);
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
    stackCount: acc.stackCount + (stat.storageType === 'stack' ? 1 : 0),
    heapCount: acc.heapCount + (stat.storageType === 'heap' ? 1 : 0)
  }), { totalBufferCount: 0, totalSamples: 0, totalMemoryKB: 0, stackCount: 0, heapCount: 0 });
  
  return {
    individual: stats,
    totals: totals,
    optimization: {
      stackThreshold: AUDIO_STACK_BUFFER_THRESHOLD,
      description: `Uses efficient Int16Array storage for <${AUDIO_STACK_BUFFER_THRESHOLD} buffers, heap storage for >=${AUDIO_STACK_BUFFER_THRESHOLD} buffers`
    }
  };
};

/**
 * Audio Buffer Storage Manager
 * Optimizes memory usage by using stack-based storage for small buffer counts
 * and heap-based storage for larger accumulations
 */
class AudioBufferStorage {
  constructor(audioId) {
    this.audioId = audioId;
    this.stackBuffers = [];  // Efficient storage for small counts (< 5)
    this.heapBuffer = null;  // Consolidated storage for large counts (>= 5)
    this.isUsingHeap = false;
    this.totalSamples = 0;
  }

  /**
   * Add audio samples to the buffer using optimal storage strategy
   * @param {Int16Array} sampleBuffer - Raw audio samples
   * @param {number} timestamp - Sample timestamp
   */
  addSamples(sampleBuffer, timestamp) {
    const currentBufferCount = this.getBufferCount();
    
    if (currentBufferCount < AUDIO_STACK_BUFFER_THRESHOLD && !this.isUsingHeap) {
      // Use stack-based storage for small counts - keep as Int16Array for efficiency
      this.stackBuffers.push({
        samples: new Int16Array(sampleBuffer), // Keep as typed array
        timestamp: timestamp,
        length: sampleBuffer.length
      });
      this.totalSamples += sampleBuffer.length;
    } else {
      // Transition to or use heap-based storage for large counts
      this._transitionToHeapStorage();
      this._addToHeapStorage(sampleBuffer, timestamp);
    }
    
    // Debug logging for storage transitions
    if (currentBufferCount === AUDIO_STACK_BUFFER_THRESHOLD - 1 && !this.isUsingHeap) {
      console.log(`*** Audio ${this.audioId}: Transitioning to heap storage at ${AUDIO_STACK_BUFFER_THRESHOLD} buffers ***`);
    }
  }

  /**
   * Get all buffered samples in the format expected by the backend
   * @returns {Array} Array of sample objects with timestamp
   */
  getAllSamples() {
    if (!this.isUsingHeap) {
      // Return stack buffers, converting Int16Array to regular array for JSON serialization
      return this.stackBuffers.map(buffer => ({
        samples: Array.from(buffer.samples),
        timestamp: buffer.timestamp
      }));
    } else {
      // Return heap buffer data
      return this.heapBuffer.chunks.map(chunk => ({
        samples: chunk.samples,
        timestamp: chunk.timestamp
      }));
    }
  }

  /**
   * Get the current number of buffered blocks
   * @returns {number} Number of audio blocks
   */
  getBufferCount() {
    return this.isUsingHeap ? this.heapBuffer.chunks.length : this.stackBuffers.length;
  }

  /**
   * Get total number of samples across all buffers
   * @returns {number} Total sample count
   */
  getTotalSamples() {
    return this.totalSamples;
  }

  /**
   * Clear all buffered data with proper cleanup
   */
  clear() {
    // Clean up stack buffers
    if (this.stackBuffers.length > 0) {
      // Set to null to help garbage collection of Int16Arrays
      this.stackBuffers.forEach(buffer => {
        buffer.samples = null;
      });
      this.stackBuffers = [];
    }
    
    // Clean up heap buffer
    if (this.heapBuffer) {
      this.heapBuffer.chunks.forEach(chunk => {
        chunk.samples = null;
      });
      this.heapBuffer = null;
    }
    
    this.isUsingHeap = false;
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
      storageType: this.isUsingHeap ? 'heap' : 'stack',
      memoryEstimateKB: this._estimateMemoryUsage()
    };
  }

  /**
   * Transition from stack-based to heap-based storage
   * @private
   */
  _transitionToHeapStorage() {
    if (this.isUsingHeap) return;

    // Create heap storage structure
    this.heapBuffer = {
      chunks: [],
      totalLength: 0
    };

    // Move existing stack buffers to heap storage
    this.stackBuffers.forEach(stackBuffer => {
      this.heapBuffer.chunks.push({
        samples: Array.from(stackBuffer.samples), // Convert to regular array for heap storage
        timestamp: stackBuffer.timestamp
      });
      this.heapBuffer.totalLength += stackBuffer.length;
      // Clear the stack buffer reference
      stackBuffer.samples = null;
    });

    // Clear stack storage
    this.stackBuffers = [];
    this.isUsingHeap = true;
  }

  /**
   * Add samples to heap storage
   * @private
   */
  _addToHeapStorage(sampleBuffer, timestamp) {
    const samplesArray = Array.from(sampleBuffer);
    this.heapBuffer.chunks.push({
      samples: samplesArray,
      timestamp: timestamp
    });
    this.heapBuffer.totalLength += sampleBuffer.length;
    this.totalSamples += sampleBuffer.length;
  }

  /**
   * Estimate memory usage in KB
   * @private
   */
  _estimateMemoryUsage() {
    if (!this.isUsingHeap) {
      // Stack storage: Int16Array = 2 bytes per sample + object overhead
      return ((this.totalSamples * 2) + (this.stackBuffers.length * 50)) / 1024;
    } else {
      // Heap storage: Regular array = ~8 bytes per sample + object overhead  
      return ((this.totalSamples * 8) + (this.heapBuffer.chunks.length * 50)) / 1024;
    }
  }
}
