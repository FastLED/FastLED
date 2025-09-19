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

import { isDenseGrid } from './graphics_utils.js';

// Declare THREE as global namespace for type checking
/* global THREE */

/** Disable geometry merging for debugging (set to true to force individual LED objects) */
const DISABLE_MERGE_GEOMETRIES = false;

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
export class GraphicsManagerThreeJS {
  /**
   * Creates a new GraphicsManagerThreeJS instance
   * @param {Object} graphicsArgs - Configuration options
   * @param {string} graphicsArgs.canvasId - ID of the canvas element to render to
   * @param {Object} graphicsArgs.threeJsModules - Three.js modules and dependencies
   */
  constructor(graphicsArgs) {
    const { canvasId, threeJsModules } = graphicsArgs;

    // Configuration
    /** @type {string} HTML canvas element ID */
    this.canvasId = canvasId;

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
      this.renderer.setSize(this.SCREEN_WIDTH, this.SCREEN_HEIGHT);
    }
  }

  /**
   * Initializes the Three.js rendering environment
   * Sets up scene, camera, renderer, and post-processing pipeline
   * @param {Object} frameData - The frame data containing screen mapping information
   */
  initThreeJS(frameData) {
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
   * @param {Object} frameData - Frame data containing screen mapping
   */
  _setupCanvasAndDimensions(frameData) {
    const RESOLUTION_BOOST = 2; // 2x resolution for higher quality
    const MAX_WIDTH = 640; // Max pixels width on browser

    const canvas = document.getElementById(this.canvasId);
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

    // But keep display size the same
    canvas.style.width = `${targetWidth}px`;
    canvas.style.height = `${targetHeight}px`;
    canvas.style.maxWidth = `${targetWidth}px`;
    canvas.style.maxHeight = `${targetHeight}px`;

    // Store the bounds for orthographic camera calculations
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
    const canvas = document.getElementById(this.canvasId);

    this.renderer = new THREE.WebGLRenderer({
      canvas,
      antialias: true,
    });
    this.renderer.setSize(this.SCREEN_WIDTH, this.SCREEN_HEIGHT);
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
    const { screenMap } = frameData;

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
      if (!(stripId in screenMap.strips)) {
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
