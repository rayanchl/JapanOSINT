# iOS: share-queue drain + upload client + widening the share sheet

The backend is built and tested (19 checks, `scratchpad/upload.sh`). This is
the iOS half. Written as a plan because it lands on the Mac, where it can
actually be compiled.

**Sequencing matters and is the point of this document:** the activation rule
widens in the SAME change that adds the upload client. Accepting files before
the app can drain them produces a share-sheet entry that queues something
nothing consumes — the dead-end tap this whole design exists to avoid.

---

## Why the upload is chunked (do not "simplify" this)

`third_party/mongoose.h` sets `MG_MAX_RECV_SIZE` to **3 MB**, and mongoose
buffers an entire request body in the event loop before the handler runs.
A single-shot `POST /file` would therefore either cap attachments at a couple
of megabytes or raise the per-connection memory ceiling for **every**
connection in the server. So the client splits the file:

```
POST /api/uploads/begin      {filename, content_type, total_bytes}
                              → {upload_id, part_max_bytes}
POST /api/uploads/:id/part?seq=N   raw binary body, ≤ part_max_bytes
POST /api/uploads/:id/commit       → {sha256, bytes, blob_present}
GET  /api/uploads/:id              → {received parts, bytes}   (resume)
DELETE /api/uploads/:id            → abandon
```

`part_max_bytes` comes from the server (1 MiB default) — **read it from the
`begin` response, never hardcode it.** Parts are idempotent on `(upload_id,
seq)`, so a retry is safe and `received_bytes` cannot drift.

---

## Step 1 — `UploadClient` (new file: `ios/JapanOsintApp/Uploads/UploadClient.swift`)

```swift
@MainActor final class UploadClient: ObservableObject {
    struct Progress { var sent: Int; var total: Int; var phase: String }
    func upload(fileURL: URL, filename: String, contentType: String,
                onProgress: @escaping (Progress) -> Void) async throws -> String  // sha256
}
```

Implementation notes:
- Read the file with `FileHandle` in `part_max_bytes` slices. **Do not
  `Data(contentsOf:)` a 64 MB file** — that is the whole allocation you were
  trying to avoid, just moved to the phone.
- Send parts sequentially. Parallelism buys little against a single SQLite
  writer and makes resume harder to reason about.
- On failure mid-way, call `GET /api/uploads/:id` and re-send only the missing
  seqs. The endpoint exists precisely for this.
- Surface real progress — `sent/total` is known up front, unlike the export
  sheet where it genuinely is not.
- The API methods do **not** exist in `API+Roadmap.swift` yet. Add:
  `uploadBegin`, `uploadPart` (raw `Data` body — see the caveat below),
  `uploadCommit`, `uploadStatus`, `uploadAbort`.

**Caveat on `uploadPart`:** the existing `post()` helper JSON-encodes and sets
`Content-Type: application/json`. A part body is raw bytes. Add a distinct
`postRaw(_:body:contentType:)` on `API` rather than bending `post()` — the
existing helper is used by ~40 call sites and should not learn a binary mode.

---

## Step 2 — Share-queue drain (`ios/JapanOsintApp/Uploads/ShareIntake.swift`)

`ShareQueue` already exists in `ios/JapanOsintApp/Widgets/SharedSnapshot.swift`
(committed): the extension writes items into the App Group, the app drains
them. What is missing is the drain.

```swift
@MainActor final class ShareIntake: ObservableObject {
    @Published var pending: [SharedInboundItem] = []
    func refresh()                       // ShareQueue.pending()
    func handle(_ item: SharedInboundItem, into caseId: String) async throws
}
```

Rules:
- **Remove from the queue only after the item is fully handled.** `ShareQueue`
  deliberately does not delete on read, so a crash mid-handle cannot lose what
  the user shared.
- `.url` and `.text` → start an OSINT search, or pin to a case. No upload.
- `.image` and (after Step 4) `.file` → `UploadClient`, then
  `POST /api/cases/:id/items {"ref_type":"attachment","ref_id":"upload:<id>"}`.
- If the user is signed out, keep the item queued and show the sign-in. The
  extension has no auth UI by design; this is where that is resolved.

Wire the drain in `JapanOsintApp.swift`: `.onOpenURL` for
`japanosint://share?id=…` plus a check on `scenePhase == .active`, because the
extension's `openURL` can fail and the item must still be picked up.

---

## Step 3 — UI

- **`CasePickerSheet`** already exists and is the natural destination: extend
  it to accept a pending share item so "share → pick case → done" is one flow.
- **Attachment rows in `CaseDetailView`**: `ref_type == "attachment"` renders
  filename, size, SHA-256 (truncated, copyable) and a download action.
  Reuse `EvidenceSection`'s treatment — an attachment IS an evidence blob,
  and the custody framing is correct rather than decorative.
- **Progress**: uploads can take a while on cellular. A determinate bar, and a
  cancel that calls `DELETE /api/uploads/:id` so the server reaps immediately
  instead of waiting out the 24 h idle TTL.

---

## Step 4 — Widen the activation rule (LAST)

Only once Steps 1–3 work. In `ios/JapanOsintShare/Info-activation-snippet.plist`
change:

```xml
<key>NSExtensionActivationSupportsFileWithMaxCount</key>
<integer>0</integer>          <!-- becomes 1 -->
```

Leave `NSExtensionActivationSupportsMovieWithMaxCount` at **0** for now — see
below.

---

## Video: deliberately still excluded, and why

Not a principle, a capacity judgement, and it is being revisited:

- The server-side ffmpeg abstraction is being built now, which removes the
  "no frame extraction" half of the objection.
- What remains is size. `JO_UPLOAD_MAX_BYTES` defaults to **64 MiB**; a phone
  video clears that in well under a minute of 4K. Accepting movies means
  either a much larger ceiling (and a matching
  `JO_EVIDENCE_MAX_BLOB_BYTES`, or bytes are silently skipped — that bug is
  fixed but the constraint is real), or client-side transcode before upload,
  which `AVAssetExportSession` can do.
- The honest interim: accept movies **only** once either the ceiling is raised
  deliberately or the client downscales first. Turning the plist key on
  without one of those produces uploads that fail at `commit` on
  `declared_length_exceeded`, which is exactly the dead-end tap this plan is
  organised to avoid.

---

## Verification checklist

- [ ] `part_max_bytes` is read from `begin`, not hardcoded
- [ ] A 20 MB file uploads and its `sha256` matches `shasum -a 256` locally
- [ ] Killing the app mid-upload and reopening resumes from `GET /:id`
- [ ] Cancel issues `DELETE /api/uploads/:id`
- [ ] A shared item survives force-quit before it is handled (queue not
      cleared on read)
- [ ] Signed out: the item stays queued rather than being dropped
- [ ] `attachment` appears in the case's item-count chips
- [ ] Share sheet offers JapanOSINT for a PDF **after** Step 4, and still not
      for a movie
