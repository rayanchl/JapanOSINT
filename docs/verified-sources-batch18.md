# Batch 18 — verified high-penetrancy sources

270 candidate endpoints were authored; **223 were proof-of-life verified** and shipped as `native/collectors/pivot/table/hp3_*.c`. The 47 that failed are kept as data in `docs/rejected-sources-batch18.tsv`, not silently dropped.

Across one probe of the verified set the endpoints returned **602,911 records** in total — that is observed emission, not a capability claim.


## What 'verified' means here

A row shipped only if, over the wire, its endpoint returned 2xx, parsed in its declared mode, and yielded **at least one real record**. Three failure modes were treated as failures rather than passes, because each produces a source that looks alive and emits nothing:


* an **empty result set** (`{"total":0,"results":[]}`) — counted as `EMPTY_RESULTSET`, not as a one-record document;
* an **auth or quota refusal served as HTTP 200** with a small JSON body;
* an endpoint that answers but whose declared shape does not parse.


Two probe adjustments were needed, both documented in `native/tools/probe_hp_batch.py`:


1. **Per-row request headers.** SEC EDGAR and Overpass refuse the default probe agent with 403 and 406. A row declares the headers its generated collector will send, and the probe sends exactly those — so a row is never verified under different request conditions than it will run under.
2. **A raised body cap.** `verify_feeds.py` refuses anything over 8 MB on the reasoning that "a feed that big is not a feed". Since `lib/jsonstream.c` landed that is no longer true: the MITRE ATT&CK STIX bundle, the Exploit-DB index and the CIRCL CVE dump are all real and record-dense. Rejecting them would record working endpoints as dead.


A serial retry pass follows the parallel sweep, because a 429 from a rate-limited host is not evidence that a source is dead.


## Sources by group


| group | shipped | subject |
| --- | ---: | --- |
| `hp3_corp.c` | 13 | Corporate registries, beneficial ownership and securities |
| `hp3_energy.c` | 14 | Energy, grid, nuclear and extractive industry |
| `hp3_geo.c` | 26 | Earth observation, climate, hazards, water and agriculture |
| `hp3_gov.c` | 18 | Government, law, courts and procurement |
| `hp3_infra.c` | 7 | Physical infrastructure inventories and national statistics |
| `hp3_maritime.c` | 7 | Maritime, ports, aviation infrastructure and logistics |
| `hp3_netintel.c` | 21 | Web, DNS, routing and internet infrastructure |
| `hp3_netintel2.c` | 21 | Deeper network intelligence: routing measurement and DNS |
| `hp3_portals.c` | 30 | Subnational and agency open-data platforms |
| `hp3_research.c` | 17 | Research, scholarly identity, patents and clinical trials |
| `hp3_sigint.c` | 14 | SIGINT, RF spectrum and space situational awareness |
| `hp3_society.c` | 18 | Public health, conflict, human rights and media |
| `hp3_threat.c` | 17 | Cyber threat intelligence and CERT advisories |
| **total** | **223** | |

## Observed response shapes


| shape | rows |
| --- | ---: |
| json-array | 54 |
| json:results | 18 |
| json:result.results | 11 |
| json-object | 10 |
| text-lines | 10 |
| json:data | 10 |
| json:datasets | 10 |
| geojson | 8 |
| json:elements | 8 |
| json:items | 5 |
| json:Answer | 4 |
| json:objects | 4 |
| json:hourly.time | 3 |
| atom | 2 |
| json:value | 2 |
| json:message.items | 2 |
| json:search | 2 |
| json:esearchresult.idlist | 2 |
| json:stations | 2 |
| json:daily.time | 2 |
| json:rdapConformance | 2 |
| json:data.prefixes | 2 |
| json:data.stats | 2 |
| json:value.timeSeries | 2 |
| json:txs | 1 |
| json:containers.adp | 1 |
| json:studies | 1 |
| json:crates | 1 |
| json:Question | 1 |
| json:dataSets | 1 |
| json:result.items | 1 |
| json:resultList.result | 1 |
| json:extension.annotation | 1 |
| rss | 1 |
| json:Kimsuky.related | 1 |
| json:win.cobalt_strike.library_entries | 1 |
| json:events | 1 |
| json:categories | 1 |
| json:near_earth_objects.2024-01-02 | 1 |
| json:geometry.coordinates | 1 |
| json:esearchresult.translationstack | 1 |
| json:products | 1 |
| json:current_predictions.cp | 1 |
| json:@context | 1 |
| json:_embedded.enheter | 1 |
| json:cveChanges | 1 |
| json:vulnerabilities | 1 |
| json:meta.contentLanguages | 1 |
| json:states | 1 |
| json:expanded-result | 1 |
| json:references | 1 |
| json:notices | 1 |
| json:links | 1 |
| json:data.abuse_contacts | 1 |
| json:data.exact | 1 |
| json:data.located_resources | 1 |
| json:data.neighbours | 1 |
| json:data.probes | 1 |
| json:data.updates | 1 |
| json:data.rrcs | 1 |
| json:see_also | 1 |
| json:data.delegations | 1 |
| json:data.result | 1 |
| json:messages | 1 |
| json:data.validating_roas | 1 |
| json:data.visibilities | 1 |
| json:data.irr_records | 1 |
| json:drugGroup.conceptGroup | 1 |
| json:relays | 1 |
| json:z_authors | 1 |

## Categories


| category | rows |
| --- | ---: |
| government | 48 |
| infrastructure | 36 |
| environment | 22 |
| threat | 20 |
| economy | 12 |
| research | 12 |
| disaster | 10 |
| health | 9 |
| corporate | 7 |
| energy | 7 |
| telecom | 7 |
| space | 6 |
| osint | 6 |
| maritime | 5 |
| reference | 4 |
| transport | 4 |
| aviation | 4 |
| legal | 3 |
| media | 1 |

## Deliberate exclusions


Three classes of candidate were written and then removed rather than shipped:


* **Duplicates.** 33 candidates named an endpoint URL already fetched elsewhere in the tree — the `tsp_*` collectors already cover much of the spectrum/space ground, and `vsrc_government_3.c` already fetches data.gov.sg. A second registration adds a row to the registry and no capability.
* **Credential-gated endpoints that could not be proven.** Rows needing a private API key were dropped rather than shipped unverified, since the tree's standing claim is that every registered source is proof-of-life verified.
* **Mislabelled provenance.** 11 rows had been given a name and portal that did not match the endpoint actually probed — five separate "sources" all resolving to `api.worldbank.org` under other organisations' names. They were relabelled to what they are. A source that misstates who published it is a fabrication about provenance even when the records are real.


## Reproducing


```sh
cd native
python3 tools/probe_hp_batch.py ../docs/candidate-sources-batch18.*.txt \
    --out ../docs/verified-sources-batch18.tsv --pass-ids /tmp/pass.txt
python3 tools/gen_hp_batch.py ../docs/candidate-sources-batch18.*.txt \
    --outdir collectors/pivot/table --pass-ids /tmp/pass.txt
make && make audit-sources && make hptest
```


The manifests are the source of truth; `hp3_*.c` is generated. Edit the manifest, not the C.
