# Batch 67 — portal-host discovery (DCAT + dataportals.org) and the full Socrata scroll

6,501 sources (registry 98,728 → 105,229), measured through the engine 2026-11-14.

## Host discovery
Candidate portal origins came from two directories: the homepage of every
data.europa.eu catalogue (214 catalogues, 107 with a homepage) and the
dataportals.org list (okfn/dataportals.org `portals.csv`) — 799 distinct
origins. Each was probed for a CKAN (`package_search`), OpenDataSoft
(`explore/v2.1`) or Socrata (`api/catalog/v1`) API: 152 answer one of them and
**27 were not in the tree** (20 CKAN, 5 Socrata, 2 ODS). Most listed portals
were already registered, which is the finding: the directories add few hosts.

* `hp3b67_ckanhost` — 19 new CKAN portals as dataset-catalogue rows
  (`package_search`, `sort=id asc`, `page_resume`): open.nrw, dati.emilia-romagna.it,
  kozadatportal.hu, datosabiertos.navarra.es, datacatalogue.gov.ie, opendatani …
* `hp3b67_ckands` — 1,249 datastore resources on those portals.
* `hp3b67_ods` — 377 datasets on the two new ODS portals (Brisbane, Brussels).

## Socrata, complete
`hp3b67_socrata` — 4,856. The federated catalogue was scrolled to its end with
`scroll_id` (batch 66 read only its first 10,000): 43,369 datasets on 285
domains, 39,709 of them unregistered. 5,000 were taken most-viewed first,
round-robin across 238 domains; the remaining ~35,000 are a measured pool, not
registered.

## Kept and dropped
6,711 candidates: 6,125 OK, 17 COLLISION, 1 SLOW kept; fetch failures kept only
where the cause is transient or this container's own path (connection reset,
no certificate judged, HTTP 502/503 under the sweep's own burst). Dropped 210:
54 EMITS_NOTHING and 156 HTTP 404/400/unparseable — `docs/rejected-sources-batch67.tsv`.
