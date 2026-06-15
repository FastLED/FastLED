#!/usr/bin/env node
/**
 * One-shot converter for FxSdCard sample data.
 *
 * Reads the legacy v1 screenmap.json plus a raw .rgb video and emits a
 * single .fled container (v1 binary header + v2 screenmap JSON + raw
 * frames). Spec: https://github.com/zackees/ledmapper/blob/main/docs/fled-format.md
 *
 * Usage:
 *   node wrap-fled.mjs <screenmap.json> <video.rgb> <out.fled>
 *
 * The .fled output is committed to the repo so end users do not need to
 * run this. It is kept here as a documented record of how the .fled
 * files were produced.
 */

import fs from 'node:fs';

const [, , screenmapPath, rgbPath, outPath] = process.argv;
if (!screenmapPath || !rgbPath || !outPath) {
    console.error('usage: node wrap-fled.mjs <screenmap.json> <video.rgb> <out.fled>');
    process.exit(1);
}

// ── Load + upgrade v1 -> v2 ─────────────────────────────────────────────
const v1Text = fs.readFileSync(screenmapPath, 'utf8');
const v1 = JSON.parse(v1Text);
if (!v1.map || typeof v1.map !== 'object') {
    console.error(`${screenmapPath}: not a v1 screenmap (missing 'map' object)`);
    process.exit(2);
}

// Convert every v1 strip into a v2 segment. v2 requires { id, pin, group, x, y };
// diameter is optional. The order in the file becomes the LED concatenation order.
const segments = [];
let pinCounter = 0;
for (const [name, strip] of Object.entries(v1.map)) {
    if (!Array.isArray(strip.x) || !Array.isArray(strip.y) || strip.x.length !== strip.y.length) {
        console.error(`segment '${name}': missing or mismatched x/y arrays`);
        process.exit(3);
    }
    const seg = {
        id: name,
        pin: String(pinCounter++),
        group: 'default',
        x: strip.x.slice(),
        y: strip.y.slice(),
    };
    if (typeof strip.diameter === 'number' && Number.isFinite(strip.diameter)) {
        seg.diameter = strip.diameter;
    }
    segments.push(seg);
}

const v2 = {
    version: 2,
    groups: {
        default: { color: '#3b82f6' },
    },
    segments,
};
const v2JsonStr = JSON.stringify(v2);
const v2JsonBytes = Buffer.from(v2JsonStr, 'utf8');

// ── Build header ────────────────────────────────────────────────────────
const header = Buffer.alloc(12);
header.write('FLED', 0, 'ascii');
header[4] = 1;    // version
header[5] = 0;    // pixel_format = rgb8
// bytes 6, 7 reserved (already zero)
header.writeUInt32LE(v2JsonBytes.length, 8);

// ── Concatenate header + JSON + payload ────────────────────────────────
const rgb = fs.readFileSync(rgbPath);
const out = Buffer.concat([header, v2JsonBytes, rgb]);
fs.writeFileSync(outPath, out);

console.log(`${outPath}: ${out.length} bytes (header 12 + json ${v2JsonBytes.length} + payload ${rgb.length})`);
console.log(`  segments=${segments.length}, total LEDs=${segments.reduce((s, x) => s + x.x.length, 0)}`);
