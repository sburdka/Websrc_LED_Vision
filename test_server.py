#!/usr/bin/env python3
"""
LED Vision local test server
Serves all static web files + returns mock API responses so every page works in a desktop browser.

Usage:
    cd /path/to/Websrc_LED_Vision
    python3 test_server.py
    # then open http://localhost:8080/Home.html
"""

import http.server, json, mimetypes, os, urllib.parse

PORT     = 8080
WEB_ROOT = os.path.dirname(os.path.abspath(__file__))

# ── Mock API responses ────────────────────────────────────────────────────────

MOCK_VIDEOS = {
    "flag": 1,
    "files": ["/media/sample_video_1.mp4", "/media/sample_video_2.mp4"],
    "names": ["sample_video_1.mp4", "sample_video_2.mp4"],
    "sizes": [52428800, 10485760],
    "count": 2,
}
MOCK_IMAGES = {
    "flag": 1,
    "files": ["/media/photo_001.jpg", "/media/photo_002.jpg", "/media/photo_003.jpg"],
    "names": ["photo_001.jpg", "photo_002.jpg", "photo_003.jpg"],
    "sizes": [204800, 153600, 307200],
    "count": 3,
}

# ── Request handler ────────────────────────────────────────────────────────────

class Handler(http.server.BaseHTTPRequestHandler):

    def log_message(self, fmt, *args):
        status = args[1]
        color  = "\033[32m" if status.startswith("2") else "\033[31m" if status.startswith(("4","5")) else "\033[33m"
        print(f"  {color}{args[1]}\033[0m  {args[0]}")

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        path   = urllib.parse.unquote(parsed.path)

        # ── API routes ────────────────────────────────────────────────────────
        if path == "/api/videos":
            return self._json(MOCK_VIDEOS)
        if path == "/api/images":
            return self._json(MOCK_IMAGES)
        if path == "/api/serialid":
            return self._text("AMLOGIC-S905W-TEST-0001")
        if path in ("/api/sound", "/api/hdmi"):
            return self._text("ok")
        if path == "/api/importusb":
            return self._json({"status": "importing"})
        if path == "/api/importusb/status":
            return self._json({"flag": 1, "copied": 5})
        if path.startswith("/media/"):
            self.send_response(404); self.end_headers(); return

        # ── Static files ──────────────────────────────────────────────────────
        if path == "/":
            path = "/Home.html"

        # Try exact path, then with .html appended
        for candidate in [WEB_ROOT + path, WEB_ROOT + path + ".html"]:
            if os.path.isfile(candidate):
                mime, _ = mimetypes.guess_type(candidate)
                mime = mime or "application/octet-stream"
                with open(candidate, "rb") as f:
                    data = f.read()
                self.send_response(200)
                self.send_header("Content-Type",   mime)
                self.send_header("Content-Length", len(data))
                self.send_header("Cache-Control",  "no-store")
                self.end_headers()
                self.wfile.write(data)
                return

        self.send_response(404)
        self.send_header("Content-Type", "text/plain")
        self.end_headers()
        self.wfile.write(f"Not found: {path}".encode())

    def _json(self, data):
        body = json.dumps(data).encode()
        self.send_response(200)
        self.send_header("Content-Type",   "application/json")
        self.send_header("Content-Length", len(body))
        self.end_headers()
        self.wfile.write(body)

    def _text(self, text):
        body = text.encode()
        self.send_response(200)
        self.send_header("Content-Type",   "text/plain")
        self.send_header("Content-Length", len(body))
        self.end_headers()
        self.wfile.write(body)


# ── Entry point ───────────────────────────────────────────────────────────────

if __name__ == "__main__":
    mimetypes.add_type("image/svg+xml",       ".svg")
    mimetypes.add_type("font/ttf",            ".ttf")
    mimetypes.add_type("font/woff2",          ".woff2")
    mimetypes.add_type("application/javascript", ".js")

    print(f"\n  LED Vision test server")
    print(f"  ─────────────────────────────────────────")
    print(f"  Open in browser → http://localhost:{PORT}/Home.html\n")
    print(f"  Quick links:")
    print(f"    Main menu    http://localhost:{PORT}/Home.html")
    print(f"    Snellen      http://localhost:{PORT}/canva.html?data=1")
    print(f"    Multi-row    http://localhost:{PORT}/snellennext.html?val=3")
    print(f"    Video list   http://localhost:{PORT}/main.html")
    print(f"    Image list   http://localhost:{PORT}/image.html")
    print(f"    Media picker http://localhost:{PORT}/imgorvideo.html")
    print(f"    Settings     http://localhost:{PORT}/settingsmain.html\n")
    print(f"  Press Ctrl+C to stop\n")

    with http.server.HTTPServer(("0.0.0.0", PORT), Handler) as httpd:
        httpd.serve_forever()
