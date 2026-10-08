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
                    content_type="text/html", body="<canvas id='canvas'></canvas>"
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
