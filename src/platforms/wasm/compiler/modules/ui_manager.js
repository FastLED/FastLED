/* eslint-disable no-console */
/* eslint-disable import/prefer-default-export */
/* eslint-disable no-restricted-syntax */
/* eslint-disable max-len */
/* eslint-disable guard-for-in */


function createNumberField(element) {
  const controlDiv = document.createElement('div');
  controlDiv.className = 'ui-control';

  const label = document.createElement('label');
  label.textContent = element.name;
  label.htmlFor = `number-${element.id}`;

  const numberInput = document.createElement('input');
  numberInput.type = 'number';
  numberInput.id = `number-${element.id}`;
  numberInput.value = element.value;
  numberInput.min = element.min;
  numberInput.max = element.max;
  numberInput.step = (element.step !== undefined) ? element.step : 'any';

  controlDiv.appendChild(label);
  controlDiv.appendChild(numberInput);

  return controlDiv;
}

function createAudioField(element) {
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
      const analysisSetup = window.setupAudioAnalysis(audio);
      
      // Now play the audio
      audio.loop = true;
      
      // Ensure we can play (autoplay policies might block)
      audio.play().then(() => {
        console.log("Audio playback started successfully");
        
        // Set up a timer to periodically mark samples as active
        // This ensures we regularly send audio data to FastLED
        setInterval(() => {
          if (!audio.paused) {
            window.audioData.hasActiveSamples = true;
          }
        }, 50); // Send samples approximately 20 times per second
        
      }).catch(err => {
        console.error("Error starting audio playback:", err);
        // Add a play button as fallback
        const playButton = document.createElement('button');
        playButton.textContent = "Play Audio";
        playButton.className = "audio-play-button";
        playButton.onclick = () => {
          audio.play().then(() => {
            // Set up the timer after successful play
            setInterval(() => {
              if (!audio.paused) {
                window.audioData.hasActiveSamples = true;
              }
            }, 50);
          });
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

function createSlider(element) {
  const controlDiv = document.createElement('div');
  controlDiv.className = 'ui-control';

  const labelValueContainer = document.createElement('div');
  labelValueContainer.style.display = 'flex';
  labelValueContainer.style.justifyContent = 'space-between';
  labelValueContainer.style.width = '100%';

  const label = document.createElement('label');
  label.textContent = element.name;
  label.htmlFor = `slider-${element.id}`;

  const valueDisplay = document.createElement('span');
  valueDisplay.textContent = element.value;

  labelValueContainer.appendChild(label);
  labelValueContainer.appendChild(valueDisplay);

  const slider = document.createElement('input');
  slider.type = 'range';
  slider.id = `slider-${element.id}`;
  slider.min = Number.parseFloat(element.min);
  slider.max = Number.parseFloat(element.max);
  slider.value = Number.parseFloat(element.value);
  slider.step = Number.parseFloat(element.step);
  setTimeout(() => {
    // Sets the slider value, for some reason we have to do it
    // next frame.
    slider.value = Number.parseFloat(element.value);
    valueDisplay.textContent = slider.value;
  }, 0);

  slider.addEventListener('input', () => {
    valueDisplay.textContent = slider.value;
  });

  controlDiv.appendChild(labelValueContainer);
  controlDiv.appendChild(slider);

  return controlDiv;
}

function createCheckbox(element) {
  const controlDiv = document.createElement('div');
  controlDiv.className = 'ui-control';

  const label = document.createElement('label');
  label.textContent = element.name;
  label.htmlFor = `checkbox-${element.id}`;

  const checkbox = document.createElement('input');
  checkbox.type = 'checkbox';
  checkbox.id = `checkbox-${element.id}`;
  checkbox.checked = element.value;

  const flexContainer = document.createElement('div');
  flexContainer.style.display = 'flex';
  flexContainer.style.alignItems = 'center';
  flexContainer.style.justifyContent = 'space-between';

  flexContainer.appendChild(label);
  flexContainer.appendChild(checkbox);

  controlDiv.appendChild(flexContainer);

  return controlDiv;
}

function createButton(element) {
  const controlDiv = document.createElement('div');
  controlDiv.className = 'ui-control';

  const button = document.createElement('button');
  button.textContent = element.name;
  button.id = `button-${element.id}`;
  button.setAttribute('data-pressed', 'false');

  button.addEventListener('mousedown', () => {
    button.setAttribute('data-pressed', 'true');
    button.classList.add('active');
  });

  button.addEventListener('mouseup', () => {
    button.setAttribute('data-pressed', 'false');
    button.classList.remove('active');
  });

  button.addEventListener('mouseleave', () => {
    button.setAttribute('data-pressed', 'false');
    button.classList.remove('active');
  });
  controlDiv.appendChild(button);
  return controlDiv;
}

function createUiControlsContainer() {
  const container = document.getElementById(this.uiControlsId);
  if (!container) {
    console.error('UI controls container not found in the HTML');
  }
  return container;
}

function setTitle(titleData) {
  if (titleData && titleData.text) {
    document.title = titleData.text;
    const h1Element = document.querySelector('h1');
    if (h1Element) {
      h1Element.textContent = titleData.text;
    } else {
      console.warn('H1 element not found in document');
    }
  } else {
    console.warn('Invalid title data received:', titleData);
  }
}

function setDescription(descData) {
  if (descData && descData.text) {
    // Create or find description element
    let descElement = document.querySelector('#fastled-description');
    if (!descElement) {
      descElement = document.createElement('div');
      descElement.id = 'fastled-description';
      // Insert after h1
      const h1Element = document.querySelector('h1');
      if (h1Element && h1Element.nextSibling) {
        h1Element.parentNode.insertBefore(descElement, h1Element.nextSibling);
      } else {
        console.warn('Could not find h1 element to insert description after');
        document.body.insertBefore(descElement, document.body.firstChild);
      }
    }
    descElement.textContent = descData.text;
  } else {
    console.warn('Invalid description data received:', descData);
  }
}

// Global audio data storage - ensure it's initialized
if (!window.audioData) {
  console.log("Initializing global audio data storage");
  window.audioData = {
    audioContexts: {},
    audioSamples: {},
    hasActiveSamples: false
  };
}

// Helper function for audio analysis - make it available globally
window.setupAudioAnalysis = function(audioElement) {
  // Create audio context
  const AudioContext = window.AudioContext || window.webkitAudioContext;
  const audioContext = new AudioContext();
  
  // Create script processor node for raw sample access
  // Note: ScriptProcessorNode is deprecated but still widely supported
  // We use it here for simplicity in getting raw audio samples
  const scriptNode = audioContext.createScriptProcessor(1024, 1, 1);
  
  // Create a buffer to store the latest 1024 samples as Int16Array
  const sampleBuffer = new Int16Array(1024);
  
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
    
    // Log audio processing occasionally (every ~2 seconds to avoid console spam)
    if (Math.random() < 0.01) {
      // Calculate some basic stats about the audio data
      const nonZeroCount = Array.from(sampleBuffer).filter(v => v !== 0).length;
      const hasAudioData = nonZeroCount > 0;
      
      // console.log(`Audio processing for ${audioId}:`);
      // console.log(`  Buffer size: ${sampleBuffer.length}`);
      // console.log(`  Non-zero samples: ${nonZeroCount}`);
      // console.log(`  Has audio data: ${hasAudioData}`);
      
      // if (hasAudioData) {
      //   // Show a few sample values
      //   console.log(`  Sample values (first 5): ${Array.from(sampleBuffer.slice(0, 5))}`);
      // }
    }
    
    // Mark this audio element as having active samples
    window.audioData.hasActiveSamples = true;
    
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
};

export class UiManager {
  constructor(uiControlsId) {
    this.uiElements = {};
    this.previousUiState = {};
    this.uiControlsId = uiControlsId;
  }

  // Returns a Json object if there are changes, otherwise null.
  processUiChanges() {
    const changes = {}; // Json object to store changes.
    let hasChanges = false;
    for (const id in this.uiElements) {
      const element = this.uiElements[id];
      let currentValue;
      if (element.type === 'checkbox') {
        currentValue = element.checked;
      } else if (element.type === 'submit') {
        const attr = element.getAttribute('data-pressed');
        currentValue = attr === 'true';
      } else if (element.type === 'number') {
        currentValue = parseFloat(element.value);
      } else if (element.type === 'file' && element.accept === 'audio/*') {
        // Handle audio input - get the latest 1024 int16 samples
        if (window.audioData && window.audioData.audioSamples) {
          const samples = window.audioData.audioSamples[element.id];
          
          if (samples && window.audioData.hasActiveSamples) {
            // Convert Int16Array to regular array for JSON serialization
            currentValue = Array.from(samples);
            
            // // Print out the audio values being serialized
            // console.log(`Audio data for ${id}:`);
            // console.log(`  Sample count: ${currentValue.length}`);
            // console.log(`  First 5 samples: ${currentValue.slice(0, 5)}`);
            // console.log(`  Last 5 samples: ${currentValue.slice(-5)}`);
            
            // Calculate some statistics
            // const nonZeroCount = currentValue.filter(v => v !== 0).length;
            // const min = Math.min(...currentValue);
            // const max = Math.max(...currentValue);
            // const sum = currentValue.reduce((a, b) => a + Math.abs(b), 0);
            // const avg = sum / currentValue.length;
            
            // console.log(`  Non-zero samples: ${nonZeroCount} (${Math.round(nonZeroCount/currentValue.length*100)}%)`);
            // console.log(`  Range: ${min} to ${max}, Average amplitude: ${avg.toFixed(2)}`);
            
            // Always include audio samples in changes when audio is active
            changes[id] = currentValue;
            hasChanges = true;
            
            // Reset the flag after sending samples
            window.audioData.hasActiveSamples = false;
            
            continue; // Skip the comparison below for audio
          }
        }
        
        // If we reach here, either no samples are available or they've already been sent
        // Don't add to changes object, so we don't spam with empty arrays
      } else {
        currentValue = parseFloat(element.value);
      }
      
      // For non-audio elements, only include if changed
      if (this.previousUiState[id] !== currentValue) {
        changes[id] = currentValue;
        hasChanges = true;
        this.previousUiState[id] = currentValue;
      }
    }

    if (hasChanges) {
      // Log the final JSON that will be sent to FastLED
      // console.log('Sending UI changes to FastLED:');
      
      // Check if there's audio data in the changes
      const audioKeys = Object.keys(changes).filter(key => 
        this.uiElements[key] && 
        this.uiElements[key].type === 'file' && 
        this.uiElements[key].accept === 'audio/*'
      );
      
      if (audioKeys.length > 0) {
        // For each audio element, log summary info but not the full array
        audioKeys.forEach(key => {
          const audioData = changes[key];
          //console.log(`  Audio ${key}: ${audioData.length} samples`);
          
          // Create a copy of changes with abbreviated audio data for logging
          const changesCopy = {...changes};
          changesCopy[key] = `[${audioData.length} samples]`;
          //console.log(JSON.stringify(changesCopy, null, 2));
        });
      } else {
        // No audio data, log the full changes object
        //console.log(JSON.stringify(changes, null, 2));
      }
    }
    
    return hasChanges ? changes : null;
  }

  addUiElements(jsonData) {
    console.log('UI elements added:', jsonData);
    const uiControlsContainer = document.getElementById(this.uiControlsId) || createUiControlsContainer();
    let foundUi = false;
    jsonData.forEach((data) => {
      console.log('data:', data);
      const { group } = data;
      const hasGroup = group !== '' && group !== undefined;
      if (hasGroup) {
        console.log(`Group ${group} found, for item ${data.name}`);
      }

      if (data.type === 'title') {
        setTitle(data);
        return; // Skip creating UI control for title
      }

      if (data.type === 'description') {
        setDescription(data);
        return; // Skip creating UI control for description
      }

      let control;
      if (data.type === 'slider') {
        control = createSlider(data);
      } else if (data.type === 'checkbox') {
        control = createCheckbox(data);
      } else if (data.type === 'button') {
        control = createButton(data);
      } else if (data.type === 'number') {
        control = createNumberField(data);
      } else if (data.type === 'audio') {
        control = createAudioField(data);
      }

      // AI hallucinated this:
      // if (hasGroup) {
      //   const groupContainer = document.getElementById(group);
      //   if (!groupContainer) {
      //     console.error(`Group ${group} not found in the HTML`);
      //     return;
      //   }
      //   groupContainer.appendChild(control);
      // } else {
      //   uiControlsContainer.appendChild(control);
      // }

      if (control) {
        foundUi = true;
        uiControlsContainer.appendChild(control);
        if (data.type === 'button') {
          this.uiElements[data.id] = control.querySelector('button');
        } else {
          this.uiElements[data.id] = control.querySelector('input');
        }
        this.previousUiState[data.id] = data.value;
      }
    });
    if (foundUi) {
      console.log('UI elements added, showing UI controls container');
      uiControlsContainer.classList.add('active');
    }
  }
}
