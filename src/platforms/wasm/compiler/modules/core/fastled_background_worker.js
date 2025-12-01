/**
 * FastLED Background Worker - Dedicated WASM Worker
 *
 * This worker script runs in a background Web Worker and handles all FastLED WASM
 * compilation and rendering operations using OffscreenCanvas. It provides a clean
 * separation between the main UI thread and intensive rendering operations.
 *
 * Key responsibilities:
 * - Load and initialize FastLED WASM module in worker context
 * - Handle OffscreenCanvas rendering operations
 * - Manage frame data transfer between main thread and worker
 * - Process FastLED setup/loop cycles asynchronously
 * - Optimize data transfer using SharedArrayBuffer when available
 *
 * @module FastLED/BackgroundWorker
 */

/* eslint-env worker */

// Worker-specific imports and initialization - ES6 module imports
// NOTE: Workers created with {type: 'module'} must use ES6 imports, not importScripts()
// The fastled.js WASM module will be loaded dynamically after worker initialization

/* global postMessage, self, performance, requestAnimationFrame, cancelAnimationFrame */

// CRITICAL FIX: Workers don't support import maps, but Three.js jsm files use bare "three" imports.
// Solution: Use local vendor files with patched relative imports to three.module.js

// VideoRecorder import removed - recording now happens on main thread with mirror canvas
// Worker only captures and transfers frames as ImageBitmap for main thread MediaRecorder

/**
 * Loads ThreeJS modules for 3D rendering in worker context
 * @returns {Promise<Object>} ThreeJS modules object
 */
async function loadThreeJSModules() {
  workerLog('LOG', 'BACKGROUND_WORKER', 'Loading ThreeJS modules in worker context...');

  try {
    // WORKER FIX: Use local vendor files with relative paths
    // Workers don't support import maps, so we need to use full relative paths
    const [
      THREE,
      { EffectComposer },
      { RenderPass },
      { UnrealBloomPass },
      { OutputPass },
      BufferGeometryUtils
    ] = await Promise.all([
      // @ts-ignore - Local vendor module import
      import('../../vendor/three/three.module.js'),
      // @ts-ignore - Local vendor module import
      import('../../vendor/three/examples/jsm/postprocessing/EffectComposer.js'),
      // @ts-ignore - Local vendor module import
      import('../../vendor/three/examples/jsm/postprocessing/RenderPass.js'),
      // @ts-ignore - Local vendor module import
      import('../../vendor/three/examples/jsm/postprocessing/UnrealBloomPass.js'),
      // @ts-ignore - Local vendor module import
      import('../../vendor/three/examples/jsm/postprocessing/OutputPass.js'),
      // @ts-ignore - Local vendor module import
      import('../../vendor/three/examples/jsm/utils/BufferGeometryUtils.js')
    ]);

    const modules = {
      THREE,
      EffectComposer,
      RenderPass,
      UnrealBloomPass,
      OutputPass,
      BufferGeometryUtils
    };

    workerLog('LOG', 'BACKGROUND_WORKER', 'ThreeJS modules loaded successfully', {
      moduleNames: Object.keys(modules)
    });

    return modules;
  } catch (error) {
    workerLog('ERROR', 'BACKGROUND_WORKER', 'Failed to load ThreeJS modules', error);
    throw error;
  }
}

/**
 * @typedef {Object} WorkerState
 * @property {boolean} initialized - Worker initialization status
 * @property {OffscreenCanvas|null} canvas - OffscreenCanvas instance
 * @property {Object|null} fastledModule - WASM module instance
 * @property {Object|null} graphicsManager - Graphics rendering manager
 * @property {boolean} running - Animation loop status
 * @property {number} frameRate - Target frame rate
 * @property {Object} capabilities - Worker capabilities
 * @property {Object|null} renderingContext - Rendering context
 * @property {number|null} animationFrameId - Animation frame ID
 * @property {number} frameCount - Frame counter
 * @property {number} startTime - Worker start timestamp
 * @property {number} lastFrameTime - Last frame timestamp
 * @property {number} averageFrameTime - Average frame time in ms
 * @property {Object|null} externFunctions - External functions
 * @property {Object|null} wasmFunctions - WASM functions
 * @property {Function|null} processUiInput - UI input processing function
 * @property {Object} urlParams - URL parameters from main thread
 * @property {boolean} isCapturingFrames - Frame capture enabled for main-thread recording
 * @property {number} frameCaptureInterval - Milliseconds between frame captures
 * @property {number} lastFrameCaptureTime - Timestamp of last frame capture
 * @property {Object} screenMaps - Dictionary of screenmaps (stripId → screenmap, push-based from C++)
 */

/**
 * Worker state management
 * @type {WorkerState}
 */
const workerState = {
  initialized: false,
  canvas: null,
  fastledModule: null,
  graphicsManager: null,
  running: false,
  frameRate: 60,
  capabilities: null,
  renderingContext: null,
  animationFrameId: null,
  frameCount: 0,
  startTime: performance.now(),
  lastFrameTime: 0,
  averageFrameTime: 16.67, // Default to ~60 FPS

  // Function references
  externFunctions: null,
  wasmFunctions: null,
  processUiInput: null,

  // URL parameters passed from main thread
  urlParams: {},

  // Frame capture for main-thread recording (MediaRecorder runs on main thread)
  isCapturingFrames: false,
  frameCaptureInterval: 16.67, // Default 60 FPS
  lastFrameCaptureTime: 0,

  // Screenmap caching - updated via push notifications from C++ when screenmaps change
  // C++ calls js_notify_screenmap_update() → handleScreenMapUpdate() → cache update
  // Zero polling overhead - completely event-driven
  // Dictionary format: { "0": {strips: {...}, absMin: [...], absMax: [...]}, "1": {...} }
  screenMaps: {}
};

/**
 * Performance monitoring
 */
const performanceMonitor = {
  frameTimes: [],
  maxSamples: 60,
  lastStatsReport: 0,
  statsReportInterval: 1000 // Report every second

};

/**
 * Debug logging in worker context
 * @param {string} level - Log level (LOG, ERROR, TRACE)
 * @param {string} module - Module name
 * @param {string} message - Log message
 * @param {*} data - Additional data
 */
function workerLog(level, module, message, data = null) {
  const timestamp = (performance.now() - workerState.startTime).toFixed(1);
  const logData = {
    timestamp: `${timestamp}ms`,
    worker: true,
    level,
    module,
    message,
    ...(data && { data })
  };

  console[level.toLowerCase()](
    `[${timestamp}ms] [WORKER] [${level}] [${module}] ${message}`,
    data || ''
  );

  // Send log to main thread for centralized logging
  postMessage({
    type: 'debug_log',
    payload: logData
  });
}

/**
 * Worker message handler
 * @param {MessageEvent} event - Message from main thread
 */
self.onmessage = async function(event) {
  const { type, id, payload } = event.data;

  workerLog('TRACE', 'BACKGROUND_WORKER', 'Message received', { type, id });

  // Ignore messages without a type (possibly from browser devtools or other sources)
  if (!type) {
    workerLog('TRACE', 'BACKGROUND_WORKER', 'Ignoring message without type', event.data);
    return;
  }

  try {
    let response = null;

    switch (type) {
      case 'initialize':
        response = await handleInitialize(payload);
        break;

      case 'start':
        response = await handleStart(payload);
        break;

      case 'stop':
        response = await handleStop(payload);
        break;

      case 'ping':
        response = handlePing(payload);
        break;

      case 'update_frame_rate':
        response = handleUpdateFrameRate(payload);
        break;

      case 'get_performance_stats':
        response = getPerformanceStats();
        break;

      case 'ui_changes':
        response = handleUiChanges(payload);
        break;

      case 'start_recording':
        response = await handleStartRecording(payload);
        break;

      case 'stop_recording':
        response = await handleStopRecording(payload);
        break;

      case 'screenmap_update':
        console.log('[WORKER] Received screenmap_update message, payload:', payload);
        response = handleScreenMapUpdate(payload);
        console.log('[WORKER] handleScreenMapUpdate completed, response:', response);
        break;

      default:
        throw new Error(`Unknown message type: ${type}`);
    }

    // Send response back to main thread
    if (id && response !== null) {
      console.log('🔧 [WORKER] About to send response:', {
        type: `${type}_response`,
        id,
        hasPayload: !!response,
        success: true
      });
      postMessage({
        type: `${type}_response`,
        id,
        payload: response,
        success: true
      });
      console.log('🔧 [WORKER] Response sent for message:', id);
    } else {
      console.log('🔧 [WORKER] NOT sending response:', {
        hasId: !!id,
        hasResponse: response !== null,
        originalType: type
      });
    }

  } catch (error) {
    workerLog('ERROR', 'BACKGROUND_WORKER', 'Message handling error', {
      type,
      error: error.message,
      stack: error.stack
    });

    // Send error response
    if (id) {
      postMessage({
        type: `${type}_response`,
        id,
        payload: { error: error.message },
        success: false
      });
    }

    // Report error to main thread
    postMessage({
      type: 'error',
      payload: {
        source: 'message_handler',
        message: error.message,
        stack: error.stack,
        messageType: type
      }
    });
  }
};


/**
 * Handles worker initialization
 * @param {Object} payload - Initialization payload
 * @returns {Promise<Object>} Initialization result
 */
async function handleInitialize(payload) {
  workerLog('LOG', 'BACKGROUND_WORKER', 'Initializing worker...');

  try {
    // Store initialization parameters
    workerState.canvas = payload.canvas;
    workerState.capabilities = payload.capabilities;
    workerState.frameRate = payload.frameRate || 60;
    workerState.urlParams = payload.urlParams || {}; // Store URL parameters from main thread

    workerLog('LOG', 'BACKGROUND_WORKER', 'URL parameters received from main thread', workerState.urlParams);

    // Validate OffscreenCanvas
    if (!workerState.canvas || !(workerState.canvas instanceof OffscreenCanvas)) {
      throw new Error('Invalid OffscreenCanvas provided to worker');
    }

    // Note: WebGL context will be created by GraphicsManager

    // Load and initialize FastLED WASM module in worker
    await initializeFastLEDModule();

    // Set up graphics manager for OffscreenCanvas
    await initializeGraphicsManager();

    workerState.initialized = true;

    const result = {
      success: true,
      capabilities: workerState.capabilities,
      canvas: {
        width: workerState.canvas.width,
        height: workerState.canvas.height
      },
      contextType: 'webgl2'
    };

    workerLog('LOG', 'BACKGROUND_WORKER', 'Worker initialized successfully', result);

    // Notify main thread that worker is ready
    postMessage({
      type: 'worker_ready',
      payload: result
    });

    return result;

  } catch (error) {
    workerLog('ERROR', 'BACKGROUND_WORKER', 'Worker initialization failed', error);
    throw error;
  }
}

/**
 * Initializes the FastLED WASM module in worker context
 * @returns {Promise<void>}
 */
async function initializeFastLEDModule() {
  workerLog('LOG', 'BACKGROUND_WORKER', 'Loading FastLED WASM module...');

  try {
    // For module workers, we need to dynamically load the fastled.js script
    // Module workers don't support importScripts(), so we use dynamic import or script injection

    // Check if fastled is already available (from a previous initialization attempt)
    // @ts-ignore - fastled is loaded dynamically
    if (typeof self.fastled !== 'function') {
      // Load fastled.js by injecting it as a script tag in the worker global scope
      // Since workers don't have document, we'll use a workaround with importScripts wrapped in a non-module worker
      // OR we can use fetch + eval (less safe but works for trusted code)

      workerLog('LOG', 'BACKGROUND_WORKER', 'Dynamically loading fastled.js...');

      // Fetch the fastled.js script and evaluate it in the worker context
      // Worker is at /modules/core/fastled_background_worker.js, fastled.js is at /fastled.js
      // So we need to go up two levels: ../../fastled.js
      const fastledScriptPath = new URL('../../fastled.js', self.location.href).href;
      workerLog('LOG', 'BACKGROUND_WORKER', `Fetching: ${fastledScriptPath}`);

      const response = await fetch(fastledScriptPath);
      if (!response.ok) {
        throw new Error(`Failed to fetch fastled.js: ${response.status} ${response.statusText}`);
      }

      const scriptText = await response.text();
      workerLog('LOG', 'BACKGROUND_WORKER', `Fetched ${scriptText.length} bytes, evaluating...`);

      // Evaluate the script in the worker's global scope
      // This is safe because fastled.js is our own trusted code
      // Using Function constructor instead of eval to satisfy ESLint
      // The script defines 'var fastled = ...' so we need to return it and assign to self
      const scriptFunc = new Function(`${scriptText}\nreturn fastled;`);
      // @ts-ignore - fastled is dynamically loaded from Emscripten-generated code
      self.fastled = scriptFunc.call(self);

      workerLog('LOG', 'BACKGROUND_WORKER', 'fastled.js evaluated successfully');
    }

    // Verify fastled function is now available
    // @ts-ignore - fastled is loaded dynamically
    if (typeof self.fastled !== 'function') {
      throw new Error('FastLED module not available after dynamic load');
    }

    // Initialize the module with URL parameters passed through
    // @ts-ignore - fastled is loaded dynamically
    workerState.fastledModule = await self.fastled({
      canvas: workerState.canvas,
      // Fix pthread worker creation: Tell Emscripten to use fastled.js for pthread workers,
      // not the current worker script (fastled_background_worker.js)
      mainScriptUrlOrBlob: new URL('../../fastled.js', self.location.href).href,
      // Pass URL parameters to WASM module via environment
      // This allows C++ code to access them through getenv() or similar
      preRun: [(Module) => {
        // Set URL parameters as environment variables for C++ access
        if (workerState.urlParams) {
          for (const [key, value] of Object.entries(workerState.urlParams)) {
            workerLog('LOG', 'BACKGROUND_WORKER', `Setting URL param: ${key}=${value}`);
            // Store in Module for JavaScript access
            if (!Module.urlParams) {
              Module.urlParams = {};
            }
            Module.urlParams[key] = value;
          }
        }
      }],
      locateFile: (path) => {
        // Resolve WASM file paths relative to worker location
        // Worker is at /modules/core/, so WASM files need to go up two levels
        if (path.endsWith('.wasm')) {
          return `../../${path}`;
        }
        return path;
      }
    });

    // Verify essential functions are available
    const requiredFunctions = ['_extern_setup', '_extern_loop', '_main'];
    for (const funcName of requiredFunctions) {
      if (typeof workerState.fastledModule[funcName] !== 'function') {
        throw new Error(`Required WASM function ${funcName} not found`);
      }
    }

    workerLog('LOG', 'BACKGROUND_WORKER', 'FastLED WASM module loaded successfully');

  } catch (error) {
    workerLog('ERROR', 'BACKGROUND_WORKER', 'Failed to load FastLED WASM module', error);
    throw error;
  }
}

/**
 * Initializes graphics manager for OffscreenCanvas rendering
 * @returns {Promise<void>}
 */
async function initializeGraphicsManager() {
  workerLog('LOG', 'BACKGROUND_WORKER', 'Initializing graphics manager...');

  try {
    // Check URL parameters for graphics mode override
    const FORCE_FAST_RENDERER = workerState.urlParams['gfx'] === '0';
    const FORCE_THREEJS_RENDERER = workerState.urlParams['gfx'] === '1';

    workerLog('LOG', 'BACKGROUND_WORKER', 'Graphics mode selection', {
      gfxParam: workerState.urlParams['gfx'],
      FORCE_FAST_RENDERER,
      FORCE_THREEJS_RENDERER
    });

    if (FORCE_FAST_RENDERER) {
      // Use fast 2D renderer (explicitly requested)
      workerLog('LOG', 'BACKGROUND_WORKER', 'Using Fast GraphicsManager (2D) - forced by URL param gfx=0');

      const graphicsManagerModule = await import('../graphics/graphics_manager.js');
      workerState.graphicsManager = new graphicsManagerModule.GraphicsManager({
        canvas: workerState.canvas, // Pass OffscreenCanvas directly (not canvasId)
        usePixelatedRendering: true
      });
    } else {
      // Default to ThreeJS renderer (gfx=1) if no parameter specified
      const explicitlyRequested = FORCE_THREEJS_RENDERER ? 'gfx=1' : 'default (gfx=1)';
      workerLog('LOG', 'BACKGROUND_WORKER', `ThreeJS renderer (${explicitlyRequested}) - loading ThreeJS modules...`);

      const threeJsModules = await loadThreeJSModules();
      workerLog('LOG', 'BACKGROUND_WORKER', 'ThreeJS modules loaded, creating GraphicsManagerThreeJS...');

      const graphicsManagerModule = await import('../graphics/graphics_manager_threejs.js');
      workerState.graphicsManager = new graphicsManagerModule.GraphicsManagerThreeJS({
        canvas: workerState.canvas, // Pass OffscreenCanvas directly
        threeJsModules: threeJsModules
      });

      workerLog('LOG', 'BACKGROUND_WORKER', 'Beautiful 3D GraphicsManager (ThreeJS) initialized in worker mode');
    }

    // The graphics manager will handle WebGL setup internally
    workerLog('LOG', 'BACKGROUND_WORKER', 'Graphics manager initialized', {
      canvas: {
        width: workerState.canvas.width,
        height: workerState.canvas.height
      }
    });

    // Initialization order handling: If screenMaps arrived before graphics manager was ready,
    // push them now to ensure graphics manager has the data for first render
    if (Object.keys(workerState.screenMaps).length > 0 && workerState.graphicsManager.updateScreenMap) {
      workerState.graphicsManager.updateScreenMap(workerState.screenMaps);
      workerLog('LOG', 'BACKGROUND_WORKER', 'Pushed cached screenMaps to newly initialized graphics manager');
    }

  } catch (error) {
    workerLog('ERROR', 'BACKGROUND_WORKER', 'Failed to initialize graphics manager', error);
    throw error;
  }
}

/**
 * Handles animation start request
 * @param {Object} _payload - Start parameters (unused)
 * @returns {Promise<Object>} Start result
 */
async function handleStart(_payload) {
  workerLog('LOG', 'BACKGROUND_WORKER', 'Starting animation loop...');

  if (!workerState.initialized) {
    throw new Error('Worker not initialized');
  }

  if (workerState.running) {
    workerLog('LOG', 'BACKGROUND_WORKER', 'Animation already running');
    return { success: true, already_running: true };
  }

  try {
    // Bind synchronous extern functions if not already bound (Asyncify removed)
    if (!workerState.externFunctions) {
      const Module = workerState.fastledModule;
      workerState.externFunctions = {
        externSetup: Module.cwrap('extern_setup', 'number', []),
        externLoop: Module.cwrap('extern_loop', 'number', [])
      };
    }

    // Call FastLED setup function (synchronous - worker thread allows blocking)
    workerState.externFunctions.externSetup();
    workerLog('LOG', 'BACKGROUND_WORKER', 'FastLED setup completed');

    // Poll for screenmap data after setup (C++ setup() has registered screenmaps)
    // EM_JS push mechanism has linking issues, so we use polling instead
    try {
      const Module = workerState.fastledModule;

      // Bind getScreenMapData if not already bound
      if (!workerState.wasmFunctions) {
        workerState.wasmFunctions = {
          getFrameData: Module.cwrap('getFrameData', 'number', ['number']),
          getScreenMapData: Module.cwrap('getScreenMapData', 'number', ['number']),
          getStripPixelData: Module.cwrap('getStripPixelData', 'number', ['number', 'number']),
          freeFrameData: Module.cwrap('freeFrameData', null, ['number'])
        };
      }

      // Fetch screenmap data from C++
      const screenMapSizePtr = Module._malloc(4);
      const screenMapDataPtr = workerState.wasmFunctions.getScreenMapData(screenMapSizePtr);

      if (screenMapDataPtr !== 0) {
        const screenMapSize = Module.getValue(screenMapSizePtr, 'i32');
        const screenMapJson = Module.UTF8ToString(screenMapDataPtr, screenMapSize);
        const screenMapData = JSON.parse(screenMapJson);

        // Update worker state and notify graphics manager
        workerState.screenMaps = screenMapData;
        if (workerState.graphicsManager && workerState.graphicsManager.updateScreenMap) {
          workerState.graphicsManager.updateScreenMap(screenMapData);
          workerLog('LOG', 'BACKGROUND_WORKER', 'ScreenMaps fetched and sent to graphics manager', {
            screenMapCount: Object.keys(screenMapData).length
          });
        }

        // Free the allocated memory
        workerState.wasmFunctions.freeFrameData(screenMapDataPtr);
      } else {
        workerLog('WARN', 'BACKGROUND_WORKER', 'No screenmap data available after setup');
      }

      Module._free(screenMapSizePtr);
    } catch (error) {
      workerLog('ERROR', 'BACKGROUND_WORKER', 'Failed to fetch screenmap data', error);
      // Non-fatal - continue with animation
    }

    // Start animation loop
    workerState.running = true;
    workerState.startTime = performance.now();
    workerState.frameCount = 0;

    startAnimationLoop();

    const result = {
      success: true,
      frameRate: workerState.frameRate,
      startTime: workerState.startTime
    };

    postMessage({
      type: 'animation_started',
      payload: result
    });

    return result;

  } catch (error) {
    workerLog('ERROR', 'BACKGROUND_WORKER', 'Failed to start animation', error);
    workerState.running = false;
    throw error;
  }
}

/**
 * Handles animation stop request
 * @param {Object} _payload - Stop parameters (unused)
 * @returns {Object} Stop result
 */
function handleStop(_payload) {
  workerLog('LOG', 'BACKGROUND_WORKER', 'Stopping animation loop...');

  if (!workerState.running) {
    return { success: true, already_stopped: true };
  }

  workerState.running = false;

  if (workerState.animationFrameId) {
    cancelAnimationFrame(workerState.animationFrameId);
    workerState.animationFrameId = null;
  }

  const result = {
    success: true,
    totalFrames: workerState.frameCount,
    totalTime: performance.now() - workerState.startTime
  };

  postMessage({
    type: 'animation_stopped',
    payload: result
  });

  workerLog('LOG', 'BACKGROUND_WORKER', 'Animation loop stopped', result);
  return result;
}

/**
 * Handles ping request for connectivity testing
 * @param {Object} payload - Ping payload
 * @returns {Object} Pong response
 */
function handlePing(payload) {
  return {
    type: 'pong',
    timestamp: Date.now(),
    workerTime: performance.now(),
    originalTimestamp: payload.timestamp
  };
}

/**
 * Handles frame rate update
 * @param {Object} payload - Frame rate parameters
 * @returns {Object} Update result
 */
function handleUpdateFrameRate(payload) {
  const oldFrameRate = workerState.frameRate;
  workerState.frameRate = payload.frameRate || 60;

  workerLog('LOG', 'BACKGROUND_WORKER', 'Frame rate updated', {
    oldFrameRate,
    newFrameRate: workerState.frameRate
  });

  return {
    success: true,
    oldFrameRate,
    newFrameRate: workerState.frameRate
  };
}

/**
 * Handles UI changes from main thread and passes them to C++ WASM
 * @param {Object} payload - UI changes data
 * @returns {Object} Processing result
 */
function handleUiChanges(payload) {
  workerLog('LOG', 'BACKGROUND_WORKER', 'Processing UI changes from main thread', payload);

  try {
    const Module = workerState.fastledModule;

    if (!Module || !Module.cwrap) {
      throw new Error('WASM module not available for UI processing');
    }

    // Bind processUiInput if not already bound
    if (!workerState.processUiInput) {
      workerState.processUiInput = Module.cwrap('processUiInput', null, ['string']);
    }

    // Convert changes to JSON string and send to C++
    const jsonString = JSON.stringify(payload.changes);
    workerState.processUiInput(jsonString);

    workerLog('LOG', 'BACKGROUND_WORKER', 'UI changes processed successfully');

    return {
      success: true,
      changesProcessed: Object.keys(payload.changes || {}).length
    };

  } catch (error) {
    workerLog('ERROR', 'BACKGROUND_WORKER', 'Failed to process UI changes', error);
    throw error;
  }
}

/**
 * Captures current frame as ImageBitmap and sends to main thread for recording
 * Called after each render when recording is active
 * Uses zero-copy transferable ImageBitmap for efficient frame transfer
 */
async function captureAndTransferFrame() {
  if (!workerState.isCapturingFrames || !workerState.canvas) {
    return;
  }

  try {
    const now = performance.now();
    const elapsed = now - workerState.lastFrameCaptureTime;

    // Throttle based on configured FPS to avoid overwhelming main thread
    if (elapsed < workerState.frameCaptureInterval) {
      return;
    }

    // Create ImageBitmap from OffscreenCanvas (hardware-accelerated, async)
    const bitmap = await createImageBitmap(workerState.canvas);

    // Transfer to main thread (zero-copy transfer via transferables)
    postMessage({
      type: 'frame_update',
      payload: {
        bitmap: bitmap,
        timestamp: now,
        frameNumber: workerState.frameCount,
        width: workerState.canvas.width,
        height: workerState.canvas.height
      }
    }, /** @type {*} */ ([bitmap])); // Transfer bitmap ownership to main thread

    workerState.lastFrameCaptureTime = now;
  } catch (error) {
    workerLog('ERROR', 'BACKGROUND_WORKER', 'Frame capture failed', error);
    // Don't stop the loop on capture failure, just log the error
  }
}

/**
 * Handles start_recording message to begin frame capture for main-thread recording
 * @param {Object} payload - Recording configuration
 * @returns {Promise<Object>} Recording start confirmation
 */
async function handleStartRecording(payload) {
  workerLog('LOG', 'BACKGROUND_WORKER', 'Starting frame capture for main-thread recording', payload);

  try {
    // Check if canvas is available
    if (!workerState.canvas) {
      throw new Error('Canvas not available for frame capture');
    }

    // Check if already capturing
    if (workerState.isCapturingFrames) {
      throw new Error('Frame capture already in progress');
    }

    // Configure frame capture based on requested FPS
    const fps = payload.fps || 60;
    workerState.frameCaptureInterval = 1000 / fps;
    workerState.isCapturingFrames = true;
    workerState.lastFrameCaptureTime = performance.now();

    workerLog('LOG', 'BACKGROUND_WORKER', 'Frame capture enabled', {
      fps,
      interval: workerState.frameCaptureInterval
    });

    return {
      success: true,
      fps: fps,
      canvasDimensions: {
        width: workerState.canvas.width,
        height: workerState.canvas.height
      }
    };
  } catch (error) {
    workerLog('ERROR', 'BACKGROUND_WORKER', 'Failed to start frame capture', error);
    throw error;
  }
}

/**
 * Handles stop_recording message to disable frame capture
 * @param {Object} _payload - Unused payload
 * @returns {Promise<Object>} Stop confirmation
 */
async function handleStopRecording(_payload) {
  workerLog('LOG', 'BACKGROUND_WORKER', 'Stopping frame capture');

  try {
    // Disable frame capture
    workerState.isCapturingFrames = false;

    workerLog('LOG', 'BACKGROUND_WORKER', 'Frame capture stopped');

    return {
      success: true
    };
  } catch (error) {
    workerLog('ERROR', 'BACKGROUND_WORKER', 'Failed to stop frame capture', error);
    throw error;
  }
}

/**
 * Handles screenmap_update message from C++ via event system
 * Caches the screenmap data (dictionary format) to avoid per-frame fetching
 * @param {Object} payload - Screenmap update data (dictionary: stripId → screenmap)
 * @returns {Object} Update confirmation
 */
function handleScreenMapUpdate(payload) {
  workerLog('LOG', 'BACKGROUND_WORKER', 'Screenmap update received', payload);

  try {
    // Cache the screenmap data (dictionary format)
    // This is sent from C++ only when screenmaps change (strip added, layout updated)
    workerState.screenMaps = payload.screenMapData;

    workerLog('LOG', 'BACKGROUND_WORKER', 'Screenmap cache updated', {
      screenMapCount: Object.keys(workerState.screenMaps || {}).length
    });

    // NEW: Directly notify graphics manager (event-driven architecture)
    // Graphics manager will rebuild scene/recalculate dimensions on next frame
    if (workerState.graphicsManager && workerState.graphicsManager.updateScreenMap) {
      workerState.graphicsManager.updateScreenMap(payload.screenMapData);
      workerLog('LOG', 'BACKGROUND_WORKER', 'Graphics manager notified of screenMap update');
    } else {
      workerLog('WARN', 'BACKGROUND_WORKER', 'Graphics manager not ready, screenMap will be applied during initialization');
    }

    return {
      success: true,
      cached: true,
      graphicsManagerNotified: !!workerState.graphicsManager
    };
  } catch (error) {
    workerLog('ERROR', 'BACKGROUND_WORKER', 'Failed to update screenmap cache', error);
    throw error;
  }
}

/**
 * Starts the animation loop in worker context
 */
function startAnimationLoop() {
  const frameInterval = 1000 / workerState.frameRate;
  let lastTime = performance.now();

  function animationFrame(currentTime) {
    if (!workerState.running) {
      return;
    }

    const deltaTime = currentTime - lastTime;

    // Throttle frame rate
    if (deltaTime >= frameInterval) {
      executeFrameLoop(currentTime);
      lastTime = currentTime - (deltaTime % frameInterval);
    }

    // Schedule next frame
    workerState.animationFrameId = requestAnimationFrame(animationFrame);
  }

  // Start the loop
  workerState.animationFrameId = requestAnimationFrame(animationFrame);
  workerLog('LOG', 'BACKGROUND_WORKER', 'Animation loop started', {
    frameRate: workerState.frameRate,
    frameInterval
  });
}

/**
 * Executes a single frame of the animation loop
 * @param {number} currentTime - Current timestamp
 */
async function executeFrameLoop(currentTime) {
  const frameStartTime = performance.now();

  try {
    workerState.frameCount++;

    // Call FastLED loop function synchronously (Asyncify removed - worker thread allows blocking)
    if (!workerState.externFunctions) {
      const Module = workerState.fastledModule;
      workerState.externFunctions = {
        externSetup: Module.cwrap('extern_setup', 'number', []),
        externLoop: Module.cwrap('extern_loop', 'number', [])
      };
    }
    workerState.externFunctions.externLoop();

    // Get frame data from WASM
    const frameData = extractFrameData();

    if (frameData) {
      // Render frame to OffscreenCanvas (automatically syncs to main thread canvas)
      workerState.graphicsManager.updateCanvas(frameData);

      // Capture frame for main-thread recording if enabled
      if (workerState.isCapturingFrames) {
        captureAndTransferFrame().catch((err) => {
          workerLog('ERROR', 'BACKGROUND_WORKER', 'Frame capture error', err);
        });
      }

      // Send lightweight performance telemetry to main thread
      postMessage({
        type: 'frame_rendered',
        payload: {
          frameNumber: workerState.frameCount,
          timestamp: currentTime,
          frameTime: performance.now() - frameStartTime
        }
      });
    }

    // Update performance metrics
    const frameTime = performance.now() - frameStartTime;
    updatePerformanceMetrics(frameTime);

  } catch (error) {
    workerLog('ERROR', 'BACKGROUND_WORKER', 'Frame execution error', error);

    // Report error but don't stop the loop (for resilience)
    postMessage({
      type: 'error',
      payload: {
        source: 'frame_loop',
        message: error.message,
        frameNumber: workerState.frameCount
      }
    });
  }
}


/**
 * Extracts frame data from the WASM module
 * @returns {Object|null} Frame data object
 */
function extractFrameData() {
  try {
    const Module = workerState.fastledModule;

    // Get frame data from WASM using proper cwrap bindings
    if (!Module.cwrap) {
      workerLog('ERROR', 'BACKGROUND_WORKER', 'Module.cwrap not available');
      return null;
    }

    // Bind WASM functions if not already bound
    if (!workerState.wasmFunctions) {
      workerState.wasmFunctions = {
        getFrameData: Module.cwrap('getFrameData', 'number', ['number']),
        getScreenMapData: Module.cwrap('getScreenMapData', 'number', ['number']),
        getStripPixelData: Module.cwrap('getStripPixelData', 'number', ['number', 'number']),
        freeFrameData: Module.cwrap('freeFrameData', null, ['number'])
      };
    }

    const funcs = workerState.wasmFunctions;

    // Get frame data from C++
    const dataSizePtr = Module._malloc(4);
    const frameDataPtr = funcs.getFrameData(dataSizePtr);

    if (!frameDataPtr) {
      Module._free(dataSizePtr);
      return null;
    }

    const dataSize = Module.getValue(dataSizePtr, 'i32');
    const frameDataJson = Module.UTF8ToString(frameDataPtr, dataSize);
    const frameData = JSON.parse(frameDataJson);

    // Process each strip's pixel data
    // NOTE: screenMap is NOT attached to frameData - it's pushed directly to graphics manager
    // via handleScreenMapUpdate() when it changes (event-driven architecture)
    for (const stripData of frameData) {
      const pixelSizePtr = Module._malloc(4);
      const pixelDataPtr = funcs.getStripPixelData(stripData.strip_id, pixelSizePtr);

      if (pixelDataPtr) {
        const pixelSize = Module.getValue(pixelSizePtr, 'i32');
        // Copy pixel data from WASM heap
        const pixelData = new Uint8Array(Module.HEAPU8.buffer, pixelDataPtr, pixelSize);
        stripData.pixel_data = new Uint8Array(pixelData); // Make a copy
      } else {
        stripData.pixel_data = null;
      }

      Module._free(pixelSizePtr);
    }

    // Clean up
    funcs.freeFrameData(frameDataPtr);
    Module._free(dataSizePtr);

    return frameData;

  } catch (error) {
    workerLog('ERROR', 'BACKGROUND_WORKER', 'Failed to extract frame data', error);
    return null;
  }
}

// parseScreenMapData and parsePixelData functions removed -
// Data extraction is now handled directly in extractFrameData() using proper WASM bindings

/**
 * Updates performance monitoring metrics
 * @param {number} frameTime - Frame execution time in milliseconds
 */
function updatePerformanceMetrics(frameTime) {
  performanceMonitor.frameTimes.push(frameTime);

  // Keep only recent samples
  if (performanceMonitor.frameTimes.length > performanceMonitor.maxSamples) {
    performanceMonitor.frameTimes.shift();
  }

  // Calculate average frame time
  const sum = performanceMonitor.frameTimes.reduce((a, b) => a + b, 0);
  workerState.averageFrameTime = sum / performanceMonitor.frameTimes.length;

  // Report stats periodically
  const now = performance.now();
  if (now - performanceMonitor.lastStatsReport > performanceMonitor.statsReportInterval) {
    reportPerformanceStats();
    performanceMonitor.lastStatsReport = now;
  }
}

/**
 * Reports performance statistics to main thread
 */
function reportPerformanceStats() {
  const stats = getPerformanceStats();

  postMessage({
    type: 'performance_stats',
    payload: stats
  });
}

/**
 * Gets current performance statistics
 * @returns {Object} Performance statistics
 */
function getPerformanceStats() {
  const currentTime = performance.now();
  const runTime = currentTime - workerState.startTime;
  const fps = workerState.frameCount / (runTime / 1000);

  return {
    fps: fps || 0,
    averageFrameTime: workerState.averageFrameTime,
    frameCount: workerState.frameCount,
    runTime: runTime,
    memoryUsage: 'self' in globalThis && 'performance' in self && self.performance.memory ? {
      usedJSHeapSize: self.performance.memory.usedJSHeapSize,
      totalJSHeapSize: self.performance.memory.totalJSHeapSize,
      jsHeapSizeLimit: self.performance.memory.jsHeapSizeLimit
    } : null,
    isRunning: workerState.running,
    initialized: workerState.initialized
  };
}

/**
 * Handle worker errors
 * @param {ErrorEvent|string} error - Error event
 */
self.onerror = function(error) {
  const errorData = typeof error === 'string'
    ? { message: error, filename: '', lineno: 0, colno: 0 }
    : {
        message: error.message,
        filename: error.filename || '',
        lineno: error.lineno || 0,
        colno: error.colno || 0
      };

  workerLog('ERROR', 'BACKGROUND_WORKER', 'Worker error', errorData);

  postMessage({
    type: 'error',
    payload: {
      source: 'worker_error',
      message: errorData.message,
      filename: errorData.filename,
      lineno: errorData.lineno
    }
  });
};

/**
 * Handle unhandled promise rejections
 */
self.onunhandledrejection = function(event) {
  workerLog('ERROR', 'BACKGROUND_WORKER', 'Unhandled promise rejection', {
    reason: event.reason
  });

  postMessage({
    type: 'error',
    payload: {
      source: 'promise_rejection',
      message: event.reason?.message || 'Unhandled promise rejection',
      reason: event.reason
    }
  });
};

// Initialize worker
workerLog('LOG', 'BACKGROUND_WORKER', 'Background worker script loaded and ready');

postMessage({
  type: 'worker_script_loaded',
  payload: {
    timestamp: Date.now(),
    capabilities: {
      offscreenCanvas: typeof OffscreenCanvas !== 'undefined',
      webgl2: true, // Will be verified during initialization
      sharedArrayBuffer: typeof SharedArrayBuffer !== 'undefined'
    }
  }
});