/**
 * Consolidated type definitions for the FastLED WASM frontend.
 *
 * This module is the single source of truth for all shared interfaces.
 * Import from here instead of using inline @typedef JSDoc.
 */

// ============================================================================
// Core Data Types
// ============================================================================

export interface StripData {
  id: number;
  leds: number[];
  map: {
    x: number[];
    y: number[];
  };
  diameter?: number;
  type?: 'el_wire' | 'el_panel';
  vertices_x?: number[];
  vertices_y?: number[];
  thickness?: number;
}

export interface ScreenMapData {
  absMax?: number[];
  absMin?: number[];
  strips: { [stripId: string]: StripData };
  [key: string]: unknown;
}

export interface FrameData {
  strip_id: number;
  type: string;
  pixel_data: Uint8Array | number[];
  screenMap: ScreenMapData;
  [Symbol.iterator](): Iterator<unknown>;
}

export interface AudioData {
  audioContexts: { [id: string]: AudioContext };
  audioProcessors: { [id: string]: unknown };
  audioSources: { [id: string]: MediaElementAudioSourceNode | MediaStreamAudioSourceNode };
  mediaStreams: { [id: string]: MediaStream };
  [key: string]: unknown;
}

// ============================================================================
// Worker Types
// ============================================================================

export interface WorkerCapabilities {
  offscreenCanvas: boolean;
  sharedArrayBuffer: boolean;
  transferableObjects: boolean;
  webgl2: boolean;
}

export interface WorkerConfiguration {
  canvas: HTMLCanvasElement;
  fastledModule: unknown;
  frameRate: number;
  enableFallback: boolean;
  maxRetries: number;
}

export interface WorkerState {
  initialized: boolean;
  canvas: OffscreenCanvas | null;
  fastledModule: unknown;
  graphicsManager: unknown;
  running: boolean;
  frameRate: number;
  capabilities: WorkerCapabilities;
  renderingContext: unknown;
  animationFrameId: number | null;
  frameCount: number;
  startTime: number;
  lastFrameTime: number;
  averageFrameTime: number;
  externFunctions: unknown;
  wasmFunctions: unknown;
  processUiInput: ((input: unknown) => void) | null;
  urlParams: Record<string, string>;
  isCapturingFrames: boolean;
  frameCaptureInterval: number;
  lastFrameCaptureTime: number;
  screenMaps: Record<string, unknown>;
}

// ============================================================================
// UI Types
// ============================================================================

export interface GroupInfo {
  container: HTMLDivElement;
  content: HTMLDivElement;
  name: string;
  isWide: boolean;
  isFullWidth: boolean;
  parentContainer: HTMLElement;
}

export type LayoutMode = 'mobile' | 'tablet' | 'desktop' | 'ultrawide';

export interface LayoutState {
  mode: LayoutMode;
  viewportWidth: number;
  availableWidth: number;
  canvasSize: number;
  uiColumns: number;
  uiColumnWidth: number;
  uiTotalWidth: number;
  canExpand: boolean;
  container2Visible: boolean;
  totalGroups: number;
  totalElements: number;
  containers: Record<string, unknown>;
}

export interface StateChangeEvent {
  state: LayoutState;
  previousState: LayoutState;
  changedFields: string[];
}

export interface LayoutBreakpoints {
  mobile: { max: number };
  tablet: { min: number; max: number };
  desktop: { min: number; max: number };
  ultrawide: { min: number };
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
  minContentRatio: number;
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

export interface ValidationResult {
  isValid: boolean;
  efficiency: number;
  warnings: string[];
  errors: string[];
  metrics: LayoutMetrics;
  recommendations: string[];
}

export interface LayoutMetrics {
  spaceUtilization: number;
  columnBalance: number;
  contentDensity: number;
  aspectRatio: number;
  whitespaceRatio: number;
}

export interface OptimizationSuggestion {
  type: string;
  description: string;
  params: Record<string, unknown>;
  impact: number;
}

export interface ResizeEvent {
  width: number;
  height: number;
  timestamp: number;
  source: string;
}

export interface ResizeMetrics {
  totalResizes: number;
  throttledResizes: number;
  lastResizeTime: number;
  averageResizeInterval: number;
}

// ============================================================================
// Recording Types
// ============================================================================

export type UIEventType = 'add' | 'update' | 'remove';

export interface UIEvent {
  timestamp: number;
  type: UIEventType;
  elementId: string;
  data: {
    elementType?: string;
    value?: unknown;
    previousValue?: unknown;
    elementConfig?: Record<string, unknown>;
    elementSnapshot?: Record<string, unknown>;
  };
}

export interface UIRecording {
  recording: {
    version: string;
    startTime: string;
    endTime?: string;
    metadata: {
      recordingId: string;
      layoutMode: string;
      totalDuration?: number;
    };
  };
  events: UIEvent[];
}

export interface PlaybackOptions {
  speed?: number;
  validateElements?: boolean;
  debugMode?: boolean;
  maxEvents?: number;
}

export interface PlaybackStatus {
  isPlaying: boolean;
  isPaused: boolean;
  currentTimestamp: number;
  totalDuration: number;
  currentEventIndex: number;
  totalEvents: number;
  progress: number;
  speed: number;
}

export interface FrameInfo {
  imageData: ImageData;
  timestamp: number;
  frameNumber: number;
  width: number;
  height: number;
  fromWebGL?: boolean;
}

// ============================================================================
// Video Settings
// ============================================================================

export interface VideoSettings {
  videoCodec: string;
  videoBitrate: number;
  audioCodec: string;
  audioBitrate: number;
  fps: number;
}

// ============================================================================
// Three.js Module Types
// ============================================================================

export interface ThreeJsModules {
  THREE: unknown;
  Stats: unknown;
  GUI: unknown;
  OrbitControls: unknown;
  GLTFLoader: unknown;
  EffectComposer: unknown;
  RenderPass: unknown;
  UnrealBloomPass: unknown;
  OutputPass: unknown;
  BufferGeometryUtils: unknown;
}

export interface ThreeJsOptions {
  containerId: string;
  modules: ThreeJsModules;
}

// ============================================================================
// FastLED Options
// ============================================================================

export interface FastLEDOptions {
  canvasId: string;
  uiControlsId: string;
  printId: string;
  frameRate: number;
  fastled: unknown;
  threeJs: ThreeJsOptions;
}
