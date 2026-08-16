# MLIT 川の防災情報 — flood, gauge and river-camera sources

What this is: the collector family behind `native/collectors/sources/hp_jp_river.c`,
the reachable JSON surface of MLIT's 川の防災情報 portal, and an honest statement
of the one piece that is **not** covered.

## Why the government portal and not the broadcaster

The trigger for this work was a TV bulletin showing a 水位観測データ chart
(栄町 / 葭川) and river camera stills. That display is re-published government
telemetry. Collecting the portal instead of the aggregator removes a hop, avoids
a commercial ToS problem, and means nobody else's editorial choice decides which
gauges we can see.

## Reachability, as measured

`www.river.go.jp` answers `403` to a bare request and `200` with an ordinary
browser `User-Agent`. It is a Vue SPA, so **the HTML contains no data** — every
useful path below was read out of the app bundle
(`/kawabou/js/app.6ca8c169.js`) and then confirmed live on 2026-08-16.

Three JSON hosts exist, all under `location.origin`:

| repository | base | holds |
|---|---|---|
| JsonRepository | `/kawabou/file/files` | announcements, observatory lists |
| MapRepository | `/kawabou/file/gjson` | map layers, keyed by river-warning code |
| systemRepository | `/kawabou/file/system` | the telemetry clocks |

### Confirmed live and collected

| endpoint | observed | source id |
|---|---|---|
| `file/system/tmCrntTime.json` | `{"crntObsTime":"2026/08/16 02:10"}` | `JP_MLIT_KAWABOU_OBSTIME` |
| `file/files/info/info/infolist.json` | 2.6 MB, 1,001 announcements, 3,513 `relObs` rows, 2,994 distinct gauges | `JP_MLIT_KAWABOU_INFO` |
| `file/files/info/optional/kwb/optinfolist.json` | 水防警報 / advisory channel | `JP_MLIT_KAWABOU_OPTINFO` |
| `file/files/info/emergency/kwb/emginfolist.json` | `{"info":[]}` on a calm day | `JP_MLIT_KAWABOU_EMGINFO` |
| `file/files/info/obslist/obs/{code}.json` | per-gauge detail, 3,487 codes available | `JP_MLIT_KAWABOU_OBS_DETAIL` |
| `cam.river.go.jp/cam/now/*.jpg` | camera still, HTTP 200 | carried as fields, see below |

Measured on a first run:

```
JP_MLIT_KAWABOU_INFO     emitted 1001 of 1001 available across 1 page(s)
JP_MLIT_KAWABOU_OPTINFO  emitted 2 of 2 available across 1 page(s)
JP_MLIT_KAWABOU_EMGINFO  records=0 in 183ms      <- real fetch, honest empty
JP_MLIT_KAWABOU_OBSTIME  emitted 1 of 1 available
```

### The gauge code

`obsrvtnPointFullCode` is 13 digits and decomposes as
`ofcCd(5) + itmkndCd(3) + obsCd(5)`. Across the live list `itmkndCd` reads:

| itmkndCd | meaning | count |
|---|---|---|
| 1 | 雨量 rainfall | 1,395 |
| 4 | 水位 stage | 1,341 |
| 6 | 水質 water quality | 105 |
| 7 | ダム dam | 81 |
| 3 | 積雪 snow depth | 52 |
| 12 / 9 / 16 | other | 20 |

That decomposition is what makes `JP_MLIT_KAWABOU_OBS_DETAIL` a meaningful pivot
on a bare gauge code.

### Cameras

River cameras are not a separate endpoint. Each announcement's `relObs` rows
carry the camera inline — `cameraId`, `cameraNm`, `cameraOfcCd`, `cameraSysId`
and four URL fields, of which `cameraCurrProvUrl` is the direct still, e.g.
`https://cam.river.go.jp/cam/now/cctv_010006_11C03774.jpg` (verified 200).
Because the engine flattens every scalar with dotted keys, those arrive as
`relObs.N.cameraCurrProvUrl` without any row naming them. Nothing is dropped.

Only a small number of `relObs` rows carry a camera at any moment — the camera
binding appears on the announcement that needs it, so coverage grows during an
actual flood event, which is when it matters.

## What is NOT covered, and why

**The water-level time series** — the actual chart from the broadcast.

The SPA reaches it through a REST route built as
`tmObsStage/{ofcCd}/{itmkndCd}/{obsCd}/{YYYY-MM-DD HH:mm}/{isCurrent}`
(and `tmObsRain`, `tmObsDam`, `tmObsSnow`, `tmObsWtrQual` siblings). The route is
real; its base is not reachable from outside the app. Every candidate base
(`/api`, `/kawabou/api`, `/kawabou/pcfull/api`, `/kawabou/rp/api`) answers
`200` with the SPA shell — a catch-all, not data. The `tmlist/stg/...` file
variant 404s for every real gauge code tried.

No row here pretends to carry that series. Per house rule 1, an uncovered
endpoint is stated, not faked.

The canonical series does exist elsewhere: **水文水質データベース**
(`www1.river.go.jp`), and its contract is confirmed:

```
SrchWaterData.exe?ID=…&KIND=1&PAGE=0   -> the SEARCH FORM, not data (9 KB)
DspWaterData.exe?KIND=1&ID=<15-digit>&BGNDATE=YYYYMMDD&ENDDATE=YYYYMMDD&KAWABOU=NO
    -> 任意期間時刻水位一覧表 — 年月日 / 時刻 / 水位(m), 確定値 vs 暫定値
DspWaterData.exe?KIND=6&…             -> パラメータに誤りがあります (wrong KIND)
```

So `KIND=1` on `DspWaterData.exe` is the hourly water-level table — the
broadcast chart's actual data. Two things stand between that and a collector,
and neither is a blocker, just work:

1. **EUC-JP HTML tables**, so it needs a bespoke parser rather than a
   declarative row. The tree already links iconv and has `lib/htmlparse.c`.
2. **A station-ID crosswalk.** 水文水質DB keys on a 15-digit 観測所記号
   (e.g. `303011283306010`), a different namespace from kawabou's 13-digit
   `obsrvtnPointFullCode`. The 1,341 stage gauges kawabou gives us cannot be
   fed to this CGI directly; the station list has to be harvested from
   水文水質DB's own search interface first.

That is the next collector in this family, and it is deliberately not
half-built here.

## Engine changes this work required

Two fixes in `lib/hpengine.c`, both verified by `make hptest` (23/23) and
`make audit-sources` (strict set: 0 findings):

1. **Scheduled HP rows could never run.** `hp_run()` returned immediately when
   `ctx->entity` was NULL. A row setting `.interval` was therefore registered
   with a live `update_interval_sec`, picked up by the scheduler, and did
   nothing on every tick — reporting `rc=0 records=0` as if the upstream had
   been quiet. A scheduled run is now allowed exactly when the row's URL and
   POST body reference no `{q…}`/`{Q}` token; rows that do need an entity still
   return the honest empty. No row in the tree had ever set `.interval`, so this
   was a latent gap rather than a live regression.

2. **`emitted N of 0 available`.** The root-is-the-record path never
   incremented the availability tally, so a single-object endpoint disclosed a
   shortfall that had not happened. That line is the in-band "how much of how
   much" the exhaustive-use rule depends on, so it has to be right.
