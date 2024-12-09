/* eslint-disable no-console */
/* eslint-disable import/prefer-default-export */
/* eslint-disable no-restricted-syntax */
/* eslint-disable max-len */
/* eslint-disable guard-for-in */
/* eslint-disable camelcase */
/* eslint-disable no-underscore-dangle */
/* eslint-disable no-plusplus */
/* eslint-disable no-continue */

// Selective bloom demo:
// https://discourse.threejs.org/t/totentanz-selective-bloom/8329

// Note that this is shared with index.js
export function isDenseGrid(frameData) {
  const { screenMap } = frameData;

  // Check if all pixel densities are undefined
  let allPixelDensitiesUndefined = true;
  for (const stripId in screenMap.strips) {
    const strip = screenMap.strips[stripId];
    allPixelDensitiesUndefined = allPixelDensitiesUndefined && (strip.diameter === undefined);
    if (!allPixelDensitiesUndefined) {
      break;
    }
  }

  if (!allPixelDensitiesUndefined) {
    return false;
  }

  // Calculate total pixels and screen area
  let totalPixels = 0;
  for (const strip of frameData) {
    if (strip.strip_id in screenMap.strips) {
      const stripMap = screenMap.strips[strip.strip_id];
      const len = Math.min(stripMap.map.x.length, stripMap.map.y.length);
      totalPixels += len;
    }
  }

  const width = 1 + (screenMap.absMax[0] - screenMap.absMin[0]);
  const height = 1 + (screenMap.absMax[1] - screenMap.absMin[1]);
  const screenArea = width * height;
  const pixelDensity = totalPixels / screenArea;

  // Return true if density is close to 1 (indicating a grid)
  return pixelDensity > 0.9 && pixelDensity < 1.1;
}

export class GraphicsManagerThreeJS {
  constructor(graphicsArgs) {
    const { canvasId, threeJsModules } = graphicsArgs;
    this.canvasId = canvasId;
    this.threeJsModules = threeJsModules;
    this.SEGMENTS = 16;
    this.LED_SCALE = 1.0;
    this.leds = [];
    this.scene = null;
    this.camera = null;
    this.renderer = null;
    this.composer = null;
    this.previousTotalLeds = 0;
    this.bloom_stength = 1;
    this.bloom_radius = 16;
  }

  reset() {
    // Clean up existing objects
    if (this.leds) {
      this.leds.forEach((led) => {
        led.geometry.dispose();
        led.material.dispose();
        this.scene?.remove(led);
      });
    }
    this.leds = [];

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

  makePositionCalculators(frameData) {
    // Calculate dot size based on LED density
    const { screenMap } = frameData;
    const width = screenMap.absMax[0] - screenMap.absMin[0];
    const height = screenMap.absMax[1] - screenMap.absMin[1];

    const __screenWidth = this.SCREEN_WIDTH;
    const __screenHeight = this.SCREEN_HEIGHT;

    function calcXPosition(x) {
      return (((x - screenMap.absMin[0]) / width) * __screenWidth) - (__screenWidth / 2);
    }

    function calcYPosition(y) {
      const negY = (((y - screenMap.absMin[1]) / height) * __screenHeight) - (__screenHeight / 2);
      return -negY;
    }
    return { calcXPosition, calcYPosition };
  }

  initThreeJS(frameData) {
    const FOV = 45;
    const margin = 1.05; // Add a small margin around the screen
    const RESOLUTION_BOOST = 2; // 2x resolution for higher quality
    const MAX_WIDTH = 640; // Max pixels width on browser.

    const {
      THREE, EffectComposer, RenderPass, UnrealBloomPass,
    } = this.threeJsModules;
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
    const circleRadius = Math.max(this.SCREEN_WIDTH, this.SCREEN_HEIGHT) * 0.5;
    const cameraZ = (circleRadius / Math.tan(THREE.MathUtils.degToRad(FOV / 2))) * margin;

    this.scene = new THREE.Scene();

    // Use perspective camera with narrower FOV for less distortion
    this.camera = new THREE.PerspectiveCamera(FOV, aspectRatio, 0.1, 5000);
    // Adjust camera position to ensure the circle fits within the view
    this.camera.position.z = cameraZ;
    this.camera.position.y = 0;

    this.renderer = new THREE.WebGLRenderer({
      canvas,
      antialias: true,
    });
    this.renderer.setSize(this.SCREEN_WIDTH, this.SCREEN_HEIGHT);
    const renderScene = new RenderPass(this.scene, this.camera);
    this.composer = new EffectComposer(this.renderer);
    this.composer.addPass(renderScene);

    // Create LED grid.
    const { isDenseScreenMap } = this.createGrid(frameData);

    if (!isDenseScreenMap) {
      this.bloom_stength = 16;
      this.bloom_radius = 1;
    } else {
      this.bloom_stength = 0;
      this.bloom_radius = 0;
    }

    if (this.bloom_stength > 0 || this.bloom_radius > 0) {
      const bloomPass = new UnrealBloomPass(
        new THREE.Vector2(this.SCREEN_WIDTH, this.SCREEN_HEIGHT),
        this.bloom_stength,
        this.bloom_radius, // radius
        0.0, // threshold
      );
      this.composer.addPass(bloomPass);
    }
  }

  createGrid(frameData) {
    const { THREE } = this.threeJsModules;
    const { screenMap } = frameData;

    // Clear existing LEDs
    this.leds.forEach((led) => {
      led.geometry.dispose();
      led.material.dispose();
      this.scene?.remove(led);
    });
    this.leds = [];

    // Calculate total number of LEDs and their positions
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
    const width = screenMap.absMax[0] - screenMap.absMin[0];
    const height = screenMap.absMax[1] - screenMap.absMin[1];
    const { calcXPosition, calcYPosition } = this.makePositionCalculators(frameData);
    const isDenseScreenMap = isDenseGrid(frameData);
    let pixelDensityDefault;
    if (isDenseScreenMap) {
      console.log('Pixel density is close to 1, assuming grid or strip');
      pixelDensityDefault = Math.abs(calcXPosition(0) - calcXPosition(1));
    }

    const screenArea = width * height;
    // Use point diameter from screen map if available, otherwise calculate default
    const defaultDotSizeScale = Math.max(4, Math.sqrt(screenArea / (ledPositions.length * Math.PI)) * 0.4);
    const stripDotSizes = Object.values(screenMap.strips).map((strip) => strip.diameter);
    const avgPointDiameter = stripDotSizes.reduce((a, b) => a + b, 0) / stripDotSizes.length;
    let defaultDotSize = defaultDotSizeScale * avgPointDiameter;
    if (pixelDensityDefault) {
      // Override default dot size if pixel density is close to 1 for this dense strip.
      defaultDotSize = pixelDensityDefault;
    }

    const normalizedScale = this.SCREEN_WIDTH / width;

    // Create LEDs at mapped positions
    frameData.forEach((strip) => {
      const stripId = strip.strip_id;
      if (stripId in screenMap.strips) {
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
          let geometry;
          if (isDenseScreenMap) {
            const w = stripDiameter * this.LED_SCALE;
            const h = stripDiameter * this.LED_SCALE;
            geometry = new THREE.PlaneGeometry(w, h);
          } else {
            geometry = new THREE.CircleGeometry(stripDiameter * this.LED_SCALE, this.SEGMENTS);
          }
          const material = new THREE.MeshBasicMaterial({ color: 0x000000 });
          const led = new THREE.Mesh(geometry, material);
          // Position LED according to map, normalized to screen coordinates
          const x = calcXPosition(x_array[i]);
          const y = calcYPosition(y_array[i]);
          led.position.set(x, y, 500);
          this.scene.add(led);
          this.leds.push(led);
        }
      }
    });
    return { isDenseScreenMap };
  }

  updateCanvas(frameData) {
    if (frameData.length === 0) {
      console.warn('Received empty frame data, skipping update');
      return;
    }
    const totalPixels = frameData.reduce((acc, strip) => acc + strip.pixel_data.length / 3, 0);

    // Initialize scene if it doesn't exist or if LED count changed
    if (!this.scene || totalPixels !== this.previousTotalLeds) {
      if (this.scene) {
        this.reset(); // Clear existing scene if LED count changed
      }
      this.initThreeJS(frameData);
      this.previousTotalLeds = totalPixels;
    }

    const { screenMap } = frameData;

    // Create a map to store LED data by position
    const positionMap = new Map();

    // First pass: collect all LED data and positions
    frameData.forEach((strip) => {
      const { strip_id } = strip;
      if (!(strip_id in screenMap.strips)) {
        console.warn(`No screen map found for strip ID ${strip_id}, skipping update`);
        return;
      }

      const stripData = screenMap.strips[strip_id];
      const { map } = stripData;
      const data = strip.pixel_data;
      const pixelCount = data.length / 3;
      const x_array = stripData.map.x;
      const y_array = stripData.map.y;
      const length = Math.min(x_array.length, y_array.length);

      for (let j = 0; j < pixelCount; j++) {
        if (j >= length) {
          console.warn(`Strip ${strip_id}: Pixel ${j} is outside the screen map ${map.length}, skipping update`);
          continue;
        }
        const x = x_array[j];
        const y = y_array[j];
        const posKey = `${x},${y}`;
        const srcIndex = j * 3;
        const r = (data[srcIndex] & 0xFF) / 255;  // eslint-disable-line
        const g = (data[srcIndex + 1] & 0xFF) / 255;  // eslint-disable-line
        const b = (data[srcIndex + 2] & 0xFF) / 255;  // eslint-disable-line
        const brightness = (r + g + b) / 3;

        // Only update if this LED is brighter than any existing LED at this position
        if (!positionMap.has(posKey) || positionMap.get(posKey).brightness < brightness) {
          positionMap.set(posKey, {
            x,
            y,
            r,
            g,
            b,
            brightness,
          });
        }
      }
    });

    // Calculate normalized coordinates
    const min_x = screenMap.absMin[0];
    const min_y = screenMap.absMin[1];
    const width = screenMap.absMax[0] - min_x;
    const height = screenMap.absMax[1] - min_y;

    // Second pass: update LED positions and colors
    let ledIndex = 0;
    for (const [_, ledData] of positionMap) {  // eslint-disable-line
      if (ledIndex >= this.leds.length) break;

      const led = this.leds[ledIndex];
      const x = ledData.x - min_x;
      const y = ledData.y - min_y;

      // Convert to normalized coordinates
      const normalizedX = (x / width) * this.SCREEN_WIDTH - this.SCREEN_WIDTH / 2;
      const normalizedY = (y / height) * this.SCREEN_HEIGHT - this.SCREEN_HEIGHT / 2;

      // Calculate z position based on distance from center for subtle depth
      const distFromCenter = Math.sqrt(normalizedX ** 2 + normalizedY ** 2);
      const maxDist = Math.sqrt((this.SCREEN_WIDTH / 2) ** 2 + (this.SCREEN_HEIGHT / 2) ** 2);
      const z = (distFromCenter / maxDist) * 100; // Max depth of 100 units

      led.position.set(normalizedX, normalizedY, z);
      led.material.color.setRGB(ledData.r, ledData.g, ledData.b);
      ledIndex++;
    }

    // Clear any remaining LEDs
    for (let i = ledIndex; i < this.leds.length; i++) {
      this.leds[i].material.color.setRGB(0, 0, 0);
      this.leds[i].position.set(-1000, -1000, 0); // Move offscreen
    }

    this.composer.render();
  }
}
