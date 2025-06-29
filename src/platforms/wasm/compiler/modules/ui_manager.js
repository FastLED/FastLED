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

/**
 * Simple markdown to HTML converter
 * Supports: headers, bold, italic, code, links, lists, and paragraphs
 */
function markdownToHtml(markdown) {
  if (!markdown) return '';
  
  let html = markdown;
  
  // Convert headers (# ## ### etc.)
  html = html.replace(/^### (.+)$/gm, '<h3>$1</h3>');
  html = html.replace(/^## (.+)$/gm, '<h2>$1</h2>');
  html = html.replace(/^# (.+)$/gm, '<h1>$1</h1>');
  
  // Convert bold **text** and __text__
  html = html.replace(/\*\*(.+?)\*\*/g, '<strong>$1</strong>');
  html = html.replace(/__(.+?)__/g, '<strong>$1</strong>');
  
  // Convert italic *text* and _text_
  html = html.replace(/\*(.+?)\*/g, '<em>$1</em>');
  html = html.replace(/_(.+?)_/g, '<em>$1</em>');
  
  // Convert inline code `code`
  html = html.replace(/`([^`]+)`/g, '<code>$1</code>');
  
  // Convert code blocks ```code```
  html = html.replace(/```([^`]+)```/g, '<pre><code>$1</code></pre>');
  
  // Convert links [text](url)
  html = html.replace(/\[([^\]]+)\]\(([^)]+)\)/g, '<a href="$2" target="_blank">$1</a>');
  
  // Convert unordered lists (- item or * item)
  html = html.replace(/^[\-\*] (.+)$/gm, '<li>$1</li>');
  
  // Convert ordered lists (1. item, 2. item, etc.)
  html = html.replace(/^\d+\. (.+)$/gm, '<li class="ordered">$1</li>');
  
  // Wrap consecutive <li> elements properly
  html = html.replace(/(<li(?:\s+class="ordered")?>.*?<\/li>(?:\s*<li(?:\s+class="ordered")?>.*?<\/li>)*)/gs, function(match) {
    if (match.includes('class="ordered"')) {
      return '<ol>' + match.replace(/\s+class="ordered"/g, '') + '</ol>';
    } else {
      return '<ul>' + match + '</ul>';
    }
  });
  
  // Convert line breaks to paragraphs (double newlines become paragraph breaks)
  const paragraphs = html.split(/\n\s*\n/);
  html = paragraphs.map(p => {
    const trimmed = p.trim();
    if (trimmed && !trimmed.startsWith('<h') && !trimmed.startsWith('<ul') && !trimmed.startsWith('<ol') && !trimmed.startsWith('<pre')) {
      return '<p>' + trimmed.replace(/\n/g, '<br>') + '</p>';
    }
    return trimmed;
  }).join('\n');
  
  return html;
}

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

function createDropdown(element) {
  const controlDiv = document.createElement('div');
  controlDiv.className = 'ui-control';

  const label = document.createElement('label');
  label.textContent = element.name;
  label.htmlFor = `dropdown-${element.id}`;

  const dropdown = document.createElement('select');
  dropdown.id = `dropdown-${element.id}`;
  dropdown.value = element.value;

  // Add options to the dropdown
  if (element.options && Array.isArray(element.options)) {
    element.options.forEach((option, index) => {
      const optionElement = document.createElement('option');
      optionElement.value = index;
      optionElement.textContent = option;
      dropdown.appendChild(optionElement);
    });
  }

  // Set the selected option
  dropdown.selectedIndex = element.value;

  controlDiv.appendChild(label);
  controlDiv.appendChild(dropdown);

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
    
    // Always process text as markdown (plain text is valid markdown)
    descElement.innerHTML = markdownToHtml(descData.text);
  } else {
    console.warn('Invalid description data received:', descData);
  }
}

function createHelp(element) {
  const helpContainer = document.createElement('div');
  helpContainer.className = 'ui-help-container';
  helpContainer.id = `help-${element.id}`;
  
  // Create help button
  const helpButton = document.createElement('button');
  helpButton.className = 'ui-help-button';
  helpButton.textContent = '?';
  helpButton.setAttribute('type', 'button');
  helpButton.setAttribute('aria-label', 'Help');
  
  // Prepare content for tooltip and popup
  const markdownContent = element.markdownContent || '';
  const tooltipText = markdownContent.length > 200 
    ? markdownContent.substring(0, 200).trim() + '...' 
    : markdownContent;
  
  // Convert markdown to HTML for popup
  const htmlContent = markdownToHtml(markdownContent);
  
  // Create tooltip
  const tooltip = document.createElement('div');
  tooltip.className = 'ui-help-tooltip';
  tooltip.textContent = tooltipText;
  
  // Add event listeners for tooltip
  helpButton.addEventListener('mouseenter', () => {
    if (tooltipText.trim()) {
      showTooltip(helpButton, tooltip);
    }
  });
  
  helpButton.addEventListener('mouseleave', () => {
    hideTooltip(tooltip);
  });
  
  // Add event listener for popup
  helpButton.addEventListener('click', (e) => {
    e.preventDefault();
    e.stopPropagation();
    showHelpPopup(htmlContent);
  });
  
  // Assemble the help container
  helpContainer.appendChild(helpButton);
  
  // Append tooltip to document body so it can appear above everything
  document.body.appendChild(tooltip);
  
  // Add styles if not already present
  if (!document.querySelector('#ui-help-styles')) {
    const style = document.createElement('style');
    style.id = 'ui-help-styles';
    style.textContent = `
      .ui-help-container {
        position: relative;
        display: inline-block;
        margin: 5px;
      }
      
      .ui-help-button {
        width: 24px;
        height: 24px;
        border-radius: 50%;
        background-color: #6c757d;
        color: white;
        border: none;
        font-size: 14px;
        font-weight: bold;
        cursor: pointer;
        display: flex;
        align-items: center;
        justify-content: center;
        transition: background-color 0.2s ease;
      }
      
      .ui-help-button:hover {
        background-color: #5a6268;
      }
      
      .ui-help-tooltip {
        position: fixed;
        background-color: #333;
        color: white;
        padding: 8px 12px;
        border-radius: 4px;
        font-size: 12px;
        white-space: pre-wrap;
        max-width: 300px;
        z-index: 10001;
        visibility: hidden;
        opacity: 0;
        transition: opacity 0.2s ease;
        box-shadow: 0 2px 8px rgba(0,0,0,0.2);
        pointer-events: none;
      }
      

      
      .ui-help-popup {
        position: fixed;
        top: 0;
        left: 0;
        width: 100%;
        height: 100%;
        background-color: rgba(0, 0, 0, 0.8);
        z-index: 10000;
        display: flex;
        align-items: center;
        justify-content: center;
        padding: 20px;
        box-sizing: border-box;
      }
      
      .ui-help-popup-content {
        background-color: #2d3748;
        color: #e2e8f0;
        border-radius: 8px;
        max-width: 90%;
        max-height: 90%;
        overflow-y: auto;
        padding: 24px;
        position: relative;
        box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.1), 0 10px 10px -5px rgba(0, 0, 0, 0.04);
        font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
        line-height: 1.6;
      }
      
      .ui-help-popup-close {
        position: absolute;
        top: 16px;
        right: 16px;
        background: none;
        border: none;
        color: #a0aec0;
        font-size: 24px;
        cursor: pointer;
        padding: 4px;
        line-height: 1;
      }
      
      .ui-help-popup-close:hover {
        color: #e2e8f0;
      }
      
      .ui-help-popup-content h1,
      .ui-help-popup-content h2,
      .ui-help-popup-content h3 {
        color: #f7fafc;
        margin-top: 0;
        margin-bottom: 16px;
      }
      
      .ui-help-popup-content h1 { font-size: 1.875rem; }
      .ui-help-popup-content h2 { font-size: 1.5rem; }
      .ui-help-popup-content h3 { font-size: 1.25rem; }
      
      .ui-help-popup-content p {
        margin: 16px 0;
        color: #cbd5e0;
      }
      
      .ui-help-popup-content ul,
      .ui-help-popup-content ol {
        padding-left: 24px;
        margin: 16px 0;
      }
      
      .ui-help-popup-content li {
        margin: 8px 0;
        color: #cbd5e0;
      }
      
      .ui-help-popup-content code {
        background-color: #4a5568;
        color: #f7fafc;
        padding: 2px 6px;
        border-radius: 4px;
        font-family: 'JetBrains Mono', 'Fira Code', 'Courier New', monospace;
        font-size: 0.875rem;
      }
      
      .ui-help-popup-content pre {
        background-color: #1a202c;
        color: #f7fafc;
        padding: 16px;
        border-radius: 6px;
        overflow-x: auto;
        margin: 16px 0;
        border: 1px solid #4a5568;
      }
      
      .ui-help-popup-content pre code {
        background-color: transparent;
        padding: 0;
        color: inherit;
      }
      
      .ui-help-popup-content a {
        color: #63b3ed;
        text-decoration: none;
      }
      
      .ui-help-popup-content a:hover {
        color: #90cdf4;
        text-decoration: underline;
      }
      
      .ui-help-popup-content strong {
        color: #f7fafc;
        font-weight: 600;
      }
      
      .ui-help-popup-content em {
        color: #e2e8f0;
        font-style: italic;
      }
      
      .ui-help-popup-content blockquote {
        border-left: 4px solid #4a5568;
        margin: 16px 0;
        padding-left: 16px;
        color: #a0aec0;
        font-style: italic;
      }
    `;
    document.head.appendChild(style);
  }
  
  return helpContainer;
}

function showHelpPopup(htmlContent) {
  // Remove any existing popup
  const existingPopup = document.querySelector('.ui-help-popup');
  if (existingPopup) {
    existingPopup.remove();
  }
  
  // Create popup
  const popup = document.createElement('div');
  popup.className = 'ui-help-popup';
  
  const popupContent = document.createElement('div');
  popupContent.className = 'ui-help-popup-content';
  
  const closeButton = document.createElement('button');
  closeButton.className = 'ui-help-popup-close';
  closeButton.innerHTML = '&times;';
  closeButton.setAttribute('aria-label', 'Close help');
  
  const contentDiv = document.createElement('div');
  contentDiv.innerHTML = htmlContent;
  
  popupContent.appendChild(closeButton);
  popupContent.appendChild(contentDiv);
  popup.appendChild(popupContent);
  
  // Add event listeners
  closeButton.addEventListener('click', () => {
    popup.remove();
  });
  
  popup.addEventListener('click', (e) => {
    if (e.target === popup) {
      popup.remove();
    }
  });
  
  // Handle escape key
  const handleEscape = (e) => {
    if (e.key === 'Escape') {
      popup.remove();
      document.removeEventListener('keydown', handleEscape);
    }
  };
  document.addEventListener('keydown', handleEscape);
  
  // Add to DOM
  document.body.appendChild(popup);
}

function showTooltip(button, tooltip) {
  // Get button position relative to viewport
  const buttonRect = button.getBoundingClientRect();
  
  // Make tooltip visible but transparent to measure it
  tooltip.style.visibility = 'visible';
  tooltip.style.opacity = '0';
  
  // Now get tooltip dimensions
  const tooltipRect = tooltip.getBoundingClientRect();
  
  // Calculate tooltip position
  const buttonCenterX = buttonRect.left + buttonRect.width / 2;
  const tooltipTop = buttonRect.top - tooltipRect.height - 8; // 8px gap above button
  
  // Position tooltip centered above the button
  let tooltipLeft = buttonCenterX - tooltipRect.width / 2;
  
  // Ensure tooltip doesn't go off-screen horizontally
  const padding = 10;
  if (tooltipLeft < padding) {
    tooltipLeft = padding;
  } else if (tooltipLeft + tooltipRect.width > window.innerWidth - padding) {
    tooltipLeft = window.innerWidth - tooltipRect.width - padding;
  }
  
  // Position tooltip
  tooltip.style.left = `${tooltipLeft}px`;
  tooltip.style.top = `${tooltipTop}px`;
  
  // Show tooltip with fade-in
  tooltip.style.opacity = '1';
}

function hideTooltip(tooltip) {
  tooltip.style.visibility = 'hidden';
  tooltip.style.opacity = '0';
}


export class JsonUiManager {
  constructor(uiControlsId) {
    // console.log('*** JsonUiManager JS: CONSTRUCTOR CALLED ***');
    this.uiElements = {};
    this.previousUiState = {};
    this.uiControlsId = uiControlsId;
    this.groups = new Map(); // Track created groups
    this.ungroupedContainer = null; // Container for ungrouped items
    this.debugMode = false; // Debug logging control
  }

  // Method called by C++ backend to update UI components
  updateUiComponents(jsonString) {
    // console.log('*** C++→JS: Backend update received:', jsonString);
    
    try {
      const updates = JSON.parse(jsonString);
      
      // Process each update
      for (const [elementId, updateData] of Object.entries(updates)) {
        // Strip 'id_' prefix if present to match our element storage
        const actualElementId = elementId.startsWith('id_') ? elementId.substring(3) : elementId;
        
        const element = this.uiElements[actualElementId];
        if (element) {
          // Extract value from update data
          const value = updateData.value !== undefined ? updateData.value : updateData;
          
          // Update the element based on its type
          if (element.type === 'checkbox') {
            element.checked = Boolean(value);
          } else if (element.type === 'range') {
            element.value = value;
            // Also update the display value if it exists
            const valueDisplay = element.parentElement.querySelector('span');
            if (valueDisplay) {
              valueDisplay.textContent = value;
            }
          } else if (element.type === 'number') {
            element.value = value;
          } else if (element.tagName === 'SELECT') {
            element.selectedIndex = value;
          } else if (element.type === 'submit') {
            element.setAttribute('data-pressed', value ? 'true' : 'false');
            if (value) {
              element.classList.add('active');
            } else {
              element.classList.remove('active');
            }
          } else {
            element.value = value;
          }
          
          // Update our internal state tracking
          this.previousUiState[actualElementId] = value;
          // console.log(`*** C++→JS: Updated UI element '${actualElementId}' = ${value} ***`);
        } else {
          // console.warn(`*** C++→JS: Element '${actualElementId}' not found ***`);
        }
      }
    } catch (error) {
      console.error('*** C++→JS: Update error:', error, 'JSON:', jsonString);
    }
  }

  // Create a collapsible group container
  createGroupContainer(groupName) {
    if (this.groups.has(groupName)) {
      return this.groups.get(groupName);
    }

    const groupDiv = document.createElement('div');
    groupDiv.className = 'ui-group';
    groupDiv.id = `group-${groupName}`;

    const headerDiv = document.createElement('div');
    headerDiv.className = 'ui-group-header';
    
    const titleSpan = document.createElement('span');
    titleSpan.className = 'ui-group-title';
    titleSpan.textContent = groupName;
    
    const toggleSpan = document.createElement('span');
    toggleSpan.className = 'ui-group-toggle';
    toggleSpan.textContent = '▼';
    
    headerDiv.appendChild(titleSpan);
    headerDiv.appendChild(toggleSpan);
    
    const contentDiv = document.createElement('div');
    contentDiv.className = 'ui-group-content';
    
    // Add click handler for collapse/expand
    headerDiv.addEventListener('click', () => {
      groupDiv.classList.toggle('collapsed');
    });
    
    groupDiv.appendChild(headerDiv);
    groupDiv.appendChild(contentDiv);
    
    const groupInfo = {
      container: groupDiv,
      content: contentDiv,
      name: groupName
    };
    
    this.groups.set(groupName, groupInfo);
    return groupInfo;
  }

  // Create or get the ungrouped items container
  createUngroupedContainer() {
    if (this.ungroupedContainer) {
      return this.ungroupedContainer;
    }

    const ungroupedDiv = document.createElement('div');
    ungroupedDiv.className = 'ui-ungrouped';
    ungroupedDiv.id = 'ungrouped-items';
    
    this.ungroupedContainer = ungroupedDiv;
    return ungroupedDiv;
  }

  // Clear all UI elements and groups
  clearUiElements() {
    const uiControlsContainer = document.getElementById(this.uiControlsId);
    if (uiControlsContainer) {
      uiControlsContainer.innerHTML = '';
    }
    
    // Remove any tooltips that were added to document.body
    const tooltips = document.querySelectorAll('.ui-help-tooltip');
    tooltips.forEach(tooltip => tooltip.remove());
    
    this.groups.clear();
    this.ungroupedContainer = null;
    this.uiElements = {};
    this.previousUiState = {};
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
      } else if (element.tagName === 'SELECT') {
        currentValue = parseInt(element.value);
      } else if (element.type === 'file' && element.accept === 'audio/*') {
        // Handle audio input - get all accumulated sample blocks with timestamps
        if (window.audioData && window.audioData.audioBuffers && window.audioData.hasActiveSamples) {
          const bufferStorage = window.audioData.audioBuffers[element.id];
          
          if (bufferStorage && bufferStorage.getBufferCount() > 0) {
            // Get all samples using the optimized storage system
            const samples = bufferStorage.getAllSamples();
            changes[id] = samples;
            hasChanges = true;
            
            // Debug logging for audio stats (only when enabled)
            if (this.debugMode) {
              const stats = bufferStorage.getStats();
              console.log(`🎵 UI Audio ${id}: ${stats.bufferCount} blocks, ${stats.totalSamples} samples, ${stats.storageType} storage, ~${stats.memoryEstimateKB.toFixed(1)}KB`);
            }
            
            // Clear the buffer with proper cleanup after sending samples
            bufferStorage.clear();
            window.audioData.hasActiveSamples = false;
            
            continue; // Skip the comparison below for audio
          }
        }
      } else {
        currentValue = parseFloat(element.value);
      }
      
      // For non-audio elements, only include if changed
      if (this.previousUiState[id] !== currentValue) {
        // console.log(`*** UI CHANGE: '${id}' changed from ${this.previousUiState[id]} to ${currentValue} ***`);
        changes[id] = currentValue;
        hasChanges = true;
        this.previousUiState[id] = currentValue;
      }
    }

    if (hasChanges) {
      // Send changes directly without wrapping in "value" objects
      const transformedChanges = {};
      for (const [id, value] of Object.entries(changes)) {
        const key = `${id}`;
        transformedChanges[key] = value;
      }
      // console.log('*** SENDING TO BACKEND:', JSON.stringify(transformedChanges));
      
      // Check if there's audio data in the changes
      const audioKeys = Object.keys(changes).filter(key => 
        this.uiElements[key] && 
        this.uiElements[key].type === 'file' && 
        this.uiElements[key].accept === 'audio/*'
      );
      
      // Debug logging for audio processing (only when enabled)
      if (this.debugMode && audioKeys.length > 0) {
        audioKeys.forEach(key => {
          const audioData = changes[key];
          console.log(`🎵 UI Audio ${key}: ${audioData.length} samples sent to backend`);
        });
      }
      
      // Return the transformed format
      return transformedChanges;
    }
    
    return null;
  }

  addUiElements(jsonData) {
    console.log('UI elements added:', jsonData);
    const uiControlsContainer = document.getElementById(this.uiControlsId) || createUiControlsContainer();
    
    // Clear existing UI elements
    this.clearUiElements();
    
    let foundUi = false;
    const groupedElements = new Map();
    const ungroupedElements = [];

    // First pass: organize elements by group
    jsonData.forEach((data) => {
      console.log('data:', data);
      const { group } = data;
      const hasGroup = group !== '' && group !== undefined && group !== null;
      
      if (hasGroup) {
        console.log(`Group ${group} found, for item ${data.name}`);
        if (!groupedElements.has(group)) {
          groupedElements.set(group, []);
        }
        groupedElements.get(group).push(data);
      } else {
        ungroupedElements.push(data);
      }
    });

    // Second pass: create groups and add elements
    // Add ungrouped elements first
    if (ungroupedElements.length > 0) {
      const ungroupedContainer = this.createUngroupedContainer();
      uiControlsContainer.appendChild(ungroupedContainer);
      
      ungroupedElements.forEach((data) => {
        const control = this.createControlElement(data);
        if (control) {
          foundUi = true;
          ungroupedContainer.appendChild(control);
          this.registerControlElement(control, data);
        }
      });
    }

    // Add grouped elements
    for (const [groupName, elements] of groupedElements) {
      const groupInfo = this.createGroupContainer(groupName);
      uiControlsContainer.appendChild(groupInfo.container);
      
      elements.forEach((data) => {
        const control = this.createControlElement(data);
        if (control) {
          foundUi = true;
          groupInfo.content.appendChild(control);
          this.registerControlElement(control, data);
        }
      });
    }

    if (foundUi) {
      console.log('UI elements added, showing UI controls container');
      uiControlsContainer.classList.add('active');
    }
  }

  // Create a control element based on data type
  createControlElement(data) {
    if (data.type === 'title') {
      setTitle(data);
      return null; // Skip creating UI control for title
    }

    if (data.type === 'description') {
      setDescription(data);
      return null; // Skip creating UI control for description
    }

    if (data.type === 'help') {
      return createHelp(data); // Return the help element for insertion
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
    } else if (data.type === 'dropdown') {
      control = createDropdown(data);
    }

    return control;
  }

  // Register a control element for state tracking
  registerControlElement(control, data) {
    if (data.type === 'button') {
      this.uiElements[data.id] = control.querySelector('button');
    } else if (data.type === 'dropdown') {
      this.uiElements[data.id] = control.querySelector('select');
    } else {
      this.uiElements[data.id] = control.querySelector('input');
    }
    this.previousUiState[data.id] = data.value;
    
    if (this.debugMode) {
      console.log(`🎵 UI Registered element: ID '${data.id}' (${data.type}) - Total: ${Object.keys(this.uiElements).length}`);
    }
  }

  // Enable or disable debug logging
  setDebugMode(enabled) {
    this.debugMode = enabled;
    console.log(`🎵 UI Manager debug mode ${enabled ? 'enabled' : 'disabled'}`);
  }
}

// Global debug controls for UI Manager
if (typeof window !== 'undefined') {
  window.setUiDebug = function(enabled = true) {
    // Access the global UI manager instance if available
    if (window.uiManager && typeof window.uiManager.setDebugMode === 'function') {
      window.uiManager.setDebugMode(enabled);
    } else {
      console.warn('🎵 UI Manager instance not found. Debug mode will be applied when manager is created.');
      // Store the preference for when the manager is created
      window._pendingUiDebugMode = enabled;
    }
  };
}
