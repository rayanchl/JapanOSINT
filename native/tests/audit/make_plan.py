#!/usr/bin/env python3
"""Turn inventory.jsonl into the sweep plan: one "<id>\\t<entity>" line per run.

Scheduled sources (interval>0) run with no entity — that is exactly how the
scheduler runs them, so testing them any other way tests the wrong path.

On-demand sources are OSINT pivots and do nothing without ctx->entity, so each
gets a probe value guessed from its id/name/description. The guess is coarse on
purpose: a wrong guess shows up in the results as EMPTY with a live HTTP 200,
which is the signal an auditor needs to correct it per-source.
"""
import json, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))

# Probe values chosen to be real, stable, and safe to query repeatedly.
PROBES = {
    "domain":   "github.com",
    "ip":       "8.8.8.8",
    "email":    "test@example.com",
    "username": "torvalds",
    "person":   "Shinzo Abe",
    "company":  "Toyota",
    "hash":     "44d88612fea8a8f36de82e1278abb02f",
    "phone":    "+81312345678",
    "btc":      "1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa",
    "cve":      "CVE-2021-44228",
    "country":  "Japan",
    "url":      "https://github.com",
    "keyword":  "Tokyo",
    "doi":      "10.1038/nature12373",
    "vessel":   "9074729",
    "asn":      "AS15169",
    "coords":   "35.6762,139.6503",
}

# Ordered: first pattern that hits the id+name+description wins. More specific
# rules must come first (e.g. "reverse_dns" is ip-shaped, not domain-shaped).
RULES = [
    (r"reverse[_-]?dns|ptr\b",                     "ip"),
    (r"reverse[_-]?geocod|geocode.*reverse",       "coords"),
    (r"geocod|address[_-]?resolv|nominatim",       "keyword"),
    (r"\bcve\b|vuln|advisor|kev\b|exploit|poc[_-]", "cve"),
    (r"crypto|blockchain|wallet|onchain|bitcoin|ethereum", "btc"),
    (r"vessel|ais\b|marine|maritime|ship|imo\b",   "vessel"),
    (r"\basn\b|\bbgp\b|peering|ripe|as[_-]?lookup", "ip"),
    (r"ip[_-]|_ip\b|geoip|shodan|censys|netlas|fofa|greynoise|abuseipdb|tor[_-]?exit|port[_-]?scan",
                                                    "ip"),
    (r"hash|malware|virustotal|sample|yara",       "hash"),
    (r"phone|msisdn|telephon|numver",              "phone"),
    (r"email|breach|holehe|smtp|mx[_-]|mail",      "email"),
    (r"username|social|maigret|sherlock|profile|handle|whatsmyname",
                                                    "username"),
    (r"doi\b|crossref|scholar|arxiv|pubmed|openalex|citation|orcid|academic|paper",
                                                    "doi"),
    (r"compan|corp|registry|business|edgar|edinet|houjin|gbiz|krs|cnpj|kbo|cvr|brreg|prh|cro\b|gemi|ares|rpo|abr\b|ised|cipc|mca\b|dart",
                                                    "company"),
    (r"person|people|pep\b|sanction|watchlist|politic|wanted|missing", "person"),
    (r"subdomain|\bdns\b|whois|rdap|certificate|ct[_-]?log|crtsh|domain|ssl|tls|tech[_-]?stack|url[_-]?analy|urlscan|wayback|dnstwist",
                                                    "domain"),
    (r"country|nation|world[_-]?bank|gazette",     "country"),
]


def classify(rec):
    hay = " ".join(str(rec.get(k) or "") for k in ("id", "name", "category", "url")).lower()
    for pat, kind in RULES:
        if re.search(pat, hay):
            return kind
    return "keyword"


def main():
    rows = [json.loads(l) for l in open(os.path.join(HERE, "inventory.jsonl"), encoding="utf-8")]
    sched, ondem = [], []
    for r in rows:
        # Internal pods (_maint/_enrich/_test/_breach) are not data sources; they
        # are enrichment workers and a --run of them measures nothing.
        if (r.get("collector") or "").startswith("_"):
            continue
        if (r.get("interval") or 0) > 0:
            sched.append((r["id"], ""))
        else:
            ondem.append((r["id"], PROBES[classify(r)], classify(r)))

    with open(os.path.join(HERE, "plan_scheduled.tsv"), "w", encoding="utf-8") as fh:
        for sid, _ in sched:
            fh.write(sid + "\n")
    with open(os.path.join(HERE, "plan_ondemand.tsv"), "w", encoding="utf-8") as fh:
        for sid, ent, _ in ondem:
            fh.write("%s\t%s\n" % (sid, ent))
    with open(os.path.join(HERE, "plan_ondemand_kinds.tsv"), "w", encoding="utf-8") as fh:
        for sid, ent, kind in ondem:
            fh.write("%s\t%s\t%s\n" % (sid, kind, ent))

    import collections
    print("scheduled runs: %d   on-demand runs: %d" % (len(sched), len(ondem)), file=sys.stderr)
    print("probe kinds:", dict(collections.Counter(k for _, _, k in ondem)), file=sys.stderr)


if __name__ == "__main__":
    main()
