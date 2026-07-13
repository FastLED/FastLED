// @ts-nocheck

/** FastLED boundary adapter for the shared @fastled/gfx render core. */
import { createGfxCore } from '@fastled/gfx/core';

function asBytes(value) {
  if (value instanceof Uint8Array) return value;
  if (ArrayBuffer.isView(value)) return new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
  if (Array.isArray(value)) return Uint8Array.from(value);
  return null;
}

/** Convert FastLED's `{group: {strips: {id: {x, y}}}}` shape to package JSON. */
export function toGfxScreenmap(screenMaps) {
  if (screenMaps?.map) return screenMaps;
  const map = {};
  for (const [groupId, group] of Object.entries(screenMaps ?? {})) {
    if (group?.strips && typeof group.strips === 'object') {
      for (const [stripId, strip] of Object.entries(group.strips)) {
        if (Array.isArray(strip?.x) && Array.isArray(strip?.y)) map[`${groupId}:${stripId}`] = strip;
      }
    } else if (Array.isArray(group?.x) && Array.isArray(group?.y)) {
      map[groupId] = group;
    }
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
    this.screenMaps = screenMapsData ?? {};
    if (this.core) this.core.setScreenmap(toGfxScreenmap(this.screenMaps));
  }

  updateCanvas(frameData) {
    if (!frameData) return;
    if (frameData.screenMap) this.updateScreenMap(frameData.screenMap);
    if (!this.core) {
      if (Object.keys(this.screenMaps).length === 0) throw new Error('screenmap: frame arrived before initialization');
      this.core = createGfxCore({
        canvas: this.canvas,
        screenmap: toGfxScreenmap(this.screenMaps),
        paneSize: this.paneSize,
        diameter: this.diameter,
        bloom: this._autoBloom ? { mode: 'auto' } : { mode: 'off' },
      });
    }
    this.core.pushFrame(combineFrame(frameData));
  }

  reset() { this.core?.dispose(); this.core = null; }
  dispose() { this.reset(); }
}
