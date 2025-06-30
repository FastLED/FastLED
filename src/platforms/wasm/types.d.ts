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
  min?: number[];
  max?: number[];
  diameter?: number;
}

export interface ScreenMapData {
  strips: { [key: string]: StripData };
  min?: number[];
  max?: number[];
  diameter?: number;
  // Additional properties found in graphics_utils.js
  absMin?: number[];
  absMax?: number[];
}

export interface FrameData {
  screenMap: ScreenMapData;
  strip_id?: number;
  pixel_data?: Uint8Array;
  // Make FrameData iterable for for-of loops
  [Symbol.iterator](): Iterator<any>;
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
}

// Audio system types
export interface AudioData {
  audioContexts: { [key: string]: AudioContext };
  audioProcessors: { [key: string]: any };
  audioSources: { [key: string]: any };
  audioBuffers: { [key: string]: any };
  audioSamples: { [key: string]: Int16Array };
  hasActiveSamples?: boolean;
}

export interface AudioBufferStorage {
  audioId: string;
  // Add other properties as needed
}

// UI Manager types
export interface UIGroupInfo {
  container: HTMLDivElement;
  content: HTMLDivElement;
  name: string;
  isWide: boolean;
  isFullWidth: boolean;
}

export interface UIElement {
  id: string;
  type: string;
  value: number | string | boolean;
  min?: number | string;
  max?: number | string;
  step?: number | string;
  accept?: string;
}

export interface UILayoutPlacementManager {
  mediaQuery?: MediaQueryList;
  // Add other properties as needed
}

// AudioWorklet types for worklet environment
declare global {
  // AudioWorklet context globals
  var AudioWorkletProcessor: {
    new(): AudioWorkletProcessor;
  };
  
  interface AudioWorkletProcessor {
    port: MessagePort;
    currentTime: number;
  }
  
  var registerProcessor: (name: string, processorClass: any) => void;

  // Browser compatibility extensions
  interface Window {
    webkitAudioContext?: typeof AudioContext;
    
    // FastLED global functions
    audioData?: AudioData;
    setupAudioAnalysis?: (audioElement: HTMLAudioElement) => void;
    getAudioCapabilities?: () => any;
    setAudioProcessor?: (type: string) => void;
    useBestAudioProcessor?: () => void;
    forceAudioWorklet?: () => void;
    forceScriptProcessor?: () => void;
    setAudioDebug?: (enabled?: boolean) => void;
    getAudioDebugSettings?: () => any;
    testAudioWorkletPath?: (customPath?: string | null) => Promise<any>;
    getAudioWorkletEnvironmentInfo?: () => any;
    getAudioBufferStats?: () => any;
    
    // UI Manager globals
    uiManager?: any;
    setUiDebug?: (enabled?: boolean) => void;
    _pendingUiDebugMode?: boolean;
  }

  // DOM element type extensions
  interface HTMLInputElement {
    // Ensure proper types for number inputs
    valueAsNumber: number;
  }

  // File input event target
  interface HTMLInputFileEvent extends Event {
    target: HTMLInputElement & { files: FileList };
  }
}

// Module exports for ES modules
export {};
