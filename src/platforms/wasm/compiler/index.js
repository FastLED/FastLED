
// Selective bloom demo:
// https://discourse.threejs.org/t/totentanz-selective-bloom/8329


globalThis.loadFastLED = async function () {
    log("FastLED loader function was not set.");
    return null;
};

const urlParams = new URLSearchParams(window.location.search);
const FORCE_FAST_RENDERER = urlParams.get('gfx') === '0';
const FORCE_THREEJS_RENDERER = urlParams.get('gfx') === '1';

// Will be overridden during initialization.
var print = function () { };

const prev_console = console;

// Override console.log, console.warn, console.error to print to the output element
const _prev_log = prev_console.log;
const _prev_warn = prev_console.warn;
const _prev_error = prev_console.error;

function log(...args) {
    _prev_log(...args);
    try { print(...args); } catch (e) {
        __prev_log("Error in log", e);
    }
}
function warn(...args) {
    _prev_warn(...args);
    try { print(...args); } catch (e) {
        __prev_warn("Error in warn", e);
    }
}

function error(...args) {
    const stackTrace = new Error().stack;
    args = args + '\n' + stackTrace;
    _prev_error(args);
    try { print(args); } catch (e) {
        _prev_error("Error in error", e);
    }
}

console = {};
console.log = log;
console.warn = warn;
console.error = error;


function isDenseGrid(frameData) {
    const screenMap = frameData.screenMap;
    
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
            totalPixels += stripMap.map.length;
        }
    }

    const width = 1 + (screenMap.absMax[0] - screenMap.absMin[0]);
    const height = 1 + (screenMap.absMax[1] - screenMap.absMin[1]);
    const screenArea = width * height;
    const pixelDensity = totalPixels / screenArea;

    // Return true if density is close to 1 (indicating a grid)
    return pixelDensity > 0.9 && pixelDensity < 1.1;
}


async function initThreeJS(threeJsModules, containerId) {

    function initThreeJsStyle() {
        const styleSheet = "https://threejs.org/files/main.css";
        const style = document.createElement('link');
        style.rel = 'stylesheet';
        style.type = 'text/css';
        style.href = styleSheet;
        document.head.appendChild(style);
    }

    let camera, stats;
    let composer, renderer, mixer, clock;

    const params = {
        threshold: 0,
        strength: 1,
        radius: 0,
        exposure: 1
    };

    async function initThreeScene() {
        console.log("threeJsModules", threeJsModules);
        const { THREE, Stats, GUI, OrbitControls, GLTFLoader, EffectComposer, RenderPass, UnrealBloomPass, OutputPass } = threeJsModules;
        const container = document.getElementById(containerId);
        clock = new THREE.Clock();
        const scene = new THREE.Scene();

        camera = new THREE.PerspectiveCamera(40, window.innerWidth / window.innerHeight, 1, 100);
        camera.position.set(-5, 2.5, -3.5);
        scene.add(camera);

        scene.add(new THREE.AmbientLight(0xcccccc));

        const pointLight = new THREE.PointLight(0xffffff, 100);
        camera.add(pointLight);

        const loader = new GLTFLoader();
        const gltf = await loader.loadAsync('https://threejs.org/examples/models/gltf/PrimaryIonDrive.glb');

        const model = gltf.scene;
        scene.add(model);

        mixer = new THREE.AnimationMixer(model);
        const clip = gltf.animations[0];
        mixer.clipAction(clip.optimize()).play();

        renderer = new THREE.WebGLRenderer({ antialias: true });
        renderer.setPixelRatio(window.devicePixelRatio);
        renderer.setSize(window.innerWidth, window.innerHeight);
        renderer.setAnimationLoop(animate);
        renderer.toneMapping = THREE.ReinhardToneMapping;
        container.appendChild(renderer.domElement);

        const renderScene = new RenderPass(scene, camera);
        const bloomPass = new UnrealBloomPass(new THREE.Vector2(window.innerWidth, window.innerHeight), 1.5, 0.4, 0.85);
        bloomPass.threshold = params.threshold;
        bloomPass.strength = params.strength;
        bloomPass.radius = params.radius;

        const outputPass = new OutputPass();

        composer = new EffectComposer(renderer);
        composer.addPass(renderScene);
        composer.addPass(bloomPass);
        composer.addPass(outputPass);

        stats = new Stats();
        container.appendChild(stats.dom);

        const controls = new OrbitControls(camera, renderer.domElement);
        controls.maxPolarAngle = Math.PI * 0.5;
        controls.minDistance = 3;
        controls.maxDistance = 8;

        window.addEventListener('resize', onWindowResize);
    }

    function onWindowResize() {
        const width = window.innerWidth;
        const height = window.innerHeight;

        camera.aspect = width / height;
        camera.updateProjectionMatrix();

        renderer.setSize(width, height);
        composer.setSize(width, height);
    }

    function animate() {
        const delta = clock.getDelta();
        mixer.update(delta);
        stats.update();
        composer.render();
    }
    initThreeJsStyle();
    await initThreeScene();
}

class GraphicsManager {
    constructor(graphicsArgs) {
        const { canvasId } = graphicsArgs;
        this.canvasId = canvasId;
        this.gl = null;
        this.program = null;
        this.positionBuffer = null;
        this.texCoordBuffer = null;
        this.texture = null;
        this.texWidth = 0;
        this.texHeight = 0;
        this.texData = null;
    }

    createShaders() {
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


    initWebGL() {
        this.createShaders();
        const canvas = document.getElementById(this.canvasId);
        this.gl = canvas.getContext('webgl');
        if (!this.gl) {
            error('WebGL not supported');
            return;
        }

        // Create shaders
        const vertexShader = this.createShader(this.gl.VERTEX_SHADER, document.getElementById('fastled_vertexShader').text);
        const fragmentShader = this.createShader(this.gl.FRAGMENT_SHADER, document.getElementById('fastled_FragmentShader').text);

        // Create program
        this.program = this.createProgram(vertexShader, fragmentShader);

        // Create buffers
        this.positionBuffer = this.gl.createBuffer();
        this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.positionBuffer);
        this.gl.bufferData(this.gl.ARRAY_BUFFER, new Float32Array([-1, -1, 1, -1, -1, 1, 1, 1]), this.gl.STREAM_DRAW);

        this.texCoordBuffer = this.gl.createBuffer();
        this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.texCoordBuffer);
        this.gl.bufferData(this.gl.ARRAY_BUFFER, new Float32Array([0, 0, 1, 0, 0, 1, 1, 1]), this.gl.STREAM_DRAW);

        // Create texture
        this.texture = this.gl.createTexture();
        this.gl.bindTexture(this.gl.TEXTURE_2D, this.texture);
        this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_WRAP_S, this.gl.CLAMP_TO_EDGE);
        this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_WRAP_T, this.gl.CLAMP_TO_EDGE);
        this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MIN_FILTER, this.gl.NEAREST);
        this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MAG_FILTER, this.gl.NEAREST);
    }

    createShader(type, source) {
        const shader = this.gl.createShader(type);
        this.gl.shaderSource(shader, source);
        this.gl.compileShader(shader);
        if (!this.gl.getShaderParameter(shader, this.gl.COMPILE_STATUS)) {
            error('Shader compile error:', this.gl.getShaderInfoLog(shader));
            this.gl.deleteShader(shader);
            return null;
        }
        return shader;
    }

    createProgram(vertexShader, fragmentShader) {
        const program = this.gl.createProgram();
        this.gl.attachShader(program, vertexShader);
        this.gl.attachShader(program, fragmentShader);
        this.gl.linkProgram(program);
        if (!this.gl.getProgramParameter(program, this.gl.LINK_STATUS)) {
            error('Program link error:', this.gl.getProgramInfoLog(program));
            return null;
        }
        return program;
    }

    updateCanvas(frameData) {
        if (frameData.length === 0) {
            console.warn("Received empty frame data, skipping update");
            return;
        }

        if (!this.gl) this.initWebGL();

        const canvasWidth = this.gl.canvas.width;
        const canvasHeight = this.gl.canvas.height;

        // Check if we need to reallocate the texture
        const newTexWidth = Math.pow(2, Math.ceil(Math.log2(canvasWidth)));
        const newTexHeight = Math.pow(2, Math.ceil(Math.log2(canvasHeight)));

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
                null
            );

            // Reallocate texData buffer
            this.texData = new Uint8Array(this.texWidth * this.texHeight * 3);
        }

        const screenMap = frameData.screenMap;

        // Clear the texture data
        this.texData.fill(0);

        for (let i = 0; i < frameData.length; i++) {
            const strip = frameData[i];
            const data = strip.pixel_data;
            const strip_id = strip.strip_id;
            if (!(strip_id in screenMap.strips)) {
                console.warn(`No screen map found for strip ID ${strip_id}, skipping update`);
                continue;
            }
            const stripData = screenMap.strips[strip_id];
            const pixelCount = data.length / 3;
            const map = stripData.map;
            const min_x = screenMap.absMin[0];
            const min_y = screenMap.absMin[1];
            //log("Writing data to canvas");
            for (let i = 0; i < pixelCount; i++) {
                if (i >= map.length) {
                    console.warn(`Strip ${strip_id}: Pixel ${i} is outside the screen map ${map.length}, skipping update`);
                    continue;
                }
                let [x, y] = map[i];
                x -= min_x;
                y -= min_y;

                // Can't access the texture with floating point.
                x = Number.parseInt(x, 10);
                y = Number.parseInt(y, 10);

                // check to make sure that the pixel is within the canvas
                if (x < 0 || x >= canvasWidth || y < 0 || y >= canvasHeight) {
                    console.warn(`Strip ${strip_id}: Pixel ${i} is outside the canvas at ${x}, ${y}, skipping update`);
                    continue;
                }
                //log(x, y);
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
                            const r = data[srcIndex] & 0xFF;
                            const g = data[srcIndex + 1] & 0xFF;
                            const b = data[srcIndex + 2] & 0xFF;
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
            this.texData
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
            0, 0,
            canvasWidth / this.texWidth, 0,
            0, canvasHeight / this.texHeight,
            canvasWidth / this.texWidth, canvasHeight / this.texHeight,
        ]);
        this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.texCoordBuffer);
        this.gl.bufferData(this.gl.ARRAY_BUFFER, texCoords, this.gl.STREAM_DRAW);

        this.gl.drawArrays(this.gl.TRIANGLE_STRIP, 0, 4);
    }
}


class GraphicsManagerThreeJS {
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
            this.leds.forEach(led => {
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
        const screenMap = frameData.screenMap;
        const width = screenMap.absMax[0] - screenMap.absMin[0];
        const height = screenMap.absMax[1] - screenMap.absMin[1];

        const __screenWidth = this.SCREEN_WIDTH;
        const __screenHeight = this.SCREEN_HEIGHT;

        function calcXPosition(x) {
            return (x - screenMap.absMin[0]) / width * __screenWidth - __screenWidth / 2;
        }

        function calcYPosition(y) {
            return -((y - screenMap.absMin[1]) / height * __screenHeight - __screenHeight / 2);
        }
        return { calcXPosition, calcYPosition };
    }

    initThreeJS(frameData) {

        const { THREE, EffectComposer, RenderPass, UnrealBloomPass } = this.threeJsModules;
        const canvas = document.getElementById(this.canvasId);

        // Always set width to 640px and scale height proportionally
        const targetWidth = 640;
        const aspectRatio = canvas.height / canvas.width;
        const targetHeight = Math.round(targetWidth * aspectRatio);

        // Set the rendering resolution (2x the display size)
        this.SCREEN_WIDTH = targetWidth * 2;
        this.SCREEN_HEIGHT = targetHeight * 2;

        // Set internal canvas size to 2x for higher resolution
        canvas.width = targetWidth * 2;
        canvas.height = targetHeight * 2;
        // But keep display size the same
        canvas.style.width = targetWidth + 'px';
        canvas.style.height = targetHeight + 'px';
        canvas.style.maxWidth = targetWidth + 'px';
        canvas.style.maxHeight = targetHeight + 'px';

        this.scene = new THREE.Scene();
        const margin = 1.05;  // Add a small margin around the screen
        // Use perspective camera with narrower FOV for less distortion
        // BIG TODO: This camera setup does not respond to z-position changes. Eventually
        // we we want to have a camera that shows leds closer to the screen as larger.
        const fov = 45;
        const aspect = this.SCREEN_WIDTH / this.SCREEN_HEIGHT;
        this.camera = new THREE.PerspectiveCamera(fov, aspect, 0.1, 2000);
        // Position camera closer to fill more of the screen
        this.camera.position.z = Math.max(this.SCREEN_WIDTH, this.SCREEN_HEIGHT) * margin;
        this.camera.position.y = 0;

        this.renderer = new THREE.WebGLRenderer({
            canvas: canvas,
            antialias: true
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
                this.bloom_radius,  // radius
                0.0  // threshold
            );
            this.composer.addPass(bloomPass);
        }
    }

    createGrid(frameData) {
        const { THREE } = this.threeJsModules;
        const screenMap = frameData.screenMap;

        // Clear existing LEDs
        this.leds.forEach(led => {
            led.geometry.dispose();
            led.material.dispose();
            this.scene?.remove(led);
        });
        this.leds = [];

        // Calculate total number of LEDs and their positions
        let ledPositions = [];
        frameData.forEach(strip => {
            const stripId = strip.strip_id;
            if (stripId in screenMap.strips) {
                const stripMap = screenMap.strips[stripId];
                stripMap.map.forEach(pos => {
                    ledPositions.push(pos);
                });
            }
        });
        const width = screenMap.absMax[0] - screenMap.absMin[0];
        const height = screenMap.absMax[1] - screenMap.absMin[1];
        const { calcXPosition, calcYPosition } = this.makePositionCalculators(frameData);
        const isDenseScreenMap = isDenseGrid(frameData);
        let pixelDensityDefault = undefined;
        if (isDenseScreenMap) {
            console.log("Pixel density is close to 1, assuming grid or strip");
            pixelDensityDefault = Math.abs(calcXPosition(0) - calcXPosition(1));
        }

        const screenArea = width * height;
        // Use point diameter from screen map if available, otherwise calculate default
        const defaultDotSizeScale = Math.max(4, Math.sqrt(screenArea / (ledPositions.length * Math.PI)) * 0.4);
        const stripDotSizes = Object.values(screenMap.strips).map(strip => strip.diameter);
        const avgPointDiameter = stripDotSizes.reduce((a, b) => a + b, 0) / stripDotSizes.length;
        let defaultDotSize = defaultDotSizeScale * avgPointDiameter;
        if (pixelDensityDefault) {
            // Override default dot size if pixel density is close to 1 for this dense strip.
            defaultDotSize = pixelDensityDefault;
        }

        const normalizedScale = this.SCREEN_WIDTH / width;


        // Create LEDs at mapped positions
        let ledIndex = 0;
        frameData.forEach(strip => {
            const stripId = strip.strip_id;
            if (stripId in screenMap.strips) {
                const stripData = screenMap.strips[stripId];
                let stripDiameter = null;
                if (stripData.diameter) {
                    stripDiameter = stripData.diameter * normalizedScale;
                } else {
                    stripDiameter = defaultDotSize;
                }
                stripData.map.forEach(pos => {
                    let geometry;
                    if (isDenseScreenMap) {
                        const width = stripDiameter * this.LED_SCALE;
                        const height = stripDiameter * this.LED_SCALE;
                        geometry = new THREE.PlaneGeometry(width, height);
                    } else {
                        geometry = new THREE.CircleGeometry(stripDiameter * this.LED_SCALE, this.SEGMENTS);

                    }
                    const material = new THREE.MeshBasicMaterial({ color: 0x000000 });
                    const led = new THREE.Mesh(geometry, material);

                    // Position LED according to map, normalized to screen coordinates
                    const x = calcXPosition(pos[0]);
                    const y = calcYPosition(pos[1]);
                    led.position.set(x, y, 500);

                    this.scene.add(led);
                    this.leds.push(led);
                    ledIndex++;
                });
            }
        });
        return {isDenseScreenMap}
    }

    updateCanvas(frameData) {

        if (frameData.length === 0) {
            console.warn("Received empty frame data, skipping update");
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

        const screenMap = frameData.screenMap;

        // Create a map to store LED data by position
        const positionMap = new Map();

        // First pass: collect all LED data and positions
        frameData.forEach(strip => {
            const strip_id = strip.strip_id;
            if (!(strip_id in screenMap.strips)) {
                console.warn(`No screen map found for strip ID ${strip_id}, skipping update`);
                return;
            }

            const stripData = screenMap.strips[strip_id];
            const map = stripData.map;
            const data = strip.pixel_data;
            const pixelCount = data.length / 3;

            for (let j = 0; j < pixelCount; j++) {
                if (j >= map.length) {
                    console.warn(`Strip ${strip_id}: Pixel ${j} is outside the screen map ${map.length}, skipping update`);
                    continue;
                }

                const [x, y] = map[j];
                const posKey = `${x},${y}`;
                const srcIndex = j * 3;
                const r = (data[srcIndex] & 0xFF) / 255;
                const g = (data[srcIndex + 1] & 0xFF) / 255;
                const b = (data[srcIndex + 2] & 0xFF) / 255;
                const brightness = (r + g + b) / 3;

                // Only update if this LED is brighter than any existing LED at this position
                if (!positionMap.has(posKey) || positionMap.get(posKey).brightness < brightness) {
                    positionMap.set(posKey, {
                        x: x,
                        y: y,
                        r: r,
                        g: g,
                        b: b,
                        brightness: brightness
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
        for (const [_, ledData] of positionMap) {
            if (ledIndex >= this.leds.length) break;

            const led = this.leds[ledIndex];
            const x = ledData.x - min_x;
            const y = ledData.y - min_y;

            // Convert to normalized coordinates
            const normalizedX = (x / width) * this.SCREEN_WIDTH - this.SCREEN_WIDTH / 2;
            const normalizedY = (y / height) * this.SCREEN_HEIGHT - this.SCREEN_HEIGHT / 2;
            
            // Calculate z position based on distance from center for subtle depth
            const distFromCenter = Math.sqrt(Math.pow(normalizedX, 2) + Math.pow(normalizedY, 2));
            const maxDist = Math.sqrt(Math.pow(this.SCREEN_WIDTH/2, 2) + Math.pow(this.SCREEN_HEIGHT/2, 2));
            const z = (distFromCenter / maxDist) * 100;  // Max depth of 100 units
            
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



class UiManager {
    constructor(uiControlsId) {
        this.uiElements = {};
        this.previousUiState = {};
        this.uiControlsId = uiControlsId;
    }

    processUiChanges(uiUpdateCallback) {
        const changes = {};
        let hasChanges = false;
        for (const id in this.uiElements) {
            const element = this.uiElements[id];
            let currentValue;
            if (element.type === 'checkbox') {
                currentValue = element.checked;
            } else if (element.type === 'submit') {
                let attr = element.getAttribute('data-pressed');
                currentValue = attr === 'true';
            } else if (element.type === 'number') {
                currentValue = parseFloat(element.value);
            } else {
                currentValue = parseFloat(element.value);
            }
            if (this.previousUiState[id] !== currentValue) {
                changes[id] = currentValue;
                hasChanges = true;
                this.previousUiState[id] = currentValue;
            }
        }
        if (hasChanges) {
            const data = JSON.stringify(changes);
            uiUpdateCallback(data);
        }
    }

    addUiElements(jsonData) {
        console.log("UI elements added:", jsonData);


        const uiControlsContainer = document.getElementById(this.uiControlsId) || this.createUiControlsContainer();

        let foundUi = false;
        jsonData.forEach(data => {
            console.log("data:", data);
            const group = data.group;
            const hasGroup = group !== "" && group !== undefined;
            if (hasGroup) {
                console.log(`Group ${group} found, for item ${data.name}`);
            }

            let control;
            if (data.type === 'slider') {
                control = this.createSlider(data);
            } else if (data.type === 'checkbox') {
                control = this.createCheckbox(data);
            } else if (data.type === 'button') {
                control = this.createButton(data);
            } else if (data.type === 'number') {
                control = this.createNumberField(data);
            }

            if (control) {
                foundUi = true;
                uiControlsContainer.appendChild(control);
                if (data.type === 'button') {
                    this.uiElements[data.id] = control.querySelector('button');
                } else {
                    this.uiElements[data.id] = control.querySelector('input');
                }
                this.previousUiState[data.id] = data.value;
            }
        });
        if (foundUi) {
            console.log("UI elements added, showing UI controls container");
            uiControlsContainer.classList.add('active');
        }
    }

    createUiControlsContainer() {
        const container = document.getElementById(this.uiControlsId);
        if (!container) {
            error('UI controls container not found in the HTML');
        }
        return container;
    }

    createNumberField(element) {
        const controlDiv = document.createElement('div');
        controlDiv.className = 'ui-control';

        const label = document.createElement('label');
        label.textContent = element.name;
        label.htmlFor = `number-${element.id}`;

        const numberInput = document.createElement('input');
        numberInput.type = 'number';
        numberInput.id = `number-${element.id}`;
        numberInput.value = element.value;
        numberInput.min = element.min;
        numberInput.max = element.max;
        numberInput.step = (element.step !== undefined) ? element.step : 'any';

        controlDiv.appendChild(label);
        controlDiv.appendChild(numberInput);

        return controlDiv;
    }

    createSlider(element) {
        const controlDiv = document.createElement('div');
        controlDiv.className = 'ui-control';

        const labelValueContainer = document.createElement('div');
        labelValueContainer.style.display = 'flex';
        labelValueContainer.style.justifyContent = 'space-between';
        labelValueContainer.style.width = '100%';

        const label = document.createElement('label');
        label.textContent = element.name;
        label.htmlFor = `slider-${element.id}`;

        const valueDisplay = document.createElement('span');
        valueDisplay.textContent = element.value;

        labelValueContainer.appendChild(label);
        labelValueContainer.appendChild(valueDisplay);

        const slider = document.createElement('input');
        slider.type = 'range';
        slider.id = `slider-${element.id}`;
        slider.min = Number.parseFloat(element.min);
        slider.max = Number.parseFloat(element.max);
        slider.value = Number.parseFloat(element.value);
        slider.step = Number.parseFloat(element.step);
        setTimeout(() => {
            // Sets the slider value, for some reason we have to do it
            // next frame.
            slider.value = Number.parseFloat(element.value);
            valueDisplay.textContent = slider.value;
        }, 0);


        slider.addEventListener('input', function () {
            valueDisplay.textContent = this.value;
        });

        controlDiv.appendChild(labelValueContainer);
        controlDiv.appendChild(slider);

        return controlDiv;
    }

    createCheckbox(element) {
        const controlDiv = document.createElement('div');
        controlDiv.className = 'ui-control';

        const label = document.createElement('label');
        label.textContent = element.name;
        label.htmlFor = `checkbox-${element.id}`;

        const checkbox = document.createElement('input');
        checkbox.type = 'checkbox';
        checkbox.id = `checkbox-${element.id}`;
        checkbox.checked = element.value;

        const flexContainer = document.createElement('div');
        flexContainer.style.display = 'flex';
        flexContainer.style.alignItems = 'center';
        flexContainer.style.justifyContent = 'space-between';

        flexContainer.appendChild(label);
        flexContainer.appendChild(checkbox);

        controlDiv.appendChild(flexContainer);

        return controlDiv;
    }

    createButton(element) {
        const controlDiv = document.createElement('div');
        controlDiv.className = 'ui-control';

        const button = document.createElement('button');
        button.textContent = element.name;
        button.id = `button-${element.id}`;
        button.setAttribute('data-pressed', 'false');

        button.addEventListener('mousedown', function () {
            this.setAttribute('data-pressed', 'true');
            this.classList.add('active');
        });

        button.addEventListener('mouseup', function () {
            this.setAttribute('data-pressed', 'false');
            this.classList.remove('active');
        });

        button.addEventListener('mouseleave', function () {
            this.setAttribute('data-pressed', 'false');
            this.classList.remove('active');
        });
        controlDiv.appendChild(button);
        return controlDiv;
    }
}


(function () {
    const DEFAULT_FRAME_RATE_60FPS = 60; // 60 FPS
    let frameRate = DEFAULT_FRAME_RATE_60FPS;
    let receivedCanvas = false;
    // screenMap contains data mapping a strip id to a screen map,
    // transforming led strip data pixel with an index
    // to a screen pixel with xy.
    let screenMap = {
        strips: {},
    };
    let canvasId;
    let uiControlsId;
    let outputId;

    let uiManager;
    let uiCanvasChanged = false;
    let threeJsModules = {};  // For graphics.
    let graphicsManager;
    let containerId;  // for ThreeJS
    let graphicsArgs = {};

    function customPrintFunction(...args) {
        if (containerId === undefined) {
            return;  // Not ready yet.
        }
        // take the args and stringify them, then add them to the output element
        let cleanedArgs = args.map(arg => {
            if (typeof arg === 'object') {
                try {
                    return JSON.stringify(arg).slice(0, 100);
                } catch (e) {
                    return "" + arg;
                }
            }
            return arg;
        });
        const output = document.getElementById(outputId);
        const allText = output.textContent + [...cleanedArgs].join(' ') + '\n';
        // split into lines, and if there are more than 100 lines, remove one.
        const lines = allText.split('\n');
        while (lines.length > 100) {
            lines.shift();
        }
        output.textContent = lines.join('\n');
    }


    function minMax(array_xy) {
        // array_xy is a an array of an array of x and y values
        // returns the lower left and upper right
        let min_x = array_xy[0][0];
        let min_y = array_xy[0][1];
        let max_x = array_xy[0][0];
        let max_y = array_xy[0][1];
        for (let i = 1; i < array_xy.length; i++) {
            min_x = Math.min(min_x, array_xy[i][0]);
            min_y = Math.min(min_y, array_xy[i][1]);
            max_x = Math.max(max_x, array_xy[i][0]);
            max_y = Math.max(max_y, array_xy[i][1]);
        }
        return [[min_x, min_y], [max_x, max_y]];
    }

    globalThis.FastLED_onStripUpdate = function (jsonData) {
        console.log("Received strip update:", jsonData);

        const event = jsonData.event;
        let width = 0;
        let height = 0;
        let eventHandled = false;
        if (event === "set_canvas_map") {
            eventHandled = true;
            // Work in progress.
            const map = jsonData.map;

            console.log("Received map:", jsonData);



            const [min, max] = minMax(map);
            console.log("min", min, "max", max);

            const stripId = jsonData.strip_id;
            const isUndefined = (value) => typeof value === 'undefined';
            if (isUndefined(stripId)) {
                throw new Error("strip_id is required for set_canvas_map event");
            }

            screenMap.strips[stripId] = {
                map: map,
                min: min,
                max: max,
                diameter: jsonData.diameter
            };


            console.log("Screen map updated:", screenMap);
            // iterate through all the screenMaps and get the absolute min and max
            let absMin = [Number.POSITIVE_INFINITY, Number.POSITIVE_INFINITY];
            let absMax = [Number.NEGATIVE_INFINITY, Number.NEGATIVE_INFINITY];
            let setAtLeastOnce = false;
            for (const stripId in screenMap.strips) {
                console.log("Processing strip ID", stripId);
                const id = Number.parseInt(stripId, 10);
                const stripData = screenMap.strips[id];
                absMin[0] = Math.min(absMin[0], stripData.min[0]);
                absMin[1] = Math.min(absMin[1], stripData.min[1]);
                absMax[0] = Math.max(absMax[0], stripData.max[0]);
                absMax[1] = Math.max(absMax[1], stripData.max[1]);
                setAtLeastOnce = true;
            }
            if (!setAtLeastOnce) {
                error("No screen map data found, skipping canvas size update");
                return;
            }
            screenMap.absMin = absMin;
            screenMap.absMax = absMax;
            width = Number.parseInt(absMax[0] - absMin[0], 10) + 1;
            height = Number.parseInt(absMax[1] - absMin[1], 10) + 1;
            console.log("canvas updated with width and height", width, height);
            // now update the canvas size.
            const canvas = document.getElementById(canvasId);
            canvas.width = width;
            canvas.height = height;
            uiCanvasChanged = true;
            console.log("Screen map updated:", screenMap);
        }

        if (!eventHandled) {
            console.warn(`We do not support event ${event} yet.`);
            return;
        }
        if (receivedCanvas) {
            console.warn("Canvas size has already been set, setting multiple canvas sizes is not supported yet and the previous one will be overwritten.");
        }
        const canvas = document.getElementById(canvasId);
        canvas.width = width;
        canvas.height = height;

        // Set display size (CSS pixels) to 640px width while maintaining aspect ratio
        const displayWidth = 640;
        const displayHeight = Math.round((height / width) * displayWidth);

        // Set CSS display size while maintaining aspect ratio
        canvas.style.width = displayWidth + 'px';
        canvas.style.height = displayHeight + 'px';
        console.log(`Canvas size set to ${width}x${height}, displayed at ${canvas.style.width}x${canvas.style.height} `);
        // unconditionally delete the graphicsManager
        if (graphicsManager) {
            graphicsManager.reset();
            graphicsManager = null;
        }
    };



    globalThis.FastLED_onStripAdded = function (stripId, stripLength) {
        const output = document.getElementById(outputId);
        output.textContent += `Strip added: ID ${stripId}, length ${stripLength}\n`;
    };



    globalThis.FastLED_onFrame = function (frameData, uiUpdateCallback) {
        uiManager.processUiChanges(uiUpdateCallback);
        if (frameData.length === 0) {
            console.warn("Received empty frame data, skipping update");
            return;
        }

        updateCanvas(frameData);
    };

    globalThis.FastLED_onUiElementsAdded = function (jsonData) {
        uiManager.addUiElements(jsonData);
    };

    // Function to call the setup and loop functions
    function runFastLED(extern_setup, extern_loop, frame_rate, moduleInstance) {
        console.log("Calling setup function...");
        extern_setup();

        console.log("Starting loop...");
        const frameInterval = 1000 / frame_rate;
        let lastFrameTime = 0;

        // Executes every frame but only runs the loop function at the specified frame rate
        function runLoop(currentTime) {
            if (currentTime - lastFrameTime >= frameInterval) {
                extern_loop();
                lastFrameTime = currentTime;
            }
            requestAnimationFrame(runLoop);
        }
        requestAnimationFrame(runLoop);
    }




    function updateCanvas(frameData) {
       // we are going to add the screenMap to the graphicsManager
       frameData.screenMap = screenMap;
        if (!graphicsManager) {
            const isDenseMap = isDenseGrid(frameData);
            if (FORCE_THREEJS_RENDERER) {
                console.log("Creating Beautiful GraphicsManager with canvas ID (forced)", canvasId);
                graphicsManager = new GraphicsManagerThreeJS(graphicsArgs);
            } else if (FORCE_FAST_RENDERER) {
                console.log("Creating Fast GraphicsManager with canvas ID (forced)", canvasId);
                graphicsManager = new GraphicsManager(graphicsArgs);
            } else if (isDenseMap) {
                console.log("Creating Fast GraphicsManager with canvas ID", canvasId);
                graphicsManager = new GraphicsManager(graphicsArgs);
            } else {
                console.log("Creating Beautiful GraphicsManager with canvas ID", canvasId);
                graphicsManager = new GraphicsManagerThreeJS(graphicsArgs);
            }
            uiCanvasChanged = false;
        }

        if (uiCanvasChanged) {
            uiCanvasChanged = false;
            graphicsManager.reset();
        }

 
        graphicsManager.updateCanvas(frameData);
    }


    // Ensure we wait for the module to load
    const onModuleLoaded = async (fastLedLoader) => {
        // Unpack the module functions and send them to the runFastLED function
        function __runFastLED(moduleInstance, frameRate) {
            const exports_exist = moduleInstance && moduleInstance._extern_setup && moduleInstance._extern_loop;
            if (!exports_exist) {
                console.error("FastLED setup or loop functions are not available.");
                return;
            }
            return runFastLED(moduleInstance._extern_setup, moduleInstance._extern_loop, frameRate, moduleInstance);
        }

        try {
            if (typeof fastLedLoader === 'function') {
                // Load the module
                fastLedLoader().then(instance => {
                    console.log("Module loaded, running FastLED...");
                    __runFastLED(instance, frameRate);
                }).catch(err => {
                    console.error("Error loading fastled as a module:", err);
                });
            } else {
                console.log("Could not detect a valid module loading for FastLED, expected function but got", typeof fastledLoader);
            }
        } catch (error) {
            console.error("Failed to load FastLED:", error);
        }
    };



    async function loadFastLed(options) {
        canvasId = options.canvasId;
        uiControlsId = options.uiControlsId;
        outputId = options.printId;
        print = customPrintFunction;
        console.log("Loading FastLED with options:", options);
        frameRate = options.frameRate || DEFAULT_FRAME_RATE_60FPS;
        uiManager = new UiManager(uiControlsId);
        threeJs = options.threeJs;
        console.log("ThreeJS:", threeJs);
        const fastLedLoader = options.fastled;
        threeJsModules = threeJs.modules;
        containerId = threeJs.containerId;
        console.log("ThreeJS modules:", threeJsModules);
        console.log("Container ID:", containerId);
        graphicsArgs = {
            canvasId: canvasId,
            threeJsModules: threeJsModules
        }
        await onModuleLoaded(fastLedLoader);
    }
    globalThis.loadFastLED = loadFastLed;
})();
