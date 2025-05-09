/* eslint-disable no-console */
/* eslint-disable import/prefer-default-export */
/* eslint-disable no-restricted-syntax */
/* eslint-disable max-len */
/* eslint-disable guard-for-in */

// Audio processing configuration
const AUDIO_SAMPLE_BLOCK_SIZE = 512;  // Matches i2s read size on esp32c3

// Audio Manager class to handle audio processing
export class AudioManager {
  constructor() {
    // Global audio data storage - ensure it's initialized
    if (!window.audioData) {
      console.log("Initializing global audio data storage");
      window.audioData = {
        audioContexts: {},
        audioSamples: {},
        audioBuffers: {},
        hasActiveSamples: false
      };
    }
  }

  setupAudioAnalysis(audioElement) {
    // Create audio context
    const AudioContext = window.AudioContext || window.webkitAudioContext;
    const audioContext = new AudioContext();
    
    // Create script processor node for raw sample access
    // Note: ScriptProcessorNode is deprecated but still widely supported
    // We use it here for simplicity in getting raw audio samples
    const scriptNode = audioContext.createScriptProcessor(AUDIO_SAMPLE_BLOCK_SIZE, 1, 1);
    
    // Create a buffer to store the latest samples as Int16Array
    const sampleBuffer = new Int16Array(AUDIO_SAMPLE_BLOCK_SIZE);
    
    // Connect audio element to script processor
    const source = audioContext.createMediaElementSource(audioElement);
    source.connect(audioContext.destination); // Connect to output
    source.connect(scriptNode); // Also connect to script processor
    scriptNode.connect(audioContext.destination); // Connect script processor to output
    
    // Make sure audio is playing
    audioElement.play().catch(err => {
      console.error("Error playing audio:", err);
    });
    
    // Get the audio element's input ID
    const audioId = audioElement.parentNode.querySelector('input').id;
    
    // Store the audio context and sample buffer in our global object
    window.audioData.audioContexts[audioId] = audioContext;
    window.audioData.audioSamples[audioId] = sampleBuffer;
    window.audioData.audioBuffers[audioId] = [];
    
    // Process audio data
    scriptNode.onaudioprocess = function(audioProcessingEvent) {
      // Get the input buffer
      const inputBuffer = audioProcessingEvent.inputBuffer;
      
      // Get the actual audio data from channel 0 (left channel)
      const inputData = inputBuffer.getChannelData(0);
      
      // Convert float32 audio data to int16 range and store in our buffer
      for (let i = 0; i < inputData.length; i++) {
        // Convert from float32 (-1.0 to 1.0) to int16 range (-32768 to 32767)
        sampleBuffer[i] = Math.floor(inputData[i] * 32767);
      }
      
      // Create a copy of the current sample buffer and add it to our accumulated buffers
      if (!audioElement.paused) {
        const bufferCopy = new Int16Array(sampleBuffer);
        window.audioData.audioBuffers[audioId].push(Array.from(bufferCopy));
        window.audioData.hasActiveSamples = true;
      }
      
      // Optional: Update UI with a simple indicator that audio is being processed
      const label = document.getElementById('canvas-label');
      if (label && !audioElement.paused) {
        label.textContent = 'Audio: Processing';
        if (!label.classList.contains('show-animation')) {
          label.classList.add('show-animation');
        }
      }
    };
    
    return {
      audioContext,
      scriptNode,
      source,
      sampleBuffer
    };
  }

  createAudioField(element) {
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
    
    audioInput.addEventListener('change', (event) => {
      const file = event.target.files[0];
      if (file) {
        const url = URL.createObjectURL(file);
        
        // Update button text to show selected file
        uploadButton.textContent = file.name.length > 20 
          ? file.name.substring(0, 17) + '...' 
          : file.name;
        
        // Create or update audio element
        let audio = controlDiv.querySelector('audio');
        if (!audio) {
          audio = document.createElement('audio');
          audio.controls = true;
          audio.className = 'audio-player';
          controlDiv.appendChild(audio);
        }
        
        // Clean up any previous audio context
        if (window.audioData && window.audioData.audioContexts && 
            window.audioData.audioContexts[audioInput.id]) {
          try {
            window.audioData.audioContexts[audioInput.id].close();
          } catch (e) {
            console.warn('Error closing previous audio context:', e);
          }
        }
        
        // Set source first
        audio.src = url;
        
        // Initialize audio analysis before playing
        const analysisSetup = this.setupAudioAnalysis(audio);
        
        // Now play the audio
        audio.loop = true;
        
        // Ensure we can play (autoplay policies might block)
        audio.play().then(() => {
          console.log("Audio playback started successfully");
        }).catch(err => {
          console.error("Error starting audio playback:", err);
          // Add a play button as fallback
          const playButton = document.createElement('button');
          playButton.textContent = "Play Audio";
          playButton.className = "audio-play-button";
          playButton.onclick = () => {
            audio.play();
          };
          controlDiv.appendChild(playButton);
        });
        
        // Add a small indicator that audio is being processed
        const audioIndicator = document.createElement('div');
        audioIndicator.className = 'audio-indicator';
        audioIndicator.textContent = 'Audio samples ready';
        
        // Replace any existing indicator
        const existingIndicator = controlDiv.querySelector('.audio-indicator');
        if (existingIndicator) {
          controlDiv.removeChild(existingIndicator);
        }
        
        controlDiv.appendChild(audioIndicator);
      }
    });
    
    controlDiv.appendChild(uploadButton);
    controlDiv.appendChild(audioInput);
    
    return controlDiv;
  }
}

// Create a global instance of AudioManager
const audioManager = new AudioManager();

// Make setupAudioAnalysis available globally
window.setupAudioAnalysis = function(audioElement) {
  return audioManager.setupAudioAnalysis(audioElement);
};
