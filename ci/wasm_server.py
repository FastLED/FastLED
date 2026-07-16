"""Serve a WASM example with the headers required by pthreads/SAB builds."""

import argparse
import http.server
import socketserver
import webbrowser
from pathlib import Path


class IsolatedRequestHandler(http.server.SimpleHTTPRequestHandler):
    """Static-file handler that enables cross-origin isolation."""

    extensions_map = {
        **http.server.SimpleHTTPRequestHandler.extensions_map,
        ".js": "application/javascript",
        ".mjs": "application/javascript",
        ".wasm": "application/wasm",
        ".json": "application/json",
        ".css": "text/css",
    }

    def end_headers(self) -> None:
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        super().end_headers()

    def log_message(self, format: str, *args: object) -> None:  # noqa: A002
        pass


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--no-browser", action="store_true")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    directory = args.directory.resolve()
    if not directory.is_dir():
        raise FileNotFoundError(f"WASM output directory not found: {directory}")

    handler = lambda *handler_args, **handler_kwargs: IsolatedRequestHandler(  # noqa: E731
        *handler_args, directory=str(directory), **handler_kwargs
    )
    with socketserver.ThreadingTCPServer(("", args.port), handler) as server:
        url = f"http://127.0.0.1:{args.port}/"
        print(f"Serving cross-origin-isolated WASM app at {url}", flush=True)
        if not args.no_browser:
            webbrowser.open(url)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            print("\nStopping WASM server", flush=True)


if __name__ == "__main__":
    main()
