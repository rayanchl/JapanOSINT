# Batch 63 (cn) — Chinese government homepages, shape-inferred scaffolds

2,870 `.gov.cn` government homepages from Wikidata P856 that are UNREACHABLE
from this authoring container (egress resets them) and that batches 59/61 could
not measure. Registered at the repository owner's explicit instruction to keep
all sources even when they cannot be verified here.

## Why they could not be measured, and the "find a way" attempts
Every route that could have revealed each site's CMS link shape without a live
fetch is also egress-blocked from this container, each tested and recorded:
- Wayback *availability* API (archive.org) works and confirms every host has a
  live snapshot — but Wayback page content (`/web/<ts>/` and `id_/`) returns
  HTTP 403, the CDX index returns "Blocked by egress policy", Common Crawl's
  index returns empty, and mementoweb 502s.
- Search-engine `site:` scraping (Bing/DDG) returns only bare hostnames, not
  the deep article paths needed to detect a pattern.

So the exact per-site pattern is unknowable here. Each row is given
`href_must=/t20`, the dominant Chinese-government CMS article pattern measured
across the ~467 reachable sites of batches 59/61 (~70% of those that had any
recognised pattern: `/t20\d{6}_\d+.html`). Sample of 40 run through the engine:
35 FETCH_FAILED (egress), 2 SLOW, 3 reachable-but-empty-under-this-pattern.

## What these rows are, honestly
A scaffold, not measured data. In a normally-connected deployment each row
stores dated notices where the site uses the `/t20` CMS and files an honest
empty/notice otherwise, to be re-pointed. The shape-learner is
`tools/`-style `wayback_shape.py` in the session scratch; from a connected
environment, fetch each homepage, detect its real pattern, and rewrite
`href_must`. Tagged `shape-inferred` so they are easy to find and refine.
They are deliberately NOT a claim of verified emission.
