#!/usr/bin/env python3
"""Offline self-test for the consolidated audit tools (audit OPS-13).

    python3 tools/test_audit_tools.py        # also: make tools-selftest

No network, no real binary: a shell script stands in for `japanosint` and
answers `--list-sources` and `--run <id>`. Each shared entry point is driven
through a tool that uses it, so a regression in the shared code fails here
rather than turning every audit into a silent "0 findings":

  jo_registry.warm_template / list_sources   <- audit_batch_emit, audit_registry_emit
  manifest.iter_rows / iter_lines            <- audit_batch_pagination, audit_batch_reachable,
                                                audit_batch_emit
"""
import os
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import manifest  # noqa: E402
import jo_registry  # noqa: E402

ROW = "|".join(["%(id)s", "json", "x", "cat", "rt", "tag", "portal", "Name", "",
                "%(url)s", "%(probe)s", "desc", "%(opts)s"])

FAKE = r"""#!/bin/sh
# stand-in for bin/japanosint
case "$1" in
  --list-sources)
    python3 -c "import os,sqlite3; c=sqlite3.connect(os.environ['JO_DB']); c.execute('create table if not exists t(x)'); c.commit()"
    echo 'GOOD_ROW collector=HP_JSON interval=3600'
    echo 'DROP_ROW collector=HP_JSON interval=3600'
    ;;
  --run)
    case "$2" in
      GOOD_ROW) echo '[hp:GOOD_ROW] emitted 5 of 5 available' >&2 ;;
      DROP_ROW) echo '[hp:DROP_ROW] emitted 0 of 9 available' >&2 ;;
      *) echo 'unknown source' >&2 ;;
    esac
    ;;
esac
exit 0
"""


class T(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.d = tempfile.mkdtemp(prefix="jo_audit_tools_test_")
        cls.bin = os.path.join(cls.d, "fakebin")
        with open(cls.bin, "w") as f:
            f.write(FAKE)
        os.chmod(cls.bin, 0o755)
        cls.man = os.path.join(cls.d, "candidate-sources-batch0.test.txt")
        lines = [
            "# comment",
            ROW % dict(id="GOOD_ROW", url="https://example.invalid/a?page=1",
                       probe="https://example.invalid/a?page=1",
                       opts="page_param=page;interval=3600"),
            ROW % dict(id="DROP_ROW", url="https://example.invalid/b",
                       probe="https://example.invalid/b", opts="interval=3600"),
            "only|three|fields",
        ]
        with open(cls.man, "w", newline="") as f:
            f.write("\r\n".join(lines) + "\r\n")        # CRLF on purpose

    def sh(self, *argv, rc=None):
        p = subprocess.run([sys.executable] + list(argv), capture_output=True,
                           text=True, timeout=300)
        if rc is not None:
            self.assertEqual(p.returncode, rc, p.stdout + p.stderr)
        return p.stdout + p.stderr

    # -- manifest.py -------------------------------------------------------
    def test_manifest_iter_rows(self):
        rows, bad = manifest.iter_rows([self.man])
        self.assertEqual([r["id"] for r in rows], ["GOOD_ROW", "DROP_ROW"])
        self.assertEqual(len(bad), 1)
        self.assertTrue(bad[0][0].endswith(":4"), bad)
        self.assertFalse(any("\r" in r["opts"] for r in rows), "CRLF leaked")
        self.assertEqual(rows[0]["_at"], "candidate-sources-batch0.test.txt:2")

    # -- jo_registry.py ----------------------------------------------------
    def test_registry_selftest_and_helpers(self):
        self.sh(os.path.join(HERE, "jo_registry.py"), "--self-test", rc=0)
        db = os.path.join(self.d, "w.db")
        self.assertEqual(jo_registry.warm_template(self.bin, db), db)
        self.assertEqual([s[0] for s in jo_registry.list_sources(self.bin, db)],
                         ["GOOD_ROW", "DROP_ROW"])

    # -- audit_batch_emit (jo_registry + manifest.iter_lines) ---------------
    def test_batch_emit_verdicts(self):
        out = os.path.join(self.d, "emit.tsv")
        self.sh(os.path.join(HERE, "audit_batch_emit.py"), self.man, "--bin",
                self.bin, "--jobs", "2", "--out", out, "--workdir", self.d)
        tsv = dict((l.split("\t")[0], l.split("\t")[1])
                   for l in open(out).read().splitlines()[1:])
        self.assertEqual(tsv, {"GOOD_ROW": "OK", "DROP_ROW": "DROPS_EVERYTHING"})

    # -- audit_registry_emit (jo_registry) ---------------------------------
    def test_registry_emit_runs(self):
        out = os.path.join(self.d, "reg.tsv")
        self.sh(os.path.join(HERE, "audit_registry_emit.py"), "--bin", self.bin,
                "--all", "--jobs", "2", "--no-pin", "--quiet", "--out", out,
                "--workdir", self.d)
        ids = [l.split("\t")[0] for l in open(out).read().splitlines()[1:]]
        self.assertEqual(sorted(ids), ["DROP_ROW", "GOOD_ROW"])

    # -- the two batch auditors on the shared reader -----------------------
    def test_batch_reachable_and_pagination_read_malformed(self):
        o = self.sh(os.path.join(HERE, "audit_batch_reachable.py"), self.man)
        self.assertIn("MALFORMED", o.upper(), o)
        o = self.sh(os.path.join(HERE, "audit_batch_pagination.py"), self.man)
        self.assertIn("MALFORMED", o.upper(), o)

    # -- the dispatcher ----------------------------------------------------
    def test_jo_audit_dispatch(self):
        self.sh(os.path.join(HERE, "jo_audit.py"), "--self-test", rc=0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
