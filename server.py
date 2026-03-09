"""
server.py - PROBE MMUKO OS Web Server
Serves index.html and exposes /probe endpoint so the UI can call probe.exe

Usage:
    python server.py
    python server.py 8080

Then open: http://localhost:8000
"""

import http.server
import socketserver
import subprocess
import json
import os
import sys
import urllib.parse

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
BASE = os.path.dirname(os.path.abspath(__file__))

# Find probe binary
PROBE_BIN = None
for candidate in ["probe.exe", "probe", "./probe.exe", "./probe"]:
    if os.path.exists(os.path.join(BASE, candidate)):
        PROBE_BIN = os.path.join(BASE, candidate)
        break

class ProbeHandler(http.server.SimpleHTTPRequestHandler):

    def log_message(self, format, *args):
        print(f"  {self.address_string()} {format % args}")

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)

        # ── /probe?addr=8.8.8.8&q=WHO ──────────────────────────────────────
        if parsed.path == "/probe":
            params = urllib.parse.parse_qs(parsed.query)
            addr   = params.get("addr", [""])[0].strip()
            q      = params.get("q",    ["WHERE"])[0].strip().upper()

            if not addr:
                self._json(400, {"error": "missing addr"})
                return

            result = self._run_probe(addr, q)
            self._json(200, result)
            return

        # ── /probe/network?cidr=192.168.1.0/24 ─────────────────────────────
        if parsed.path == "/probe/network":
            params = urllib.parse.parse_qs(parsed.query)
            cidr   = params.get("cidr", ["192.168.1.0/24"])[0].strip()
            result = self._run_probe_network(cidr)
            self._json(200, result)
            return

        # ── Static files ────────────────────────────────────────────────────
        super().do_GET()

    def _run_probe(self, addr, question):
        if not PROBE_BIN:
            return {"state": "MAYBE", "detail": f"{question}: probe binary not found", "addr": addr}

        try:
            result = subprocess.run(
                [PROBE_BIN, addr, question],
                capture_output=True, text=True, timeout=5
            )
            output = result.stdout.strip()

            # Parse [YES/NO/MAYBE] detail
            state, detail = self._parse(output)
            return {"state": state, "detail": detail, "addr": addr, "question": question}

        except subprocess.TimeoutExpired:
            return {"state": "MAYBE", "detail": f"{question}: {addr} timeout", "addr": addr}
        except Exception as e:
            return {"state": "NO", "detail": str(e), "addr": addr}

    def _run_probe_network(self, cidr):
        if not PROBE_BIN:
            return {"nodes": [], "error": "probe binary not found"}
        try:
            result = subprocess.run(
                [PROBE_BIN, "network", cidr],
                capture_output=True, text=True, timeout=60
            )
            nodes = []
            for line in result.stdout.splitlines():
                line = line.strip()
                if line.startswith("["):
                    state, detail = self._parse(line)
                    nodes.append({"state": state, "detail": detail})
            return {"nodes": nodes, "cidr": cidr}
        except subprocess.TimeoutExpired:
            return {"nodes": [], "error": "network scan timeout"}

    def _parse(self, output):
        import re
        m = re.match(r'\[(YES|NO|MAYBE)\]\s*(.*)', output)
        if m:
            return m.group(1), m.group(2)
        return "MAYBE", output or "no output"

    def _json(self, code, data):
        body = json.dumps(data, indent=2).encode()
        self.send_response(code)
        self.send_header("Content-Type",  "application/json")
        self.send_header("Content-Length", len(body))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(body)

# ── Serve ────────────────────────────────────────────────────────────────────

os.chdir(BASE)

with socketserver.TCPServer(("", PORT), ProbeHandler) as httpd:
    httpd.allow_reuse_address = True
    print(f"\n  PROBE // MMUKO OS Web Server")
    print(f"  ─────────────────────────────")
    print(f"  http://localhost:{PORT}")
    print(f"  http://localhost:{PORT}/probe?addr=8.8.8.8&q=WHO")
    print(f"  http://localhost:{PORT}/probe/network?cidr=192.168.1.0/24")
    print(f"  probe binary: {PROBE_BIN or 'NOT FOUND'}")
    print(f"\n  Ctrl+C to stop\n")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n  Server stopped.")
