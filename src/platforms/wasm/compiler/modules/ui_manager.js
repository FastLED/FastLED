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
/* eslint-disable import/prefer-default-export */
/* eslint-disable no-restricted-syntax */
/* eslint-disable max-len */
/* eslint-disable guard-for-in */

import { AudioManager } from './audio_manager.js';
import { UILayoutPlacementManager } from './ui_layout_placement_manager.js';

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
    function (match) {
      if (match.includes('class="ordered"')) {
        return '<ol>' + match.replace(/\s+class="ordered"/g, '') + '</ol>';
      } else {
        return '<ul>' + match + '</ul>';
      }
    },
  );

  // Convert line breaks to paragraphs (double newlines become paragraph breaks)
  const paragraphs = html.split(/\n\s*\n/);
  html = paragraphs.map((p) => {
    const trimmed = p.trim();
    if (
      trimmed && !trimmed.startsWith('<h') && !trimmed.startsWith('<ul') &&
      !trimmed.startsWith('<ol') && !trimmed.startsWith('<pre')
    ) {
      return '<p>' + trimmed.replace(/\n/g, '<br>') + '</p>';
    }
    return trimmed;
  }).join('\n');

  return html;
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

    /** @type {Map<string, HTMLElement>} Track created UI groups */
    this.groups = new Map();

    /** @type {HTMLElement|null} Container for ungrouped UI items */
    this.ungroupedContainer = null;

    /** @type {boolean} Enable debug logging for UI operations */
    this.debugMode = false;

    // Initialize the UI Layout Placement Manager
    /** @type {UILayoutPlacementManager} Responsive layout management */
    this.layoutManager = new UILayoutPlacementManager();

    // Listen for layout changes to potentially optimize UI element rendering
    this.layoutManager.mediaQuery
      ? this.layoutManager.mediaQuery.addEventListener('change', (e) => {
        this.onLayoutChange(e.matches ? 'desktop' : 'portrait');
      })
      // Handle new breakpoint system
      : Object.values(this.layoutManager.breakpoints).forEach((mq) => {
        mq.addEventListener('change', () => {
          this.onLayoutChange(this.layoutManager.currentLayout);
        });
      });

    // Listen for custom layout events from the enhanced layout manager
    globalThis.addEventListener('layoutChanged', (e) => {
      this.onAdvancedLayoutChange(e.detail);
    });

    // Apply any pending debug mode setting
    if (window._pendingUiDebugMode !== undefined) {
      this.setDebugMode(window._pendingUiDebugMode);
      delete window._pendingUiDebugMode;
    }
  }

  /**
   * Updates UI components from backend data (called by C++ backend)
   * Processes JSON updates and synchronizes UI element states
   * @param {string} jsonString - JSON string containing UI element updates
   */
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

  /**
   * Creates a collapsible group container for organizing UI elements
   * @param {string} groupName - Name of the group (displayed as header)
   * @returns {HTMLElement} The group container element
   */
  createGroupContainer(groupName) {
    if (this.groups.has(groupName)) {
      return this.groups.get(groupName);
    }

    const groupDiv = document.createElement('div');
    groupDiv.className = 'ui-group';
    groupDiv.id = `group-${groupName}`;

    // Add data attributes for layout optimization
    groupDiv.setAttribute('data-group-name', groupName);

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
    };

    this.groups.set(groupName, groupInfo);
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
    tooltips.forEach((tooltip) => tooltip.remove());

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
      if (!Object.prototype.hasOwnProperty.call(this.uiElements, id)) continue;
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
      const audioKeys = Object.keys(changes).filter((key) =>
        this.uiElements[key] &&
        this.uiElements[key].type === 'file' &&
        this.uiElements[key].accept === 'audio/*'
      );

      // Debug logging for audio processing (only when enabled)
      if (this.debugMode && audioKeys.length > 0) {
        audioKeys.forEach((key) => {
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
    const uiControlsContainer = document.getElementById(this.uiControlsId) ||
      createUiControlsContainer();

    // Clear existing UI elements
    this.clearUiElements();

    let foundUi = false;
    const groupedElements = new Map();
    const ungroupedElements = [];

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

    // Optimize layout based on current screen size and element count
    this.optimizeLayoutForElements(groupedElements, ungroupedElements);

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

    // Add grouped elements with optimized ordering
    const sortedGroups = this.sortGroupsForOptimalLayout(groupedElements);

    for (const [groupName, elements] of sortedGroups) {
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

      // Trigger layout optimization after UI is visible
      setTimeout(() => {
        this.optimizeCurrentLayout();
      }, 100);
    }
  }

  /**
   * Add layout hints to UI elements based on their type and properties
   */
  addElementLayoutHints(data) {
    // Mark elements that might benefit from wider layouts
    if (
      data.type === 'audio' ||
      data.type === 'slider' && data.name.toLowerCase().includes('spectrum')
    ) {
      data._layoutHint = 'wide';
    }

    // Mark elements that should always be full width
    if (data.type === 'help' || data.name.toLowerCase().includes('debug')) {
      data._layoutHint = 'full-width';
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
   * Balance heights across multiple columns by reordering elements
   */
  balanceColumnHeights() {
    const uiControlsContainer = document.getElementById(this.uiControlsId);
    if (!uiControlsContainer) return;

    // This could be enhanced with more sophisticated column balancing
    // For now, CSS Grid handles most of the heavy lifting
    const groups = uiControlsContainer.querySelectorAll('.ui-group');

    if (this.debugMode && groups.length > 0) {
      console.log(`🎵 UI Column balancing: ${groups.length} groups distributed`);
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

    // Add layout classes based on element hints
    if (data._layoutHint === 'wide') {
      control.classList.add('wide-control');
    } else if (data._layoutHint === 'full-width') {
      control.classList.add('full-width-control');
    }

    if (this.debugMode) {
      console.log(
        `🎵 UI Registered element: ID '${data.id}' (${data.type}${
          data._layoutHint ? ', ' + data._layoutHint : ''
        }) - Total: ${Object.keys(this.uiElements).length}`,
      );
    }
  }

  // Enable or disable debug logging
  setDebugMode(enabled) {
    this.debugMode = enabled;
    console.log(`🎵 UI Manager debug mode ${enabled ? 'enabled' : 'disabled'}`);

    // Store globally for layout manager access
    window.uiManager = this;
  }

  // Handle layout changes (enhanced for new system)
  onLayoutChange(layoutMode) {
    if (this.debugMode) {
      console.log(`🎵 UI Manager: Layout changed to ${layoutMode}`);
    }

    // Force layout update in case UI elements were added before layout was ready
    if (this.layoutManager) {
      this.layoutManager.forceLayoutUpdate();
    }

    // Re-optimize layout for new mode
    setTimeout(() => {
      this.optimizeCurrentLayout();
    }, 100);
  }

  /**
   * Handle advanced layout changes from the enhanced layout system
   */
  onAdvancedLayoutChange(layoutDetail) {
    const { layout, data } = layoutDetail;

    if (this.debugMode) {
      console.log(`🎵 UI Manager: Advanced layout change to ${layout}:`, data);
    }

    // Adjust UI elements based on new layout data
    this.adaptToLayoutData(data);
  }

  /**
   * Adapt UI elements to new layout constraints
   */
  adaptToLayoutData(layoutData) {
    const { uiColumns, uiColumnWidth, canExpand } = layoutData;

    // Update group layouts based on available columns
    this.groups.forEach((groupInfo, groupName) => {
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

  // Cleanup method to remove event listeners
  destroy() {
    if (this.layoutManager) {
      this.layoutManager.destroy();
      this.layoutManager = null;
    }

    globalThis.removeEventListener('layoutChanged', this.onAdvancedLayoutChange);
  }
}

// Global debug controls for UI Manager
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
}
