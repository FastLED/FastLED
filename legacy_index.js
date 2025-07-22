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
 * Main setup and loop execution function for FastLED programs
 * @param {Function} extern_setup - Setup function from the WASM module
 * @param {Function} extern_loop - Loop function from the WASM module
 * @param {number} frame_rate - Target frame rate for the animation loop
 */
function FastLED_SetupAndLoop(extern_setup, extern_loop, frame_rate) {
  extern_setup();
  console.log('Starting loop...');
  const frameInterval = 1000 / frame_rate;
  let lastFrameTime = 0;

  /**
   * Animation loop function that maintains consistent frame rate
   * @param {number} currentTime - Current timestamp from requestAnimationFrame
   */
  function runLoop(currentTime) {
    if (currentTime - lastFrameTime >= frameInterval) {
      extern_loop();
      lastFrameTime = currentTime;
    }
    requestAnimationFrame(runLoop);
  }
  requestAnimationFrame(runLoop);
}

/**
 * Handles strip update events from the FastLED library
 * Processes screen mapping configuration and canvas setup
 * @param {Object} jsonData - Strip update data from FastLED
 * @param {string} jsonData.event - Event type (e.g., 'set_canvas_map')
 * @param {number} jsonData.strip_id - ID of the LED strip
 * @param {Object} jsonData.map - Coordinate mapping data
 * @param {number} [jsonData.diameter] - LED diameter in mm (default: 0.2)
 */
function FastLED_onStripUpdate(jsonData) {
  // Hooks into FastLED to receive updates from the FastLED library related
  // to the strip state. This is where the ScreenMap will be effectively set.
  // uses global variables.
  console.log('Received strip update:', jsonData);
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
      return;
    }
    screenMap.absMin = absMin;
    screenMap.absMax = absMax;
    width = Number.parseInt(absMax[0] - absMin[0], 10) + 1;
    height = Number.parseInt(absMax[1] - absMin[1], 10) + 1;
    console.log('canvas updated with width and height', width, height);
    // now update the canvas size.
    const canvas = document.getElementById(canvasId);
    canvas.width = width;
    canvas.height = height;
    uiCanvasChanged = true;
    console.log('Screen map updated:', screenMap);
  }

  if (!eventHandled) {
    console.warn(`We do not support event ${event} yet.`);
    return;
  }
  if (receivedCanvas) {
    console.warn(
      'Canvas size has already been set, setting multiple canvas sizes is not supported yet and the previous one will be overwritten.',
    );
  }
  const canvas = document.getElementById(canvasId);
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
  // unconditionally delete the graphicsManager
  if (graphicsManager) {
    graphicsManager.reset();
    graphicsManager = null;
  }
}

/**
 * Handles new LED strip registration events
 * @param {number} stripId - Unique identifier for the LED strip
 * @param {number} stripLength - Number of LEDs in the strip
 */
function FastLED_onStripAdded(stripId, stripLength) {
  // uses global variables.
  const output = document.getElementById(outputId);
  output.textContent += `Strip added: ID ${stripId}, length ${stripLength}\n`;
}

/**
 * Main frame processing function called by FastLED for each animation frame
 * @param {Array<Object>} frameData - Array of strip data with pixel colors
 * @param {Function} uiUpdateCallback - Callback to send UI changes back to FastLED
 */
function FastLED_onFrame(frameData, uiUpdateCallback) {
  // uiUpdateCallback is a function from FastLED that will parse a json string
  // representing the changes to the UI that FastLED will need to respond to.
  // uses global variables.
  const changesJson = uiManager.processUiChanges();
  if (changesJson !== null) {
    const changesJsonStr = JSON.stringify(changesJson);
    uiUpdateCallback(changesJsonStr);
  }
  if (frameData.length === 0) {
    console.warn('Received empty frame data, skipping update');
    // New experiment try to run anyway.
    // return;
  }
  frameData.screenMap = screenMap;
  updateCanvas(frameData);
}

/**
 * Handles UI element addition events from FastLED
 * @param {Object} jsonData - UI element configuration data
 */
function FastLED_onUiElementsAdded(jsonData) {
  // uses global variables.
  uiManager.addUiElements(jsonData);
}

/**
 * Main function to initialize and start the FastLED setup/loop cycle
 * @async
 * @param {Function} extern_setup - Setup function from WASM module
 * @param {Function} extern_loop - Loop function from WASM module
 * @param {number} frame_rate - Target frame rate for animations
 * @param {Object} moduleInstance - The loaded WASM module instance
 * @param {Array<Object>} filesJson - Array of files to load into the virtual filesystem
 */
async function fastledLoadSetupLoop(
  extern_setup,
  extern_loop,
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

  // Bind the functions to the global scope.
  globalThis.FastLED_onUiElementsAdded = FastLED_onUiElementsAdded;
  globalThis.FastLED_onFrame = FastLED_onFrame;
  globalThis.FastLED_onStripAdded = FastLED_onStripAdded;
  globalThis.FastLED_onStripUpdate = FastLED_onStripUpdate;

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

  console.log('Starting fastled');
  FastLED_SetupAndLoop(extern_setup, extern_loop, frame_rate);
}

/**
 * Callback function executed when the WASM module is loaded
 * Sets up the module loading infrastructure
 * @param {Function} fastLedLoader - The FastLED loader function
 */
function onModuleLoaded(fastLedLoader) {
  // Unpack the module functions and send them to the fastledLoadSetupLoop function

  /**
   * Internal function to start FastLED with loaded module
   * @param {Object} moduleInstance - The loaded WASM module instance
   * @param {number} frameRate - Target frame rate for animations
   * @param {Array<Object>} filesJson - Files to load into virtual filesystem
   */
  function __fastledLoadSetupLoop(moduleInstance, frameRate, filesJson) {
    const exports_exist = moduleInstance && moduleInstance._extern_setup &&
      moduleInstance._extern_loop;
    if (!exports_exist) {
      console.error('FastLED setup or loop functions are not available.');
      return;
    }

    fastledLoadSetupLoop(
      moduleInstance._extern_setup,
      moduleInstance._extern_loop,
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
        __fastledLoadSetupLoop(instance, frameRate, filesJson);
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
