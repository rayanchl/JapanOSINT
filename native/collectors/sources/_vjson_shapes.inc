/* _vjson_shapes.inc — page-shaping helpers for VJSON_PREP hooks whose upstream
 * answers in a shape the JSON emitter cannot turn into records. Include AFTER
 * _vjson_idkeys.inc (VJSON_PREP lives there).
 *
 * WHY. lib/jsonlist.c emits OBJECTS that carry a scalar it can label. Four
 * upstream shapes carry real data and fail that test, so the row fetched live
 * and emitted nothing on every run (measured 2026-09-15 across the fleet):
 *
 *   1. a list of SCALARS — a vocabulary or id register: NWS event types
 *      ["911 Telephone Outage", …], RCSB PDB ids ["1ECZ", …], ransomware group
 *      names, NGA subregion numbers;
 *   2. a SINGLE OBJECT that is itself the record — HIBP's latest breach, an
 *      RDAP answer, Shodan InternetDB, a RIPEstat data call;
 *   3. a list of ONE-KEY WRAPPERS — [{pkg:{…}}], [{Dataset:{…}}], [{Tdnet:{…}}];
 *   4. SPARQL result bindings — [{s:{type,value}, p:{…}, o:{…}}], where every
 *      field is an object and none is a scalar.
 *
 * HOW. Each helper only reshapes values already in the page: a scalar becomes
 * {FIELD: scalar}; a whole object is copied into a one-element list; a wrapper
 * is replaced by the object it wraps; a binding's `value` is lifted beside it
 * as `<var>` (the {type,value} object is kept as `<var>_binding`). Nothing is
 * invented and nothing is dropped. */
#ifndef JO_VJSON_SHAPES_INC
#define JO_VJSON_SHAPES_INC
#include "_vjson_idkeys.inc"

/* The array at a dotted, case-insensitive path ("" = the document itself). */
static inline cJSON *vshape_at(cJSON *doc, const char *path) {
  if (!path || !*path) return doc;
  char seg[96];
  const char *p = path;
  cJSON *o = doc;
  while (o && *p) {
    const char *dot = strchr(p, '.');
    size_t n = dot ? (size_t)(dot - p) : strlen(p);
    if (n >= sizeof seg) return NULL;
    memcpy(seg, p, n);
    seg[n] = '\0';
    o = cJSON_GetObjectItem(o, seg);
    p = dot ? dot + 1 : p + n;
  }
  return o;
}

/* 1. [scalar, …] at PATH → [{FIELD: scalar}, …]. Objects already there stay. */
static inline void vshape_scalars(cJSON *doc, const char *path, const char *field) {
  cJSON *arr = vshape_at(doc, path);
  if (!cJSON_IsArray(arr)) return;
  int n = cJSON_GetArraySize(arr);
  for (int i = 0; i < n; i++) {
    cJSON *v = cJSON_GetArrayItem(arr, i);
    if (!v || cJSON_IsObject(v) || cJSON_IsArray(v) || cJSON_IsNull(v)) continue;
    cJSON *o = cJSON_CreateObject();
    if (!o) continue;
    cJSON_AddItemToObject(o, field, cJSON_Duplicate(v, 1));
    cJSON_ReplaceItemInArray(arr, i, o);
  }
}

/* 2. the object at PATH, whole, as the one record of doc["records"]. */
static inline void vshape_wrap(cJSON *doc, const char *path) {
  cJSON *src = vshape_at(doc, path);
  if (!cJSON_IsObject(src) || !cJSON_IsObject(doc) ||
      cJSON_GetObjectItemCaseSensitive(doc, "records"))
    return;
  cJSON *copy = cJSON_Duplicate(src, 1);
  cJSON *arr = copy ? cJSON_AddArrayToObject(doc, "records") : NULL;
  if (!arr) { cJSON_Delete(copy); return; }
  cJSON_AddItemToArray(arr, copy);
}

/* 3. [{only_key:{…}}, …] at PATH → [{…}, …]. */
static inline void vshape_unwrap(cJSON *doc, const char *path) {
  cJSON *arr = vshape_at(doc, path);
  if (!cJSON_IsArray(arr)) return;
  int n = cJSON_GetArraySize(arr);
  for (int i = 0; i < n; i++) {
    cJSON *w = cJSON_GetArrayItem(arr, i);
    if (!cJSON_IsObject(w) || !w->child || w->child->next ||
        !cJSON_IsObject(w->child))
      continue;
    cJSON *inner = cJSON_DetachItemViaPointer(w, w->child);
    cJSON_ReplaceItemInArray(arr, i, inner);
  }
}

/* 4. SPARQL bindings at PATH: {v:{type,value}} → {v:"value", v_binding:{…}}. */
static inline void vshape_sparql(cJSON *doc, const char *path) {
  cJSON *arr = vshape_at(doc, path);
  if (!cJSON_IsArray(arr)) return;
  cJSON *row;
  cJSON_ArrayForEach(row, arr) {
    if (!cJSON_IsObject(row)) continue;
    cJSON *b = row->child;
    while (b) {
      cJSON *next = b->next;
      const char *val = cJSON_IsObject(b)
          ? cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(b, "value")) : NULL;
      if (val && b->string) {
        char alt[128];
        snprintf(alt, sizeof alt, "%s_binding", b->string);
        if (!cJSON_GetObjectItemCaseSensitive(row, alt)) {
          cJSON *moved = cJSON_DetachItemViaPointer(row, b);
          char name[96];
          snprintf(name, sizeof name, "%s", moved->string ? moved->string : "value");
          cJSON_AddItemToObject(row, alt, moved);
          cJSON_AddStringToObject(row, name, val);
        }
      }
      b = next;
    }
  }
}

/* 5. a MAP OF OBJECTS at PATH — {"OSD-1":{…}, "OSD-2":{…}} or IMF's
 * indicators {"NGDPD":{label,…}} — whose KEY is the record's identity and is
 * not repeated inside the value. Each member becomes one element of
 * doc["records"]: a copy of the value with the key added as KEYFIELD (a scalar
 * value becomes {KEYFIELD: key, "value": v}). Nothing is invented: the key is
 * the upstream's own. The source map is left in place. */
static inline void vshape_map(cJSON *doc, const char *path, const char *keyfield) {
  cJSON *src = vshape_at(doc, path);
  if (!cJSON_IsObject(src) || !cJSON_IsObject(doc) ||
      cJSON_GetObjectItemCaseSensitive(doc, "records"))
    return;
  cJSON *arr = cJSON_CreateArray();
  if (!arr) return;
  cJSON *m;
  cJSON_ArrayForEach(m, src) {
    if (!m->string) continue;
    cJSON *rec;
    if (cJSON_IsObject(m)) {
      rec = cJSON_Duplicate(m, 1);
    } else {
      rec = cJSON_CreateObject();
      if (rec) cJSON_AddItemToObject(rec, "value", cJSON_Duplicate(m, 1));
    }
    if (!rec) continue;
    if (!cJSON_GetObjectItemCaseSensitive(rec, keyfield))
      cJSON_AddStringToObject(rec, keyfield, m->string);
    cJSON_AddItemToArray(arr, rec);
  }
  cJSON_AddItemToObject(doc, "records", arr);
}

#endif /* JO_VJSON_SHAPES_INC */
