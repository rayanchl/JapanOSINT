#!/usr/bin/env python3
"""jo_audit.py — one entry point for the source-audit tools (audit OPS-13).

    python3 tools/jo_audit.py --help            # every subcommand, one line each
    python3 tools/jo_audit.py emit registry --help
    python3 tools/jo_audit.py paging ceiling --probe-pages 3 ...
    python3 tools/jo_audit.py --self-test       # every subcommand's --help must exit 0

This is a DISPATCHER, deliberately. The tools keep their own behaviour and
remain runnable directly (`python3 tools/audit_registry_emit.py ...`); every
argument after the subcommand is passed through untouched, in a child process
with this interpreter. Nothing was moved into a shared module: the scratch-DB
and pinned-binary code in audit_registry_emit / audit_batch_emit / audit_cycle_*
looks alike but differs in details (slot pools, template reuse, per-host
serialisation), and rule 4c of CLAUDE.md is the warning about merging two
readers of one thing without proving they behave the same. The one-line
summary of each subcommand is read from the target script's own docstring, so
it cannot drift from the tool.
"""
import ast
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

# subcommand path -> (script, has_argparse_help)
# has_argparse_help False: the script takes positional args only and treats
# "--help" as a path, so the dispatcher answers --help itself from its docstring.
COMMANDS = {
    ("emit", "registry"):          ("audit_registry_emit.py", True),
    ("emit", "batch"):             ("audit_batch_emit.py", True),
    ("paging", "ceiling"):         ("audit_page_ceiling.py", True),
    ("paging", "cycle-health"):    ("audit_cycle_health.py", True),
    ("paging", "cycle-advance"):   ("audit_cycle_advance.py", True),
    ("paging", "page-two"):        ("audit_page_two.py", True),
    ("paging", "page-param"):      ("audit_page_param.py", False),
    ("idkeys",):                   ("audit_idkeys.py", True),
    ("url-limits",):               ("audit_url_limits.py", True),
    ("reachable",):                ("audit_batch_reachable.py", True),
    ("probe",):                    ("probe_hp_batch.py", True),
}


def docstring(script):
    try:
        with open(os.path.join(HERE, script), encoding="utf-8") as f:
            return ast.get_docstring(ast.parse(f.read())) or ""
    except (OSError, SyntaxError):
        return ""


def one_line(script):
    for line in docstring(script).splitlines():
        line = line.strip()
        if line:
            return line
    return "(no description)"


def usage():
    out = ["usage: jo_audit.py <group> [<tool>] [tool arguments...]  |  --self-test  |  --help", "",
           "Dispatches to the existing tools in native/tools/ (each still runs directly).", ""]
    for key, (script, _) in COMMANDS.items():
        out.append("  %-22s %s" % (" ".join(key), one_line(script)))
    out += ["", "Use `jo_audit.py <subcommand> --help` for that tool's own options."]
    return "\n".join(out)


def resolve(argv):
    """Longest-prefix match of argv against COMMANDS -> (key, rest) or None."""
    for n in (2, 1):
        key = tuple(argv[:n])
        if key in COMMANDS:
            return key, argv[n:]
    return None


def run(key, rest):
    script, argparse_help = COMMANDS[key]
    path = os.path.join(HERE, script)
    if not os.path.exists(path):
        print("jo_audit: missing tool %s" % script, file=sys.stderr)
        return 2
    if rest and rest[0] in ("-h", "--help") and not argparse_help:
        print(docstring(script) or one_line(script))
        return 0
    return subprocess.call([sys.executable, path] + rest)


def self_test():
    bad = 0
    for key, (script, _) in COMMANDS.items():
        name = " ".join(key)
        if not os.path.exists(os.path.join(HERE, script)):
            print("FAIL  %-22s missing %s" % (name, script)); bad += 1; continue
        r = subprocess.run([sys.executable, os.path.abspath(__file__)] + list(key) + ["--help"],
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=120)
        ok = r.returncode == 0 and len(r.stdout) > 0
        print("%s  %-22s --help exit %d" % ("  ok" if ok else "FAIL", name, r.returncode))
        if not ok:
            bad += 1
    r = subprocess.run([sys.executable, os.path.abspath(__file__), "--help"], stdout=subprocess.PIPE)
    listed = r.stdout.decode()
    for key in COMMANDS:
        if " ".join(key) not in listed:
            print("FAIL  top-level --help does not list %s" % " ".join(key)); bad += 1
    print("jo_audit self-test: %s" % ("FAILED (%d)" % bad if bad else "ok (%d subcommands)" % len(COMMANDS)))
    return 1 if bad else 0


def main(argv):
    if not argv or argv[0] in ("-h", "--help"):
        print(usage())
        return 0
    if argv[0] == "--self-test":
        return self_test()
    hit = resolve(argv)
    if not hit:
        print("jo_audit: unknown subcommand %r\n\n%s" % (" ".join(argv[:2]), usage()), file=sys.stderr)
        return 2
    return run(*hit)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
