
Proceeding with Phase 1, using my recommendation (dedicated breach_items + breach_fts). Reality check I'll state up front: there's no C compiler on this Windows box, so I'll write this carefully to compile-by-construction and match existing conventions exactly, but it will need a make on your Mac to confirm. Let me read the real code first so the implementation integrates precisely.

  Read 3 files, ran 1 shell command

Critical detail from the real code: the current ingest hashes the identifier and discards the cleartext (nv is freed at breach_index.c:266 right after sha1_hex) — the shard only ever stores the SHA-1, never the value. So to make datapoints searchable by value as intel, the eager path must capture the normalized cleartext before it's freed (emails/usernames/phones only; passwords stay hash-only). Good — that matches the plan's security model.

Now let me see how the schema and FTS5 tables are actually declared so breach_items/breach_fts match conventions.

  Ran 2 shell commands

The existing FTS tables are already unicode61 — MeCab enters as a pre-segmentation step in C (fts_segment inserts spaces into Japanese text before it hits unicode61). So for breach data I simply skip that pre-segmentation