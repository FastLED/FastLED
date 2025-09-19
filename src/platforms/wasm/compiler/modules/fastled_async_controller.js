/**
 * FastLED Pure JavaScript Async Controller
 *
 * This module implements a pure JavaScript async controller that handles all async logic,
 * frame processing, and WASM module integration. It replaces the embedded JavaScript
 * approach with a clean separation between C++ (data processing) and JavaScript (coordination).
 *
 * Key Features:
 * - Pure JavaScript async patterns with proper Asyncify integration
 * - Clean data export/import with C++ via Module.cwrap
 * - Event-driven architecture replacing callback chains
 * - Proper error handling and performance monitoring
 * - No embedded JavaScript - all async logic in JavaScript domain
 *
 * @module FastLEDAsyncController
 */

/**
 * @typedef {Object} FrameData
 * @property {number} strip_id - ID of the LED strip
 * @property {string} type - Type of frame data
 * @property {Uint8Array|number[]} pixel_data - Pixel color data
 * @property {Object} screenMap - Screen mapping data for LED positions
 */

// Import debug logging
import { FASTLED_DEBUG_LOG, FASTLED_DEBUG_ERROR, FASTLED_DEBUG_TRACE } from './fastled_debug_logger.js';

class FastLEDAsyncController {
  constructor(wasmModule, frameRate = 60) {
    FASTLED_DEBUG_TRACE('ASYNC_CONTROLLER', 'constructor', 'ENTER', { frameRate });

    FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Initializing controller properties...');
    this.module = wasmModule;
    this.frameRate = frameRate;
    this.running = false;
    this.frameCount = 0;
    this.frameTimes = [];
    this.maxFrameTimeHistory = 60;
    this.setupCompleted = false;
    this.fastledSetupCalled = false; // Track if extern_setup() has been called
    this.lastFrameTime = 0;
    this.frameInterval = 1000 / this.frameRate;

    FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Properties initialized', {
      hasModule: Boolean(this.module),
      frameRate: this.frameRate,
      frameInterval: this.frameInterval,
    });

    // Bind C++ functions via cwrap - no embedded JavaScript
    FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Binding C++ functions...');
    this.bindCppFunctions();

    // Bind methods to preserve 'this' context
    FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Binding JavaScript methods...');
    this.loop = this.loop.bind(this);
    this.stop = this.stop.bind(this);

    console.log('FastLED Pure JavaScript Async Controller initialized');
    FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'FastLED Pure JavaScript Async Controller initialized successfully');
    FASTLED_DEBUG_TRACE('ASYNC_CONTROLLER', 'constructor', 'EXIT');
  }

  /**
     * Bind C++ functions using Module.cwrap for pure data exchange
     */
  bindCppFunctions() {
    FASTLED_DEBUG_TRACE('ASYNC_CONTROLLER', 'bindCppFunctions', 'ENTER');

    if (!this.module) {
      FASTLED_DEBUG_ERROR('ASYNC_CONTROLLER', 'Module is null/undefined, cannot bind functions', null);
      throw new Error('WASM module is null or undefined');
    }

    if (!this.module.cwrap) {
      FASTLED_DEBUG_ERROR('ASYNC_CONTROLLER', 'Module.cwrap is not available', null);
      throw new Error('Module.cwrap is not available');
    }

    FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Binding frame data functions...');
    try {
      // Frame data functions
      this.getFrameData = this.module.cwrap('getFrameData', 'number', ['number']);
      this.getScreenMapData = this.module.cwrap('getScreenMapData', 'number', ['number']);
      this.freeFrameData = this.module.cwrap('freeFrameData', null, ['number']);
      FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Frame data functions bound successfully');
    } catch (error) {
      FASTLED_DEBUG_ERROR('ASYNC_CONTROLLER', 'Failed to bind frame data functions', error);
      throw error;
    }

    FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Binding strip functions...');
    try {
      // Strip functions
      this.getStripPixelData = this.module.cwrap('getStripPixelData', 'number', ['number', 'number']);
      this.notifyStripAdded = this.module.cwrap('notifyStripAdded', null, ['number', 'number']);
      FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Strip functions bound successfully');
    } catch (error) {
      FASTLED_DEBUG_ERROR('ASYNC_CONTROLLER', 'Failed to bind strip functions', error);
      throw error;
    }

    FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Binding UI functions...');
    try {
      // UI functions
      this.processUiInput = this.module.cwrap('processUiInput', null, ['string']);
      this.getUiUpdateData = this.module.cwrap('getUiUpdateData', 'number', ['number']);
      FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'UI functions bound successfully');
    } catch (error) {
      FASTLED_DEBUG_ERROR('ASYNC_CONTROLLER', 'Failed to bind UI functions', error);
      throw error;
    }

    // NOTE: externSetup and externLoop are now bound in setup() method with async flag
    // due to ASYNCIFY_EXPORTS requirement

    console.log('C++ functions bound successfully via cwrap');
    FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'C++ functions bound successfully via cwrap (async extern functions bound in setup)');

    // Verify all functions are available (extern functions will be bound in setup())
    const functionStatus = {
      getFrameData: typeof this.getFrameData,
      freeFrameData: typeof this.freeFrameData,
      getStripPixelData: typeof this.getStripPixelData,
      notifyStripAdded: typeof this.notifyStripAdded,
      processUiInput: typeof this.processUiInput,
      getUiUpdateData: typeof this.getUiUpdateData,
      // externSetup and externLoop will be bound in setup() method
    };

    FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Function binding verification', functionStatus);
    FASTLED_DEBUG_TRACE('ASYNC_CONTROLLER', 'bindCppFunctions', 'EXIT');
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
     * Setup function for initializing WASM module bindings
     * @returns {void} Synchronous setup since extern functions are now synchronous
     */
  setup() {
    try {
      FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Setting up FastLED Async Controller...');

      // In PROXY_TO_PTHREAD mode, main() runs automatically on a pthread
      // We only need to bind extern functions for JavaScript control
      console.log('🔄 PROXY_TO_PTHREAD MODE: main() runs automatically on pthread, binding extern functions only');

      // Bind extern functions as ASYNC - they are in ASYNCIFY_EXPORTS list
      this.externSetup = this.module.cwrap('extern_setup', 'number', [], { async: true });
      this.externLoop = this.module.cwrap('extern_loop', 'number', [], { async: true });
      this.getFrameData = this.module.cwrap('getFrameData', 'number', ['number']);
      this.getScreenMapData = this.module.cwrap('getScreenMapData', 'number', ['number']);
      this.getStripPixelData = this.module.cwrap('getStripPixelData', 'number', ['number', 'number']);
      this.freeFrameData = this.module.cwrap('freeFrameData', 'null', ['number']);

      FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'WASM functions bound successfully');

      // Test function availability
      if (!this.externSetup || !this.externLoop) {
        throw new Error('extern_setup() or extern_loop() functions not available - check WASM exports');
      }

      // In PROXY_TO_PTHREAD mode:
      // - main() is automatically called by Emscripten on a pthread
      // - Socket proxy functionality is handled automatically
      // - We control FastLED via extern_setup() and extern_loop() calls
      // - extern functions are async due to ASYNCIFY_EXPORTS

      console.log('✅ PROXY_TO_PTHREAD setup complete - main() pthread ready, async extern functions bound');
      FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'PROXY_TO_PTHREAD setup complete - ready for async extern function calls');

      // Mark setup as completed
      this.setupCompleted = true;

      FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Setup completed successfully');
    } catch (error) {
      FASTLED_DEBUG_ERROR('ASYNC_CONTROLLER', 'Setup failed', error);
      throw error;
    }
  }

  /**
     * Starts the async animation loop
     * Uses requestAnimationFrame for smooth, non-blocking animation
     * @returns {Promise<void>} Promise that resolves when start is complete
     */
  async start() {
    FASTLED_DEBUG_TRACE('ASYNC_CONTROLLER', 'start', 'ENTER');

    if (this.running) {
      console.warn('FastLED loop is already running');
      FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'FastLED loop is already running, ignoring start request');
      return;
    }

    // No need to check setupCompleted - setup is handled by the loop itself via extern_setup()

    console.log('Starting FastLED pure JavaScript async loop...');
    FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Starting FastLED pure JavaScript async loop...');

    this.running = true;
    this.frameCount = 0;
    this.lastFrameTime = performance.now();

    FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Loop state initialized', {
      running: this.running,
      frameCount: this.frameCount,
      lastFrameTime: this.lastFrameTime,
    });

    FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Requesting first animation frame...');
    requestAnimationFrame(this.loop);
    FASTLED_DEBUG_TRACE('ASYNC_CONTROLLER', 'start', 'EXIT');
  }

  /**
     * Main animation loop function - handles async extern_loop calls
     * @param {number} currentTime - Current timestamp from requestAnimationFrame
     */
  async loop(currentTime) {
    // Only log every 60 frames to avoid spam
    const shouldLog = this.frameCount % 60 === 0;

    if (shouldLog) {
      FASTLED_DEBUG_TRACE('ASYNC_CONTROLLER', 'loop', 'ENTER', { currentTime, frameCount: this.frameCount });
    }

    if (!this.running) {
      if (shouldLog) {
        FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Loop not running, returning');
      }
      return;
    }

    // Maintain consistent frame rate
    if (currentTime - this.lastFrameTime < this.frameInterval) {
      requestAnimationFrame(this.loop);
      return;
    }

    const frameStartTime = performance.now();

    if (shouldLog) {
      FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Processing frame', {
        frameCount: this.frameCount,
        timeSinceLastFrame: currentTime - this.lastFrameTime,
        frameInterval: this.frameInterval,
      });
    }

    try {
      // Call extern_setup() once if it hasn't been called yet
      if (!this.fastledSetupCalled) {
        if (shouldLog) {
          console.log('🔧 Calling extern_setup() for first-time initialization (async)');
          FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Calling extern_setup() for first-time initialization (async)...');
        }
        await this.externSetup(); // Now async call
        this.fastledSetupCalled = true;
        console.log('✅ extern_setup() completed - FastLED initialized');
        FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'extern_setup() completed successfully');
      }

      // Call extern_loop() to run one iteration of the FastLED loop
      if (shouldLog) {
        FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Calling extern_loop() to pump FastLED loop (async)...');
      }

      // Step 1: Call extern_loop() to run one FastLED loop iteration
      await this.externLoop(); // Now async call

      // Step 2: Process the resulting frame data
      await this.processFrame();

      this.frameCount++;
      this.lastFrameTime = currentTime;

      // Track performance
      const frameEndTime = performance.now();
      const frameTime = frameEndTime - frameStartTime;
      this.trackFramePerformance(frameTime);

      if (shouldLog) {
        FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Frame completed', {
          frameCount: this.frameCount,
          frameTime: `${frameTime.toFixed(2)}ms`,
          fps: this.getFPS().toFixed(1),
        });
      }

      // Schedule next frame
      this.scheduleNextFrame();
    } catch (error) {
      console.error('Fatal error in FastLED pure JavaScript async loop:', error);
      FASTLED_DEBUG_ERROR('ASYNC_CONTROLLER', 'Fatal error in loop', error);
      this.stop();
    }
  }

  /**
     * Process frame data using pure JavaScript data exchange
     * @returns {Promise<void>} Promise that resolves when frame is processed
     */
  async processFrame() {
    // Step 1: Get frame data from C++
    const dataSize = this.module._malloc(4);
    const frameDataPtr = this.getFrameData(dataSize);
    const size = this.module.getValue(dataSize, 'i32');

    let frameData = []; // Initialize as empty array in case of no data

    if (frameDataPtr !== 0) {
      try {
        // Step 2: Process frame data
        const jsonStr = this.module.UTF8ToString(frameDataPtr, size);
        frameData = JSON.parse(jsonStr);

        // Step 2.5: Get and attach screenMap data
        const screenMapSize = this.module._malloc(4);
        const screenMapPtr = this.getScreenMapData(screenMapSize);
        const screenMapSizeValue = this.module.getValue(screenMapSize, 'i32');

        if (screenMapPtr !== 0) {
          try {
            const screenMapJsonStr = this.module.UTF8ToString(screenMapPtr, screenMapSizeValue);
            const screenMapData = JSON.parse(screenMapJsonStr);

            // Attach screenMap as property to the array (JavaScript arrays can have properties)
            frameData.screenMap = screenMapData;
          } catch (screenMapError) {
            console.warn('Failed to parse screenMap data:', screenMapError);
          } finally {
            this.freeFrameData(screenMapPtr);
            this.module._free(screenMapSize);
          }
        }

        // Step 3: Add pixel data to frame
        await this.addPixelDataToFrame(frameData);

        // Step 4: Render frame
        await this.renderFrame(frameData);

        // Step 5: Process UI updates
        await this.processUiUpdates();
      } catch (error) {
        console.error('Error processing frame:', error);
      } finally {
        // Step 6: Clean up
        this.freeFrameData(frameDataPtr);
      }
    } else {
      // No frame data available - still process what we can
      console.warn('No frame data available from C++');

      // Try to render with empty data
      await this.renderFrame(frameData);

      // Process UI updates
      await this.processUiUpdates();
    }

    this.module._free(dataSize);
  }

  /**
     * Add pixel data to frame using pure JavaScript data exchange
     * @param {Array} frameData - Array of strip data objects
     * @returns {Promise<void>} Promise that resolves when pixel data is added
     */
  async addPixelDataToFrame(frameData) {
    for (const stripData of frameData) {
      const sizePtr = this.module._malloc(4);
      const dataPtr = this.getStripPixelData(stripData.strip_id, sizePtr);

      if (dataPtr !== 0) {
        const size = this.module.getValue(sizePtr, 'i32');
        const pixelData = new Uint8Array(this.module.HEAPU8.buffer, dataPtr, size);
        stripData.pixel_data = pixelData;
      } else {
        stripData.pixel_data = null;
      }

      this.module._free(sizePtr);
    }
  }

  /**
     * Render frame using user-defined callback
     * @param {FrameData|Array} frameData - Frame data with pixel information
     * @returns {Promise<void>} Promise that resolves when frame is rendered
     */
  async renderFrame(frameData) {
    // Call user-defined render function
    if (globalThis.FastLED_onFrame) {
      await globalThis.FastLED_onFrame(frameData);
    }
  }

  /**
     * Process UI updates using pure JavaScript data exchange
     * @returns {Promise<void>} Promise that resolves when UI updates are processed
     */
  async processUiUpdates() {
    // Get UI updates from user function
    if (globalThis.FastLED_processUiUpdates) {
      const uiJson = await globalThis.FastLED_processUiUpdates();
      if (uiJson && uiJson !== '[]' && uiJson !== '{}' && uiJson.trim() !== '') {
        // Only send non-empty, meaningful UI updates to C++
        // Skip empty arrays, empty objects, and blank strings
        this.processUiInput(uiJson);
      }
    }
  }

  /**
     * Stops the animation loop
     */
  stop() {
    if (!this.running) return;

    console.log('Stopping FastLED pure JavaScript async loop...');
    this.running = false;
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
     */
  scheduleNextFrame() {
    const avgFrameTime = this.getAverageFrameTime();

    if (avgFrameTime > 16) {
      // If we can't maintain 60fps, throttle to 30fps
      setTimeout(() => requestAnimationFrame(this.loop), 16);
    } else {
      requestAnimationFrame(this.loop);
    }
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
      setupCompleted: this.setupCompleted,
    };
  }

  /**
     * Handle strip addition events from C++
     * @param {number} stripId - Strip identifier
     * @param {number} numLeds - Number of LEDs in strip
     * @returns {Promise<void>} Promise that resolves when strip addition is handled
     */
  async handleStripAdded(stripId, numLeds) {
    if (globalThis.FastLED_onStripAdded) {
      await globalThis.FastLED_onStripAdded(stripId, numLeds);
    }
  }

  /**
     * Handle strip update events from C++
     * @param {Object} updateData - Strip update data
     * @returns {Promise<void>} Promise that resolves when strip update is handled
     */
  async handleStripUpdate(updateData) {
    if (globalThis.FastLED_onStripUpdate) {
      await globalThis.FastLED_onStripUpdate(updateData);
    }
  }

  /**
     * Handle UI element addition events from C++
     * @param {Object} uiData - UI element data
     * @returns {Promise<void>} Promise that resolves when UI elements are handled
     */
  async handleUiElementsAdded(uiData) {
    if (globalThis.FastLED_onUiElementsAdded) {
      await globalThis.FastLED_onUiElementsAdded(uiData);
    }
  }

  /**
   * Check if the controller is currently running
   * @returns {boolean} True if running, false otherwise
   */
  isRunning() {
    return this.running;
  }

  /**
   * Set the frame rate for the controller
   * @param {number} rate - Frame rate in frames per second
   */
  setFrameRate(rate) {
    if (typeof rate !== 'number' || rate <= 0) {
      FASTLED_DEBUG_ERROR('ASYNC_CONTROLLER', 'Invalid frame rate', { rate });
      return;
    }

    this.frameRate = rate;
    this.frameInterval = 1000 / this.frameRate;

    FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Frame rate updated', {
      frameRate: this.frameRate,
      frameInterval: this.frameInterval
    });
  }
}

export { FastLEDAsyncController };
