"""
Imperium Bot — HTTP server

Two endpoints:
  GET  /        — keep-alive ping for UptimeRobot
  POST /auth    — loader authentication endpoint

POST /auth expects JSON body:
  { "username": "...", "password": "...", "hwid": "..." }

Returns JSON:
  { "success": true,  "message": "OK" }
  { "success": false, "message": "..." }
"""

from http.server import BaseHTTPRequestHandler, HTTPServer
import threading
import json


class ImperiumHandler(BaseHTTPRequestHandler):

    # ── GET / ─────────────────────────────────────────────────────────────────
    def do_GET(self):
        if self.path == "/":
            self._respond(200, b"OK", "text/plain")
        else:
            self._respond(404, b"Not Found", "text/plain")

    # ── HEAD / ───────────────────────────────────────────────────────────────
    def do_HEAD(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.end_headers()

    # ── POST /auth ────────────────────────────────────────────────────────────
    def do_POST(self):
        if self.path != "/auth":
            self._respond(404, b"Not Found", "text/plain")
            return

        # Read body
        length = int(self.headers.get("Content-Length", 0))
        if length == 0:
            self._json_respond(400, {"success": False, "message": "Empty body."})
            return

        try:
            body = json.loads(self.rfile.read(length).decode("utf-8"))
        except (json.JSONDecodeError, UnicodeDecodeError):
            self._json_respond(400, {"success": False, "message": "Invalid JSON."})
            return

        username = (body.get("username") or "").strip()
        password = (body.get("password") or "").strip()
        hwid     = (body.get("hwid")     or "").strip()

        if not username or not password or not hwid:
            self._json_respond(400, {
                "success": False,
                "message": "username, password, and hwid are required."
            })
            return

        # Import here to avoid circular issues at module load time
        from cogs.database import authenticate
        success, message = authenticate(username, password, hwid)

        status = 200 if success else 401
        self._json_respond(status, {"success": success, "message": message})

    # ── Helpers ───────────────────────────────────────────────────────────────
    def _respond(self, code: int, body: bytes, content_type: str):
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _json_respond(self, code: int, data: dict):
        body = json.dumps(data).encode("utf-8")
        self._respond(code, body, "application/json")

    def log_message(self, format, *args):
        # Only log auth attempts, suppress keep-alive noise
        if self.path == "/auth":
            print(f"[Auth] {self.address_string()} — {format % args}", flush=True)


def keep_alive():
    server = HTTPServer(("0.0.0.0", 8080), ImperiumHandler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    print("[Keep-Alive] HTTP server running on port 8080 (/auth endpoint active)", flush=True)
