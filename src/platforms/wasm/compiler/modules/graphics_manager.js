/**
 * FastLED Graphics Manager - 2D WebGL Renderer
 *
 * Provides fast 2D rendering of LED layouts using WebGL for optimal performance.
 * This renderer is optimized for dense LED grids and high frame rates.
 *
 * Key features:
 * - WebGL-based texture rendering for performance
 * - Power-of-2 texture allocation for compatibility
 * - Pixel-perfect LED positioning and sizing
 * - Automatic canvas and texture management
 *
 * @module GraphicsManager
 */

/* eslint-disable no-console */
/* eslint-disable import/prefer-default-export */
/* eslint-disable no-restricted-syntax */
/* eslint-disable max-len */
/* eslint-disable guard-for-in */
/* eslint-disable camelcase */
/* eslint-disable no-plusplus */
/* eslint-disable no-continue */

/**
 * Creates and injects WebGL shaders into the document head
 * Ensures shaders are only created once by checking for existing elements
 */
function createShaders() {
  const fragmentShaderId = 'fastled_FragmentShader';
  const vertexShaderId = 'fastled_vertexShader';
  if (document.getElementById(fragmentShaderId) && document.getElementById(vertexShaderId)) {
    return;
  }
  const vertexShaderStr = `
        attribute vec2 a_position;
        attribute vec2 a_texCoord;
        varying vec2 v_texCoord;
        void main() {
            gl_Position = vec4(a_position, 0, 1);
            v_texCoord = a_texCoord;
        }
        `;

  const fragmentShaderStr = `
        precision mediump float;
        uniform sampler2D u_image;
        varying vec2 v_texCoord;
        void main() {
            gl_FragColor = texture2D(u_image, v_texCoord);
        }
        `;
  const fragmentShader = document.createElement('script');
  const vertexShader = document.createElement('script');
  fragmentShader.id = fragmentShaderId;
  vertexShader.id = vertexShaderId;
  fragmentShader.type = 'x-shader/x-fragment';
  vertexShader.type = 'x-shader/x-vertex';
  fragmentShader.text = fragmentShaderStr;
  vertexShader.text = vertexShaderStr;
  document.head.appendChild(fragmentShader);
  document.head.appendChild(vertexShader);
}

/**
 * Fast 2D Graphics Manager using WebGL for LED rendering
 * Optimized for dense LED grids and high frame rates
 */
export class GraphicsManager {
  /**
   * Creates a new GraphicsManager instance
   * @param {Object} graphicsArgs - Configuration arguments
   * @param {string} graphicsArgs.canvasId - ID of the HTML canvas element to render to
   */
  constructor(graphicsArgs) {
    const { canvasId } = graphicsArgs;

    /** @type {string} HTML canvas element ID */
    this.canvasId = canvasId;

    /** @type {WebGLRenderingContext|null} WebGL rendering context */
    this.gl = null;

    /** @type {WebGLProgram|null} Compiled shader program */
    this.program = null;

    /** @type {WebGLBuffer|null} Vertex position buffer */
    this.positionBuffer = null;

    /** @type {WebGLBuffer|null} Texture coordinate buffer */
    this.texCoordBuffer = null;

    /** @type {WebGLTexture|null} Main rendering texture */
    this.texture = null;

    /** @type {number} Current texture width (power of 2) */
    this.texWidth = 0;

    /** @type {number} Current texture height (power of 2) */
    this.texHeight = 0;

    /** @type {Uint8Array|null} Texture data buffer (RGB) */
    this.texData = null;
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
   */
  initWebGL() {
    createShaders();
    const canvas = document.getElementById(this.canvasId);
    this.gl = canvas.getContext('webgl');
    if (!this.gl) {
      console.error('WebGL not supported');
      return;
    }

    // Create shaders
    const vertexShader = this.createShader(
      this.gl.VERTEX_SHADER,
      document.getElementById('fastled_vertexShader').text,
    );
    const fragmentShader = this.createShader(
      this.gl.FRAGMENT_SHADER,
      document.getElementById('fastled_FragmentShader').text,
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
   * @param {Array<Object>} frameData - Array of LED strip data
   * @param {Object} frameData[].strip_id - Strip identifier
   * @param {Uint8Array} frameData[].pixel_data - RGB pixel data (3 bytes per pixel)
   * @param {Object} frameData.screenMap - Screen coordinate mapping data
   */
  updateCanvas(frameData) {
    if (frameData.length === 0) {
      console.warn('Received empty frame data, skipping update');
      return;
    }

    if (!this.gl) this.initWebGL();

    const canvasWidth = this.gl.canvas.width;
    const canvasHeight = this.gl.canvas.height;

    // Check if we need to reallocate the texture
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
    }

    const { screenMap } = frameData;

    // Clear the texture data
    this.texData.fill(0);

    for (let i = 0; i < frameData.length; i++) {
      const strip = frameData[i];
      const data = strip.pixel_data;
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
}
