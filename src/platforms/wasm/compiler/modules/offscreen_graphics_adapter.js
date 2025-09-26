/**
 * OffscreenCanvas Graphics Adapter - Canvas API Compatibility Layer
 *
 * This module provides a compatibility layer between regular HTMLCanvasElement
 * and OffscreenCanvas APIs, enabling seamless transitions between main thread
 * and background worker rendering modes.
 *
 * Key responsibilities:
 * - Unified API for both HTMLCanvasElement and OffscreenCanvas
 * - Context creation and management for different environments
 * - Feature detection and capability reporting
 * - Automatic fallback handling between canvas types
 * - Performance optimization for each canvas type
 *
 * @module FastLED/OffscreenGraphicsAdapter
 */

import { FASTLED_DEBUG_LOG, FASTLED_DEBUG_ERROR, FASTLED_DEBUG_TRACE } from './fastled_debug_logger.js';

/**
 * @typedef {Object} CanvasCapabilities
 * @property {boolean} offscreenCanvas - OffscreenCanvas support
 * @property {boolean} webgl2 - WebGL 2.0 support
 * @property {boolean} webgl - WebGL 1.0 support
 * @property {boolean} canvas2d - Canvas 2D support
 * @property {boolean} transferable - Transferable objects support
 * @property {boolean} workerContext - Running in worker context
 */

/**
 * @typedef {Object} ContextConfig
 * @property {string} contextType - Context type (webgl2, webgl, 2d)
 * @property {Object} contextAttributes - Context creation attributes
 * @property {boolean} preserveDrawingBuffer - Preserve drawing buffer
 * @property {boolean} antialias - Enable antialiasing
 * @property {string} powerPreference - Power preference setting
 */

/**
 * OffscreenCanvas Graphics Adapter Class
 *
 * Provides a unified interface for working with both HTMLCanvasElement
 * and OffscreenCanvas, abstracting away the differences between main thread
 * and worker environments.
 */
export class OffscreenGraphicsAdapter {
  /**
   * Creates a new OffscreenGraphicsAdapter instance
   * @param {HTMLCanvasElement|OffscreenCanvas} canvas - Canvas element
   * @param {ContextConfig} config - Context configuration
   */
  constructor(canvas, config = {}) {
    FASTLED_DEBUG_TRACE('OFFSCREEN_ADAPTER', 'constructor', 'ENTER', { config });

    /** @type {HTMLCanvasElement|OffscreenCanvas} Canvas instance */
    this.canvas = canvas;

    /** @type {ContextConfig} Context configuration */
    this.config = {
      contextType: 'webgl2',
      contextAttributes: {
        alpha: false,
        antialias: false,
        preserveDrawingBuffer: false,
        powerPreference: 'high-performance',
        failIfMajorPerformanceCaveat: false
      },
      ...config
    };

    /** @type {CanvasCapabilities} Detected capabilities */
    this.capabilities = this.detectCapabilities();

    /** @type {RenderingContext|null} Active rendering context */
    this.context = null;

    /** @type {string|null} Active context type */
    this.activeContextType = null;

    /** @type {boolean} Whether this is an OffscreenCanvas */
    this.isOffscreen = canvas instanceof OffscreenCanvas;

    /** @type {boolean} Whether we're in a worker environment */
    this.isWorker = typeof WorkerGlobalScope !== 'undefined' && self instanceof WorkerGlobalScope;

    /** @type {Object} Performance monitoring data */
    this.performanceData = {
      contextCreationTime: 0,
      lastRenderTime: 0,
      renderCount: 0
    };

    FASTLED_DEBUG_LOG('OFFSCREEN_ADAPTER', 'Adapter initialized', {
      isOffscreen: this.isOffscreen,
      isWorker: this.isWorker,
      capabilities: this.capabilities,
      canvasSize: { width: this.canvas.width, height: this.canvas.height }
    });

    FASTLED_DEBUG_TRACE('OFFSCREEN_ADAPTER', 'constructor', 'EXIT');
  }

  /**
   * Detects canvas and context capabilities
   * @returns {CanvasCapabilities} Detected capabilities
   */
  detectCapabilities() {
    FASTLED_DEBUG_TRACE('OFFSCREEN_ADAPTER', 'detectCapabilities', 'ENTER');

    const capabilities = {
      offscreenCanvas: typeof OffscreenCanvas !== 'undefined',
      webgl2: false,
      webgl: false,
      canvas2d: false,
      transferable: this.isTransferSupported(),
      workerContext: typeof WorkerGlobalScope !== 'undefined' && self instanceof WorkerGlobalScope
    };

    // Test context creation capabilities
    try {
      // Test WebGL 2.0
      const testWebGL2 = this.canvas.getContext('webgl2', { failIfMajorPerformanceCaveat: true });
      capabilities.webgl2 = !!testWebGL2;
      if (testWebGL2) {
        // Clean up test context
        const ext = testWebGL2.getExtension('WEBGL_lose_context');
        if (ext) ext.loseContext();
      }
    } catch (error) {
      FASTLED_DEBUG_LOG('OFFSCREEN_ADAPTER', 'WebGL2 test failed', error.message);
    }

    try {
      // Test WebGL 1.0
      const testWebGL = this.canvas.getContext('webgl', { failIfMajorPerformanceCaveat: true });
      capabilities.webgl = !!testWebGL;
      if (testWebGL) {
        // Clean up test context
        const ext = testWebGL.getExtension('WEBGL_lose_context');
        if (ext) ext.loseContext();
      }
    } catch (error) {
      FASTLED_DEBUG_LOG('OFFSCREEN_ADAPTER', 'WebGL test failed', error.message);
    }

    try {
      // Test Canvas 2D
      const test2D = this.canvas.getContext('2d');
      capabilities.canvas2d = !!test2D;
    } catch (error) {
      FASTLED_DEBUG_LOG('OFFSCREEN_ADAPTER', 'Canvas2D test failed', error.message);
    }

    FASTLED_DEBUG_LOG('OFFSCREEN_ADAPTER', 'Capabilities detected', capabilities);
    FASTLED_DEBUG_TRACE('OFFSCREEN_ADAPTER', 'detectCapabilities', 'EXIT', capabilities);

    return capabilities;
  }

  /**
   * Checks if transferable objects are supported
   * @returns {boolean} Transfer support status
   */
  isTransferSupported() {
    try {
      // Test transferable object support
      if (this.isWorker) {
        return typeof postMessage === 'function';
      } else {
        return typeof Worker !== 'undefined' &&
               typeof window !== 'undefined' &&
               'postMessage' in window;
      }
    } catch (error) {
      return false;
    }
  }

  /**
   * Creates and initializes the rendering context
   * @param {string} contextType - Desired context type
   * @param {Object} attributes - Context attributes
   * @returns {Promise<RenderingContext|null>} Created context
   */
  async createContext(contextType = null, attributes = null) {
    FASTLED_DEBUG_TRACE('OFFSCREEN_ADAPTER', 'createContext', 'ENTER', { contextType, attributes });

    const startTime = performance.now();

    try {
      // Use provided parameters or fall back to config
      const targetContextType = contextType || this.config.contextType;
      const contextAttributes = { ...this.config.contextAttributes, ...(attributes || {}) };

      // Validate context type is supported
      if (!this.isContextTypeSupported(targetContextType)) {
        throw new Error(`Context type ${targetContextType} not supported`);
      }

      // Create context
      this.context = this.canvas.getContext(targetContextType, contextAttributes);

      if (!this.context) {
        throw new Error(`Failed to create ${targetContextType} context`);
      }

      this.activeContextType = targetContextType;
      this.performanceData.contextCreationTime = performance.now() - startTime;

      // Initialize context-specific settings
      await this.initializeContext();

      FASTLED_DEBUG_LOG('OFFSCREEN_ADAPTER', 'Context created successfully', {
        type: targetContextType,
        creationTime: `${this.performanceData.contextCreationTime.toFixed(2)}ms`,
        canvas: { width: this.canvas.width, height: this.canvas.height }
      });

      FASTLED_DEBUG_TRACE('OFFSCREEN_ADAPTER', 'createContext', 'EXIT', { success: true });
      return this.context;

    } catch (error) {
      FASTLED_DEBUG_ERROR('OFFSCREEN_ADAPTER', 'Context creation failed', error);

      // Attempt fallback context creation
      const fallbackContext = await this.createFallbackContext();
      if (fallbackContext) {
        FASTLED_DEBUG_LOG('OFFSCREEN_ADAPTER', 'Fallback context created');
        FASTLED_DEBUG_TRACE('OFFSCREEN_ADAPTER', 'createContext', 'EXIT', { success: true, fallback: true });
        return fallbackContext;
      }

      FASTLED_DEBUG_TRACE('OFFSCREEN_ADAPTER', 'createContext', 'EXIT', { success: false });
      return null;
    }
  }

  /**
   * Checks if a context type is supported
   * @param {string} contextType - Context type to check
   * @returns {boolean} Support status
   */
  isContextTypeSupported(contextType) {
    switch (contextType) {
      case 'webgl2':
        return this.capabilities.webgl2;
      case 'webgl':
        return this.capabilities.webgl;
      case '2d':
        return this.capabilities.canvas2d;
      default:
        return false;
    }
  }

  /**
   * Creates a fallback context when the preferred type fails
   * @returns {Promise<RenderingContext|null>} Fallback context
   */
  async createFallbackContext() {
    FASTLED_DEBUG_TRACE('OFFSCREEN_ADAPTER', 'createFallbackContext', 'ENTER');

    // Try fallback context types in order of preference
    const fallbackTypes = ['webgl2', 'webgl', '2d'];

    for (const contextType of fallbackTypes) {
      if (contextType === this.activeContextType) {
        continue; // Skip the type that already failed
      }

      if (this.isContextTypeSupported(contextType)) {
        try {
          this.context = this.canvas.getContext(contextType, this.config.contextAttributes);

          if (this.context) {
            this.activeContextType = contextType;
            await this.initializeContext();

            FASTLED_DEBUG_LOG('OFFSCREEN_ADAPTER', 'Fallback context created', {
              type: contextType,
              original: this.config.contextType
            });

            FASTLED_DEBUG_TRACE('OFFSCREEN_ADAPTER', 'createFallbackContext', 'EXIT', { success: true });
            return this.context;
          }
        } catch (error) {
          FASTLED_DEBUG_LOG('OFFSCREEN_ADAPTER', 'Fallback context failed', {
            type: contextType,
            error: error.message
          });
        }
      }
    }

    FASTLED_DEBUG_ERROR('OFFSCREEN_ADAPTER', 'All fallback contexts failed');
    FASTLED_DEBUG_TRACE('OFFSCREEN_ADAPTER', 'createFallbackContext', 'EXIT', { success: false });
    return null;
  }

  /**
   * Initializes the created context with default settings
   * @returns {Promise<void>}
   */
  async initializeContext() {
    FASTLED_DEBUG_TRACE('OFFSCREEN_ADAPTER', 'initializeContext', 'ENTER');

    if (!this.context) {
      throw new Error('No context available for initialization');
    }

    try {
      if (this.activeContextType === 'webgl2' || this.activeContextType === 'webgl') {
        await this.initializeWebGLContext();
      } else if (this.activeContextType === '2d') {
        await this.initialize2DContext();
      }

      FASTLED_DEBUG_LOG('OFFSCREEN_ADAPTER', 'Context initialized', {
        type: this.activeContextType
      });

    } catch (error) {
      FASTLED_DEBUG_ERROR('OFFSCREEN_ADAPTER', 'Context initialization failed', error);
      throw error;
    }

    FASTLED_DEBUG_TRACE('OFFSCREEN_ADAPTER', 'initializeContext', 'EXIT');
  }

  /**
   * Initializes WebGL context settings
   * @returns {Promise<void>}
   */
  async initializeWebGLContext() {
    const gl = this.context;

    // Set viewport
    gl.viewport(0, 0, this.canvas.width, this.canvas.height);

    // Enable depth testing
    gl.enable(gl.DEPTH_TEST);
    gl.depthFunc(gl.LEQUAL);

    // Set clear color
    gl.clearColor(0.0, 0.0, 0.0, 1.0);

    // Enable blending for transparency
    gl.enable(gl.BLEND);
    gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);

    FASTLED_DEBUG_LOG('OFFSCREEN_ADAPTER', 'WebGL context initialized', {
      version: gl.getParameter(gl.VERSION),
      vendor: gl.getParameter(gl.VENDOR),
      renderer: gl.getParameter(gl.RENDERER)
    });
  }

  /**
   * Initializes Canvas 2D context settings
   * @returns {Promise<void>}
   */
  async initialize2DContext() {
    const ctx = this.context;

    // Set default composite operation
    ctx.globalCompositeOperation = 'source-over';

    // Set image smoothing
    ctx.imageSmoothingEnabled = false; // Pixel art friendly

    FASTLED_DEBUG_LOG('OFFSCREEN_ADAPTER', '2D context initialized');
  }

  /**
   * Resizes the canvas and updates viewport
   * @param {number} width - New width
   * @param {number} height - New height
   * @returns {boolean} Resize success status
   */
  resize(width, height) {
    FASTLED_DEBUG_TRACE('OFFSCREEN_ADAPTER', 'resize', 'ENTER', { width, height });

    try {
      const oldWidth = this.canvas.width;
      const oldHeight = this.canvas.height;

      // Update canvas size
      this.canvas.width = width;
      this.canvas.height = height;

      // Update WebGL viewport if applicable
      if (this.context && (this.activeContextType === 'webgl2' || this.activeContextType === 'webgl')) {
        this.context.viewport(0, 0, width, height);
      }

      FASTLED_DEBUG_LOG('OFFSCREEN_ADAPTER', 'Canvas resized', {
        oldSize: { width: oldWidth, height: oldHeight },
        newSize: { width, height }
      });

      FASTLED_DEBUG_TRACE('OFFSCREEN_ADAPTER', 'resize', 'EXIT', { success: true });
      return true;

    } catch (error) {
      FASTLED_DEBUG_ERROR('OFFSCREEN_ADAPTER', 'Resize failed', error);
      FASTLED_DEBUG_TRACE('OFFSCREEN_ADAPTER', 'resize', 'EXIT', { success: false });
      return false;
    }
  }

  /**
   * Clears the canvas with specified color
   * @param {number} r - Red component (0-1)
   * @param {number} g - Green component (0-1)
   * @param {number} b - Blue component (0-1)
   * @param {number} a - Alpha component (0-1)
   */
  clear(r = 0, g = 0, b = 0, a = 1) {
    if (!this.context) return;

    const renderStart = performance.now();

    try {
      if (this.activeContextType === 'webgl2' || this.activeContextType === 'webgl') {
        const gl = this.context;
        gl.clearColor(r, g, b, a);
        gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
      } else if (this.activeContextType === '2d') {
        const ctx = this.context;
        ctx.fillStyle = `rgba(${Math.floor(r * 255)}, ${Math.floor(g * 255)}, ${Math.floor(b * 255)}, ${a})`;
        ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
      }

      this.updateRenderStats(performance.now() - renderStart);

    } catch (error) {
      FASTLED_DEBUG_ERROR('OFFSCREEN_ADAPTER', 'Clear operation failed', error);
    }
  }

  /**
   * Transfers canvas to ImageBitmap (for main thread display)
   * @returns {Promise<ImageBitmap|null>} Canvas as ImageBitmap
   */
  async transferToImageBitmap() {
    FASTLED_DEBUG_TRACE('OFFSCREEN_ADAPTER', 'transferToImageBitmap', 'ENTER');

    if (!this.isOffscreen || typeof this.canvas.transferToImageBitmap !== 'function') {
      FASTLED_DEBUG_LOG('OFFSCREEN_ADAPTER', 'transferToImageBitmap not supported');
      return null;
    }

    try {
      const imageBitmap = this.canvas.transferToImageBitmap();
      FASTLED_DEBUG_LOG('OFFSCREEN_ADAPTER', 'ImageBitmap created', {
        width: imageBitmap.width,
        height: imageBitmap.height
      });

      FASTLED_DEBUG_TRACE('OFFSCREEN_ADAPTER', 'transferToImageBitmap', 'EXIT', { success: true });
      return imageBitmap;

    } catch (error) {
      FASTLED_DEBUG_ERROR('OFFSCREEN_ADAPTER', 'ImageBitmap transfer failed', error);
      FASTLED_DEBUG_TRACE('OFFSCREEN_ADAPTER', 'transferToImageBitmap', 'EXIT', { success: false });
      return null;
    }
  }

  /**
   * Updates rendering performance statistics
   * @param {number} renderTime - Render time in milliseconds
   */
  updateRenderStats(renderTime) {
    this.performanceData.lastRenderTime = renderTime;
    this.performanceData.renderCount++;
  }

  /**
   * Gets current performance statistics
   * @returns {Object} Performance data
   */
  getPerformanceStats() {
    return {
      ...this.performanceData,
      canvasSize: {
        width: this.canvas.width,
        height: this.canvas.height
      },
      contextType: this.activeContextType,
      isOffscreen: this.isOffscreen,
      capabilities: this.capabilities
    };
  }

  /**
   * Gets the underlying canvas element
   * @returns {HTMLCanvasElement|OffscreenCanvas} Canvas element
   */
  getCanvas() {
    return this.canvas;
  }

  /**
   * Gets the rendering context
   * @returns {RenderingContext|null} Rendering context
   */
  getContext() {
    return this.context;
  }

  /**
   * Gets adapter capabilities
   * @returns {CanvasCapabilities} Capabilities
   */
  getCapabilities() {
    return this.capabilities;
  }

  /**
   * Destroys the adapter and cleans up resources
   */
  destroy() {
    FASTLED_DEBUG_TRACE('OFFSCREEN_ADAPTER', 'destroy', 'ENTER');

    // Lose WebGL context if applicable
    if (this.context && (this.activeContextType === 'webgl2' || this.activeContextType === 'webgl')) {
      const ext = this.context.getExtension('WEBGL_lose_context');
      if (ext) {
        ext.loseContext();
      }
    }

    this.context = null;
    this.activeContextType = null;
    this.canvas = null;

    FASTLED_DEBUG_LOG('OFFSCREEN_ADAPTER', 'Adapter destroyed');
    FASTLED_DEBUG_TRACE('OFFSCREEN_ADAPTER', 'destroy', 'EXIT');
  }
}

/**
 * Factory function to create an appropriate adapter
 * @param {HTMLCanvasElement|OffscreenCanvas} canvas - Canvas element
 * @param {ContextConfig} config - Context configuration
 * @returns {OffscreenGraphicsAdapter} Created adapter
 */
export function createGraphicsAdapter(canvas, config = {}) {
  FASTLED_DEBUG_LOG('OFFSCREEN_ADAPTER', 'Creating graphics adapter', {
    canvasType: canvas.constructor.name,
    config
  });

  return new OffscreenGraphicsAdapter(canvas, config);
}

/**
 * Utility function to check OffscreenCanvas support
 * @returns {boolean} OffscreenCanvas support status
 */
export function isOffscreenCanvasSupported() {
  return typeof OffscreenCanvas !== 'undefined';
}

/**
 * Utility function to check if running in worker context
 * @returns {boolean} Worker context status
 */
export function isWorkerContext() {
  return typeof WorkerGlobalScope !== 'undefined' && self instanceof WorkerGlobalScope;
}