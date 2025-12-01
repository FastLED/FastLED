/// <reference path="../../types.d.ts" />

/**
 * FastLED Graphics Manager Module
 *
 * High-performance 2D graphics rendering system for FastLED WebAssembly applications.
 * Uses WebGL for hardware-accelerated pixel rendering and supports real-time LED strip visualization.
 *
 * @module GraphicsManager
 */

/* eslint-disable no-console */
/* eslint-disable no-restricted-syntax */
/* eslint-disable max-len */
/* eslint-disable guard-for-in */
/* eslint-disable camelcase */
/* eslint-disable no-plusplus */
/* eslint-disable no-continue */

import { computeScreenMapBounds } from './graphics_utils.js';

/**
 * @typedef {Object} StripData
 * @property {number} strip_id - Unique identifier for the LED strip
 * @property {Uint8Array} pixel_data - Raw pixel data (RGB values)
 * @property {number} length - Number of LEDs in the strip
 */

/**
 * @typedef {Object} ScreenMapData
 * @property {number[]} [absMax] - Maximum coordinates array (computed on-demand)
 * @property {number[]} [absMin] - Minimum coordinates array (computed on-demand)
 * @property {{ [key: string]: any }} strips - Strip configuration data
 */

/**
 * @typedef {Object} FrameData
 * @property {number} strip_id - Strip identifier
 * @property {Uint8Array} pixel_data - RGB pixel data (3 bytes per pixel)
 * @property {ScreenMapData} screenMap - Screen coordinate mapping data
 */

/**
 * Creates and injects WebGL shaders into the document head
 * Ensures shaders are only created once by checking for existing elements
 */
// Shader source code (used by both main thread and worker)
const VERTEX_SHADER_SRC = `
        attribute vec2 a_position;
        attribute vec2 a_texCoord;
        varying vec2 v_texCoord;
        void main() {
            gl_Position = vec4(a_position, 0, 1);
            v_texCoord = a_texCoord;
        }
        `;

const FRAGMENT_SHADER_SRC = `
        precision mediump float;
        uniform sampler2D u_image;
        varying vec2 v_texCoord;
        void main() {
            gl_FragColor = texture2D(u_image, v_texCoord);
        }
        `;

function createShaders() {
  const fragmentShaderId = 'fastled_FragmentShader';
  const vertexShaderId = 'fastled_vertexShader';

  // In worker context, document is not available - shaders will be used directly
  if (typeof document === 'undefined') {
    return { vertex: VERTEX_SHADER_SRC, fragment: FRAGMENT_SHADER_SRC };
  }

  // Main thread: inject shaders into DOM if not already present
  if (document.getElementById(fragmentShaderId) && document.getElementById(vertexShaderId)) {
    return { vertex: VERTEX_SHADER_SRC, fragment: FRAGMENT_SHADER_SRC };
  }

  const fragmentShader = document.createElement('script');
  const vertexShader = document.createElement('script');
  fragmentShader.id = fragmentShaderId;
  vertexShader.id = vertexShaderId;
  fragmentShader.type = 'x-shader/x-fragment';
  vertexShader.type = 'x-shader/x-vertex';
  fragmentShader.text = FRAGMENT_SHADER_SRC;
  vertexShader.text = VERTEX_SHADER_SRC;
  document.head.appendChild(fragmentShader);
  document.head.appendChild(vertexShader);

  return { vertex: VERTEX_SHADER_SRC, fragment: FRAGMENT_SHADER_SRC };
}

/**
 * WebGL-based 2D graphics manager for FastLED visualization
 * Provides hardware-accelerated rendering of LED strips using WebGL shaders
 */
export class GraphicsManager {
  /**
   * Creates a new GraphicsManager instance
   * @param {Object} graphicsArgs - Configuration options
   * @param {string} [graphicsArgs.canvasId] - ID of the canvas element to render to (main thread)
   * @param {HTMLCanvasElement|OffscreenCanvas} [graphicsArgs.canvas] - Canvas object directly (worker thread)
   * @param {Object} [graphicsArgs.threeJsModules] - Three.js modules (unused but kept for consistency)
   * @param {boolean} [graphicsArgs.usePixelatedRendering=true] - Whether to use pixelated rendering
   */
  constructor(graphicsArgs) {
    const { canvasId, canvas, usePixelatedRendering = true } = graphicsArgs;

    /** @type {string|HTMLCanvasElement|OffscreenCanvas} */
    this.canvasId = canvas || canvasId;

    /** @type {HTMLCanvasElement|OffscreenCanvas|null} */
    this.canvas = null;

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

    /** @type {Object} Dictionary of screenmaps (stripId → screenmap object) */
    this.screenMaps = {};

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
   * @returns {boolean} True if initialization was successful
   */
  initialize() {
    // Check if canvasId is actually a canvas object (OffscreenCanvas or HTMLCanvasElement)
    // @ts-ignore - OffscreenCanvas type checking
    const isOffscreenCanvas = typeof OffscreenCanvas !== 'undefined' && this.canvasId instanceof OffscreenCanvas;
    // @ts-ignore - HTMLCanvasElement type checking
    const isHTMLCanvas = typeof HTMLCanvasElement !== 'undefined' && this.canvasId instanceof HTMLCanvasElement;

    if (isOffscreenCanvas || isHTMLCanvas) {
      // Worker context or direct canvas object: use canvas directly
      this.canvas = /** @type {HTMLCanvasElement|OffscreenCanvas} */ (this.canvasId);
    } else if (typeof this.canvasId === 'string') {
      // Main thread: look up canvas by ID
      this.canvas = /** @type {HTMLCanvasElement|OffscreenCanvas} */ (document.getElementById(this.canvasId));
      if (!this.canvas) {
        console.error(`Canvas with id ${this.canvasId} not found`);
        return false;
      }
    } else {
      console.error('Invalid canvas parameter: must be string ID, HTMLCanvasElement, or OffscreenCanvas');
      return false;
    }

    this.gl = this.canvas.getContext('webgl');
    if (!this.gl) {
      console.error('WebGL not supported');
      return false;
    }

    return this.initWebGL();
  }

  /**
   * Updates the screenMaps data when it changes (event-driven)
   * Called directly from worker's handleScreenMapUpdate()
   * @param {Object} screenMapsData - Dictionary of screenmaps (stripId → screenmap object)
   */
  updateScreenMap(screenMapsData) {
    this.screenMaps = screenMapsData;
    console.log('[GraphicsManager] ScreenMaps updated', {
      screenMapCount: Object.keys(screenMapsData || {}).length
    });
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
    const shaders = createShaders();

    // Use this.canvas which was already set up in initialize()
    this.gl = this.canvas.getContext('webgl');
    if (!this.gl) {
      console.error('WebGL not supported');
      return false;
    }

    // Create shaders using the source strings
    const vertexShader = this.createShader(
      this.gl.VERTEX_SHADER,
      shaders.vertex,
    );
    const fragmentShader = this.createShader(
      this.gl.FRAGMENT_SHADER,
      shaders.fragment,
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

    // ScreenMap is updated via push notifications (updateScreenMap() method)
    // frameData now only contains pixel data, no screenMap property

    if (!this.gl) this.initWebGL();

    // Update canvas size based on composite screenMap dimensions
    if (Object.keys(this.screenMaps).length > 0 && this.canvas) {
      // Calculate composite bounds across all screenmaps
      let globalMinX = Infinity, globalMinY = Infinity;
      let globalMaxX = -Infinity, globalMaxY = -Infinity;

      for (const screenMap of Object.values(this.screenMaps)) {
        const bounds = computeScreenMapBounds(screenMap);
        globalMinX = Math.min(globalMinX, bounds.absMin[0]);
        globalMinY = Math.min(globalMinY, bounds.absMin[1]);
        globalMaxX = Math.max(globalMaxX, bounds.absMax[0]);
        globalMaxY = Math.max(globalMaxY, bounds.absMax[1]);
      }

      const screenWidth = globalMaxX - globalMinX;
      const screenHeight = globalMaxY - globalMinY;

      // Only update canvas size if it's different from current size
      if (this.canvas.width !== screenWidth || this.canvas.height !== screenHeight) {
        // Try to resize canvas, but skip if it's been transferred to worker
        try {
          this.canvas.width = screenWidth;
          this.canvas.height = screenHeight;
        } catch (error) {
          // Canvas has been transferred to offscreen worker - this is expected
          console.log('Canvas resize skipped - canvas is controlled by worker');
          return;
        }

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

    if (Object.keys(this.screenMaps).length === 0) {
      console.warn('No screenMaps found, skipping update');
      return;
    }

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

      // Get screenmap for this specific strip
      const screenMap = this.screenMaps[strip_id];
      if (!screenMap) {
        console.warn(`No screenMap found for strip ${strip_id}`);
        continue;
      }

      // Look up strip data in its screenmap
      if (!(strip_id in screenMap.strips)) {
        console.warn(`Strip ${strip_id} not found in its screenMap`);
        continue;
      }

      const stripData = screenMap.strips[strip_id];
      const pixelCount = data.length / 3;
      const { map } = stripData;
      const bounds = computeScreenMapBounds(screenMap);
      const min_x = bounds.absMin[0];
      const min_y = bounds.absMin[1];
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
}
