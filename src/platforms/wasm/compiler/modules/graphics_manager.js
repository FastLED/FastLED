/// <reference path="../types.d.ts" />

/**
 * FastLED Graphics Manager Module
 *
 * High-performance 2D graphics rendering system for FastLED WebAssembly applications.
 * Uses Three.js for hardware-accelerated tile-based LED rendering and supports real-time LED strip visualization.
 * Now includes OffscreenCanvas support for background worker rendering.
 *
 * @module GraphicsManager
 */

// Import base class for unified Three.js functionality
import { GraphicsManagerBase } from './graphics_manager_base.js';

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
 * Three.js-based 2D graphics manager for FastLED visualization
 * Provides hardware-accelerated tile rendering of LED strips using Three.js
 */
export class GraphicsManager extends GraphicsManagerBase {
  /**
   * Creates a new GraphicsManager instance
   * @param {Object} graphicsArgs - Configuration options
   * @param {string|HTMLCanvasElement|OffscreenCanvas} graphicsArgs.canvasId - Canvas element or ID
   * @param {Object} graphicsArgs.threeJsModules - Three.js modules for tile rendering
   * @param {boolean} [graphicsArgs.usePixelatedRendering=true] - Whether to use pixelated rendering
   * @param {OffscreenCanvas} [graphicsArgs.offscreenCanvas] - Direct OffscreenCanvas instance
   */
  constructor(graphicsArgs) {
    const { threeJsModules, usePixelatedRendering = true } = graphicsArgs;

    // Call parent constructor
    super(graphicsArgs);

    if (!threeJsModules) {
      throw new Error('ThreeJS modules are required for GraphicsManager');
    }

    /** @type {Object} Configuration options */
    this.args = { usePixelatedRendering };

    /** @type {Object|null} Three.js texture for tile rendering */
    this.texture = null;

    /** @type {Object|null} Three.js material for tile rendering */
    this.material = null;

    /** @type {Object|null} Three.js plane geometry for displaying the texture */
    this.planeGeometry = null;

    /** @type {Object|null} Three.js mesh for the tile surface */
    this.planeMesh = null;

    /** @type {Uint8Array|null} */
    this.texData = null;

    /** @type {number} */
    this.texWidth = 0;

    /** @type {number} */
    this.texHeight = 0;
  }

  /**
   * Initializes the Three.js context and sets up tile rendering resources
   * @returns {boolean|Promise<boolean>} True if initialization was successful
   */
  initialize() {
    // Call parent initialization
    const baseInitialized = super.initialize();
    if (!baseInitialized) {
      return false;
    }

    // Additional tile-specific initialization will be done in initThreeJS
    return true;
  }

  /**
   * Initializes the Three.js rendering environment for tile-based rendering
   * @param {Object} frameData - The frame data containing screen mapping information
   * @returns {boolean} True if initialization was successful
   */
  initThreeJS(frameData) {
    try {
      this._setupCanvasAndDimensions(frameData);
      this._setupScene();
      this._setupRenderer();
      this._setupTileRenderingSystem();

      // Update camera projection matrix
      if (this.camera) {
        this.camera.updateProjectionMatrix();
      }

      console.log('Three.js tile rendering initialized successfully', {
        isOffscreen: this.canvasAdapter?.isOffscreen || false,
        canvasSize: {
          width: this.canvas.width,
          height: this.canvas.height
        }
      });

      return true;
    } catch (error) {
      console.error('Three.js tile rendering initialization failed:', error);
      return false;
    }
  }

  /**
   * Updates the display with new frame data from FastLED
   * @param {StripData[]} frameData - Array of LED strip data to render
   */
  updateDisplay(frameData) {
    if (!this.scene || !this.camera || !this.renderer) {
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
   * Renders the current texture to the canvas using Three.js
   */
  render() {
    if (!this.renderer || !this.scene || !this.camera) {
      return;
    }

    // Render the scene
    this.renderer.render(this.scene, this.camera);

    // Handle OffscreenCanvas ImageBitmap transfer for worker mode
    this.handleOffscreenTransfer();
  }


  /**
   * Sets up the tile rendering system with Three.js texture
   * @private
   */
  _setupTileRenderingSystem() {
    const { THREE } = this.threeJsModules;

    // Create a plane geometry to display the texture
    this.planeGeometry = new THREE.PlaneGeometry(this.SCREEN_WIDTH, this.SCREEN_HEIGHT);

    // Create texture for tile rendering
    this.texture = new THREE.DataTexture(
      null, // data will be set later
      1, 1, // initial size
      THREE.RGBFormat,
      THREE.UnsignedByteType
    );

    // Set texture parameters for pixel-perfect rendering
    this.texture.magFilter = this.args.usePixelatedRendering ? THREE.NearestFilter : THREE.LinearFilter;
    this.texture.minFilter = this.args.usePixelatedRendering ? THREE.NearestFilter : THREE.LinearFilter;
    this.texture.wrapS = THREE.ClampToEdgeWrapping;
    this.texture.wrapT = THREE.ClampToEdgeWrapping;

    // Create material with the texture
    this.material = new THREE.MeshBasicMaterial({
      map: this.texture,
      transparent: false
    });

    // Create mesh and add to scene
    this.planeMesh = new THREE.Mesh(this.planeGeometry, this.material);
    this.planeMesh.position.set(0, 0, 0);
    this.scene.add(this.planeMesh);
  }

  /**
   * Resets and cleans up Three.js resources
   * Disposes of textures, materials, and geometries to free memory
   */
  reset() {
    // Call parent reset
    super.reset();

    // Clean up tile-specific resources
    if (this.texture) {
      this.texture.dispose();
      this.texture = null;
    }
    if (this.material) {
      this.material.dispose();
      this.material = null;
    }
    if (this.planeGeometry) {
      this.planeGeometry.dispose();
      this.planeGeometry = null;
    }
    if (this.planeMesh && this.scene) {
      this.scene.remove(this.planeMesh);
      this.planeMesh = null;
    }

    this.texWidth = 0;
    this.texHeight = 0;
    this.texData = null;
  }




  /**
   * Updates the canvas with new LED frame data
   * Processes strip data and renders LEDs to the Three.js texture using tile rendering
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

    // Initialize Three.js if not already done
    if (!this.scene || !this.camera || !this.renderer) {
      this.initThreeJS(frameData);
    }

    // Update canvas size based on screenMap dimensions
    if (this.screenMap && this.canvas) {
      const screenWidth = this.screenMap.absMax[0] - this.screenMap.absMin[0];
      const screenHeight = this.screenMap.absMax[1] - this.screenMap.absMin[1];

      // Only update canvas size if it's different from current size
      if (this.canvas.width !== screenWidth || this.canvas.height !== screenHeight) {
        this.canvas.width = screenWidth;
        this.canvas.height = screenHeight;

        // Update Three.js renderer size
        if (this.renderer) {
          this.renderer.setSize(screenWidth, screenHeight);
        }
      }
    }

    const canvasWidth = this.canvas.width;
    const canvasHeight = this.canvas.height;

    // Check if we need to reallocate the texture - optimized to reduce reallocations
    const newTexWidth = 2 ** Math.ceil(Math.log2(canvasWidth));
    const newTexHeight = 2 ** Math.ceil(Math.log2(canvasHeight));

    if (this.texWidth !== newTexWidth || this.texHeight !== newTexHeight) {
      this.texWidth = newTexWidth;
      this.texHeight = newTexHeight;

      // Reallocate Three.js texture and data buffer
      this.texData = new Uint8Array(this.texWidth * this.texHeight * 3);

      if (this.texture) {
        this.texture.dispose(); // Clean up old texture
      }

      // Create new Three.js texture
      const { THREE } = this.threeJsModules;
      this.texture = new THREE.DataTexture(
        this.texData,
        this.texWidth,
        this.texHeight,
        THREE.RGBFormat,
        THREE.UnsignedByteType
      );

      // Set texture parameters for pixel-perfect rendering
      this.texture.magFilter = this.args.usePixelatedRendering ? THREE.NearestFilter : THREE.LinearFilter;
      this.texture.minFilter = this.args.usePixelatedRendering ? THREE.NearestFilter : THREE.LinearFilter;
      this.texture.wrapS = THREE.ClampToEdgeWrapping;
      this.texture.wrapT = THREE.ClampToEdgeWrapping;

      // Update material with new texture
      if (this.material) {
        this.material.map = this.texture;
        this.material.needsUpdate = true;
      }

      console.log(`Three.js texture reallocated: ${this.texWidth}x${this.texHeight} (${(this.texData.length / 1024 / 1024).toFixed(1)}MB)`);
    }

    if (!this.screenMap) {
      console.warn('No screenMap found, skipping update');
      return;
    }

    const { screenMap } = this;

    // Clear the texture data
    if (this.texData) {
      this.texData.fill(0);
    }

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

    // Update Three.js texture with new data
    if (this.texture && this.texData) {
      // Update texture size if needed
      this.texture.image = {
        data: this.texData,
        width: this.texWidth,
        height: this.texHeight
      };
      this.texture.needsUpdate = true;
    }
  }

  /**
   * Gets performance statistics including canvas adapter info
   * @returns {Object} Performance and capability information
   */
  getPerformanceInfo() {
    const baseInfo = super.getPerformanceInfo();

    return {
      ...baseInfo,
      textureSize: {
        width: this.texWidth,
        height: this.texHeight
      },
      renderingMode: 'tile-based',
      usePixelatedRendering: this.args.usePixelatedRendering
    };
  }

  /**
   * Clean up resources including Three.js tile rendering objects
   */
  destroy() {
    // Clean up tile-specific resources
    this.reset();

    // Call parent destroy
    super.destroy();

    console.log('Tile-based graphics manager destroyed');
  }
}
