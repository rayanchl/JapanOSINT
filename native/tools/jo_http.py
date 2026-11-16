#!/usr/bin/env python3
"""THE request conditions every fetching tool in this directory uses.

Why one module. On 2026-11-07 ten fetchers here sent EIGHT different
User-Agents — `Mozilla/5.0 (compatible; JapanOSINT/1.0)`, `… feed discovery`,
`JapanOSINT-discovery/1.0`, three spellings of the compat agent, verify_feeds'
`… Python-urllib` — and only tools/probe_hp_batch.py read
core/host_agents.def, the engine's per-host agent table. So a host the engine
reaches under its `HA_COMPAT` or `HA_BROWSER` line answered 403 to most of the
tools that measure it, and the row read as dead to the tool and alive to the
engine; and a host that bans the word `collector` answered the tools 200 while
refusing the engine — a sweep's verdict being about the agent it sent, not the
row. CLAUDE.md records both shapes (the UA word filter, found twice on 55
hosts; `www.rakuten-bank.co.jp` judging the engine's Accept headers).

So the engine's resolution, read from the engine's own files, lives here once:

  * the agent:  core/host_agents.def's entry for the host (its agent replaces
    any User-Agent the row declares, plus the HA_BROWSER extra headers), else
    the row's own header, else JO_USER_AGENT from core/httpclient.h;
  * `Accept`:   the row's own, else `application/json` for a JSON row —
    lib/hpengine.c adds that default for `mode == HP_JSON` ONLY — else
    libcurl's `*/*`;
  * `Accept-Encoding`: core/httpclient.c sets CURLOPT_ACCEPT_ENCODING "",
    which is every encoding the linked libcurl decodes (`deflate, gzip, br,
    zstd` on the 8.5.0 build here). This module offers the subset Python can
    decode — `deflate, gzip`, plus `br`/`zstd` when their modules are
    installed — and decodes what comes back, because offering an encoding we
    cannot decode would turn a live body into garbage;
  * TLS:       tools/jo_tls.py, always (verification stays on);
  * pacing:    one request per host per gap across threads, a 429 or
    Retry-After widening that host's gap (probe_hp_batch.py's rule, which
    core/hostgate.c applies to the engine).

`curl_header_args()` gives the same headers as a curl argv for the tools that
shell out (curl's `--compressed` sends exactly libcurl's Accept-Encoding).

    python3 tools/jo_http.py --self-test
"""
import gzip
import os
import re
import sys
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import jo_tls                                                    # noqa: E402


def _read(rel):
    try:
        with open(os.path.join(NATIVE, rel), encoding="utf-8") as fh:
            return fh.read()
    except OSError:
        return ""


def engine_user_agent():
    """JO_USER_AGENT, parsed from core/httpclient.h rather than copied (the
    probe's reason: 1,623 of batch 19's rows were once proven against a
    string they never send)."""
    m = re.search(r"#define\s+JO_USER_AGENT\s*((?:\\\s*\n|.)*)", _read("core/httpclient.h"))
    if not m:
        return None
    return "".join(re.findall(r'"([^"]*)"', m.group(1))) or None


DEFAULT_UA = engine_user_agent() or \
    "JapanOSINT/1.0 (+https://github.com/RCorp/OSINTsaas; feed collector; contact via repo issues)"


def load_host_agents():
    """core/host_agents.def as {host: (agent, extra_headers)}."""
    src = re.sub(r"/\*.*?\*/", "", _read("core/host_agents.def"), flags=re.S)
    cli = _read("core/httpclient.c")
    if not src:
        return {}
    kinds = dict(re.findall(r'HOST_AGENT_KIND\(\s*(\w+)\s*,\s*"([^"]*)"\s*\)', src))
    m = re.search(r"BROWSER_EXTRA_HEADERS\[\]\s*=\s*\{(.*?)NULL", cli, re.S)
    extra = {}
    for h in re.findall(r'"([^"]+)"', m.group(1) if m else ""):
        k, _, v = h.partition(":")
        extra[k.strip()] = v.strip()
    # HA_SEC's agent may be replaced by the operator's own mailbox; the engine's
    # host_agent_ua() reads the same variable.
    sec_env = os.environ.get("SEC_CONTACT_UA")
    if sec_env and "HA_SEC" in kinds:
        kinds["HA_SEC"] = sec_env
    out = {}
    for host, kind in re.findall(r'HOST_AGENT\(\s*"([^"]+)"\s*,\s*(\w+)\s*\)', src):
        if kind in kinds:
            out[host.lower()] = (kinds[kind], extra if kind == "HA_BROWSER" else {})
    return out


HOST_AGENTS = load_host_agents()


def host_of(url):
    """The authority without port, lower-cased — host_agent_index()'s match."""
    return (urllib.parse.urlsplit(url).hostname or "").lower()


def host_agent(url):
    """(agent, extra_headers) when core/host_agents.def lists the host."""
    return HOST_AGENTS.get(host_of(url))


def _decoders():
    names = ["deflate", "gzip"]
    try:
        import brotli  # noqa: F401
        names.append("br")
    except ImportError:
        pass
    try:
        import zstandard  # noqa: F401
        names.append("zstd")
    except ImportError:
        pass
    return names


ACCEPT_ENCODING = ", ".join(_decoders())


def default_accept(mode):
    """lib/hpengine.c: `Accept: application/json` for `mode == HP_JSON` when
    the row declares no Accept; every other mode (ndjson included) gets
    libcurl's own default, `*/*`."""
    m = (mode or "").lower().replace("hp_", "")
    return "application/json" if m == "json" else "*/*"


def parse_header_lines(lines):
    """["Name: value", …] (a C row's .headers) -> [(name, value)]."""
    out = []
    for h in lines or ():
        if not isinstance(h, str):
            continue
        name, _, val = h.partition(":")
        if name.strip():
            out.append((name.strip(), val.strip()))
    return out


def engine_headers(url, row_headers=None, mode=None, content_type=None):
    """The header dict the ENGINE puts on the wire for `url`.

    `row_headers` is the row's own (a dict, or "Name: value" lines);
    `content_type` is the POST body's ("Content-Type: …") when there is one —
    a declared Content-Type wins, as in hp_run. Mirrors do_once() in
    core/httpclient.c: a listed host's agent replaces ANY User-Agent the row
    set; an unlisted host keeps the row's, else JO_USER_AGENT."""
    if isinstance(row_headers, dict):
        pairs = list(row_headers.items())
    else:
        pairs = parse_header_lines(row_headers)
    hdrs = {}
    for k, v in pairs:
        hdrs.setdefault(k, v)                    # a duplicated header is one header here
    low = {k.lower() for k in hdrs}
    if "accept" not in low:
        hdrs["Accept"] = default_accept(mode)
    if content_type and "content-type" not in low:
        n, _, v = content_type.partition(":")
        hdrs[n.strip()] = v.strip()
    ha = host_agent(url)
    if ha:
        for k in [k for k in hdrs if k.lower() == "user-agent"]:
            del hdrs[k]
        hdrs["User-Agent"] = ha[0]
        for k, v in ha[1].items():
            hdrs.setdefault(k, v)
    elif "user-agent" not in low:
        hdrs["User-Agent"] = DEFAULT_UA
    hdrs["Accept-Encoding"] = ACCEPT_ENCODING
    return hdrs


def curl_header_args(url, row_headers=None, mode=None):
    """The same headers as curl arguments (`-A agent -H …`). Accept-Encoding
    is left to the caller's `--compressed`, which sends exactly what libcurl's
    CURLOPT_ACCEPT_ENCODING "" sends."""
    args = []
    for k, v in engine_headers(url, row_headers, mode).items():
        if k.lower() == "user-agent":
            args += ["-A", v]
        elif k.lower() != "accept-encoding":
            args += ["-H", "%s: %s" % (k, v)]
    return args


def decode_content(raw, encoding):
    """A body as the server's Content-Encoding left it -> its bytes."""
    enc = (encoding or "").strip().lower()
    try:
        if enc in ("gzip", "x-gzip"):
            return gzip.decompress(raw)
        if enc == "deflate":
            try:
                return zlib.decompress(raw)
            except zlib.error:
                return zlib.decompress(raw, -zlib.MAX_WBITS)   # raw deflate
        if enc == "br":
            import brotli
            return brotli.decompress(raw)
        if enc == "zstd":
            import zstandard
            return zstandard.ZstdDecompressor().decompress(raw)
    except Exception:
        return raw
    return raw


# ── per-host pacing ──────────────────────────────────────────────────────
class Pacer:
    """One request per host per `gap_ms`, across threads; a 429 or a
    Retry-After doubles that host's gap (capped at 60 s) for the rest of the
    run. Hosts the rows do not share pay nothing."""

    def __init__(self, gap_ms=1000):
        self.gap_ms = gap_ms
        self._lock = threading.Lock()
        self._next = {}
        self._gap = {}

    def wait(self, url):
        host = urllib.parse.urlsplit(url).netloc.lower()
        if not host or self.gap_ms <= 0:
            return host
        while True:
            with self._lock:
                now = time.monotonic()
                due = self._next.get(host, 0.0)
                if now >= due:
                    self._next[host] = now + self._gap.get(host, self.gap_ms / 1000.0)
                    return host
                wait = due - now
            time.sleep(min(wait, 5.0))

    def throttled(self, host, status, retry_after):
        if not host:
            return
        try:
            ra = float(retry_after) if retry_after else 0.0
        except ValueError:
            ra = 0.0
        with self._lock:
            g = self._gap.get(host, self.gap_ms / 1000.0)
            self._gap[host] = min(max(g * 2, ra, 1.0), 60.0)
            self._next[host] = time.monotonic() + self._gap[host]
        sys.stderr.write("  [pace] %s answered %s; gap now %.1fs\n" % (host, status, self._gap[host]))


PACER = Pacer()


def _encode_url(url):
    """urllib needs an ASCII request line; a CKAN `?q=防災` must not die as a
    UnicodeEncodeError read as a dead endpoint (verify_feeds.encode_url)."""
    p = urllib.parse.urlsplit(url)
    host = p.netloc
    if any(ord(c) > 127 for c in host):
        try:
            host = host.encode("idna").decode("ascii")
        except UnicodeError:
            pass
    q = urllib.parse.quote
    return urllib.parse.urlunsplit((
        p.scheme, host, q(p.path, safe="/%:@!$&'()*+,;=~-._"),
        q(p.query, safe="/%:@!$&'()*+,;=~-._?"),
        q(p.fragment, safe="/%:@!$&'()*+,;=~-._?")))


def request(url, headers=None, timeout=25, body=None, max_bytes=64 * 1024 * 1024,
            pacer=PACER, method=None, mode=None, engine=True):
    """GET/POST `url` -> (status, content_type, body_bytes, response_headers).

    With `engine` (the default) `headers` are the ROW's and the engine's
    resolution is applied (engine_headers); with engine=False they are sent
    as given. Raises urllib.error.HTTPError for >= 400 like urlopen does — its
    body is still readable — and URLError / socket.timeout on transport."""
    host = pacer.wait(url) if pacer else ""
    hdrs = engine_headers(url, headers, mode) if engine else dict(headers or {})
    hdrs.setdefault("Accept-Encoding", ACCEPT_ENCODING)
    data = body.encode("utf-8") if isinstance(body, str) else body
    req = urllib.request.Request(_encode_url(url), headers=hdrs, data=data,
                                 method=method or ("POST" if data is not None else "GET"))
    try:
        cm = urllib.request.urlopen(req, timeout=timeout, context=jo_tls.context())
    except urllib.error.HTTPError as e:
        if pacer and (e.code == 429 or e.headers.get("Retry-After")):
            pacer.throttled(host, e.code, e.headers.get("Retry-After"))
        raise
    with cm as resp:
        raw = resp.read(max_bytes + 1)
        raw = decode_content(raw, resp.headers.get("Content-Encoding"))
        return resp.status, resp.headers.get("Content-Type", ""), raw, resp.headers


def fetch_any(url, headers=None, timeout=25, body=None, max_bytes=64 * 1024 * 1024,
              pacer=PACER, mode=None, engine=True):
    """Like request(), but never raises: -> (status, content_type, body, err).
    An HTTP error carries its status and (decoded) body; a transport failure
    is status -1 with the error text in `err`."""
    try:
        st, ct, raw, _h = request(url, headers, timeout, body, max_bytes, pacer,
                                  mode=mode, engine=engine)
        return st, ct, raw, ""
    except urllib.error.HTTPError as e:
        try:
            raw = decode_content(e.read(max_bytes + 1), e.headers.get("Content-Encoding"))
        except Exception:
            raw = b""
        return e.code, e.headers.get("Content-Type", "") if e.headers else "", raw, str(e.reason)
    except Exception as e:
        return -1, "", b"", "%s: %s" % (type(e).__name__, e)


# ── the wall vocabulary, read from lib/hpengine.c ─────────────────────────
#
# A 200 that is a bot wall, a JS challenge or a rate-limit page is not an
# unparseable body — it is a refusal with a remedy, and naming it UNPARSEABLE
# sends the reader to hunt a shape change that never happened (CLAUDE.md rule
# 1: Archive-It, the Imperva wall). The engine names three shapes in three
# functions; this reads their markers OUT OF THE C so a marker cannot be in one
# copy and missing from the other. `cf_chl_opt` is the challenge's own options
# variable; `cdn-cgi/challenge-platform` is deliberately NOT a marker, because
# Cloudflare injects it into ordinary error pages (measured 2026-11-05: 1 of
# the 6 agent-recoverable hosts carried it).
def _c_markers(fn):
    src = _read("lib/hpengine.c")
    m = re.search(r"static int %s\(const char \*body\)\s*\{(.*?)\n\}" % fn, src, re.S)
    return [s.lower() for s in re.findall(r'jo_strcasestr\(body,\s*"([^"]+)"\)', m.group(1))] if m else []


RATELIMIT_MARKERS = _c_markers("hp_body_is_ratelimit")
BOTWALL_MARKERS = _c_markers("hp_body_is_botwall")
CHALLENGE_MARKERS = _c_markers("hp_body_is_challenge")


def _has(text, markers):
    low = (text or "").lower()
    return any(mk in low for mk in markers)


def wall_kind(text, status=200):
    """'CHALLENGE', 'BOT_WALL', 'RATE_LIMITED' or '' for a body the row could
    not read, in the engine's order: on a 200 that did not parse, rate-limit
    words first, then the bot-wall vocabulary (a challenge is reported as
    CHALLENGE, the narrower claim); on a 401/403, only a CHALLENGE or a
    rate-limit page is named — the engine does not read a 4xx body for the
    wider wall words, because "captcha" and "Attention Required" are not a
    refusal's own wording on every host."""
    if not text:
        return ""
    if status in (401, 403):
        if _has(text, CHALLENGE_MARKERS):
            return "CHALLENGE"
        if _has(text, RATELIMIT_MARKERS):
            return "RATE_LIMITED"
        return ""
    if _has(text, RATELIMIT_MARKERS):
        return "RATE_LIMITED"
    if _has(text, BOTWALL_MARKERS):
        return "CHALLENGE" if _has(text, CHALLENGE_MARKERS) else "BOT_WALL"
    return ""


def self_test():
    bad = []

    def check(label, got, want):
        if got != want:
            bad.append("%-52s got=%r want=%r" % (label, got, want))

    # The engine's lists were found at all (a parse that finds nothing reads
    # as "no wall ever", which is the remedy pointing the wrong way).
    check("rate-limit markers parsed from the C", "too many requests" in RATELIMIT_MARKERS, True)
    check("challenge markers parsed from the C", "cf_chl_opt" in CHALLENGE_MARKERS, True)
    check("challenge-platform is NOT a marker",
          any("challenge-platform" in m for m in BOTWALL_MARKERS + CHALLENGE_MARKERS), False)
    check("a cf challenge page", wall_kind("<script>window._cf_chl_opt={}</script>"), "CHALLENGE")
    check("Imperva wall", wall_kind("<title>Pardon Our Interruption</title>"), "BOT_WALL")
    check("Archive-It rate limit", wall_kind("<title>Rate limit reached</title>"), "RATE_LIMITED")
    check("ordinary CF 403 with challenge-platform is not a wall",
          wall_kind('<html>403 Forbidden<script src="/cdn-cgi/challenge-platform/x.js">', 403), "")
    check("a 403 challenge", wall_kind("var _cf_chl_opt = 1", 403), "CHALLENGE")
    check("a 403 carrying 'captcha' is not named (engine reads challenge/ratelimit only)",
          wall_kind("solve the captcha", 403), "")
    check("'limit=' in a body is not a rate limit", wall_kind('{"limit":100}'), "")
    # Accept / Accept-Encoding mirror the engine.
    check("json row Accept", engine_headers("https://x.example/", None, "json")["Accept"],
          "application/json")
    check("csv row Accept is libcurl's */*", engine_headers("https://x.example/", None, "csv")["Accept"], "*/*")
    check("ndjson is not HP_JSON", engine_headers("https://x.example/", None, "ndjson")["Accept"], "*/*")
    check("a row's own Accept wins",
          engine_headers("https://x.example/", ["Accept: text/csv"], "json")["Accept"], "text/csv")
    check("gzip offered", "gzip" in ACCEPT_ENCODING and "deflate" in ACCEPT_ENCODING, True)
    check("unlisted host: engine agent", engine_headers("https://x.example/")["User-Agent"], DEFAULT_UA)
    check("unlisted host keeps the row's agent",
          engine_headers("https://x.example/", {"User-Agent": "Row/1"})["User-Agent"], "Row/1")
    if HOST_AGENTS:
        h, (agent, extra) = sorted(HOST_AGENTS.items())[0]
        got = engine_headers("https://%s:8443/p" % h, {"user-agent": "Row/1"})
        check("listed host: table agent replaces the row's", got.get("User-Agent"), agent)
        check("listed host: row agent dropped", "user-agent" in got, False)
        check("lookalike host does not inherit", host_agent("https://%s.evil.example/" % h), None)
    else:
        bad.append("core/host_agents.def parsed to nothing")
    check("deflate (zlib) decoded", decode_content(zlib.compress(b"abc"), "deflate"), b"abc")
    co = zlib.compressobj(wbits=-zlib.MAX_WBITS)
    check("raw deflate decoded", decode_content(co.compress(b"abc") + co.flush(), "deflate"), b"abc")
    check("gzip decoded", decode_content(gzip.compress(b"abc"), "gzip"), b"abc")
    ca = curl_header_args("https://x.example/", None, "json")
    check("curl args carry the agent", ca[ca.index("-A") + 1] if "-A" in ca else None, DEFAULT_UA)
    for b in bad:
        print("jo_http SELF-TEST FAIL  " + b)
    return not bad


if __name__ == "__main__":
    if "--self-test" in sys.argv[1:]:
        ok = self_test()
        print("jo_http fixtures: %s" % ("all pass" if ok else "FAILED"))
        sys.exit(0 if ok else 2)
    print("User-Agent      %s\nAccept-Encoding %s\nhost agents     %d\n"
          "wall markers    ratelimit=%s\n                botwall=%s\n                challenge=%s"
          % (DEFAULT_UA, ACCEPT_ENCODING, len(HOST_AGENTS), RATELIMIT_MARKERS,
             BOTWALL_MARKERS, CHALLENGE_MARKERS))
