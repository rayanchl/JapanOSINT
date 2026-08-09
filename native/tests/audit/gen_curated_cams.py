#!/usr/bin/env python3
"""Regenerate the curated camera catalogs the C port dropped — WITH the
per-camera URLs and endpoint fields Node derived.

The first pass of this generator restored coordinates but silently dropped every
URL, which made the 179 curated rows the only cameras in the fleet you could not
open (camera_upsert sets it.link from properties.url). It also missed two
catalogs: EARTHCAM_JP, because Node builds it through a positional helper
`EC(slug,name,lat,lon,city)` rather than object literals, and
WEBCAMENDIRECT_SEED.

URL derivation, reproduced from cameraDiscovery.js:
  jma_volcano   https://www.data.jma.go.jp/svd/volcam/data/gazo/<vid>.html   :116
  mlit_river    https://www.river.go.jp/portal/?region=80&contents=multi
                &pointLat=<lat>&pointLng=<lon>                               :124
  manual        http://<ip>:<port><path or '/'>                              :920
  earthcam      https://www.earthcam.com<slug>                     (EC, :478)
  expressway / broadcast / tourism / webcamendirect: the entry's own `url`

Provenance keys use the unified vocabulary (geo_provenance / geo_precision /
geo_uncertain) shared with the aggregator collectors.

Reads the deleted _cameraSources.js out of git; writes
collectors/sources/cam_curated_jp.c.
"""
import os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(os.path.dirname(HERE))
OUT = os.path.join(NATIVE, "collectors", "sources", "cam_curated_jp.c")
DELETED = "server/src/collectors/_cameraSources.js"

# catalog -> (camera_type, discovery_channel, human label)
CATALOGS = {
    "JMA_VOLCANO_CAMS":   ("volcano_monitor", "jma_volcano", "JMA volcano observation camera"),
    "MLIT_RIVER_CAMS":    ("river_monitor",   "mlit_river",  "MLIT river monitoring camera"),
    "EXPRESSWAY_CAMS":    ("traffic",         "expressway_cctv", "Expressway traffic camera"),
    "BROADCAST_LIVECAMS": ("broadcast",       "broadcast_livecam", "Broadcast live camera"),
    "TOURISM_CAMS":       ("tourism",         "tourism_webcam", "Tourism live camera"),
    "EARTHCAM_JP":        ("aggregator_earthcam", "earthcam", "EarthCam Japan"),
    "MANUAL_IP_CAMS":     ("ip_camera",       "manual_ip_seed", "Manually catalogued IP camera"),
    "WEBCAMENDIRECT_SEED": ("aggregator", "webcamendirect_seed", "Webcam-en-direct curated seed"),
}

ENTRY = re.compile(r"\{([^{}]*)\}")
FIELD = re.compile(r"(\w+)\s*:\s*(?:'([^']*)'|\"([^\"]*)\"|(-?[\d.]+))")
EC = re.compile(r"EC\(\s*'([^']*)'\s*,\s*'([^']*)'\s*,\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*,\s*'([^']*)'\s*\)")


def esc(s):
    return (s or "").replace("\\", "\\\\").replace('"', '\\"')


def main():
    root = subprocess.run(["git", "rev-parse", "--show-toplevel"],
                          capture_output=True, cwd=NATIVE).stdout.decode().strip()
    sha = subprocess.run(["git", "log", "--diff-filter=D", "--format=%H", "-1",
                          "--", DELETED], capture_output=True,
                         cwd=root).stdout.decode().strip()
    if not sha:
        sys.exit("could not find the commit that deleted %s" % DELETED)
    js = subprocess.run(["git", "show", "%s^:%s" % (sha, DELETED)],
                        capture_output=True, cwd=root).stdout.decode("utf-8", "replace")

    rows = []
    for cat, (ctype, chan, label) in CATALOGS.items():
        m = re.search(r"export const %s = \[(.*?)\n\];" % cat, js, re.S)
        if not m:
            print("  (catalog %s not found)" % cat, file=sys.stderr)
            continue
        body, n = m.group(1), 0

        if cat == "EARTHCAM_JP":               # positional helper, not literals
            for slug, name, lat, lon, city in EC.findall(body):
                rows.append({"name": name, "lat": float(lat), "lon": float(lon),
                             "ctype": ctype, "chan": chan, "catalog": label,
                             "url": "https://www.earthcam.com" + slug,
                             "ref": city, "operator": "", "thumb": "",
                             "ip": "", "port": 0, "path": "", "product": ""})
                n += 1
            print("  %-22s %3d entries" % (cat, n), file=sys.stderr)
            continue

        for em in ENTRY.finditer(body):
            f = {}
            for fm in FIELD.finditer(em.group(1)):
                f[fm.group(1)] = fm.group(2) or fm.group(3) or fm.group(4)
            if "lat" not in f or "lon" not in f or "name" not in f:
                continue
            try:
                lat, lon = float(f["lat"]), float(f["lon"])
            except ValueError:
                continue

            url = f.get("url", "")
            if cat == "JMA_VOLCANO_CAMS" and f.get("vid"):
                url = ("https://www.data.jma.go.jp/svd/volcam/data/gazo/%s.html"
                       % f["vid"])
            elif cat == "MLIT_RIVER_CAMS":
                url = ("https://www.river.go.jp/portal/?region=80&contents=multi"
                       "&pointLat=%s&pointLng=%s" % (f["lat"], f["lon"]))
            elif cat == "MANUAL_IP_CAMS" and f.get("ip"):
                url = "http://%s:%s%s" % (f["ip"], f.get("port", "80"),
                                          f.get("path") or "/")

            rows.append({"name": f["name"], "lat": lat, "lon": lon,
                         "ctype": f.get("type") or ctype, "chan": chan,
                         "catalog": label, "url": url,
                         "ref": f.get("vid") or f.get("office") or f.get("city") or "",
                         "operator": f.get("operator", ""),
                         "thumb": f.get("thumbnail", ""),
                         "ip": f.get("ip", ""), "port": int(f.get("port") or 0),
                         "path": f.get("path", ""), "product": f.get("product", "")})
            n += 1
        print("  %-22s %3d entries" % (cat, n), file=sys.stderr)

    with open(OUT, "w", encoding="utf-8") as fh:
        fh.write('''/* cam_curated_jp.c — curated Japanese camera catalogs with REAL coordinates.
 *
 * Restores the per-camera catalogs the C port dropped. Every other camera
 * channel is an aggregator scrape that falls back to a prefecture centroid when
 * the page carries no coordinate, which is why a large share of camera rows
 * stack onto roughly twenty points. These entries are the operators' own
 * published station positions — JMA volcano observation cameras, MLIT river
 * gauge cameras, expressway, broadcast, tourism, EarthCam, the webcam-en-direct
 * seed, and the manually catalogued IP cameras.
 *
 * Each row carries the URL Node derived for it, reproduced exactly
 * (cameraDiscovery.js:116 volcano, :124 river portal deep-link, :920 IP camera,
 * _cameraSources.js:478 EarthCam). Without it camera_upsert() has no `m_url`,
 * so `it.link` is NULL and these — the only rows in the fleet with exact
 * operator coordinates — become the only ones a user cannot open.
 *
 * Provenance uses the unified vocabulary shared with every other camera
 * collector: geo_provenance / geo_precision / geo_uncertain.
 *
 * Regenerated by tests/audit/gen_curated_cams.py; nothing here is derived or
 * guessed. Emitted through camera_upsert() so the rows land in the shared
 * camera keyspace (camera_fc_json cannot see geojson_emit_features output). */
#include "../../source.h"
#include "../../core/camera_store.h"
#include "../../lib/camfeature.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>

typedef struct {
  const char *name;
  double lat, lon;
  const char *camera_type;
  const char *channel;
  const char *catalog;
  const char *url;         /* the operator's own page for THIS camera        */
  const char *ref;         /* vid / bureau / city — provenance, not a link   */
  const char *operator_;
  const char *thumb;
  const char *ip;          /* MANUAL_IP_CAMS only                            */
  int         port;
  const char *path;
  const char *product;
} curated_cam;

static const curated_cam CAMS[] = {
''')
        for r in rows:
            fh.write('  {"%s", %.6f, %.6f, "%s", "%s", "%s", "%s", "%s", "%s", "%s", "%s", %d, "%s", "%s"},\n'
                     % (esc(r["name"]), r["lat"], r["lon"], r["ctype"], r["chan"],
                        esc(r["catalog"]), esc(r["url"]), esc(r["ref"]),
                        esc(r["operator"]), esc(r["thumb"]), esc(r["ip"]),
                        r["port"], esc(r["path"]), esc(r["product"])))
        fh.write('''};
#define N_CAMS ((int)(sizeof CAMS / sizeof CAMS[0]))

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = 0;
  for (int i = 0; i < N_CAMS; i++) {
    const curated_cam *c = &CAMS[i];
    char portbuf[16];
    if (c->port) snprintf(portbuf, sizeof portbuf, "%d", c->port);
    cam_kv extra[11];
    int e = 0;
    /* `url` MUST come first among the extras: camera_upsert reads it as m_url
     * for it.link, and cam_make_feature keys the uid tail off it. */
    extra[e].k = "url";            extra[e].sv = c->url[0] ? c->url : NULL;
    extra[e].is_num = extra[e].is_bool = extra[e].is_null = 0; e++;
    extra[e].k = "thumbnail_url";  extra[e].sv = c->thumb[0] ? c->thumb : NULL;
    extra[e].is_num = extra[e].is_bool = extra[e].is_null = 0; e++;
    extra[e].k = "operator";       extra[e].sv = c->operator_[0] ? c->operator_ : NULL;
    extra[e].is_num = extra[e].is_bool = extra[e].is_null = 0; e++;
    extra[e].k = "catalog";        extra[e].sv = c->catalog;
    extra[e].is_num = extra[e].is_bool = extra[e].is_null = 0; e++;
    extra[e].k = "reference";      extra[e].sv = c->ref[0] ? c->ref : NULL;
    extra[e].is_num = extra[e].is_bool = extra[e].is_null = 0; e++;
    if (c->ip[0]) {
      extra[e].k = "ip";      extra[e].sv = c->ip;
      extra[e].is_num = extra[e].is_bool = extra[e].is_null = 0; e++;
      extra[e].k = "port";    extra[e].sv = portbuf;
      extra[e].is_num = extra[e].is_bool = extra[e].is_null = 0; e++;
      if (c->path[0]) {
        extra[e].k = "path";  extra[e].sv = c->path;
        extra[e].is_num = extra[e].is_bool = extra[e].is_null = 0; e++;
      }
    }
    if (c->product[0]) {
      extra[e].k = "product"; extra[e].sv = c->product;
      extra[e].is_num = extra[e].is_bool = extra[e].is_null = 0; e++;
    }
    /* unified provenance vocabulary */
    extra[e].k = "geo_provenance"; extra[e].sv = "curated-catalog";
    extra[e].is_num = extra[e].is_bool = extra[e].is_null = 0; e++;
    extra[e].k = "geo_precision";  extra[e].sv = "exact";
    extra[e].is_num = extra[e].is_bool = extra[e].is_null = 0; e++;

    cJSON *f = cam_make_feature(c->lat, c->lon, c->name, c->camera_type,
                                c->channel, extra, e);
    if (!f) continue;
    if (camera_upsert(ctx->db, sink, f, c->channel) >= 0) n++;
    cJSON_Delete(f);
  }
  fprintf(stderr, "[cam-curated-jp] emitted %d of %d curated cameras\\n", n, N_CAMS);
  return 0;
}

static const source_def cam_curated_jp_def = {
  .id = "cam-curated-jp", .collector = "camera-discovery",
  .name = "Curated Japan camera catalog", .name_ja = "\\xe5\\x9b\\xbd\\xe5\\x86\\x85\\xe3\\x82\\xab\\xe3\\x83\\xa1\\xe3\\x83\\xa9\\xe3\\x82\\xab\\xe3\\x82\\xbf\\xe3\\x83\\xad\\xe3\\x82\\xb0",
  .update_interval_sec = 3600, .run = run,
  .category = "infrastructure", .type = "dataset",
  .url = "internal://curated-camera-catalog",
  .description = "Curated JMA volcano, MLIT river, expressway, broadcast, tourism, "
                 "EarthCam and manually catalogued IP camera positions with exact "
                 "operator-published coordinates and per-camera links.",
  .layer = "cameras", .free_tier = 1,
};
REGISTER_SOURCE(cam_curated_jp_def)
''')
    print("wrote %s with %d cameras (%d carrying a URL)"
          % (OUT, len(rows), sum(1 for r in rows if r["url"])), file=sys.stderr)


if __name__ == "__main__":
    main()
