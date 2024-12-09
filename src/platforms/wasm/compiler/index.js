
import { UiManager } from "./modules/ui_manager.js";
import { GraphicsManager } from "./modules/graphics_manager.js";
import { GraphicsManagerThreeJS } from "./modules/graphics_manager_threejs.js";


const urlParams = new URLSearchParams(window.location.search);
const FORCE_FAST_RENDERER = urlParams.get('gfx') === '0';
const FORCE_THREEJS_RENDERER = urlParams.get('gfx') === '1';
const MAX_STDOUT_LINES = 50;

console.log("index.js loaded");
console.log("FastLED loader function:", typeof _loadFastLED);



async function _loadFastLED(options) {
    // Stub to let the user/dev know that something went wrong.
    log("FastLED loader function was not set.");
    return null;
}


export async function loadFastLED(options) {
    return await _loadFastLED(options); // This will be overridden by through the initialization.
};



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

// DO NOT OVERRIDE ERROR! When something goes really wrong we want it
// to always go to the console. If we hijack it then startup errors become
// extremely difficult to debug.

console = {};
console.log = log;
console.warn = warn;
console.error = _prev_error;


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


(function () {
    const DEFAULT_FRAME_RATE_60FPS = 60; // 60 FPS
    let frameRate = DEFAULT_FRAME_RATE_60FPS;
    let receivedCanvas = false;
    // screenMap contains data mapping a strip id to a screen map,
    // transforming led strip data pixel with an index
    // to a screen pixel with xy.
    let screenMap = {
        strips: {},
        absMin: [0, 0],
        absMax: [0, 0]
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
        while (lines.length > MAX_STDOUT_LINES) {
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
    async function runFastLED(extern_setup, extern_loop, frame_rate, moduleInstance, filesJson) {
        console.log("Calling setup function...");

        function getFileManifestJson() {
            const trimmedFilesJson = filesJson.map(file => {
                return {
                    path: file.path,
                    size: file.size,
                };
            });
            const options = {
                files: trimmedFilesJson,
                frameRate: frame_rate,
            };
            return options;

        }

        function partition(filesJson, immediateExtensions) {
            const immediateFiles = [];
            const streamingFiles = [];
            filesJson.map(file => {
                for (const ext of immediateExtensions) {
                    const pathLower = file.path.toLowerCase();
                    if (pathLower.endsWith(ext.toLowerCase())) {
                        immediateFiles.push(file);
                        return;
                    }
                }
                streamingFiles.push(file);
            });
            return [immediateFiles, streamingFiles];
        }



        const fileManifest = getFileManifestJson();
        moduleInstance._fastled_declare_files(JSON.stringify(fileManifest));


        console.log("Files JSON:", filesJson);


        function jsAppendFile(path_cstr, data_cbytes, len_int) {
            // Stream this chunk
            moduleInstance.ccall('jsAppendFile',
                'number',  // return value
                ['number', 'number', 'number'], // argument types, not sure why numbers works.
                [path_cstr, data_cbytes, len_int]
            );
        }

        function jsAppendFileUint8(path, blob) {
            const n = moduleInstance.lengthBytesUTF8(path) + 1;
            const path_cstr = moduleInstance._malloc(n);
            moduleInstance.stringToUTF8(path, path_cstr, n);
            const ptr = moduleInstance._malloc(blob.length);
            moduleInstance.HEAPU8.set(blob, ptr);
            jsAppendFile(path_cstr, ptr, blob.length);
            moduleInstance._free(ptr);
            moduleInstance._free(path_cstr);
        }


        const processFile = async (file) => {
            try {
                const response = await fetch(file.path);
                const reader = response.body.getReader();
        
                console.log(`File fetched: ${file.path}, size: ${file.size}`);
        
                while (true) {
                    const { value, done } = await reader.read();
                    if (done) break;
        
                    // Allocate and copy chunk data
                    jsAppendFileUint8(file.path, value);
                }
            } catch (error) {
                console.error(`Error processing file ${file.path}:`, error);
            }
        };
        


        const fetchAllFiles = async (filesJson, onComplete) => {
            const promises = filesJson.map(async (file) => {
                await processFile(file);
            });
            await Promise.all(promises);
            if (onComplete) {
                onComplete();
            }
        };


        function onComplete_SetupFastLEDAndLoop() {
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


        // Come back to this later - we want to partition the files into immediate and streaming files
        // so that large projects don't try to download ALL the large files BEFORE setup/loop is called.
        const [immediateFiles, streamingFiles] = partition(filesJson, [".json", ".csv", ".txt", ".cfg"]);
        console.log("The following files will be immediatly available and can be read during setup():", immediateFiles);
        console.log("The following files will be streamed in during loop():", streamingFiles);

        fetchAllFiles(immediateFiles, () => {
            onComplete_SetupFastLEDAndLoop();
        });
        fetchAllFiles(streamingFiles, () => {
            if (streamingFiles.length !== 0) {
                console.log("All streaming files processed");
            }
        });
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

        function __runFastLED(moduleInstance, frameRate, filesJson) {
            const exports_exist = moduleInstance && moduleInstance._extern_setup && moduleInstance._extern_loop;
            if (!exports_exist) {
                console.error("FastLED setup or loop functions are not available.");
                return;
            }

            runFastLED(moduleInstance._extern_setup, moduleInstance._extern_loop, frameRate, moduleInstance, filesJson);
        }
        // Start fetch now in parallel
        const fetchFilePromise = async (fetchFilePath) => {
            const response = await fetch(fetchFilePath);
            const data = await response.json();
            return data;
        };
        const filesJsonPromise = fetchFilePromise("files.json");
        try {
            if (typeof fastLedLoader === 'function') {
                // Load the module
                fastLedLoader().then(async (instance) => {
                    console.log("Module loaded, running FastLED...");
                    // Wait for the files.json to load.
                    let filesJson = null;
                    try {
                        filesJson = await filesJsonPromise;
                        console.log("Files JSON:", filesJson);
                    } catch (error) {
                        console.error("Error fetching files.json:", error);
                        filesJson = {};
                    }
                    __runFastLED(instance, frameRate, filesJson);
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



    async function localLoadFastLed(options) {
        try {
            canvasId = options.canvasId;
            uiControlsId = options.uiControlsId;
            outputId = options.printId;
            print = customPrintFunction;
            console.log("Loading FastLED with options:", options);
            frameRate = options.frameRate || DEFAULT_FRAME_RATE_60FPS;
            uiManager = new UiManager(uiControlsId);
            let threeJs = options.threeJs;
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
            let out = await onModuleLoaded(fastLedLoader);
            console.log("Module loaded:", out);
        } catch (error) {
            console.error("Error loading FastLED:", error);
            debugger;
        }
    }
    _loadFastLED = localLoadFastLed;
})();
