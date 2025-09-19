/**
 * FastLED UI Manager Module
 *
 * Comprehensive UI management system for FastLED WebAssembly applications.
 * Handles dynamic UI element creation, user interaction processing, and layout management.
 *
 * Key features:
 * - Dynamic UI element creation from JSON configuration
 * - Responsive layout management with multi-column support
 * - Audio integration and file handling
 * - Markdown parsing for rich text descriptions
 * - Real-time change tracking and synchronization with FastLED
 * - Accessible UI controls with proper labeling
 * - Advanced layout optimization and grouping
 *
 * Supported UI elements:
 * - Sliders (range inputs with live value display)
 * - Checkboxes (boolean toggles)
 * - Buttons (momentary and toggle actions)
 * - Number fields (numeric inputs with validation)
 * - Dropdowns (select lists with options)
 * - Audio controls (file upload and playback)
 * - Help sections (expandable content with markdown support)
 *
 * @module UIManager
 */

/* eslint-disable no-console */
/* eslint-disable no-restricted-syntax */

/**
 * @typedef {Object} GroupInfo
 * @property {HTMLDivElement} container - The main container element
 * @property {HTMLDivElement} content - The content container element
 * @property {string} name - The group name
 * @property {boolean} isWide - Whether the group is wide
 * @property {boolean} isFullWidth - Whether the group is full width
 * @property {HTMLElement} parentContainer - The parent container element
 */
/* eslint-disable max-len */
/* eslint-disable guard-for-in */

import { AudioManager } from './audio_manager.js';
import { UILayoutPlacementManager } from './ui_layout_placement_manager.js';
import { UIRecorder } from './ui_recorder.js';

/** Global instance of AudioManager for audio processing */
const audioManager = new AudioManager();

// Make setupAudioAnalysis available globally
window.setupAudioAnalysis = function (audioElement) {
  return audioManager.setupAudioAnalysis(audioElement);
};

/**
 * Simple markdown to HTML converter
 * Supports: headers, bold, italic, code, links, lists, and paragraphs
 * @param {string} markdown - Markdown text to convert
 * @returns {string} HTML string with converted markdown
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
  html = html.replace(
    /(<li(?:\s+class="ordered")?>.*?<\/li>(?:\s*<li(?:\s+class="ordered")?>.*?<\/li>)*)/gs,
    (match) => {
      if (match.includes('class="ordered"')) {
        return `<ol>${match.replace(/\s+class="ordered"/g, '')}</ol>`;
      }
      return `<ul>${match}</ul>`;
    },
  );

  // Convert line breaks to paragraphs (double newlines become paragraph breaks)
  const paragraphs = html.split(/\n\s*\n/);
  html = paragraphs.map((p) => {
    const trimmed = p.trim();
    if (
      trimmed && !trimmed.startsWith('<h') && !trimmed.startsWith('<ul')
      && !trimmed.startsWith('<ol') && !trimmed.startsWith('<pre')
    ) {
      return `<p>${trimmed.replace(/\n/g, '<br>')}</p>`;
    }
    return trimmed;
  }).join('\n');

  return html;
}

/**
 * Groups adjacent number fields into pairs for more efficient space utilization
 * @param {Array} elements - Array of UI element configurations
 * @returns {Array} Array with adjacent number fields grouped into pairs
 */
function groupAdjacentNumberFields(elements) {
  const result = [];
  let i = 0;

  while (i < elements.length) {
    const current = elements[i];

    // Check if current element is a number field and the next one is also a number field
    if (current.type === 'number' && i + 1 < elements.length && elements[i + 1].type === 'number') {
      const next = elements[i + 1];

      // Create a paired element that will be handled specially
      result.push({
        type: 'number-pair',
        leftElement: current,
        rightElement: next,
        id: `pair-${current.id}-${next.id}`,
        group: current.group, // Use the group from the first element
      });

      i += 2; // Skip both elements since we've paired them
    } else {
      // Add single element as-is
      result.push(current);
      i += 1;
    }
  }

  return result;
}

/**
 * Creates a number input field UI element
 * @param {Object} element - Element configuration object
 * @param {string} element.name - Display name for the input
 * @param {string} element.id - Unique identifier for the input
 * @param {number} element.value - Current/default value
 * @param {number} element.min - Minimum allowed value
 * @param {number} element.max - Maximum allowed value
 * @param {number} [element.step] - Step increment (defaults to 'any')
 * @returns {HTMLDivElement} Container div with label and number input
 */
function createNumberField(element) {
  return createSingleNumberField(element);
}

/**
 * Creates a paired number input field UI element with two number fields side by side
 * @param {Object} leftElement - Left element configuration object
 * @param {Object} rightElement - Right element configuration object
 * @returns {HTMLDivElement} Container div with two number fields in a flex layout
 */
function createNumberFieldPair(leftElement, rightElement) {
  const pairContainer = document.createElement('div');
  pairContainer.className = 'ui-control number-pair-control';
  pairContainer.style.display = 'flex';
  pairContainer.style.gap = '20px';
  pairContainer.style.alignItems = 'center';
  pairContainer.style.justifyContent = 'space-between';

  // Create left number field
  const leftField = createSingleNumberField(leftElement);
  leftField.style.flex = '1';
  leftField.style.maxWidth = 'calc(50% - 10px)';

  // Create right number field
  const rightField = createSingleNumberField(rightElement);
  rightField.style.flex = '1';
  rightField.style.maxWidth = 'calc(50% - 10px)';

  pairContainer.appendChild(leftField);
  pairContainer.appendChild(rightField);

  // Store reference to both elements for later registration
  /** @type {HTMLDivElement & {_leftElement?: any, _rightElement?: any, _leftControl?: any, _rightControl?: any}} */
  const containerWithProps = /** @type {any} */ (pairContainer);
  containerWithProps._leftElement = leftElement;
  containerWithProps._rightElement = rightElement;
  containerWithProps._leftControl = leftField;
  containerWithProps._rightControl = rightField;

  return pairContainer;
}

/**
 * Creates a single number input field UI element (used by both single and paired fields)
 * @param {Object} element - Element configuration object
 * @returns {HTMLDivElement} Container div with label and number input
 */
function createSingleNumberField(element) {
  const controlDiv = document.createElement('div');
  controlDiv.className = 'ui-control number-control inline-row single-number-field';

  const label = document.createElement('label');
  label.textContent = element.name;
  label.htmlFor = `number-${element.id}`;
  label.style.display = 'inline-block';
  label.style.verticalAlign = 'middle';
  label.style.fontWeight = '500';
  label.style.color = '#E0E0E0';
  label.style.marginRight = '10px';
  label.style.fontSize = '0.9em'; // Slightly smaller for paired layout

  const numberInput = document.createElement('input');
  numberInput.type = 'number';
  numberInput.id = `number-${element.id}`;
  numberInput.value = element.value;
  numberInput.min = element.min;
  numberInput.max = element.max;
  numberInput.step = (element.step !== undefined) ? element.step : 'any';
  numberInput.style.display = 'inline-block';
  numberInput.style.verticalAlign = 'middle';
  numberInput.style.width = '60px';
  numberInput.style.boxSizing = 'border-box';

  controlDiv.appendChild(label);
  controlDiv.appendChild(numberInput);

  return controlDiv;
}

/**
 * Creates an audio input field UI element
 * Delegates to the AudioManager for specialized audio handling
 * @param {Object} element - Element configuration object for audio control
 * @returns {HTMLElement} Audio control element from AudioManager
 */
function createAudioField(element) {
  return audioManager.createAudioField(element);
}

/**
 * Creates a slider (range) input UI element with live value display
 * @param {Object} element - Element configuration object
 * @param {string} element.name - Display name for the slider
 * @param {string} element.id - Unique identifier for the slider
 * @param {number} element.value - Current/default value
 * @param {number} element.min - Minimum allowed value
 * @param {number} element.max - Maximum allowed value
 * @param {number} element.step - Step increment for the slider
 * @returns {HTMLDivElement} Container div with label, value display, and slider
 */
function createSlider(element) {
  const controlDiv = document.createElement('div');
  controlDiv.className = 'ui-control slider-control';

  // Create the slider container with relative positioning
  const sliderContainer = document.createElement('div');
  sliderContainer.className = 'slider-container';

  // Create the slider input
  const slider = document.createElement('input');
  slider.type = 'range';
  slider.id = `slider-${element.id}`;
  slider.setAttribute('min', String(Number.parseFloat(String(element.min))));
  slider.setAttribute('max', String(Number.parseFloat(String(element.max))));
  slider.setAttribute('value', String(Number.parseFloat(String(element.value))));

  // Check if element.step exists and is not undefined/null
  if (element.step !== undefined && element.step !== null) {
    slider.setAttribute('step', String(Number.parseFloat(String(element.step))));
  } else {
    // Set a default step
    slider.step = 'any';
  }

  // Create the overlay label div
  const overlayDiv = document.createElement('div');
  overlayDiv.className = 'slider-label-overlay';

  const labelText = document.createElement('span');
  labelText.className = 'label-text';
  labelText.textContent = element.name;

  const valueDisplay = document.createElement('span');
  valueDisplay.className = 'slider-value';
  valueDisplay.textContent = String(element.value);

  overlayDiv.appendChild(labelText);
  overlayDiv.appendChild(valueDisplay);

  // Set initial value in next frame to ensure proper initialization
  setTimeout(() => {
    slider.setAttribute('value', String(Number.parseFloat(String(element.value))));
    valueDisplay.textContent = slider.value;
  }, 0);

  // Update value display when slider changes
  slider.addEventListener('input', () => {
    valueDisplay.textContent = slider.value;
  });

  // Add elements to container
  sliderContainer.appendChild(slider);
  sliderContainer.appendChild(overlayDiv);
  controlDiv.appendChild(sliderContainer);

  return controlDiv;
}

/**
 * Creates a checkbox UI element for boolean values
 * @param {Object} element - Element configuration object
 * @param {string} element.name - Display name for the checkbox
 * @param {string} element.id - Unique identifier for the checkbox
 * @param {boolean} element.value - Current/default checked state
 * @returns {HTMLDivElement} Container div with label and checkbox
 */
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

/**
 * Creates a button UI element with press/release state tracking
 * @param {Object} element - Element configuration object
 * @param {string} element.name - Display text for the button
 * @param {string} element.id - Unique identifier for the button
 * @returns {HTMLDivElement} Container div with configured button element
 */
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
    ? `${markdownContent.substring(0, 200).trim()}...`
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
        font-size: 16px;
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

/**
 * Main UI Manager class for FastLED WebAssembly applications
 * Handles dynamic UI creation, change tracking, and synchronization with the FastLED backend
 */
export class JsonUiManager {
  /**
   * Creates a new JsonUiManager instance
   * @param {string} uiControlsId - HTML element ID where UI controls should be rendered
   */
  constructor(uiControlsId) {
    // console.log('*** JsonUiManager JS: CONSTRUCTOR CALLED ***');

    /** @type {Object} Map of UI element IDs to DOM elements */
    this.uiElements = {};

    /** @type {Object} Previous state values for change detection */
    this.previousUiState = {};

    /** @type {string} HTML element ID for the UI controls container */
    this.uiControlsId = uiControlsId;

    /** @type {string} HTML element ID for the second UI controls container (ultra-wide mode) */
    this.uiControls2Id = 'ui-controls-2';

    /** @type {Map<string, GroupInfo>} Track created UI groups */
    this.groups = new Map();

    /** @type {Map<string, GroupInfo>} Track created UI groups in second container */
    this.groups2 = new Map();

    /** @type {HTMLElement|null} Container for ungrouped UI items */
    this.ungroupedContainer = null;

    /** @type {HTMLElement|null} Container for ungrouped UI items in second container */
    this.ungroupedContainer2 = null;

    /** @type {boolean} Enable debug logging for UI operations */
    this.debugMode = false;

    /** @type {number} Counter to track UI element distribution in ultra-wide mode */
    this.elementDistributionIndex = 0;

    /** @type {Object} Configuration for container spillover thresholds */
    this.spilloverConfig = {
      // For tablet/desktop (2-container layouts): need at least 4 groups or 8 total elements
      twoContainer: {
        minGroups: 4, // At least 4 groups before using second container
        minElements: 8, // At least 8 total elements before using second container
        minElementsPerGroup: 2, // Average elements per group threshold
      },
      // For ultrawide (3-container layouts): need at least 6 groups or 12 total elements
      threeContainer: {
        minGroups: 6, // At least 6 groups before using all three areas
        minElements: 12, // At least 12 total elements before using all three areas
        minElementsPerGroup: 2, // Average elements per group threshold
      },
    };

    /** @type {Array|null} Stored JSON data for rebuilding UI on layout changes */
    this.lastJsonData = null;

    /** @type {string|null} Current layout mode for tracking transitions */
    this.currentLayout = null;

    // Initialize the UI Layout Placement Manager
    /** @type {UILayoutPlacementManager} Responsive layout management */
    this.layoutManager = new UILayoutPlacementManager();

    // Initialize the UI Recorder
    /** @type {UIRecorder|null} UI recording functionality */
    this.uiRecorder = null;

    /** @type {ReturnType<typeof setTimeout>|null} Timeout for debounced element redistribution */
    this.redistributionTimeout = null;

    // Timing constants
    this.LAYOUT_OPTIMIZATION_DELAY = 150; // ms

    // REMOVED legacy media query listeners to prevent duplicate event handling
    // The UILayoutPlacementManager now handles all layout detection and changes

    // Listen for custom layout events from the enhanced layout manager
    // This is the single source of truth for layout changes
    globalThis.addEventListener('layoutChanged', (e) => {
      this.onAdvancedLayoutChange(e.detail);
    });

    // Apply any pending debug mode setting
    if (window._pendingUiDebugMode !== undefined) {
      this.setDebugMode(window._pendingUiDebugMode);
      delete window._pendingUiDebugMode;
    }

    // Apply any pending spillover configuration
    if (window._pendingSpilloverConfig !== undefined) {
      this.updateSpilloverConfig(window._pendingSpilloverConfig);
      delete window._pendingSpilloverConfig;
    }
  }

  /**
   * Initialize the UI manager
   * @returns {Promise<void>} Promise that resolves when initialization is complete
   */
  async initialize() {
    // Initialize the layout manager if not already done
    if (!this.layoutManager) {
      this.layoutManager = new UILayoutPlacementManager();
    }

    // Initialize UI recorder if not already done
    if (!this.uiRecorder) {
      this.uiRecorder = new UIRecorder();
    }

    // Set up any additional initialization as needed
    this.initializationComplete = true;
  }

  /**
   * Update a slider element with a new value
   * @param {string} name - Name/ID of the slider element
   * @param {number} value - New value for the slider
   */
  updateSlider(name, value) {
    const element = this.uiElements[name];
    if (element && element.type === 'range') {
      element.value = value;

      // Update the value display if it exists
      const valueDisplay = document.getElementById(`${name}_value`);
      if (valueDisplay) {
        valueDisplay.textContent = String(value);
      }

      // Trigger change event
      element.dispatchEvent(new Event('input', { bubbles: true }));
    }
  }

  /**
   * Updates UI components from backend data (called by C++ backend)
   * Processes JSON updates and synchronizes UI element states
   * @param {string} jsonString - JSON string containing UI element updates
   */
  updateUiComponents(jsonString) {
    // console.log('*** C++→JS: Backend update received:', jsonString);

    // Log the inbound update to the inspector
    if (window.jsonInspector) {
      window.jsonInspector.logInboundEvent(jsonString, 'C++ → JS');
    }

    try {
      const updates = JSON.parse(jsonString);

      // Process each update
      for (const [elementId, updateData] of Object.entries(updates)) {
        // Strip 'id_' prefix if present to match our element storage
        const actualElementId = elementId.startsWith('id_') ? elementId.substring(3) : elementId;

        const element = this.uiElements[actualElementId];
        if (element && element.parentNode) {
          // Extract value from update data
          const value = updateData.value !== undefined ? updateData.value : updateData;
          const previousValue = this.previousUiState[actualElementId];

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

          // Record the update if recorder is active
          if (this.uiRecorder && previousValue !== value) {
            this.uiRecorder.recordElementUpdate(actualElementId, value, previousValue);
          }

          // Update our internal state tracking
          this.previousUiState[actualElementId] = value;
          // console.log(`*** C++→JS: Updated UI element '${actualElementId}' = ${value} ***`);
        } else {
          // Element not found or removed from DOM, clean it up
          if (this.uiElements[actualElementId] && !this.uiElements[actualElementId].parentNode) {
            delete this.uiElements[actualElementId];
            delete this.previousUiState[actualElementId];
            console.log(`*** UI Manager: Cleaned up orphaned element '${actualElementId}' ***`);
          }
          // console.warn(`*** C++→JS: Element '${actualElementId}' not found ***`);
        }
      }
    } catch (error) {
      console.error('*** C++→JS: Update error:', error, 'JSON:', jsonString);
    }
  }

  /**
   * Cleans up orphaned UI elements that have been removed from the DOM
   * but are still referenced in our internal tracking
   */
  cleanupOrphanedElements() {
    const orphanedIds = [];

    for (const id in this.uiElements) {
      if (!Object.prototype.hasOwnProperty.call(this.uiElements, id)) continue;

      const element = this.uiElements[id];
      if (!element || !element.parentNode) {
        orphanedIds.push(id);
      }
    }

    if (orphanedIds.length > 0) {
      console.log(`*** UI Manager: Cleaning up ${orphanedIds.length} orphaned elements:`, orphanedIds);

      for (const id of orphanedIds) {
        delete this.uiElements[id];
        delete this.previousUiState[id];
      }
    }
  }

  /**
   * Creates a collapsible group container for organizing UI elements
   * @param {string} groupName - Name of the group (displayed as header)
   * @param {HTMLElement} [targetContainer] - Specific container to use (optional)
   * @param {number} [totalGroups] - Total number of groups for spillover analysis
   * @param {number} [totalElements] - Total number of elements for spillover analysis
   * @returns {GroupInfo} The group info object
   */
  createGroupContainer(groupName, targetContainer = null, totalGroups = 0, totalElements = 0) {
    // First check if group already exists in ANY container and return it
    const existingGroup = this.findExistingGroup(groupName);
    if (existingGroup) {
      return existingGroup;
    }

    // Determine which container to use - but ensure groups stay together
    const container = targetContainer || this.getTargetContainerForGroup(groupName, totalGroups, totalElements);
    const groupsMap = this.getGroupsForContainer(container);

    const groupDiv = document.createElement('div');
    groupDiv.className = 'ui-group';
    groupDiv.id = `group-${groupName}`;

    // Add data attributes for layout optimization
    groupDiv.setAttribute('data-group-name', groupName);
    groupDiv.setAttribute('data-container', container.id);

    // Analyze group name to determine if it should be wide or full-width
    const isWideGroup = this.shouldBeWideGroup(groupName);
    const isFullWidthGroup = this.shouldBeFullWidthGroup(groupName);

    if (isFullWidthGroup) {
      groupDiv.classList.add('full-width');
    } else if (isWideGroup) {
      groupDiv.classList.add('wide-group');
    }

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

      // Trigger layout recalculation after animation
      setTimeout(() => {
        if (this.layoutManager) {
          this.layoutManager.forceLayoutUpdate();
        }
      }, 300);
    });

    groupDiv.appendChild(headerDiv);
    groupDiv.appendChild(contentDiv);

    const groupInfo = {
      container: groupDiv,
      content: contentDiv,
      name: groupName,
      isWide: isWideGroup,
      isFullWidth: isFullWidthGroup,
      parentContainer: container,
    };

    // Store in appropriate groups map
    groupsMap.set(groupName, groupInfo);

    // Append to the target container
    container.appendChild(groupDiv);

    if (this.debugMode) {
      console.log(`🎵 Created group "${groupName}" in container ${container.id}`);
    }

    return groupInfo;
  }

  /**
   * Determine if a group should span multiple columns
   */
  shouldBeWideGroup(groupName) {
    const wideGroupPatterns = [
      /audio/i,
      /spectrum/i,
      /visualization/i,
      /advanced/i,
      /settings/i,
    ];

    return wideGroupPatterns.some((pattern) => pattern.test(groupName));
  }

  /**
   * Determine if a group should span all columns
   */
  shouldBeFullWidthGroup(groupName) {
    const fullWidthPatterns = [
      /debug/i,
      /output/i,
      /console/i,
      /log/i,
    ];

    return fullWidthPatterns.some((pattern) => pattern.test(groupName));
  }

  // Clear all UI elements and groups
  clearUiElements() {
    // Record removal of all elements if recorder is active
    if (this.uiRecorder) {
      for (const elementId in this.uiElements) {
        if (Object.prototype.hasOwnProperty.call(this.uiElements, elementId)) {
          this.uiRecorder.recordElementRemove(elementId);
        }
      }
    }

    const uiControlsContainer = document.getElementById(this.uiControlsId);
    if (uiControlsContainer) {
      uiControlsContainer.innerHTML = '';
    }

    const uiControls2Container = document.getElementById(this.uiControls2Id);
    if (uiControls2Container) {
      uiControls2Container.innerHTML = '';
    }

    // Remove any tooltips that were added to document.body
    const tooltips = document.querySelectorAll('.ui-help-tooltip');
    tooltips.forEach((tooltip) => tooltip.remove());

    this.groups.clear();
    this.groups2.clear();
    this.ungroupedContainer = null;
    this.ungroupedContainer2 = null;
    this.uiElements = {};
    this.previousUiState = {};

    // Reset element distribution counter for ultra-wide mode
    this.elementDistributionIndex = 0;

    if (this.debugMode) {
      console.log('🎵 Cleared all UI elements and reset distribution');
    }
  }

  // Returns a Json object if there are changes, otherwise null.
  processUiChanges() {
    const changes = {}; // Json object to store changes.
    let hasChanges = false;

    for (const id in this.uiElements) {
      if (!Object.prototype.hasOwnProperty.call(this.uiElements, id)) continue;
      const element = this.uiElements[id];

      // Check if element is null or has been removed from DOM
      if (!element || !element.parentNode) {
        // Element has been removed, clean it up
        delete this.uiElements[id];
        continue;
      }

      let currentValue;
      if (element.type === 'checkbox') {
        currentValue = element.checked;
      } else if (element.type === 'submit') {
        const attr = element.getAttribute('data-pressed');
        currentValue = attr === 'true';
      } else if (element.type === 'number') {
        currentValue = parseFloat(element.value);
      } else if (element.tagName === 'SELECT') {
        currentValue = parseInt(element.value, 10);
      } else if (element.type === 'file' && element.accept === 'audio/*') {
        // Handle audio input - get all accumulated sample blocks with timestamps
        if (
          window.audioData && window.audioData.audioBuffers && window.audioData.hasActiveSamples
        ) {
          const bufferStorage = window.audioData.audioBuffers[element.id];

          if (bufferStorage && bufferStorage.getBufferCount() > 0) {
            // Get all samples using the optimized storage system
            const samples = bufferStorage.getAllSamples();
            changes[id] = samples;
            hasChanges = true;

            // Debug logging for audio stats (only when enabled)
            if (this.debugMode) {
              const stats = bufferStorage.getStats();
              console.log(
                `🎵 UI Audio ${id}: ${stats.bufferCount} blocks, ${stats.totalSamples} samples, ${stats.storageType} storage, ~${
                  stats.memoryEstimateKB.toFixed(1)
                }KB`,
              );
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
      const audioKeys = Object.keys(changes).filter((key) => this.uiElements[key]
        && this.uiElements[key].type === 'file'
        && this.uiElements[key].accept === 'audio/*');

      // Debug logging for audio processing (only when enabled)
      if (this.debugMode && audioKeys.length > 0) {
        audioKeys.forEach((key) => {
          const audioData = changes[key];
          console.log(`🎵 UI Audio ${key}: ${audioData.length} samples sent to backend`);
        });
      }

      // Log outbound changes to the inspector
      if (window.jsonInspector) {
        window.jsonInspector.logOutboundEvent(transformedChanges, 'JS → C++');
      }

      // Return the transformed format
      return transformedChanges;
    }

    return null;
  }

  addUiElements(jsonData) {
    console.log('UI elements added:', jsonData);

    // Store the JSON data for potential layout rebuilds
    this.lastJsonData = JSON.parse(JSON.stringify(jsonData));

    // Clear existing UI elements
    this.clearUiElements();

    let foundUi = false;
    const groupedElements = new Map();
    let ungroupedElements = [];

    // First pass: organize elements by group and analyze layout requirements
    jsonData.forEach((data) => {
      console.log('data:', data);
      const { group } = data;
      const hasGroup = group !== '' && group !== undefined && group !== null;

      // Add layout hints based on element type
      this.addElementLayoutHints(data);

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

    // Apply number field grouping to ungrouped elements
    ungroupedElements = groupAdjacentNumberFields(ungroupedElements);

    // Apply number field grouping to each group
    for (const [groupName, elements] of groupedElements.entries()) {
      const groupedElementsArray = groupAdjacentNumberFields(elements);
      groupedElements.set(groupName, groupedElementsArray);
    }

    // Optimize layout based on current screen size and element count
    this.optimizeLayoutForElements(groupedElements, ungroupedElements);

    // Second pass: create groups and add elements with smart container distribution
    // First, analyze total content to determine if we need multiple containers
    const totalGroups = groupedElements.size;
    const totalElements = jsonData.length;
    const sortedGroups = this.sortGroupsForOptimalLayout(groupedElements);
    const groupContainerMap = new Map();

    if (this.debugMode) {
      console.log(`🎵 UI Content Analysis: ${totalGroups} groups, ${totalElements} total elements`);
    }

    // Pre-assign groups to containers to prevent splitting
    for (const [groupName] of sortedGroups) {
      const groupInfo = this.createGroupContainer(groupName, null, totalGroups, totalElements);
      groupContainerMap.set(groupName, groupInfo);
    }

    // Add ungrouped elements, distributing across containers intelligently
    if (ungroupedElements.length > 0) {
      let ungroupedIndex = 0;
      ungroupedElements.forEach((data) => {
        // For ungrouped elements, still distribute but consider existing group balance
        const targetContainer = this.getBalancedTargetContainer(ungroupedIndex, totalGroups, totalElements);
        const ungroupedContainer = this.getUngroupedContainer(targetContainer);
        ungroupedIndex++;

        const control = this.createControlElement(data);
        if (control) {
          foundUi = true;
          ungroupedContainer.appendChild(control);
          this.registerControlElement(control, data);
        }
      });
    }

    // Add grouped elements (groups are already created and assigned containers)
    for (const [groupName, elements] of sortedGroups) {
      const groupInfo = groupContainerMap.get(groupName);

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
      console.log('UI elements added, showing UI controls containers');

      // Show main container
      const uiControlsContainer = document.getElementById(this.uiControlsId);
      if (uiControlsContainer) {
        uiControlsContainer.classList.add('active');
      }

      // Show secondary container if it has content
      const uiControls2Container = document.getElementById(this.uiControls2Id);
      if (uiControls2Container && uiControls2Container.children.length > 0) {
        uiControls2Container.classList.add('active');
      }

      // CRITICAL FIX: Force layout re-application now that UI elements are ready
      // This ensures the layout manager detects the UI elements and doesn't hide containers
      if (this.layoutManager) {
        this.layoutManager.forceLayoutUpdate();
      }

      // Trigger layout optimization after UI is visible
      setTimeout(() => {
        this.optimizeCurrentLayout();
      }, 100);

      if (this.debugMode) {
        const main1Count = uiControlsContainer ? uiControlsContainer.children.length : 0;
        const main2Count = uiControls2Container ? uiControls2Container.children.length : 0;
        console.log(
          `🎵 UI Distribution: Container 1: ${main1Count} elements, Container 2: ${main2Count} elements`,
        );
      }
    }
  }

  /**
   * Add layout hints to UI elements based on their type and properties
   */
  addElementLayoutHints(data) {
    // Mark elements that might benefit from wider layouts
    try {
      if (
        data.type === 'audio'
        || data.type === 'slider' && data.name.toLowerCase().includes('spectrum')
      ) {
        data._layoutHint = 'wide';
      }

      // Mark elements that should always be full width
      if (data.type === 'help' || data.name.toLowerCase().includes('debug')) {
        data._layoutHint = 'full-width';
      }
    } catch (e) {
      console.log('Error adding element layout hints:', e, data);
    }
  }

  /**
   * Optimize layout distribution based on element analysis
   */
  optimizeLayoutForElements(groupedElements, ungroupedElements) {
    const layoutInfo = this.layoutManager.getLayoutInfo();
    const totalGroups = groupedElements.size;
    const totalUngrouped = ungroupedElements.length;

    if (this.debugMode) {
      console.log(
        `🎵 UI Layout optimization: ${totalGroups} groups, ${totalUngrouped} ungrouped, ${layoutInfo.uiColumns} columns available`,
      );
    }

    // Suggest layout adjustments to the layout manager if needed
    if (layoutInfo.uiColumns > 1 && totalGroups > layoutInfo.uiColumns) {
      // Many groups with multiple columns available - optimize for density
      this.requestLayoutOptimization('dense');
    } else if (layoutInfo.uiColumns === 1 && (totalGroups + totalUngrouped) > 10) {
      // Single column with many elements - consider requesting more space
      this.requestLayoutOptimization('expand');
    }
  }

  /**
   * Sort groups for optimal multi-column layout
   */
  sortGroupsForOptimalLayout(groupedElements) {
    const layoutInfo = this.layoutManager.getLayoutInfo();

    if (layoutInfo.uiColumns <= 1) {
      // Single column - return as-is
      return Array.from(groupedElements.entries());
    }

    // Multi-column layout - optimize placement
    const groups = Array.from(groupedElements.entries());

    // Sort by priority: full-width first, then wide, then regular
    return groups.sort(([nameA, elementsA], [nameB, elementsB]) => {
      const priorityA = this.getGroupLayoutPriority(nameA);
      const priorityB = this.getGroupLayoutPriority(nameB);

      if (priorityA !== priorityB) {
        return priorityB - priorityA; // Higher priority first
      }

      // Same priority - sort by element count (more elements first)
      return elementsB.length - elementsA.length;
    });
  }

  /**
   * Get layout priority for group ordering
   */
  getGroupLayoutPriority(groupName) {
    if (this.shouldBeFullWidthGroup(groupName)) return 3;
    if (this.shouldBeWideGroup(groupName)) return 2;
    return 1;
  }

  /**
   * Request layout optimization from the layout manager
   */
  requestLayoutOptimization(type) {
    if (this.debugMode) {
      console.log(`🎵 UI Requesting layout optimization: ${type}`);
    }

    // Could be extended to communicate with layout manager
    // for dynamic layout adjustments
  }

  /**
   * Optimize the current layout after UI elements are added
   */
  optimizeCurrentLayout() {
    const layoutInfo = this.layoutManager.getLayoutInfo();

    if (layoutInfo.uiColumns > 1) {
      this.balanceColumnHeights();
    }

    if (this.debugMode) {
      console.log(
        `🎵 UI Layout optimized for ${layoutInfo.mode} mode with ${layoutInfo.uiColumns} columns`,
      );
    }
  }

  /**
   * Balance column heights in grid layouts
   */
  balanceColumnHeights() {
    const layoutInfo = this.layoutManager.getLayoutInfo();

    const uiControlsContainer = document.getElementById(this.uiControlsId);
    const uiControls2Container = document.getElementById(this.uiControls2Id);

    // Handle ultra-wide mode with two separate containers
    if (
      layoutInfo.mode === 'ultrawide' && uiControls2Container
      && uiControls2Container.children.length > 0
    ) {
      // Balance between two containers
      const container1Groups = uiControlsContainer.querySelectorAll('.ui-group');
      const container2Groups = uiControls2Container.querySelectorAll('.ui-group');

      let height1 = 0;
      let height2 = 0;

      container1Groups.forEach((group) => {
        height1 += /** @type {HTMLElement} */ (group).offsetHeight;
      });

      container2Groups.forEach((group) => {
        height2 += /** @type {HTMLElement} */ (group).offsetHeight;
      });

      if (this.debugMode) {
        console.log(
          `🎵 Grid Layout Heights - UI Container 1: ${height1}px, UI Container 2: ${height2}px`,
        );
        console.log(
          `🎵 Distribution: Container 1 has ${container1Groups.length} groups, Container 2 has ${container2Groups.length} groups`,
        );
      }

      // In grid layout, CSS Grid handles positioning, so we just report statistics
      const heightDiff = Math.abs(height1 - height2);
      if (heightDiff > 200 && this.debugMode) {
        console.log(
          `🎵 Height imbalance detected: ${heightDiff}px difference (handled by CSS Grid)`,
        );
      }
    } else if (this.debugMode) {
      // Single container layouts
      const groups = uiControlsContainer.querySelectorAll('.ui-group');

      if (groups.length > 0) {
        console.log(`🎵 Grid Layout: ${groups.length} groups in single column layout`);
      }
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
    } else if (data.type === 'number-pair') {
      control = createNumberFieldPair(data.leftElement, data.rightElement);
    } else if (data.type === 'audio') {
      control = createAudioField(data);
    } else if (data.type === 'dropdown') {
      control = createDropdown(data);
    }

    return control;
  }

  // Register a control element for state tracking
  registerControlElement(control, data) {
    if (data.type === 'number-pair') {
      // Register both left and right elements separately
      const { leftElement } = data;
      const { rightElement } = data;

      // Find the input elements within the paired control
      const leftInput = control._leftControl.querySelector('input');
      const rightInput = control._rightControl.querySelector('input');

      this.uiElements[leftElement.id] = leftInput;
      this.uiElements[rightElement.id] = rightInput;
      this.previousUiState[leftElement.id] = leftElement.value;
      this.previousUiState[rightElement.id] = rightElement.value;

      // Record element addition if recorder is active
      if (this.uiRecorder) {
        this.uiRecorder.recordElementAdd(leftElement.id, leftElement);
        this.uiRecorder.recordElementAdd(rightElement.id, rightElement);
      }

      if (this.debugMode) {
        console.log(
          `🎵 UI Registered paired elements: IDs '${leftElement.id}' and '${rightElement.id}' (number-pair) - Total: ${Object.keys(this.uiElements).length}`,
        );
      }
    } else if (data.type === 'button') {
      this.uiElements[data.id] = control.querySelector('button');
      this.previousUiState[data.id] = data.value;

      // Record element addition if recorder is active
      if (this.uiRecorder) {
        this.uiRecorder.recordElementAdd(data.id, data);
      }
    } else if (data.type === 'dropdown') {
      this.uiElements[data.id] = control.querySelector('select');
      this.previousUiState[data.id] = data.value;

      // Record element addition if recorder is active
      if (this.uiRecorder) {
        this.uiRecorder.recordElementAdd(data.id, data);
      }
    } else {
      this.uiElements[data.id] = control.querySelector('input');
      this.previousUiState[data.id] = data.value;

      // Record element addition if recorder is active
      if (this.uiRecorder) {
        this.uiRecorder.recordElementAdd(data.id, data);
      }
    }

    // Add layout classes based on element hints (only for non-paired elements)
    if (data.type !== 'number-pair') {
      if (data._layoutHint === 'wide') {
        control.classList.add('wide-control');
      } else if (data._layoutHint === 'full-width') {
        control.classList.add('full-width-control');
      }

      if (this.debugMode && data.type !== 'number-pair') {
        console.log(
          `🎵 UI Registered element: ID '${data.id}' (${data.type}${
            data._layoutHint ? `, ${data._layoutHint}` : ''
          }) - Total: ${Object.keys(this.uiElements).length}`,
        );
      }
    }
  }

  // Enable or disable debug logging
  setDebugMode(enabled) {
    this.debugMode = enabled;
    console.log(`🎵 UI Manager debug mode ${enabled ? 'enabled' : 'disabled'}`);

    // Store globally for layout manager access
    window.uiManager = this;
  }

  /**
   * Starts UI recording
   * @param {Object} [metadata] - Optional recording metadata
   * @returns {string|null} Recording ID or null if failed
   */
  startUIRecording(metadata = {}) {
    if (!this.uiRecorder) {
      this.uiRecorder = new UIRecorder({
        debugMode: this.debugMode,
        maxEvents: 50000
      });
    }

    return this.uiRecorder.startRecording(metadata);
  }

  /**
   * Stops UI recording and returns the recording data
   * @returns {Object|null} Recording data or null if no recording
   */
  stopUIRecording() {
    if (this.uiRecorder) {
      return this.uiRecorder.stopRecording();
    }
    return null;
  }

  /**
   * Gets the current recording status
   * @returns {Object} Recording status
   */
  getUIRecordingStatus() {
    if (this.uiRecorder) {
      return this.uiRecorder.getStatus();
    }
    return { isRecording: false, eventCount: 0 };
  }

  /**
   * Exports current recording as JSON
   * @returns {string|null} JSON string or null
   */
  exportUIRecording() {
    if (this.uiRecorder) {
      return this.uiRecorder.exportRecording();
    }
    return null;
  }

  /**
   * Clears the current recording
   */
  clearUIRecording() {
    if (this.uiRecorder) {
      this.uiRecorder.clearRecording();
    }
  }

  /**
   * Update spillover configuration thresholds
   * @param {Object} newConfig - New spillover configuration
   * @param {Object} newConfig.twoContainer - 2-container thresholds
   * @param {Object} newConfig.threeContainer - 3-container thresholds
   */
  updateSpilloverConfig(newConfig) {
    if (newConfig.twoContainer) {
      Object.assign(this.spilloverConfig.twoContainer, newConfig.twoContainer);
    }
    if (newConfig.threeContainer) {
      Object.assign(this.spilloverConfig.threeContainer, newConfig.threeContainer);
    }

    if (this.debugMode) {
      console.log('🎵 Updated spillover configuration:', this.spilloverConfig);
    }
  }

  /**
   * Get current spillover configuration
   * @returns {Object} Current spillover configuration
   */
  getSpilloverConfig() {
    return JSON.parse(JSON.stringify(this.spilloverConfig));
  }

  // Handle layout changes (LEGACY - now deprecated)
  // This method is kept for backward compatibility but should not be actively used
  onLayoutChange(layoutMode) {
    if (this.debugMode) {
      console.log(`🎵 UI Manager: LEGACY layout change to ${layoutMode} (consider using onAdvancedLayoutChange instead)`);
    }

    // The new onAdvancedLayoutChange method handles all layout changes
    // This method now just serves as a fallback to avoid breaking existing code

    // NOTE: Do not duplicate redistribution logic here since onAdvancedLayoutChange
    // will be called by the UILayoutPlacementManager for the same resize event
  }

  /**
   * Redistribute UI elements from hidden containers to visible ones
   * This fixes the bug where elements disappear when the layout changes
   */
  redistributeElementsIfNeeded() {
    // Clear any pending redistribution to avoid multiple rapid calls
    if (this.redistributionTimeout) {
      clearTimeout(this.redistributionTimeout);
    }

    // Debounce redistribution to allow CSS transitions to complete
    this.redistributionTimeout = setTimeout(() => {
      this.performElementRedistribution();
    }, 100); // Wait for CSS transitions to complete
  }

  /**
   * Actually perform the element redistribution after debouncing
   * @private
   */
  performElementRedistribution() {
    const uiControls2Container = document.getElementById(this.uiControls2Id);
    if (!uiControls2Container) return;

    // Force a reflow to ensure CSS changes are applied
    void uiControls2Container.offsetHeight;

    // Check if the second container is hidden by CSS
    const containerStyle = window.getComputedStyle(uiControls2Container);
    const isSecondContainerVisible = containerStyle.display !== 'none'
                                   && containerStyle.visibility !== 'hidden'
                                   && containerStyle.opacity !== '0';

    if (!isSecondContainerVisible && uiControls2Container.children.length > 0) {
      if (this.debugMode) {
        console.log(`🎵 Moving ${uiControls2Container.children.length} elements from hidden ui-controls-2 to ui-controls`);
      }

      const mainContainer = document.getElementById(this.uiControlsId);
      if (mainContainer) {
        // Move all children from the hidden container to the main container
        while (uiControls2Container.children.length > 0) {
          const element = uiControls2Container.children[0];
          mainContainer.appendChild(element);
        }

        // Update our internal group tracking
        this.groups2.forEach((groupInfo, groupName) => {
          // Move group from groups2 to groups
          this.groups.set(groupName, {
            ...groupInfo,
            parentContainer: mainContainer,
          });
        });
        this.groups2.clear();

        // Reset ungrouped container reference
        this.ungroupedContainer2 = null;

        if (this.debugMode) {
          console.log(`🎵 Redistributed elements to main container. Groups in main: ${this.groups.size}, Groups in secondary: ${this.groups2.size}`);
        }
      }
    }
  }

  /**
   * Handle advanced layout changes from the enhanced layout system
   */
  onAdvancedLayoutChange(layoutDetail) {
    const { layout, data } = layoutDetail;

    if (this.debugMode) {
      console.log(`🎵 UI Manager: Advanced layout change to ${layout}:`, data);
    }

    // Check if we need to rebuild the entire UI layout
    const previousLayout = this.currentLayout;
    this.currentLayout = layout;

    if (this.shouldRebuildLayout(previousLayout, layout)) {
      if (this.debugMode) {
        console.log(`🎵 UI Manager: Rebuilding UI layout from ${previousLayout} to ${layout}`);
      }
      this.rebuildUIFromStoredData();
    } else {
      // CRITICAL FIX: Redistribute UI elements if second container becomes hidden
      // This is now the primary method for handling layout changes
      this.redistributeElementsIfNeeded();

      // Adjust UI elements based on new layout data
      this.adaptToLayoutData(data);
    }

    // Re-optimize layout for new mode
    setTimeout(() => {
      this.optimizeCurrentLayout();
    }, this.LAYOUT_OPTIMIZATION_DELAY); // Slightly longer delay to ensure redistribution completes first
  }

  /**
   * Determine if a full UI rebuild is needed based on layout transitions
   * @param {string|undefined} previousLayout - Previous layout mode
   * @param {string} newLayout - New layout mode
   * @returns {boolean} True if a full rebuild is needed
   */
  shouldRebuildLayout(previousLayout, newLayout) {
    // Always rebuild if we have stored JSON data and the layout mode changes significantly
    if (!this.lastJsonData || !previousLayout) {
      return false;
    }

    // Rebuild on significant layout transitions that affect column count
    const significantTransitions = [
      // Transitions between 1-column and multi-column layouts
      ['mobile', 'tablet'],
      ['mobile', 'desktop'],
      ['mobile', 'ultrawide'],
      ['tablet', 'mobile'],
      ['desktop', 'mobile'],
      ['ultrawide', 'mobile'],
      // Transitions between 2-column and 3-column layouts
      ['tablet', 'ultrawide'],
      ['desktop', 'ultrawide'],
      ['ultrawide', 'tablet'],
      ['ultrawide', 'desktop'],
    ];

    const transition = [previousLayout, newLayout];
    return significantTransitions.some(([from, to]) =>
      transition[0] === from && transition[1] === to
    );
  }

  /**
   * Rebuild the entire UI from stored JSON data
   * This ensures optimal layout distribution for the new screen size
   */
  rebuildUIFromStoredData() {
    if (!this.lastJsonData) {
      if (this.debugMode) {
        console.log('🎵 UI Manager: No stored JSON data available for rebuild');
      }
      return;
    }

    // Preserve current element values before rebuilding
    const currentValues = {};
    for (const [elementId, element] of Object.entries(this.uiElements)) {
      if (element && element.parentNode) {
        if (element.type === 'checkbox') {
          currentValues[elementId] = element.checked;
        } else if (element.tagName === 'SELECT') {
          currentValues[elementId] = element.selectedIndex;
        } else if (element.type === 'submit') {
          currentValues[elementId] = element.getAttribute('data-pressed') === 'true';
        } else {
          currentValues[elementId] = element.value;
        }
      }
    }

    // Rebuild UI from stored JSON
    this.addUiElements(this.lastJsonData);

    // Restore preserved values
    for (const [elementId, value] of Object.entries(currentValues)) {
      const element = this.uiElements[elementId];
      if (element && element.parentNode) {
        if (element.type === 'checkbox') {
          element.checked = Boolean(value);
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
      }
    }

    if (this.debugMode) {
      console.log('🎵 UI Manager: UI rebuilt from stored JSON data with preserved values');
    }
  }

  /**
   * Adapt UI elements to new layout constraints
   */
  adaptToLayoutData(layoutData) {
    const { uiColumns } = layoutData;

    // Update group layouts based on available columns
    this.groups.forEach((groupInfo) => {
      const { container } = groupInfo;

      // Adjust wide groups based on available columns
      if (groupInfo.isWide && uiColumns < 2) {
        container.classList.remove('wide-group');
      } else if (groupInfo.isWide && uiColumns >= 2) {
        container.classList.add('wide-group');
      }

      // Adjust full-width groups
      if (groupInfo.isFullWidth && uiColumns > 1) {
        container.classList.add('full-width');
      }
    });

    if (this.debugMode) {
      console.log(`🎵 UI Adapted ${this.groups.size} groups to ${uiColumns} columns`);
    }
  }

  // Get current layout information
  getLayoutInfo() {
    if (this.layoutManager) {
      return this.layoutManager.getLayoutInfo();
    }
    return null;
  }

  /**
   * Get a balanced target container for ungrouped elements
   * @param {number} elementIndex - Index of the current element
   * @param {number} totalGroups - Total number of groups
   * @param {number} totalElements - Total number of elements
   * @returns {HTMLElement} The target container
   */
  getBalancedTargetContainer(elementIndex, totalGroups = 0, totalElements = 0) {
    const layoutInfo = this.layoutManager.getLayoutInfo();

    // Check if we should even use multiple containers
    if (!this.shouldUseMultipleContainers(layoutInfo.mode, totalGroups, totalElements)) {
      return document.getElementById(this.uiControlsId);
    }

    if (layoutInfo.mode === 'ultrawide') {
      const container1 = document.getElementById(this.uiControlsId);
      const container2 = document.getElementById(this.uiControls2Id);

      if (container1 && container2) {
        // Balance based on total content (groups + ungrouped elements)
        const container1Elements = container1.children.length;
        const container2Elements = container2.children.length;

        // Use the container with fewer total elements
        if (container2Elements < container1Elements) {
          return container2;
        } if (container1Elements < container2Elements) {
          return container1;
        }
        // Equal - alternate
        return elementIndex % 2 === 0 ? container1 : container2;
      }
    }

    return document.getElementById(this.uiControlsId);
  }

  // Cleanup method to remove event listeners
  destroy() {
    if (this.layoutManager) {
      this.layoutManager.destroy();
      this.layoutManager = null;
    }

    // Clear any pending redistribution timeout
    if (this.redistributionTimeout) {
      clearTimeout(this.redistributionTimeout);
      this.redistributionTimeout = null;
    }

    globalThis.removeEventListener('layoutChanged', this.onAdvancedLayoutChange);
  }

  /**
   * Get the appropriate UI container based on current layout and element distribution
   * @returns {HTMLElement} The container where the next UI element should be placed
   */
  getTargetContainer() {
    const layoutInfo = this.layoutManager.getLayoutInfo();

    if (layoutInfo.mode === 'ultrawide') {
      // In ultra-wide mode, alternate between containers for better distribution
      const useSecondContainer = this.elementDistributionIndex % 2 === 1;
      this.elementDistributionIndex++;

      if (useSecondContainer) {
        const container2 = document.getElementById(this.uiControls2Id);
        if (container2) {
          return container2;
        }
      }
    }

    // Default to main container for all other layouts
    return document.getElementById(this.uiControlsId);
  }

  /**
   * Get the appropriate UI container for a specific group, ensuring groups don't get split
   * @param {string} groupName - Name of the group
   * @param {number} totalGroups - Total number of groups being created
   * @param {number} totalElements - Total number of UI elements
   * @returns {HTMLElement} The container where the group should be placed
   */
  getTargetContainerForGroup(groupName, totalGroups = 0, totalElements = 0) {
    const layoutInfo = this.layoutManager.getLayoutInfo();

    // Check if this group already exists in a container
    const existingGroup = this.findExistingGroup(groupName);
    if (existingGroup && existingGroup.parentContainer) {
      return existingGroup.parentContainer;
    }

    // Check if we should even use multiple containers based on content amount
    if (!this.shouldUseMultipleContainers(layoutInfo.mode, totalGroups, totalElements)) {
      return document.getElementById(this.uiControlsId);
    }

    if (layoutInfo.mode === 'ultrawide') {
      // For ultra-wide, try to balance containers by group count, not individual elements
      const container1 = document.getElementById(this.uiControlsId);
      const container2 = document.getElementById(this.uiControls2Id);

      if (container1 && container2) {
        const groups1Count = this.groups.size;
        const groups2Count = this.groups2.size;

        // Use the container with fewer groups
        if (groups2Count < groups1Count) {
          return container2;
        }
      }
    }

    // Default to main container
    return document.getElementById(this.uiControlsId);
  }

  /**
   * Determine if we should use multiple containers based on content amount
   * @param {string} layoutMode - Current layout mode
   * @param {number} totalGroups - Total number of groups
   * @param {number} totalElements - Total number of elements
   * @returns {boolean} Whether to use multiple containers
   */
  shouldUseMultipleContainers(layoutMode, totalGroups, totalElements) {
    if (layoutMode === 'mobile') {
      return false; // Mobile always uses single container
    }

    // CRITICAL FIX: Check if the second UI container is actually visible
    // This prevents the bug where elements get placed in hidden containers
    const uiControls2Container = document.getElementById(this.uiControls2Id);
    if (!uiControls2Container) {
      return false; // Second container doesn't exist
    }

    // Check if the second container is hidden by CSS (e.g., in tablet mode)
    const containerStyle = window.getComputedStyle(uiControls2Container);
    const isSecondContainerVisible = containerStyle.display !== 'none'
                                   && containerStyle.visibility !== 'hidden'
                                   && containerStyle.opacity !== '0';

    if (!isSecondContainerVisible) {
      if (this.debugMode) {
        console.log(`🎵 Second UI container is hidden by CSS in ${layoutMode} mode - using single container`);
      }
      return false; // Don't use multiple containers if the second one is hidden
    }

    let thresholds;
    if (layoutMode === 'ultrawide') {
      thresholds = this.spilloverConfig.threeContainer;
    } else if (layoutMode === 'tablet' || layoutMode === 'desktop') {
      thresholds = this.spilloverConfig.twoContainer;
    } else {
      return false;
    }

    // Check if we meet the minimum thresholds for spillover
    const hasEnoughGroups = totalGroups >= thresholds.minGroups;
    const hasEnoughElements = totalElements >= thresholds.minElements;
    const hasGoodGroupDensity = totalGroups > 0 && (totalElements / totalGroups) >= thresholds.minElementsPerGroup;

    const shouldSpill = hasEnoughGroups || (hasEnoughElements && hasGoodGroupDensity);

    if (this.debugMode) {
      console.log(`🎵 Spillover analysis for ${layoutMode}:`);
      console.log(`  Groups: ${totalGroups} (need ${thresholds.minGroups})`);
      console.log(`  Elements: ${totalElements} (need ${thresholds.minElements})`);
      console.log(`  Density: ${totalGroups > 0 ? (totalElements / totalGroups).toFixed(1) : 0} (need ${thresholds.minElementsPerGroup})`);
      console.log(`  Second container visible: ${isSecondContainerVisible}`);
      console.log(`  Result: ${shouldSpill ? 'USE MULTIPLE CONTAINERS' : 'USE SINGLE CONTAINER'}`);
    }

    return shouldSpill;
  }

  /**
   * Find an existing group across all containers
   * @param {string} groupName - Name of the group to find
   * @returns {GroupInfo|null} The group info object or null if not found
   */
  findExistingGroup(groupName) {
    if (this.groups.has(groupName)) {
      return this.groups.get(groupName);
    }
    if (this.groups2.has(groupName)) {
      return this.groups2.get(groupName);
    }
    return null;
  }

  /**
   * Get the appropriate groups map based on the target container
   * @param {HTMLElement} container - The target container
   * @returns {Map<string, GroupInfo>} The groups map for the container
   */
  getGroupsForContainer(container) {
    if (container && container.id === this.uiControls2Id) {
      return this.groups2;
    }
    return this.groups;
  }

  /**
   * Get the ungrouped container for the specified UI container
   * @param {HTMLElement} container - The target container
   * @returns {HTMLElement|null} The ungrouped container
   */
  getUngroupedContainer(container) {
    if (container && container.id === this.uiControls2Id) {
      if (!this.ungroupedContainer2) {
        this.ungroupedContainer2 = this.createUngroupedContainer(
          container,
          'ungrouped-container-2',
        );
      }
      return this.ungroupedContainer2;
    }

    if (!this.ungroupedContainer) {
      this.ungroupedContainer = this.createUngroupedContainer(
        document.getElementById(this.uiControlsId),
        'ungrouped-container',
      );
    }
    return this.ungroupedContainer;
  }

  /**
   * Create an ungrouped container for the specified parent
   * @param {HTMLElement} parent - The parent container
   * @param {string} id - The ID for the ungrouped container
   * @returns {HTMLElement} The created ungrouped container
   */
  createUngroupedContainer(parent, id) {
    if (!parent) return null;

    let container = document.getElementById(id);
    if (!container) {
      container = document.createElement('div');
      container.id = id;
      container.className = 'ui-group ungrouped';
      parent.appendChild(container);
    }
    return container;
  }
}

// Global debug and configuration controls for UI Manager
if (typeof window !== 'undefined') {
  window.setUiDebug = function (enabled = true) {
    // Access the global UI manager instance if available
    if (window.uiManager && typeof window.uiManager.setDebugMode === 'function') {
      window.uiManager.setDebugMode(enabled);
    } else {
      console.warn(
        '🎵 UI Manager instance not found. Debug mode will be applied when manager is created.',
      );
      // Store the preference for when the manager is created
      window._pendingUiDebugMode = enabled;
    }
  };

  window.setUiSpilloverThresholds = function (config) {
    if (window.uiManager && typeof window.uiManager.updateSpilloverConfig === 'function') {
      window.uiManager.updateSpilloverConfig(config);
    } else {
      console.warn(
        '🎵 UI Manager instance not found. Spillover config will be applied when manager is created.',
      );
      window._pendingSpilloverConfig = config;
    }
  };

  window.getUiSpilloverThresholds = function () {
    if (window.uiManager && typeof window.uiManager.getSpilloverConfig === 'function') {
      return window.uiManager.getSpilloverConfig();
    }
    console.warn('🎵 UI Manager instance not found.');
    return null;
  };

  // Example usage helper
  window.setUiSpilloverExample = function () {
    console.log('🎵 Example spillover configuration:');
    console.log('setUiSpilloverThresholds({');
    console.log('  twoContainer: { minGroups: 3, minElements: 6, minElementsPerGroup: 2 },');
    console.log('  threeContainer: { minGroups: 5, minElements: 10, minElementsPerGroup: 2 }');
    console.log('});');
  };
}
