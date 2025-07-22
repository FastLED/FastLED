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

/* eslint-disable import/prefer-default-export */

/* eslint-disable import/extensions */

import { JsonUiManager } from './modules/ui_manager.js';
import { GraphicsManager } from './modules/graphics_manager.js';
import { GraphicsManagerThreeJS } from './modules/graphics_manager_threejs.js';
import { isDenseGrid } from './modules/graphics_utils.js';
import { JsonInspector } from './modules/json_inspector.js';

/** URL parameters for runtime configuration */
const urlParams = new URLSearchParams(window.location.search);

/** Force fast 2D renderer when gfx=0 URL parameter is present */
const FORCE_FAST_RENDERER = urlParams.get('gfx') === '0';

/** Force beautiful 3D renderer when gfx=1 URL parameter is present */
const FORCE_THREEJS_RENDERER = urlParams.get('gfx') === '1';

/** Maximum number of lines to keep in stdout output display */
const MAX_STDOUT_LINES = 50;

/** Default frame rate for FastLED animations (60 FPS) */
const DEFAULT_FRAME_RATE_60FPS = 60;

/** Current frame rate setting */
let frameRate = DEFAULT_FRAME_RATE_60FPS;

/** Flag indicating if canvas data has been received */
const receivedCanvas = false;

/**
 * Screen mapping data structure containing strip-to-screen coordinate mappings
 * @type {Object} screenMap
 * @property {Object} strips - Map of strip ID to screen coordinate data
 * @property {number[]} absMin - Absolute minimum [x, y] coordinates
 * @property {number[]} absMax - Absolute maximum [x, y] coordinates
 */
const screenMap = {
  strips: {},
  absMin: [0, 0],
  absMax: [0, 0],
};

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

/**
 * AsyncFastLEDController - Handles Asyncify-enabled FastLED setup and loop functions
 * 
 * This controller manages the async lifecycle of FastLED programs when Emscripten's
 * Asyncify feature is enabled. It handles both synchronous and asynchronous returns
 * from extern_setup and extern_loop functions, providing smooth animation loops
 * without blocking the browser's main thread.
 * 
 * @example Basic Usage:
 * ```javascript
 * // After WASM module is loaded
 * const controller = new AsyncFastLEDController(moduleInstance, 60);
 * await controller.setup();
 * controller.start();
 * 
 * // Monitor performance
 * setInterval(() => {
 *   console.log(`FPS: ${controller.getFPS().toFixed(1)}`);
 * }, 1000);
 * 
 * // Stop when needed
 * controller.stop();
 * ```
 * 
 * @example With Error Handling:
 * ```javascript
 * try {
 *   const controller = new AsyncFastLEDController(moduleInstance);
 *   await controller.setup();
 *   controller.start();
 *   
 *   // Add to global scope for debugging
 *   window.fastLEDController = controller;
 * } catch (error) {
 *   console.error('FastLED initialization failed:', error);
 *   document.getElementById('error-display').textContent = 
 *     'FastLED failed to start. Check console for details.';
 * }
 * ```
 * 
 * @example Performance Monitoring:
 * ```javascript
 * const controller = new AsyncFastLEDController(moduleInstance);
 * await controller.setup();
 * controller.start();
 * 
 * // Monitor performance stats
 * function updatePerformanceDisplay() {
 *   const stats = controller.getPerformanceStats();
 *   console.log('FastLED Performance:', {
 *     fps: stats.fps.toFixed(1),
 *     frameTime: stats.averageFrameTime.toFixed(2) + 'ms',
 *     totalFrames: stats.frameCount,
 *     running: stats.running
 *   });
 * }
 * setInterval(updatePerformanceDisplay, 2000);
 * ```
 */
class AsyncFastLEDController {
  constructor(moduleInstance, frameRate = DEFAULT_FRAME_RATE_60FPS) {
    this.moduleInstance = moduleInstance;
    this.frameRate = frameRate;
    this.running = false;
    this.frameCount = 0;
    this.frameTimes = [];
    this.maxFrameTimeHistory = 60; // Track last 60 frames
    this.setupCompleted = false;
    this.lastFrameTime = 0;
    this.frameInterval = 1000 / this.frameRate;
    
    // Bind functions to preserve 'this' context
    this.loop = this.loop.bind(this);
    this.stop = this.stop.bind(this);
  }

  /**
   * Safely calls an async function, handling both sync and async returns
   * @param {string} funcName - Name of the function for error reporting
   * @param {Function} func - The function to call
   * @param {...*} args - Arguments to pass to the function
   * @returns {Promise<*>} Promise that resolves with the function result
   */
  async safeCall(funcName, func, ...args) {
    try {
      const result = func(...args);
      
      // Handle both async and sync returns from Asyncify
      if (result instanceof Promise) {
        return await result;
      }
      return result;
      
    } catch (error) {
      if (error.name === 'ExitStatus') {
        console.error(`FastLED function ${funcName} exited with code:`, error.status);
      } else if (error.message && error.message.includes('unreachable')) {
        console.error(`FastLED function ${funcName} hit unreachable code - possible memory corruption`);
      } else {
        console.error(`FastLED function ${funcName} error:`, error);
      }
      throw error;
    }
  }

  /**
   * Initializes the FastLED program by calling the async-aware setup function
   * @returns {Promise<void>} Promise that resolves when setup is complete
   */
  async setup() {
    if (this.setupCompleted) {
      console.warn('FastLED setup already completed, skipping');
      return;
    }

    console.log('Setting up FastLED with Asyncify support...');
    try {
      await this.safeCall('extern_setup', this.moduleInstance._extern_setup);
      this.setupCompleted = true;
      console.log('FastLED setup completed successfully');
    } catch (error) {
      console.error('FastLED setup failed:', error);
      throw error;
    }
  }

  /**
   * Starts the async animation loop
   * Uses requestAnimationFrame for smooth, non-blocking animation
   */
  start() {
    if (this.running) {
      console.warn('FastLED loop is already running');
      return;
    }

    if (!this.setupCompleted) {
      console.error('Cannot start loop: setup() must be called first');
      return;
    }

    console.log('Starting FastLED async loop...');
    this.running = true;
    this.frameCount = 0;
    this.lastFrameTime = performance.now();
    requestAnimationFrame(this.loop);
  }

  /**
   * Main animation loop function - handles async extern_loop calls
   * @param {number} currentTime - Current timestamp from requestAnimationFrame
   */
  async loop(currentTime) {
    if (!this.running) return;

    // Maintain consistent frame rate
    if (currentTime - this.lastFrameTime < this.frameInterval) {
      requestAnimationFrame(this.loop);
      return;
    }

    const frameStartTime = performance.now();
    
    try {
      // Call the async-aware extern_loop function
      await this.safeCall('extern_loop', this.moduleInstance._extern_loop);
      
      this.frameCount++;
      this.lastFrameTime = currentTime;
      
      // Track performance
      const frameEndTime = performance.now();
      const frameTime = frameEndTime - frameStartTime;
      this.trackFramePerformance(frameTime);
      
      // Schedule next frame
      this.scheduleNextFrame(frameTime);
      
    } catch (error) {
      console.error('Fatal error in FastLED async loop:', error);
      this.stop();
    }
  }

  /**
   * Tracks frame performance and logs warnings for slow frames
   * @param {number} frameTime - Time taken for this frame in milliseconds
   */
  trackFramePerformance(frameTime) {
    this.frameTimes.push(frameTime);
    if (this.frameTimes.length > this.maxFrameTimeHistory) {
      this.frameTimes.shift();
    }

    // Log performance warnings for slow frames
    if (frameTime > 32) { // Slower than ~30 FPS
      console.warn(`Slow FastLED frame: ${frameTime.toFixed(2)}ms`);
    }
  }

  /**
   * Schedules the next animation frame with adaptive frame rate
   * @param {number} frameTime - Time taken for the current frame
   */
  scheduleNextFrame(frameTime) {
    const avgFrameTime = this.getAverageFrameTime();
    
    if (avgFrameTime > 16) {
      // If we can't maintain 60fps, throttle to 30fps
      setTimeout(() => requestAnimationFrame(this.loop), 16);
    } else {
      requestAnimationFrame(this.loop);
    }
  }

  /**
   * Stops the animation loop
   */
  stop() {
    if (!this.running) return;
    
    console.log('Stopping FastLED async loop...');
    this.running = false;
  }

  /**
   * Gets the average frame time over recent frames
   * @returns {number} Average frame time in milliseconds
   */
  getAverageFrameTime() {
    if (this.frameTimes.length === 0) return 0;
    return this.frameTimes.reduce((a, b) => a + b, 0) / this.frameTimes.length;
  }

  /**
   * Gets the current frames per second
   * @returns {number} Current FPS
   */
  getFPS() {
    const avgFrameTime = this.getAverageFrameTime();
    return avgFrameTime > 0 ? 1000 / avgFrameTime : 0;
  }

  /**
   * Gets performance statistics
   * @returns {Object} Performance stats object
   */
  getPerformanceStats() {
    return {
      frameCount: this.frameCount,
      averageFrameTime: this.getAverageFrameTime(),
      fps: this.getFPS(),
      running: this.running,
      setupCompleted: this.setupCompleted
    };
  }
}

/**
 * Global reference to the current AsyncFastLEDController instance
 * @type {AsyncFastLEDController|null}
 */
let fastLEDController = null;

/**
 * Stub FastLED loader function (replaced during initialization)
 * @param {Object} options - Loading options
 * @returns {Promise<null>} Always returns null (stub implementation)
 */
let _loadFastLED = function (options) {
  // Stub to let the user/dev know that something went wrong.
  // This function is replaced with an async implementation, so it must be async for interface compatibility
  console.log('FastLED loader function was not set.');
  return Promise.resolve(null);
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
 */
let print = function () {};

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
const originalConsole = console;
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
 * Calculates minimum and maximum values from coordinate arrays
 * @param {number[]} x_array - Array of X coordinates
 * @param {number[]} y_array - Array of Y coordinates
 * @returns {Array<Array<number>>} [[min_x, min_y], [max_x, max_y]]
 */
function minMax(x_array, y_array) {
  let min_x = x_array[0];
  let min_y = y_array[0];
  let max_x = x_array[0];
  let max_y = y_array[0];
  for (let i = 1; i < x_array.length; i++) {
    min_x = Math.min(min_x, x_array[i]);
    min_y = Math.min(min_y, y_array[i]);
    max_x = Math.max(max_x, x_array[i]);
    max_y = Math.max(max_y, y_array[i]);
  }
  return [[min_x, min_y], [max_x, max_y]];
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
 * @param {Array<Object>} frameData - Array of strip data with pixel information
 */
function updateCanvas(frameData) {
  // we are going to add the screenMap to the graphicsManager
  if (frameData.screenMap === undefined) {
    console.warn('Screen map not found in frame data, skipping canvas update');
    return;
  }
  if (!graphicsManager) {
    const isDenseMap = isDenseGrid(frameData);
    if (FORCE_THREEJS_RENDERER) {
      console.log('Creating Beautiful GraphicsManager with canvas ID (forced)', canvasId);
      graphicsManager = new GraphicsManagerThreeJS(graphicsArgs);
    } else if (FORCE_FAST_RENDERER) {
      console.log('Creating Fast GraphicsManager with canvas ID (forced)', canvasId);
      graphicsManager = new GraphicsManager(graphicsArgs);
    } else if (isDenseMap) {
      console.log('Creating Fast GraphicsManager with canvas ID', canvasId);
      graphicsManager = new GraphicsManager(graphicsArgs);
    } else {
      console.log('Creating Beautiful GraphicsManager with canvas ID', canvasId);
      graphicsManager = new GraphicsManagerThreeJS(graphicsArgs);
    }
    uiCanvasChanged = false;
  }

  if (uiCanvasChanged) {
    uiCanvasChanged = false;
    graphicsManager.reset();
  }

  graphicsManager.updateCanvas(frameData);
}

/**
 * Main setup and loop execution function for FastLED programs (Asyncify-enabled)
 * @async
 * @param {Object} moduleInstance - The WASM module instance
 * @param {number} frame_rate - Target frame rate for the animation loop
 * @returns {Promise<void>} Promise that resolves when setup is complete and loop is started
 */
async function FastLED_SetupAndLoop(moduleInstance, frame_rate) {
  try {
    // Create the async controller
    fastLEDController = new AsyncFastLEDController(moduleInstance, frame_rate);
    
    // Expose controller globally for debugging and external control
    window.fastLEDController = fastLEDController;
    
    // Setup FastLED asynchronously
    await fastLEDController.setup();
    
    // Start the async animation loop
    fastLEDController.start();
    
    // Add UI controls for start/stop if elements exist
    const startBtn = document.getElementById('start-btn');
    const stopBtn = document.getElementById('stop-btn');
    const toggleBtn = document.getElementById('toggle-btn');
    const fpsDisplay = document.getElementById('fps-display');
    
    if (startBtn) {
      startBtn.onclick = () => {
        if (fastLEDController.setupCompleted) {
          fastLEDController.start();
        } else {
          console.warn('FastLED setup not completed yet');
        }
      };
    }
    
    if (stopBtn) {
      stopBtn.onclick = () => fastLEDController.stop();
    }
    
    if (toggleBtn) {
      toggleBtn.onclick = () => {
        const isRunning = toggleFastLED();
        toggleBtn.textContent = isRunning ? 'Pause' : 'Resume';
      };
    }
    
    // Performance monitoring display
    if (fpsDisplay) {
      setInterval(() => {
        const fps = fastLEDController.getFPS();
        fpsDisplay.textContent = `FPS: ${fps.toFixed(1)}`;
      }, 1000);
    }
    
    console.log('FastLED Asyncify integration initialized successfully');
    
  } catch (error) {
    console.error('Failed to initialize FastLED with Asyncify:', error);
    
    // Show user-friendly error message if error display element exists
    const errorDisplay = document.getElementById('error-display');
    if (errorDisplay) {
      errorDisplay.textContent = 'Failed to load FastLED. Please refresh the page.';
    }
    
    throw error;
  }
}

/**
 * Async-aware handler for strip update events from the FastLED library
 * Processes screen mapping configuration and canvas setup
 * @async
 * @param {Object} jsonData - Strip update data from FastLED
 * @param {string} jsonData.event - Event type (e.g., 'set_canvas_map')
 * @param {number} jsonData.strip_id - ID of the LED strip
 * @param {Object} jsonData.map - Coordinate mapping data
 * @param {number} [jsonData.diameter] - LED diameter in mm (default: 0.2)
 * @returns {Promise<void>} Promise that resolves when strip update is processed
 */
async function FastLED_onStripUpdate(jsonData) {
  try {
    // Hooks into FastLED to receive updates from the FastLED library related
    // to the strip state. This is where the ScreenMap will be effectively set.
    // uses global variables.
    console.log('Received async strip update:', jsonData);
    const { event } = jsonData;
    let width = 0;
    let height = 0;
    let eventHandled = false;
    
    if (event === 'set_canvas_map') {
      eventHandled = true;
      // Work in progress.
      const { map } = jsonData;
      console.log('Received map:', jsonData);
      const [min, max] = minMax(map.x, map.y);
      console.log('min', min, 'max', max);

      const stripId = jsonData.strip_id;
      const isUndefined = (value) => typeof value === 'undefined';
      if (isUndefined(stripId)) {
        throw new Error('strip_id is required for set_canvas_map event');
      }

      let diameter = jsonData.diameter;
      if (diameter === undefined) {
        const stripId = jsonData.strip_id;
        console.warn(`Diameter was unset for strip ${stripId}, assuming default value of 2 mm.`);
        diameter = 0.2;
      }

      screenMap.strips[stripId] = {
        map,
        min,
        max,
        diameter: diameter,
      };
      console.log('Screen map updated:', screenMap);
      
      // iterate through all the screenMaps and get the absolute min and max
      const absMin = [Number.POSITIVE_INFINITY, Number.POSITIVE_INFINITY];
      const absMax = [Number.NEGATIVE_INFINITY, Number.NEGATIVE_INFINITY];
      let setAtLeastOnce = false;
      for (const stripId in screenMap.strips) {
        if (!Object.prototype.hasOwnProperty.call(screenMap.strips, stripId)) continue;
        console.log('Processing strip ID', stripId);
        const id = Number.parseInt(stripId, 10);
        const stripData = screenMap.strips[id];
        absMin[0] = Math.min(absMin[0], stripData.min[0]);
        absMin[1] = Math.min(absMin[1], stripData.min[1]);
        absMax[0] = Math.max(absMax[0], stripData.max[0]);
        absMax[1] = Math.max(absMax[1], stripData.max[1]);
        // if diff x = 0, expand by one on each direction.
        if (absMin[0] === absMax[0]) {
          absMin[0] = absMin[0] - 1;
          absMax[0] = absMax[0] + 1;
        }
        // if diff y = 0, expand by one on each direction.
        if (absMin[1] === absMax[1]) {
          absMin[1] = absMin[1] - 1;
          absMax[1] = absMax[1] + 1;
        }
        setAtLeastOnce = true;
      }
      
      if (!setAtLeastOnce) {
        console.error('No screen map data found, skipping canvas size update');
        return; // Early return, but still async
      }
      
      screenMap.absMin = absMin;
      screenMap.absMax = absMax;
      width = Number.parseInt(absMax[0] - absMin[0], 10) + 1;
      height = Number.parseInt(absMax[1] - absMin[1], 10) + 1;
      console.log('canvas updated with width and height', width, height);
      
      // now update the canvas size asynchronously
      const canvas = document.getElementById(canvasId);
      if (canvas) {
        canvas.width = width;
        canvas.height = height;
        uiCanvasChanged = true;
        console.log('Screen map updated:', screenMap);
      } else {
        console.warn('Canvas element not found:', canvasId);
      }
    }

    if (!eventHandled) {
      console.warn(`We do not support event ${event} yet.`);
      return; // Early return, but still async
    }
    
    if (receivedCanvas) {
      console.warn(
        'Canvas size has already been set, setting multiple canvas sizes is not supported yet and the previous one will be overwritten.',
      );
    }
    
    const canvas = document.getElementById(canvasId);
    if (canvas) {
      canvas.width = width;
      canvas.height = height;

      // Set display size (CSS pixels) to 640px width while maintaining aspect ratio
      const displayWidth = 640;
      const displayHeight = Math.round((height / width) * displayWidth);

      // Set CSS display size while maintaining aspect ratio
      canvas.style.width = `${displayWidth}px`;
      canvas.style.height = `${displayHeight}px`;
      console.log(
        `Canvas size set to ${width}x${height}, displayed at ${canvas.style.width}x${canvas.style.height} `,
      );
    }
    
    // unconditionally delete the graphicsManager (async safe)
    if (graphicsManager) {
      graphicsManager.reset();
      graphicsManager = null;
    }
    
    // Yield to allow other async operations
    await new Promise(resolve => setTimeout(resolve, 0));
    
  } catch (error) {
    console.error('Error in async FastLED_onStripUpdate:', error);
    throw error; // Re-throw to let C++ side handle the error
  }
}

/**
 * Async-aware handler for new LED strip registration events
 * @async
 * @param {number} stripId - Unique identifier for the LED strip
 * @param {number} stripLength - Number of LEDs in the strip
 * @returns {Promise<void>} Promise that resolves when strip addition is processed
 */
async function FastLED_onStripAdded(stripId, stripLength) {
  try {
    // uses global variables.
    const output = document.getElementById(outputId);
    if (output) {
      output.textContent += `Strip added: ID ${stripId}, length ${stripLength}\n`;
    }
    
    // Log for debugging
    console.log(`FastLED Strip Added: ID ${stripId}, Length ${stripLength}`);
    
    // Yield to allow other async operations
    await new Promise(resolve => setTimeout(resolve, 0));
    
  } catch (error) {
    console.error('Error in async FastLED_onStripAdded:', error);
    throw error; // Re-throw to let C++ side handle the error
  }
}

/**
 * Async-aware main frame processing function called by FastLED for each animation frame
 * @async
 * @param {Array<Object>} frameData - Array of strip data with pixel colors
 * @param {Function} uiUpdateCallback - Async callback to send UI changes back to FastLED
 * @returns {Promise<void>} Promise that resolves when frame processing is complete
 */
async function FastLED_onFrame(frameData, uiUpdateCallback) {
  try {
    // uiUpdateCallback is now an async function from FastLED that will parse a json string
    // representing the changes to the UI that FastLED will need to respond to.
    
    // Create async-aware wrapped callback
    let wrappedCallback = uiUpdateCallback;
    if (window.jsonInspector) {
      const originalCallback = uiUpdateCallback;
      wrappedCallback = async function(jsonStr) {
        // Log the event
        window.jsonInspector.wrapUiUpdateCallback(originalCallback);
        
        // Call the original callback and handle async
        const result = originalCallback(jsonStr);
        if (result instanceof Promise) {
          await result;
        }
        return result;
      };
    }
    
    // Process UI changes asynchronously
    const changesJson = uiManager.processUiChanges();
    if (changesJson !== null) {
      const changesJsonStr = JSON.stringify(changesJson);
      
      // Call the async callback and await the result
      const callbackResult = wrappedCallback(changesJsonStr);
      if (callbackResult instanceof Promise) {
        await callbackResult;
      }
    }
    
    if (frameData.length === 0) {
      console.warn('Received empty frame data, skipping canvas update');
      // Still process the frame but skip canvas update
    } else {
      frameData.screenMap = screenMap;
      updateCanvas(frameData);
    }
    
  } catch (error) {
    console.error('Error in async FastLED_onFrame:', error);
    throw error; // Re-throw to let C++ side handle the error
  }
}

/**
 * Async-aware handler for UI element addition events from FastLED
 * @async
 * @param {Object} jsonData - UI element configuration data
 * @returns {Promise<void>} Promise that resolves when UI elements are added
 */
async function FastLED_onUiElementsAdded(jsonData) {
  try {
    // Log the inbound event to the inspector
    if (window.jsonInspector) {
      window.jsonInspector.logInboundEvent(jsonData);
    }
    
    // uses global variables.
    if (uiManager && typeof uiManager.addUiElements === 'function') {
      uiManager.addUiElements(jsonData);
    } else {
      console.warn('UI Manager not available or addUiElements method missing');
    }
    
    // Yield to allow other async operations
    await new Promise(resolve => setTimeout(resolve, 0));
    
  } catch (error) {
    console.error('Error in async FastLED_onUiElementsAdded:', error);
    throw error; // Re-throw to let C++ side handle the error
  }
}

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

  // Bind the async-aware functions to the global scope.
  globalThis.FastLED_onUiElementsAdded = FastLED_onUiElementsAdded;
  globalThis.FastLED_onFrame = FastLED_onFrame;
  globalThis.FastLED_onStripAdded = FastLED_onStripAdded;
  globalThis.FastLED_onStripUpdate = FastLED_onStripUpdate;
  
  // Add debugging info for async functions
  console.log('Registered async FastLED callback functions:', {
    FastLED_onUiElementsAdded: typeof globalThis.FastLED_onUiElementsAdded,
    FastLED_onFrame: typeof globalThis.FastLED_onFrame,
    FastLED_onStripAdded: typeof globalThis.FastLED_onStripAdded,
    FastLED_onStripUpdate: typeof globalThis.FastLED_onStripUpdate
  });

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
    const delay = new Promise((r) => setTimeout(r, 250));
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
    const exports_exist = moduleInstance && moduleInstance._extern_setup &&
      moduleInstance._extern_loop;
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
        typeof fastledLoader,
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
    const jsonInspector = new JsonInspector();

    // Expose UI manager globally for debug functions and C++ module
    window.uiManager = uiManager;
    window.uiManagerInstance = uiManager;

    // Apply pending debug mode setting if it was set before manager creation
    if (typeof window._pendingUiDebugMode !== 'undefined') {
      uiManager.setDebugMode(window._pendingUiDebugMode);
      delete window._pendingUiDebugMode;
    }

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
 * @returns {AsyncFastLEDController|null} Current controller instance or null
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
 * @returns {boolean} True if started successfully, false otherwise
 */
function startFastLED() {
  if (!fastLEDController) {
    console.error('FastLED controller not initialized');
    return false;
  }
  fastLEDController.start();
  return true;
}

/**
 * Stops the FastLED animation loop (for external control)
 * @returns {boolean} True if stopped successfully, false otherwise
 */
function stopFastLED() {
  if (!fastLEDController) {
    console.error('FastLED controller not initialized');
    return false;
  }
  fastLEDController.stop();
  return true;
}

/**
 * Toggles the FastLED animation loop
 * @returns {boolean} True if now running, false if now stopped
 */
function toggleFastLED() {
  if (!fastLEDController) {
    console.error('FastLED controller not initialized');
    return false;
  }
  
  if (fastLEDController.running) {
    fastLEDController.stop();
    return false;
  } else {
    fastLEDController.start();
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
      event.reason.message?.includes('FastLED') ||
      event.reason.message?.includes('extern_setup') ||
      event.reason.message?.includes('extern_loop') ||
      event.reason.stack?.includes('AsyncFastLEDController')
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

// Set up global error handlers
setupGlobalErrorHandlers();
