#!/usr/bin/env python3
# Copyright Plain Browser contributors. MPL-2.0.
"""Static file server that also accepts a WebSocket handshake (answers 101 and closes after a second).
Also answers /redirect?to=<url> with a 302. Used by scripts/test-adblock.ps1: a socket that opens was allowed, one that errors although the server would
have accepted it was stopped by the browser. usage: test-http-ws-server.py <port> <directory>"""
import base64, functools, hashlib, sys, time, urllib.parse
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

class Handler(SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.headers.get("Upgrade", "").lower() == "websocket":
            accept = base64.b64encode(hashlib.sha1((self.headers["Sec-WebSocket-Key"] + GUID).encode()).digest()).decode()
            self.send_response(101, "Switching Protocols")
            self.send_header("Upgrade", "websocket")
            self.send_header("Connection", "Upgrade")
            self.send_header("Sec-WebSocket-Accept", accept)
            self.end_headers()
            self.wfile.flush()
            time.sleep(1)
            return
        if self.path.startswith("/redirect?"):  # 302 to ?to=<url>, for the redirect checks of test-adblock.ps1
            target = urllib.parse.parse_qs(urllib.parse.urlsplit(self.path).query).get("to", [""])[0]
            self.send_response(302)
            self.send_header("Location", target)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        super().do_GET()

if __name__ == "__main__":
    port, directory = int(sys.argv[1]), sys.argv[2]
    ThreadingHTTPServer(("127.0.0.1", port), functools.partial(Handler, directory=directory)).serve_forever()
