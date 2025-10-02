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

// Worker-specific imports and initialization - Optimized loading
/** @type {function(string): void} */
// @ts-ignore
const importScripts = self.importScripts;
importScripts('../fastled.js'); // Import the WASM module

/* global fastled, postMessage, self, performance, requestAnimationFrame, cancelAnimationFrame */

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
 * @property {Int32Array|null} sharedFrameBuffer - Shared frame buffer
 * @property {Int32Array|null} sharedFrameHeader - Shared frame header
 * @property {Uint8ClampedArray|null} sharedPixelBuffer - Shared pixel buffer
 * @property {boolean} useSharedMemory - Whether shared memory is enabled
 * @property {number} maxPixelDataSize - Maximum pixel data size
 * @property {Object|null} externFunctions - External functions
 * @property {Object|null} wasmFunctions - WASM functions
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

  // SharedArrayBuffer support for zero-copy data transfer
  sharedFrameBuffer: null,
  sharedFrameHeader: null,
  sharedPixelBuffer: null,
  useSharedMemory: false,
  maxPixelDataSize: 0,

  // Function references
  externFunctions: null,
  wasmFunctions: null
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

      default:
        throw new Error(`Unknown message type: ${type}`);
    }

    // Send response back to main thread
    if (id && response !== null) {
      postMessage({
        type: `${type}_response`,
        id,
        payload: response,
        success: true
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
 * Initializes SharedArrayBuffer for zero-copy data transfer
 * @param {Object} payload - Initialization payload containing shared buffers
 * @returns {Promise<void>}
 */
async function initializeSharedMemory(payload) {
  try {
    // Check if SharedArrayBuffer is available and provided
    if (typeof SharedArrayBuffer === 'undefined') {
      workerLog('LOG', 'BACKGROUND_WORKER', 'SharedArrayBuffer not available, using standard transfer');
      return;
    }

    if (!payload.sharedBuffers) {
      workerLog('LOG', 'BACKGROUND_WORKER', 'No shared buffers provided, using standard transfer');
      return;
    }

    const { frameBuffer, pixelBuffer, maxPixelDataSize } = payload.sharedBuffers;

    if (frameBuffer instanceof SharedArrayBuffer && pixelBuffer instanceof SharedArrayBuffer) {
      workerState.sharedFrameBuffer = new Int32Array(frameBuffer);
      workerState.sharedPixelBuffer = new Uint8ClampedArray(pixelBuffer);
      workerState.maxPixelDataSize = maxPixelDataSize || 1024 * 1024; // Default 1MB
      workerState.useSharedMemory = true;

      // Create views for shared memory access
      // Frame header: [frameCount, pixelDataOffset, pixelDataLength, screenMapPresent, timestamp]
      workerState.sharedFrameHeader = new Int32Array(frameBuffer, 0, 8);

      workerLog('LOG', 'BACKGROUND_WORKER', 'SharedArrayBuffer initialized successfully', {
        frameBufferSize: frameBuffer.byteLength,
        pixelBufferSize: pixelBuffer.byteLength,
        maxPixelDataSize: workerState.maxPixelDataSize
      });
    }
  } catch (error) {
    workerLog('ERROR', 'BACKGROUND_WORKER', 'SharedArrayBuffer initialization failed', error);
    // Fall back to standard transfer
    workerState.useSharedMemory = false;
  }
}

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

    // Initialize SharedArrayBuffer support if available
    await initializeSharedMemory(payload);

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
    // Load the FastLED WASM module
    // Note: The module is loaded via importScripts at the top of this file
    // @ts-ignore - fastled is loaded globally via importScripts
    if (typeof self.fastled !== 'function') {
      throw new Error('FastLED module not available in worker context');
    }

    // Initialize the module
    // @ts-ignore - fastled is loaded globally via importScripts
    workerState.fastledModule = await self.fastled({
      canvas: workerState.canvas,
      locateFile: (path) => {
        // Resolve WASM file paths relative to worker location
        if (path.endsWith('.wasm')) {
          return `../${path}`;
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
    // Dynamically import GraphicsManager module for worker context
    const { GraphicsManager } = await import('./graphics_manager.js');

    // Create graphics manager with OffscreenCanvas
    workerState.graphicsManager = new GraphicsManager({
      canvasId: /** @type {string} */ (/** @type {*} */ (workerState.canvas)), // Pass OffscreenCanvas directly
      usePixelatedRendering: true
    });

    // The graphics manager will handle WebGL setup internally
    workerLog('LOG', 'BACKGROUND_WORKER', 'Graphics manager initialized', {
      canvas: {
        width: workerState.canvas.width,
        height: workerState.canvas.height
      }
    });

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
    // Bind async extern functions if not already bound
    if (!workerState.externFunctions) {
      const Module = workerState.fastledModule;
      workerState.externFunctions = {
        externSetup: Module.cwrap('extern_setup', 'number', [], { async: true }),
        externLoop: Module.cwrap('extern_loop', 'number', [], { async: true })
      };
    }

    // Call FastLED setup function
    await workerState.externFunctions.externSetup();
    workerLog('LOG', 'BACKGROUND_WORKER', 'FastLED setup completed');

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

    // Call FastLED loop function using the bound async function
    if (!workerState.externFunctions) {
      const Module = workerState.fastledModule;
      workerState.externFunctions = {
        externSetup: Module.cwrap('extern_setup', 'number', [], { async: true }),
        externLoop: Module.cwrap('extern_loop', 'number', [], { async: true })
      };
    }
    await workerState.externFunctions.externLoop();

    // Get frame data from WASM
    const frameData = extractFrameData();

    if (frameData) {
      // Render frame locally in worker
      workerState.graphicsManager.updateCanvas(frameData);

      // Transfer data to main thread using the most efficient method
      if (workerState.useSharedMemory) {
        // Use SharedArrayBuffer for zero-copy transfer
        transferFrameDataShared(frameData, currentTime);
      } else {
        // Use standard postMessage (fallback)
        transferFrameDataStandard(frameData, currentTime);
      }
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
 * Transfers frame data using SharedArrayBuffer (zero-copy)
 * @param {Object} frameData - Frame data from WASM
 * @param {number} timestamp - Current timestamp
 */
function transferFrameDataShared(frameData, timestamp) {
  try {
    if (!workerState.sharedFrameHeader || !workerState.sharedPixelBuffer) {
      // Fall back to standard transfer if shared memory is not available
      transferFrameDataStandard(frameData, timestamp);
      return;
    }

    // Calculate total pixel data size
    let totalPixelDataSize = 0;
    for (const strip of frameData) {
      if (strip.pixel_data) {
        totalPixelDataSize += strip.pixel_data.length;
      }
    }

    // Check if pixel data fits in shared buffer
    if (totalPixelDataSize > workerState.maxPixelDataSize) {
      workerLog('WARN', 'BACKGROUND_WORKER', `Pixel data too large for shared buffer: ${totalPixelDataSize} > ${workerState.maxPixelDataSize}`);
      transferFrameDataStandard(frameData, timestamp);
      return;
    }

    // Use atomic operations for synchronization
    const header = new Int32Array(workerState.sharedFrameHeader);

    // Atomic write sequence: set write flag first, then data, then commit
    Atomics.store(header, 7, 1); // Set write flag (reserved field becomes write flag)

    // Write frame header to shared memory
    Atomics.store(header, 0, workerState.frameCount); // frameNumber
    Atomics.store(header, 1, 0); // pixelDataOffset (will be updated)
    Atomics.store(header, 2, totalPixelDataSize); // pixelDataLength
    Atomics.store(header, 3, frameData.screenMap ? 1 : 0); // screenMapPresent
    Atomics.store(header, 4, Math.floor(timestamp)); // timestamp (lower 32 bits)
    Atomics.store(header, 5, Math.floor((timestamp * 1000) % 1000)); // timestamp fractional part
    Atomics.store(header, 6, frameData.length); // number of strips

    // Write pixel data to shared buffer
    const pixelView = new Uint8Array(workerState.sharedPixelBuffer);
    let pixelOffset = 0;

    for (let i = 0; i < frameData.length; i++) {
      const strip = frameData[i];
      if (strip.pixel_data && strip.pixel_data.length > 0) {
        // Copy pixel data to shared buffer
        pixelView.set(strip.pixel_data, pixelOffset);
        strip.sharedOffset = pixelOffset; // Store offset for main thread
        strip.sharedLength = strip.pixel_data.length;
        pixelOffset += strip.pixel_data.length;

        // Clear local pixel data to save memory (it's now in shared buffer)
        strip.pixel_data = null;
      } else {
        strip.sharedOffset = -1; // No pixel data
        strip.sharedLength = 0;
      }
    }

    // Write screen map to shared buffer if present (after pixel data)
    let screenMapOffset = -1;
    let screenMapLength = 0;
    if (frameData.screenMap) {
      const screenMapJson = JSON.stringify(frameData.screenMap);
      const screenMapBytes = new TextEncoder().encode(screenMapJson);

      if (pixelOffset + screenMapBytes.length <= workerState.maxPixelDataSize) {
        pixelView.set(screenMapBytes, pixelOffset);
        screenMapOffset = pixelOffset;
        screenMapLength = screenMapBytes.length;
        pixelOffset += screenMapBytes.length;
      }
    }

    // Update header with screen map info using atomic operations
    if (screenMapOffset >= 0) {
      // Store screen map info in header
      Atomics.store(header, 1, screenMapOffset); // pixelDataOffset now stores screenMap offset
    }

    // Atomic commit: clear write flag to signal data is ready
    Atomics.store(header, 7, 0); // Clear write flag to commit the frame

    // Notify main thread that new frame data is available in shared memory
    postMessage({
      type: 'frame_shared',
      payload: {
        frameNumber: workerState.frameCount,
        timestamp: timestamp,
        stripCount: frameData.length,
        totalPixelDataSize: totalPixelDataSize,
        screenMapOffset: screenMapOffset,
        screenMapLength: screenMapLength,
        sharedFrameHeader: workerState.sharedFrameHeader,
        sharedPixelBuffer: workerState.sharedPixelBuffer,
        frameDataStructure: frameData.map(strip => ({
          strip_id: strip.strip_id,
          sharedOffset: strip.sharedOffset,
          sharedLength: strip.sharedLength
        }))
      }
    });

  } catch (error) {
    workerLog('ERROR', 'BACKGROUND_WORKER', 'SharedArrayBuffer transfer failed', error);
    // Fall back to standard transfer
    transferFrameDataStandard(frameData, timestamp);
  }
}

/**
 * Transfers frame data using standard postMessage (with copying)
 * @param {Object} frameData - Frame data from WASM
 * @param {number} timestamp - Current timestamp
 */
function transferFrameDataStandard(frameData, timestamp) {
  // Standard postMessage approach (existing behavior)
  postMessage({
    type: 'frame_rendered',
    payload: {
      frameNumber: workerState.frameCount,
      timestamp: timestamp,
      hasScreenMap: !!frameData.screenMap,
      pixelCount: Array.isArray(frameData) ? frameData.length : 0
    }
  });
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

    // Get screen map data
    const screenMapSizePtr = Module._malloc(4);
    const screenMapPtr = funcs.getScreenMapData(screenMapSizePtr);
    let screenMap = null;

    if (screenMapPtr) {
      const screenMapSize = Module.getValue(screenMapSizePtr, 'i32');
      const screenMapJson = Module.UTF8ToString(screenMapPtr, screenMapSize);
      screenMap = JSON.parse(screenMapJson);
      funcs.freeFrameData(screenMapPtr);
    }
    Module._free(screenMapSizePtr);

    // Process each strip's pixel data
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

    // Add screenMap to frameData
    if (screenMap) {
      frameData.screenMap = screenMap;
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