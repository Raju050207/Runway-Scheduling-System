#!/usr/bin/env python3
"""
Intelligent Airport Runway Operation and Scheduling System
Web Server Backend (Python stdlib - Zero external pip dependencies)
Dispatches all core scheduling & algorithmic tasks directly to the compiled C++ binary.
Alliance University - Alliance School of Advanced Computing
"""

import os
import sys
import json
import subprocess
from http.server import HTTPServer, SimpleHTTPRequestHandler
from urllib.parse import urlparse

PORT = int(os.environ.get("PORT", 5000))
if len(sys.argv) > 1 and sys.argv[1].isdigit():
    PORT = int(sys.argv[1])

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
ENGINE_BIN = os.path.join(BASE_DIR, "runway_engine")

class ReusableHTTPServer(HTTPServer):
    allow_reuse_address = True

def ensure_engine_compiled():
    """Ensure the native C++ engine is built and executable."""
    if not os.path.exists(ENGINE_BIN):
        print("Compiling C++ engine binary...")
        cmd = ["g++", "-std=c++17", "-O2", os.path.join(BASE_DIR, "runway_engine.cpp"), "-o", ENGINE_BIN]
        res = subprocess.run(cmd, cwd=BASE_DIR, capture_output=True, text=True)
        if res.returncode != 0:
            print(f"Error compiling C++ engine: {res.stderr}")
            sys.exit(1)
        print("C++ engine successfully compiled.")

def call_cpp_engine(args):
    """Call native C++ binary and return parsed JSON output."""
    cmd = [ENGINE_BIN] + [str(a) for a in args]
    try:
        res = subprocess.run(cmd, cwd=BASE_DIR, capture_output=True, text=True, check=True)
        return json.loads(res.stdout)
    except subprocess.CalledProcessError as e:
        print(f"Error calling C++ engine: {e.stderr}")
        return {"error": e.stderr}
    except Exception as e:
        print(f"Engine invocation error: {str(e)}")
        return {"error": str(e)}

class RunwayAPIHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=BASE_DIR, **kwargs)

    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path == "/api/state":
            state = call_cpp_engine(["get_state"])
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(json.dumps(state).encode("utf-8"))
            return

        # Default static file serving (index.html, etc.)
        return super().do_GET()

    def do_POST(self):
        parsed = urlparse(self.path)
        if parsed.path == "/api/action":
            content_length = int(self.headers.get("Content-Length", 0))
            post_data = self.rfile.read(content_length)
            try:
                payload = json.loads(post_data.decode("utf-8"))
                action = payload.get("action", "")

                args = [action]
                if action == "add_aircraft":
                    args.extend([
                        payload["id"],
                        payload["type"],
                        payload["time"],
                        payload["priority"],
                        1 if payload.get("emergency") else 0,
                        payload["runwayTime"]
                    ])
                elif action in ("emergency", "delete_aircraft"):
                    args.append(payload["id"])
                elif action == "delay":
                    args.extend([payload["id"], payload["delayMins"]])
                elif action == "weather":
                    args.append(payload["weather"])
                elif action == "toggle_runway":
                    args.append(payload["runwayId"])

                new_state = call_cpp_engine(args)
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                self.wfile.write(json.dumps(new_state).encode("utf-8"))
            except Exception as e:
                self.send_response(400)
                self.send_header("Content-Type", "application/json")
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                self.wfile.write(json.dumps({"error": str(e)}).encode("utf-8"))
            return

        self.send_response(404)
        self.end_headers()

def run_server():
    ensure_engine_compiled()
    server_address = ("0.0.0.0", PORT)
    httpd = ReusableHTTPServer(server_address, RunwayAPIHandler)
    print(f"\n============================================================")
    print(f"  ALLIANCE UNIVERSITY - AIRPORT RUNWAY SCHEDULING SYSTEM")
    print(f"  Web Interface running at: http://localhost:{PORT}")
    print(f"  Core Decision-Support Logic: Native C++ Engine (runway_engine)")
    print(f"============================================================\n")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down server.")
        httpd.server_close()

if __name__ == "__main__":
    run_server()

