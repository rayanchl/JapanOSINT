# s_docs: CLAUDE.md cut and docs prune (2026-11-16)

Branch: worktree-agent-a87085a48879c0c5b (from claude/peaceful-ritchie-i7e0hj @ 2690d21). Not pushed.
Commits: f38cc7a (Task 1), 41b9db0 (Task 2).

## Task 1
New CLAUDE.md: 24,821 bytes (was 328,316; 4,739 lines). Outline: header + registered count 105,229 (measured 2026-11-16; make source-count / registry-floor.txt authoritative) | 1 never fabricate | 2 never discard | 3 reachable | 4 / 4b / 4c / 4d | Engine subtleties for row authors | Process rules (incl. SEC_CONTACT credential) | Commands | Where things live | Cadence policy | Source pages API and per-source settings | Embeddings, thresholds, tenancy, CORS | Pointers. Each section ends with a History pointer line.

History files (docs/history/, 14, ~332 KB, verbatim line-range splits of the old file, 2-line header comment each): batches-and-tooling 47,817; cadence-and-scheduling 8,355; ci-and-tests 11,849; embeddings-and-thresholds 11,924; engine-row-authoring-history 86,502; infra-audit 3,931; manifest-parser-and-tls 11,934; rule1-fetch-failure-disclosure 22,201; rule2-exhaustiveness-and-paging 45,117; rule3-reachability 5,696; rule4-emit-store-identity 24,520; source-count-ledger-notes 31,760; source-pages-api-and-settings 10,785; tls-and-agents 9,442. Every old line is in exactly one file (coverage asserted).

Verbatim check (normalised whitespace, old CLAUDE.md from HEAD): 389 paragraphs, 0 missing from history or new CLAUDE.md. Path check of new CLAUDE.md: all cited repo paths exist except docs/DOCS_PRUNE_2026-11-16.md (created in commit 2) and glob/ellipsis forms (candidate-sources-batch*, hp-engine).

## Task 2
docs/ before: 802 files, 149.71 MB (not 743/125 MB; counted by git ls-files at f38cc7a, which already includes the 14 history files). Removed 81 files, 1.72 MB. After: 721 files + report = 722 on disk, ~148.0 MB (du reports 143M).
By reason: batch39 scratch dirs (cn39/jp39/tbl39_scratch, unreferenced scripts and scratch TSVs); batch40/scripts helper .py/.sh; batch40 *_ids.txt working lists; batch32 intermediate discovery/selection dumps and generator helpers (16 files); one unreferenced WSL scratch tar.gz. Exact table with per-file size/reason/recovery commit f38cc7ad4e7c0d5db0e6b8346236e71bba94d4ef in docs/DOCS_PRUNE_2026-11-16.md.
Post-removal check: 0 references to any removed path/basename in CLAUDE.md, README.md, docs md, native/, client/, ios/, .github/. native/tools gen_hp_batch, probe_hp_batch, batch_exclusions, audit_batch_emit, lint_sources run --help fine.

## Unsure / not done
- The pass was conservative: ~114 MB of docs are kept only because native/ C comments cite them or because they are batch evidence (candidate manifests, verified/rejected TSVs, registry-health sweeps 3.5-3.9 MB each). No file over 1 MB failed every test, so no large file was removed. Older sweep outputs superseded by a later file of the same series (e.g. registry-health-sweep-2026-10-22.tsv, cited from OPEN_ITEMS) were kept because they are referenced.
- docs/batch{30,32,39,40}/existing_{ids,urls}.txt are dumps but each BRIEF.md says to read them, so kept.
- Reference test is token/basename based, so a few unrelated files may be kept by a coincidental basename (conservative direction).
- The new CLAUDE.md is a rewrite; I trimmed to meet 25 KB and may have compressed some engine-subtlety nuance (e.g. ODataFeed/CBS note, bespoke-collector details); all nuance remains in history. Not verified: the cited 'make' targets were not run (no code changes, no binary).
- Another in-flight change to native/tools and native/core was not merged here; path checks are against this worktree.
