/// <reference path="types.d.ts" />

/**
 * FastLED WebAssembly Compiler Main Module
 *
 * This module serves as the main entry point for the FastLED WebAssembly compiler.
 * It handles module loading, UI management, graphics rendering, file processing,
 * and the main setup/loop execution for FastLED programs.
 *
 * Key responsibilities:
 * - Loading and initializing the FastLED WASM module
 * - Managing UI components and user interactions
 * - Handling graphics rendering (both fast 2D and beautiful 3D modes)
 * - Processing frame data and LED strip updates
 * - File system operations and asset loading
 * - Audio integration and processing
 *
 * @module FastLED/Compiler
 */

// Import type definitions for TypeScript checking
/// <reference path="./types.d.ts" />


// UI System imports
import { JsonUiManager } from './modules/ui/ui_manager.js';

// Graphics System imports
import { GraphicsManager } from './modules/graphics/graphics_manager.js';
import { GraphicsManagerThreeJS } from './modules/graphics/graphics_manager_threejs.js';

// Utility imports
import { JsonInspector } from './modules/utils/json_inspector.js';

// Recording System imports
import { VideoRecorder } from './modules/recording/video_recorder.js';
// Import UI recording modules to make them available globally
import './modules/recording/ui_recorder.js';
import './modules/recording/ui_playback.js';
import './modules/recording/ui_recorder_test.js';

// Core FastLED Integration imports
import { FastLEDAsyncController } from './modules/core/fastled_async_controller.js';
import './modules/core/fastled_callbacks.js';
import { fastLEDEvents, fastLEDPerformanceMonitor } from './modules/core/fastled_events.js';
import { FASTLED_DEBUG_LOG, FASTLED_DEBUG_ERROR, FASTLED_DEBUG_TRACE } from './modules/core/fastled_debug_logger.js';

/**
 * @typedef {Object} FrameData
 * @property {number} strip_id - ID of the LED strip
 * @property {string} type - Type of frame data
 * @property {Uint8Array|number[]} pixel_data - Pixel color data
 * @property {ScreenMapData} screenMap - Screen mapping data for LED positions
 */

/**
 * @typedef {Object} ScreenMapData
 * @property {number[]} [absMax] - Maximum coordinates array (computed on-demand)
 * @property {number[]} [absMin] - Minimum coordinates array (computed on-demand)
 * @property {{ [key: string]: any }} strips - Strip configuration data
 */

/** URL parameters for runtime configuration */
const urlParams = new URLSearchParams(window.location.search);
console.log(`⭐ index.js loading, URL: ${window.location.href}`);

/**
 * Browser Compatibility Check
 * FastLED WASM requires OffscreenCanvas and WebGL2 support for Web Worker mode
 */
(function checkBrowserCompatibility() {
  const errors = [];

  // Check OffscreenCanvas support
  if (typeof OffscreenCanvas === 'undefined') {
    errors.push('OffscreenCanvas not supported');
  } else {
    // Check WebGL2 support with OffscreenCanvas
    try {
      const testCanvas = new OffscreenCanvas(1, 1);
      const ctx = testCanvas.getContext('webgl2');
      if (!ctx) {
        errors.push('WebGL2 not supported with OffscreenCanvas');
      }
    } catch (error) {
      errors.push(`OffscreenCanvas WebGL2 test failed: ${error.message}`);
    }
  }

  if (errors.length > 0) {
    const errorMessage = `
      <div style="
        position: fixed;
        top: 50%;
        left: 50%;
        transform: translate(-50%, -50%);
        background: #ff6b6b;
        color: white;
        padding: 30px;
        border-radius: 10px;
        font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
        max-width: 600px;
        box-shadow: 0 4px 20px rgba(0,0,0,0.3);
        z-index: 10000;
      ">
        <h2 style="margin-top: 0;">Browser Not Supported</h2>
        <p>This browser doesn't support the required features for FastLED WASM:</p>
        <ul style="text-align: left;">
          ${errors.map(err => `<li>${err}</li>`).join('')}
        </ul>
        <p><strong>Please use one of these browsers:</strong></p>
        <ul style="text-align: left;">
          <li>Chrome 69+ or Edge 79+</li>
          <li>Firefox 105+</li>
          <li>Safari 16.4+</li>
        </ul>
      </div>
    `;

    document.addEventListener('DOMContentLoaded', () => {
      document.body.insertAdjacentHTML('beforeend', errorMessage);
    });

    // Throw error to prevent further loading
    throw new Error(`Browser compatibility check failed: ${errors.join(', ')}`);
  }

  console.log('✅ Browser compatibility check passed');
})();

/** Force fast 2D renderer when gfx=0 URL parameter is present */
const FORCE_FAST_RENDERER = urlParams.get('gfx') === '0';

/** Force beautiful 3D renderer when gfx=1 URL parameter is present */
const FORCE_THREEJS_RENDERER = urlParams.get('gfx') === '1';

console.log('🔍 GFX Parameter Debug:');
console.log('  URL gfx param value:', urlParams.get('gfx'));
console.log('  FORCE_FAST_RENDERER:', FORCE_FAST_RENDERER);
console.log('  FORCE_THREEJS_RENDERER:', FORCE_THREEJS_RENDERER);

/** Maximum number of lines to keep in stdout output display */
const MAX_STDOUT_LINES = 50;

/** Default frame rate for FastLED animations (60 FPS) */
const DEFAULT_FRAME_RATE_60FPS = 60;

/** Current frame rate setting */
let frameRate = DEFAULT_FRAME_RATE_60FPS;

/** HTML element ID for the main rendering canvas */
let canvasId;

/** HTML element ID for the UI controls container */
let uiControlsId;

/** HTML element ID for the output/console display */
let outputId;

/** UI manager instance for handling user interface components */
let uiManager;

/** Flag indicating if UI canvas settings have changed */
let uiCanvasChanged = false;

/** Three.js modules container for 3D rendering */
let threeJsModules = {};

/** Graphics manager instance (either 2D or 3D) */
let graphicsManager;

/** Container ID for ThreeJS rendering context */
let containerId;

/** Graphics configuration arguments */
let graphicsArgs = {};

/** Mirror canvas for main-thread video recording (receives frames from worker) */
let mirrorCanvas = null;

/** Mirror canvas 2D context for drawing received frames */
let mirrorCanvasContext = null;

/** VideoRecorder instance for main-thread recording */
let mainThreadVideoRecorder = null;

/**
 * NOTE: AsyncFastLEDController has been moved to fastled_async_controller.js
 * This is now imported as a pure JavaScript module for better separation of concerns
 * and to eliminate embedded JavaScript in C++ code.
 *
 * The new FastLEDAsyncController provides:
 * - Pure JavaScript async patterns with proper Asyncify integration
 * - Clean data export/import with C++ via Module.cwrap
 * - Event-driven architecture replacing callback chains
 * - Proper error handling and performance monitoring
 * - No embedded JavaScript - all async logic in JavaScript domain
 */

// The AsyncFastLEDController is now imported from fastled_async_controller.js
// Old implementation has been replaced with pure JavaScript architecture

/**
 * Global reference to the current FastLEDAsyncController instance
 * @type {FastLEDAsyncController|null}
 */
let fastLEDController = null;

/**
 * Stub FastLED loader function (replaced during initialization)
 * @param {Object} _options - Loading options (ignored in stub)
 * @returns {Promise<void>} Promise that resolves when initialization completes
 */
let _loadFastLED = function (_options) {
  // Stub to let the user/dev know that something went wrong.
  // This function is replaced with an async implementation, so it must be async for interface compatibility
  console.log('FastLED loader function was not set.');
  return Promise.resolve();
};

/**
 * Public FastLED loading function (delegates to private implementation)
 * @async
 * @param {Object} options - Loading options and configuration
 * @returns {Promise<*>} Result from the FastLED loader
 */
export async function loadFastLED(options) {
  // This will be overridden through the initialization.
  return await _loadFastLED(options);
}

/** Application start time epoch for timing calculations */
const EPOCH = new Date().getTime();

/**
 * Gets elapsed time since application start
 * @returns {string} Time in seconds with one decimal place
 */
function getTimeSinceEpoc() {
  const outMS = new Date().getTime() - EPOCH;
  const outSec = outMS / 1000;
  // one decimal place
  return outSec.toFixed(1);
}

/**
 * Print function (will be overridden during initialization)
 * @function
 * @param {...*} _args - Arguments to print
 */
let print = function (..._args) {};

/** Store reference to original console for fallback */
const prev_console = console;

// Store original console methods
const _prev_log = prev_console.log;
const _prev_warn = prev_console.warn;
const _prev_error = prev_console.error;

/**
 * Adds timestamp to console arguments
 * @param {...*} args - Console arguments to timestamp
 * @returns {Array} Arguments array with timestamp prepended
 */
function toStringWithTimeStamp(...args) {
  const time = `${getTimeSinceEpoc()}s`;
  return [time, ...args]; // Return array with time prepended, don't join
}

/**
 * Custom console.log implementation with timestamps
 * @param {...*} args - Arguments to log
 */
function log(...args) {
  const argsWithTime = toStringWithTimeStamp(...args);
  _prev_log(...argsWithTime); // Spread the array when calling original logger
  try {
    print(...argsWithTime);
  } catch (e) {
    _prev_log('Error in log', e);
  }
}

/**
 * Custom console.warn implementation with timestamps
 * @param {...*} args - Arguments to warn about
 */
function warn(...args) {
  const argsWithTime = toStringWithTimeStamp(...args);
  _prev_warn(...argsWithTime);
  try {
    print(...argsWithTime);
  } catch (e) {
    _prev_warn('Error in warn', e);
  }
}

/**
 * Custom print function for displaying output in the UI
 * @param {...*} args - Arguments to print to UI output
 */
function customPrintFunction(...args) {
  if (containerId === undefined) {
    return; // Not ready yet.
  }
  // take the args and stringify them, then add them to the output element
  const cleanedArgs = args.map((arg) => {
    if (typeof arg === 'object') {
      try {
        return JSON.stringify(arg).slice(0, 100);
      } catch (e) {
        return `${arg}`;
      }
    }
    return arg;
  });

  const output = document.getElementById(outputId);
  const allText = `${output.textContent + [...cleanedArgs].join(' ')}\n`;
  // split into lines, and if there are more than 100 lines, remove one.
  const lines = allText.split('\n');
  while (lines.length > MAX_STDOUT_LINES) {
    lines.shift();
  }
  output.textContent = lines.join('\n');
}

// DO NOT OVERRIDE ERROR! When something goes really wrong we want it
// to always go to the console. If we hijack it then startup errors become
// extremely difficult to debug.

// Override console for custom logging behavior
// Note: Modifying existing console properties instead of reassigning the global
console.log = log;
console.warn = warn;
console.error = _prev_error;

/**
 * Appends raw file data to WASM module file system
 * @param {Object} moduleInstance - The WASM module instance
 * @param {number} path_cstr - C string pointer to file path
 * @param {number} data_cbytes - C bytes pointer to file data
 * @param {number} len_int - Length of data in bytes
 */
function jsAppendFileRaw(moduleInstance, path_cstr, data_cbytes, len_int) {
  // Stream this chunk
  moduleInstance.ccall(
    'jsAppendFile',
    'number', // return value
    ['number', 'number', 'number'], // argument types, not sure why numbers works.
    [path_cstr, data_cbytes, len_int],
  );
}

/**
 * Appends Uint8Array file data to WASM module file system
 * @param {Object} moduleInstance - The WASM module instance
 * @param {string} path - File path in the virtual file system
 * @param {Uint8Array} blob - File data as byte array
 */
function jsAppendFileUint8(moduleInstance, path, blob) {
  const n = moduleInstance.lengthBytesUTF8(path) + 1;
  const path_cstr = moduleInstance._malloc(n);
  moduleInstance.stringToUTF8(path, path_cstr, n);
  const ptr = moduleInstance._malloc(blob.length);
  moduleInstance.HEAPU8.set(blob, ptr);
  jsAppendFileRaw(moduleInstance, path_cstr, ptr, blob.length);
  moduleInstance._free(ptr);
  moduleInstance._free(path_cstr);
}

/**
 * Partitions files into immediate and streaming categories based on extensions
 * @param {Array<Object>} filesJson - Array of file objects with path and data
 * @param {string[]} immediateExtensions - Extensions that should be loaded immediately
 * @returns {Array<Array<Object>>} [immediateFiles, streamingFiles]
 */
function partition(filesJson, immediateExtensions) {
  const immediateFiles = [];
  const streamingFiles = [];
  filesJson.map((file) => {
    for (const ext of immediateExtensions) {
      const pathLower = file.path.toLowerCase();
      if (pathLower.endsWith(ext.toLowerCase())) {
        immediateFiles.push(file);
        return;
      }
    }
    streamingFiles.push(file);
  });
  return [immediateFiles, streamingFiles];
}

/**
 * Creates a file manifest JSON for the WASM module
 * @param {Array<Object>} filesJson - Array of file objects
 * @param {number} frame_rate - Target frame rate for animations
 * @returns {Object} Manifest object with files and frameRate
 */
function getFileManifestJson(filesJson, frame_rate) {
  const trimmedFilesJson = filesJson.map((file) => ({
    path: file.path,
    size: file.size,
  }));
  const options = {
    files: trimmedFilesJson,
    frameRate: frame_rate,
  };
  return options;
}

/**
 * Updates the canvas with new frame data from FastLED
 * @param {FrameData | (Array & {screenMap?: ScreenMapData})} frameData - Frame data with pixel information and screen mapping
 */
function updateCanvas(frameData) {
  // we are going to add the screenMap to the graphicsManager
  if (frameData.screenMap === undefined) {
    console.warn('Screen map not found in frame data, skipping canvas update');
    return;
  }
  if (!graphicsManager) {
    // Ensure graphicsArgs has required properties
    const currentGraphicsArgs = {
      canvasId: canvasId || 'canvas',
      threeJsModules: graphicsArgs.threeJsModules || null,
      ...graphicsArgs
    };

    // Try to create graphics manager - default to ThreeJS (gfx=1) if not specified
    try {
      if (FORCE_FAST_RENDERER) {
        console.log('Creating Fast GraphicsManager with canvas ID (gfx=0)', canvasId);
        graphicsManager = new GraphicsManager(currentGraphicsArgs);
      } else {
        // Default to ThreeJS renderer (gfx=1) if no parameter specified
        const explicitlyRequested = FORCE_THREEJS_RENDERER ? 'gfx=1' : 'default (gfx=1)';
        console.log(`Creating Beautiful GraphicsManager with canvas ID (${explicitlyRequested})`, canvasId);
        graphicsManager = new GraphicsManagerThreeJS(currentGraphicsArgs);
      }
    } catch (error) {
      console.error('Failed to create graphics manager:', error);

      const errorDisplay = document.getElementById('error-display');
      if (errorDisplay) {
        errorDisplay.textContent = `Graphics initialization failed: ${error.message}`;
        errorDisplay.style.color = '#ff6b6b'; // Red error color
      }
      return; // Exit early on failure
    }

    // Expose graphics manager globally for video recorder initialization
    window.graphicsManager = graphicsManager;

    uiCanvasChanged = false;
  }

  if (uiCanvasChanged) {
    uiCanvasChanged = false;
    try {
      graphicsManager.reset();
    } catch (resetError) {
      console.error('Graphics manager reset failed:', resetError);
      // Try to recreate the graphics manager
      graphicsManager = null;
      updateCanvas(frameData); // Recursive call to recreate
      return;
    }
  }

  try {
    graphicsManager.updateCanvas(frameData);
  } catch (updateError) {
    console.error('Graphics manager update failed:', updateError);

    // Show error to user
    const errorDisplay = document.getElementById('error-display');
    if (errorDisplay) {
      errorDisplay.textContent = 'Rendering error occurred';
      errorDisplay.style.color = '#ff6b6b';
    }
  }
}

/**
 * Main setup and loop execution function for FastLED programs (Pure JavaScript Architecture)
 * @async
 * @param {Object} moduleInstance - The WASM module instance
 * @param {number} frame_rate - Target frame rate for the animation loop
 * @returns {Promise<void>} Promise that resolves when setup is complete and loop is started
 */
async function FastLED_SetupAndLoop(moduleInstance, frame_rate) {
  FASTLED_DEBUG_TRACE('INDEX_JS', 'FastLED_SetupAndLoop', 'ENTER', { frame_rate });

  try {
    FASTLED_DEBUG_LOG('INDEX_JS', 'Initializing FastLED with Pure JavaScript Architecture...');
    console.log('Initializing FastLED with Pure JavaScript Architecture...');

    // Check if moduleInstance is valid
    FASTLED_DEBUG_LOG('INDEX_JS', 'Checking moduleInstance', {
      hasModule: !!moduleInstance,
      hasExternSetup: !!(moduleInstance && moduleInstance._extern_setup),
      hasExternLoop: !!(moduleInstance && moduleInstance._extern_loop),
      hasCwrap: !!(moduleInstance && moduleInstance.cwrap),
    });

    if (!moduleInstance) {
      throw new Error('moduleInstance is null or undefined');
    }

    // Create the pure JavaScript async controller
    FASTLED_DEBUG_LOG('INDEX_JS', 'Creating FastLEDAsyncController...');
    fastLEDController = new FastLEDAsyncController(moduleInstance, frame_rate);
    FASTLED_DEBUG_LOG('INDEX_JS', 'FastLEDAsyncController created successfully');

    // Expose controller globally for debugging and external control
    window.fastLEDController = fastLEDController;

    // Expose event system globally
    window.fastLEDEvents = fastLEDEvents;
    window.fastLEDPerformanceMonitor = fastLEDPerformanceMonitor;

    FASTLED_DEBUG_LOG('INDEX_JS', 'Globals exposed, calling controller.setup()...');

    // Setup FastLED with proper error handling
    console.log('🔧 About to call fastLEDController.setup()');
    try {
      fastLEDController.setup();
      console.log('🔧 fastLEDController.setup() completed successfully');
    } catch (error) {
      console.error('🔧 setup() threw error:', error);
      console.error('🔧 Error stack:', error.stack);
      throw error; // Re-throw to prevent silent failure
    }

    FASTLED_DEBUG_LOG('INDEX_JS', 'Controller setup completed, initializing worker mode...');

    // Initialize Web Worker mode for background thread rendering (always enabled)
    const canvas = /** @type {HTMLCanvasElement | null} */ (document.getElementById('myCanvas'));
    if (!canvas) {
      throw new Error('Canvas element not found');
    }

    FASTLED_DEBUG_LOG('INDEX_JS', 'Initializing Web Worker mode with OffscreenCanvas...');
    await fastLEDController.initializeWorkerMode(canvas, {
      maxRetries: 3
    });
    FASTLED_DEBUG_LOG('INDEX_JS', 'Web Worker mode initialized successfully');

    // Start the async animation loop in worker mode
    await fastLEDController.startWithWorkerSupport();
    FASTLED_DEBUG_LOG('INDEX_JS', 'Animation loop started with Web Worker support');
    console.log('FastLED running in Web Worker mode (background thread)');

    FASTLED_DEBUG_LOG('INDEX_JS', 'Animation loop started, setting up UI controls...');

    // Add UI controls for start/stop if elements exist
    const startBtn = document.getElementById('start-btn');
    const stopBtn = document.getElementById('stop-btn');
    const toggleBtn = document.getElementById('toggle-btn');
    const fpsDisplay = document.getElementById('fps-display');

    FASTLED_DEBUG_LOG('INDEX_JS', 'UI controls found', {
      startBtn: !!startBtn,
      stopBtn: !!stopBtn,
      toggleBtn: !!toggleBtn,
      fpsDisplay: !!fpsDisplay,
    });

    if (startBtn) {
      startBtn.onclick = async () => {
        FASTLED_DEBUG_LOG('INDEX_JS', 'Start button clicked');
        if (fastLEDController.setupCompleted) {
          await fastLEDController.startWithWorkerSupport();
        } else {
          console.warn('FastLED setup not completed yet');
          FASTLED_DEBUG_LOG('INDEX_JS', 'Start button clicked but setup not completed');
        }
      };
    }

    if (stopBtn) {
      stopBtn.onclick = async () => {
        FASTLED_DEBUG_LOG('INDEX_JS', 'Stop button clicked');
        await fastLEDController.stopWithWorkerSupport();
      };
    }

    if (toggleBtn) {
      toggleBtn.onclick = async () => {
        FASTLED_DEBUG_LOG('INDEX_JS', 'Toggle button clicked');
        const isRunning = await toggleFastLED();
        toggleBtn.textContent = isRunning ? 'Pause' : 'Resume';
      };
    }

    // Performance monitoring display with event system integration
    if (fpsDisplay) {
      FASTLED_DEBUG_LOG('INDEX_JS', 'Setting up performance monitoring...');
      setInterval(() => {
        const fps = fastLEDController.getFPS();
        const frameTime = fastLEDController.getAverageFrameTime();

        // Record performance metrics
        fastLEDPerformanceMonitor.recordFrameTime(frameTime);

        // Update display
        fpsDisplay.textContent = `FPS: ${fps.toFixed(1)} | Frame: ${frameTime.toFixed(1)}ms`;

        // Monitor memory usage if available
        if (performance.memory) {
          fastLEDPerformanceMonitor.recordMemoryUsage(performance.memory.usedJSHeapSize);
        }
      }, 1000);
    }

    // Set up event monitoring for debugging
    if (window.fastLEDDebug) {
      fastLEDEvents.setDebugMode(true);
      FASTLED_DEBUG_LOG('INDEX_JS', 'Event debug mode enabled');
    }

    FASTLED_DEBUG_LOG('INDEX_JS', 'Checking callback function availability...');
    const callbackStatus = {
      FastLED_onFrame: typeof globalThis.FastLED_onFrame,
      FastLED_processUiUpdates: typeof globalThis.FastLED_processUiUpdates,
      FastLED_onStripUpdate: typeof globalThis.FastLED_onStripUpdate,
      FastLED_onStripAdded: typeof globalThis.FastLED_onStripAdded,
      FastLED_onUiElementsAdded: typeof globalThis.FastLED_onUiElementsAdded,
    };

    FASTLED_DEBUG_LOG('INDEX_JS', 'Callback function status', callbackStatus);

    console.log('FastLED Pure JavaScript Architecture initialized successfully');
    console.log('Available features:', {
      asyncController: !!fastLEDController,
      eventSystem: !!fastLEDEvents,
      performanceMonitor: !!fastLEDPerformanceMonitor,
      callbacks: callbackStatus,
    });

    FASTLED_DEBUG_LOG('INDEX_JS', 'FastLED_SetupAndLoop completed successfully');
    FASTLED_DEBUG_TRACE('INDEX_JS', 'FastLED_SetupAndLoop', 'EXIT');
  } catch (error) {
    FASTLED_DEBUG_ERROR('INDEX_JS', 'Failed to initialize FastLED with Pure JavaScript Architecture', error);
    console.error('Failed to initialize FastLED with Pure JavaScript Architecture:', error);

    // Emit error event
    if (fastLEDEvents) {
      fastLEDEvents.emitError('initialization', error.message, { stack: error.stack });
    }

    // Show user-friendly error message if error display element exists
    const errorDisplay = document.getElementById('error-display');
    if (errorDisplay) {
      errorDisplay.textContent = 'Failed to load FastLED with Pure JavaScript Architecture. Please refresh the page.';
    }

    throw error;
  }
}

/**
 * NOTE: All callback functions have been moved to fastled_callbacks.js
 * This provides better separation of concerns and eliminates embedded JavaScript.
 *
 * The pure JavaScript callbacks include:
 * - FastLED_onStripUpdate() - handles strip configuration changes
 * - FastLED_onStripAdded() - handles new strip registration
 * - FastLED_onFrame() - handles frame rendering
 * - FastLED_processUiUpdates() - handles UI state collection
 * - FastLED_onUiElementsAdded() - handles UI element addition
 * - FastLED_onError() - handles error reporting
 *
 * All callbacks are automatically available via the imported module.
 */

// Callback functions are now pure JavaScript modules - no embedded definitions needed

/**
 * Main function to initialize and start the FastLED setup/loop cycle (Asyncify-enabled)
 * @async
 * @param {number} frame_rate - Target frame rate for animations
 * @param {Object} moduleInstance - The loaded WASM module instance
 * @param {Array<Object>} filesJson - Array of files to load into the virtual filesystem
 */
async function fastledLoadSetupLoop(
  frame_rate,
  moduleInstance,
  filesJson,
) {
  console.log('Calling setup function...');

  const fileManifest = getFileManifestJson(filesJson, frame_rate);
  moduleInstance.cwrap('fastled_declare_files', null, ['string'])(JSON.stringify(fileManifest));
  console.log('Files JSON:', filesJson);

  /**
   * Processes a single file by streaming it to the WASM module
   * @async
   * @param {Object} file - File object with path and data
   * @param {string} file.path - File path in the virtual filesystem
   * @param {number} file.size - File size in bytes
   */
  const processFile = async (file) => {
    try {
      const response = await fetch(file.path);
      const reader = response.body.getReader();

      console.log(`File fetched: ${file.path}, size: ${file.size}`);

      while (true) {
        // deno-lint-ignore no-await-in-loop
        const { value, done } = await reader.read();
        if (done) break;
        // Allocate and copy chunk data
        jsAppendFileUint8(moduleInstance, file.path, value);
      }
    } catch (error) {
      console.error(`Error processing file ${file.path}:`, error);
    }
  };

  /**
   * Fetches all files in parallel and calls completion callback
   * @async
   * @param {Array<Object>} filesJson - Array of file objects to fetch
   * @param {Function} [onComplete] - Optional callback when all files are loaded
   */
  const fetchAllFiles = async (filesJson, onComplete) => {
    const promises = filesJson.map(async (file) => {
      await processFile(file);
    });
    await Promise.all(promises);
    if (onComplete) {
      onComplete();
    }
  };

  // NOTE: Callback functions are now automatically registered by importing fastled_callbacks.js
  // No need to manually bind them here - they're pure JavaScript functions

  // Verify that the pure JavaScript callbacks are properly loaded
  console.log('FastLED Pure JavaScript callbacks verified:', {
    FastLED_onUiElementsAdded: typeof globalThis.FastLED_onUiElementsAdded,
    FastLED_onFrame: typeof globalThis.FastLED_onFrame,
    FastLED_onStripAdded: typeof globalThis.FastLED_onStripAdded,
    FastLED_onStripUpdate: typeof globalThis.FastLED_onStripUpdate,
    FastLED_processUiUpdates: typeof globalThis.FastLED_processUiUpdates,
    FastLED_onError: typeof globalThis.FastLED_onError,
  });

  // Initialize event system integration
  if (fastLEDEvents) {
    console.log('FastLED Event System ready with stats:', fastLEDEvents.getEventStats());
  }

  // Come back to this later - we want to partition the files into immediate and streaming files
  // so that large projects don't try to download ALL the large files BEFORE setup/loop is called.
  const [immediateFiles, streamingFiles] = partition(filesJson, ['.json', '.csv', '.txt', '.cfg']);
  console.log(
    'The following files will be immediatly available and can be read during setup():',
    immediateFiles,
  );
  console.log('The following files will be streamed in during loop():', streamingFiles);

  const promiseImmediateFiles = fetchAllFiles(immediateFiles, () => {
    if (immediateFiles.length !== 0) {
      console.log('All immediate files downloaded to FastLED.');
    }
  });
  await promiseImmediateFiles;
  if (streamingFiles.length > 0) {
    const streamingFilesPromise = fetchAllFiles(streamingFiles, () => {
      console.log('All streaming files downloaded to FastLED.');
    });
    const delay = new Promise((r) => { setTimeout(r, 250); });
    // Wait for either the time delay or the streaming files to be processed, whichever
    // happens first.
    await Promise.any([delay, streamingFilesPromise]);
  }

  console.log('Starting fastled with Asyncify support');
  await FastLED_SetupAndLoop(moduleInstance, frame_rate);
}

/**
 * Callback function executed when the WASM module is loaded
 * Sets up the module loading infrastructure
 * @param {Function} fastLedLoader - The FastLED loader function
 */
function onModuleLoaded(fastLedLoader) {
  // Unpack the module functions and send them to the fastledLoadSetupLoop function

  /**
   * Internal function to start FastLED with loaded module (Asyncify-enabled)
   * @async
   * @param {Object} moduleInstance - The loaded WASM module instance
   * @param {number} frameRate - Target frame rate for animations
   * @param {Array<Object>} filesJson - Files to load into virtual filesystem
   */
  async function __fastledLoadSetupLoop(moduleInstance, frameRate, filesJson) {
    const exports_exist = moduleInstance && moduleInstance._extern_setup
      && moduleInstance._extern_loop;
    if (!exports_exist) {
      console.error('FastLED setup or loop functions are not available.');
      return;
    }

    await fastledLoadSetupLoop(
      frameRate,
      moduleInstance,
      filesJson,
    );
  }
  // Start fetch now in parallel

  /**
   * Fetches and parses JSON from a given file path
   * @async
   * @param {string} fetchFilePath - Path to the JSON file to fetch
   * @returns {Promise<Object>} Parsed JSON data
   */
  const fetchJson = async (fetchFilePath) => {
    const response = await fetch(fetchFilePath);
    const data = await response.json();
    return data;
  };
  const filesJsonPromise = fetchJson('files.json');
  try {
    if (typeof fastLedLoader === 'function') {
      // Load the module
      fastLedLoader().then(async (instance) => {
        console.log('Module loaded, running FastLED...');

        // Expose the updateUiComponents method to the C++ module
        // This should be called BY C++ TO UPDATE the frontend, not the other way around
        instance._jsUiManager_updateUiComponents = function (jsonString) {
          console.log('*** C++ CALLING JS: updateUiComponents with:', jsonString);
          if (window.uiManagerInstance && window.uiManagerInstance.updateUiComponents) {
            window.uiManagerInstance.updateUiComponents(jsonString);
          } else {
            console.error('*** UI BINDING ERROR: uiManagerInstance not available ***');
          }
        };

        // Wait for the files.json to load.
        let filesJson = null;
        try {
          filesJson = await filesJsonPromise;
          console.log('Files JSON:', filesJson);
        } catch (error) {
          console.error('Error fetching files.json:', error);
          filesJson = {};
        }
        await __fastledLoadSetupLoop(instance, frameRate, filesJson);
      }).catch((err) => {
        console.error('Error loading fastled as a module:', err);
      });
    } else {
      console.log(
        'Could not detect a valid module loading for FastLED, expected function but got',
        typeof fastLedLoader,
      );
    }
  } catch (error) {
    console.error('Failed to load FastLED:', error);
  }
}

/**
 * Main FastLED loading and initialization function
 * Sets up the entire FastLED environment including UI, graphics, and WASM module
 * @async
 * @param {Object} options - Configuration options for FastLED initialization
 * @param {string} options.canvasId - ID of the HTML canvas element for rendering
 * @param {string} options.uiControlsId - ID of the HTML element for UI controls
 * @param {string} options.printId - ID of the HTML element for console output
 * @param {number} [options.frameRate] - Target frame rate (defaults to 60 FPS)
 * @param {Object} options.threeJs - Three.js configuration object
 * @param {Object} options.threeJs.modules - Three.js module imports
 * @param {string} options.threeJs.containerId - Container ID for Three.js rendering
 * @param {Function} options.fastled - FastLED WASM module loader function
 * @returns {Promise<void>} Promise that resolves when FastLED is fully loaded
 */
async function localLoadFastLed(options) {
  try {
    console.log('Loading FastLED with options:', options);
    canvasId = options.canvasId;
    uiControlsId = options.uiControlsId;
    outputId = options.printId;
    print = customPrintFunction;
    console.log('Loading FastLED with options:', options);
    frameRate = options.frameRate || DEFAULT_FRAME_RATE_60FPS;
    uiManager = new JsonUiManager(uiControlsId);

    // Initialize JSON Inspector
    new JsonInspector();

    // Expose UI manager globally for debug functions (main thread only)
    window.uiManager = uiManager;
    window.uiManagerInstance = uiManager;

    // Apply pending debug mode setting if it was set before manager creation
    if (typeof window._pendingUiDebugMode !== 'undefined') {
      uiManager.setDebugMode(window._pendingUiDebugMode);
      delete window._pendingUiDebugMode;
    }

    // Set up periodic cleanup of orphaned UI elements
    setInterval(() => {
      if (uiManager && uiManager.cleanupOrphanedElements) {
        uiManager.cleanupOrphanedElements();
      }
    }, 5000); // Run cleanup every 5 seconds

    // Set up periodic UI polling for worker mode
    // In worker mode, there's no main thread loop, so we poll for UI changes
    setInterval(() => {
      if (window.fastLEDWorkerManager && window.fastLEDWorkerManager.isWorkerActive) {
        // Worker mode: poll UI changes and send to worker
        if (window.uiManager && typeof window.uiManager.processUiChanges === 'function') {
          const changes = window.uiManager.processUiChanges();
          if (changes && Object.keys(changes).length > 0) {
            //console.log('🎮 [UI_POLL] Detected UI changes in worker mode:', changes);
            const message = {
              type: 'ui_changes',
              payload: {
                changes: changes
              }
            };
            window.fastLEDWorkerManager.worker.postMessage(message);
            //console.log('🎮 [UI_POLL] UI changes sent to worker');
          }
        }
      }
    }, 1000 / 60); // Poll at 60Hz to match typical frame rate

    const { threeJs } = options;
    console.log('ThreeJS:', threeJs);
    const fastLedLoader = options.fastled;
    threeJsModules = threeJs.modules;
    containerId = threeJs.containerId;
    console.log('ThreeJS modules:', threeJsModules);
    console.log('Container ID:', containerId);
    graphicsArgs = {
      canvasId,
      threeJsModules,
    };
    await onModuleLoaded(fastLedLoader);
  } catch (error) {
    console.error('Error loading FastLED:', error);
    // Debug point removed for linting compliance
  }
}

/** Replace the stub loader with the actual implementation */
_loadFastLED = localLoadFastLed;

/**
 * Global debugging and control functions for AsyncFastLEDController
 * These functions are exposed to window for external access and debugging
 */

/**
 * Gets the current FastLED controller instance
 * @returns {FastLEDAsyncController|null} Current controller instance or null
 */
function getFastLEDController() {
  return fastLEDController;
}

/**
 * Gets performance statistics from the current controller
 * @returns {Object|null} Performance stats or null if no controller
 */
function getFastLEDPerformanceStats() {
  return fastLEDController ? fastLEDController.getPerformanceStats() : null;
}

/**
 * Starts the FastLED animation loop (for external control)
 * @returns {Promise<void>} Promise that resolves when start is complete
 */
async function startFastLED() {
  if (!fastLEDController) {
    console.error('FastLED controller not initialized');
    return;
  }
  await fastLEDController.startWithWorkerSupport();
}

/**
 * Stops the FastLED animation loop (for external control)
 * @returns {Promise<boolean>} Promise that resolves to true if stopped successfully, false otherwise
 */
async function stopFastLED() {
  if (!fastLEDController) {
    console.error('FastLED controller not initialized');
    return false;
  }
  await fastLEDController.stopWithWorkerSupport();
  return true;
}

/**
 * Toggles the FastLED animation loop
 * @returns {Promise<boolean>} Promise that resolves to running state (true if now running)
 */
async function toggleFastLED() {
  if (!fastLEDController) {
    console.error('FastLED controller not initialized');
    return false;
  }

  if (fastLEDController.running) {
    await fastLEDController.stopWithWorkerSupport();
    return false;
  } else {
    await fastLEDController.startWithWorkerSupport();
    return true;
  }
}

/**
 * Sets up global error handlers for unhandled promise rejections
 * This helps catch async errors that might otherwise be silent
 */
function setupGlobalErrorHandlers() {
  // Handle unhandled promise rejections
  window.addEventListener('unhandledrejection', (event) => {
    console.error('Unhandled promise rejection in FastLED:', event.reason);

    // Check if this is a FastLED-related error
    if (event.reason && (
      event.reason.message?.includes('FastLED')
      || event.reason.message?.includes('extern_setup')
      || event.reason.message?.includes('extern_loop')
      || event.reason.stack?.includes('AsyncFastLEDController')
    )) {
      console.error('FastLED async error detected - stopping animation loop');
      if (fastLEDController) {
        fastLEDController.stop();
      }

      // Show user-friendly error message
      const errorDisplay = document.getElementById('error-display');
      if (errorDisplay) {
        errorDisplay.textContent = 'FastLED encountered an error. Animation stopped.';
      }
    }
  });

  // Handle general errors
  window.addEventListener('error', (event) => {
    if (event.error && event.error.stack?.includes('AsyncFastLEDController')) {
      console.error('FastLED error detected:', event.error);
      if (fastLEDController) {
        fastLEDController.stop();
      }
    }
  });
}

// Expose debugging functions globally
window.getFastLEDController = getFastLEDController;
window.getFastLEDPerformanceStats = getFastLEDPerformanceStats;
window.startFastLED = startFastLED;
window.stopFastLED = stopFastLED;
window.toggleFastLED = toggleFastLED;

// Expose rendering function globally (main thread only)
window.updateCanvas = updateCanvas;

// Set up global error handlers
setupGlobalErrorHandlers();

/**
 * Video Recording Setup
 */
let videoRecorder = null;

/**
 * Initializes the video recorder with canvas and audio context
 */
function initializeVideoRecorder() {
  // Wait for DOM to be ready
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initializeVideoRecorder);
    return;
  }

  const recordButton = document.getElementById('record-btn');

  if (!recordButton) {
    console.warn('Record button not found, video recording disabled');
    return;
  }

  // Check if worker mode is active - if so, use worker-based recording
  if (window.fastLEDWorkerManager && window.fastLEDWorkerManager.isWorkerActive) {
    console.log('Worker mode active - using worker-based video recording');
    initializeWorkerVideoRecorder(recordButton);
    return;
  }

  // For non-worker mode, wait for canvas to be ready
  const canvas = document.getElementById('myCanvas');
  if (!canvas) {
    console.warn('Canvas not found, video recording disabled');
    return;
  }

  // Wait for canvas to be properly initialized with graphics manager
  let retryCount = 0;
  const maxRetries = 30; // Max 6 seconds of retrying
  let initialized = false; // Track if initialization succeeded

  const tryInitialize = () => {
    // Stop retrying if already initialized
    if (initialized) {
      return;
    }

    try {
      // Check if worker mode became active during retry period
      if (window.fastLEDWorkerManager && window.fastLEDWorkerManager.isWorkerActive) {
        console.log('Worker mode became active - switching to worker-based recording');
        initializeWorkerVideoRecorder(recordButton);
        initialized = true; // Mark as successfully initialized
        return;
      }

      // Check if graphics manager has been initialized (this is the real dependency)
      if (typeof window.graphicsManager === 'undefined' && typeof graphicsManager === 'undefined') {
        throw new Error('Graphics manager not initialized yet');
      }

      // Validate canvas element exists (without creating conflicting context)
      if (!canvas || !canvas.getContext) {
        throw new Error('Canvas not ready yet');
      }

      actuallyInitializeVideoRecorder(canvas, recordButton);
      initialized = true; // Mark as successfully initialized
    } catch (error) {
      retryCount++;
      if (retryCount < maxRetries) {
        setTimeout(tryInitialize, 200);
      } else {
        console.warn('Failed to initialize video recorder - canvas/graphics not ready after maximum retries');
        recordButton.style.display = 'none';
      }
    }
  };

  // Start trying to initialize after a short delay
  setTimeout(tryInitialize, 1000);
}

/**
 * Creates a hidden mirror canvas to receive frames from worker for main-thread recording
 * @param {number} width - Canvas width
 * @param {number} height - Canvas height
 * @returns {HTMLCanvasElement} Mirror canvas
 */
function createMirrorCanvas(width, height) {
  console.log('Creating mirror canvas:', width, 'x', height);

  // Create canvas element
  const canvas = document.createElement('canvas');
  canvas.id = 'mirror-canvas';
  canvas.width = width;
  canvas.height = height;

  // Hide canvas (not displayed, only used for recording)
  canvas.style.position = 'absolute';
  canvas.style.left = '-9999px';
  canvas.style.top = '-9999px';
  canvas.style.pointerEvents = 'none';

  // Add to DOM (required for MediaRecorder)
  document.body.appendChild(canvas);

  console.log('Mirror canvas created and added to DOM');

  return canvas;
}

/**
 * Sets up message listener for frame updates from worker
 */
function setupWorkerFrameListener() {
  if (!window.fastLEDWorkerManager || !window.fastLEDWorkerManager.worker) {
    console.error('Worker not available for frame listener setup');
    return;
  }

  // Add custom handler for frame_update messages
  const originalHandler = window.fastLEDWorkerManager.worker.onmessage;

  window.fastLEDWorkerManager.worker.onmessage = (event) => {
    const { type, payload } = event.data;

    if (type === 'frame_update' && payload && payload.bitmap) {
      handleWorkerFrameUpdate(payload);
      return; // Don't pass to original handler
    }

    // Pass other messages to original handler
    if (originalHandler) {
      originalHandler(event);
    }
  };

  console.log('Worker frame listener setup complete');
}

/**
 * Handles frame update from worker - draws to mirror canvas
 * @param {Object} payload - Frame data with ImageBitmap
 */
function handleWorkerFrameUpdate(payload) {
  const { bitmap, width, height } = payload;

  try {
    // Create mirror canvas if needed or resize if dimensions changed
    if (!mirrorCanvas || mirrorCanvas.width !== width || mirrorCanvas.height !== height) {
      console.log('Creating/resizing mirror canvas:', width, 'x', height);

      if (mirrorCanvas) {
        document.body.removeChild(mirrorCanvas);
      }

      mirrorCanvas = createMirrorCanvas(width, height);
      mirrorCanvasContext = mirrorCanvas.getContext('2d', { willReadFrequently: false });
    }

    // Draw bitmap to mirror canvas (hardware-accelerated)
    if (mirrorCanvasContext) {
      mirrorCanvasContext.drawImage(bitmap, 0, 0);
    }

    // Bitmap is transferred, no need to close it
  } catch (error) {
    console.error('Error drawing frame to mirror canvas:', error);
  }
}

/**
 * Initializes main-thread video recording with worker frame transfer
 * Worker captures frames as ImageBitmap and sends to main thread
 * Main thread draws to mirror canvas and uses MediaRecorder (where it's available)
 * @param {HTMLElement} recordButton - The record button element
 */
function initializeWorkerVideoRecorder(recordButton) {
  console.log('Initializing main-thread video recorder with worker frame transfer');

  // Check MediaRecorder support
  if (typeof MediaRecorder === 'undefined') {
    console.warn('MediaRecorder API not supported, video recording disabled');
    recordButton.style.display = 'none';
    return;
  }

  // Make record button visible
  recordButton.classList.add('visible');
  console.log('Record button made visible');

  // Set up worker frame listener
  setupWorkerFrameListener();

  let isRecording = false;

  // Handle record button click
  recordButton.addEventListener('click', async () => {
    if (!window.fastLEDWorkerManager || !window.fastLEDWorkerManager.isWorkerActive) {
      console.error('Worker not active, cannot record');
      return;
    }

    try {
      if (!isRecording) {
        // === START RECORDING ===

        // Send start message to worker to begin frame capture
        const response = await window.fastLEDWorkerManager.sendMessageWithResponse({
          type: 'start_recording',
          payload: {
            fps: 60,
            settings: window.getVideoSettings ? window.getVideoSettings() : {}
          }
        });

        if (!response.success) {
          throw new Error('Worker failed to start frame capture');
        }

        console.log('Worker frame capture started:', response);

        // Wait for mirror canvas to be created (give frames time to arrive)
        await new Promise((resolve) => {
          setTimeout(resolve, 500);
        });

        if (!mirrorCanvas) {
          throw new Error('Mirror canvas not created - no frames received from worker');
        }

        // Create VideoRecorder on main thread with mirror canvas
        const defaultSettings = window.getVideoSettings ? window.getVideoSettings() : {};

        mainThreadVideoRecorder = new VideoRecorder({
          canvas: mirrorCanvas,
          audioContext: null, // No audio for now
          fps: response.fps || 60,
          settings: {
            ...defaultSettings,
            fps: undefined // Remove fps from settings to use constructor parameter
          },
          onStateChange: (recording) => {
            isRecording = recording;

            if (recording) {
              recordButton.classList.add('recording');
              const recordIcon = /** @type {HTMLElement|null} */ (recordButton.querySelector('.record-icon'));
              const stopIcon = /** @type {HTMLElement|null} */ (recordButton.querySelector('.stop-icon'));
              if (recordIcon) recordIcon.style.display = 'none';
              if (stopIcon) stopIcon.style.display = 'block';
            } else {
              recordButton.classList.remove('recording');
              const recordIcon = /** @type {HTMLElement|null} */ (recordButton.querySelector('.record-icon'));
              const stopIcon = /** @type {HTMLElement|null} */ (recordButton.querySelector('.stop-icon'));
              if (recordIcon) recordIcon.style.display = 'block';
              if (stopIcon) stopIcon.style.display = 'none';
            }

            // Update tooltip
            if (window.updateRecordButtonTooltip) {
              window.updateRecordButtonTooltip();
            }
          }
        });

        // Start recording on main thread
        mainThreadVideoRecorder.startRecording();

        console.log('Main-thread recording started with codec:', mainThreadVideoRecorder.selectedMimeType);
      } else {
        // === STOP RECORDING ===

        // Stop recording on main thread
        if (mainThreadVideoRecorder) {
          mainThreadVideoRecorder.stopRecording();
        }

        // Tell worker to stop frame capture
        await window.fastLEDWorkerManager.sendMessageWithResponse({
          type: 'stop_recording',
          payload: {}
        });

        console.log('Recording stopped');
      }
    } catch (error) {
      console.error('Recording error:', error);
      isRecording = false;
      recordButton.classList.remove('recording');
    }
  });

  console.log('Main-thread video recorder initialized with worker frame transfer');
}

/**
 * Actually initializes the video recorder once canvas is ready
 */
function actuallyInitializeVideoRecorder(canvas, recordButton) {
  // Note: Audio context removed for async video recorder
  // Audio recording will be handled separately if needed

  // Check if MediaRecorder is supported
  if (typeof MediaRecorder === 'undefined') {
    console.warn('MediaRecorder API not supported, video recording disabled');
    recordButton.style.display = 'none';
    return;
  }

  // Always use default settings (no localStorage persistence)
  const defaultSettings = window.getVideoSettings ? window.getVideoSettings() : null;

  try {
    // Validate canvas is ready (without creating a context that would conflict with graphics manager)
    if (!canvas.getContext) {
      throw new Error('Canvas does not support getContext method');
    }

    // Don't create a context here - the graphics manager handles context creation
    // Just validate the canvas element is ready for use

    // Create video recorder instance with optimized settings
    videoRecorder = new VideoRecorder({
      canvas,
      audioContext: null, // Disable audio for better performance
      fps: defaultSettings?.fps || 60,
      settings: {
        // Use default settings but exclude fps to avoid conflicts
        ...defaultSettings,
        fps: undefined, // Remove fps from settings to ensure constructor fps parameter takes precedence
      },
      onStateChange: (isRecording) => {
        // Update button visual state
        if (isRecording) {
          recordButton.classList.add('recording');
          // Update icon
          const recordIcon = recordButton.querySelector('.record-icon');
          const stopIcon = recordButton.querySelector('.stop-icon');
          if (recordIcon) recordIcon.style.display = 'none';
          if (stopIcon) stopIcon.style.display = 'block';
        } else {
          recordButton.classList.remove('recording');
          // Update icon
          const recordIcon = recordButton.querySelector('.record-icon');
          const stopIcon = recordButton.querySelector('.stop-icon');
          if (recordIcon) recordIcon.style.display = 'block';
          if (stopIcon) stopIcon.style.display = 'none';
        }

        // Update tooltip with current encoding format
        // Use setTimeout to ensure the state change is complete
        setTimeout(() => {
          if (window.updateRecordButtonTooltip) {
            window.updateRecordButtonTooltip();
          }
        }, 0);
      },
    });

    // Expose video recorder globally for settings updates
    window.videoRecorder = videoRecorder;

    // Create custom tooltip element
    const tooltip = document.createElement('div');
    tooltip.id = 'record-tooltip';
    tooltip.style.cssText = `
      position: absolute;
      background: rgba(0, 0, 0, 0.9);
      color: white;
      padding: 8px 12px;
      border-radius: 4px;
      font-size: 14px;
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
      white-space: nowrap;
      z-index: 10000;
      pointer-events: none;
      display: none;
      box-shadow: 0 2px 8px rgba(0, 0, 0, 0.3);
    `;
    document.body.appendChild(tooltip);

    // Function to update record button tooltip with current encoding format
    function updateRecordButtonTooltip() {
      if (videoRecorder && recordButton) {
        const codecName = videoRecorder.getCodecDisplayName();
        const isRecording = videoRecorder.getIsRecording();

        let tooltipText;
        if (isRecording) {
          tooltipText = `Stop Recording (${codecName})`;
        } else {
          tooltipText = `Record in ${codecName}`;
        }

        // Store tooltip text for mouse events
        recordButton.setAttribute('data-tooltip', tooltipText);
        // Remove browser default tooltip
        recordButton.removeAttribute('title');
      }
    }

    // Add mouse events for immediate tooltip display
    recordButton.addEventListener('mouseenter', (e) => {
      const tooltipText = e.target.getAttribute('data-tooltip');
      if (tooltipText) {
        tooltip.textContent = tooltipText;
        tooltip.style.display = 'block';

        // Position tooltip above the button
        const rect = recordButton.getBoundingClientRect();
        tooltip.style.left = `${rect.left + rect.width / 2}px`;
        tooltip.style.top = `${rect.top - tooltip.offsetHeight - 8}px`;

        // Adjust horizontal position if tooltip would go off-screen
        const tooltipRect = tooltip.getBoundingClientRect();
        if (tooltipRect.right > window.innerWidth - 10) {
          tooltip.style.left = `${window.innerWidth - tooltipRect.width - 10}px`;
        }
        if (tooltipRect.left < 10) {
          tooltip.style.left = '10px';
        }
      }
    });

    recordButton.addEventListener('mouseleave', () => {
      tooltip.style.display = 'none';
    });

    recordButton.addEventListener('mousemove', () => {
      // Update position on mouse move for better tracking
      const rect = recordButton.getBoundingClientRect();
      tooltip.style.left = `${rect.left + rect.width / 2}px`;
      tooltip.style.top = `${rect.top - tooltip.offsetHeight - 8}px`;

      // Center tooltip horizontally relative to button
      const tooltipRect = tooltip.getBoundingClientRect();
      const centerOffset = tooltipRect.width / 2;
      tooltip.style.left = `${rect.left + rect.width / 2 - centerOffset}px`;
    });

    // Update tooltip initially
    updateRecordButtonTooltip();

    // Expose globally so it can be called when settings change
    window.updateRecordButtonTooltip = updateRecordButtonTooltip;

    // Add click handler to record button
    recordButton.addEventListener('click', (e) => {
      e.preventDefault();
      if (videoRecorder) {
        videoRecorder.toggleRecording();
      }
    });

    // Add keyboard shortcut (Ctrl+R or Cmd+R)
    document.addEventListener('keydown', (e) => {
      if ((e.ctrlKey || e.metaKey) && e.key === 'r' && !e.shiftKey) {
        e.preventDefault();
        if (videoRecorder) {
          videoRecorder.toggleRecording();
        }
      }
    });

    console.log('Optimized video recorder initialized (performance settings applied)');
  } catch (error) {
    console.error('Failed to initialize video recorder:', error);
    recordButton.style.display = 'none';
  }
}

// Initialize video recorder after FastLED is ready
// Prefer event-based initialization for faster response, with timeout fallback
let videoRecorderInitialized = false;

// Listen for worker:initialized event for immediate initialization
if (typeof window !== 'undefined' && window.fastLEDEvents) {
  window.fastLEDEvents.on('worker:initialized', () => {
    if (!videoRecorderInitialized) {
      console.log('Worker initialized event received, initializing video recorder immediately');
      videoRecorderInitialized = true;
      initializeVideoRecorder();
    }
  });
}

// Fallback timeout in case worker mode fails or events don't fire
// Reduced from 2000ms to 500ms for better responsiveness
setTimeout(() => {
  if (!videoRecorderInitialized) {
    console.log('Initializing video recorder via timeout fallback (worker event not received)');
    videoRecorderInitialized = true;
    initializeVideoRecorder();
  }
}, 500); // Reduced timeout - worker initializes at ~200ms, 500ms gives buffer

// Expose video recorder functions globally for debugging
window.getVideoRecorder = () => videoRecorder;
window.startVideoRecording = () => videoRecorder?.startRecording();
window.stopVideoRecording = () => videoRecorder?.stopRecording();
window.testVideoRecording = () => videoRecorder?.testRecording();
