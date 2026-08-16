/* collectors/sources/hp_jp_river.c — MLIT 川の防災情報 (kawabou) flood, gauge
 * and river-camera collectors.
 *
 * WHY THESE ROWS. Japanese broadcast flood coverage — the 水位観測データ charts
 * and the river camera stills cut into the bulletin — is re-displayed
 * government telemetry. The owner is MLIT's 川の防災情報 portal, so that is
 * what we collect: the gauge network, the outage/warning announcements against
 * it, and the camera URLs it publishes. Going to the aggregator instead would
 * add a hop, a ToS problem, and a second party's editorial choice about what to
 * show.
 *
 * The portal is a Vue SPA, so none of this is scrapeable from its HTML — every
 * path below is the JSON the app itself fetches, confirmed live at authoring
 * time (2026-08-16) rather than recalled:
 *
 *   file/system/rwCrntTime.json          {"crntRwTime":"2026/08/16 02:05"}
 *   file/files/info/info/infolist.json   2.6 MB, 1,001 announcements,
 *                                        3,513 relObs rows, 2,994 gauges
 *   file/files/info/optional/kwb/…       水防警報 / advisory announcements
 *   file/files/info/emergency/kwb/…      emergency announcements
 *   file/files/info/obslist/obs/{code}   per-gauge detail behind each list hit
 *
 * The 13-digit obsrvtnPointFullCode decomposes as ofcCd(5) + itmkndCd(3) +
 * obsCd(5); itmkndCd is the observation kind, and across the live list it reads
 * 1=雨量 1395, 4=水位 1341, 6=水質 105, 7=ダム 81, 3=積雪 52 — which is what
 * makes OBS_DETAIL below a meaningful pivot on a bare gauge code.
 *
 * EXHAUSTIVE USE. Nothing here hand-picks fields. The engine flattens every
 * scalar of every record with dotted keys, so an announcement carries its whole
 * relObs array — including relObs.N.cameraCurrProvUrl, the direct
 * cam.river.go.jp still image (verified 200), and the upstream-gauge links that
 * make the network navigable. Records the flatten bounds truncate are stamped,
 * not dropped silently. See docs/SOURCE_EXHAUSTIVENESS.md.
 *
 * NOT YET COVERED, on purpose rather than by omission: the water-level time
 * SERIES behind the broadcast chart. The SPA reaches it through a REST route
 * (tmObsStage/{ofcCd}/{itmkndCd}/{obsCd}/{time}/{isCurrent}) whose base is not
 * reachable from outside the app — every candidate base answers 200 with the
 * SPA shell, which is a catch-all, not data. The canonical series lives in
 * 水文水質データベース (www1.river.go.jp, SrchWaterData.exe, EUC-JP HTML) and
 * needs a bespoke parser, not a declarative row. Tracked in
 * docs/jp-river-flood-sources.md; no row here pretends to carry it. */
#include "../../lib/hpengine.h"

#define KWB_FILES  "https://www.river.go.jp/kawabou/file/files"
#define KWB_SYSTEM "https://www.river.go.jp/kawabou/file/system"
#define KWB_PORTAL "https://www.river.go.jp/kawabou/"
#define KWB_UA     "User-Agent: Mozilla/5.0 (compatible; JapanOSINT/1.0)"
#define KWB_REF    "Referer: https://www.river.go.jp/kawabou/"
#define KWB_HDRS   { KWB_UA, KWB_REF, NULL }
#define KWB_TAGS   "\"japan\",\"river\",\"flood\",\"mlit\",\"disaster\""

static const hp_source HP_JP_RIVER[] = {
  /* ── The announcement set: what MLIT is currently saying about the gauge
   * network. Mostly 欠測・未受信 (a gauge has stopped reporting) plus flood and
   * water-level-reached notices — and a gauge going dark during rain is itself
   * the signal. Each record drags its whole relObs array along, which is where
   * the camera URLs live. ─────────────────────────────────────────────────── */
  { .id = "JP_MLIT_KAWABOU_INFO",
    .name = "MLIT Kawabou — river information announcements",
    .name_ja = "国土交通省 川の防災情報 — お知らせ情報",
    .collector = "jp_river", .category = "disaster", .type = "api",
    .portal = KWB_PORTAL, .record_type = "jp-river-info", .tags = KWB_TAGS,
    .mode = HP_JSON, .want = HP_ANY,
    .url = KWB_FILES "/info/info/infolist.json", .headers = KWB_HDRS,
    .array_path = "info",
    .title_keys = "summary", .id_keys = "infoManageCode",
    .date_keys = "announceTime", .body_keys = "detail",
    .link_tmpl = KWB_PORTAL, .free_tier = 1, .interval = 600,
    .description = "Every active 川の防災情報 announcement: outage/欠測 notices, "
      "water-level-reached and flood notices, with announce/start/end times, the "
      "issuing area, and the full related-observatory list (gauge codes, names, "
      "upstream links and river-camera still URLs) attached to each" },

  /* ── 水防警報 and other optional-channel announcements. Same shape, different
   * publication channel; the portal keeps them separate so we do too. ────── */
  { .id = "JP_MLIT_KAWABOU_OPTINFO",
    .name = "MLIT Kawabou — flood-prevention and optional announcements",
    .name_ja = "国土交通省 川の防災情報 — 水防警報・任意情報",
    .collector = "jp_river", .category = "disaster", .type = "api",
    .portal = KWB_PORTAL, .record_type = "jp-river-alert", .tags = KWB_TAGS,
    .mode = HP_JSON, .want = HP_ANY,
    .url = KWB_FILES "/info/optional/kwb/optinfolist.json", .headers = KWB_HDRS,
    .array_path = "info",
    .title_keys = "summary", .id_keys = "infoManageCode",
    .date_keys = "announceTime", .body_keys = "detail",
    .link_tmpl = KWB_PORTAL, .free_tier = 1, .interval = 600,
    .description = "水防警報 (flood-prevention warnings) and other optional-channel "
      "announcements published by MLIT river offices, with issuing area, "
      "announcement and validity times and the full announcement text" },

  /* ── The emergency channel. Empty on a calm day ({\"info\":[]}), which is an
   * honest empty and NOT an error — that distinction is SOURCE_AUTHORING R3 and
   * the engine already gets it right. ────────────────────────────────────── */
  { .id = "JP_MLIT_KAWABOU_EMGINFO",
    .name = "MLIT Kawabou — emergency river announcements",
    .name_ja = "国土交通省 川の防災情報 — 緊急情報",
    .collector = "jp_river", .category = "disaster", .type = "api",
    .portal = KWB_PORTAL, .record_type = "jp-river-emergency", .tags = KWB_TAGS,
    .mode = HP_JSON, .want = HP_ANY,
    .url = KWB_FILES "/info/emergency/kwb/emginfolist.json", .headers = KWB_HDRS,
    .array_path = "info",
    .title_keys = "summary", .id_keys = "infoManageCode",
    .date_keys = "announceTime", .body_keys = "detail",
    .link_tmpl = KWB_PORTAL, .free_tier = 1, .interval = 300,
    .description = "MLIT emergency river announcements — the channel used when a "
      "river situation escalates beyond routine warning. Normally empty; a "
      "non-empty fetch is the event" },

  /* ── The pivot. Hand it a 13-digit gauge code (they arrive by the thousand in
   * the rows above) and it returns that gauge's own announcement history. This
   * is the detail endpoint behind each list hit, exposed as an entity pivot
   * rather than fired 3,487 times per scheduled run. ─────────────────────── */
  { .id = "JP_MLIT_KAWABOU_OBS_DETAIL",
    .name = "MLIT Kawabou — observatory detail by gauge code",
    .name_ja = "国土交通省 川の防災情報 — 観測所詳細",
    .collector = "jp_river", .category = "disaster", .type = "api",
    .portal = KWB_PORTAL, .record_type = "jp-river-observatory", .tags = KWB_TAGS,
    .mode = HP_JSON, .want = HP_NUMERIC,
    .url = KWB_FILES "/info/obslist/obs/{qd}.json", .headers = KWB_HDRS,
    .array_path = "tm",
    .id_keys = "obsrvtnPointFullCode",
    .link_tmpl = KWB_PORTAL, .free_tier = 1,
    .description = "Everything 川の防災情報 holds against one 13-digit observatory "
      "code (ofcCd+itmkndCd+obsCd): its announcements, upstream-gauge links and "
      "any river camera bound to it, including the direct still-image URL" },

  /* ── Freshness anchor. One scalar, but it is the timestamp every other row's
   * data is as-of, and a stalled clock is how you notice the whole telemetry
   * feed has frozen. ─────────────────────────────────────────────────────── */
  { .id = "JP_MLIT_KAWABOU_OBSTIME",
    .name = "MLIT Kawabou — telemetry clock",
    .name_ja = "国土交通省 川の防災情報 — 観測時刻",
    .collector = "jp_river", .category = "disaster", .type = "api",
    .portal = KWB_PORTAL, .record_type = "jp-river-clock", .tags = KWB_TAGS,
    .mode = HP_JSON, .want = HP_ANY,
    .url = KWB_SYSTEM "/tmCrntTime.json", .headers = KWB_HDRS,
    /* The document is one scalar, {"crntObsTime":"2026/08/16 02:10"}, and the
     * engine drops a record that resolves neither a title nor an id — correctly,
     * since that is shape noise everywhere else. Name the field explicitly. */
    .title_keys = "crntObsTime", .id_keys = "crntObsTime",
    .date_keys = "crntObsTime",
    .link_tmpl = KWB_PORTAL, .free_tier = 1, .interval = 600,
    .description = "The observation timestamp the whole 川の防災情報 telemetry set "
      "is current as of. Cheap, and it is what distinguishes 'the river is calm' "
      "from 'the feed stopped updating'" },
};

HP_REGISTER_TABLE(HP_JP_RIVER)
