/// <reference path="../types.d.ts" />

/**
 * FastLED Graphics Manager Module
 *
 * High-performance 2D graphics rendering system for FastLED WebAssembly applications.
 * Uses WebGL for hardware-accelerated pixel rendering and supports real-time LED strip visualization.
 * Now includes OffscreenCanvas support for background worker rendering.
 *
 * @module GraphicsManager
 */

// Import OffscreenCanvas adapter for unified canvas handling
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

// Shader creation function removed - shaders are now defined inline in initWebGL()
// to avoid DOM dependencies and work properly in both main thread and worker contexts

/**
 * WebGL-based 2D graphics manager for FastLED visualization
 * Provides hardware-accelerated rendering of LED strips using WebGL shaders
 */
export class GraphicsManager {
  /**
   * Creates a new GraphicsManager instance
   * @param {Object} graphicsArgs - Configuration options
   * @param {string|HTMLCanvasElement|OffscreenCanvas} graphicsArgs.canvasId - Canvas element or ID
   * @param {Object} [graphicsArgs.threeJsModules] - Three.js modules (unused but kept for consistency)
   * @param {boolean} [graphicsArgs.usePixelatedRendering=true] - Whether to use pixelated rendering
   * @param {OffscreenCanvas} [graphicsArgs.offscreenCanvas] - Direct OffscreenCanvas instance
   */
  constructor(graphicsArgs) {
    const { canvasId, usePixelatedRendering = true, offscreenCanvas } = graphicsArgs;

    /** @type {string|HTMLCanvasElement|OffscreenCanvas} */
    this.canvasId = canvasId;

    /** @type {HTMLCanvasElement|OffscreenCanvas|null} */
    this.canvas = null;

    /** @type {OffscreenCanvas|null} Direct OffscreenCanvas reference */
    this.offscreenCanvas = offscreenCanvas || null;

    /** @type {import('./offscreen_graphics_adapter.js').OffscreenGraphicsAdapter|null} */
    this.canvasAdapter = null;

    /** @type {WebGLRenderingContext|null} */
    this.gl = null;

    /** @type {WebGLProgram|null} */
    this.program = null;

    /** @type {WebGLBuffer|null} */
    this.positionBuffer = null;

    /** @type {WebGLBuffer|null} */
    this.texCoordBuffer = null;

    /** @type {WebGLTexture|null} */
    this.texture = null;

    /** @type {Uint8Array|null} */
    this.texData = null;

    /** @type {ScreenMapData} */
    this.screenMap = null;

    /** @type {Object} */
    this.args = { usePixelatedRendering };

    /** @type {number} */
    this.texWidth = 0;

    /** @type {number} */
    this.texHeight = 0;

    this.initialize();
  }

  /**
   * Initializes the WebGL context and sets up rendering resources
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

      // Create canvas adapter for unified API
      this.canvasAdapter = createGraphicsAdapter(this.canvas, {
        contextType: 'webgl2', // Prefer WebGL2 for better OffscreenCanvas support
        contextAttributes: {
          alpha: false,
          antialias: false,
          preserveDrawingBuffer: false,
          powerPreference: 'high-performance'
        }
      });

      // Initialize context through adapter
      return this.initializeWithAdapter();

    } catch (error) {
      console.error('Graphics manager initialization failed:', error);
      return false;
    }
  }

  /**
   * Initializes WebGL context through the canvas adapter
   * @returns {Promise<boolean>} True if initialization was successful
   */
  async initializeWithAdapter() {
    try {
      // Create WebGL context through adapter
      this.gl = await this.canvasAdapter.createContext();

      if (!this.gl) {
        console.error('Failed to create WebGL context through adapter');
        return false;
      }

      console.log('WebGL context created successfully', {
        contextType: this.canvasAdapter.activeContextType,
        isOffscreen: this.canvasAdapter.isOffscreen,
        canvasSize: {
          width: this.canvas.width,
          height: this.canvas.height
        }
      });

      return this.initWebGL();

    } catch (error) {
      console.error('Canvas adapter initialization failed:', error);
      return false;
    }
  }

  /**
   * Updates the display with new frame data from FastLED
   * @param {StripData[]} frameData - Array of LED strip data to render
   */
  updateDisplay(frameData) {
    if (!this.gl || !this.canvas) {
      console.warn('Graphics manager not properly initialized');
      return;
    }

    this.clearTexture();
    this.processFrameData(frameData);
    this.render();
  }

  /**
   * Clears the texture data buffer - optimized for recording performance
   */
  clearTexture() {
    if (this.texData) {
      // Use faster TypedArray.fill(0) - already optimal
      this.texData.fill(0);
    }
  }

  /**
   * Processes frame data and updates texture
   * @param {StripData[] & {screenMap?: ScreenMapData}} frameData - Array of LED strip data to render with optional screenMap
   */
  processFrameData(frameData) {
    // Implementation delegated to updateCanvas for now
    this.updateCanvas(frameData);
  }

  /**
   * Renders the current texture to the canvas
   */
  render() {
    if (!this.gl || !this.program) {
      return;
    }

    const canvasWidth = this.gl.canvas.width;
    const canvasHeight = this.gl.canvas.height;

    // Set the viewport
    this.gl.viewport(0, 0, canvasWidth, canvasHeight);
    this.gl.clearColor(0, 0, 0, 1);
    this.gl.clear(this.gl.COLOR_BUFFER_BIT);

    this.gl.useProgram(this.program);

    // Bind position buffer
    const positionLocation = this.gl.getAttribLocation(this.program, 'a_position');
    this.gl.enableVertexAttribArray(positionLocation);
    this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.positionBuffer);
    this.gl.vertexAttribPointer(positionLocation, 2, this.gl.FLOAT, false, 0, 0);

    // Bind texture coordinate buffer
    const texCoordLocation = this.gl.getAttribLocation(this.program, 'a_texCoord');
    this.gl.enableVertexAttribArray(texCoordLocation);
    this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.texCoordBuffer);
    this.gl.vertexAttribPointer(texCoordLocation, 2, this.gl.FLOAT, false, 0, 0);

    // Draw
    this.gl.drawArrays(this.gl.TRIANGLE_STRIP, 0, 4);

    // Handle OffscreenCanvas ImageBitmap transfer for worker mode
    this.handleOffscreenTransfer();
  }

  /**
   * Handles ImageBitmap transfer for OffscreenCanvas rendering
   * Creates and transfers ImageBitmap to main thread when in worker mode
   * @private
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
        // The worker manager will handle the transfer
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
   * Resets and cleans up WebGL resources
   * Disposes of buffers, textures, and programs to free memory
   */
  reset() {
    if (this.gl) {
      this.gl.deleteBuffer(this.positionBuffer);
      this.gl.deleteBuffer(this.texCoordBuffer);
      this.gl.deleteTexture(this.texture);
      this.gl.deleteProgram(this.program);
    }
    this.texWidth = 0;
    this.texHeight = 0;
    this.gl = null;
  }

  /**
   * Initializes the WebGL rendering context and resources
   * Sets up shaders, buffers, and textures for LED rendering
   * @returns {boolean} True if initialization was successful
   */
  initWebGL() {
    // Use the already created context from initializeWithAdapter
    if (!this.gl) {
      console.error('WebGL context not available');
      return false;
    }

    // Get shader sources directly (avoid DOM dependency)
    const vertexShaderSource = `
        attribute vec2 a_position;
        attribute vec2 a_texCoord;
        varying vec2 v_texCoord;
        void main() {
            gl_Position = vec4(a_position, 0, 1);
            v_texCoord = a_texCoord;
        }
    `;

    const fragmentShaderSource = `
        precision mediump float;
        uniform sampler2D u_image;
        varying vec2 v_texCoord;
        void main() {
            gl_FragColor = texture2D(u_image, v_texCoord);
        }
    `;

    // Create shaders
    const vertexShader = this.createShader(
      this.gl.VERTEX_SHADER,
      vertexShaderSource,
    );
    const fragmentShader = this.createShader(
      this.gl.FRAGMENT_SHADER,
      fragmentShaderSource,
    );

    // Create program
    this.program = this.createProgram(vertexShader, fragmentShader);

    // Create buffers
    this.positionBuffer = this.gl.createBuffer();
    this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.positionBuffer);
    this.gl.bufferData(
      this.gl.ARRAY_BUFFER,
      new Float32Array([-1, -1, 1, -1, -1, 1, 1, 1]),
      this.gl.STREAM_DRAW,
    );

    this.texCoordBuffer = this.gl.createBuffer();
    this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.texCoordBuffer);
    this.gl.bufferData(
      this.gl.ARRAY_BUFFER,
      new Float32Array([0, 0, 1, 0, 0, 1, 1, 1]),
      this.gl.STREAM_DRAW,
    );

    // Create texture
    this.texture = this.gl.createTexture();
    this.gl.bindTexture(this.gl.TEXTURE_2D, this.texture);
    this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_WRAP_S, this.gl.CLAMP_TO_EDGE);
    this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_WRAP_T, this.gl.CLAMP_TO_EDGE);
    this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MIN_FILTER, this.gl.NEAREST);
    this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MAG_FILTER, this.gl.NEAREST);

    return true;
  }

  /**
   * Creates and compiles a WebGL shader
   * @param {number} type - Shader type (VERTEX_SHADER or FRAGMENT_SHADER)
   * @param {string} source - GLSL shader source code
   * @returns {WebGLShader|null} Compiled shader or null on error
   */
  createShader(type, source) {
    const shader = this.gl.createShader(type);
    this.gl.shaderSource(shader, source);
    this.gl.compileShader(shader);
    if (!this.gl.getShaderParameter(shader, this.gl.COMPILE_STATUS)) {
      console.error('Shader compile error:', this.gl.getShaderInfoLog(shader));
      this.gl.deleteShader(shader);
      return null;
    }
    return shader;
  }

  /**
   * Creates and links a WebGL program from vertex and fragment shaders
   * @param {WebGLShader} vertexShader - Compiled vertex shader
   * @param {WebGLShader} fragmentShader - Compiled fragment shader
   * @returns {WebGLProgram|null} Linked program or null on error
   */
  createProgram(vertexShader, fragmentShader) {
    const program = this.gl.createProgram();
    this.gl.attachShader(program, vertexShader);
    this.gl.attachShader(program, fragmentShader);
    this.gl.linkProgram(program);
    if (!this.gl.getProgramParameter(program, this.gl.LINK_STATUS)) {
      console.error('Program link error:', this.gl.getProgramInfoLog(program));
      return null;
    }
    return program;
  }

  /**
   * Updates the canvas with new LED frame data
   * Processes strip data and renders LEDs to the WebGL texture
   * @param {StripData[] & {screenMap?: ScreenMapData}} frameData - Array of frame data containing LED strip information with optional screenMap
   */
  updateCanvas(frameData) {
    // Check if frameData is null or invalid
    if (!frameData) {
      console.warn('Received null frame data, skipping update');
      return;
    }

    if (!Array.isArray(frameData)) {
      console.warn('Received non-array frame data:', frameData);
      return;
    }

    if (frameData.length === 0) {
      console.warn('Received empty frame data, skipping update');
      return;
    }

    // Update screenMap from frameData if available
    if (frameData.screenMap) {
      this.screenMap = frameData.screenMap;
    }

    if (!this.gl) this.initWebGL();

    // Update canvas size based on screenMap dimensions
    if (this.screenMap && this.canvas) {
      const screenWidth = this.screenMap.absMax[0] - this.screenMap.absMin[0];
      const screenHeight = this.screenMap.absMax[1] - this.screenMap.absMin[1];

      // Only update canvas size if it's different from current size
      if (this.canvas.width !== screenWidth || this.canvas.height !== screenHeight) {
        this.canvas.width = screenWidth;
        this.canvas.height = screenHeight;

        // Update WebGL viewport to match new canvas size
        if (this.gl) {
          this.gl.viewport(0, 0, screenWidth, screenHeight);
        }
      }
    }

    const canvasWidth = this.gl.canvas.width;
    const canvasHeight = this.gl.canvas.height;

    // Check if we need to reallocate the texture - optimized to reduce reallocations
    const newTexWidth = 2 ** Math.ceil(Math.log2(canvasWidth));
    const newTexHeight = 2 ** Math.ceil(Math.log2(canvasHeight));

    if (this.texWidth !== newTexWidth || this.texHeight !== newTexHeight) {
      this.texWidth = newTexWidth;
      this.texHeight = newTexHeight;

      // Reallocate texture
      this.gl.bindTexture(this.gl.TEXTURE_2D, this.texture);
      this.gl.texImage2D(
        this.gl.TEXTURE_2D,
        0,
        this.gl.RGB,
        this.texWidth,
        this.texHeight,
        0,
        this.gl.RGB,
        this.gl.UNSIGNED_BYTE,
        null,
      );

      // Reallocate texData buffer
      this.texData = new Uint8Array(this.texWidth * this.texHeight * 3);
      console.log(`WebGL texture reallocated: ${this.texWidth}x${this.texHeight} (${(this.texData.length / 1024 / 1024).toFixed(1)}MB)`);
    }

    if (!this.screenMap) {
      console.warn('No screenMap found, skipping update');
      return;
    }

    const { screenMap } = this;

    // Clear the texture data
    this.texData.fill(0);

    for (let i = 0; i < frameData.length; i++) {
      const strip = frameData[i];
      if (!strip) {
        console.warn('Null strip encountered, skipping');
        continue;
      }

      const data = strip.pixel_data;
      if (!data || typeof data.length !== 'number') {
        console.warn('Invalid pixel data for strip:', strip);
        continue;
      }

      const { strip_id } = strip;
      if (!(strip_id in screenMap.strips)) {
        console.warn(`No screen map found for strip ID ${strip_id}, skipping update`);
        continue;
      }
      const stripData = screenMap.strips[strip_id];
      const pixelCount = data.length / 3;
      const { map } = stripData;
      const min_x = screenMap.absMin[0];
      const min_y = screenMap.absMin[1];
      const x_array = map.x;
      const y_array = map.y;
      const len = Math.min(x_array.length, y_array.length);
      // log("Writing data to canvas");
      for (let i = 0; i < pixelCount; i++) { // eslint-disable-line
        if (i >= len) {
          console.warn(
            `Strip ${strip_id}: Pixel ${i} is outside the screen map ${map.length}, skipping update`,
          );
          continue;
        }
        // let [x, y] = map[i];
        let x = x_array[i];
        let y = y_array[i];
        x -= min_x;
        y -= min_y;

        // Can't access the texture with floating point.
        x = Number.parseInt(x, 10);
        y = Number.parseInt(y, 10);

        // check to make sure that the pixel is within the canvas
        if (x < 0 || x >= canvasWidth || y < 0 || y >= canvasHeight) {
          console.warn(
            `Strip ${strip_id}: Pixel ${i} is outside the canvas at ${x}, ${y}, skipping update`,
          );
          continue;
        }
        // log(x, y);
        const diameter = stripData.diameter || 1.0;
        const radius = Math.floor(diameter / 2);

        // Draw a filled square for each LED
        for (let dy = -radius; dy <= radius; dy++) {
          for (let dx = -radius; dx <= radius; dx++) {
            const px = x + dx;
            const py = y + dy;

            // Check bounds
            if (px >= 0 && px < canvasWidth && py >= 0 && py < canvasHeight) {
              const srcIndex = i * 3;
              const destIndex = (py * this.texWidth + px) * 3;
              // Pixel data is already in 0-255 range, use directly
              const r = data[srcIndex] & 0xFF; // eslint-disable-line
              const g = data[srcIndex + 1] & 0xFF; // eslint-disable-line
              const b = data[srcIndex + 2] & 0xFF; // eslint-disable-line
              this.texData[destIndex] = r;
              this.texData[destIndex + 1] = g;
              this.texData[destIndex + 2] = b;
            }
          }
        }
      }
    }

    // Update texture with new data
    this.gl.bindTexture(this.gl.TEXTURE_2D, this.texture);
    this.gl.texSubImage2D(
      this.gl.TEXTURE_2D,
      0,
      0,
      0,
      this.texWidth,
      this.texHeight,
      this.gl.RGB,
      this.gl.UNSIGNED_BYTE,
      this.texData,
    );

    // Set the viewport to the original canvas size
    this.gl.viewport(0, 0, canvasWidth, canvasHeight);
    this.gl.clearColor(0, 0, 0, 1);
    this.gl.clear(this.gl.COLOR_BUFFER_BIT);

    this.gl.useProgram(this.program);

    const positionLocation = this.gl.getAttribLocation(this.program, 'a_position');
    this.gl.enableVertexAttribArray(positionLocation);
    this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.positionBuffer);
    this.gl.vertexAttribPointer(positionLocation, 2, this.gl.FLOAT, false, 0, 0);

    const texCoordLocation = this.gl.getAttribLocation(this.program, 'a_texCoord');
    this.gl.enableVertexAttribArray(texCoordLocation);
    this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.texCoordBuffer);
    this.gl.vertexAttribPointer(texCoordLocation, 2, this.gl.FLOAT, false, 0, 0);

    // Update texture coordinates based on actual canvas size
    const texCoords = new Float32Array([
      0,
      0,
      canvasWidth / this.texWidth,
      0,
      0,
      canvasHeight / this.texHeight,
      canvasWidth / this.texWidth,
      canvasHeight / this.texHeight,
    ]);
    this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.texCoordBuffer);
    this.gl.bufferData(this.gl.ARRAY_BUFFER, texCoords, this.gl.STREAM_DRAW);

    this.gl.drawArrays(this.gl.TRIANGLE_STRIP, 0, 4);
  }

  /**
   * Resizes the canvas and updates WebGL viewport
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
        if (this.gl) {
          this.gl.viewport(0, 0, width, height);
        }
      }

      console.log('Graphics manager resized', {
        width,
        height,
        isOffscreen: this.canvasAdapter?.isOffscreen || false
      });

      return true;

    } catch (error) {
      console.error('Graphics manager resize failed:', error);
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
      textureSize: {
        width: this.texWidth,
        height: this.texHeight
      },
      hasWebGL: !!this.gl,
      initialized: !!(this.gl && this.program)
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
   * Clean up resources including canvas adapter
   */
  destroy() {
    // Clean up WebGL resources
    if (this.gl) {
      if (this.program) {
        this.gl.deleteProgram(this.program);
      }
      if (this.positionBuffer) {
        this.gl.deleteBuffer(this.positionBuffer);
      }
      if (this.texCoordBuffer) {
        this.gl.deleteBuffer(this.texCoordBuffer);
      }
      if (this.texture) {
        this.gl.deleteTexture(this.texture);
      }
    }

    // Clean up canvas adapter
    if (this.canvasAdapter) {
      this.canvasAdapter.destroy();
      this.canvasAdapter = null;
    }

    this.gl = null;
    this.canvas = null;
    this.program = null;
    this.positionBuffer = null;
    this.texCoordBuffer = null;
    this.texture = null;
    this.texData = null;

    console.log('Graphics manager destroyed');
  }
}
