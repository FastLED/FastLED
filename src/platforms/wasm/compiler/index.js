const FRAME_RATE = 60; // 60 FPS
const CANVAS_SIZE = 16; // 16x16 initial canvas size

let receivedCanvas = false;


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

let screenMap = {};

globalThis.FastLED_onStripUpdate = function (jsonData) {
    console.log("Received strip update:", jsonData);

    const event = jsonData.event;
    let width = 0;
    let height = 0;
    if (event === "set_canvas_map") {
        // Work in progress.
        const map = jsonData.map;
        const [min, max] = minMax(map);
        console.log("min", min, "max", max);
        width = max[0] - min[0];
        height = max[1] - min[1];
        screenMap[jsonData.stripId] = {
            map: map,
            min: min,
            max: max,
        };

    } else {
        width = jsonData.width;
        height = jsonData.height;
    }

    if (jsonData.event !== 'set_canvas_size') {
        console.warn("We do not support any event other than \"set_canvas_size\" yet, got event:", jsonData.event);
        return;
    }
    if (receivedCanvas) {
        console.warn("Canvas size has already been set, setting multiple canvas sizes is not supported yet and the previous one will be overwritten.");
    }
    const canvas = document.getElementById('myCanvas');
    canvas.width = width;
    canvas.height = height;

    const maxDisplaySize = 640; // Maximum display size in pixels
    const scaleFactor = Math.min(maxDisplaySize / width, maxDisplaySize / height, 20);

    canvas.style.width = Math.round(width * scaleFactor) + 'px';
    canvas.style.height = Math.round(height * scaleFactor) + 'px';
    console.log(`Canvas size set to ${width}x${height}, displayed at ${canvas.style.width}x${canvas.style.height}`);
};

globalThis.FastLED_onStripAdded = function (stripId, stripLength) {
    const output = document.getElementById('output');
    output.textContent += `Strip added: ID ${stripId}, length ${stripLength}\n`;
};
let uiElements = {};
let previousUiState = {};


globalThis.FastLED_onFrame = function (frameInfo, frameData, uiUpdateCallback) {
    console.log(frameInfo);
    console.log(frameData);
    updateCanvas(frameData.getFirstPixelData_Uint8());

    const changes = {};
    let hasChanges = false;

    for (const id in uiElements) {
        const element = uiElements[id];
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

        if (previousUiState[id] !== currentValue) {
            changes[id] = currentValue;
            hasChanges = true;
            previousUiState[id] = currentValue;
        }
    }

    if (hasChanges) {
        const data = JSON.stringify(changes);
        uiUpdateCallback(data);
    }
};

globalThis.FastLED_onUiElementsAdded = function (jsonData) {
    console.log("UI elements added:", jsonData);

    const uiControlsContainer = document.getElementById('ui-controls') || createUiControlsContainer();

    let foundUi = false;
    jsonData.forEach(element => {
        let control;
        if (element.type === 'slider') {
            control = createSlider(element);
        } else if (element.type === 'checkbox') {
            control = createCheckbox(element);
        } else if (element.type === 'button') {
            control = createButton(element);
        } else if (element.type === 'number') {
            control = createNumberField(element);
        }

        if (control) {
            foundUi = true;
            uiControlsContainer.appendChild(control);
            if (element.type === 'button') {
                uiElements[element.id] = control.querySelector('button');
            } else {
                uiElements[element.id] = control.querySelector('input');
            }
            previousUiState[element.id] = element.value;
        }
    });
    if (foundUi) {
        console.log("UI elements added, showing UI controls container");
        uiControlsContainer.classList.add('active');
    }
};

function createNumberField(element) {
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
    numberInput.step = (element.step !== undefined) ? element.step : 'any'; // Use provided step or allow any decimal

    controlDiv.appendChild(label);
    controlDiv.appendChild(numberInput);

    return controlDiv;
}

function createUiControlsContainer() {
    const container = document.getElementById('ui-controls');
    if (!container) {
        console.error('UI controls container not found in the HTML');
    }
    return container;
}

function createSlider(element) {
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
    slider.min = element.min;
    slider.max = element.max;
    slider.value = element.value;
    slider.step = element.step;

    slider.addEventListener('input', function () {
        valueDisplay.textContent = this.value;
    });

    controlDiv.appendChild(labelValueContainer);
    controlDiv.appendChild(slider);

    return controlDiv;
}

function createCheckbox(element) {
    const controlDiv = document.createElement('div');
    controlDiv.className = 'ui-control';

    // Create the label
    const label = document.createElement('label');
    label.textContent = element.name;
    label.htmlFor = `checkbox-${element.id}`;

    // Create the checkbox input
    const checkbox = document.createElement('input');
    checkbox.type = 'checkbox';
    checkbox.id = `checkbox-${element.id}`;
    checkbox.checked = element.value;

    // Add both label and checkbox to a flex container to align properly
    const flexContainer = document.createElement('div');
    flexContainer.style.display = 'flex';
    flexContainer.style.alignItems = 'center';
    flexContainer.style.justifyContent = 'space-between'; // Ensure checkbox is right-aligned

    flexContainer.appendChild(label);
    flexContainer.appendChild(checkbox);

    controlDiv.appendChild(flexContainer);

    return controlDiv;
}

function createButton(element) {
    const controlDiv = document.createElement('div');
    controlDiv.className = 'ui-control';

    const button = document.createElement('button');
    button.textContent = element.name;
    button.id = `button-${element.id}`;
    button.setAttribute('data-pressed', 'false'); // Initialize data-pressed as false

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


let gl, program, positionBuffer, texCoordBuffer, texture;

function initWebGL() {
    const canvas = document.getElementById('myCanvas');
    gl = canvas.getContext('webgl');
    if (!gl) {
        console.error('WebGL not supported');
        return;
    }

    // Create shaders
    const vertexShader = createShader(gl, gl.VERTEX_SHADER, document.getElementById('vertexShader').text);
    const fragmentShader = createShader(gl, gl.FRAGMENT_SHADER, document.getElementById('fragmentShader').text);

    // Create program
    program = createProgram(gl, vertexShader, fragmentShader);

    // Create buffers
    positionBuffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, positionBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 1, -1, -1, 1, 1, 1]), gl.STREAM_DRAW);

    texCoordBuffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, texCoordBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([0, 0, 1, 0, 0, 1, 1, 1]), gl.STREAM_DRAW);

    // Create texture
    texture = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
}

function createShader(gl, type, source) {
    const shader = gl.createShader(type);
    gl.shaderSource(shader, source);
    gl.compileShader(shader);
    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
        console.error('Shader compile error:', gl.getShaderInfoLog(shader));
        gl.deleteShader(shader);
        return null;
    }
    return shader;
}

function createProgram(gl, vertexShader, fragmentShader) {
    const program = gl.createProgram();
    gl.attachShader(program, vertexShader);
    gl.attachShader(program, fragmentShader);
    gl.linkProgram(program);
    if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
        console.error('Program link error:', gl.getProgramInfoLog(program));
        return null;
    }
    return program;
}
let texWidth = 0, texHeight = 0;
let texData;

function updateCanvas(data) {
    // TODO: map coordinates using the screenMap
    if (data.length === 0) {
        console.warn("Received empty data, skipping update");
        return;
    }
    if (!gl) initWebGL();

    const canvasWidth = gl.canvas.width;
    const canvasHeight = gl.canvas.height;

    // Check if we need to reallocate the texture
    const newTexWidth = Math.pow(2, Math.ceil(Math.log2(canvasWidth)));
    const newTexHeight = Math.pow(2, Math.ceil(Math.log2(canvasHeight)));

    if (texWidth !== newTexWidth || texHeight !== newTexHeight) {
        console.log("Reallocating texture");
        texWidth = newTexWidth;
        texHeight = newTexHeight;
        console.log(`Texture size: ${texWidth}x${texHeight}`);
        console.log(`Canvas size: ${canvasWidth}x${canvasHeight}`);

        // Reallocate texture
        gl.bindTexture(gl.TEXTURE_2D, texture);
        gl.texImage2D(
            gl.TEXTURE_2D,
            0,
            gl.RGB,
            texWidth,
            texHeight,
            0,
            gl.RGB,
            gl.UNSIGNED_BYTE,
            null
        );

        // Reallocate texData buffer
        texData = new Uint8Array(texWidth * texHeight * 3);
    }

    // Update texData with new frame data
    const srcRowSize = canvasWidth * 3;
    const destRowSize = texWidth * 3;

    
    for (let y = 0; y < canvasHeight; y++) {
        for (let x = 0; x < canvasWidth; x++) {
            const srcIndex = (y * srcRowSize) + (x * 3);
            const destIndex = (y * destRowSize) + (x * 3);
            texData[destIndex] = data[srcIndex];
            texData[destIndex + 1] = data[srcIndex + 1];
            texData[destIndex + 2] = data[srcIndex + 2];
        }
    }

    // Update texture with new data
    gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.texSubImage2D(
        gl.TEXTURE_2D,
        0,
        0,
        0,
        texWidth,
        texHeight,
        gl.RGB,
        gl.UNSIGNED_BYTE,
        texData
    );

    // Set the viewport to the original canvas size
    gl.viewport(0, 0, canvasWidth, canvasHeight);
    gl.clearColor(0, 0, 0, 1);
    gl.clear(gl.COLOR_BUFFER_BIT);

    gl.useProgram(program);

    const positionLocation = gl.getAttribLocation(program, 'a_position');
    gl.enableVertexAttribArray(positionLocation);
    gl.bindBuffer(gl.ARRAY_BUFFER, positionBuffer);
    gl.vertexAttribPointer(positionLocation, 2, gl.FLOAT, false, 0, 0);

    const texCoordLocation = gl.getAttribLocation(program, 'a_texCoord');
    gl.enableVertexAttribArray(texCoordLocation);
    gl.bindBuffer(gl.ARRAY_BUFFER, texCoordBuffer);
    gl.vertexAttribPointer(texCoordLocation, 2, gl.FLOAT, false, 0, 0);

    // Update texture coordinates based on actual canvas size
    const texCoords = new Float32Array([
        0, 0,
        canvasWidth / texWidth, 0,
        0, canvasHeight / texHeight,
        canvasWidth / texWidth, canvasHeight / texHeight,
    ]);
    gl.bindBuffer(gl.ARRAY_BUFFER, texCoordBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, texCoords, gl.STREAM_DRAW);

    gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
}


// Ensure we wait for the module to load
const onModuleLoaded = async () => {
    // Unpack the module functions and send them to the runFastLED function
    function __runFastLED(moduleInstance, frame_rate) {
        const exports_exist = moduleInstance && moduleInstance._extern_setup && moduleInstance._extern_loop;
        if (!exports_exist) {
            console.error("FastLED setup or loop functions are not available.");
            return;
        }
        return runFastLED(moduleInstance._extern_setup, moduleInstance._extern_loop, frame_rate, moduleInstance);
    }
    try {
        if (typeof fastled === 'function') {
            // Load the module
            fastled().then(instance => {
                console.log("Module loaded, running FastLED...");
                __runFastLED(instance, FRAME_RATE);
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

// Wait for fonts to load before showing content
if (document.fonts && document.fonts.ready) {
    document.fonts.ready.then(function () {
        document.body.style.opacity = 1;
    });
} else {
    // Fallback for browsers that do not support document.fonts
    window.onload = function () {
        document.body.style.opacity = 1;
    };
}

// Bind the window.onload event to the onModuleLoaded function.
window.addEventListener('load', onModuleLoaded);