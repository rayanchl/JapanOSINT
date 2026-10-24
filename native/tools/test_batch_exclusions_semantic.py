#!/usr/bin/env python3
"""Offline check of `batch_exclusions.py --semantic` / `--coverage`.

    python3 tools/test_batch_exclusions_semantic.py [--bin ./bin/japanosint]

Runs against tools/embed_stub_server.py (hashed character trigrams — no
model, no network beyond loopback). What it pins:

  1. Without JO_EMBED_URL the semantic check says SKIPPED, never "0 found".
  2. A candidate that restates a registered source's name and description is
     flagged SEMANTIC-DUP against THAT source; an unrelated row is not.
  3. The card vectors are cached: a second run embeds no registry card.
  4. --coverage prints the category x country table and the candidates' cells.

The stub's similarity is lexical, not semantic, so this proves the plumbing
(same card text as the C index, cosine, threshold, cache), not the quality of
any real model's judgement.
"""
import argparse
import os
import socket
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(HERE)


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def run(args, env):
    r = subprocess.run([sys.executable, os.path.join(HERE, "batch_exclusions.py")]
                       + args, capture_output=True, text=True, env=env, cwd=NATIVE)
    return r.returncode, r.stdout, r.stderr


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=os.path.join(NATIVE, "bin", "japanosint"))
    a = ap.parse_args()

    dump = subprocess.run([a.bin, "--dump-source-cards"], capture_output=True,
                          text=True, check=True).stdout.splitlines()
    # A scheduled source with a long description, no alias line: its card is
    # "ID: Name. description", which the candidate restates.
    pick = None
    for line in dump:
        sid, kind, cat, text = line.split("\t", 3)
        if kind == "scheduled" and "\\n" not in text and 300 < len(text) < 600:
            pick = (sid, cat, text)
            break
    assert pick, "no suitable source card"
    sid, cat, text = pick
    name, _, desc = text[len(sid) + 2:].partition(". ")

    tmp = tempfile.mkdtemp(prefix="semdup-")
    man = os.path.join(tmp, "candidate-sources-batch999.test.txt")
    cols = "id|mode|want|category|record_type|tags|portal|name|name_ja|url|probe|description|opts"
    rows = [
        ["TEST_SEMDUP_A", "json", "any", cat, "r", "t", "https://a.invalid/",
         name, "", "https://a.invalid/v1/items", "", desc, ""],
        ["TEST_SEMDUP_B", "json", "any", "wildlife", "r", "t", "https://b.invalid/",
         "Zebra herd GPS collars", "", "https://b.invalid/zz/collars", "",
         "Hourly GPS fixes from collared plains zebra herds in the Okavango "
         "delta, with herd identifier and battery voltage.", ""],
    ]
    with open(man, "w") as f:
        f.write("# " + cols + "\n")
        for r in rows:
            f.write("|".join(r) + "\n")

    env = dict(os.environ)
    env.pop("JO_EMBED_URL", None)
    env.pop("JO_EMBED_MODEL", None)
    rc, out, err = run(["--bin", a.bin, "--check", man, "--semantic"], env)
    assert "SEMANTIC: SKIPPED" in out, out + err
    print("  without JO_EMBED_URL: SKIPPED, stated: ok")

    port = free_port()
    stub = subprocess.Popen([sys.executable, os.path.join(HERE, "embed_stub_server.py"),
                             str(port), "256"], stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL)
    try:
        for _ in range(100):
            try:
                socket.create_connection(("127.0.0.1", port), timeout=0.2).close()
                break
            except OSError:
                time.sleep(0.1)
        env["JO_EMBED_URL"] = "http://127.0.0.1:%d" % port
        cache = os.path.join(tmp, "cards.bin")
        args = ["--bin", a.bin, "--check", man, "--semantic",
                "--semantic-cache", cache]
        rc, out, err = run(args, env)
        assert ("SEMANTIC-DUP TEST_SEMDUP_A ~ %s " % sid) in out, out + err
        assert "TEST_SEMDUP_B ~" not in out, out
        assert "embedding" in err and "cards" in err, err
        print("  restated %s flagged SEMANTIC-DUP, unrelated row not: ok" % sid)
        rc, out2, err2 = run(args, env)
        assert ("SEMANTIC-DUP TEST_SEMDUP_A ~ %s " % sid) in out2, out2
        assert " cards (" not in err2, err2           # nothing re-embedded
        print("  second run embeds no registry card (cache): ok")
        rc, out3, _ = run(["--bin", a.bin, "--check", man, "--coverage"], env)
        assert "category x country cells" in out3 and "candidate rows by cell" in out3, out3
        print("  coverage table and candidate cells printed: ok")
    finally:
        stub.kill()
        stub.wait()
    print("test_batch_exclusions_semantic: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
