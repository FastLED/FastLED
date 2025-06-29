/**
 * FastLED WASM Global Type Definitions
 * Defines custom types and window extensions for FastLED's JavaScript environment
 */

declare global {
  interface Window {
    // Audio-related global functions
    setupAudioAnalysis: (audioElement: HTMLAudioElement) => unknown;
    getAudioCapabilities: () => unknown;
    setAudioProcessor: (type: string) => boolean;
    useBestAudioProcessor: () => string;
    forceAudioWorklet: () => string;
    forceScriptProcessor: () => string;
    setAudioDebug: (enabled?: boolean) => void;
    getAudioDebugSettings: () => unknown;
    testAudioWorkletPath: (customPath?: string | null) => Promise<boolean>;
    getAudioWorkletEnvironmentInfo: () => unknown;
    getAudioBufferStats: () => unknown;

    // UI-related global functions
    setUiDebug: (enabled?: boolean) => void;
    uiManager?: unknown;
    uiManagerInstance?: unknown;
    _pendingUiDebugMode?: boolean;

    // Audio data storage
    audioData?: {
      audioContexts: Record<string, AudioContext>;
      audioSamples: Record<string, Int16Array>;
      audioBuffers: Record<string, unknown>;
      audioProcessors: Record<string, unknown>;
      audioSources: Record<string, MediaElementAudioSourceNode>;
      hasActiveSamples: boolean;
    };

    // Legacy WebKit audio support
    webkitAudioContext?: typeof AudioContext;
  }

  // Audio Worklet global types (for worklet context)
  declare class AudioWorkletProcessor {
    port: MessagePort;
    currentTime: number;
    constructor(options?: unknown);
    process(
      inputs: Float32Array[][],
      outputs: Float32Array[][],
      parameters: Record<string, Float32Array>,
    ): boolean;
  }

  declare function registerProcessor(
    name: string,
    processorClass: typeof AudioWorkletProcessor,
  ): void;

  // File input event target extensions
  interface EventTarget {
    files?: FileList;
  }

  // HTML element extensions for better typing
  interface HTMLElement {
    text?: string; // For script elements
  }

  interface HTMLInputElement {
    // Already properly typed in DOM lib, just ensuring min/max can be numbers
    min: string;
    max: string;
    value: string;
    step: string;
  }
}

export {}; // Make this a module
