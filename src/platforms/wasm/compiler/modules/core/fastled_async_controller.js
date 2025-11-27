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

// Import worker manager for background worker integration
import { fastLEDWorkerManager } from './fastled_worker_manager.js';

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
    this.lastSlowFrameWarningTime = 0; // Track last slow frame warning timestamp

    // Worker integration properties
    this.isMainThread = typeof Worker !== 'undefined' && typeof window !== 'undefined';
    this.canvasElement = null; // Reference to canvas element for worker transfer

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

    // NOTE: externSetup and externLoop are now bound in setup() method as synchronous functions
    // Worker thread mode (PROXY_TO_PTHREAD) allows blocking calls without freezing the UI

    console.log('C++ functions bound successfully via cwrap');
    FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'C++ functions bound successfully via cwrap (synchronous extern functions bound in setup)');

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

      // Bind extern functions as synchronous (Asyncify removed - using pure worker thread mode)
      this.externSetup = this.module.cwrap('extern_setup', 'number', []);
      this.externLoop = this.module.cwrap('extern_loop', 'number', []);
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
      // - extern functions are synchronous (worker thread allows blocking calls without UI freeze)

      console.log('✅ PROXY_TO_PTHREAD setup complete - main() pthread ready, synchronous extern functions bound');
      FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'PROXY_TO_PTHREAD setup complete - ready for synchronous extern function calls');

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
     * Main animation loop function - handles synchronous extern_loop calls
     * @param {number} currentTime - Current timestamp from requestAnimationFrame
     */
  loop(currentTime) {
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

    // Maintain consistent frame rate with some tolerance for timing jitter
    const timeSinceLastFrame = currentTime - this.lastFrameTime;
    if (timeSinceLastFrame < this.frameInterval * 0.9) { // Allow 10% tolerance
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
          console.log('🔧 Calling extern_setup() for first-time initialization (synchronous)');
          FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Calling extern_setup() for first-time initialization (synchronous)...');
        }
        this.externSetup(); // Synchronous call - worker thread allows blocking
        this.fastledSetupCalled = true;
        console.log('✅ extern_setup() completed - FastLED initialized');
        FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'extern_setup() completed successfully');
      }

      // Call extern_loop() to run one iteration of the FastLED loop
      if (shouldLog) {
        FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Calling extern_loop() to pump FastLED loop (synchronous)...');
      }

      // Step 1: Call extern_loop() to run one FastLED loop iteration
      this.externLoop(); // Synchronous call - worker thread allows blocking

      // Step 2: Process the resulting frame data (may still be async for rendering)
      this.processFrame();

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
     * Main thread checks for UI changes and sends them to worker thread
     * @returns {Promise<void>} Promise that resolves when UI updates are processed
     */
  async processUiUpdates() {
    // Get UI updates from UI manager on main thread
    if (window.uiManager && typeof window.uiManager.processUiChanges === 'function') {
      const changes = window.uiManager.processUiChanges();

      if (changes && Object.keys(changes).length > 0) {
        //console.log('🎮 [UI_UPDATES] Detected UI changes:', changes);
        //console.log('🎮 [UI_UPDATES] Worker active:', fastLEDWorkerManager?.isWorkerActive);

        // Send UI changes to worker thread via worker manager
        if (fastLEDWorkerManager && fastLEDWorkerManager.isWorkerActive) {
          //console.log('🎮 [UI_UPDATES] Sending changes to worker');
          const message = {
            type: 'ui_changes',
            payload: {
              changes: changes
            }
          };

          // Send to worker (fire and forget, no response needed)
          fastLEDWorkerManager.worker.postMessage(message);
          //console.log('🎮 [UI_UPDATES] Changes sent to worker successfully');
        } else {
          //console.log('🎮 [UI_UPDATES] Using non-worker mode, calling C++ directly');
          // Fallback for non-worker mode: call C++ directly
          if (this.processUiInput) {
            const uiJson = JSON.stringify(changes);
            this.processUiInput(uiJson);
          }
        }
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

    // Log performance warnings for slow frames (max 1 per second)
    if (frameTime > 32) { // Slower than ~30 FPS
      const now = Date.now();
      if (now - this.lastSlowFrameWarningTime >= 1000) {
        console.warn(`Slow FastLED frame: ${frameTime.toFixed(2)}ms`);
        this.lastSlowFrameWarningTime = now;
      }
    }
  }

  /**
     * Schedules the next animation frame - always use RAF for consistent 60fps
     */
  scheduleNextFrame() {
    // Always use requestAnimationFrame for consistent browser-optimized timing
    // Let the browser handle frame timing instead of complex adaptive logic
    requestAnimationFrame(this.loop);
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

  /**
   * Initialize background worker mode for FastLED rendering
   * @param {HTMLCanvasElement} canvasElement - Canvas element to transfer to worker
   * @param {Object} config - Worker configuration options
   * @returns {Promise<boolean>} Success status of worker initialization
   */
  async initializeWorkerMode(canvasElement, config = {}) {
    FASTLED_DEBUG_TRACE('ASYNC_CONTROLLER', 'initializeWorkerMode', 'ENTER', { config });

    if (!this.isMainThread) {
      FASTLED_DEBUG_ERROR('ASYNC_CONTROLLER', 'Worker mode can only be initialized from main thread');
      return false;
    }

    try {
      this.canvasElement = canvasElement;

      // Configure worker initialization
      const workerConfig = {
        canvas: canvasElement,
        fastledModule: this.module,
        frameRate: this.frameRate,
        enableFallback: config.enableFallback !== false,
        maxRetries: config.maxRetries || 3
      };

      // Initialize worker manager
      console.log('🔧 [ITERATION_12] Calling fastLEDWorkerManager.initialize()...');
      const success = await fastLEDWorkerManager.initialize(workerConfig);
      console.log('🔧 [ITERATION_12] initialize() returned:', success);
      console.log('🔧 [ITERATION_12] fastLEDWorkerManager.isWorkerActive:', fastLEDWorkerManager.isWorkerActive);

      if (success) {
        // Set workerMode flag to indicate controller is using worker mode
        this.workerMode = true;

        FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Worker mode initialized', {
          workerActive: fastLEDWorkerManager.isWorkerActive,
          workerMode: this.workerMode,
          capabilities: fastLEDWorkerManager.capabilities
        });

        // Set up worker event listeners
        this.setupWorkerEventListeners();

        FASTLED_DEBUG_TRACE('ASYNC_CONTROLLER', 'initializeWorkerMode', 'EXIT', { success: true });
        return true;
      } else {
        throw new Error('Worker initialization failed');
      }

    } catch (error) {
      FASTLED_DEBUG_ERROR('ASYNC_CONTROLLER', 'Worker initialization error', error);
      FASTLED_DEBUG_TRACE('ASYNC_CONTROLLER', 'initializeWorkerMode', 'EXIT', { success: false });
      throw error; // Re-throw to propagate error
    }
  }

  /**
   * Set up event listeners for worker communication
   * @private
   */
  setupWorkerEventListeners() {
    FASTLED_DEBUG_TRACE('ASYNC_CONTROLLER', 'setupWorkerEventListeners', 'ENTER');

    // Listen for worker events through the fastLEDEvents system
    if (typeof window !== 'undefined' && window.fastLEDEvents) {
      window.fastLEDEvents.on('worker:initialized', (data) => {
        FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Worker initialized event', data);
      });

      window.fastLEDEvents.on('worker:failed', (data) => {
        FASTLED_DEBUG_ERROR('ASYNC_CONTROLLER', 'Worker failed event', data);
        // Worker manager handles automatic fallback
      });

      window.fastLEDEvents.on('frame:rendered', (data) => {
        if (data.source === 'background_worker') {
          // Frame was rendered in worker - update our frame count
          this.frameCount = data.frameNumber || this.frameCount + 1;
        }
      });

      window.fastLEDEvents.on('performance:stats', (data) => {
        if (data.source === 'background_worker') {
          // Update local performance data from worker
          this.updatePerformanceFromWorker(data);
        }
      });

      FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Worker event listeners set up');
    }

    FASTLED_DEBUG_TRACE('ASYNC_CONTROLLER', 'setupWorkerEventListeners', 'EXIT');
  }

  /**
   * Update local performance data with worker statistics
   * @param {Object} workerStats - Performance statistics from worker
   * @private
   */
  updatePerformanceFromWorker(workerStats) {
    // Merge worker performance data with local data
    if (workerStats.averageFrameTime) {
      // Add worker frame time to our tracking
      this.frameTimes.push(workerStats.averageFrameTime);

      // Keep within history limit
      if (this.frameTimes.length > this.maxFrameTimeHistory) {
        this.frameTimes.shift();
      }
    }
  }

  /**
   * Start animation loop in worker mode
   * @returns {Promise<void>} Promise that resolves when start is complete
   */
  async startWithWorkerSupport() {
    FASTLED_DEBUG_TRACE('ASYNC_CONTROLLER', 'startWithWorkerSupport', 'ENTER');

    if (fastLEDWorkerManager.isWorkerActive) {
      FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Starting animation in worker mode');

      // Send start command to worker
      const startMessage = {
        type: 'start',
        payload: {
          frameRate: this.frameRate
        }
      };

      const response = await fastLEDWorkerManager.sendMessageWithResponse(startMessage);

      if (response && response.success) {
        this.running = true;
        FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Worker animation started successfully');
      } else {
        throw new Error('Worker start command failed');
      }

      FASTLED_DEBUG_TRACE('ASYNC_CONTROLLER', 'startWithWorkerSupport', 'EXIT');
    } else {
      throw new Error('Worker not active - cannot start animation');
    }
  }

  /**
   * Stop animation loop in worker mode
   */
  stopWithWorkerSupport() {
    FASTLED_DEBUG_TRACE('ASYNC_CONTROLLER', 'stopWithWorkerSupport', 'ENTER');

    if (fastLEDWorkerManager.isWorkerActive) {
      FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Stopping animation in worker mode');

      // Send stop command to worker
      const stopMessage = {
        type: 'stop',
        payload: {}
      };

      fastLEDWorkerManager.sendMessageWithResponse(stopMessage)
        .then((response) => {
          if (response && response.success) {
            this.running = false;
            FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Worker animation stopped successfully');
          }
        })
        .catch((error) => {
          FASTLED_DEBUG_ERROR('ASYNC_CONTROLLER', 'Worker stop failed', error);
          // Force local stop
          this.running = false;
        });
    } else {
      throw new Error('Worker not active - cannot stop animation');
    }

    FASTLED_DEBUG_TRACE('ASYNC_CONTROLLER', 'stopWithWorkerSupport', 'EXIT');
  }

  /**
   * Get current mode information
   * @returns {Object} Mode information object
   */
  getModeInfo() {
    return {
      isMainThread: this.isMainThread,
      workerStatus: fastLEDWorkerManager.getStatus(),
      running: this.running,
      frameCount: this.frameCount
    };
  }

  /**
   * Clean up worker resources
   */
  destroyWorker() {
    FASTLED_DEBUG_TRACE('ASYNC_CONTROLLER', 'destroyWorker', 'ENTER');

    if (fastLEDWorkerManager) {
      fastLEDWorkerManager.destroy();
    }

    this.canvasElement = null;

    FASTLED_DEBUG_LOG('ASYNC_CONTROLLER', 'Worker resources cleaned up');
    FASTLED_DEBUG_TRACE('ASYNC_CONTROLLER', 'destroyWorker', 'EXIT');
  }
}

export { FastLEDAsyncController };
