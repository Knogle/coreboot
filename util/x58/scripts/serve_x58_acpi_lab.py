#!/usr/bin/env python3
"""Temporarily serve explicitly named non-secret diagnostic files, GET/HEAD only."""
import argparse
import hashlib
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
from pathlib import Path
import time
from urllib.parse import urlsplit

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--host', required=True)
p.add_argument('--port', type=int, default=8087)
p.add_argument('--seconds', type=int, default=3600)
p.add_argument('--file', action='append', type=Path, required=True)
a = p.parse_args()
if not 1 <= a.seconds <= 7200:
    p.error('duration must be 1..7200 seconds')
files = {}
for path in a.file:
    resolved = path.resolve(strict=True)
    if not resolved.is_file() or resolved.name in files:
        p.error('file must be regular and basenames unique')
    data = resolved.read_bytes()
    if len(data) > 8*1024*1024:
        p.error('diagnostic file exceeds 8 MiB limit')
    files['/'+resolved.name] = data
    print(json.dumps({'served_path': '/'+resolved.name, 'bytes': len(data),
                      'sha256': hashlib.sha256(data).hexdigest()}), flush=True)

class Handler(BaseHTTPRequestHandler):
    def serve_file(self, body):
        self.connection.settimeout(10)
        target = urlsplit(self.path)
        data = files.get(target.path)
        if data is None or target.query:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header('Content-Type', 'application/octet-stream')
        self.send_header('Content-Length', str(len(data)))
        self.send_header('Cache-Control', 'no-store')
        self.end_headers()
        if body:
            self.wfile.write(data)
    def do_GET(self):
        self.serve_file(True)
    def do_HEAD(self):
        self.serve_file(False)

server = HTTPServer((a.host, a.port), Handler)
server.timeout = 0.5
deadline = time.monotonic()+a.seconds
print(json.dumps({'listen': [a.host, a.port], 'seconds': a.seconds,
                  'directory_listing': False, 'uploads': False}), flush=True)
try:
    while time.monotonic() < deadline:
        server.handle_request()
finally:
    server.server_close()
    print('diagnostic HTTP server stopped', flush=True)
