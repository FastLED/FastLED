#!/usr/bin/env python3
"""
HTTP Test Server for NetTest.ino Client

Mimics httpbin.org endpoints for local testing of fl::fetch API.

Usage:
    uv run python examples/NetTest/test_server.py
"""

import _thread
import argparse
import json
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer
from typing import Any

from rich.console import Console


console = Console()


class HTTPBinHandler(BaseHTTPRequestHandler):
    """HTTP request handler that mimics httpbin.org API."""

    def log_message(self, format: str, *args: Any) -> None:
        """Override to use rich console for logging."""
        console.print(
            f"[cyan]{self.address_string()}[/cyan] - {format % args}", style="dim"
        )

    def send_json_response(self, data: dict[str, Any], status_code: int = 200) -> None:
        """Send JSON response with appropriate headers."""
        json_data = json.dumps(data, indent=2)
        self.send_response(status_code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(json_data)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(json_data.encode())

    def send_text_response(self, text: str, status_code: int = 200) -> None:
        """Send plain text response."""
        self.send_response(status_code)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(text)))
        self.end_headers()
        self.wfile.write(text.encode())

    def do_GET(self) -> None:
        """Handle GET requests."""
        if self.path == "/":
            # Root endpoint - HTML welcome page
            html = """<!DOCTYPE html>
<html>
<head><title>FastLED Test Server</title></head>
<body>
<h1>FastLED HTTP Test Server</h1>
<p>This server mimics httpbin.org endpoints for testing FastLED fetch API.</p>
<h2>Available Endpoints:</h2>
<ul>
<li><a href="/json">/json</a> - Sample JSON slideshow data</li>
<li><a href="/get">/get</a> - Echo request information</li>
<li><a href="/ping">/ping</a> - Health check (returns "pong")</li>
</ul>
</body>
</html>"""
            self.send_text_response(html)

        elif self.path == "/json":
            # Mimic httpbin.org/json - slideshow data
            data = {
                "slideshow": {
                    "author": "FastLED Community",
                    "title": "FastLED Tutorial",
                    "slides": [
                        {
                            "title": "Introduction to FastLED",
                            "type": "tutorial",
                        },
                        {
                            "title": "LED Basics",
                            "type": "lesson",
                        },
                        {
                            "title": "HTTP Fetch API",
                            "type": "demo",
                        },
                    ],
                }
            }
            console.print("[green]→ /json - Returning slideshow data[/green]")
            self.send_json_response(data)

        elif self.path.startswith("/get"):
            # Mimic httpbin.org/get - echo request information
            query_params: dict[str, str] = {}
            if "?" in self.path:
                query_string = self.path.split("?", 1)[1]
                for param in query_string.split("&"):
                    if "=" in param:
                        key, value = param.split("=", 1)
                        query_params[key] = value

            headers: dict[str, str] = {}
            for header_name, header_value in self.headers.items():
                headers[header_name] = header_value

            data: dict[str, Any] = {
                "args": query_params,
                "headers": headers,
                "origin": self.client_address[0],
                "url": f"http://{self.headers.get('Host', 'localhost')}{self.path}",
            }
            console.print("[green]→ /get - Returning request info[/green]")
            self.send_json_response(data)

        elif self.path == "/ping":
            # Health check endpoint
            console.print("[green]→ /ping - Health check[/green]")
            self.send_text_response("pong\n")

        else:
            # 404 Not Found
            console.print(f"[red]→ {self.path} - Not Found[/red]")
            self.send_json_response(
                {"error": "Not Found", "path": self.path}, status_code=404
            )

    def do_POST(self) -> None:
        """Handle POST requests."""
        content_length = int(self.headers.get("Content-Length", 0))
        post_data = self.rfile.read(content_length).decode("utf-8")

        if self.path == "/post":
            # Mimic httpbin.org/post - echo POST data
            headers: dict[str, str] = {}
            for header_name, header_value in self.headers.items():
                headers[header_name] = header_value

            json_data: Any = None
            try:
                if post_data:
                    json_data = json.loads(post_data)
                else:
                    json_data = {}
            except json.JSONDecodeError:
                json_data = None

            data: dict[str, Any] = {
                "args": {},
                "data": post_data,
                "json": json_data,
                "headers": headers,
                "origin": self.client_address[0],
                "url": f"http://{self.headers.get('Host', 'localhost')}{self.path}",
            }
            console.print("[green]→ POST /post - Returning echo data[/green]")
            self.send_json_response(data)
        else:
            console.print(f"[red]→ POST {self.path} - Not Found[/red]")
            self.send_json_response(
                {"error": "Not Found", "path": self.path}, status_code=404
            )


def main() -> int:
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="HTTP test server for NetTest.ino client"
    )
    parser.add_argument("--host", default="localhost", help="Server host")
    parser.add_argument("--port", type=int, default=8081, help="Server port")
    args = parser.parse_args()

    server_address = (args.host, args.port)
    httpd = HTTPServer(server_address, HTTPBinHandler)

    console.print("\n[bold green]FastLED HTTP Test Server[/bold green]")
    console.print(f"Listening on [cyan]http://{args.host}:{args.port}/[/cyan]\n")
    console.print("[dim]Available endpoints:[/dim]")
    console.print("  [cyan]GET  /[/cyan]      - Welcome page")
    console.print("  [cyan]GET  /json[/cyan]  - Sample JSON slideshow data")
    console.print("  [cyan]GET  /get[/cyan]   - Echo request information")
    console.print("  [cyan]GET  /ping[/cyan]  - Health check")
    console.print("  [cyan]POST /post[/cyan]  - Echo POST data")
    console.print("\n[yellow]Press Ctrl+C to stop[/yellow]\n")

    httpd.serve_forever()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        console.print("\n[yellow]Server stopped[/yellow]")
        _thread.interrupt_main()
        sys.exit(130)
