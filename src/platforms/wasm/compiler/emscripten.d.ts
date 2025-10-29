/**
 * Emscripten Module global type definitions
 *
 * This file provides minimal type definitions for the Emscripten Module global
 * that is used throughout the WASM compiler codebase.
 */

declare const Module: {
  ccall: (name: string, returnType: string | null, argTypes: string[], args: any[]) => any;
  _malloc: (size: number) => number;
  _free: (ptr: number) => void;
  HEAP16: Int16Array;
  HEAP8: Int8Array;
  HEAP32: Int32Array;
  HEAPU8: Uint8Array;
  HEAPU16: Uint16Array;
  HEAPU32: Uint32Array;
  HEAPF32: Float32Array;
  HEAPF64: Float64Array;
};
