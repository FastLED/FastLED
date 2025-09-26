/**
 * FastLED Graphics Manager - Beautiful 3D Three.js Renderer
 *
 * Provides stunning 3D rendering of LED layouts using Three.js with advanced visual effects.
 * This renderer is optimized for sparse LED layouts and visual appeal over performance.
 *
 * Key features:
 * - Three.js-based 3D rendering with bloom effects
 * - Dynamic LED geometry creation and management
 * - Selective bloom for enhanced visual appeal
 * - Optimized mesh merging for performance
 * - Advanced lighting and post-processing effects
 *
 * @module GraphicsManagerThreeJS
 */

/* eslint-disable no-console */
/* eslint-disable no-restricted-syntax */
/* eslint-disable max-len */
/* eslint-disable guard-for-in */
/* eslint-disable camelcase */
/* eslint-disable no-underscore-dangle */
/* eslint-disable no-plusplus */
/* eslint-disable no-continue */

// Selective bloom demo:
// https://discourse.threejs.org/t/totentanz-selective-bloom/8329

import { GraphicsManagerBase } from './graphics_manager_base.js';
import { isDenseGrid } from './graphics_utils.js';

// Declare THREE as global namespace for type checking
/* global THREE */

/** Disable geometry merging for debugging (set to true to force individual LED objects) */
// const DISABLE_MERGE_GEOMETRIES = false; // Currently unused

/**
 * Creates position calculator functions for mapping LED coordinates to 3D space
 * @param {Object} frameData - Frame data containing screen mapping information
 * @param {number} screenWidth - Width of the screen coordinate system
 * @param {number} screenHeight - Height of the screen coordinate system
 * @returns {Object} Object with calcXPosition and calcYPosition functions
 */
function makePositionCalculators(frameData, screenWidth, screenHeight) {
  const { screenMap } = frameData;
  const width = screenMap.absMax[0] - screenMap.absMin[0];
  const height = screenMap.absMax[1] - screenMap.absMin[1];

  return {
    /**
     * Calculates X position in 3D space from screen coordinates
     * @param {number} x - Screen X coordinate
     * @returns {number} 3D X position centered around origin
     */
    calcXPosition: (x) => (((x - screenMap.absMin[0]) / width) * screenWidth) - (screenWidth / 2),
    /**
     * Calculates Y position in 3D space from screen coordinates
     * @param {number} y - Screen Y coordinate
     * @returns {number} 3D Y position centered around origin
     */
    calcYPosition: (y) => {
      const negY = (((y - screenMap.absMin[1]) / height) * screenHeight) - (screenHeight / 2);
      return negY; // Remove negative sign to fix Y-axis orientation
    },
  };
}

/**
 * Beautiful 3D Graphics Manager using Three.js for LED rendering
 * Provides advanced visual effects and post-processing for stunning LED displays
 */
export class GraphicsManagerThreeJS extends GraphicsManagerBase {
  /**
   * Creates a new GraphicsManagerThreeJS instance
   * @param {Object} graphicsArgs - Configuration options
   * @param {string} graphicsArgs.canvasId - ID of the canvas element to render to
   * @param {Object} graphicsArgs.threeJsModules - Three.js modules and dependencies
   */
  constructor(graphicsArgs) {
    // Call parent constructor
    super(graphicsArgs);

    /** @type {number} Number of segments for LED sphere geometry */
    this.SEGMENTS = 16;

    /** @type {number} Scale factor for LED size */
    this.LED_SCALE = 1.0;

    /** @type {number} Bloom effect strength (0-1) */
    this.bloom_stength = 1;

    /** @type {number} Bloom effect radius in pixels */
    this.bloom_radius = 16;

    // Scene objects specific to 3D rendering
    /** @type {Array} Array of LED mesh objects */
    this.leds = [];

    /** @type {Array} Array of merged mesh objects for performance */
    this.mergedMeshes = [];

    /** @type {Object|null} Post-processing composer */
    this.composer = null;

    /** @type {number} Counter for out-of-bounds warnings to prevent spam */
    this.outside_bounds_warning_count = 0;

    /** @type {boolean} Whether to use merged geometries for performance */
    this.useMergedGeometry = true; // Enable geometry merging by default

    // Instanced rendering properties
    /** @type {Object|null} Three.js instanced mesh for all LEDs */
    this.instancedMesh = null;

    /** @type {number} Total number of LED instances */
    this.instanceCount = 0;

    /** @type {Float32Array|null} Instance position data */
    this.instancePositions = null;

    /** @type {Float32Array|null} Instance color data */
    this.instanceColors = null;

    /** @type {boolean} Whether to use instanced rendering (preferred) */
    this.useInstancedRendering = true;

    // Object pools for reducing garbage collection
    /** @type {Object} Object pools for Three.js objects */
    this.objectPools = {
      colors: [],
      matrices: [],
      vectors: [],
      maxPoolSize: 100 // Limit pool size to prevent memory bloat
    };
  }

  /**
   * Gets a Color object from the pool or creates a new one
   * @returns {THREE.Color} Pooled Color object
   */
  _getPooledColor() {
    const { THREE } = this.threeJsModules;

    if (this.objectPools.colors.length > 0) {
      return this.objectPools.colors.pop();
    }
    return new THREE.Color();
  }

  /**
   * Returns a Color object to the pool
   * @param {THREE.Color} color - Color object to return
   */
  _releasePooledColor(color) {
    if (this.objectPools.colors.length < this.objectPools.maxPoolSize) {
      // Reset color to default state
      color.setRGB(0, 0, 0);
      this.objectPools.colors.push(color);
    }
  }

  /**
   * Gets a Matrix4 object from the pool or creates a new one
   * @returns {THREE.Matrix4} Pooled Matrix4 object
   */
  _getPooledMatrix() {
    const { THREE } = this.threeJsModules;

    if (this.objectPools.matrices.length > 0) {
      return this.objectPools.matrices.pop();
    }
    return new THREE.Matrix4();
  }

  /**
   * Returns a Matrix4 object to the pool
   * @param {THREE.Matrix4} matrix - Matrix4 object to return
   */
  _releasePooledMatrix(matrix) {
    if (this.objectPools.matrices.length < this.objectPools.maxPoolSize) {
      // Reset matrix to identity
      matrix.identity();
      this.objectPools.matrices.push(matrix);
    }
  }

  /**
   * Gets a Vector3 object from the pool or creates a new one
   * @returns {THREE.Vector3} Pooled Vector3 object
   */
  _getPooledVector() {
    const { THREE } = this.threeJsModules;

    if (this.objectPools.vectors.length > 0) {
      return this.objectPools.vectors.pop();
    }
    return new THREE.Vector3();
  }

  /**
   * Returns a Vector3 object to the pool
   * @param {THREE.Vector3} vector - Vector3 object to return
   */
  _releasePooledVector(vector) {
    if (this.objectPools.vectors.length < this.objectPools.maxPoolSize) {
      // Reset vector to origin
      vector.set(0, 0, 0);
      this.objectPools.vectors.push(vector);
    }
  }

  /**
   * Clears all object pools (useful for cleanup)
   */
  _clearObjectPools() {
    this.objectPools.colors.length = 0;
    this.objectPools.matrices.length = 0;
    this.objectPools.vectors.length = 0;
  }

  /**
   * Cleans up and resets the rendering environment
   * Disposes of all Three.js objects and clears scene
   */
  reset() {
    // Clean up LED objects
    if (this.leds) {
      this.leds.forEach((led) => {
        // Release pooled objects from LED tracking objects
        if (led.material && led.material.color && led.material.color instanceof this.threeJsModules.THREE.Color) {
          this._releasePooledColor(led.material.color);
        }
        if (led.position && led.position instanceof this.threeJsModules.THREE.Vector3) {
          this._releasePooledVector(led.position);
        }

        // Clean up non-instanced LED objects
        if (!led._isMerged && !led._isInstanced) {
          led.geometry?.dispose();
          led.material?.dispose();
          this.scene?.remove(led);
        }
      });
    }
    this.leds = [];

    // Clean up merged meshes
    if (this.mergedMeshes) {
      this.mergedMeshes.forEach((mesh) => {
        if (mesh.geometry) mesh.geometry.dispose();
        if (mesh.material) mesh.material.dispose();
        this.scene?.remove(mesh);
      });
    }
    this.mergedMeshes = [];

    // Clean up instanced rendering resources
    if (this.instancedMesh) {
      this.instancedMesh.geometry.dispose();
      this.instancedMesh.material.dispose();
      this.scene?.remove(this.instancedMesh);
      this.instancedMesh = null;
    }
    this.instanceCount = 0;
    this.instancePositions = null;
    this.instanceColors = null;

    // Dispose of the composer
    if (this.composer) {
      this.composer.dispose();
    }

    // Clear object pools to prevent memory leaks
    this._clearObjectPools();

    // Call parent reset (which clears the scene)
    super.reset();

    // Don't remove the renderer or canvas
    if (this.renderer) {
      this.renderer.setSize(this.SCREEN_WIDTH, this.SCREEN_HEIGHT);
    }
  }

  /**
   * Initializes the Three.js rendering environment
   * Sets up scene, camera, renderer, and post-processing pipeline
   * @param {Object} frameData - The frame data containing screen mapping information
   */
  initThreeJS(frameData) {
    // Use inherited methods from base class
    this._setupCanvasAndDimensions(frameData);
    this._setupScene();
    this._setupRenderer();
    this._setupRenderPasses(frameData);

    // For orthographic camera, we need to update the projection matrix
    // after all setup is complete
    if (this.camera) {
      this.camera.updateProjectionMatrix();
    }
  }





  /**
   * Sets up render passes including bloom effect
   * @private
   */
  _setupRenderPasses(frameData) {
    const {
      THREE, EffectComposer, RenderPass, UnrealBloomPass,
    } = this.threeJsModules;

    // Create basic render pass
    const renderScene = new RenderPass(this.scene, this.camera);
    this.composer = new EffectComposer(this.renderer);
    this.composer.addPass(renderScene);

    // Create LED grid
    const { isDenseScreenMap } = this.createGrid(frameData);

    // Configure bloom effect based on grid density
    if (!isDenseScreenMap) {
      this.bloom_stength = 16;
      this.bloom_radius = 1;
    } else {
      this.bloom_stength = 0;
      this.bloom_radius = 0;
    }

    // Add bloom pass if needed
    if (this.bloom_stength > 0 || this.bloom_radius > 0) {
      const bloomPass = new UnrealBloomPass(
        new THREE.Vector2(this.SCREEN_WIDTH, this.SCREEN_HEIGHT),
        this.bloom_stength,
        this.bloom_radius,
        0.0, // threshold
      );
      this.composer.addPass(bloomPass);
    }
  }

  /**
   * Creates the LED grid based on frame data
   * @param {Object} frameData - The frame data containing screen mapping information
   * @returns {Object} - Object containing isDenseScreenMap flag
   */
  createGrid(frameData) {
    this._clearExistingLeds();

    // Collect LED positions and calculate layout properties
    const ledPositions = this._collectLedPositions(frameData);
    const { screenMap } = frameData;
    const width = screenMap.absMax[0] - screenMap.absMin[0];
    const height = screenMap.absMax[1] - screenMap.absMin[1];

    // Get position calculators from utility function
    const { calcXPosition, calcYPosition } = makePositionCalculators(
      frameData,
      this.SCREEN_WIDTH,
      this.SCREEN_HEIGHT,
    );

    // Determine if we have a dense grid layout
    const isDenseScreenMap = isDenseGrid(frameData);

    // Calculate dot sizes
    const { defaultDotSize, normalizedScale } = this._calculateDotSizes(
      frameData,
      ledPositions,
      width,
      height,
      calcXPosition,
      isDenseScreenMap,
    );

    // Create LED objects
    this._createLedObjects(
      frameData,
      calcXPosition,
      calcYPosition,
      isDenseScreenMap,
      defaultDotSize,
      normalizedScale,
    );

    return { isDenseScreenMap };
  }

  /**
   * Clears existing LED objects
   * @private
   */
  _clearExistingLeds() {
    this.leds.forEach((led) => {
      led.geometry.dispose();
      led.material.dispose();
      this.scene?.remove(led);
    });
    this.leds = [];
  }

  /**
   * Collects all LED positions from frame data
   * @private
   */
  _collectLedPositions(frameData) {
    const { screenMap } = frameData;
    const ledPositions = [];

    frameData.forEach((strip) => {
      const stripId = strip.strip_id;
      if (stripId in screenMap.strips) {
        const stripMap = screenMap.strips[stripId];
        const x_array = stripMap.map.x;
        const y_array = stripMap.map.y;

        for (let i = 0; i < x_array.length; i++) {
          ledPositions.push([x_array[i], y_array[i]]);
        }
      }
    });

    return ledPositions;
  }

  /**
   * Calculates appropriate dot sizes for LEDs
   * @private
   */
  _calculateDotSizes(frameData, ledPositions, width, height, calcXPosition, isDenseScreenMap) {
    const { screenMap } = frameData;
    const screenArea = width * height;
    let pixelDensityDefault;

    // For dense grids, use the distance between adjacent pixels
    if (isDenseScreenMap) {
      console.log('Pixel density is close to 1, assuming grid or strip');
      pixelDensityDefault = Math.abs(calcXPosition(0) - calcXPosition(1));
    }

    // Calculate default dot size based on screen area and LED count
    const defaultDotSizeScale = Math.max(
      4,
      Math.sqrt(screenArea / (ledPositions.length * Math.PI)) * 0.4,
    );

    // Get average diameter from strip data
    const stripDotSizes = Object.values(screenMap.strips).map((strip) => strip.diameter);
    const avgPointDiameter = stripDotSizes.reduce((a, b) => a + b, 0) / stripDotSizes.length;

    // Use pixel density for dense grids, otherwise calculate based on area
    let defaultDotSize = defaultDotSizeScale * avgPointDiameter;
    if (pixelDensityDefault) {
      // Override default dot size if pixel density is close to 1 for this dense strip.
      defaultDotSize = pixelDensityDefault;
    }

    const normalizedScale = this.SCREEN_WIDTH / width;

    return { defaultDotSize, normalizedScale };
  }

  /**
   * Creates LED objects using instanced rendering for maximum performance
   * @private
   */
  _createLedObjects(
    frameData,
    calcXPosition,
    calcYPosition,
    isDenseScreenMap,
    defaultDotSize,
    normalizedScale,
  ) {
    const { THREE } = this.threeJsModules;
    const { screenMap } = frameData;

    // Count total LEDs first
    let totalLeds = 0;
    const ledDataList = [];

    frameData.forEach((strip) => {
      const stripId = strip.strip_id;
      if (!(stripId in screenMap.strips)) {
        return;
      }

      const stripData = screenMap.strips[stripId];
      const stripDiameter = stripData.diameter ? stripData.diameter * normalizedScale : defaultDotSize;

      const x_array = stripData.map.x;
      const y_array = stripData.map.y;

      for (let i = 0; i < x_array.length; i++) {
        const x = calcXPosition(x_array[i]);
        const y = calcYPosition(y_array[i]);
        const z = 500;
        const scale = stripDiameter * this.LED_SCALE;

        ledDataList.push({ x, y, z, scale, stripId, ledIndex: i });
        totalLeds++;
      }
    });

    if (totalLeds === 0) {
      console.warn('No LEDs to create');
      return;
    }

    console.log(`Creating instanced mesh for ${totalLeds} LEDs`);

    // Create base geometry (same for all instances)
    let baseGeometry;
    if (isDenseScreenMap) {
      baseGeometry = new THREE.PlaneGeometry(1.0, 1.0);
    } else {
      baseGeometry = new THREE.CircleGeometry(1.0, this.SEGMENTS);
    }

    // Create instanced mesh
    const material = new THREE.MeshBasicMaterial({
      color: 0xffffff, // Will be overridden by instance colors
    });

    this.instancedMesh = new THREE.InstancedMesh(baseGeometry, material, totalLeds);
    this.instanceCount = totalLeds;

    // Allocate instance data arrays
    this.instancePositions = new Float32Array(totalLeds * 3); // x, y, z for each instance
    this.instanceColors = new Float32Array(totalLeds * 3); // r, g, b for each instance

    // Set up instance transformations and colors using pooled objects
    const matrix = this._getPooledMatrix();
    const color = this._getPooledColor();

    for (let i = 0; i < totalLeds; i++) {
      const ledData = ledDataList[i];

      // Set instance transformation matrix (position and scale)
      matrix.makeScale(ledData.scale, ledData.scale, 1);
      matrix.setPosition(ledData.x, ledData.y, ledData.z);
      this.instancedMesh.setMatrixAt(i, matrix);

      // Set initial color to black
      color.setRGB(0, 0, 0);
      this.instancedMesh.setColorAt(i, color);

      // Store position data for faster updates
      this.instancePositions[i * 3] = ledData.x;
      this.instancePositions[i * 3 + 1] = ledData.y;
      this.instancePositions[i * 3 + 2] = ledData.z;

      // Store initial color data
      this.instanceColors[i * 3] = 0;
      this.instanceColors[i * 3 + 1] = 0;
      this.instanceColors[i * 3 + 2] = 0;

      // Create LED tracking object for compatibility
      this.leds.push({
        _isInstanced: true,
        _instanceIndex: i,
        _stripId: ledData.stripId,
        _ledIndex: ledData.ledIndex,
        material: { color: this._getPooledColor() }, // Released in reset()
        position: this._getPooledVector().set(ledData.x, ledData.y, ledData.z), // Released in reset()
      });
    }

    // Add to scene
    this.scene.add(this.instancedMesh);

    // Clean up base geometry (it's been transferred to the instanced mesh)
    baseGeometry.dispose();

    // Return pooled objects to their pools
    this._releasePooledMatrix(matrix);
    this._releasePooledColor(color);

    console.log(`Instanced rendering setup complete: ${totalLeds} LEDs in 1 draw call`);
  }

  /**
   * Updates the canvas with new frame data
   * @param {Object} frameData - The frame data containing LED colors and positions
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
      // New experiment try to run anyway
      console.warn('Received empty frame data, skipping update');
      // return;
    }

    // Check if we need to initialize or reinitialize the scene
    this._checkAndInitializeScene(frameData);

    // Process the frame data
    const positionMap = this._collectLedColorData(frameData);

    // Update LED visuals
    const screenMap = (/** @type {any} */ (frameData)).screenMap;
    if (screenMap) {
      this._updateLedVisuals(positionMap);
    } else {
      console.warn('No screenMap available for LED visual updates');
    }

    // Render the scene
    this.composer.render();
  }

  /**
   * Checks if scene needs initialization and handles it
   * @private
   */
  _checkAndInitializeScene(frameData) {
    // Check if frameData is valid array
    if (!frameData || !Array.isArray(frameData)) {
      console.warn('Invalid frame data in _checkAndInitializeScene:', frameData);
      return;
    }

    const totalPixels = frameData.reduce(
      (acc, strip) => {
        // Check if strip and pixel_data exist and pixel_data has length property
        if (!strip || !strip.pixel_data || typeof strip.pixel_data.length !== 'number') {
          console.warn('Invalid strip data:', strip);
          return acc;
        }
        return acc + strip.pixel_data.length / 3;
      },
      0,
    );

    // Initialize scene if it doesn't exist or if LED count changed
    if (!this.scene || totalPixels !== this.previousTotalLeds) {
      if (this.scene) {
        this.reset(); // Clear existing scene if LED count changed
      }
      this.initThreeJS(frameData);
      this.previousTotalLeds = totalPixels;
    }
  }

  /**
   * Collects LED color data from frame data
   * @private
   * @returns {Map} - Map of LED positions to color data
   */
  _collectLedColorData(frameData) {
    // Handle frameData as array or object with screenMap
    const dataArray = Array.isArray(frameData) ? frameData : (frameData.data || []);
    const screenMap = (/** @type {any} */ (frameData)).screenMap;

    if (!screenMap) {
      console.warn('No screenMap found in frameData:', frameData);
      return new Map();
    }

    const positionMap = new Map();
    const WARNING_COUNT = 10;

    // Process each strip
    dataArray.forEach((strip) => {
      if (!strip) {
        console.warn('Null strip encountered, skipping');
        return;
      }

      const { strip_id } = strip;
      if (!screenMap.strips || !(strip_id in screenMap.strips)) {
        console.warn(`No screen map found for strip ID ${strip_id}, skipping update`);
        return;
      }

      const stripData = screenMap.strips[strip_id];
      if (!stripData || !stripData.map) {
        console.warn(`Invalid strip data for strip ID ${strip_id}:`, stripData);
        return;
      }

      const { map } = stripData;
      const data = strip.pixel_data;
      if (!data || typeof data.length !== 'number') {
        console.warn(`Invalid pixel data for strip ID ${strip_id}:`, data);
        return;
      }

      const pixelCount = data.length / 3;
      const x_array = stripData.map.x;
      const y_array = stripData.map.y;
      if (!x_array || !y_array || typeof x_array.length !== 'number' || typeof y_array.length !== 'number') {
        console.warn(`Invalid coordinate arrays for strip ID ${strip_id}:`, { x_array, y_array });
        return;
      }

      const length = Math.min(x_array.length, y_array.length);

      // Process each pixel in the strip
      for (let j = 0; j < pixelCount; j++) {
        if (j >= length) {
          this._handleOutOfBoundsPixel(strip_id, j, map.length, WARNING_COUNT);
          continue;
        }

        // Get pixel coordinates and color
        const x = x_array[j];
        const y = y_array[j];
        const posKey = `${x},${y}`;
        const srcIndex = j * 3;

        // Extract RGB values (0-1 range)
        const r = (data[srcIndex] & 0xFF) / 255; // eslint-disable-line
        const g = (data[srcIndex + 1] & 0xFF) / 255; // eslint-disable-line
        const b = (data[srcIndex + 2] & 0xFF) / 255; // eslint-disable-line
        const brightness = (r + g + b) / 3;

        // Only update if this LED is brighter than any existing LED at this position
        if (!positionMap.has(posKey) || positionMap.get(posKey).brightness < brightness) {
          positionMap.set(posKey, {
            x, y, r, g, b, brightness,
          });
        }
      }
    });

    return positionMap;
  }

  /**
   * Handles warning for pixels outside the screen map bounds
   * @private
   */
  _handleOutOfBoundsPixel(strip_id, j, mapLength, WARNING_COUNT) {
    this.outside_bounds_warning_count++;
    if (this.outside_bounds_warning_count < WARNING_COUNT) {
      console.warn(
        `Strip ${strip_id}: Pixel ${j} is outside the screen map ${mapLength}, skipping update`,
      );
      if (this.outside_bounds_warning_count === WARNING_COUNT) {
        console.warn('Suppressing further warnings about pixels outside the screen map');
      }
    }
    console.warn(
      `Strip ${strip_id}: Pixel ${j} is outside the screen map ${mapLength}, skipping update`,
    );
  }

  /**
   * Updates LED visuals based on position map data using instanced rendering
   * @private
   */
  _updateLedVisuals(positionMap) {
    // const { THREE } = this.threeJsModules; // Currently unused in this method

    if (this.useInstancedRendering && this.instancedMesh) {
      // Use highly optimized instanced rendering path
      this._updateInstancedLeds(positionMap);
      return;
    }

    // Fallback to legacy rendering (for compatibility)
    this._updateLegacyLeds(positionMap);
  }

  /**
   * Updates instanced LEDs with maximum performance using batch operations
   * @private
   */
  _updateInstancedLeds(positionMap) {
    // THREE.js modules available via this.threeJsModules if needed

    // Use the stored bounds from setup - currently unused but may be needed for future optimizations
    // const min_x = this.screenBounds.minX;
    // const min_y = this.screenBounds.minY;
    // const { width } = this.screenBounds;
    // const { height } = this.screenBounds;

    const color = this._getPooledColor();
    let ledIndex = 0;
    let hasChanges = false;

    // Batch update colors for active LEDs
    for (const [, ledData] of positionMap) {
      if (ledIndex >= this.instanceCount) break;

      // Check if color actually changed to avoid unnecessary GPU updates
      const currentR = this.instanceColors[ledIndex * 3];
      const currentG = this.instanceColors[ledIndex * 3 + 1];
      const currentB = this.instanceColors[ledIndex * 3 + 2];

      if (Math.abs(currentR - ledData.r) > 0.001 ||
          Math.abs(currentG - ledData.g) > 0.001 ||
          Math.abs(currentB - ledData.b) > 0.001) {

        // Color changed, update it
        color.setRGB(ledData.r, ledData.g, ledData.b);
        this.instancedMesh.setColorAt(ledIndex, color);

        // Update our tracking array
        this.instanceColors[ledIndex * 3] = ledData.r;
        this.instanceColors[ledIndex * 3 + 1] = ledData.g;
        this.instanceColors[ledIndex * 3 + 2] = ledData.b;

        // Update LED tracking object for compatibility
        if (this.leds[ledIndex]) {
          this.leds[ledIndex].material.color.setRGB(ledData.r, ledData.g, ledData.b);
        }

        hasChanges = true;
      }

      ledIndex++;
    }

    // Clear any remaining LEDs to black (batch operation)
    color.setRGB(0, 0, 0);
    for (let i = ledIndex; i < this.instanceCount; i++) {
      // Check if LED is not already black
      if (this.instanceColors[i * 3] !== 0 ||
          this.instanceColors[i * 3 + 1] !== 0 ||
          this.instanceColors[i * 3 + 2] !== 0) {

        this.instancedMesh.setColorAt(i, color);
        this.instanceColors[i * 3] = 0;
        this.instanceColors[i * 3 + 1] = 0;
        this.instanceColors[i * 3 + 2] = 0;

        if (this.leds[i]) {
          this.leds[i].material.color.setRGB(0, 0, 0);
        }

        hasChanges = true;
      }
    }

    // Only trigger GPU update if there were actual changes
    if (hasChanges) {
      this.instancedMesh.instanceColor.needsUpdate = true;
    }

    // Return pooled color object
    this._releasePooledColor(color);
  }

  /**
   * Legacy LED update method (fallback for merged/individual geometry)
   * @private
   */
  _updateLegacyLeds(positionMap) {
    // THREE.js modules available via this.threeJsModules if needed

    // Use the stored bounds from setup
    const min_x = this.screenBounds.minX;
    const min_y = this.screenBounds.minY;
    const { width } = this.screenBounds;
    const { height } = this.screenBounds;

    // Track which merged meshes need updates
    const mergedMeshUpdates = new Map();

    // Update LED positions and colors
    let ledIndex = 0;
    for (const [, ledData] of positionMap) {
      if (ledIndex >= this.leds.length) break;

      const led = this.leds[ledIndex];

      // Calculate normalized position for orthographic view
      const x = ledData.x - min_x;
      const y = ledData.y - min_y;
      const normalizedX = (x / width) * this.SCREEN_WIDTH - this.SCREEN_WIDTH / 2;
      const normalizedY = (y / height) * this.SCREEN_HEIGHT - this.SCREEN_HEIGHT / 2;

      // Get z position (fixed for orthographic camera)
      const z = this._calculateDepthEffect();

      // Update LED position and color
      if (led._isMerged) {
        // For merged geometries, track color updates
        led.material.color.setRGB(ledData.r, ledData.g, ledData.b);
        led.position.set(normalizedX, normalizedY, z);

        // Track which parent mesh needs updating
        if (led._parentMesh) {
          if (!mergedMeshUpdates.has(led._parentMesh)) {
            mergedMeshUpdates.set(led._parentMesh, []);
          }
          const tempColor = this._getPooledColor();
          tempColor.setRGB(ledData.r, ledData.g, ledData.b);
          mergedMeshUpdates.get(led._parentMesh).push({
            index: led._mergedIndex,
            color: tempColor, // Will be released in _updateMergedMeshes
          });
        }
      } else {
        // For individual LEDs, update position and color directly
        led.position.set(normalizedX, normalizedY, z);
        led.material.color.setRGB(ledData.r, ledData.g, ledData.b);
      }

      ledIndex++;
    }

    // Update merged meshes with instance colors
    this._updateMergedMeshes(mergedMeshUpdates);

    // Clear any remaining LEDs
    this._clearUnusedLeds(ledIndex);
  }

  /**
   * Updates merged meshes with new colors
   * @private
   */
  _updateMergedMeshes(mergedMeshUpdates) {
    const { THREE } = this.threeJsModules;

    // Process each mesh that needs updates
    for (const [mesh, updates] of mergedMeshUpdates.entries()) {
      if (!mesh.geometry || !mesh.material) continue;

      // Create or update the color attribute if needed
      if (!mesh.geometry.attributes.color) {
        // Create a new color attribute
        const { count } = mesh.geometry.attributes.position;
        const colorArray = new Float32Array(count * 3);
        mesh.geometry.setAttribute('color', new THREE.BufferAttribute(colorArray, 3));

        // Update material to use vertex colors
        mesh.material.vertexColors = true;
      }

      // Get the color attribute
      const colorAttribute = mesh.geometry.attributes.color;

      // Update colors for each LED
      updates.forEach((update) => {
        const { index, color } = update;

        // Each vertex of the geometry needs the color
        const verticesPerInstance = mesh.geometry.attributes.position.count / this.leds.length;
        for (let v = 0; v < verticesPerInstance; v++) {
          const i = (index * verticesPerInstance + v) * 3;
          colorAttribute.array[i] = color.r;
          colorAttribute.array[i + 1] = color.g;
          colorAttribute.array[i + 2] = color.b;
        }

        // Return pooled color object after use
        this._releasePooledColor(color);
      });

      // Mark the attribute as needing an update
      colorAttribute.needsUpdate = true;
    }
  }

  /**
   * Calculates a depth effect based on distance from center
   * @private
   */
  _calculateDepthEffect() {
    // With orthographic camera, we don't need a depth effect based on distance
    // But we can still use a small z-offset to prevent z-fighting
    return 0; // Fixed z position for orthographic view
  }

  /**
   * Clears unused LEDs by setting them to black and moving offscreen
   * @private
   */
  _clearUnusedLeds(startIndex) {
    // Track which merged meshes need updates for clearing
    const mergedMeshUpdates = new Map();
    const { THREE } = this.threeJsModules;

    for (let i = startIndex; i < this.leds.length; i++) {
      const led = this.leds[i];
      led.material.color.setRGB(0, 0, 0);

      if (!led._isMerged) {
        led.position.set(-1000, -1000, 0); // Move offscreen
      } else {
        led.position.set(-1000, -1000, 0); // Update tracking position

        // Track which parent mesh needs clearing
        if (led._parentMesh) {
          if (!mergedMeshUpdates.has(led._parentMesh)) {
            mergedMeshUpdates.set(led._parentMesh, []);
          }
          mergedMeshUpdates.get(led._parentMesh).push({
            index: led._mergedIndex,
            color: new THREE.Color(0, 0, 0),
          });
        }
      }
    }

    // Update merged meshes with cleared colors
    this._updateMergedMeshes(mergedMeshUpdates);
  }

}
