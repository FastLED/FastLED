/**
 * FastLED Graphics Manager Base Class
 *
 * Unified base class for all graphics managers using Three.js.
 * Provides common functionality for both tile-based and 3D LED rendering.
 *
 * @module GraphicsManagerBase
 */

import { createGraphicsAdapter } from './offscreen_graphics_adapter.js';

/* eslint-disable no-console */
/* eslint-disable no-restricted-syntax */
/* eslint-disable max-len */
/* eslint-disable guard-for-in */
/* eslint-disable camelcase */
/* eslint-disable no-plusplus */
/* eslint-disable no-continue */

/**
 * @typedef {Object} StripData
 * @property {number} strip_id - Unique identifier for the LED strip
 * @property {Uint8Array} pixel_data - Raw pixel data (RGB values)
 * @property {number} length - Number of LEDs in the strip
 */

/**
 * @typedef {Object} ScreenMapData
 * @property {number[]} absMax - Maximum coordinates array
 * @property {number[]} absMin - Minimum coordinates array
 * @property {{ [key: string]: any }} strips - Strip configuration data
 */

/**
 * @typedef {Object} FrameData
 * @property {number} strip_id - Strip identifier
 * @property {Uint8Array} pixel_data - RGB pixel data (3 bytes per pixel)
 * @property {ScreenMapData} screenMap - Screen coordinate mapping data
 */

/**
 * Base class for graphics managers providing common Three.js functionality
 */
export class GraphicsManagerBase {
  /**
   * Creates a new GraphicsManagerBase instance
   * @param {Object} graphicsArgs - Configuration options
   * @param {string|HTMLCanvasElement|OffscreenCanvas} graphicsArgs.canvasId - Canvas element or ID
   * @param {Object} graphicsArgs.threeJsModules - Three.js modules
   * @param {OffscreenCanvas} [graphicsArgs.offscreenCanvas] - Direct OffscreenCanvas instance
   */
  constructor(graphicsArgs) {
    const { canvasId, threeJsModules, offscreenCanvas } = graphicsArgs;

    /** @type {string|HTMLCanvasElement|OffscreenCanvas} */
    this.canvasId = canvasId;

    /** @type {Object} Three.js modules and dependencies */
    this.threeJsModules = threeJsModules;

    /** @type {HTMLCanvasElement|OffscreenCanvas|null} */
    this.canvas = null;

    /** @type {OffscreenCanvas|null} Direct OffscreenCanvas reference */
    this.offscreenCanvas = offscreenCanvas || null;

    /** @type {import('./offscreen_graphics_adapter.js').OffscreenGraphicsAdapter|null} */
    this.canvasAdapter = null;

    /** @type {Object|null} Three.js scene object */
    this.scene = null;

    /** @type {Object|null} Three.js camera object */
    this.camera = null;

    /** @type {Object|null} Three.js WebGL renderer */
    this.renderer = null;

    /** @type {ScreenMapData} */
    this.screenMap = null;

    /** @type {number} Screen width in rendering units */
    this.SCREEN_WIDTH = 0;

    /** @type {number} Screen height in rendering units */
    this.SCREEN_HEIGHT = 0;

    /** @type {number} Previous frame's total LED count for optimization */
    this.previousTotalLeds = 0;

    // Initialize common components
    this.initialize();
  }

  /**
   * Initializes the base graphics system
   * @returns {boolean|Promise<boolean>} True if initialization was successful
   */
  initialize() {
    try {
      // Handle different canvas input types
      if (this.offscreenCanvas) {
        // Direct OffscreenCanvas provided
        this.canvas = this.offscreenCanvas;
      } else if (typeof this.canvasId === 'string') {
        // Canvas ID string provided
        if (typeof document !== 'undefined') {
          this.canvas = document.getElementById(this.canvasId);
        } else {
          console.error('Document not available for canvas ID lookup in worker context');
          return false;
        }
      } else if (this.canvasId instanceof HTMLCanvasElement || this.canvasId instanceof OffscreenCanvas) {
        // Direct canvas element provided
        this.canvas = this.canvasId;
      } else {
        console.error('Invalid canvas reference provided');
        return false;
      }

      if (!this.canvas) {
        console.error(`Canvas with id ${this.canvasId} not found`);
        return false;
      }

      // Create canvas adapter for unified API (but we'll use Three.js renderer instead)
      this.canvasAdapter = createGraphicsAdapter(this.canvas, {
        contextType: 'webgl2',
        contextAttributes: {
          alpha: false,
          antialias: true,
          preserveDrawingBuffer: false,
          powerPreference: 'high-performance'
        }
      });

      return true;

    } catch (error) {
      console.error('Base graphics manager initialization failed:', error);
      return false;
    }
  }

  /**
   * Sets up canvas dimensions and display properties
   * @protected
   * @param {Object} frameData - Frame data containing screen mapping
   */
  _setupCanvasAndDimensions(frameData) {
    const RESOLUTION_BOOST = 2; // 2x resolution for higher quality
    const MAX_WIDTH = 640; // Max pixels width on browser

    // Handle both direct canvas access and document lookup
    const canvas = this.canvas || (typeof document !== 'undefined' ? document.getElementById(this.canvasId) : null);
    if (!canvas) {
      console.error('Canvas not available for dimension setup');
      return;
    }

    const { screenMap } = frameData;
    const screenMapWidth = screenMap.absMax[0] - screenMap.absMin[0];
    const screenMapHeight = screenMap.absMax[1] - screenMap.absMin[1];

    // Always set width to 640px and scale height proportionally
    const targetWidth = MAX_WIDTH;
    const aspectRatio = screenMapWidth / screenMapHeight;
    const targetHeight = Math.round(targetWidth / aspectRatio);

    // Set the rendering resolution (2x the display size)
    this.SCREEN_WIDTH = targetWidth * RESOLUTION_BOOST;
    this.SCREEN_HEIGHT = targetHeight * RESOLUTION_BOOST;

    // Set internal canvas size to 2x for higher resolution
    canvas.width = targetWidth * RESOLUTION_BOOST;
    canvas.height = targetHeight * RESOLUTION_BOOST;

    // But keep display size the same (if we have style access)
    if (canvas.style) {
      canvas.style.width = `${targetWidth}px`;
      canvas.style.height = `${targetHeight}px`;
      canvas.style.maxWidth = `${targetWidth}px`;
      canvas.style.maxHeight = `${targetHeight}px`;
    }

    // Store the bounds for camera calculations
    this.screenBounds = {
      width: screenMapWidth,
      height: screenMapHeight,
      minX: screenMap.absMin[0],
      minY: screenMap.absMin[1],
      maxX: screenMap.absMax[0],
      maxY: screenMap.absMax[1],
    };
  }

  /**
   * Sets up the Three.js scene and camera
   * @protected
   */
  _setupScene() {
    const { THREE } = this.threeJsModules;

    // Create the scene
    this.scene = new THREE.Scene();

    // Camera configuration
    this._setupCamera();
  }

  /**
   * Sets up the camera with proper positioning and projection
   * @protected
   */
  _setupCamera() {
    const { THREE } = this.threeJsModules;

    // Camera parameters
    const NEAR_PLANE = 0.1;
    const FAR_PLANE = 5000;
    const MARGIN = 1.05; // Add a small margin around the screen

    // Calculate half width and height for orthographic camera
    const halfWidth = this.SCREEN_WIDTH / 2;
    const halfHeight = this.SCREEN_HEIGHT / 2;

    // Create orthographic camera
    this.camera = new THREE.OrthographicCamera(
      -halfWidth * MARGIN, // left
      halfWidth * MARGIN, // right
      halfHeight * MARGIN, // top
      -halfHeight * MARGIN, // bottom
      NEAR_PLANE,
      FAR_PLANE,
    );

    // Position the camera at a fixed distance
    this.camera.position.set(0, 0, 1000);
    this.camera.zoom = 1.0;
    this.camera.updateProjectionMatrix();
  }

  /**
   * Sets up the Three.js WebGL renderer
   * @protected
   */
  _setupRenderer() {
    const { THREE } = this.threeJsModules;

    this.renderer = new THREE.WebGLRenderer({
      canvas: this.canvas,
      antialias: true,
    });
    this.renderer.setSize(this.SCREEN_WIDTH, this.SCREEN_HEIGHT);
  }

  /**
   * Updates the display with new frame data from FastLED
   * This method should be overridden by subclasses
   * @param {StripData[]} _frameData - Array of LED strip data to render
   */
  updateDisplay(_frameData) {
    throw new Error('updateDisplay must be implemented by subclass');
  }

  /**
   * Updates the canvas with new LED frame data
   * This method should be overridden by subclasses
   * @param {StripData[] & {screenMap?: ScreenMapData}} _frameData - Array of frame data containing LED strip information with optional screenMap
   */
  updateCanvas(_frameData) {
    throw new Error('updateCanvas must be implemented by subclass');
  }

  /**
   * Handles ImageBitmap transfer for OffscreenCanvas rendering
   * Creates and transfers ImageBitmap to main thread when in worker mode
   * @protected
   */
  async handleOffscreenTransfer() {
    if (!this.canvasAdapter || !this.canvasAdapter.isOffscreen) {
      return; // Not in OffscreenCanvas mode
    }

    try {
      // Check if we're in a worker context
      const isWorker = typeof WorkerGlobalScope !== 'undefined' &&
                       typeof self !== 'undefined' &&
                       self instanceof WorkerGlobalScope;

      if (isWorker) {
        // In worker context - create and transfer ImageBitmap
        const imageBitmap = await this.canvasAdapter.transferToImageBitmap();

        if (imageBitmap && typeof postMessage === 'function') {
          // Send ImageBitmap to main thread for display
          postMessage({
            type: 'frame_imageBitmap',
            payload: {
              imageBitmap,
              width: this.canvas.width,
              height: this.canvas.height,
              timestamp: performance.now()
            }
          }, [imageBitmap]); // Transfer ImageBitmap
        }
      } else {
        // In main thread with OffscreenCanvas (worker manager mode)
        if (typeof window !== 'undefined' && window.fastLEDEvents) {
          window.fastLEDEvents.emit('offscreen:frame_ready', {
            canvas: this.canvas,
            timestamp: performance.now()
          });
        }
      }
    } catch (error) {
      console.error('Failed to transfer OffscreenCanvas ImageBitmap:', error);
    }
  }

  /**
   * Resizes the canvas and updates viewport
   * @param {number} width - New canvas width
   * @param {number} height - New canvas height
   * @returns {boolean} True if resize was successful
   */
  resize(width, height) {
    try {
      if (this.canvasAdapter) {
        // Use adapter for unified resize handling
        const success = this.canvasAdapter.resize(width, height);
        if (!success) {
          console.error('Canvas adapter resize failed');
          return false;
        }
      } else {
        // Fallback direct resize
        this.canvas.width = width;
        this.canvas.height = height;
      }

      // Update Three.js renderer size
      if (this.renderer) {
        this.renderer.setSize(width, height);
      }

      // Update camera if needed
      if (this.camera && this.camera.isOrthographicCamera) {
        const halfWidth = width / 2;
        const halfHeight = height / 2;
        const MARGIN = 1.05;

        this.camera.left = -halfWidth * MARGIN;
        this.camera.right = halfWidth * MARGIN;
        this.camera.top = halfHeight * MARGIN;
        this.camera.bottom = -halfHeight * MARGIN;
        this.camera.updateProjectionMatrix();
      }

      console.log('Base graphics manager resized', {
        width,
        height,
        isOffscreen: this.canvasAdapter?.isOffscreen || false
      });

      return true;

    } catch (error) {
      console.error('Base graphics manager resize failed:', error);
      return false;
    }
  }

  /**
   * Gets performance statistics including canvas adapter info
   * @returns {Object} Performance and capability information
   */
  getPerformanceInfo() {
    const baseInfo = {
      canvasSize: {
        width: this.canvas?.width || 0,
        height: this.canvas?.height || 0
      },
      screenSize: {
        width: this.SCREEN_WIDTH,
        height: this.SCREEN_HEIGHT
      },
      hasThreeJS: !!(this.scene && this.camera && this.renderer),
      initialized: !!(this.scene && this.camera && this.renderer)
    };

    if (this.canvasAdapter) {
      return {
        ...baseInfo,
        ...this.canvasAdapter.getPerformanceStats()
      };
    }

    return baseInfo;
  }

  /**
   * Resets and cleans up graphics resources
   * Should be overridden by subclasses for specific cleanup
   */
  reset() {
    // Clear the scene
    if (this.scene) {
      while (this.scene.children.length > 0) {
        this.scene.remove(this.scene.children[0]);
      }
    }

    this.previousTotalLeds = 0;
  }

  /**
   * Clean up resources including canvas adapter
   */
  destroy() {
    // Clean up Three.js resources
    if (this.scene) {
      while (this.scene.children.length > 0) {
        this.scene.remove(this.scene.children[0]);
      }
    }

    if (this.renderer) {
      this.renderer.dispose();
    }

    // Clean up canvas adapter
    if (this.canvasAdapter) {
      this.canvasAdapter.destroy();
      this.canvasAdapter = null;
    }

    this.scene = null;
    this.camera = null;
    this.renderer = null;
    this.canvas = null;

    console.log('Base graphics manager destroyed');
  }
}