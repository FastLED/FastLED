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

import { isDenseGrid, computeScreenMapBounds } from './graphics_utils.js';

// Declare THREE as global namespace for type checking
/* global THREE */

/** Disable geometry merging for debugging (set to true to force individual LED objects) */
const DISABLE_MERGE_GEOMETRIES = false;

/**
 * Creates position calculator functions for mapping LED coordinates to 3D space
 * @param {Object} screenMap - Screen mapping data
 * @param {number} screenWidth - Width of the screen coordinate system
 * @param {number} screenHeight - Height of the screen coordinate system
 * @returns {Object} Object with calcXPosition and calcYPosition functions
 */
function makePositionCalculators(screenMap, screenWidth, screenHeight) {
  // Compute bounds from screenMap if not already present
  const bounds = (screenMap && screenMap.absMin && screenMap.absMax)
    ? { absMin: screenMap.absMin, absMax: screenMap.absMax }
    : computeScreenMapBounds(screenMap);

  const width = bounds.absMax[0] - bounds.absMin[0];
  const height = bounds.absMax[1] - bounds.absMin[1];

  // Validate screenmap bounds - allow width=0 for Nx1 or height=0 for 1xN strips
  if (Number.isNaN(width) || Number.isNaN(height) || (width === 0 && height === 0)) {
    console.error('Invalid screenmap bounds detected:');
    console.error(`  absMin: [${bounds.absMin[0]}, ${bounds.absMin[1]}]`);
    console.error(`  absMax: [${bounds.absMax[0]}, ${bounds.absMax[1]}]`);
    console.error(`  width: ${width}, height: ${height}`);
    console.error('This indicates the screenmap was not properly set up.');
    console.error('Make sure to call .setScreenMap() on your LED controller in setup().');
  }

  return {
    /**
     * Calculates X position in 3D space from screen coordinates
     * @param {number} x - Screen X coordinate
     * @returns {number} 3D X position centered around origin
     */
    calcXPosition: (x) => {
      if (width === 0) return 0; // Handle Nx1 case
      return (((x - bounds.absMin[0]) / width) * screenWidth) - (screenWidth / 2);
    },
    /**
     * Calculates Y position in 3D space from screen coordinates
     * @param {number} y - Screen Y coordinate
     * @returns {number} 3D Y position centered around origin
     */
    calcYPosition: (y) => {
      if (height === 0) return 0; // Handle 1xN case
      const negY = (((y - bounds.absMin[1]) / height) * screenHeight) - (screenHeight / 2);
      return negY; // Remove negative sign to fix Y-axis orientation
    },
  };
}

/**
 * Beautiful 3D Graphics Manager using Three.js for LED rendering
 * Provides advanced visual effects and post-processing for stunning LED displays
 */
export class GraphicsManagerThreeJS {
  /**
   * Creates a new GraphicsManagerThreeJS instance
   * @param {Object} graphicsArgs - Configuration options
   * @param {string} [graphicsArgs.canvasId] - ID of the canvas element to render to (main thread)
   * @param {HTMLCanvasElement|OffscreenCanvas} [graphicsArgs.canvas] - Canvas object directly (worker thread)
   * @param {Object} graphicsArgs.threeJsModules - Three.js modules and dependencies
   */
  constructor(graphicsArgs) {
    const { canvasId, canvas, threeJsModules } = graphicsArgs;

    // Configuration
    /** @type {string|HTMLCanvasElement|OffscreenCanvas} HTML canvas element ID or canvas instance */
    this.canvasId = canvas || canvasId;

    /** @type {Object} Three.js modules and dependencies */
    this.threeJsModules = threeJsModules;

    /** @type {number} Number of segments for LED sphere geometry */
    this.SEGMENTS = 16;

    /** @type {number} Scale factor for LED size */
    this.LED_SCALE = 1.0;

    // Rendering properties
    /** @type {number} Screen width in rendering units */
    this.SCREEN_WIDTH = 0;

    /** @type {number} Screen height in rendering units */
    this.SCREEN_HEIGHT = 0;

    /** @type {number} Bloom effect strength (0-1) */
    this.bloom_stength = 1;

    /** @type {number} Bloom effect radius in pixels */
    this.bloom_radius = 16;

    /** @type {number} Current brightness level for auto-bloom (0-1) */
    this.current_brightness = 0;

    /** @type {number} Target brightness level for iris simulation */
    this.target_brightness = 0;

    /** @type {number} Iris response speed (lower = slower, simulates pupil dilation) */
    this.iris_response_speed = 0.05;

    /** @type {number} Base bloom strength for auto-adjustment */
    this.base_bloom_strength = 16;

    /** @type {number} Maximum bloom strength */
    this.max_bloom_strength = 20;

    /** @type {number} Minimum bloom strength */
    this.min_bloom_strength = 0.5;

    /** @type {boolean} Enable auto-bloom feature */
    this.auto_bloom_enabled = true;

    // Scene objects
    /** @type {Array} Array of LED mesh objects */
    this.leds = [];

    /** @type {Array} Array of merged mesh objects for performance */
    this.mergedMeshes = [];

    /** @type {Object|null} Three.js scene object */
    this.scene = null;

    /** @type {Object|null} Three.js camera object */
    this.camera = null;

    /** @type {Object|null} Three.js WebGL renderer */
    this.renderer = null;

    /** @type {Object|null} Post-processing composer */
    this.composer = null;

    // State tracking
    /** @type {number} Previous frame's total LED count for optimization */
    this.previousTotalLeds = 0;

    /** @type {number} Counter for out-of-bounds warnings to prevent spam */
    this.outside_bounds_warning_count = 0;

    /** @type {boolean} Whether to use merged geometries for performance */
    this.useMergedGeometry = true; // Enable geometry merging by default

    /** @type {HTMLCanvasElement|OffscreenCanvas|null} Stored canvas reference */
    this.canvas = null;

    /** @type {Object} Dictionary of screenmaps (stripId → screenmap object) */
    this.screenMaps = {};

    /** @type {boolean} Flag to trigger scene rebuild when screenMaps change */
    this.needsRebuild = false;
  }

  /**
   * Cleans up and resets the rendering environment
   * Disposes of all Three.js objects and clears scene
   */
  reset() {
    // Clean up LED objects
    if (this.leds) {
      this.leds.forEach((led) => {
        if (!led._isMerged) {
          led.geometry.dispose();
          led.material.dispose();
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

    // Dispose of the composer
    if (this.composer) {
      this.composer.dispose();
    }

    // Clear the scene
    if (this.scene) {
      while (this.scene.children.length > 0) {
        this.scene.remove(this.scene.children[0]);
      }
    }

    // Don't remove the renderer or canvas
    if (this.renderer) {
      // OffscreenCanvas doesn't have a .style property, so we need to pass false
      // to prevent Three.js from trying to set canvas.style.width/height
      const isOffscreenCanvas = typeof OffscreenCanvas !== 'undefined' && this.canvas instanceof OffscreenCanvas;
      this.renderer.setSize(this.SCREEN_WIDTH, this.SCREEN_HEIGHT, !isOffscreenCanvas);
    }
  }

  /**
   * Updates the screenMaps data when it changes (event-driven)
   * Called directly from worker's handleScreenMapUpdate()
   * Triggers scene rebuild on next frame if scene already exists
   * @param {Object} screenMapsData - Dictionary of screenmaps (stripId → screenmap object)
   */
  updateScreenMap(screenMapsData) {
    this.screenMaps = screenMapsData;

    if (this.scene) {
      // Scene exists - trigger rebuild on next updateCanvas() call
      this.needsRebuild = true;
      console.log('[GraphicsManagerThreeJS] ScreenMaps updated, scene will rebuild');
    } else {
      // Scene doesn't exist yet - screenMaps will be used during first initThreeJS()
      console.log('[GraphicsManagerThreeJS] ScreenMaps received before scene initialization');
    }
  }

  /**
   * Initializes the Three.js rendering environment
   * Sets up scene, camera, renderer, and post-processing pipeline
   * @param {Object} frameData - The frame data containing screen mapping information
   */
  initThreeJS(frameData) {
    // Get canvas - either OffscreenCanvas (worker), HTMLCanvasElement, or string ID (main thread)
    // If canvas is already stored from a previous initialization, reuse it
    if (!this.canvas) {
      // First-time initialization: resolve canvas from canvasId
      // @ts-ignore - OffscreenCanvas type checking
      const isOffscreenCanvas = typeof OffscreenCanvas !== 'undefined' && this.canvasId instanceof OffscreenCanvas;
      // @ts-ignore - HTMLCanvasElement type checking
      const isHTMLCanvas = typeof HTMLCanvasElement !== 'undefined' && this.canvasId instanceof HTMLCanvasElement;

      if (isOffscreenCanvas || isHTMLCanvas) {
        // Worker context or direct canvas object: use canvas directly
        this.canvas = /** @type {HTMLCanvasElement|OffscreenCanvas} */ (this.canvasId);
      } else if (typeof this.canvasId === 'string') {
        // Main thread: look up canvas by ID
        const canvasElement = document.getElementById(this.canvasId);
        if (!canvasElement) {
          console.warn('initThreeJS: Canvas element not found');
          return;
        }
        this.canvas = /** @type {HTMLCanvasElement|OffscreenCanvas} */ (canvasElement);
      } else {
        console.error('Invalid canvas parameter: must be string ID, HTMLCanvasElement, or OffscreenCanvas', {
          canvasIdType: typeof this.canvasId,
          canvasIdValue: this.canvasId
        });
        return;
      }
    } else {
      console.log('initThreeJS: Reusing existing canvas reference');
    }

    // Guard: Ensure canvas was successfully resolved
    if (!this.canvas) {
      console.error('initThreeJS: Canvas resolution failed - cannot initialize Three.js');
      return;
    }

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
   * Sets up canvas dimensions and display properties
   * @private
   * @param {Object} _frameData - Frame data (unused, kept for API compatibility)
   */
  _setupCanvasAndDimensions(_frameData) {
    const RESOLUTION_BOOST = 2; // 2x resolution for higher quality
    const MAX_WIDTH = 640; // Max pixels width on browser

    // Defensive check: Ensure canvas is defined
    if (!this.canvas) {
      console.error('_setupCanvasAndDimensions: canvas is undefined or null', {
        hasCanvasId: !!this.canvasId,
        canvasIdType: typeof this.canvasId,
        canvasIdValue: this.canvasId
      });
      throw new Error('Canvas reference is undefined in _setupCanvasAndDimensions');
    }

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

    // Calculate composite dimensions
    let screenMapWidth = globalMaxX - globalMinX;
    let screenMapHeight = globalMaxY - globalMinY;

    // Defensive: Ensure non-zero dimensions (for simple examples with 1 LED or no XY mapping)
    const MIN_DIMENSION = 1; // Minimum 1 pixel dimension
    if (screenMapWidth <= 0 || !Number.isFinite(screenMapWidth)) {
      console.warn('_setupCanvasAndDimensions: screenMapWidth is <= 0 or invalid, using MIN_DIMENSION', {
        screenMapWidth,
        screenMapsCount: Object.keys(this.screenMaps).length
      });
      screenMapWidth = MIN_DIMENSION;
    }
    if (screenMapHeight <= 0 || !Number.isFinite(screenMapHeight)) {
      console.warn('_setupCanvasAndDimensions: screenMapHeight is <= 0 or invalid, using MIN_DIMENSION', {
        screenMapHeight,
        screenMapsCount: Object.keys(this.screenMaps).length
      });
      screenMapHeight = MIN_DIMENSION;
    }

    // Always set width to 640px and scale height proportionally
    const targetWidth = MAX_WIDTH;
    const aspectRatio = screenMapWidth / screenMapHeight;
    const targetHeight = Math.round(targetWidth / aspectRatio);

    // Defensive: Ensure targetHeight is a valid positive integer
    if (!Number.isFinite(targetHeight) || targetHeight <= 0) {
      console.error('_setupCanvasAndDimensions: Invalid targetHeight calculated', {
        targetHeight,
        aspectRatio,
        screenMapWidth,
        screenMapHeight
      });
      throw new Error(`Invalid canvas height: ${targetHeight}. Check screen mapping dimensions.`);
    }

    // Set the rendering resolution (2x the display size)
    this.SCREEN_WIDTH = targetWidth * RESOLUTION_BOOST;
    this.SCREEN_HEIGHT = targetHeight * RESOLUTION_BOOST;

    // Set canvas size
    this.canvas.width = targetWidth * RESOLUTION_BOOST;
    this.canvas.height = targetHeight * RESOLUTION_BOOST;

    // Set display size only for HTMLCanvasElement (not OffscreenCanvas)
    // @ts-ignore - Type guard for HTMLCanvasElement.style property
    if (typeof window !== 'undefined' && !(this.canvas instanceof OffscreenCanvas) && this.canvas.style) {
      this.canvas.style.width = `${targetWidth}px`;
      this.canvas.style.height = `${targetHeight}px`;
      this.canvas.style.maxWidth = `${targetWidth}px`;
      this.canvas.style.maxHeight = `${targetHeight}px`;
    }

    // Store the composite bounds for orthographic camera calculations
    this.screenBounds = {
      width: screenMapWidth,
      height: screenMapHeight,
      minX: globalMinX,
      minY: globalMinY,
      maxX: globalMaxX,
      maxY: globalMaxY,
    };
  }

  /**
   * Sets up the Three.js scene and camera
   * @private
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
   * @private
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
   * Sets up the WebGL renderer
   * @private
   */
  _setupRenderer() {
    const { THREE } = this.threeJsModules;

    this.renderer = new THREE.WebGLRenderer({
      canvas: this.canvas, // Use stored canvas reference
      antialias: true,
    });
    // OffscreenCanvas doesn't have a .style property, so we need to pass false
    // to prevent Three.js from trying to set canvas.style.width/height
    const isOffscreenCanvas = typeof OffscreenCanvas !== 'undefined' && this.canvas instanceof OffscreenCanvas;
    this.renderer.setSize(this.SCREEN_WIDTH, this.SCREEN_HEIGHT, !isOffscreenCanvas);
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

    const width = globalMaxX - globalMinX;
    const height = globalMaxY - globalMinY;

    // Create composite screenMap-like object for position calculators
    const compositeScreenMap = {
      absMin: [globalMinX, globalMinY],
      absMax: [globalMaxX, globalMaxY]
    };

    // Get position calculators using composite bounds
    const { calcXPosition, calcYPosition } = makePositionCalculators(
      compositeScreenMap,
      this.SCREEN_WIDTH,
      this.SCREEN_HEIGHT,
    );

    // Calculate global density: all screenmaps must be dense for global density
    let isDenseScreenMap = true;
    for (const screenMap of Object.values(this.screenMaps)) {
      if (!isDenseGrid(screenMap)) {
        isDenseScreenMap = false;
        break;
      }
    }

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
    const ledPositions = [];

    frameData.forEach((strip) => {
      const stripId = strip.strip_id;

      // Get screenmap for this specific strip
      const screenMap = this.screenMaps[stripId];
      if (!screenMap) {
        console.warn(`[GraphicsManagerThreeJS] No screenMap found for strip ${stripId}`);
        return;
      }

      // Look up strip data in its screenmap
      if (!(stripId in screenMap.strips)) {
        console.warn(`[GraphicsManagerThreeJS] Strip ${stripId} not found in its screenMap`);
        return;
      }

      const stripMap = screenMap.strips[stripId];
      const x_array = stripMap.map.x;
      const y_array = stripMap.map.y;

      for (let i = 0; i < x_array.length; i++) {
        ledPositions.push([x_array[i], y_array[i]]);
      }
    });

    return ledPositions;
  }

  /**
   * Calculates appropriate dot sizes for LEDs
   * @private
   */
  _calculateDotSizes(frameData, ledPositions, width, height, calcXPosition, isDenseScreenMap) {
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

    // Get average diameter from all strips across all screenmaps
    const stripDotSizes = [];
    for (const screenMap of Object.values(this.screenMaps)) {
      for (const strip of Object.values(screenMap.strips)) {
        stripDotSizes.push(strip.diameter);
      }
    }
    const avgPointDiameter = stripDotSizes.length > 0
      ? stripDotSizes.reduce((a, b) => a + b, 0) / stripDotSizes.length
      : 0.5; // Default diameter if no strips

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
   * Creates LED objects for each pixel in the frame data
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

    // If BufferGeometryUtils is not available, fall back to individual LEDs
    const { BufferGeometryUtils } = this.threeJsModules;
    const canMergeGeometries = this.useMergedGeometry && BufferGeometryUtils
      && !DISABLE_MERGE_GEOMETRIES;

    if (!canMergeGeometries) {
      console.log('BufferGeometryUtils not available, falling back to individual LEDs');
    } else {
      console.log('Using merged geometries for better performance');
    }

    // Create template geometries for reuse
    let circleGeometry;
    let planeGeometry;
    if (!isDenseScreenMap) {
      circleGeometry = new THREE.CircleGeometry(1.0, this.SEGMENTS);
    } else {
      planeGeometry = new THREE.PlaneGeometry(1.0, 1.0);
    }

    // Group all geometries together for merging
    const allGeometries = [];
    const allLedData = [];

    frameData.forEach((strip) => {
      const stripId = strip.strip_id;

      // Get screenmap for this specific strip from cached screenMaps
      const screenMap = this.screenMaps[stripId];
      if (!screenMap) {
        console.warn(`[GraphicsManagerThreeJS] No screenMap found for strip ${stripId} in _createLedObjects`);
        return;
      }

      // Look up strip data in its screenmap
      if (!(stripId in screenMap.strips)) {
        console.warn(`[GraphicsManagerThreeJS] Strip ${stripId} not found in its screenMap in _createLedObjects`);
        return;
      }

      const stripData = screenMap.strips[stripId];
      let stripDiameter = null;
      if (stripData.diameter) {
        stripDiameter = stripData.diameter * normalizedScale;
      } else {
        stripDiameter = defaultDotSize;
      }

      const x_array = stripData.map.x;
      const y_array = stripData.map.y;

      for (let i = 0; i < x_array.length; i++) {
        // Calculate position
        const x = calcXPosition(x_array[i]);
        const y = calcYPosition(y_array[i]);
        const z = 500;

        // Validate coordinates - detect NaN which indicates missing screenmap registration
        if (Number.isNaN(x) || Number.isNaN(y)) {
          console.error(`Invalid LED coordinates detected for strip ${stripId}, LED ${i}:`);
          console.error(`  x=${x}, y=${y} (raw: x_array[${i}]=${x_array[i]}, y_array[${i}]=${y_array[i]})`);
          console.error(`  This usually means you created a ScreenMap but forgot to call .setScreenMap() on the controller.`);
          console.error(`  Example: FastLED.addLeds<...>(leds, NUM_LEDS).setScreenMap(yourScreenMap);`);
          throw new Error(`Invalid LED coordinates for strip ${stripId}, LED ${i}. Did you forget to call .setScreenMap()?`);
        }

        if (!canMergeGeometries) {
          // Create individual LEDs (original approach)
          let geometry;
          if (isDenseScreenMap) {
            const w = stripDiameter * this.LED_SCALE;
            const h = stripDiameter * this.LED_SCALE;
            geometry = new THREE.PlaneGeometry(w, h);
          } else {
            geometry = new THREE.CircleGeometry(
              stripDiameter * this.LED_SCALE,
              this.SEGMENTS,
            );
          }

          const material = new THREE.MeshBasicMaterial({ color: 0x000000 });
          const led = new THREE.Mesh(geometry, material);
          led.position.set(x, y, z);

          this.scene.add(led);
          this.leds.push(led);
        } else {
          // Create instance geometry for merging
          let instanceGeometry;
          if (isDenseScreenMap) {
            instanceGeometry = planeGeometry.clone();
            instanceGeometry.scale(
              stripDiameter * this.LED_SCALE,
              stripDiameter * this.LED_SCALE,
              1,
            );
          } else {
            instanceGeometry = circleGeometry.clone();
            instanceGeometry.scale(
              stripDiameter * this.LED_SCALE,
              stripDiameter * this.LED_SCALE,
              1,
            );
          }

          // Apply position transformation
          instanceGeometry.translate(x, y, z);

          // Add to collection for merging
          allGeometries.push(instanceGeometry);
          allLedData.push({ x, y, z });
        }
      }
    });

    // If using merged geometry, create a single merged mesh for all LEDs
    if (canMergeGeometries && allGeometries.length > 0) {
      try {
        // Merge all geometries together
        const mergedGeometry = BufferGeometryUtils.mergeGeometries(allGeometries);

        // Create material and mesh with vertex colors
        const material = new THREE.MeshBasicMaterial({
          color: 0xffffff,
          vertexColors: true,
        });

        // Create color attribute for the merged geometry
        const colorCount = mergedGeometry.attributes.position.count;
        const colorArray = new Float32Array(colorCount * 3);
        mergedGeometry.setAttribute('color', new THREE.BufferAttribute(colorArray, 3));

        const mesh = new THREE.Mesh(mergedGeometry, material);
        this.scene.add(mesh);
        this.mergedMeshes.push(mesh);

        // Create dummy objects for individual control
        for (let i = 0; i < allLedData.length; i++) {
          const pos = allLedData[i];
          const dummyObj = {
            material: { color: new THREE.Color(0, 0, 0) },
            position: new THREE.Vector3(pos.x, pos.y, pos.z),
            _isMerged: true,
            _mergedIndex: i,
            _parentMesh: mesh,
          };
          this.leds.push(dummyObj);
        }
      } catch (e) {
        console.log(BufferGeometryUtils);
        console.error('Failed to merge geometries:', e);

        // Fallback to individual geometries
        for (let i = 0; i < allGeometries.length; i++) {
          const pos = allLedData[i];
          const material = new THREE.MeshBasicMaterial({ color: 0x000000 });
          const geometry = allGeometries[i].clone();
          geometry.translate(-pos.x, -pos.y, -pos.z); // Reset position
          const led = new THREE.Mesh(geometry, material);
          led.position.set(pos.x, pos.y, pos.z);
          this.scene.add(led);
          this.leds.push(led);
        }
      }

      // Clean up template geometries
      if (circleGeometry) circleGeometry.dispose();
      if (planeGeometry) planeGeometry.dispose();

      // Clean up individual geometries
      allGeometries.forEach((g) => g.dispose());
    }
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

    // Calculate average brightness for auto-bloom
    if (this.auto_bloom_enabled) {
      this._updateAutoBrightness(positionMap);
    }

    // Update LED visuals
    if (Object.keys(this.screenMaps).length > 0) {
      this._updateLedVisuals(positionMap);
    } else {
      console.warn('No screenMaps available for LED visual updates');
    }

    // Render the scene
    this.composer.render();
  }

  /**
   * Updates auto-brightness tracking and adjusts bloom effect
   * @private
   * @param {Map} positionMap - Map of LED positions to color data
   */
  _updateAutoBrightness(positionMap) {
    if (!positionMap || positionMap.size === 0) {
      return;
    }

    // Calculate average brightness from all LEDs
    let totalBrightness = 0;
    let count = 0;

    for (const [, ledData] of positionMap) {
      totalBrightness += ledData.brightness;
      count++;
    }

    // Set target brightness (0-1 range)
    this.target_brightness = count > 0 ? totalBrightness / count : 0;

    // Apply iris response delay (smoothly interpolate current to target)
    const delta = this.target_brightness - this.current_brightness;
    this.current_brightness += delta * this.iris_response_speed;

    // Calculate bloom strength based on brightness and density
    // Lower brightness = stronger bloom (like eyes dilating in darkness)
    // Higher brightness = weaker bloom (like pupils constricting)
    const densityFactor = count / Math.max(this.leds.length, 1);
    const invertedBrightness = 1.0 - this.current_brightness;

    // Scale bloom: brighter scenes get less bloom, darker scenes get more
    const bloomStrength = this.min_bloom_strength
      + (this.max_bloom_strength - this.min_bloom_strength)
      * invertedBrightness
      * densityFactor;

    // Update bloom pass if it exists
    this._updateBloomStrength(bloomStrength);
  }

  /**
   * Updates the bloom pass strength dynamically
   * @private
   * @param {number} strength - New bloom strength value
   */
  _updateBloomStrength(strength) {
    if (!this.composer || !this.composer.passes) {
      return;
    }

    // Find the bloom pass and update its strength
    for (const pass of this.composer.passes) {
      if (pass.constructor.name === 'UnrealBloomPass') {
        pass.strength = strength;
        this.bloom_stength = strength;
        break;
      }
    }
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

    // Initialize scene if it doesn't exist, LED count changed, or needsRebuild flag is set
    if (!this.scene || totalPixels !== this.previousTotalLeds || this.needsRebuild) {
      if (this.scene) {
        this.reset(); // Clear existing scene if LED count changed
      }
      this.initThreeJS(frameData);
      this.previousTotalLeds = totalPixels;
      this.needsRebuild = false; // Clear rebuild flag
    }
  }

  /**
   * Collects LED color data from frame data
   * @private
   * @returns {Map} - Map of LED positions to color data
   */
  _collectLedColorData(frameData) {
    // frameData is an array of strip data (pixel colors only)
    const dataArray = Array.isArray(frameData) ? frameData : (frameData.data || []);

    if (Object.keys(this.screenMaps).length === 0) {
      console.warn('No screenMaps available');
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

      // Get screenmap for this specific strip
      const screenMap = this.screenMaps[strip_id];
      if (!screenMap) {
        console.warn(`No screenMap found for strip ${strip_id}`);
        return;
      }

      // Look up strip data in its screenmap
      if (!screenMap.strips || !(strip_id in screenMap.strips)) {
        console.warn(`Strip ${strip_id} not found in its screenMap`);
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
   * Updates LED visuals based on position map data
   * @private
   */
  _updateLedVisuals(positionMap) {
    const { THREE } = this.threeJsModules;

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
          mergedMeshUpdates.get(led._parentMesh).push({
            index: led._mergedIndex,
            color: new THREE.Color(ledData.r, ledData.g, ledData.b),
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
