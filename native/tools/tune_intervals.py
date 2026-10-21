#!/usr/bin/env python3
"""Set each feed row's `interval` from the age of its newest item at validation
— the `newest <date>` its description records — and mark it adaptive.

    python3 tools/tune_intervals.py [--now 2026-10-18] [--no-adaptive] MANIFEST...

The rule (the same one core/scheduler.c's sched_freshness_bucket re-applies
after every run of an adaptive row):

    newest item <= 3 days old   -> 3600   (hourly)
    <= 30 days                  -> 21600  (6-hourly)
    <= 1 year                   -> 86400  (daily)
    older, or no date field     -> 604800 (weekly)

Applied to batches 43–48 on 2026-10-19: 15,620 public-body feeds stopped
polling hourly, the fleet's load went from ~7.2 to ~3.3 requests/s, and no
row lost freshness it had (the press and IGO beats stay hourly by their own
measurement). `adaptive_interval=1` is added so the scheduler keeps the
reading honest; --no-adaptive leaves that to the author."""
import sys, re, datetime, email.utils, collections, argparse


def parse(s):
    s = s.strip()
    if not s or s.startswith("(no date"):
        return None
    try:
        d = email.utils.parsedate_to_datetime(s)
        if d:
            return d if d.tzinfo else d.replace(tzinfo=datetime.timezone.utc)
    except Exception:
        pass
    for fmt in ("%Y-%m-%dT%H:%M:%S%z", "%Y-%m-%dT%H:%M:%S.%f%z", "%Y-%m-%d %H:%M:%S%z",
                "%Y-%m-%dT%H:%M:%S", "%Y-%m-%d %H:%M:%S", "%Y-%m-%d", "%d/%m/%Y", "%Y/%m/%d"):
        try:
            d = datetime.datetime.strptime(s.replace("Z", "+0000"), fmt)
            return d if d.tzinfo else d.replace(tzinfo=datetime.timezone.utc)
        except Exception:
            pass
    m = re.search(r"(\d{4})-(\d{2})-(\d{2})", s)
    if m:
        try:
            return datetime.datetime(int(m[1]), int(m[2]), int(m[3]), tzinfo=datetime.timezone.utc)
        except ValueError:
            return None   # 0000-00-00 is a date field with nothing in it
    return None


def interval_for(age_days):
    if age_days is None: return 604800, "undated"
    if age_days <= 3: return 3600, "<=3d"
    if age_days <= 30: return 21600, "<=30d"
    if age_days <= 365: return 86400, "<=1y"
    return 604800, ">1y"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--now", help="YYYY-MM-DD the ages are measured against (default: today, UTC)")
    ap.add_argument("--no-adaptive", action="store_true", help="do not add adaptive_interval=1")
    ap.add_argument("manifests", nargs="+")
    a = ap.parse_args()
    now = (datetime.datetime.strptime(a.now, "%Y-%m-%d").replace(tzinfo=datetime.timezone.utc)
           if a.now else datetime.datetime.now(datetime.timezone.utc))
    for p in a.manifests:
        out = []; c = collections.Counter(); unparsed = 0
        for line in open(p, encoding="utf-8").read().splitlines():
            if line.startswith("#") or not line.strip():
                out.append(line); continue
            f = line.split("|")
            m = re.search(r"newest (.*?)\. ", f[11] + " ")
            d = parse(m.group(1)) if m else None
            if m and not d and not m.group(1).startswith("(no date"):
                unparsed += 1
            age = (now - d).total_seconds() / 86400 if d else None
            if age is not None and age < -2:
                age = 0   # a clock ahead of ours is "now"
            iv, bucket = interval_for(age)
            f[12] = re.sub(r"(^|;)interval=\d+", r"\1interval=%d" % iv, f[12])
            if not a.no_adaptive and not re.search(r"(^|;)adaptive_interval=", f[12]):
                f[12] += ";adaptive_interval=1"
            c[bucket] += 1; out.append("|".join(f))
        open(p, "w", encoding="utf-8").write("\n".join(out) + "\n")
        print(p.rsplit("/", 1)[-1], dict(c), "unparsed", unparsed)


if __name__ == "__main__":
    main()
