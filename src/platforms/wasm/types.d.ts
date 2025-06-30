/**
 * FastLED WASM Global Type Definitions
 * Defines custom types and window extensions for FastLED's JavaScript environment
 * Provides TypeScript support for the FastLED WebAssembly platform
 */

// Type definitions for FastLED WebAssembly JavaScript modules

// Core LED and graphics types
export interface StripData {
  id: number;
  leds: number[];
  map?: {
    x: number[];
    y: number[];
  };
}

export interface ScreenMapData {
  strips: { [key: string]: StripData };
  min?: number[];
  max?: number[];
  diameter?: number;
}

export interface LayoutData {
  screenMap: ScreenMapData;
}

export interface LayoutConfig {
  containerPadding: number;
  minCanvasSize: number;
  maxCanvasSize: number;
  preferredUIColumnWidth: number;
  maxUIColumnWidth: number;
  minUIColumnWidth: number;
  maxUIColumns: number;
  canvasExpansionRatio: number;
  horizontalGap: number;
  verticalGap: number;
}

export interface LayoutResult {
  viewportWidth: number;
  availableWidth: number;
  canvasSize: number;
  uiColumns: number;
  uiColumnWidth: number;
  uiTotalWidth: number;
  contentWidth: number;
  layoutMode: string;
  canExpand: boolean;
}

// UI element types
export interface UIElementData {
  id: string;
  type: string;
  name: string;
  value?: string | number | boolean;
  min?: number;
  max?: number;
  step?: number;
  checked?: boolean;
  options?: string[];
}

export interface UIChanges {
  [key: string]: string | number | boolean | Int16Array;
}

export interface GraphicsManager {
  initialize(): void;
  updateFrame(frameData: LayoutData): void;
  resize(width: number, height: number): void;
  destroy(): void;
}

export interface EmscriptenModule {
  ccall: (name: string, returnType: string, argTypes: string[], args: unknown[]) => unknown;
  cwrap: (name: string, returnType: string, argTypes: string[]) => Function;
  _malloc: (size: number) => number;
  _free: (ptr: number) => void;
  HEAP8: Int8Array;
  HEAP16: Int16Array;  
  HEAP32: Int32Array;
  HEAPU8: Uint8Array;
  HEAPU16: Uint16Array;
  HEAPU32: Uint32Array;
  HEAPF32: Float32Array;
  HEAPF64: Float64Array;
}

// Audio Worklet types
export interface AudioWorkletProcessor {
  readonly port: MessagePort;
  readonly currentTime: number;
  process(
    inputs: Float32Array[][],
    outputs: Float32Array[][],
    parameters: Record<string, Float32Array>
  ): boolean;
}

export interface AudioWorkletProcessorConstructor {
  new (): AudioWorkletProcessor;
}

export interface AudioWorkletGlobalScope {
  registerProcessor(name: string, constructor: AudioWorkletProcessorConstructor): void;
  AudioWorkletProcessor: AudioWorkletProcessorConstructor;
  currentTime: number;
}

// Extend global scope for WebAssembly modules
declare global {
  interface Window {
    // Audio system globals
    audioData?: {
      audioBuffers: { [key: string]: unknown };
      hasActiveSamples: boolean;
    };
    
    // FastLED globals
    FastLED_onFrame?: (frameData: LayoutData) => void;
    FastLED_onStripAdded?: (stripData: StripData) => void;
    FastLED_onStripRemoved?: (stripId: number) => void;
    
    // UI globals
    onFastLEDUiUpdate?: (changes: UIChanges) => void;
    onFastLEDUiUpdateJson?: (jsonData: UIElementData[]) => void;
    onFastLEDUiElementRegistered?: (elementData: UIElementData) => void;
    onFastLEDUiSetDebugMode?: (enabled: boolean) => void;
    onFastLEDUiLayoutChanged?: (layoutMode: string) => void;
    onFastLEDUiAdvancedLayoutChanged?: (layout: string, data: LayoutResult) => void;
    
    // Graphics globals
    createGraphicsManager?: () => GraphicsManager;
    createGraphicsManagerThreeJS?: () => GraphicsManager;
    
    // Module system
    Module?: EmscriptenModule;

    // UI manager instances
    uiManager?: unknown;
    uiManagerInstance?: unknown;
    _pendingUiDebugMode?: boolean;
    
    // UI helper functions
    setUiDebug?: (enabled?: boolean) => void;
  }

  // Browser globals that Deno doesn't know about
  declare const document: Document;
  declare const requestAnimationFrame: (callback: FrameRequestCallback) => number;
  declare const AudioContext: {
    new (): AudioContext;
    prototype: AudioContext;
  };
  declare const AudioWorkletNode: {
    new (context: AudioContext, name: string, options?: AudioWorkletNodeOptions): AudioWorkletNode;
  };

  // Audio Worklet global scope
  interface AudioWorkletGlobalScope {
    registerProcessor(name: string, constructor: AudioWorkletProcessorConstructor): void;
    AudioWorkletProcessor: AudioWorkletProcessorConstructor;
    currentTime: number;
  }

  // Extend AudioWorkletProcessor to include port and currentTime
  interface AudioWorkletProcessor {
    readonly port: MessagePort;
    readonly currentTime: number;
  }

  // HTML Element extensions
  interface HTMLElement {
    text?: string; // For script elements
  }
}

// Export for module usage
export {};
