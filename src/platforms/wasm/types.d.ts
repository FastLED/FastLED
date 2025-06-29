/**
 * FastLED WASM Global Type Definitions
 * Defines custom types and window extensions for FastLED's JavaScript environment
 * Provides TypeScript support for the FastLED WebAssembly platform
 */

declare global {
  interface Window {
    // Audio-related global functions
    /**
     * Sets up audio analysis for an HTML audio element
     * @param audioElement - The HTML audio element to analyze
     * @returns Audio analysis configuration object
     */
    setupAudioAnalysis: (audioElement: HTMLAudioElement) => unknown;

    /**
     * Gets the current audio capabilities of the browser/system
     * @returns Object containing audio capability information
     */
    getAudioCapabilities: () => unknown;

    /**
     * Sets the audio processor type (worklet vs script processor)
     * @param type - The processor type identifier
     * @returns True if the processor was successfully set
     */
    setAudioProcessor: (type: string) => boolean;

    /**
     * Automatically selects and uses the best available audio processor
     * @returns The name of the selected processor type
     */
    useBestAudioProcessor: () => string;

    /**
     * Forces the use of AudioWorklet processor
     * @returns The processor type that was set
     */
    forceAudioWorklet: () => string;

    /**
     * Forces the use of ScriptProcessor (legacy)
     * @returns The processor type that was set
     */
    forceScriptProcessor: () => string;

    /**
     * Enables or disables audio debug logging
     * @param enabled - Whether to enable debug mode (default: true)
     */
    setAudioDebug: (enabled?: boolean) => void;

    /**
     * Gets current audio debug settings and statistics
     * @returns Object containing debug configuration and stats
     */
    getAudioDebugSettings: () => unknown;

    /**
     * Tests if AudioWorklet processor can be loaded from the given path
     * @param customPath - Optional custom path to test (uses default paths if null)
     * @returns Promise that resolves to true if the worklet loads successfully
     */
    testAudioWorkletPath: (customPath?: string | null) => Promise<boolean>;

    /**
     * Gets information about the AudioWorklet environment
     * @returns Object containing environment and capability information
     */
    getAudioWorkletEnvironmentInfo: () => unknown;

    /**
     * Gets statistics about audio buffer usage and performance
     * @returns Object containing buffer statistics
     */
    getAudioBufferStats: () => unknown;

    // UI-related global functions
    /**
     * Enables or disables UI debug logging
     * @param enabled - Whether to enable debug mode (default: true)
     */
    setUiDebug: (enabled?: boolean) => void;

    /** UI manager instance (may be undefined during initialization) */
    uiManager?: unknown;

    /** UI manager singleton instance */
    uiManagerInstance?: unknown;

    /** Internal flag for pending UI debug mode changes */
    _pendingUiDebugMode?: boolean;

    // Audio data storage
    /**
     * Global audio data storage containing all audio-related objects
     * Maintains audio contexts, samples, buffers, and processing state
     */
    audioData?: {
      /** Map of audio context instances by ID */
      audioContexts: Record<string, AudioContext>;
      /** Map of audio sample buffers by ID */
      audioSamples: Record<string, Int16Array>;
      /** Map of audio buffer objects by ID */
      audioBuffers: Record<string, unknown>;
      /** Map of audio processor instances by ID */
      audioProcessors: Record<string, unknown>;
      /** Map of audio source nodes by ID */
      audioSources: Record<string, MediaElementAudioSourceNode>;
      /** Flag indicating if there are active audio samples being processed */
      hasActiveSamples: boolean;
    };

    // Legacy WebKit audio support
    /**
     * Legacy WebKit AudioContext constructor for older Safari versions
     * @deprecated Use AudioContext instead
     */
    webkitAudioContext?: typeof AudioContext;
  }

  // Audio Worklet global types (for worklet context)
  /**
   * Base class for audio worklet processors
   * Runs on the audio thread for high-performance audio processing
   */
  declare class AudioWorkletProcessor {
    /** Message port for communication with main thread */
    port: MessagePort;
    /** Current time in the audio context (in seconds) */
    currentTime: number;

    /**
     * Creates a new AudioWorkletProcessor instance
     * @param options - Optional configuration object
     */
    constructor(options?: unknown);

    /**
     * Main audio processing method called by the audio system
     * @param inputs - Array of input audio channel arrays
     * @param outputs - Array of output audio channel arrays
     * @param parameters - Audio parameters as Float32Array values
     * @returns True to keep the processor alive, false to terminate
     */
    process(
      inputs: Float32Array[][],
      outputs: Float32Array[][],
      parameters: Record<string, Float32Array>,
    ): boolean;
  }

  /**
   * Registers an audio worklet processor class
   * @param name - The name to register the processor under
   * @param processorClass - The processor class to register
   */
  declare function registerProcessor(
    name: string,
    processorClass: typeof AudioWorkletProcessor,
  ): void;

  // File input event target extensions
  interface EventTarget {
    /** File list for file input elements */
    files?: FileList;
  }

  // HTML element extensions for better typing
  interface HTMLElement {
    /** Text content for script elements */
    text?: string;
  }

  interface HTMLInputElement {
    // Enhanced typing for numeric input attributes
    /** Minimum value for numeric inputs */
    min: string;
    /** Maximum value for numeric inputs */
    max: string;
    /** Current value of the input */
    value: string;
    /** Step increment for numeric inputs */
    step: string;
  }
}

export {}; // Make this a module
