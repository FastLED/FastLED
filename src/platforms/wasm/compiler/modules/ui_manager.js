/* eslint-disable no-console */
/* eslint-disable import/prefer-default-export */
/* eslint-disable no-restricted-syntax */
/* eslint-disable max-len */
/* eslint-disable guard-for-in */

import { AudioManager } from './audio_manager.js';

// Create a global instance of AudioManager
const audioManager = new AudioManager();

// Make setupAudioAnalysis available globally
window.setupAudioAnalysis = function(audioElement) {
  return audioManager.setupAudioAnalysis(audioElement);
};

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
  return audioManager.createAudioField(element);
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
        // Handle audio input - get all accumulated sample blocks
        if (window.audioData && window.audioData.audioBuffers && window.audioData.hasActiveSamples) {
          const buffers = window.audioData.audioBuffers[element.id];
          
          if (buffers && buffers.length > 0) {
            // Concatenate all accumulated sample blocks into one flat array
            const allSamples = [].concat(...buffers);
            
            // Log some stats about the accumulated audio data
            // if (Math.random() < 0.1) {
            //   console.log(`Audio data for ${id}:`);
            //   console.log(`  Blocks: ${buffers.length}`);
            //   console.log(`  Total samples: ${allSamples.length}`);
            //   console.log(`  First 5 samples: ${allSamples.slice(0, 5)}`);
            //   console.log(`  Last 5 samples: ${allSamples.slice(-5)}`);
            // }
            
            // Always include audio samples in changes when audio is active
            changes[id] = allSamples;
            hasChanges = true;
            
            // Clear the buffer after sending samples
            window.audioData.audioBuffers[element.id] = [];
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
