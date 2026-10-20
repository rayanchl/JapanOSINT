#!/usr/bin/env python3
"""vision_embed_server.py — image/text embedding sidecar for camera imagery.

A small HTTP server that turns camera stills (and text queries) into vectors in
ONE shared space, so a still can later be found by a sentence ("snow on the
road", "冠水した道路") or by another still. It is a sidecar in exactly the way
the text embed server (llama-server --embedding, launch.sh cmd_llama_embed) is:
the C binary talks to it over HTTP at JO_VISION_EMBED_URL and treats its
absence as "feature off", never as an error.

ENDPOINTS
  POST /embed/image   {"images": ["<base64>", ...]}
                      -> {"model", "dim", "data": [{"index", "embedding"}],
                          "errors": [{"index", "error"}]}
  POST /embed/text    {"input": ["...", ...]}            (same envelope)
  GET  /v1/models     {"object":"list","data":[{"id","object":"model","dim",...}]}
  GET  /health        {"status":"ok","model","dim","ready":true}

  An image that does not decode is reported in `errors` by index and the rest
  of the batch is still embedded — one bad JPEG from one camera must not throw
  away the other 31 (CLAUDE.md rule 2). `data` only ever holds real vectors.
  Every vector is L2-normalised, so dot product == cosine and an L2 index
  ranks identically (d^2 = 2 - 2cos), matching how intel_vec is queried.

MODEL
  VISION_EMBED_MODEL   Hugging Face id or a local directory.
                       Default: google/siglip2-base-patch16-224 — SigLIP 2,
                       Apache-2.0, trained with a multilingual (Gemma)
                       tokenizer, so Japanese and English queries land in the
                       same space as the images. Alternatives that also allow
                       this use: line-corporation/clip-japanese-base
                       (Apache-2.0; Japanese text only, needs
                       trust_remote_code), any open_clip SigLIP2 checkpoint
                       (e.g. hf-hub:timm/ViT-B-16-SigLIP2). Deliberately NOT
                       the default: jinaai/jina-clip-v2, which is
                       CC-BY-NC-4.0 (non-commercial).
  VISION_EMBED_BACKEND transformers | open_clip | auto (default auto: open_clip
                       for "hf-hub:" ids, transformers otherwise)
  VISION_EMBED_DEVICE  cpu | cuda | mps (default: cuda if available, else cpu)
  VISION_EMBED_BATCH   images/texts per forward pass (default 16)
  VISION_EMBED_TRUST_REMOTE_CODE=1  for models that ship their own code

  Models are NEVER downloaded by --check, and by the server only if the
  operator has not set HF_HUB_OFFLINE=1. launch.sh runs --check first and
  skips the component silently when the model is not already on disk.

LIMITS (per request; over-limit requests are refused with 413, not truncated)
  VISION_EMBED_MAX_ITEMS   64 inputs
  VISION_EMBED_MAX_BYTES   8 MiB per decoded image

STUB MODE (--stub, or VISION_EMBED_STUB=1)
  No ML dependencies at all, not even Pillow: each input's bytes are hashed
  (SHA-256 in counter mode) into a deterministic VISION_STUB_DIM-d (default 64)
  unit vector, model "stub-sha256-v1". The SAME input always yields the SAME
  vector, different inputs different ones — enough to test the wire format,
  batching, error reporting and the C client end to end. The vectors carry NO
  semantics and the model id says so; nothing should ever index them as if
  they did.

PRIVACY
  This server embeds whole frames. It has no face, person or identity feature
  and must not be given one; which camera channels may be indexed at all is
  decided by the C side (core/camera_privacy.h), not here.
"""
from __future__ import annotations

import argparse
import base64
import binascii
import hashlib
import io
import json
import math
import os
import struct
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

DEFAULT_MODEL = "google/siglip2-base-patch16-224"
MAX_BODY = 256 * 1024 * 1024


def env_int(name: str, default: int, lo: int, hi: int) -> int:
    try:
        v = int(os.environ.get(name, "") or default)
    except ValueError:
        v = default
    return max(lo, min(hi, v))


MAX_ITEMS = env_int("VISION_EMBED_MAX_ITEMS", 64, 1, 4096)
MAX_IMAGE_BYTES = env_int("VISION_EMBED_MAX_BYTES", 8 * 1024 * 1024, 1024, 128 * 1024 * 1024)
BATCH = env_int("VISION_EMBED_BATCH", 16, 1, 512)


def l2norm(v):
    n = math.sqrt(sum(x * x for x in v))
    if n == 0.0 or not math.isfinite(n):
        return None
    return [x / n for x in v]


# ── backends ─────────────────────────────────────────────────────────────────

class StubBackend:
    """Deterministic hash vectors. No third-party imports."""

    def __init__(self, dim: int):
        self.dim = dim
        self.model_id = "stub-sha256-v1"

    def _vec(self, data: bytes, domain: bytes):
        out = []
        ctr = 0
        while len(out) < self.dim:
            h = hashlib.sha256(domain + struct.pack(">I", ctr) + data).digest()
            for i in range(0, 32, 4):
                (u,) = struct.unpack(">I", h[i:i + 4])
                out.append(u / 2147483647.5 - 1.0)
            ctr += 1
        return l2norm(out[: self.dim])

    def embed_images(self, blobs):
        res = []
        for b in blobs:
            # A stub that accepted anything would make the error path
            # untestable: insist on a recognisable image signature.
            if not (b[:3] == b"\xff\xd8\xff" or b[:8] == b"\x89PNG\r\n\x1a\n"
                    or b[:6] in (b"GIF87a", b"GIF89a") or (b[:4] == b"RIFF" and b[8:12] == b"WEBP")):
                res.append(ValueError("not_an_image"))
                continue
            res.append(self._vec(b, b"img\0"))
        return res

    def embed_texts(self, texts):
        return [self._vec(t.encode("utf-8"), b"txt\0") for t in texts]


class TorchBackend:
    """transformers (CLIP/SigLIP/SigLIP2 AutoModel) or open_clip."""

    def __init__(self, model_id: str, backend: str, device: str | None, local_only: bool):
        import torch  # noqa: F401  (import errors are the caller's signal)
        from PIL import Image  # noqa: F401

        self.torch = torch
        self.model_id = model_id
        self.lock = threading.Lock()  # one forward pass at a time per model
        if device:
            self.device = device
        elif torch.cuda.is_available():
            self.device = "cuda"
        else:
            self.device = "cpu"
        if backend == "auto":
            backend = "open_clip" if model_id.startswith("hf-hub:") else "transformers"
        self.backend = backend
        trust = os.environ.get("VISION_EMBED_TRUST_REMOTE_CODE") == "1"
        if backend == "open_clip":
            import open_clip

            self.model, self.preprocess = open_clip.create_model_from_pretrained(model_id)
            self.tokenizer = open_clip.get_tokenizer(model_id)
            self.model.eval().to(self.device)
        else:
            from transformers import AutoModel, AutoProcessor

            kw = {"local_files_only": local_only}
            if trust:
                kw["trust_remote_code"] = True
            self.model = AutoModel.from_pretrained(model_id, **kw).eval().to(self.device)
            self.processor = AutoProcessor.from_pretrained(model_id, **kw)
        # Probe the output width once with a real forward pass rather than
        # trusting a config field whose name differs between architectures.
        self.dim = len(self.embed_texts(["probe"])[0])

    def _norm_rows(self, t):
        t = t.float()
        t = t / t.norm(dim=-1, keepdim=True).clamp_min(1e-12)
        return [row.tolist() for row in t.cpu()]

    def embed_images(self, blobs):
        from PIL import Image, UnidentifiedImageError

        res: list = [None] * len(blobs)
        imgs, idx = [], []
        for i, b in enumerate(blobs):
            try:
                im = Image.open(io.BytesIO(b))
                im.load()
                imgs.append(im.convert("RGB"))
                idx.append(i)
            except (UnidentifiedImageError, OSError, ValueError) as e:
                res[i] = ValueError("not_an_image: %s" % type(e).__name__)
        torch = self.torch
        for s in range(0, len(imgs), BATCH):
            chunk, cidx = imgs[s:s + BATCH], idx[s:s + BATCH]
            with self.lock, torch.inference_mode():
                if self.backend == "open_clip":
                    x = torch.stack([self.preprocess(im) for im in chunk]).to(self.device)
                    f = self.model.encode_image(x)
                else:
                    inp = self.processor(images=chunk, return_tensors="pt").to(self.device)
                    f = self.model.get_image_features(**inp)
            for i, v in zip(cidx, self._norm_rows(f)):
                res[i] = v
        return res

    def embed_texts(self, texts):
        torch = self.torch
        out = []
        for s in range(0, len(texts), BATCH):
            chunk = texts[s:s + BATCH]
            with self.lock, torch.inference_mode():
                if self.backend == "open_clip":
                    f = self.model.encode_text(self.tokenizer(chunk).to(self.device))
                else:
                    # SigLIP/SigLIP2 were trained on max_length padding; CLIP
                    # accepts it too, so one call shape serves both.
                    inp = self.processor(text=chunk, padding="max_length", truncation=True,
                                         max_length=64, return_tensors="pt").to(self.device)
                    f = self.model.get_text_features(**inp)
            out.extend(self._norm_rows(f))
        return out


# ── HTTP ─────────────────────────────────────────────────────────────────────

BACKEND = None


class Handler(BaseHTTPRequestHandler):
    server_version = "jo-vision-embed/1"
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):  # one line per request, stderr
        sys.stderr.write("[vision-embed] %s %s\n" % (self.address_string(), fmt % args))

    def _send(self, code: int, obj) -> None:
        body = json.dumps(obj, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _body(self):
        try:
            n = int(self.headers.get("Content-Length") or "0")
        except ValueError:
            n = -1
        if n < 0 or n > MAX_BODY:
            self._send(413, {"error": "body_too_large", "max_bytes": MAX_BODY})
            return None
        raw = self.rfile.read(n) if n else b""
        try:
            return json.loads(raw.decode("utf-8") or "null")
        except (UnicodeDecodeError, json.JSONDecodeError):
            self._send(400, {"error": "invalid_json"})
            return None

    def do_GET(self):
        b = BACKEND
        if self.path == "/health":
            self._send(200, {"status": "ok", "ready": True, "model": b.model_id, "dim": b.dim})
        elif self.path in ("/v1/models", "/models"):
            self._send(200, {"object": "list", "data": [{
                "id": b.model_id, "object": "model", "dim": b.dim,
                "modalities": ["image", "text"], "normalized": True,
                "stub": isinstance(b, StubBackend)}]})
        else:
            self._send(404, {"error": "not_found"})

    def _envelope(self, results):
        data, errors = [], []
        for i, r in enumerate(results):
            if isinstance(r, Exception) or r is None:
                errors.append({"index": i, "error": str(r) if r is not None else "zero_vector"})
            else:
                data.append({"index": i, "embedding": r})
        return {"model": BACKEND.model_id, "dim": BACKEND.dim, "data": data, "errors": errors}

    def do_POST(self):
        if self.path not in ("/embed/image", "/embed/text"):
            self._send(404, {"error": "not_found"})
            return
        req = self._body()
        if req is None:
            return
        if not isinstance(req, dict):
            self._send(400, {"error": "expected_object"})
            return
        key = "images" if self.path == "/embed/image" else "input"
        items = req.get(key)
        if isinstance(items, str) and key == "input":
            items = [items]
        if not isinstance(items, list) or not items:
            self._send(400, {"error": "expected_nonempty_list", "field": key})
            return
        if len(items) > MAX_ITEMS:
            # Refused whole, never silently cut to the first MAX_ITEMS.
            self._send(413, {"error": "too_many_items", "max_items": MAX_ITEMS, "got": len(items)})
            return
        if key == "input":
            if not all(isinstance(t, str) for t in items):
                self._send(400, {"error": "input_must_be_strings"})
                return
            try:
                res = BACKEND.embed_texts(items)
            except Exception as e:  # model failure is a 500 with a reason
                self._send(500, {"error": "embed_failed", "detail": type(e).__name__})
                return
            self._send(200, self._envelope(res))
            return
        blobs, pre = [], {}
        for i, s in enumerate(items):
            if not isinstance(s, str):
                pre[i] = ValueError("not_base64_string")
                blobs.append(b"")
                continue
            if s.startswith("data:") and "," in s:  # tolerate a data: URL
                s = s.split(",", 1)[1]
            try:
                b = base64.b64decode(s, validate=True)
            except (binascii.Error, ValueError):
                pre[i] = ValueError("bad_base64")
                blobs.append(b"")
                continue
            if len(b) > MAX_IMAGE_BYTES:
                pre[i] = ValueError("image_too_large")
                blobs.append(b"")
                continue
            blobs.append(b)
        live = [i for i in range(len(blobs)) if i not in pre]
        try:
            out = BACKEND.embed_images([blobs[i] for i in live]) if live else []
        except Exception as e:
            self._send(500, {"error": "embed_failed", "detail": type(e).__name__})
            return
        res: list = [None] * len(blobs)
        for i, e in pre.items():
            res[i] = e
        for i, v in zip(live, out):
            res[i] = v
        self._send(200, self._envelope(res))


# ── main ─────────────────────────────────────────────────────────────────────

def check_model(model_id: str) -> tuple[bool, str]:
    """Is everything needed already on THIS machine? Never downloads."""
    try:
        import torch  # noqa: F401
        import PIL  # noqa: F401
    except ImportError as e:
        return False, "missing python dependency: %s (pip install -r scripts/requirements-vision.txt)" % e.name
    if os.path.isdir(model_id):
        return True, "local model directory %s" % model_id
    if model_id.startswith("hf-hub:"):
        try:
            import open_clip  # noqa: F401
        except ImportError:
            return False, "missing python dependency: open_clip_torch"
        repo = model_id[len("hf-hub:"):]
    else:
        try:
            import transformers  # noqa: F401
        except ImportError:
            return False, "missing python dependency: transformers"
        repo = model_id
    try:
        from huggingface_hub import snapshot_download

        p = snapshot_download(repo, local_files_only=True)
        return True, "cached at %s" % p
    except Exception as e:  # not cached / hub lib missing
        return False, "model %s not on disk (%s)" % (repo, type(e).__name__)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--host", default=os.environ.get("VISION_EMBED_HOST", "127.0.0.1"))
    ap.add_argument("--port", type=int, default=env_int("VISION_EMBED_PORT", 8083, 1, 65535))
    ap.add_argument("--model", default=os.environ.get("VISION_EMBED_MODEL") or DEFAULT_MODEL)
    ap.add_argument("--backend", default=os.environ.get("VISION_EMBED_BACKEND", "auto"),
                    choices=["auto", "transformers", "open_clip"])
    ap.add_argument("--device", default=os.environ.get("VISION_EMBED_DEVICE") or None)
    ap.add_argument("--stub", action="store_true", default=os.environ.get("VISION_EMBED_STUB") == "1",
                    help="deterministic hash vectors, no ML dependencies (tests only)")
    ap.add_argument("--check", action="store_true",
                    help="exit 0 if the model and dependencies are present locally, 1 otherwise; never downloads")
    a = ap.parse_args(argv)

    global BACKEND
    if a.stub:
        if a.check:
            print("stub mode: no model needed")
            return 0
        BACKEND = StubBackend(env_int("VISION_STUB_DIM", 64, 2, 4096))
    else:
        ok, why = check_model(a.model)
        if a.check:
            print(why)
            return 0 if ok else 1
        local_only = os.environ.get("HF_HUB_OFFLINE") == "1" or ok
        try:
            BACKEND = TorchBackend(a.model, a.backend, a.device, local_only)
        except ImportError as e:
            sys.stderr.write("[vision-embed] missing dependency: %s\n" % e)
            return 1
        except Exception as e:
            sys.stderr.write("[vision-embed] could not load %s: %s: %s\n" % (a.model, type(e).__name__, e))
            return 1

    srv = ThreadingHTTPServer((a.host, a.port), Handler)
    srv.daemon_threads = True
    sys.stderr.write("[vision-embed] model=%s dim=%d listening on http://%s:%d\n"
                     % (BACKEND.model_id, BACKEND.dim, a.host, a.port))
    sys.stderr.flush()
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
