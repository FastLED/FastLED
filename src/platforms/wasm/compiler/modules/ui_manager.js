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
      
      audio.src = url;
      audio.autoplay = true;
      audio.loop = true;
      
      // Set up audio analysis if needed
      if (typeof window.setupAudioAnalysis === 'function') {
        window.setupAudioAnalysis(audio);
      }
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

// Helper function for audio analysis - make it available globally
window.setupAudioAnalysis = function(audioElement) {
  // Create audio context
  const AudioContext = window.AudioContext || window.webkitAudioContext;
  const audioContext = new AudioContext();
  
  // Create analyzer
  const analyzer = audioContext.createAnalyser();
  analyzer.fftSize = 2048;
  
  // Connect audio element to analyzer
  const source = audioContext.createMediaElementSource(audioElement);
  source.connect(analyzer);
  analyzer.connect(audioContext.destination);
  
  // Buffer for frequency data
  const bufferLength = analyzer.frequencyBinCount;
  const dataArray = new Uint8Array(bufferLength);
  
  // Update function for visualization
  function updateAudioData() {
    // Get frequency data
    analyzer.getByteFrequencyData(dataArray);
    
    // Calculate average amplitude
    let sum = 0;
    let nonZeroCount = 0;
    
    for (let i = 0; i < bufferLength; i++) {
      if (dataArray[i] > 0) {
        sum += dataArray[i];
        nonZeroCount++;
      }
    }
    
    // Only update if we have data and audio is playing
    if (nonZeroCount > 0 && !audioElement.paused) {
      const average = Math.round(sum / nonZeroCount);
      const timestamp = (audioElement.currentTime).toFixed(2);
      
      // Update the canvas label to show the current audio data
      const label = document.getElementById('canvas-label');
      if (label) {
        label.textContent = `Audio: ${average}`;
        
        // Make sure the label is visible
        if (!label.classList.contains('show-animation')) {
          label.classList.add('show-animation');
        }
      }
      
      // Log to console for debugging
      console.log(`[${timestamp}s] Audio sample size: ${bufferLength}, Average amplitude: ${average}`);
    }
    
    // Continue updating
    requestAnimationFrame(updateAudioData);
  }
  
  // Start updating
  updateAudioData();
  
  return {
    audioContext,
    analyzer,
    source,
    bufferLength,
    dataArray
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
      } else if (element.type === 'audio') {
        // currentValue = element.value;
        console.error('Audio input not supported yet');
      } else {
        currentValue = parseFloat(element.value);
      }
      if (this.previousUiState[id] !== currentValue) {
        changes[id] = currentValue;
        hasChanges = true;
        this.previousUiState[id] = currentValue;
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
