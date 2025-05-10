/* eslint-disable no-console */
/* eslint-disable import/prefer-default-export */
/* eslint-disable no-restricted-syntax */
/* eslint-disable max-len */
/* eslint-disable guard-for-in */

/**
 * Configuration constants
 */
const AUDIO_SAMPLE_BLOCK_SIZE = 512;  // Matches i2s read size on esp32c3

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
        audioBuffers: {},   // Store accumulated audio buffers by ID
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
    window.audioData.audioBuffers[audioId] = [];
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
    window.audioData.audioBuffers[audioId].push(Array.from(bufferCopy));
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
   * Clean up any previous audio context
   * @param {string} inputId - The ID of the audio input
   */
  cleanupPreviousAudioContext(inputId) {
    if (window.audioData?.audioContexts?.[inputId]) {
      try {
        window.audioData.audioContexts[inputId].close();
      } catch (e) {
        console.warn('Error closing previous audio context:', e);
      }
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
