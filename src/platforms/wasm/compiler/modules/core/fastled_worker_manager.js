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

    console.log('🔧 detectCapabilities:', {
      offscreenCanvas: capabilities.offscreenCanvas,
      sharedArrayBuffer: capabilities.sharedArrayBuffer,
      transferableObjects: capabilities.transferableObjects
    });

    // Test WebGL 2.0 support with OffscreenCanvas
    if (capabilities.offscreenCanvas) {
      try {
        const testCanvas = new OffscreenCanvas(1, 1);
        const ctx = testCanvas.getContext('webgl2');
        capabilities.webgl2 = !!ctx;
        console.log('🔧 WebGL2 OffscreenCanvas test: SUCCESS');
        FASTLED_DEBUG_LOG('WORKER_MANAGER', 'WebGL2 OffscreenCanvas test successful');
      } catch (error) {
        console.error('🔧 WebGL2 OffscreenCanvas test: FAILED', error);
        FASTLED_DEBUG_ERROR('WORKER_MANAGER', 'WebGL2 OffscreenCanvas test failed', error);
        capabilities.webgl2 = false;
      }
    } else {
      console.warn('🔧 OffscreenCanvas not available in this browser');
    }

    console.log('🔧 Final capabilities:', capabilities);
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
    console.log('🔧 initialize() called');

    try {
      // Validate configuration
      if (!config.canvas || !config.fastledModule) {
        console.error('🔧 Invalid config: missing canvas or WASM module');
        throw new Error('Invalid worker configuration: missing canvas or WASM module');
      }
      console.log('🔧 Config validation: OK');

      this.mainCanvas = config.canvas;
      this.maxRetries = config.maxRetries || 3;

      // Transfer canvas control to OffscreenCanvas
      console.log('🔧 About to transfer canvas to offscreen...');
      this.offscreenCanvas = this.mainCanvas.transferControlToOffscreen();
      console.log('🔧 Canvas transferred successfully');
      FASTLED_DEBUG_LOG('WORKER_MANAGER', `Canvas control transferred to OffscreenCanvas`);

      // Create and configure worker
      console.log('🔧 About to create worker...');
      await this.createWorker(config);
      console.log('🔧 Worker created successfully');

      // Verify worker is operational
      console.log('🔧 About to verify worker readiness...');
      const isWorkerReady = await this.verifyWorkerReady();
      console.log(`🔧 Worker ready check: ${isWorkerReady}`);
      if (!isWorkerReady) {
        throw new Error('Worker failed readiness verification');
      }

      this.isWorkerActive = true;
      console.log('🔧 Worker mode fully initialized!');
      console.log('🔧 Set isWorkerActive =', this.isWorkerActive);

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
      console.error('🔧 Worker initialization FAILED:', error);
      console.error('🔧 Error message:', error.message);
      console.error('🔧 Error stack:', error.stack);
      FASTLED_DEBUG_ERROR('WORKER_MANAGER', 'Worker initialization failed', error);

      // Fail hard - no fallback mode
      FASTLED_DEBUG_TRACE('WORKER_MANAGER', 'initialize', 'EXIT', { success: false });
      throw error; // Re-throw to propagate the error
    }
  }


  /**
   * Creates and configures the background worker
   * @param {WorkerConfiguration} config - Worker configuration
   * @returns {Promise<void>}
   */
  async createWorker(config) {
    FASTLED_DEBUG_TRACE('WORKER_MANAGER', 'createWorker', 'ENTER');
    console.log('🔧 createWorker: ENTER');

    try {
      // Create worker with the background worker script
      console.log('🔧 createWorker: Creating Worker instance...');
      const workerScript = new URL('./fastled_background_worker.js', import.meta.url);
      console.log('🔧 createWorker: Worker script URL:', workerScript.href);
      this.worker = new Worker(workerScript, {
        type: 'module',
        name: 'fastled-background-worker'
      });
      console.log('🔧 createWorker: Worker instance created');

      // Set up worker message handling
      console.log('🔧 createWorker: Setting up message handlers...');
      this.worker.onmessage = this.handleWorkerMessage.bind(this);
      this.worker.onerror = this.handleWorkerError.bind(this);
      this.worker.onmessageerror = this.handleWorkerMessageError.bind(this);
      console.log('🔧 createWorker: Message handlers set up');

      // Initialize worker with configuration
      console.log('🔧 createWorker: Preparing initialization message...');

      // Extract URL parameters from main thread to pass to worker
      const urlParams = new URLSearchParams(window.location.search);
      const urlParamsObject = {};
      for (const [key, value] of urlParams.entries()) {
        urlParamsObject[key] = value;
      }

      const initMessage = {
        type: 'initialize',
        id: this.generateMessageId(),
        payload: {
          canvas: this.offscreenCanvas,
          capabilities: this.capabilities,
          frameRate: config.frameRate,
          urlParams: urlParamsObject, // Pass URL parameters to worker
          wasmModuleConfig: {
            // Extract relevant WASM configuration for worker
            // Note: Full module transfer happens separately
          }
        }
      };
      console.log('🔧 createWorker: Init message prepared, id:', initMessage.id);
      console.log('🔧 createWorker: URL params being passed to worker:', urlParamsObject);

      console.log('🔧 createWorker: About to call sendMessageWithResponse...');
      console.log('🔧 createWorker: isWorkerActive BEFORE send:', this.isWorkerActive);
      const success = await this.sendMessageWithResponse(initMessage, [this.offscreenCanvas]);
      console.log('🔧 createWorker: sendMessageWithResponse returned:', success);
      if (!success) {
        throw new Error('Worker initialization message failed');
      }

      FASTLED_DEBUG_LOG('WORKER_MANAGER', 'Worker created and configured successfully');
      console.log('🔧 createWorker: EXIT (success)');
      FASTLED_DEBUG_TRACE('WORKER_MANAGER', 'createWorker', 'EXIT');

    } catch (error) {
      console.error('🔧 createWorker: EXCEPTION caught:', error);
      console.error('🔧 createWorker: Error message:', error.message);
      console.error('🔧 createWorker: Error stack:', error.stack);
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

      // Response comes back wrapped - check response.payload for the actual pong data
      // or check response.type === 'pong' if unwrapped by callback
      if (response && (response.type === 'pong' || response.payload?.type === 'pong')) {
        FASTLED_DEBUG_LOG('WORKER_MANAGER', 'Worker ready', {
          roundTripTime: `${roundTripTime.toFixed(2)}ms`,
          workerTimestamp: response.timestamp || response.payload?.timestamp
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
   * Sends a message to the worker and waits for response
   * @param {Object} message - Message to send
   * @param {Transferable[]|null} transferables - Transferable objects
   * @param {number} timeout - Timeout in milliseconds
   * @returns {Promise<Object>} Worker response
   */
  async sendMessageWithResponse(message, transferables = null, timeout = 10000) {
    FASTLED_DEBUG_TRACE('WORKER_MANAGER', 'sendMessageWithResponse', 'ENTER', { messageType: message.type });
    console.log('🔧 sendMessageWithResponse: ENTER, type:', message.type);
    console.log('🔧 sendMessageWithResponse: this.worker exists:', !!this.worker);
    console.log('🔧 sendMessageWithResponse: this.isWorkerActive:', this.isWorkerActive);

    if (!this.worker) {
      console.error('🔧 sendMessageWithResponse: Worker is null!');
      throw new Error('Worker not available for message sending (worker is null)');
    }

    // NOTE: For initialization and ping messages, we allow sending even when isWorkerActive is false
    // because the worker needs to receive the initialization message to become active,
    // and ping is used for readiness verification before setting isWorkerActive
    if (!this.isWorkerActive && message.type !== 'initialize' && message.type !== 'ping') {
      console.error('🔧 sendMessageWithResponse: Worker not active and message type is:', message.type);
      throw new Error('Worker not active for message sending (only initialize/ping allowed)');
    }
    console.log('🔧 sendMessageWithResponse: Validation passed, proceeding...');

    return new Promise((resolve, reject) => {
      const messageId = message.id || this.generateMessageId();
      message.id = messageId;
      console.log('🔧 sendMessageWithResponse: Message ID:', messageId);

      // Set up timeout
      const timeoutId = setTimeout(() => {
        console.error('🔧 sendMessageWithResponse: TIMEOUT after', timeout, 'ms for message:', messageId);
        this.pendingCallbacks.delete(messageId);
        reject(new Error(`Worker message timeout after ${timeout}ms`));
      }, timeout);

      // Store callback for response
      this.pendingCallbacks.set(messageId, (response) => {
        console.log('🔧 sendMessageWithResponse: Response received for message:', messageId);
        clearTimeout(timeoutId);
        this.pendingCallbacks.delete(messageId);
        resolve(response);
      });
      console.log('🔧 sendMessageWithResponse: Callback stored for ID:', messageId);
      console.log('🔧 sendMessageWithResponse: Pending count:', this.pendingCallbacks.size);
      console.log('🔧 sendMessageWithResponse: Pending IDs after store:', Array.from(this.pendingCallbacks.keys()));

      try {
        // Send message to worker
        if (transferables && transferables.length > 0) {
          console.log('🔧 sendMessageWithResponse: Posting message WITH transferables...');
          this.worker.postMessage(message, transferables);
          console.log('🔧 sendMessageWithResponse: Message posted (with transferables)');
          FASTLED_DEBUG_LOG('WORKER_MANAGER', 'Message sent with transferables', {
            type: message.type,
            id: messageId,
            transferables: transferables.length
          });
        } else {
          console.log('🔧 sendMessageWithResponse: Posting message WITHOUT transferables...');
          this.worker.postMessage(message);
          console.log('🔧 sendMessageWithResponse: Message posted (no transferables)');
          FASTLED_DEBUG_LOG('WORKER_MANAGER', 'Message sent', {
            type: message.type,
            id: messageId
          });
        }
      } catch (error) {
        console.error('🔧 sendMessageWithResponse: postMessage EXCEPTION:', error);
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

        case 'ui_elements_add':
          // Handle UI elements from C++ worker thread - call uiManager on main thread
          //console.log('🎨 [WORKER_MANAGER] Received ui_elements_add message from worker:', data);
          this.handleUiElementsAdd(data.payload);
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
   * Handles UI elements from worker thread - forwards to main thread UI manager
   * @param {Object} payload - UI elements data
   */
  handleUiElementsAdd(payload) {
    FASTLED_DEBUG_LOG('WORKER_MANAGER', 'Received UI elements from worker', payload);
    //console.log('🎨 [WORKER_MANAGER] handleUiElementsAdd called with payload:', payload);
    //console.log('🎨 [WORKER_MANAGER] payload.elements:', payload ? payload.elements : 'NO PAYLOAD');
    //console.log('🎨 [WORKER_MANAGER] window.uiManager exists:', !!window.uiManager);
    //console.log('🎨 [WORKER_MANAGER] uiManager.addUiElements exists:', window.uiManager ? typeof window.uiManager.addUiElements : 'N/A');

    try {
      if (!payload || !payload.elements) {
        //console.error('🎨 [WORKER_MANAGER] Invalid UI elements payload - missing elements array');
        FASTLED_DEBUG_ERROR('WORKER_MANAGER', 'Invalid UI elements payload', payload);
        return;
      }

      console.log('🎨 [WORKER_MANAGER] Payload validation passed, elements count:', payload.elements.length);

      // Log the inbound event to the inspector if available
      if (window.jsonInspector) {
        //console.log('🎨 [WORKER_MANAGER] Logging to jsonInspector');
        window.jsonInspector.logInboundEvent(payload.elements, 'Worker → Main');
      }

      // Call UI manager on main thread to add/update UI elements
      if (window.uiManager && typeof window.uiManager.addUiElements === 'function') {
        //console.log('🎨 [WORKER_MANAGER] Calling uiManager.addUiElements with', payload.elements.length, 'elements');
        window.uiManager.addUiElements(payload.elements);
        //console.log('🎨 [WORKER_MANAGER] UI elements processed by uiManager - SUCCESS');
        //FASTLED_DEBUG_LOG('WORKER_MANAGER', 'UI elements processed by uiManager');
      } else {
        console.error('🎨 [WORKER_MANAGER] UI Manager not available on main thread!');
        console.warn('UI Manager not available on main thread');
      }

    } catch (error) {
      console.error('🎨 [WORKER_MANAGER] Exception in handleUiElementsAdd:', error);
      console.error('🎨 [WORKER_MANAGER] Error stack:', error.stack);
      FASTLED_DEBUG_ERROR('WORKER_MANAGER', 'Error handling UI elements from worker', error);
    }
  }

  /**
   * Handles worker errors
   * @param {ErrorEvent} event - Worker error event
   */
  handleWorkerError(event) {
    console.error('🔧 handleWorkerError: Worker error occurred!');
    console.error('🔧 handleWorkerError: message:', event.message);
    console.error('🔧 handleWorkerError: filename:', event.filename);
    console.error('🔧 handleWorkerError: lineno:', event.lineno);
    console.error('🔧 handleWorkerError: colno:', event.colno);
    console.error('🔧 handleWorkerError: error:', event.error);

    FASTLED_DEBUG_ERROR('WORKER_MANAGER', 'Worker error', {
      message: event.message,
      filename: event.filename,
      lineno: event.lineno,
      colno: event.colno,
      error: event.error
    });

    // Attempt recovery or fallback
    const error = event.error || new Error(event.message || 'Worker error (no details)');
    this.handleWorkerFailure('worker_error', error);
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
   * Handles worker failure - fails hard without recovery
   * @param {string} reason - Failure reason
   * @param {Error} error - Error object
   */
  handleWorkerFailure(reason, error) {
    FASTLED_DEBUG_ERROR('WORKER_MANAGER', 'Worker failure detected', { reason, error });

    this.isWorkerActive = false;

    // Emit failure event
    fastLEDEvents.emit('worker:failed', {
      reason,
      error: error.message
    });

    // Terminate worker and fail hard
    this.terminateWorker();

    // Show user-friendly error message
    const errorDisplay = document.getElementById('error-display');
    if (errorDisplay) {
      errorDisplay.textContent = `Worker error: ${error.message}. Please refresh the page.`;
      errorDisplay.style.color = '#ff6b6b';
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
      capabilities: this.capabilities,
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
