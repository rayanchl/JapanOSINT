# batch 47 — the feeds public bodies declare on their own sites, worldwide

`collectors/pivot/table/hp3b47_{itgov,frgov,wdgov}.c` · manifests
`docs/candidate-sources-batch47.{itgov,frgov,wdgov}.txt`

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
| Italy IPA | 22,455 | 17,149 | 7,342 | 2,640 | 7,366 | 11,054 | 6,321 |
| France DILA | 28,885 | 24,037 | 10,886 | 1,650 | 11,145 | 15,168 | 11,056 |
| Wikidata, 196 countries | 11,245 (13,939 before dropping the `association` class) | 6,748 | 1,559 | 1,164 | 1,789 | 3,690 | 1,763 |

Of the Italian hosts 2,754 have no server at the root, 982 answer 403, 961
answer 410 Gone and 278 answer 509 (a hosting quota); of the French 2,983 have
no server, 1,167 answer 403 and 189 answer 418 (a bot wall that says so); of
the Wikidata hosts 2,965 have no server and 1,016 answer 403.

## Gates (2026-10-18, all live)

**16,520 rows ship — 5,640 Italian, 9,319 French, 1,561 from 157 countries via
Wikidata — every one measured emitted-AND-stored in the real binary** (16,550
after the retry pass of 2026-10-19: 5,665 / 9,323 / 1,562, 277,706 emitted /
277,362 stored).
277,289 records emitted, 276,945 stored. `docs/verified-sources-batch47.tsv`
carries the probe reading and the emit reading per row; the 15,761 candidates
that did not ship are in `docs/rejected-sources-batch47.tsv` by beat and stage.

| stage | IT in → out | FR in → out | WD in → out |
| --- | --- | --- | --- |
| builder: fetch, classify, count at the element start | 11,054 → 6,321 | 15,168 → 11,056 | 3,690 → 1,763 |
| `batch_exclusions.py --bin` | → 6,309 (12 dup) | → 11,021 (35 dup) | → 1,592 (171 dup: the US, UK and Japanese bodies batches 43/44 already read) |
| reachability / pagination | 0 / 0 | 0 / 216 SPIP `?page=backend` rows, which name the RSS template and carry `pagination_ok` | 0 / 0 |
| `probe_hp_batch.py` | 5,491 PASS | 9,141 PASS | 1,529 PASS |
| generated with EVERY row; `audit_registry_emit.py` + one re-run of each FETCH_FAILED | → 5,640 (669 failed twice) | → 9,319 (1,699 failed twice, 3 serve HTML to the engine's client) | → 1,561 (31 failed twice) |

**392 rows the probe refused ship on the engine's measurement** (177 IT, 183 FR,
32 WD) — the probe's "Bad Gateway" and 403 readings against shared municipal
hosting at twelve concurrent requests were not what the engine saw at six — and
the verified TSV carries both readings for each. The 40 collisions were each
measured against the feed itself: every verifiable one is the feed's own
byte-identical duplicate (one Russian agency feed lists every item twice;
Italy's SVTRL repeats one item in each of its three feeds), and 20 French
ones sat behind one ministry family's maintenance page at the time of
checking and were recorded `UNVERIFIABLE_NOW` with their numbers rather than
guessed at. **Re-measured 2026-10-19 with the hosts back: 33 of the 35 French
collisions are the feed's own byte-identical duplicates, the loss equal to the
surplus in every case** — a SPIP `backend` feed lists one article under every
rubrique it belongs to, so 521 items hold 474 distinct articles and 47 copies —
and the two still answering 503 lose 5 and 10 on the same shape. No key is
wrong; nothing is lost that was not a copy.

**394 of the rows dropped as 403s were a wall against the User-Agent, not
against the reader** (2026-10-20; 16,944 rows now: 5,723 IT, 9,651 FR, 1,570
WD). The 414 rows of this batch that had answered 403 to the engine's client
were re-probed with `header1=User-Agent: Mozilla/5.0 (compatible\;
JapanOSINT/1.0\; +https://github.com/rayanchl/JapanOSINT)` — the same
identified client with the browser-compatible prefix the walls key on — and
328 French, 58 Italian and 8 Wikidata rows answered with their feed AND store
it in the engine under that header (the four Italian school sites that refused
twice stay out). They are back in their beats with the header on the row, so
the condition each was proven under is the one it runs under, and
`verified-sources-batch47.tsv` marks their probe reading `PASS (User-Agent
header)`.

**Three defects found by the gates, none visible to a probe:**

* **An HTML-escaped `&amp;` in a feed URL changes what the server serves.**
  534 French rows carried `&amp;` from the page's own markup. Fixing it after
  validation meant Joomla finally RECEIVED `type=atom` and answered Atom where
  the row declared `channel.item`: 41 rows emitted nothing. Every row whose URL
  changed after validation was re-fetched, re-classified from the body the
  server serves now, and re-measured (505 OK). **Any change to a URL after
  validation invalidates the validation.**
* **A feed title can span lines and carry entities.** One title with embedded
  newlines split a manifest row across five lines; thousands carried
  `&amp;` / `&#039;` / numeric Cyrillic entities into `.name`. The builder
  normalises whitespace and unescapes now, and batches 43–45 were regenerated
  with the names fixed (no C-side repairs existed to lose).
* **The registry cap.** This batch took the tree to 43,453 and `MAX_SOURCES`
  was 32,768: the build was clean and 2,392 French rows did not exist at
  runtime. Found by comparing the beat's registration count against its table
  before believing the audit; `make registry-floor` fails on any OVERFLOW line.
  Raised to 131,072 (`registry.c`).

Tree gates after all three tables landed: build 0 warnings; `lint-sources` OK;
`audit-sources` 0 findings across 329 strict-set files; `hptest`,
`pagewalktest`, `htmlparsetest`, `authtest`, `unit`, `selftest` all pass;
`source-floor` and `registry-floor` re-recorded at 42,643.

## The split by body type (2026-10-24)

The two national tables were one collector each — `hp3b47_frgov` 9,619 rows,
`hp3b47_itgov` 5,716 — every row `category = "government"`, `record_type =
"government-notice"`, under one heading on the Collectors pages and the iOS
dashboard. The registers behind them are not one body type: the French
directory lists 6,450 mairies beside 1,163 CCAS and care services, 612 youth
and employment offices, 169 police and fire services, 130 professional orders;
Italy's IPA lists 2,725 state schools and 821 professional orders filed as
"public administration". Half the registry read as Government because of these
two tables.

Every row now names its collector explicitly (`.collector`, the field hpengine
rows otherwise default to their table's name) and carries the category of its
body type — `local-gov`, `education`, `legal`, `health`, `research`,
`environment`, `culture`, `commerce`, `utility`, `public-safety` or
`government`. The key is derived from the body's NAME by ordered regex rules
(first match wins) in `tools/split_beats_by_body_type.py`: France by the type
the service-public.fr directory puts first in the name ("Mairie - X",
"Communauté de communes - Y", "Ordre des avocats - Z"), Italy by the name plus
the IPA category in parentheses ("Gestori di Pubblici Servizi"). A row no rule
types goes to the beat's honest residue (`itgov_altri`, 181 rows: agencies,
consortia, commissioner structures); France's one unmatched row was an
association and joins the social-action collector. The URL, id and identity of
every row are untouched, so no emit measurement is invalidated, and the
manifests mirror the same two facts (field 4 and a `collector=` opt).

| collector | rows | group |
| --- | --- | --- |
| `frgov_mairies` | 6,450 | government |
| `frgov_social` (CCAS/CIAS, MDPH, elder-care and family points) | 1,163 | government |
| `frgov_services_citoyens` (missions locales, Info Jeunes, France services, GRETA) | 612 | government |
| `frgov_intercommunal` (EPCI) | 410 | government |
| `frgov_environnement` (hunting/fishing federations, nature parks, maritime) | 185 | environment |
| `frgov_securite` (police municipale, SDIS, access to justice) | 169 | safety |
| `frgov_sante` (hospitals, SAMU, occupational health) | 162 | health |
| `frgov_ordres` (orders and chambers) | 130 | legal |
| `frgov_enseignement` (universities, écoles, research) | 125 | science |
| `frgov_etat` (prefectures, directions, councils, agencies) | 111 | government |
| `frgov_culture` (Instituts français, museums, archives) | 102 | culture |
| `itgov_scuole` | 2,725 | culture (education) |
| `itgov_ordini` | 821 | legal |
| `itgov_comuni` | 675 | government |
| `itgov_servizi_pubblici` (utilities, transport, ports) | 463 | infrastructure |
| `itgov_sanita` (ASL/ASP, hospitals, IPAB care) | 283 | health |
| `itgov_territoriali` (unioni, province, regioni, GAL) | 209 | government |
| `itgov_altri` (residue) | 181 | government |
| `itgov_universita` | 150 | science |
| `itgov_ambiente` (parks, bonifica, hunting districts) | 113 | environment |
| `itgov_cultura` (theatres, sport federations, tourism) | 72 | culture |
| `itgov_camere` | 24 | corporate |

The rules are a reading of these two registers on 2026-10-24, spot-checked
by sampling three names per collector. A future re-probe that regenerates a
table re-runs `plan` and `apply`; a new body type that no rule names lands in
the residue, visibly, where it can be read and given a rule.
