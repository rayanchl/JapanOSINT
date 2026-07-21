/* core/intel.h — the single intel_sink (port of intelStore.upsertItems +
 * ftsMirror.writeOne). Every source emits here: base upsert (exact Node
 * INSERT/ON CONFLICT incl. geom_source='llm' preservation) + FTS mirror with
 * MeCab segmentation, in one transaction. */
#ifndef JO_INTEL_H
#define JO_INTEL_H
#include "../source.h"

/* Build a sink bound to db + source_id (+ optional tenant_id). */
intel_sink intel_sink_make(db_handle *db, const char *source_id,
                           const char *tenant_id);

#endif
