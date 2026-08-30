#!/usr/bin/env python3
"""Deterministic stand-in for an embedding llama-server (test fixture ONLY).

Speaks the two endpoints core/embed_pod.c and core/semsearchapi.c use —
POST /v1/embeddings (OpenAI shape, array `input`) and GET /v1/models — plus
/health. Vectors are hashed character 3-grams, L2-normalised, so similar text
gets similar vectors and a nearest-neighbour query returns something
inspectable; they carry no meaning beyond that. It exists so the pod, the
vec0 table and the endpoint can be exercised end-to-end on a machine that has
no embedding GGUF, and it says so in its model id.

    python3 tools/embed_stub_server.py [port=8099] [dim=64]
"""
import json, math, sys, hashlib
from http.server import BaseHTTPRequestHandler, HTTPServer

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8099
DIM  = int(sys.argv[2]) if len(sys.argv) > 2 else 64
MODEL = f"stub-trigram-hash-{DIM}d"

def embed(text):
    v = [0.0] * DIM
    t = f"  {text.lower()}  "
    for i in range(len(t) - 2):
        g = t[i:i+3]
        h = int.from_bytes(hashlib.blake2b(g.encode(), digest_size=8).digest(), "little")
        v[h % DIM] += 1.0 if (h >> 63) else -1.0
    n = math.sqrt(sum(x * x for x in v)) or 1.0
    return [x / n for x in v]

class H(BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def _send(self, code, obj):
        b = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(b)))
        self.end_headers()
        self.wfile.write(b)
    def do_GET(self):
        if self.path == "/health": return self._send(200, {"status": "ok"})
        if self.path == "/v1/models": return self._send(200, {"object": "list", "data": [{"id": MODEL, "object": "model"}]})
        self._send(404, {"error": "not_found"})
    def do_POST(self):
        if self.path not in ("/v1/embeddings", "/embedding"): return self._send(404, {"error": "not_found"})
        n = int(self.headers.get("Content-Length", "0"))
        body = json.loads(self.rfile.read(n) or b"{}")
        inp = body.get("input", "")
        if isinstance(inp, str): inp = [inp]
        data = [{"object": "embedding", "index": i, "embedding": embed(s)} for i, s in enumerate(inp)]
        self._send(200, {"object": "list", "model": MODEL, "data": data,
                         "usage": {"prompt_tokens": 0, "total_tokens": 0}})

if __name__ == "__main__":
    print(f"[embed-stub] {MODEL} on :{PORT}", flush=True)
    HTTPServer(("127.0.0.1", PORT), H).serve_forever()
