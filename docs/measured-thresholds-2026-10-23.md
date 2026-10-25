# The eleven similarity thresholds, measured on the production model (2026-10-23)

Open item E1, and CLAUDE.md's standing sentence about `core/thresholds.c`:
**"none has been measured on the production model"**. That is no longer true.

A real embedding server was stood up for this — llama.cpp built from source and
`multilingual-e5-large-q8_0.gguf`, which is the file this tree's own notes name
as the production model (`docs/semantic-correlation-plan-2026-09-11.md`). It
answers `/v1/embeddings` with **1024-dimensional unit vectors**, which is what
`core/llm.c` expects and why L2 ranking equals cosine ranking. The e5
`query:`/`passage:` prefixes were NOT used, because `core/service_vec.c`
measured them worse on this corpus and defaults them off — so the readings below
are in the space the engine actually uses.

Raw output: `docs/measured-thresholds-2026-10-23.tsv`
(`tools/measure_thresholds.py --embed-url … --db … --cap 200`).

**Nothing here is written back to a threshold.** The tool does not change
values, and this document does not propose any: choosing a number needs pairs a
human has judged as "should match", and for eight of the eleven keys no such
label exists anywhere in this tree. What the measurement CAN settle, and does,
is whether a value in force rejects anything at all.

## 1. `story_tau` is measured, and 0.85 is right

200 positive pairs (two items sharing an `intel_items.cluster_id`, which is
simhash and so independent of any embedding) against 200 negative pairs (items
from a different cluster AND a different source):

| | 10th pct | median | max |
| --- | --- | --- | --- |
| positives (same simhash cluster) | 0.9395 | 1.0000 | 1.0000 |
| negatives (different cluster, different source) | 0.7161 | 0.7471 | 0.8372 |

The two classes **separate completely**. Best F1 cut-off 0.8372 with F1 = 1.000;
the value in force, **0.85, achieves precision 1.000 and recall 1.000** and sits
in the gap with headroom on both sides (negatives top out at 0.8372, positives
start at 0.9395). The guess was right, and now it is a measurement.

## 2. The unrelated band on this model starts at 0.69, and five cut-offs are below it

This is the finding that matters for the other ten keys, and it needs no human
labels because the negatives are negative **by construction**: a short question
about one subject against a record from an unrelated source is not a match,
whatever a human would say about the borderline cases.

**300 question ↔ unrelated-record pairs**, 12 questions against records drawn
one per source across 30 sources:

    min 0.6874   p05 0.7206   p25 0.7467   p50 0.7641   p75 0.7859
    p95 0.8119   p99 0.8298   max 0.8448

So on multilingual-e5-large, two pieces of text that have nothing to do with
each other score **0.69 to 0.84**. Against that:

| key | in force | admits, of UNRELATED pairs |
| --- | --- | --- |
| `relevance_sim_min` | 0.40 | **100.0%** |
| `suggest_min_cos` | 0.50 | **100.0%** |
| `route_min_cos` | 0.55 | **100.0%** |
| `resolve_emb_min_cos` | 0.60 | **100.0%** |
| `repair_sem_cos_floor` | 0.70 | **99.3%** |
| `entity_vec_cos_floor` | 0.80 | 14.0% |
| `alert_sem_threshold` | 0.82 | 2.0% |

And `corpus_vec_max_distance` is an L2 ceiling of **√2 = 1.4142**: the measured
L2 distance for these unrelated pairs is **0.5572 to 0.7906**, so that ceiling
admits 100% of them too. (For unit vectors L2² = 2 − 2cos, so √2 is cos = 0.0 —
a bound no pair of real texts ever reaches on this model.)

**Six of the eleven cut-offs cannot reject anything.** Four are below the
minimum unrelated score ever observed, one is below its first percentile, and
one is an L2 ceiling at twice the observed maximum.

### `route_min_cos` measured in its own space, because it is not a record

`route_min_cos` compares a question against a **source card** (id, name,
category — short text), not against a record, so the band could have been
different. It is not: 300 question ↔ randomly-chosen-source-card pairs, cards
taken from the registry's own `sources` rows, score

    min 0.7261   p05 0.7378   p50 0.7629   p95 0.7938   max 0.8277

The **minimum** is 0.7261 against a cut-off of 0.55. CLAUDE.md describes this
key as deciding "which scheduled sources a question with no entity is routed
to — every one at or above it, no count cap", so as shipped, such a question
routes to **every scheduled source**.

### What this does and does not license

It does **not** say what these values should be. It says: any cosine cut-off on
this model has to be **above ~0.85** before it begins to reject unrelated text,
because that is where the unrelated band ends (0.8448) and where the known-same
band begins (story positives from 0.9395). Picking a value inside that window
needs positives — pairs a human has marked "these two SHOULD match" — and the
tool reads those from a `--labels` file precisely so that nobody has to invent
them. `docs/threshold-labels.jsonl` does not exist yet; it is the next step and
it is human work, not a measurement.

Two of the ten are not measurable this way at all and say so:
`resolve_emb_margin` is a GAP between a best match and a runner-up, so a pair
cannot express it (it needs a triple), and `camera_dup_cosine_min` lives in the
image index, a different model, with `camera_stills` empty in any database
available here.

## 3. Three defects in the harness, found by running it for real

The tool had only ever been run against its own stub, and the first real run
found three things — which is the usual result of pointing a gate at reality.

* **`in_force` was blank on every key.** The column was filled only from
  `--api` (a live server's `/api/admin/thresholds`) or an env var;
  `core/thresholds.c` names each default as a MACRO, and nothing resolved it. So
  `prec_at_in_force` and `rec_at_in_force` — the two columns that answer "is the
  value we ship right?" — could not be computed at all, and the tool could
  report a better cut-off while saying nothing about the current one. It now
  resolves the macro out of the owning module, with the same precedence
  `thresholds.c` implements (db > env > compiled constant), and a macro it
  cannot read leaves `in_force` blank with `source = unresolved (<MACRO>)`
  rather than a number it guessed.
* **Only six of the eleven defaults are in a HEADER.** CLAUDE.md says the
  compiled constant "stays in its header as the documented default"; five
  (`RESOLVE_EMB_MIN_COS_DEFAULT`, `RESOLVE_EMB_MARGIN_DEFAULT`,
  `REPAIR_SEM_COS_FLOOR_DEFAULT`, `CORPUS_VEC_MAX_DISTANCE_DEFAULT`,
  `ROUTE_MIN_COS_DEFAULT`) are in `core/thresholds.c` itself. A header-only
  resolver read none of them, so headers are read first and win and `.c` files
  fill the gaps.
* **THE NEGATIVE SET WAS EMPTY ON A REAL DATABASE, and the cause is a mistake
  this tree wrote up the same day in another place.** `pairs_story` read
  `order by cluster_id limit cap*40` and then required a negative pair to come
  from two different sources. On a 412,521-row database holding 31 sources,
  every one of the 8,000 rows that prefix returned came from ONE source, so the
  different-source test never held once: `n_neg = 0`, verdict "ONE CLASS ONLY",
  after a complete embedding run. **A sample taken off the head of a collection
  is not a sample of the collection** — the same error that made an 800-row
  sample say a record key was perfect while it lost 32 records a run. The sample
  is now drawn per source with a window function, and negatives pair sources
  round-robin so a 400,000-row source cannot supply both halves of every pair.

The self-test gained two cases for the first two (every key's default resolves;
every resolved default is inside the range a cosine or an L2 can take), so a
twelfth threshold added without a readable default fails `make unit`.

## 4. How to reproduce

    # 1. a server speaking the same contract as core/llm.c
    git clone --depth 1 https://github.com/ggml-org/llama.cpp src
    cmake -S src -B src/build -DCMAKE_BUILD_TYPE=Release -DLLAMA_CURL=OFF \
          -DLLAMA_BUILD_TESTS=OFF -DLLAMA_BUILD_EXAMPLES=OFF
    cmake --build src/build --target llama-server -j
    curl -L -o multilingual-e5-large-q8_0.gguf \
      https://huggingface.co/soichisumi/multilingual-e5-large-Q8_0-GGUF/resolve/main/multilingual-e5-large-q8_0.gguf
    src/build/bin/llama-server -m multilingual-e5-large-q8_0.gguf \
      --embedding --pooling mean -c 512 --host 127.0.0.1 --port 8099

    # 2. a database with items from MANY sources (the per-source sample needs
    #    them) — any scratch DB several collectors have run into will do
    # 3. the measurement
    python3 tools/measure_thresholds.py --embed-url http://127.0.0.1:8099 \
        --db /path/to/scratch.db --cap 200 \
        --out docs/measured-thresholds-<date>.tsv

A reading against a different model is a different reading, which is why the
model is printed into the file's first line.
