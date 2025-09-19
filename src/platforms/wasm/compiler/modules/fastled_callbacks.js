/// <reference path="../types.d.ts" />

/**
 * FastLED Pure JavaScript Callbacks Interface
 *
 * This module defines the user callback interface for FastLED events.
 * All callbacks are pure JavaScript functions that handle async operations
 * without embedded JavaScript in C++ code.
 *
 * Key Features:
 * - Pure JavaScript async callback patterns
 * - Clean separation from C++ code
 * - Event-driven user interface
 * - Proper error handling and debugging
 * - No embedded JavaScript dependencies
 *
 * @module FastLEDCallbacks
 */

// Import debug logging
import { FASTLED_DEBUG_LOG, FASTLED_DEBUG_ERROR, FASTLED_DEBUG_TRACE } from './fastled_debug_logger.js';

/**
 * @typedef {Object} FrameData
 * @property {number} strip_id - ID of the LED strip
 * @property {string} type - Type of frame data
 * @property {Uint8Array|number[]} pixel_data - Pixel color data
 * @property {ScreenMapData} screenMap - Screen mapping data for LED positions
 */

/**
 * @typedef {Object} ScreenMapData
 * @property {number[]} absMax - Maximum coordinates array
 * @property {number[]} absMin - Minimum coordinates array
 * @property {{ [key: string]: any }} strips - Strip configuration data
 */

/**
 * Frame rendering callback - pure JavaScript
 * Called for each animation frame with pixel data
 * @async
 * @param {(Array & {screenMap?: ScreenMapData}) | FrameData} frameData - Frame data (array with optional screenMap or FrameData object)
 * @returns {Promise<void>} Promise that resolves when frame is rendered
 */
globalThis.FastLED_onFrame = async function (frameData) {
  // Only log every 60 frames to avoid spam
  const shouldLog = !globalThis.fastLEDFrameCount || globalThis.fastLEDFrameCount % 60 === 0;
  globalThis.fastLEDFrameCount = (globalThis.fastLEDFrameCount || 0) + 1;

  if (shouldLog) {
    FASTLED_DEBUG_TRACE('CALLBACKS', 'FastLED_onFrame', 'ENTER', {
      frameDataLength: frameData && Array.isArray(frameData) ? frameData.length : (frameData ? 'FrameData object' : 0),
      frameCount: globalThis.fastLEDFrameCount,
    });
  }

  try {
    // Defensive programming: ensure frameData exists
    if (!frameData) {
      FASTLED_DEBUG_ERROR('CALLBACKS', 'FastLED_onFrame received null/undefined frameData', null);
      return;
    }

    // Defensive programming: ensure frameData is array-like
    if (!Array.isArray(frameData)) {
      console.warn('FastLED_onFrame: frameData is not an array, treating as empty');
      frameData = [];
    }

    if (frameData.length === 0) {
      if (shouldLog) {
        FASTLED_DEBUG_LOG('CALLBACKS', 'Received empty frame data');
      }
      console.warn('Received empty frame data');
      return;
    }

    if (shouldLog) {
      FASTLED_DEBUG_LOG('CALLBACKS', 'Processing frame data', {
        stripCount: frameData.length,
        hasScreenMap: Boolean(frameData.screenMap),
      });
    }

    // Add screen map data if not present
    if (!frameData.screenMap && window.screenMap) {
      frameData.screenMap = window.screenMap;
      if (shouldLog) {
        FASTLED_DEBUG_LOG('CALLBACKS', 'Added screenMap to frameData from window.screenMap');
      }
    }

    // Final defensive check: ensure screenMap has proper structure
    if (frameData.screenMap && (!frameData.screenMap.strips || typeof frameData.screenMap.strips !== 'object')) {
      console.warn('FastLED_onFrame: screenMap exists but has invalid structure, using fallback:', frameData.screenMap);
      frameData.screenMap = {
        strips: {},
        absMin: [0, 0],
        absMax: [0, 0],
      };
    }

    // Render to canvas using existing graphics manager
    if (window.updateCanvas) {
      if (shouldLog) {
        FASTLED_DEBUG_LOG('CALLBACKS', 'Calling window.updateCanvas...');
      }
      window.updateCanvas(frameData);
      if (shouldLog) {
        FASTLED_DEBUG_LOG('CALLBACKS', 'window.updateCanvas completed');
      }
    } else {
      if (shouldLog) {
        FASTLED_DEBUG_ERROR('CALLBACKS', 'No updateCanvas function available for rendering', null);
      }
      console.warn('No updateCanvas function available for rendering');
    }

    // Log frame processing for debugging
    if (window.fastLEDDebug && shouldLog) {
      console.debug(`FastLED frame processed: ${frameData.length} strips`);
      FASTLED_DEBUG_LOG('CALLBACKS', `FastLED frame processed: ${frameData.length} strips`);
    }
  } catch (error) {
    console.error('Error in FastLED_onFrame:', error);
    FASTLED_DEBUG_ERROR('CALLBACKS', 'Error in FastLED_onFrame', error);
    throw error; // Re-throw to let controller handle
  }
};

/**
 * UI processing callback - pure JavaScript
 * Called to collect UI changes and return them as JSON
 * @async
 * @returns {Promise<string>} Promise that resolves to JSON string of UI changes
 */
globalThis.FastLED_processUiUpdates = async function () {
  try {
    if (window.uiManager && typeof window.uiManager.processUiChanges === 'function') {
      const changes = window.uiManager.processUiChanges();
      return changes ? JSON.stringify(changes) : '{}';
    }

    // Return empty JSON object if no UI manager
    return '{}';
  } catch (error) {
    console.error('Error in FastLED_processUiUpdates:', error);
    return '{}'; // Return empty JSON object on error
  }
};

/**
 * Strip update callback - pure JavaScript
 * Called when strip configuration changes (e.g., screen mapping)
 * @async
 * @param {Object} stripData - Strip update data
 * @param {string} stripData.event - Event type (e.g., "set_canvas_map")
 * @param {number} stripData.strip_id - ID of the LED strip
 * @param {Object} stripData.map - Coordinate mapping data (if applicable)
 * @param {number} [stripData.diameter] - LED diameter in mm
 * @returns {Promise<void>} Promise that resolves when strip update is processed
 */
globalThis.FastLED_onStripUpdate = async function (stripData) {
  try {
    console.log('Strip update event:', stripData);

    if (stripData.event === 'set_canvas_map') {
      // Handle canvas mapping
      if (window.handleStripMapping) {
        await window.handleStripMapping(stripData);
      } else {
        console.warn('No handleStripMapping function available');
      }

      // Update global screen map
      if (window.screenMap && stripData.map) {
        const { map } = stripData;
        console.log('Updating screen map for strip', stripData.strip_id);

        // Calculate min/max coordinates
        const minMax = (x_array, y_array) => {
          let min_x = x_array[0]; let
            min_y = y_array[0];
          let max_x = x_array[0]; let
            max_y = y_array[0];
          for (let i = 1; i < x_array.length; i++) {
            min_x = Math.min(min_x, x_array[i]);
            min_y = Math.min(min_y, y_array[i]);
            max_x = Math.max(max_x, x_array[i]);
            max_y = Math.max(max_y, y_array[i]);
          }
          return [[min_x, min_y], [max_x, max_y]];
        };

        const [min, max] = minMax(map.x, map.y);
        const diameter = stripData.diameter || 0.2;

        window.screenMap.strips[stripData.strip_id] = {
          map,
          min,
          max,
          diameter,
        };

        console.log('Screen map updated:', window.screenMap);
      }
    }
  } catch (error) {
    console.error('Error in FastLED_onStripUpdate:', error);
    throw error; // Re-throw to let controller handle
  }
};

/**
 * Strip added callback - pure JavaScript
 * Called when a new LED strip is registered
 * @async
 * @param {number} stripId - Unique identifier for the LED strip
 * @param {number} numLeds - Number of LEDs in the strip
 * @returns {Promise<void>} Promise that resolves when strip addition is processed
 */
globalThis.FastLED_onStripAdded = async function (stripId, numLeds) {
  try {
    console.log(`Strip added: ID ${stripId}, LEDs ${numLeds}`);

    // Update display if output element exists
    const output = document.getElementById('output');
    if (output) {
      output.textContent += `Strip added: ID ${stripId}, length ${numLeds}\n`;
    }

    // Notify UI manager if available
    if (window.uiManager && typeof window.uiManager.onStripAdded === 'function') {
      window.uiManager.onStripAdded(stripId, numLeds);
    }

    // Trigger custom event for external listeners
    if (window.fastLEDEvents) {
      window.fastLEDEvents.emitStripAdded(stripId, numLeds);
    }
  } catch (error) {
    console.error('Error in FastLED_onStripAdded:', error);
    throw error; // Re-throw to let controller handle
  }
};

/**
 * UI elements added callback - pure JavaScript
 * Called when new UI elements are added by FastLED
 * @async
 * @param {Object} uiData - UI element configuration data
 * @returns {Promise<void>} Promise that resolves when UI elements are added
 */
globalThis.FastLED_onUiElementsAdded = async function (uiData) {
  try {
    console.log('UI elements added:', uiData);

    // Log the inbound event to the inspector if available
    if (window.jsonInspector) {
      window.jsonInspector.logInboundEvent(uiData);
    }

    // Add UI elements using UI manager
    if (window.uiManager && typeof window.uiManager.addUiElements === 'function') {
      window.uiManager.addUiElements(uiData);
    } else {
      console.warn('UI Manager not available or addUiElements method missing');
    }

    // Trigger custom event for external listeners
    if (window.fastLEDEvents) {
      window.fastLEDEvents.emitUiUpdate(uiData);
    }
  } catch (error) {
    console.error('Error in FastLED_onUiElementsAdded:', error);
    throw error; // Re-throw to let controller handle
  }
};

/**
 * Error handling callback - pure JavaScript
 * Called when FastLED encounters an error
 * @async
 * @param {string} errorType - Type of error
 * @param {string} errorMessage - Error message
 * @param {Object} [errorData] - Additional error data
 * @returns {Promise<void>} Promise that resolves when error is handled
 */
globalThis.FastLED_onError = async function (errorType, errorMessage, errorData = null) {
  try {
    console.error(`FastLED Error [${errorType}]: ${errorMessage}`, errorData);

    // Show user-friendly error message if error display element exists
    const errorDisplay = document.getElementById('error-display');
    if (errorDisplay) {
      errorDisplay.textContent = `FastLED Error: ${errorMessage}`;
      errorDisplay.style.display = 'block';
    }

    // Trigger custom event for external error handlers
    if (window.fastLEDEvents) {
      window.fastLEDEvents.dispatchEvent(new CustomEvent('error', {
        detail: { errorType, errorMessage, errorData },
      }));
    }
  } catch (error) {
    console.error('Error in FastLED_onError handler:', error);
    // Don't re-throw to avoid error loops
  }
};

/**
 * Debug logging helper
 * @param {string} message - Debug message
 * @param {...*} args - Additional arguments to log
 */
function fastLEDDebugLog(message, ...args) {
  if (window.fastLEDDebug) {
    console.debug(`[FastLED Debug] ${message}`, ...args);
  }
}

/**
 * Enable or disable debug logging
 * @param {boolean} enabled - Whether to enable debug logging
 */
function setFastLEDDebug(enabled) {
  window.fastLEDDebug = enabled;
  console.log(`FastLED debug logging ${enabled ? 'enabled' : 'disabled'}`);
}

// Export debug functions for external use
window.fastLEDDebugLog = fastLEDDebugLog;
window.setFastLEDDebug = setFastLEDDebug;

// Add debugging info for callback functions
console.log('FastLED Pure JavaScript callbacks registered:', {
  FastLED_onFrame: typeof globalThis.FastLED_onFrame,
  FastLED_processUiUpdates: typeof globalThis.FastLED_processUiUpdates,
  FastLED_onStripUpdate: typeof globalThis.FastLED_onStripUpdate,
  FastLED_onStripAdded: typeof globalThis.FastLED_onStripAdded,
  FastLED_onUiElementsAdded: typeof globalThis.FastLED_onUiElementsAdded,
  FastLED_onError: typeof globalThis.FastLED_onError,
});
