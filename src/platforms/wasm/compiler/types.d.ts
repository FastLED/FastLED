/**
 * TypeScript Type Definitions for FastLED WASM Compiler
 *
 * This file provides type definitions for custom objects, interfaces,
 * and global window extensions used throughout the FastLED WASM compiler.
 */

// ============================================================================
// Custom Interfaces and Types
// ============================================================================

/**
 * Frame data structure containing LED strip information
 */
interface FrameData {
  strip_id: number;
  type: string;
  pixel_data: Uint8Array | number[];
  screenMap: ScreenMapData;
  [Symbol.iterator](): Iterator<any>;
}

/**
 * Screen mapping data for LED strips
 */
interface ScreenMapData {
  absMax: number[];
  absMin: number[];
  strips: { [stripId: string]: any };
  [key: string]: any;
}

/**
 * Audio data structure for audio processing
 */
interface AudioData {
  frequencyData: Float32Array;
  timeData: Float32Array;
  volume: number;
  [key: string]: any;
}

/**
 * Performance memory information
 */
interface PerformanceMemory {
  usedJSHeapSize: number;
  totalJSHeapSize: number;
  jsHeapSizeLimit: number;
}

// ============================================================================
// Class Definitions
// ============================================================================

/**
 * FastLED Async Controller - the main controller class
 */
declare class FastLEDAsyncController {
  constructor(wasmModule: any, frameRate?: number);
  start(): Promise<void>;
  stop(): void;
  getPerformanceStats(): any;
  isRunning(): boolean;
  setFrameRate(rate: number): void;
  [key: string]: any;
}

/**
 * UI Manager for handling user interface
 */
declare class JsonUiManager {
  constructor(containerId: string);
  initialize(): Promise<void>;
  updateSlider(name: string, value: number): void;
  [key: string]: any;
}

/**
 * Graphics Manager for 2D rendering
 */
declare class GraphicsManager {
  constructor(options: { canvasId: string; threeJsModules?: any; usePixelatedRendering?: boolean });
  initialize(): Promise<void>;
  clearTexture(): void;
  processFrameData(frameData: FrameData): void;
  render(): void;
  [key: string]: any;
}

/**
 * Graphics Manager for 3D rendering with Three.js
 */
declare class GraphicsManagerThreeJS {
  constructor(options: { canvasId: string; threeJsModules: any });
  initialize(): Promise<void>;
  [key: string]: any;
}

/**
 * Video Recorder for recording animations
 */
declare class VideoRecorder {
  constructor();
  startRecording(): Promise<void>;
  stopRecording(): Promise<void>;
  [key: string]: any;
}

/**
 * JSON Inspector for debugging
 */
declare class JsonInspector {
  constructor();
  [key: string]: any;
}

/**
 * FastLED Events system
 */
declare class FastLEDEvents {
  constructor();
  [key: string]: any;
}

/**
 * Performance Monitor
 */
declare class FastLEDPerformanceMonitor {
  constructor();
  [key: string]: any;
}

// ============================================================================
// Audio Worklet Types
// ============================================================================

/**
 * Audio Worklet Processor base class
 */
declare class AudioWorkletProcessor {
  readonly port: MessagePort;
  readonly currentTime: number;
  constructor();
  process(inputs: Float32Array[][], outputs: Float32Array[][], parameters: Record<string, Float32Array>): boolean;
}

/**
 * Register processor function for Audio Worklet
 */
declare function registerProcessor(name: string, processorCtor: typeof AudioWorkletProcessor): void;

/**
 * Audio Worklet Global Scope
 */
declare const AudioWorkletGlobalScope: {
  registerProcessor: typeof registerProcessor;
  [key: string]: any;
};

// ============================================================================
// Three.js Namespace (minimal definitions)
// ============================================================================

declare namespace THREE {
  class Scene {}
  class Camera {}
  class WebGLRenderer {}
  class Mesh {}
  class Geometry {}
  class Material {}
  class Texture {}
  class Vector3 {}
  class Object3D {}
}

// ============================================================================
// Audio Processor Types
// ============================================================================

/**
 * Base Audio Processor interface
 */
interface AudioProcessor {
  initialize(): Promise<void>;
  process(audioData: AudioData): void;
  cleanup(): void;
  [key: string]: any;
}

/**
 * Script Processor Audio Processor
 */
interface ScriptProcessorAudioProcessor extends AudioProcessor {
  initialize(source?: MediaElementAudioSourceNode): Promise<void>;
}

// ============================================================================
// Window Extensions
// ============================================================================

declare global {
  interface Window {
    // FastLED Core
    fastLEDController: FastLEDAsyncController | null;
    fastLEDEvents: FastLEDEvents;
    fastLEDPerformanceMonitor: FastLEDPerformanceMonitor;
    fastLEDDebug: any;

    // UI Management
    uiManager: JsonUiManager;
    uiManagerInstance: JsonUiManager;
    _pendingUiDebugMode: boolean;

    // Graphics
    graphicsManager: GraphicsManager | GraphicsManagerThreeJS;
    updateCanvas: (frameData: FrameData | (any[] & {screenMap?: ScreenMapData})) => void;
    screenMap: ScreenMapData;
    handleStripMapping: any;

    // Audio
    audioData: AudioData;
    webkitAudioContext: typeof AudioContext;
    setupAudioAnalysis: (audioElement?: HTMLAudioElement) => Promise<any>;
    getAudioCapabilities: () => any;
    setAudioProcessor: (processor: string) => void;
    useBestAudioProcessor: () => void;
    forceAudioWorklet: () => void;
    forceScriptProcessor: () => void;
    setAudioDebug: (enabled: boolean) => void;
    getAudioDebugSettings: () => any;
    testAudioWorkletPath: () => void;
    getAudioWorkletEnvironmentInfo: () => any;
    getAudioBufferStats: () => any;

    // Video Recording
    getVideoRecorder: () => VideoRecorder;
    startVideoRecording: () => Promise<void>;
    stopVideoRecording: () => Promise<void>;
    testVideoRecording: () => void;
    getVideoSettings: () => any;

    // FastLED Control Functions
    getFastLEDController: () => FastLEDAsyncController | null;
    getFastLEDPerformanceStats: () => any;
    startFastLED: () => Promise<void>;
    stopFastLED: () => void;
    toggleFastLED: () => Promise<void>;

    // Debug Functions
    fastLEDDebugLog: (...args: any[]) => void;
    setFastLEDDebug: (enabled: boolean) => void;
    FASTLED_DEBUG_LOG: (...args: any[]) => void;
    FASTLED_DEBUG_ERROR: (...args: any[]) => void;
    FASTLED_DEBUG_TRACE: (...args: any[]) => void;
    setFastLEDDebugEnabled: (enabled: boolean) => void;

    // JSON Inspector
    jsonInspector: JsonInspector;

    // UI Debug and Configuration
    setUiDebug: (enabled: boolean) => void;
    setUiSpilloverThresholds: (config: any) => void;
    getUiSpilloverThresholds: () => any;
    setUiSpilloverExample: (config: any) => void;
    _pendingSpilloverConfig: any;

    // UI Recording and Playback
    UIRecorder: any;
    UIPlayback: any;
    uiRecorder: any;
    uiPlayback: any;
    startUIRecording: () => void;
    stopUIRecording: () => void;
    getUIRecordingStatus: () => any;
    exportUIRecording: () => any;
    clearUIRecording: () => void;
    loadUIRecordingForPlayback: (data: any) => void;
    startUIPlayback: () => void;
    pauseUIPlayback: () => void;
    resumeUIPlayback: () => void;
    stopUIPlayback: () => void;
    setUIPlaybackSpeed: (speed: number) => void;
    getUIPlaybackStatus: () => any;

    // UI Test Functions
    runUIRecorderTests: () => void;
    demonstrateUIRecording: () => void;
  }

  interface Performance {
    memory?: PerformanceMemory;
  }

  interface HTMLElement {
    getContext?: (contextId: string) => any;
    text?: string;
    pause?: () => void;
    src?: string;
    load?: () => void;
    files?: FileList;
    width?: number;
    height?: number;
  }

  interface EventTarget {
    files?: FileList;
  }

  interface Event {
    detail?: any;
    clientX?: number;
    clientY?: number;
  }

  interface PromiseConstructor {
    any<T>(values: Iterable<T | PromiseLike<T>>): Promise<T>;
  }
}

// ============================================================================
// Module Exports
// ============================================================================

export {
  FrameData,
  ScreenMapData,
  AudioData,
  AudioProcessor,
  ScriptProcessorAudioProcessor,
  FastLEDAsyncController,
  JsonUiManager,
  GraphicsManager,
  GraphicsManagerThreeJS,
  VideoRecorder,
  JsonInspector,
  FastLEDEvents,
  FastLEDPerformanceMonitor
};