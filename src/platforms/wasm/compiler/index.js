/* eslint-disable no-console */
/* eslint-disable import/prefer-default-export */
/* eslint-disable no-restricted-syntax */
/* eslint-disable max-len */
/* eslint-disable guard-for-in */
/* eslint-disable camelcase */
/* eslint-disable no-underscore-dangle */
/* eslint-disable no-plusplus */
/* eslint-disable no-continue */
/* eslint-disable import/extensions */

import { UiManager } from './modules/ui_manager.js';
import { GraphicsManager } from './modules/graphics_manager.js';
import { GraphicsManagerThreeJS, isDenseGrid } from './modules/graphics_manager_threejs.js';

const urlParams = new URLSearchParams(window.location.search);
const FORCE_FAST_RENDERER = urlParams.get('gfx') === '0';
const FORCE_THREEJS_RENDERER = urlParams.get('gfx') === '1';
const MAX_STDOUT_LINES = 50;

const DEFAULT_FRAME_RATE_60FPS = 60; // 60 FPS
let frameRate = DEFAULT_FRAME_RATE_60FPS;
const receivedCanvas = false;
// screenMap contains data mapping a strip id to a screen map,
// transforming led strip data pixel with an index
// to a screen pixel with xy.
const screenMap = {
  strips: {},
  absMin: [0, 0],
  absMax: [0, 0],
};
let canvasId;
let uiControlsId;
let outputId;

let uiManager;
let uiCanvasChanged = false;
let threeJsModules = {}; // For graphics.
let graphicsManager;
let containerId; // for ThreeJS
let graphicsArgs = {};

async function _loadFastLED(options) { // eslint-disable-line no-unused-vars
  // Stub to let the user/dev know that something went wrong.
  console.log('FastLED loader function was not set.');
  return null;
}

export async function loadFastLED(options) {
  // This will be overridden by through the initialization.
  return await _loadFastLED(options); // eslint-disable-line no-return-await
}

const EPOCH = new Date().getTime();
function getTimeSinceEpoc() {
  const outMS = new Date().getTime() - EPOCH;
  const outSec = outMS / 1000;
  // one decimal place
  return outSec.toFixed(1);
}
// Will be overridden during initialization.
let print = function () { }; // eslint-disable-line func-names

const prev_console = console;

// Override console.log, console.warn, console.error to print to the output element
const _prev_log = prev_console.log;
const _prev_warn = prev_console.warn;
const _prev_error = prev_console.error;

function toStringWithTimeStamp(...args) {
  const time = `${getTimeSinceEpoc()}s`;
  return [time, ...args]; // Return array with time prepended, don't join
}

function log(...args) {
  const argsWithTime = toStringWithTimeStamp(...args);
  _prev_log(...argsWithTime); // Spread the array when calling original logger
  try { print(...argsWithTime); } catch (e) {
    _prev_log('Error in log', e);
  }
}

function warn(...args) {
  const argsWithTime = toStringWithTimeStamp(...args);
  _prev_warn(...argsWithTime);
  try { print(...argsWithTime); } catch (e) {
    _prev_warn('Error in warn', e);
  }
}

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

console = {}; // eslint-disable-line no-global-assign
console.log = log;
console.warn = warn;
console.error = _prev_error;

function jsAppendFileRaw(moduleInstance, path_cstr, data_cbytes, len_int) {
  // Stream this chunk
  moduleInstance.ccall(
    'jsAppendFile',
    'number', // return value
    ['number', 'number', 'number'], // argument types, not sure why numbers works.
    [path_cstr, data_cbytes, len_int],
  );
}

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

function partition(filesJson, immediateExtensions) {
  const immediateFiles = [];
  const streamingFiles = [];
  filesJson.map((file) => { // eslint-disable-line array-callback-return
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

function FastLED_SetupAndLoop(extern_setup, extern_loop, frame_rate) {
  extern_setup();
  console.log('Starting loop...');
  const frameInterval = 1000 / frame_rate;
  let lastFrameTime = 0;
  // Executes every frame but only runs the loop function at the specified frame rate
  function runLoop(currentTime) {
    if (currentTime - lastFrameTime >= frameInterval) {
      extern_loop();
      lastFrameTime = currentTime;
    }
    requestAnimationFrame(runLoop);
  }
  requestAnimationFrame(runLoop);
}

function FastLED_onStripUpdate(jsonData) {
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
    screenMap.strips[stripId] = {
      map,
      min,
      max,
      diameter: jsonData.diameter,
    };
    console.log('Screen map updated:', screenMap);
    // iterate through all the screenMaps and get the absolute min and max
    const absMin = [Number.POSITIVE_INFINITY, Number.POSITIVE_INFINITY];
    const absMax = [Number.NEGATIVE_INFINITY, Number.NEGATIVE_INFINITY];
    let setAtLeastOnce = false;
    for (const stripId in screenMap.strips) { // eslint-disable-line no-shadow
      console.log('Processing strip ID', stripId);
      const id = Number.parseInt(stripId, 10);
      const stripData = screenMap.strips[id];
      absMin[0] = Math.min(absMin[0], stripData.min[0]);
      absMin[1] = Math.min(absMin[1], stripData.min[1]);
      absMax[0] = Math.max(absMax[0], stripData.max[0]);
      absMax[1] = Math.max(absMax[1], stripData.max[1]);
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
    console.warn('Canvas size has already been set, setting multiple canvas sizes is not supported yet and the previous one will be overwritten.');
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
  console.log(`Canvas size set to ${width}x${height}, displayed at ${canvas.style.width}x${canvas.style.height} `);
  // unconditionally delete the graphicsManager
  if (graphicsManager) {
    graphicsManager.reset();
    graphicsManager = null;
  }
}

function FastLED_onStripAdded(stripId, stripLength) {
  // uses global variables.
  const output = document.getElementById(outputId);
  output.textContent += `Strip added: ID ${stripId}, length ${stripLength}\n`;
}

// uiUpdateCallback is a function from FastLED that will parse a json string
// representing the changes to the UI that FastLED will need to respond to.
function FastLED_onFrame(frameData, uiUpdateCallback) {
  // uses global variables.
  const changesJson = uiManager.processUiChanges();
  if (changesJson !== null) {
    const changesJsonStr = JSON.stringify(changesJson);
    uiUpdateCallback(changesJsonStr);
  }
  if (frameData.length === 0) {
    console.warn('Received empty frame data, skipping update');
    return;
  }
  frameData.screenMap = screenMap; // eslint-disable-line no-param-reassign
  updateCanvas(frameData);
}

function FastLED_onUiElementsAdded(jsonData) {
  // uses global variables.
  uiManager.addUiElements(jsonData);
}

// Function to call the setup and loop functions
async function fastledLoadSetupLoop(extern_setup, extern_loop, frame_rate, moduleInstance, filesJson) {
  console.log('Calling setup function...');

  const fileManifest = getFileManifestJson(filesJson, frame_rate);
  moduleInstance._fastled_declare_files(JSON.stringify(fileManifest));
  console.log('Files JSON:', filesJson);

  const processFile = async (file) => {
    try {
      const response = await fetch(file.path);
      const reader = response.body.getReader();

      console.log(`File fetched: ${file.path}, size: ${file.size}`);

      while (true) { // eslint-disable-line no-constant-condition
        const { value, done } = await reader.read(); // eslint-disable-line no-await-in-loop
        if (done) break;
        // Allocate and copy chunk data
        jsAppendFileUint8(moduleInstance, file.path, value);
      }
    } catch (error) {
      console.error(`Error processing file ${file.path}:`, error);
    }
  };

  const fetchAllFiles = async (filesJson, onComplete) => { // eslint-disable-line no-shadow
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
  console.log('The following files will be immediatly available and can be read during setup():', immediateFiles);
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
    const delay = new Promise((r) => setTimeout(r, 250)); // eslint-disable-line no-promise-executor-return
    // Wait for either the time delay or the streaming files to be processed, whichever
    // happens first.
    await Promise.any([delay, streamingFilesPromise]);
  }

  console.log('Starting fastled');
  FastLED_SetupAndLoop(extern_setup, extern_loop, frame_rate);
}

// Ensure we wait for the module to load
async function onModuleLoaded(fastLedLoader) {
  // Unpack the module functions and send them to the fastledLoadSetupLoop function

  function __fastledLoadSetupLoop(moduleInstance, frameRate, filesJson) { // eslint-disable-line no-shadow
    const exports_exist = moduleInstance && moduleInstance._extern_setup && moduleInstance._extern_loop;
    if (!exports_exist) {
      console.error('FastLED setup or loop functions are not available.');
      return;
    }

    fastledLoadSetupLoop(moduleInstance._extern_setup, moduleInstance._extern_loop, frameRate, moduleInstance, filesJson);
  }
  // Start fetch now in parallel
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
      console.log('Could not detect a valid module loading for FastLED, expected function but got', typeof fastledLoader);
    }
  } catch (error) {
    console.error('Failed to load FastLED:', error);
  }
}

async function localLoadFastLed(options) {
  try {
    console.log('Loading FastLED with options:', options);
    canvasId = options.canvasId;
    uiControlsId = options.uiControlsId;
    outputId = options.printId;
    print = customPrintFunction;
    console.log('Loading FastLED with options:', options);
    frameRate = options.frameRate || DEFAULT_FRAME_RATE_60FPS;
    uiManager = new UiManager(uiControlsId);
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
    debugger; // eslint-disable-line no-debugger
  }
}
_loadFastLED = localLoadFastLed; // eslint-disable-line no-func-assign
