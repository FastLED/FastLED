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

    /** @type {boolean} Whether to clear texture background (for sparse LED layouts) */
    this.needsBackgroundClear = true;

    /** @type {Map} Cached position calculations per strip */
    this.positionCache = new Map();

    /** @type {number} Count bounds warnings to prevent spam */
    this.boundsWarningCount = 0;
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
    if (this.texData && this.needsBackgroundClear) {
      // Only clear if we have sparse LEDs that need black background
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

    // Ensure plane mesh exists before rendering
    if (!this.planeMesh) {
      console.warn('No plane mesh to render');
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

    // Don't create plane geometry here - we'll create it once we know the actual texture size
    this.planeGeometry = null;

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

    // Don't create mesh yet - we'll create it when we know the texture dimensions
    this.planeMesh = null;
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
    this.positionCache.clear();
  }

  /**
   * Builds position cache for a strip to avoid redundant calculations
   * @private
   * @param {number} stripId - Strip ID
   * @param {Object} stripData - Strip mapping data
   * @param {number} minX - Minimum X coordinate
   * @param {number} minY - Minimum Y coordinate
   * @param {number} canvasWidth - Canvas width for bounds calculation
   * @param {number} canvasHeight - Canvas height for bounds calculation
   * @returns {Array} Array of cached position objects with drawing bounds
   */
  _buildPositionCache(stripId, stripData, minX, minY, canvasWidth, canvasHeight) {
    const cacheKey = `${stripId}_${minX}_${minY}_${canvasWidth}_${canvasHeight}`;

    if (this.positionCache.has(cacheKey)) {
      return this.positionCache.get(cacheKey);
    }

    const { map } = stripData;
    const x_array = map.x;
    const y_array = map.y;
    const len = Math.min(x_array.length, y_array.length);
    const diameter = stripData.diameter || 1.0;
    const radius = Math.floor(diameter / 2);
    const positions = [];

    for (let i = 0; i < len; i++) {
      const x = Number.parseInt(x_array[i] - minX, 10);
      const y = Number.parseInt(y_array[i] - minY, 10);

      // Pre-calculate drawing bounds to avoid per-frame calculations
      const minDx = Math.max(-radius, -x);
      const maxDx = Math.min(radius, canvasWidth - 1 - x);
      const minDy = Math.max(-radius, -y);
      const maxDy = Math.min(radius, canvasHeight - 1 - y);

      positions.push({
        x,
        y,
        minDx,
        maxDx,
        minDy,
        maxDy,
        radius
      });
    }

    this.positionCache.set(cacheKey, positions);
    return positions;
  }

  /**
   * Updates background clear flag based on LED density
   * @private
   * @param {Object} frameData - Frame data with screen mapping
   */
  _updateBackgroundClearFlag(frameData) {
    if (!this.screenMap || !frameData || !Array.isArray(frameData)) {
      return;
    }

    // Calculate total LEDs and screen area
    let totalLeds = 0;
    for (const strip of frameData) {
      if (strip && strip.pixel_data) {
        totalLeds += strip.pixel_data.length / 3;
      }
    }

    const screenWidth = this.screenMap.absMax[0] - this.screenMap.absMin[0] + 1;
    const screenHeight = this.screenMap.absMax[1] - this.screenMap.absMin[1] + 1;
    const screenArea = screenWidth * screenHeight;
    const ledDensity = totalLeds / screenArea;

    // If LED density is high (>50% coverage), skip clearing for performance
    this.needsBackgroundClear = ledDensity < 0.5;

    console.log(`LED density: ${(ledDensity * 100).toFixed(1)}% (${totalLeds}/${screenArea}), clearing: ${this.needsBackgroundClear}`);
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

      // Determine if we need background clearing based on LED density
      this._updateBackgroundClearFlag(frameData);
    }

    // Initialize Three.js if not already done
    if (!this.scene || !this.camera || !this.renderer) {
      this.initThreeJS(frameData);
    }

    // Update canvas size based on screenMap dimensions
    if (this.screenMap && this.canvas) {
      // Add 1 to include the boundary coordinates (e.g., 0 to 63 = 64 pixels wide)
      const screenWidth = this.screenMap.absMax[0] - this.screenMap.absMin[0] + 1;
      const screenHeight = this.screenMap.absMax[1] - this.screenMap.absMin[1] + 1;

      // Only update canvas size if it's different from current size
      if (this.canvas.width !== screenWidth || this.canvas.height !== screenHeight) {
        console.error(`🔍 CANVAS UPDATE: ${this.canvas.width}x${this.canvas.height} → ${screenWidth}x${screenHeight}`);
        this.canvas.width = screenWidth;
        this.canvas.height = screenHeight;

        // Update Three.js renderer size
        if (this.renderer) {
          // Use the base class helper for OffscreenCanvas compatibility
          this._setRendererSize(screenWidth, screenHeight);
        }
      }
    }

    const canvasWidth = this.canvas.width;
    const canvasHeight = this.canvas.height;

    // Check if we need to reallocate the texture - optimized to reduce reallocations
    // Use actual canvas dimensions instead of power-of-2 when WebGL2 supports NPOT textures
    const newTexWidth = canvasWidth;
    const newTexHeight = canvasHeight;

    // Only reallocate if texture grows (never shrink to avoid thrashing)
    const needsReallocation = !this.texData ||
                             newTexWidth > this.texWidth ||
                             newTexHeight > this.texHeight;

    if (needsReallocation) {
      const oldSize = this.texData ? this.texData.length : 0;
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

      // Create or update plane geometry to match texture size
      // Remove old mesh if exists
      if (this.planeMesh && this.scene) {
        this.scene.remove(this.planeMesh);
        this.planeMesh = null;
      }

      // Dispose old geometry if exists
      if (this.planeGeometry) {
        this.planeGeometry.dispose();
      }

      // Create new plane geometry that matches the texture dimensions
      // Use the actual texture dimensions directly
      this.planeGeometry = new THREE.PlaneGeometry(this.texWidth, this.texHeight);

      // Create new mesh with updated geometry
      this.planeMesh = new THREE.Mesh(this.planeGeometry, this.material);
      this.planeMesh.position.set(0, 0, 0);
      this.scene.add(this.planeMesh);

      // Update camera to fit the new texture dimensions
      if (this.camera && this.camera.isOrthographicCamera) {
        const halfWidth = this.texWidth / 2;
        const halfHeight = this.texHeight / 2;
        const MARGIN = 1.05;

        this.camera.left = -halfWidth * MARGIN;
        this.camera.right = halfWidth * MARGIN;
        this.camera.top = halfHeight * MARGIN;
        this.camera.bottom = -halfHeight * MARGIN;
        this.camera.updateProjectionMatrix();
      }

      console.log(`Three.js texture reallocated: ${this.texWidth}x${this.texHeight} (${(this.texData.length / 1024 / 1024).toFixed(1)}MB), saved ${((oldSize - this.texData.length) / 1024 / 1024).toFixed(1)}MB`);
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
      const min_x = screenMap.absMin[0];
      const min_y = screenMap.absMin[1];

      // Get cached positions for this strip with pre-calculated drawing bounds
      const positions = this._buildPositionCache(strip_id, stripData, min_x, min_y, canvasWidth, canvasHeight);

      // log("Writing data to canvas");
      for (let i = 0; i < pixelCount; i++) { // eslint-disable-line
        if (i >= positions.length) {
          console.warn(
            `Strip ${strip_id}: Pixel ${i} is outside the screen map ${positions.length}, skipping update`,
          );
          continue;
        }

        // Use cached position calculations with pre-calculated bounds
        const posData = positions[i];
        const { x, y, minDx, maxDx, minDy, maxDy } = posData;

        // check to make sure that the pixel is within the canvas
        if (x < 0 || x >= canvasWidth || y < 0 || y >= canvasHeight) {
          this.boundsWarningCount++;
          if (this.boundsWarningCount <= 5) {
            console.error(
              `🚨 BOUNDS ERROR #${this.boundsWarningCount}: Strip ${strip_id} Pixel ${i} at ${x},${y} outside canvas ${canvasWidth}x${canvasHeight}`
            );
            if (this.boundsWarningCount === 5) {
              console.error(`🚨 Suppressing further bounds warnings...`);
            }
          }
          continue;
        }

        // Pre-calculate color data outside inner loops
        const srcIndex = i * 3;
        const r = data[srcIndex] & 0xFF; // eslint-disable-line
        const g = data[srcIndex + 1] & 0xFF; // eslint-disable-line
        const b = data[srcIndex + 2] & 0xFF; // eslint-disable-line

        // Draw a filled square for each LED with pre-calculated bounds (no bounds checking needed!)
        for (let dy = minDy; dy <= maxDy; dy++) {
          const py = y + dy;
          const rowOffset = py * this.texWidth;
          for (let dx = minDx; dx <= maxDx; dx++) {
            const px = x + dx;
            const destIndex = (rowOffset + px) * 3;
            this.texData[destIndex] = r;
            this.texData[destIndex + 1] = g;
            this.texData[destIndex + 2] = b;
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

      // Create plane mesh if it doesn't exist yet (for cases where texture wasn't reallocated)
      if (!this.planeMesh && this.scene && this.material) {
        const { THREE } = this.threeJsModules;

        // Dispose old geometry if exists
        if (this.planeGeometry) {
          this.planeGeometry.dispose();
        }

        // Create plane geometry that matches the texture dimensions
        this.planeGeometry = new THREE.PlaneGeometry(this.texWidth, this.texHeight);

        // Create mesh with geometry
        this.planeMesh = new THREE.Mesh(this.planeGeometry, this.material);
        this.planeMesh.position.set(0, 0, 0);
        this.scene.add(this.planeMesh);

        // Update camera to fit the texture dimensions
        if (this.camera && this.camera.isOrthographicCamera) {
          const halfWidth = this.texWidth / 2;
          const halfHeight = this.texHeight / 2;
          const MARGIN = 1.05;

          this.camera.left = -halfWidth * MARGIN;
          this.camera.right = halfWidth * MARGIN;
          this.camera.top = halfHeight * MARGIN;
          this.camera.bottom = -halfHeight * MARGIN;
          this.camera.updateProjectionMatrix();
        }
      }
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
