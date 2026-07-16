// @ts-nocheck

/** FastLED boundary adapter for the shared @fastled/gfx render core. */
import { createGfxCore } from '@fastled/gfx/core';

function asBytes(value) {
  if (value instanceof Uint8Array) return value;
  if (ArrayBuffer.isView(value)) return new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
  if (Array.isArray(value)) return Uint8Array.from(value);
  return null;
}

/** Extract `{x, y, diameter?}` from a strip entry. The engine's wire shape
 * nests coordinates as `{map: {x, y}, diameter}` (see js_bindings.cpp.hpp and
 * graphics_manager.ts); bare `{x, y}` is accepted for pre-normalized data. */
function stripCoords(strip) {
  if (Array.isArray(strip?.x) && Array.isArray(strip?.y)) return strip;
  if (Array.isArray(strip?.map?.x) && Array.isArray(strip?.map?.y)) {
    const entry = { x: strip.map.x, y: strip.map.y };
    if (typeof strip.diameter === 'number') entry.diameter = strip.diameter;
    return entry;
  }
  return null;
}

/** Convert FastLED's `{group: {strips: {id: {map: {x, y}, diameter}}}}` shape to package JSON. */
export function toGfxScreenmap(screenMaps) {
  if (screenMaps?.map) return screenMaps;
  const map = {};
  const segments = [];
  const groups = {};
  let hasShapes = false;
  for (const [groupId, group] of Object.entries(screenMaps ?? {})) {
    if (group?.strips && typeof group.strips === 'object') {
      for (const [stripId, strip] of Object.entries(group.strips)) {
        if (Array.isArray(strip?.shapes)) {
          hasShapes = true;
          groups[String(groupId)] = { color: '#ffffff' };
          for (const [shapeIndex, shape] of strip.shapes.entries()) {
            segments.push({ id: `${groupId}:${stripId}:shape${shapeIndex}`, type: shape.type,
              pin: String(groupId), group: String(groupId), x: shape.x ?? [], y: shape.y ?? [],
              ...(shape.thickness !== undefined ? { thickness: shape.thickness } : {}) });
          }
          continue;
        }
        const coords = stripCoords(strip);
        if (coords) map[`${groupId}:${stripId}`] = coords;
      }
    } else {
      if (Array.isArray(group?.shapes)) {
        hasShapes = true;
        groups[String(groupId)] = { color: '#ffffff' };
        for (const [shapeIndex, shape] of group.shapes.entries()) {
          segments.push({ id: `${String(groupId)}:shape${shapeIndex}`, type: shape.type, pin: String(groupId), group: String(groupId),
            x: shape.x ?? [], y: shape.y ?? [], ...(shape.thickness !== undefined ? { thickness: shape.thickness } : {}) });
        }
        continue;
      }
      const coords = stripCoords(group);
      if (coords) map[groupId] = coords;
    }
  }
  if (hasShapes) {
    for (const id of Object.keys(map)) groups[id] = { color: '#ffffff' };
    return { version: 2, groups, segments: [...segments, ...Object.entries(map).map(([id, value]) => ({ id, type: 'led_strip', pin: id, group: id, x: value.x ?? [], y: value.y ?? [], ...(value.diameter !== undefined ? { diameter: value.diameter } : {}) }))] };
  }
  if (Object.keys(map).length === 0) throw new Error('screenmap: no coordinate strips found');
  return { map };
}

function combineFrame(frameData) {
  const strips = Array.isArray(frameData) ? frameData : [frameData];
  const chunks = [];
  let length = 0;
  for (const strip of strips) {
    const bytes = asBytes(strip?.pixel_data ?? strip?.pixels);
    if (!bytes) throw new Error(`frame-data: strip ${strip?.strip_id ?? 'unknown'} has no RGB8 bytes`);
    chunks.push(bytes);
    length += bytes.byteLength;
  }
  const output = new Uint8Array(length);
  let offset = 0;
  for (const bytes of chunks) { output.set(bytes, offset); offset += bytes.byteLength; }
  return output;
}

export class GraphicsManagerGfx {
  constructor({ canvasId, canvas, paneSize = 800, diameter = 16 } = {}) {
    this.canvas = canvas ?? (typeof document !== 'undefined' ? document.getElementById(canvasId) : null);
    if (!this.canvas) throw new Error('gfx: canvas is unavailable');
    this.paneSize = paneSize;
    this.diameter = diameter;
    this.screenMaps = {};
    this.core = null;
    this._autoBloom = true;
  }

  get auto_bloom_enabled() { return this._autoBloom; }
  set auto_bloom_enabled(value) {
    this._autoBloom = value !== false;
    this.core?.setBloom(this._autoBloom ? { mode: 'auto' } : { mode: 'off' });
  }

  updateScreenMap(screenMapsData) {
    // Keep the previous screenmap when an empty update arrives; strips can
    // register a moment after the first frames at startup.
    if (!screenMapsData || Object.keys(screenMapsData).length === 0) return;
    this.screenMaps = screenMapsData;
    if (this.core) this.core.setScreenmap(toGfxScreenmap(this.screenMaps));
  }

  updateCanvas(frameData) {
    if (!frameData) return;
    if (frameData.screenMap) this.updateScreenMap(frameData.screenMap);
    if (!this.core) {
      // Frames can legitimately arrive before the screenmap during startup;
      // skip them until coordinates exist instead of erroring every frame.
      if (Object.keys(this.screenMaps).length === 0) return;
      this.core = createGfxCore({
        canvas: this.canvas,
        screenmap: toGfxScreenmap(this.screenMaps),
        paneSize: this.paneSize,
        diameter: this.diameter,
        bloom: this._autoBloom ? { mode: 'auto' } : { mode: 'off' },
      });
    }
    const frame = combineFrame(frameData);
    // Startup ticks can deliver zero strip bytes before the first show();
    // the core treats a short frame as an error, so skip until data exists.
    if (frame.byteLength === 0) return;
    this.core.pushFrame(frame);
  }

  reset() { this.core?.dispose(); this.core = null; }
  dispose() { this.reset(); }
}
