# Vendored third-party code

Everything here is compiled into `bin/japanosint` from source. Versions below
are read from the version macros in the vendored files themselves.

| Library | Version | Files | Upstream | Licence |
| --- | --- | --- | --- | --- |
| SQLite (amalgamation) | 3.49.2 (`SQLITE_VERSION` in `sqlite3.h`) | `sqlite3.c`, `sqlite3.h` | https://www.sqlite.org/download.html | Public domain |
| sqlite-vec (amalgamation) | v0.1.9 (`SQLITE_VEC_VERSION` in `sqlite-vec.h`) | `sqlite-vec.c`, `sqlite-vec.h` | https://github.com/asg017/sqlite-vec | MIT or Apache-2.0 (`sqlite-vec.LICENSE-MIT`, `sqlite-vec.LICENSE-APACHE`) |
| cJSON | 1.7.18 (`CJSON_VERSION_*` in `cJSON.h`) | `cJSON.c`, `cJSON.h` | https://github.com/DaveGamble/cJSON | MIT |
| Mongoose | 7.21 (`MG_VERSION` in `mongoose.h`) | `mongoose.c`, `mongoose.h` | https://github.com/cesanta/mongoose | **Dual-licensed: GPLv2 or a commercial licence from Cesanta** |

## Licence status

**Mongoose is dual-licensed (GPLv2 / commercial). The project has not recorded a
licence decision**, either for its own code (there is no `LICENSE` file at the
repository root) or for how it satisfies Mongoose's terms. Distributing a
binary that links Mongoose under the GPL path carries GPLv2 obligations for the
combined work; the alternative is a commercial licence from Cesanta. This is
the owner's decision and is deliberately not made here. The audit item is
OPS-03 in `docs/audit-2026-11-14/audit_tooling.md`.

The SQLite, cJSON and sqlite-vec licences are permissive; cJSON's MIT notice is
in the header of `cJSON.c` and no separate notices file ships yet.

## How these are built

`native/Makefile` compiles each as its own rule (not under the tree's
`-Wall -Wextra` flags, since it is not our code), with the hardening flags in
`HARDEN_CFLAGS` so the PIE link works. `sqlite3.c` takes the `SQLITE_DEFS`
(FTS5, RTREE, JSON1, math functions, `THREADSAFE=1`); `sqlite-vec.c` is built
`-DSQLITE_CORE` so it calls the linked SQLite directly.

## How to update one

1. Download the new release from the upstream URL above (for SQLite, the
   amalgamation zip; for sqlite-vec, the amalgamation release asset).
2. Replace the files in this directory and re-check the version macro against the
   table, then update the table.
3. Do not hand-edit vendored sources. If a local patch is unavoidable, record it
   here with the reason.
4. `make` (must stay at 0 warnings for our own code), then `make selftest unit`.
   For SQLite also confirm `PRAGMA compile_options` still shows FTS5/RTREE, and
   run `make fuzz-smoke` if a parser-facing library changed.
5. If the version changed, note it in the commit message.

llama.cpp runtime libraries live in `native/llama/` (not here) and are tracked
separately; see `.gitignore` for the soname-alias workaround.
