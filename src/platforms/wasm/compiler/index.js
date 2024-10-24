
globalThis.loadFastLED = async function () {
    console.log("FastLED loader function was not set.");
    return null;
};

class GraphicsManager {
    constructor(canvasId) {
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
            console.error('WebGL not supported');
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
            console.error('Shader compile error:', this.gl.getShaderInfoLog(shader));
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
            console.error('Program link error:', this.gl.getProgramInfoLog(program));
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
            //console.log("Writing data to canvas");
            for (let i = 0; i < pixelCount; i++) {
                if (i >= map.length) {
                    console.warn(`Strip ${strip_id}: Pixel ${i} is outside the screen map ${map.length}, skipping update`);
                    continue;
                }
                let [x, y] = map[i];
                x -= min_x;
                y -= min_y;

                // check to make sure that the pixel is within the canvas
                if (x < 0 || x >= canvasWidth || y < 0 || y >= canvasHeight) {
                    console.warn(`Strip ${strip_id}: Pixel ${i} is outside the canvas at ${x}, ${y}, skipping update`);
                    continue;
                }
                //console.log(x, y);
                const srcIndex = i * 3;
                const destIndex = (y * this.texWidth + x) * 3;
                const r = data[srcIndex];
                const g = data[srcIndex + 1];
                const b = data[srcIndex + 2];
                this.texData[destIndex] = r;
                this.texData[destIndex + 1] = g;
                this.texData[destIndex + 2] = b;
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
            console.error('UI controls container not found in the HTML');
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
        },0);


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

            // convert to integers - this is a temporary fix until we can
            // figure out why float isn't working with the engine.
            for (let i = 0; i < map.length; i++) {
                map[i][0] = Number.parseInt(map[i][0], 10);
                map[i][1] = Number.parseInt(map[i][1], 10);
            }

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
                console.error("No screen map data found, skipping canvas size update");
                return;
            }
            screenMap.absMin = absMin;
            screenMap.absMax = absMax;
            width = Number.parseInt(absMax[0] - absMin[0], 10) + 1;
            height = Number.parseInt(absMax[1] - absMin[1], 10) + 1;
            console.log("canvas updated with witdt and height", width, height);
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

        const maxDisplaySize = 640; // Maximum display size in pixels
        const scaleFactor = Math.min(maxDisplaySize / width, maxDisplaySize / height, 20);

        canvas.style.width = Math.round(width * scaleFactor) + 'px';
        canvas.style.height = Math.round(height * scaleFactor) + 'px';
        console.log(`Canvas size set to ${width}x${height}, displayed at ${canvas.style.width}x${canvas.style.height} `);
    };

    globalThis.FastLED_onStripAdded = function (stripId, stripLength) {
        const output = document.getElementById(outputId);
        output.textContent += `Strip added: ID ${stripId}, length ${stripLength}\n`;
    };

    let graphicsManager;

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
        if (!graphicsManager) {
            graphicsManager = new GraphicsManager(canvasId);
            uiCanvasChanged = false;
        }

        if (uiCanvasChanged) {
            uiCanvasChanged = false;
            graphicsManager.reset();
        }

        // we are going to add the screenMap to the graphicsManager
        frameData.screenMap = screenMap;
        graphicsManager.updateCanvas(frameData);
    }


    // Ensure we wait for the module to load
    const onModuleLoaded = async () => {
        // Unpack the module functions and send them to the runFastLED function
        function __runFastLED(moduleInstance, frameRate) {
            const exports_exist = moduleInstance && moduleInstance._extern_setup && moduleInstance._extern_loop;
            if (!exports_exist) {
                console.error("FastLED setup or loop functions are not available.");
                return;
            }
            return runFastLED(moduleInstance._extern_setup, moduleInstance._extern_loop, frameRate, moduleInstance);
        }
        var fastledLoader = globalThis.fastled;
        try {
            if (typeof fastledLoader === 'function') {
                // Load the module
                fastledLoader().then(instance => {
                    console.log("Module loaded, running FastLED...");
                    __runFastLED(instance, frameRate);
                }).catch(err => {
                    console.error("Error loading fastled as a module:", err);
                });
            } else {
                console.log("Could not detect a valid module loading for FastLED.");
            }
        } catch (error) {
            console.error("Failed to load FastLED:", error);
        }
    };
    async function loadFastLed(options) {
        canvasId = options.canvasId;
        uiControlsId = options.uiControlsId;
        outputId = options.printId;
        frameRate = options.frameRate || DEFAULT_FRAME_RATE_60FPS;
        uiManager = new UiManager(uiControlsId);
        await onModuleLoaded();
    }
    globalThis.loadFastLED = loadFastLed;
})();
