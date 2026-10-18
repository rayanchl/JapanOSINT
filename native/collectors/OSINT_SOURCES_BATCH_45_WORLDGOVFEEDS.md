# Batch 45 — the feeds public bodies declare on their own sites, worldwide

`collectors/pivot/table/hp3b45_{itgov,frgov,wdgov}.c` · manifests
`docs/candidate-sources-batch45.{itgov,frgov,wdgov}.txt`

## Method

Batch 43/44's method on three more public registers of official bodies, each
carrying the body's own website. The register is the seed; nothing was found by
guessing a path.

* **Italy — IPA**, the Indice delle Pubbliche Amministrazioni
  (`indicepa.gov.it/ipa-dati`, `amministrazioni.txt`, 23,764 bodies on
  2026-10-18, 22,455 with a distinct `sito_istituzionale` host). Name, type
  (`tipologia_amm`), comune and province come from the register.
* **France — DILA's Annuaire de l'administration**
  (`api-lannuaire.service-public.fr`, the `api-lannuaire-administration`
  dataset, 93,742 entries, 28,885 distinct `site_internet` hosts: mairies,
  intercommunalités, préfectures, CCAS, tribunaux and the national
  administrations). Name and category come from the register.
* **Worldwide — Wikidata**, every item in 196 sovereign states that is a direct
  instance of government agency, ministry, government organization, court,
  police, central bank, legislature, regulatory agency or public prosecutor and
  carries an official website (P856). The SPARQL endpoint times out on a
  world-wide query, so it was asked one cheap (country, class) query at a time,
  paced at one request every two seconds, with an English label where one
  exists.

Each site root was fetched ONCE, following redirects, and three things read
from the body (`discover_feeds.py`): the feeds it DECLARES in
`<link rel="alternate" type="application/rss+xml | atom+xml">`, anchors whose
target or label says `rss` / `atom` / `feed` or ends in `.xml` / `.rdf`, and
whether the site is WordPress (whose `/comments/feed/` is dropped before
validation). Every candidate URL was then fetched, classified by its root
element, its records counted at the ELEMENT START, and deduplicated against the
tree's endpoint URLs and then by `batch_exclusions.py --bin`. A feed answering
200 with zero records was rejected (`EMPTY_RESULTSET`).

## What a row is

`mode=xml`, the array path its format dictates, `id_keys=guid,link` (Atom:
`id,link.@href`), `date_keys=pubDate,dc:date` (Atom: `updated,published`),
hourly. Each description carries the item count and newest date **from the
fetch that qualified it**.

## Discovery, measured

| register | hosts | root answered 200 | declared a feed | anchor hints | WordPress | candidate URLs | feed with ≥1 record |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Italy IPA | 22,455 | 17,149 | 7,342 | 2,640 | 7,366 | | |
| France DILA | 28,885 | 24,037 | 10,886 | 1,650 | 11,145 | | |
| Wikidata, 196 countries | | | | | | | |

Of the Italian hosts 2,754 have no server at the root, 982 answer 403, 961
answer 410 Gone and 278 answer 509 (a hosting quota); of the French 2,983 have
no server, 1,167 answer 403 and 189 answer 418 (a bot wall that says so).

## Gates

(filled in from the run below)
