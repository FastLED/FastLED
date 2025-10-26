/**
 * FastLED Worker Manager - Background Worker Lifecycle Management
 *
 * This module manages the lifecycle of background workers for FastLED WASM compilation
 * and rendering operations. It provides a clean API for transferring canvas control
 * to workers, managing worker communication, and handling fallback scenarios.
 *
 * Key responsibilities:
 * - Worker lifecycle management (creation, termination, error handling)
 * - OffscreenCanvas transfer and management
 * - Message passing between main thread and worker
 * - Feature detection and graceful fallback
 * - Performance monitoring and optimization
 *
 * @module FastLED/WorkerManager
 */

import { FASTLED_DEBUG_LOG, FASTLED_DEBUG_ERROR, FASTLED_DEBUG_TRACE } from './fastled_debug_logger.js';
import { fastLEDEvents } from './fastled_events.js';

/**
 * @typedef {Object} WorkerCapabilities
 * @property {boolean} offscreenCanvas - OffscreenCanvas support
 * @property {boolean} sharedArrayBuffer - SharedArrayBuffer support
 * @property {boolean} transferableObjects - Transferable objects support
 * @property {boolean} webgl2 - WebGL 2.0 support
 */

/**
 * @typedef {Object} WorkerConfiguration
 * @property {HTMLCanvasElement} canvas - Main thread canvas element
 * @property {Object} fastledModule - WASM module instance
 * @property {number} frameRate - Target frame rate
 * @property {boolean} enableFallback - Enable fallback to main thread
 * @property {number} maxRetries - Maximum retry attempts for worker operations
 */

/**
 * FastLED Worker Manager Class
 *
 * Manages background workers for FastLED rendering operations, providing
 * automatic feature detection, graceful fallbacks, and optimized data transfer.
 */
export class FastLEDWorkerManager {
  constructor() {
    FASTLED_DEBUG_TRACE('WORKER_MANAGER', 'constructor', 'ENTER');

    /** @type {Worker|null} Background worker instance */
    this.worker = null;

    /** @type {OffscreenCanvas|null} Transferred canvas control */
    this.offscreenCanvas = null;

    /** @type {HTMLCanvasElement|null} Original main thread canvas */
    this.mainCanvas = null;

    /** @type {WorkerCapabilities} Detected browser capabilities */
    this.capabilities = this.detectCapabilities();

    /** @type {boolean} Current worker operational status */
    this.isWorkerActive = false;

    /** @type {boolean} Whether fallback mode is active */
    this.isFallbackMode = false;

    /** @type {Map<string, Function>} Pending message callbacks */
    this.pendingCallbacks = new Map();

    /** @type {number} Message callback counter for unique IDs */
    this.messageIdCounter = 0;

    /** @type {number} Worker retry counter */
    this.retryCount = 0;

    /** @type {number} Maximum retry attempts */
    this.maxRetries = 3;

    /** @type {AbortController} For cancelling operations */
    this.abortController = new AbortController();

    FASTLED_DEBUG_LOG('WORKER_MANAGER', 'Worker Manager initialized', {
      capabilities: this.capabilities,
      maxRetries: this.maxRetries
    });

    FASTLED_DEBUG_TRACE('WORKER_MANAGER', 'constructor', 'EXIT');
  }

  /**
   * Detects browser capabilities for worker and OffscreenCanvas support
   * @returns {WorkerCapabilities} Detected capabilities
   */
  detectCapabilities() {
    FASTLED_DEBUG_TRACE('WORKER_MANAGER', 'detectCapabilities', 'ENTER');

    const capabilities = {
      offscreenCanvas: typeof OffscreenCanvas !== 'undefined',
      sharedArrayBuffer: typeof SharedArrayBuffer !== 'undefined',
      transferableObjects: (typeof window !== 'undefined' && 'postMessage' in window),
      webgl2: false
    };

    // Test WebGL 2.0 support with OffscreenCanvas
    if (capabilities.offscreenCanvas) {
      try {
        const testCanvas = new OffscreenCanvas(1, 1);
        const ctx = testCanvas.getContext('webgl2');
        capabilities.webgl2 = !!ctx;
        FASTLED_DEBUG_LOG('WORKER_MANAGER', 'WebGL2 OffscreenCanvas test successful');
      } catch (error) {
        FASTLED_DEBUG_ERROR('WORKER_MANAGER', 'WebGL2 OffscreenCanvas test failed', error);
        capabilities.webgl2 = false;
      }
    }

    FASTLED_DEBUG_LOG('WORKER_MANAGER', 'Capabilities detected', capabilities);
    FASTLED_DEBUG_TRACE('WORKER_MANAGER', 'detectCapabilities', 'EXIT');

    return capabilities;
  }

  /**
   * Initializes the background worker with OffscreenCanvas
   * @param {WorkerConfiguration} config - Worker configuration
   * @returns {Promise<boolean>} Success status
   */
  async initialize(config) {
    FASTLED_DEBUG_TRACE('WORKER_MANAGER', 'initialize', 'ENTER', { config });

    try {
      // Validate configuration
      if (!config.canvas || !config.fastledModule) {
        throw new Error('Invalid worker configuration: missing canvas or WASM module');
      }

      this.mainCanvas = config.canvas;
      this.maxRetries = config.maxRetries || 3;

      // Check if worker mode is supported and beneficial
      if (!this.shouldUseWorker()) {
        FASTLED_DEBUG_LOG('WORKER_MANAGER', 'Worker mode not supported, using fallback');
        return this.initializeFallback(config);
      }

      // Transfer canvas control to OffscreenCanvas
      this.offscreenCanvas = this.mainCanvas.transferControlToOffscreen();
      FASTLED_DEBUG_LOG('WORKER_MANAGER', `Canvas control transferred to OffscreenCanvas`);

      // Create and configure worker
      await this.createWorker(config);

      // Verify worker is operational
      const isWorkerReady = await this.verifyWorkerReady();
      if (!isWorkerReady) {
        throw new Error('Worker failed readiness verification');
      }

      this.isWorkerActive = true;
      this.isFallbackMode = false;

      // Emit success event
      fastLEDEvents.emit('worker:initialized', {
        mode: 'background_worker',
        capabilities: this.capabilities,
        canvas: { width: this.offscreenCanvas.width, height: this.offscreenCanvas.height }
      });

      FASTLED_DEBUG_LOG('WORKER_MANAGER', 'Background worker initialized successfully');
      FASTLED_DEBUG_TRACE('WORKER_MANAGER', 'initialize', 'EXIT', { success: true });

      return true;

    } catch (error) {
      FASTLED_DEBUG_ERROR('WORKER_MANAGER', 'Worker initialization failed', error);

      // Attempt fallback if enabled
      if (config.enableFallback !== false) {
        FASTLED_DEBUG_LOG('WORKER_MANAGER', 'Attempting fallback initialization');
        return this.initializeFallback(config);
      }

      FASTLED_DEBUG_TRACE('WORKER_MANAGER', 'initialize', 'EXIT', { success: false });
      return false;
    }
  }

  /**
   * Determines if background worker should be used based on capabilities
   * @returns {boolean} Whether worker mode should be used
   */
  shouldUseWorker() {
    // Require OffscreenCanvas and WebGL2 for worker mode
    const required = this.capabilities.offscreenCanvas && this.capabilities.webgl2;

    // Check for user preference or feature flags
    const userPreference = new URLSearchParams(window.location.search).get('worker');
    if (userPreference === '0' || userPreference === 'false') {
      FASTLED_DEBUG_LOG('WORKER_MANAGER', 'Worker mode disabled by user preference');
      return false;
    }

    // Check for compile-time feature flags
    if (typeof window !== 'undefined' && /** @type {*} */(window).FASTLED_ENABLE_BACKGROUND_WORKER === false) {
      FASTLED_DEBUG_LOG('WORKER_MANAGER', 'Worker mode disabled by compile flag');
      return false;
    }

    FASTLED_DEBUG_LOG('WORKER_MANAGER', 'Worker mode decision', {
      required,
      userPreference,
      shouldUse: required
    });

    return required;
  }

  /**
   * Creates and configures the background worker
   * @param {WorkerConfiguration} config - Worker configuration
   * @returns {Promise<void>}
   */
  async createWorker(config) {
    FASTLED_DEBUG_TRACE('WORKER_MANAGER', 'createWorker', 'ENTER');

    try {
      // Create worker with the background worker script
      const workerScript = new URL('./fastled_background_worker.js', import.meta.url);
      this.worker = new Worker(workerScript, {
        type: 'module',
        name: 'fastled-background-worker'
      });

      // Set up worker message handling
      this.worker.onmessage = this.handleWorkerMessage.bind(this);
      this.worker.onerror = this.handleWorkerError.bind(this);
      this.worker.onmessageerror = this.handleWorkerMessageError.bind(this);

      // Initialize worker with configuration
      const initMessage = {
        type: 'initialize',
        id: this.generateMessageId(),
        payload: {
          canvas: this.offscreenCanvas,
          capabilities: this.capabilities,
          frameRate: config.frameRate,
          wasmModuleConfig: {
            // Extract relevant WASM configuration for worker
            // Note: Full module transfer happens separately
          }
        }
      };

      const success = await this.sendMessageWithResponse(initMessage, [this.offscreenCanvas]);
      if (!success) {
        throw new Error('Worker initialization message failed');
      }

      FASTLED_DEBUG_LOG('WORKER_MANAGER', 'Worker created and configured successfully');
      FASTLED_DEBUG_TRACE('WORKER_MANAGER', 'createWorker', 'EXIT');

    } catch (error) {
      FASTLED_DEBUG_ERROR('WORKER_MANAGER', 'Failed to create worker', error);
      this.terminateWorker();
      throw error;
    }
  }

  /**
   * Verifies that the worker is ready for operation
   * @returns {Promise<boolean>} Worker readiness status
   */
  async verifyWorkerReady() {
    FASTLED_DEBUG_TRACE('WORKER_MANAGER', 'verifyWorkerReady', 'ENTER');

    try {
      const pingMessage = {
        type: 'ping',
        id: this.generateMessageId(),
        payload: { timestamp: Date.now() }
      };

      const startTime = performance.now();
      const response = await this.sendMessageWithResponse(pingMessage, null, 5000); // 5 second timeout
      const roundTripTime = performance.now() - startTime;

      if (response && response.type === 'pong') {
        FASTLED_DEBUG_LOG('WORKER_MANAGER', 'Worker ready', {
          roundTripTime: `${roundTripTime.toFixed(2)}ms`,
          workerTimestamp: response.payload?.timestamp
        });
        FASTLED_DEBUG_TRACE('WORKER_MANAGER', 'verifyWorkerReady', 'EXIT', { ready: true });
        return true;
      }

      FASTLED_DEBUG_ERROR('WORKER_MANAGER', 'Worker failed readiness check', { response });
      FASTLED_DEBUG_TRACE('WORKER_MANAGER', 'verifyWorkerReady', 'EXIT', { ready: false });
      return false;

    } catch (error) {
      FASTLED_DEBUG_ERROR('WORKER_MANAGER', 'Worker readiness verification failed', error);
      FASTLED_DEBUG_TRACE('WORKER_MANAGER', 'verifyWorkerReady', 'EXIT', { ready: false });
      return false;
    }
  }

  /**
   * Initializes fallback mode using main thread rendering
   * @param {WorkerConfiguration} _config - Configuration for fallback (unused)
   * @returns {Promise<boolean>} Success status
   */
  async initializeFallback(_config) {
    FASTLED_DEBUG_TRACE('WORKER_MANAGER', 'initializeFallback', 'ENTER');

    try {
      this.isFallbackMode = true;
      this.isWorkerActive = false;

      // Clean up any existing worker resources
      this.terminateWorker();

      // Emit fallback mode event
      fastLEDEvents.emit('worker:fallback', {
        mode: 'main_thread',
        reason: 'worker_unavailable',
        capabilities: this.capabilities
      });

      FASTLED_DEBUG_LOG('WORKER_MANAGER', 'Fallback mode initialized successfully');
      FASTLED_DEBUG_TRACE('WORKER_MANAGER', 'initializeFallback', 'EXIT', { success: true });

      return true;

    } catch (error) {
      FASTLED_DEBUG_ERROR('WORKER_MANAGER', 'Fallback initialization failed', error);
      FASTLED_DEBUG_TRACE('WORKER_MANAGER', 'initializeFallback', 'EXIT', { success: false });
      return false;
    }
  }

  /**
   * Sends a message to the worker and waits for response
   * @param {Object} message - Message to send
   * @param {Transferable[]|null} transferables - Transferable objects
   * @param {number} timeout - Timeout in milliseconds
   * @returns {Promise<Object>} Worker response
   */
  async sendMessageWithResponse(message, transferables = null, timeout = 10000) {
    FASTLED_DEBUG_TRACE('WORKER_MANAGER', 'sendMessageWithResponse', 'ENTER', { messageType: message.type });

    if (!this.worker || !this.isWorkerActive) {
      throw new Error('Worker not available for message sending');
    }

    return new Promise((resolve, reject) => {
      const messageId = message.id || this.generateMessageId();
      message.id = messageId;

      // Set up timeout
      const timeoutId = setTimeout(() => {
        this.pendingCallbacks.delete(messageId);
        reject(new Error(`Worker message timeout after ${timeout}ms`));
      }, timeout);

      // Store callback for response
      this.pendingCallbacks.set(messageId, (response) => {
        clearTimeout(timeoutId);
        this.pendingCallbacks.delete(messageId);
        resolve(response);
      });

      try {
        // Send message to worker
        if (transferables && transferables.length > 0) {
          this.worker.postMessage(message, transferables);
          FASTLED_DEBUG_LOG('WORKER_MANAGER', 'Message sent with transferables', {
            type: message.type,
            id: messageId,
            transferables: transferables.length
          });
        } else {
          this.worker.postMessage(message);
          FASTLED_DEBUG_LOG('WORKER_MANAGER', 'Message sent', {
            type: message.type,
            id: messageId
          });
        }
      } catch (error) {
        clearTimeout(timeoutId);
        this.pendingCallbacks.delete(messageId);
        reject(error);
      }
    });
  }

  /**
   * Handles messages received from the worker
   * @param {MessageEvent} event - Worker message event
   */
  handleWorkerMessage(event) {
    const { data } = event;
    FASTLED_DEBUG_TRACE('WORKER_MANAGER', 'handleWorkerMessage', 'ENTER', { messageType: data.type });

    try {
      // Handle response to pending callback
      if (data.id && this.pendingCallbacks.has(data.id)) {
        const callback = this.pendingCallbacks.get(data.id);
        callback(data.payload || data);
        return;
      }

      // Check for response-type messages (with _response suffix)
      if (data.type && data.type.endsWith('_response') && data.id) {
        // Try to find callback by ID even if not exact match
        if (this.pendingCallbacks.has(data.id)) {
          const callback = this.pendingCallbacks.get(data.id);
          callback(data.payload || data);
          return;
        }
      }

      // Handle worker events and notifications
      switch (data.type) {
        case 'frame_rendered':
          this.handleFrameRendered(data.payload);
          break;

        case 'frame_imageBitmap':
          this.handleFrameImageBitmap(data.payload);
          break;

        case 'frame_shared':
          this.handleFrameShared(data.payload);
          break;

        case 'error':
          this.handleWorkerErrorMessage(data.payload);
          break;

        case 'performance_stats':
          this.handlePerformanceStats(data.payload);
          break;

        case 'worker_ready':
        case 'worker_script_loaded':
          FASTLED_DEBUG_LOG('WORKER_MANAGER', 'Worker reports ready status', data.payload);
          break;

        case 'debug_log':
          // Forward debug logs from worker
          if (data.payload && data.payload.level && data.payload.message) {
            FASTLED_DEBUG_LOG('WORKER', data.payload.message, data.payload.data);
          }
          break;

        case 'animation_started':
        case 'animation_stopped':
          FASTLED_DEBUG_LOG('WORKER_MANAGER', `Worker animation ${data.type.replace('animation_', '')}`, data.payload);
          break;

        default:
          // Don't log for known response types
          if (!data.type.endsWith('_response')) {
            FASTLED_DEBUG_LOG('WORKER_MANAGER', 'Unhandled worker message', { type: data.type });
          }
      }

    } catch (error) {
      FASTLED_DEBUG_ERROR('WORKER_MANAGER', 'Error handling worker message', error);
    }

    FASTLED_DEBUG_TRACE('WORKER_MANAGER', 'handleWorkerMessage', 'EXIT');
  }

  /**
   * Handles frame rendering notifications from worker
   * @param {Object} payload - Frame render data
   */
  handleFrameRendered(payload) {
    // Emit event for frame rendering
    fastLEDEvents.emit('frame:rendered', {
      source: 'background_worker',
      ...payload
    });
  }

  /**
   * Handles SharedArrayBuffer frame transfers from worker
   * @param {Object} payload - Frame shared memory references
   */
  handleFrameShared(payload) {
    try {
      if (!payload.sharedFrameHeader || !payload.sharedPixelBuffer) {
        FASTLED_DEBUG_ERROR('WORKER_MANAGER', 'Invalid shared frame payload - missing buffers');
        return;
      }

      // Read frame header from shared memory using atomic operations
      const headerView = new Int32Array(payload.sharedFrameHeader);

      // Check if data is ready (write flag should be 0)
      const writeFlag = Atomics.load(headerView, 7);
      if (writeFlag !== 0) {
        // Data is still being written, skip this frame
        FASTLED_DEBUG_LOG('WORKER_MANAGER', 'Frame data not ready, skipping');
        return;
      }

      // Atomically read frame information
      const frameInfo = {
        frameNumber: Atomics.load(headerView, 0),
        screenMapOffset: Atomics.load(headerView, 1),
        pixelDataSize: Atomics.load(headerView, 2),
        screenMapPresent: Atomics.load(headerView, 3),
        timestampLow: Atomics.load(headerView, 4),
        timestampFrac: Atomics.load(headerView, 5),
        numStrips: Atomics.load(headerView, 6)
      };

      // Reconstruct timestamp
      frameInfo.timestamp = frameInfo.timestampLow + (frameInfo.timestampFrac / 1000);

      // Read pixel data from shared memory
      const pixelView = new Uint8Array(payload.sharedPixelBuffer);

      // Reconstruct frame data structure using payload information
      const frameData = {
        numStrips: frameInfo.numStrips,
        strips: [],
        timestamp: frameInfo.timestamp,
        frameNumber: frameInfo.frameNumber
      };

      // Reconstruct strips using frameDataStructure from payload
      if (payload.frameDataStructure) {
        for (const stripInfo of payload.frameDataStructure) {
          if (stripInfo.sharedOffset >= 0 && stripInfo.sharedLength > 0) {
            // Read pixel data from shared buffer at the specified offset
            const stripPixelData = new Uint8Array(pixelView.buffer,
                                                  stripInfo.sharedOffset,
                                                  stripInfo.sharedLength);

            // Convert raw pixel data to pixel objects (assuming RGB format)
            const pixels = [];
            for (let i = 0; i < stripPixelData.length; i += 3) {
              if (i + 2 < stripPixelData.length) {
                pixels.push({
                  r: stripPixelData[i],
                  g: stripPixelData[i + 1],
                  b: stripPixelData[i + 2]
                });
              }
            }

            frameData.strips.push({
              stripId: stripInfo.strip_id,
              numPixels: pixels.length,
              pixels: pixels,
              pixel_data: stripPixelData // Keep reference to shared data
            });
          }
        }
      }

      // Read screen map if present
      if (frameInfo.screenMapPresent && frameInfo.screenMapOffset >= 0 && payload.screenMapLength > 0) {
        try {
          const screenMapData = new Uint8Array(pixelView.buffer,
                                               frameInfo.screenMapOffset,
                                               payload.screenMapLength);
          const screenMapJson = new TextDecoder().decode(screenMapData);
          frameData.screenMap = JSON.parse(screenMapJson);
        } catch (error) {
          FASTLED_DEBUG_ERROR('WORKER_MANAGER', 'Failed to parse screen map from shared memory', error);
        }
      }

      // Emit frame data event for graphics managers
      fastLEDEvents.emit('frame:data', {
        source: 'background_worker_shared',
        frameData,
        timestamp: frameInfo.timestamp
      });

    } catch (error) {
      FASTLED_DEBUG_ERROR('WORKER_MANAGER', 'Failed to process shared frame data', error);
    }
  }

  /**
   * Handles ImageBitmap frame transfers from worker
   * @param {Object} payload - Frame ImageBitmap data
   */
  handleFrameImageBitmap(payload) {
    // Display the ImageBitmap on the main canvas
    if (payload.imageBitmap && this.mainCanvas) {
      try {
        const ctx = this.mainCanvas.getContext('2d');
        if (ctx) {
          // Draw ImageBitmap to main canvas
          ctx.drawImage(payload.imageBitmap, 0, 0);

          // Close ImageBitmap to free memory
          if (payload.imageBitmap.close) {
            payload.imageBitmap.close();
          }
        }
      } catch (error) {
        FASTLED_DEBUG_ERROR('WORKER_MANAGER', 'Failed to render ImageBitmap', error);
      }
    }

    // Emit event for frame display
    fastLEDEvents.emit('frame:displayed', {
      source: 'background_worker',
      timestamp: payload.timestamp,
      width: payload.width,
      height: payload.height
    });
  }

  /**
   * Handles performance statistics from worker
   * @param {Object} payload - Performance data
   */
  handlePerformanceStats(payload) {
    // Emit performance data for monitoring
    fastLEDEvents.emit('performance:stats', {
      source: 'background_worker',
      ...payload
    });
  }

  /**
   * Handles worker errors
   * @param {ErrorEvent} event - Worker error event
   */
  handleWorkerError(event) {
    FASTLED_DEBUG_ERROR('WORKER_MANAGER', 'Worker error', {
      message: event.message,
      filename: event.filename,
      lineno: event.lineno,
      colno: event.colno,
      error: event.error
    });

    // Attempt recovery or fallback
    this.handleWorkerFailure('worker_error', event.error);
  }

  /**
   * Handles worker message errors
   * @param {MessageEvent} event - Message error event
   */
  handleWorkerMessageError(event) {
    FASTLED_DEBUG_ERROR('WORKER_MANAGER', 'Worker message error', event.data);
    this.handleWorkerFailure('message_error', new Error('Worker message error'));
  }

  /**
   * Handles error messages from worker
   * @param {Object} payload - Error payload
   */
  handleWorkerErrorMessage(payload) {
    FASTLED_DEBUG_ERROR('WORKER_MANAGER', 'Worker reported error', payload);
    this.handleWorkerFailure('worker_reported_error', new Error(payload.message));
  }

  /**
   * Handles worker failure and attempts recovery
   * @param {string} reason - Failure reason
   * @param {Error} error - Error object
   */
  async handleWorkerFailure(reason, error) {
    FASTLED_DEBUG_ERROR('WORKER_MANAGER', 'Worker failure detected', { reason, error });

    this.isWorkerActive = false;

    // Emit failure event
    fastLEDEvents.emit('worker:failed', {
      reason,
      error: error.message,
      retryCount: this.retryCount
    });

    // Attempt recovery if retries remain
    if (this.retryCount < this.maxRetries) {
      this.retryCount++;
      FASTLED_DEBUG_LOG('WORKER_MANAGER', 'Attempting worker recovery', { attempt: this.retryCount });

      try {
        // Terminate failed worker
        this.terminateWorker();

        // Wait before retry
        await new Promise((resolve) => {
          setTimeout(() => resolve(), 1000 * this.retryCount);
        });

        // Attempt to recreate worker (would need original config - store it)
        // For now, fall back to main thread mode
        await this.initializeFallback(/** @type {WorkerConfiguration} */({}));

      } catch (recoveryError) {
        FASTLED_DEBUG_ERROR('WORKER_MANAGER', 'Worker recovery failed', recoveryError);
      }
    } else {
      FASTLED_DEBUG_LOG('WORKER_MANAGER', 'Max retries exceeded, falling back to main thread');
      await this.initializeFallback(/** @type {WorkerConfiguration} */({}));
    }
  }

  /**
   * Terminates the background worker and cleans up resources
   */
  terminateWorker() {
    FASTLED_DEBUG_TRACE('WORKER_MANAGER', 'terminateWorker', 'ENTER');

    if (this.worker) {
      try {
        this.worker.terminate();
        FASTLED_DEBUG_LOG('WORKER_MANAGER', 'Worker terminated');
      } catch (error) {
        FASTLED_DEBUG_ERROR('WORKER_MANAGER', 'Error terminating worker', error);
      }
      this.worker = null;
    }

    this.isWorkerActive = false;
    this.offscreenCanvas = null;
    this.pendingCallbacks.clear();
    this.abortController.abort();

    FASTLED_DEBUG_TRACE('WORKER_MANAGER', 'terminateWorker', 'EXIT');
  }

  /**
   * Generates a unique message ID
   * @returns {string} Unique message ID
   */
  generateMessageId() {
    return `msg_${Date.now()}_${++this.messageIdCounter}`;
  }

  /**
   * Cleans up worker manager resources
   */
  destroy() {
    FASTLED_DEBUG_TRACE('WORKER_MANAGER', 'destroy', 'ENTER');

    this.terminateWorker();
    this.mainCanvas = null;

    fastLEDEvents.emit('worker:destroyed', {
      mode: this.isFallbackMode ? 'fallback' : 'worker'
    });

    FASTLED_DEBUG_LOG('WORKER_MANAGER', 'Worker manager destroyed');
    FASTLED_DEBUG_TRACE('WORKER_MANAGER', 'destroy', 'EXIT');
  }

  /**
   * Gets current worker status and statistics
   * @returns {Object} Worker status information
   */
  getStatus() {
    return {
      isWorkerActive: this.isWorkerActive,
      isFallbackMode: this.isFallbackMode,
      capabilities: this.capabilities,
      retryCount: this.retryCount,
      pendingCallbacks: this.pendingCallbacks.size,
      hasCanvas: !!this.offscreenCanvas || !!this.mainCanvas
    };
  }
}

// Export singleton instance for global use
export const fastLEDWorkerManager = new FastLEDWorkerManager();

// Expose globally for debugging and external access
if (typeof window !== 'undefined') {
  /** @type {*} */(window).fastLEDWorkerManager = fastLEDWorkerManager;
}