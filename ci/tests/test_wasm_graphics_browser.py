"""Pixel regressions for the production WASM viewer using real Chromium WebGL."""

import os
from collections.abc import Iterator
from pathlib import Path
from urllib.parse import urlparse

import pytest
from playwright.sync_api import Page, Route, sync_playwright


COMPILER = Path(__file__).resolve().parents[2] / "src/platforms/wasm/compiler"
pytestmark = pytest.mark.slow


@pytest.fixture(scope="module")
def graphics_page() -> Iterator[Page]:
    with sync_playwright() as playwright:
        browser = playwright.chromium.launch(
            args=["--use-angle=swiftshader", "--enable-unsafe-swiftshader"],
            # SwiftShader's headless Vulkan driver has no Wayland surface extension.
            env={
                key: value
                for key, value in os.environ.items()
                if key != "WAYLAND_DISPLAY"
            },
        )
        page = browser.new_page()

        def serve_source(route: Route) -> None:
            path = urlparse(route.request.url).path.lstrip("/")
            if not path:
                route.fulfill(
                    content_type="text/html",
                    body="""<script type="importmap">{"imports": {
                        "@fastled/gfx/core": "/node_modules/@fastled/gfx/dist/core.js",
                        "three": "/node_modules/three/build/three.module.js"
                    }}</script><canvas id='canvas'></canvas>""",
                )
            else:
                route.fulfill(
                    content_type="text/javascript",
                    body=(COMPILER / path).read_text(encoding="utf-8"),
                )

        page.route("http://wasm.test/**", serve_source)
        page.goto("http://wasm.test/")
        page.evaluate("""async () => {
            const { GraphicsManager } = await import('/modules/graphics/graphics_manager.ts');
            globalThis.GraphicsManager = GraphicsManager;
        }""")
        yield page
        browser.close()


@pytest.mark.parametrize("diameter", [None, -1, 0, 0.1, 1, 3])
@pytest.mark.parametrize("offscreen", [False, True])
def test_diameter_renders_led(
    graphics_page: Page, diameter: float | None, offscreen: bool
) -> None:
    result = graphics_page.evaluate(
        """({diameter, offscreen}) => {
        const canvas = offscreen ? new OffscreenCanvas(3, 3) : document.createElement('canvas');
        const manager = new GraphicsManager({canvas});
        const strip = {map: {x: [1, 0, 2], y: [1, 0, 2]}};
        if (diameter !== null) strip.diameter = diameter;
        manager.updateScreenMap({0: {strips: {0: strip}}});
        manager.updateCanvas([{strip_id: 0, pixel_data: [255,0,0]}]);
        const gl = manager.gl;
        const pixel = new Uint8Array(4);
        gl.readPixels(Math.floor(canvas.width / 2), Math.floor(canvas.height / 2),
                      1, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixel);
        const lit = manager.texData.filter((value, index) => index % 3 === 0 && value === 255).length;
        const result = {pixel: Array.from(pixel), lit, error: gl.getError()};
        gl.getExtension('WEBGL_lose_context').loseContext();
        return result;
    }""",
        {"diameter": diameter, "offscreen": offscreen},
    )
    assert result["error"] == 0
    assert result["pixel"] == [255, 0, 0, 255]
    assert result["lit"] == (9 if diameter == 3 else 1)


@pytest.mark.parametrize("offscreen", [False, True])
def test_strips_share_coordinates(graphics_page: Page, offscreen: bool) -> None:
    result = graphics_page.evaluate(
        """(offscreen) => {
        const canvas = offscreen ? new OffscreenCanvas(1, 1) : document.createElement('canvas');
        const manager = new GraphicsManager({canvas});
        const positive = Array.from({length: 256}, (_, i) => i);
        const negative = positive.map(i => -i);
        const frames = [
            {strip_id: 0, pixel_data: positive.flatMap(() => [0, 0, 255])},
            {strip_id: 1, pixel_data: negative.flatMap(() => [255, 0, 0])}
        ];
        const results = [];
        // Repeat with a translated layout to exercise bounds invalidation.
        for (const origin of [0, -300]) {
            manager.updateScreenMap({
                0: {strips: {0: {diameter: 1, map: {
                    x: positive.map(x => x + origin), y: positive.map(y => y + origin)}}}},
                1: {strips: {1: {diameter: 1, map: {
                    x: negative.map(x => x + origin), y: negative.map(y => y + origin)}}}}
            });
            manager.updateCanvas(frames);
            const gl = manager.gl;
            const pixel = new Uint8Array(4);
            gl.readPixels(canvas.width - 1, canvas.height - 1, 1, 1,
                          gl.RGBA, gl.UNSIGNED_BYTE, pixel);
            results.push({width: manager.gridWidth, height: manager.gridHeight,
                red: manager.texData.filter((v, i) => i % 3 === 0 && v === 255).length,
                blue: manager.texData.filter((v, i) => i % 3 === 2 && v === 255).length,
                corner: Array.from(pixel), error: gl.getError()});
        }
        manager.gl.getExtension('WEBGL_lose_context').loseContext();
        return results;
    }""",
        offscreen,
    )
    for frame in result:
        assert frame == {
            "width": 511,
            "height": 511,
            "red": 256,
            "blue": 255,
            "corner": [0, 0, 255, 255],
            "error": 0,
        }


@pytest.mark.parametrize("offscreen", [False, True])
@pytest.mark.parametrize("diameter", [-1, 1])
@pytest.mark.parametrize("dimensions", [(3, 48), (48, 3)])
def test_default_renderer_fits_extreme_aspect_ratio(
    graphics_page: Page, offscreen: bool, diameter: float, dimensions: tuple[int, int]
) -> None:
    result = graphics_page.evaluate(
        """async ({offscreen, diameter, dimensions: [width, height]}) => {
        const { GraphicsManagerGfx } = await import('/modules/graphics/graphics_manager_gfx.ts');
        const canvas = offscreen ? new OffscreenCanvas(1, 1) : document.createElement('canvas');
        const manager = new GraphicsManagerGfx({canvas});
        const x = [], y = [];
        for (let row = 0; row < height; row++) {
            for (let col = 0; col < width; col++) { x.push(col); y.push(row); }
        }
        manager.updateScreenMap({0: {strips: {0: {map: {x, y}, diameter}}}});
        manager.updateCanvas([{strip_id: 0, pixel_data: x.flatMap(() => [255, 0, 0])}]);
        const gl = canvas.getContext('webgl2');
        const result = await new Promise((resolve, reject) => {
            let request;
            const timeout = setTimeout(() => {
                cancelAnimationFrame(request);
                manager.dispose();
                gl.getExtension('WEBGL_lose_context').loseContext();
                reject(new Error('Default renderer produced no frame within 5 seconds'));
            }, 5000);
            function sample() {
                if (manager.core.getStats().framesRendered === 0) {
                    request = requestAnimationFrame(sample);
                    return;
                }
                const pixels = new Uint8Array(gl.drawingBufferWidth * gl.drawingBufferHeight * 4);
                gl.readPixels(0, 0, gl.drawingBufferWidth, gl.drawingBufferHeight,
                              gl.RGBA, gl.UNSIGNED_BYTE, pixels);
                let lit = 0;
                for (let i = 0; i < pixels.length; i += 4) { if (pixels[i] > 100) lit++; }
                clearTimeout(timeout);
                resolve({width: canvas.width, height: canvas.height,
                    bufferWidth: gl.drawingBufferWidth, bufferHeight: gl.drawingBufferHeight,
                    maxTexture: gl.getParameter(gl.MAX_TEXTURE_SIZE),
                    maxRenderbuffer: gl.getParameter(gl.MAX_RENDERBUFFER_SIZE),
                    viewport: Array.from(gl.getParameter(gl.MAX_VIEWPORT_DIMS)),
                    lit, error: gl.getError()});
            }
            request = requestAnimationFrame(sample);
        });
        manager.dispose();
        gl.getExtension('WEBGL_lose_context').loseContext();
        return result;
    }""",
        {"offscreen": offscreen, "diameter": diameter, "dimensions": dimensions},
    )
    assert result["error"] == 0
    assert result["lit"] > 0
    assert result["width"] == result["bufferWidth"]
    assert result["height"] == result["bufferHeight"]
    assert result["width"] <= min(
        result["maxTexture"], result["maxRenderbuffer"], result["viewport"][0]
    )
    assert result["height"] <= min(
        result["maxTexture"], result["maxRenderbuffer"], result["viewport"][1]
    )
