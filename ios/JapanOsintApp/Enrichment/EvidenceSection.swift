import SwiftUI

// ─────────────────────────────────────────────────────────────────────────────
// Roadmap 17 — chain of custody for one intel item.
//
// Lists the evidence captures behind an item (`GET /api/intel/items/:uid/
// evidence`) and shows whether the append-only hash chain still verifies
// (`GET /api/evidence/verify`).
//
// Two behaviours here are not cosmetic:
//
//   • `blob_present == false` means the LRU reaper unlinked the stored bytes —
//     "EVICTED BY RETENTION", not "missing". The ROW surviving is the point:
//     deleting it would break the hash chain and be indistinguishable from
//     tampering. The UI says so in words, because a row that merely looked
//     empty would read as data loss.
//
//   • The raw capture is operator-gated and is UNTRUSTED third-party bytes.
//     It is offered as a file to download/share, never rendered inline — not
//     as HTML, not as an image, not as an attributed string. The download
//     sheet is deliberately the only way to it.
//
// Self-contained: takes the item uid, fetches its own data, and navigates
// nowhere.
// ─────────────────────────────────────────────────────────────────────────────

struct EvidenceSection: View {
    let itemUid: String
    var title: String = "Chain of custody"
    /// The verify endpoint checks the WHOLE chain, not just this item, so a
    /// host that already shows a global verdict can switch it off here.
    var verifiesChain: Bool = true

    @EnvironmentObject private var apiClient: APIClient
    @Environment(\.theme) private var theme

    @State private var records: [EvidenceRecord] = []
    @State private var verify: EvidenceVerifyResult?
    @State private var loading = false
    @State private var verifying = false
    @State private var error: String?
    @State private var verifyError: String?
    @State private var copiedId: String?
    @State private var downloadingId: String?
    @State private var downloadError: String?
    @State private var download: EvidenceDownload?

    var body: some View {
        VStack(alignment: .leading, spacing: Space.md) {
            header

            if loading && records.isEmpty {
                ProgressView()
                    .frame(maxWidth: .infinity, alignment: .center)
                    .padding(.vertical, Space.lg)
            } else if let error {
                errorBanner(error, retry: { Task { await reload() } })
            } else if records.isEmpty {
                emptyState
            } else {
                ForEach(records) { record in
                    row(record)
                }
            }

            if let downloadError {
                errorBanner(downloadError, retry: nil)
            }
        }
        .task(id: itemUid) { await reload() }
        .sheet(item: $download) { d in
            EvidenceDownloadSheet(download: d)
        }
    }

    // MARK: - Header + chain verdict

    private var header: some View {
        HStack(spacing: Space.sm) {
            Image(systemName: "checkmark.shield")
                .font(.subheadline)
                .foregroundStyle(theme.accentAlt)
            Text(title)
                .font(.headline)
                .foregroundStyle(theme.text)
            Spacer(minLength: 0)
            chainBadge
        }
    }

    @ViewBuilder
    private var chainBadge: some View {
        if !verifiesChain {
            EmptyView()
        } else if verifying && verify == nil {
            ProgressView().controlSize(.mini)
        } else if let v = verify {
            VStack(alignment: .trailing, spacing: Space.xs) {
                if v.ok {
                    Pill(text: "CHAIN VERIFIED", tone: .success,
                         icon: "checkmark.seal.fill", maxWidth: 170)
                } else {
                    Pill(text: "CHAIN BROKEN", tone: .danger,
                         icon: "exclamationmark.triangle.fill", maxWidth: 170)
                }
                Text(chainDetail(v))
                    .font(.caption2.monospacedDigit())
                    .foregroundStyle(v.ok ? theme.textMuted : theme.danger)
                    .lineLimit(2)
                    .multilineTextAlignment(.trailing)
            }
        } else if verifyError != nil {
            Pill(text: "UNVERIFIED", tone: .neutral,
                 icon: "questionmark.circle", maxWidth: 140)
        }
    }

    private func chainDetail(_ v: EvidenceVerifyResult) -> String {
        var parts: [String] = []
        if let n = v.checked { parts.append("\(n) records checked") }
        if !v.ok, let at = v.broken_at { parts.append("broken at seq \(at)") }
        if !v.ok, let reason = v.reason, !reason.isEmpty { parts.append(reason) }
        return parts.joined(separator: " · ")
    }

    // MARK: - Row

    private func row(_ r: EvidenceRecord) -> some View {
        VStack(alignment: .leading, spacing: Space.xs) {
            // Capture time + HTTP status.
            HStack(spacing: Space.sm) {
                Pill(text: evidenceStatusText(r.response_status),
                     tone: evidenceStatusTone(r.response_status),
                     maxWidth: 90)
                Text(evidenceAbsoluteTime(r.captured_at))
                    .font(.caption.monospacedDigit())
                    .foregroundStyle(theme.text)
                Text(relativeTime(r.captured_at))
                    .font(.caption2.monospacedDigit())
                    .foregroundStyle(theme.textMuted)
                Spacer(minLength: 0)
                if let seq = r.chain_seq {
                    Text("#\(seq)")
                        .font(.caption2.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                }
            }

            // What was fetched.
            if let url = r.request_url, !url.isEmpty {
                Text("\(r.request_method ?? "GET")  \(url)")
                    .font(.caption2.monospaced())
                    .foregroundStyle(theme.textMuted)
                    .lineLimit(2)
                    .truncationMode(.middle)
            }

            HStack(spacing: Space.sm) {
                Text(r.content_type ?? "unknown type")
                    .font(.caption2.monospaced())
                    .foregroundStyle(theme.textMuted)
                    .lineLimit(1)
                Text(evidenceByteText(r.content_bytes))
                    .font(.caption2.monospacedDigit())
                    .foregroundStyle(theme.textMuted)
                Text(r.source_id)
                    .font(.caption2.monospaced())
                    .foregroundStyle(theme.textMuted)
                    .lineLimit(1)
                Spacer(minLength: 0)
            }

            hashRow(r)
            blobRow(r)
        }
        .padding(Space.md)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(theme.surfaceElevated,
                    in: RoundedRectangle(cornerRadius: Radius.md))
    }

    private func hashRow(_ r: EvidenceRecord) -> some View {
        HStack(spacing: Space.sm) {
            Text("sha256")
                .font(.caption2.monospaced())
                .foregroundStyle(theme.textMuted)
            Text(evidenceShortHash(r.content_sha256))
                .font(.caption2.monospaced())
                .foregroundStyle(theme.text)
                .textSelection(.enabled)
                .lineLimit(1)
            Button {
                Clipboard.copy(r.content_sha256)
                Haptics.success()
                copiedId = r.id
                Task {
                    try? await Task.sleep(nanoseconds: 1_500_000_000)
                    if copiedId == r.id { copiedId = nil }
                }
            } label: {
                Image(systemName: copiedId == r.id ? "checkmark" : "doc.on.doc")
                    .font(.caption2)
            }
            .buttonStyle(.plain)
            .foregroundStyle(copiedId == r.id ? theme.success : theme.accent)
            .accessibilityLabel("Copy full SHA-256")
            Spacer(minLength: 0)
        }
    }

    @ViewBuilder
    private func blobRow(_ r: EvidenceRecord) -> some View {
        if r.blob_present == false {
            HStack(alignment: .top, spacing: Space.sm) {
                Pill(text: "EVICTED", tone: .warning, icon: "clock.arrow.circlepath",
                     maxWidth: 110)
                Text("Stored bytes were evicted by retention. The record and its hash remain, so the chain is still intact and still verifiable — this is not missing evidence.")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
                    .fixedSize(horizontal: false, vertical: true)
            }
            .padding(.top, Space.xs)
        } else {
            HStack(spacing: Space.sm) {
                Button {
                    Task { await fetchRaw(r) }
                } label: {
                    if downloadingId == r.id {
                        ProgressView().controlSize(.mini)
                    } else {
                        Label("Download raw capture", systemImage: "arrow.down.doc")
                            .font(.caption2)
                    }
                }
                .buttonStyle(.bordered)
                .controlSize(.small)
                .disabled(downloadingId != nil)
                Text("Untrusted bytes — saved as a file, never rendered in the app.")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
                    .fixedSize(horizontal: false, vertical: true)
                Spacer(minLength: 0)
            }
            .padding(.top, Space.xs)
        }
    }

    // MARK: - Empty / error

    private var emptyState: some View {
        ContentUnavailableView {
            Label {
                Text("No captures").foregroundStyle(theme.text)
            } icon: {
                Image(systemName: "shield.slash").foregroundStyle(theme.textMuted)
            }
        } description: {
            Text("This item has no evidence capture. Only sources with capture enabled record the raw response and enter the hash chain.")
                .foregroundStyle(theme.textMuted)
        }
    }

    private func errorBanner(_ message: String, retry: (() -> Void)?) -> some View {
        VStack(alignment: .leading, spacing: Space.sm) {
            HStack(alignment: .top, spacing: Space.sm) {
                Image(systemName: "exclamationmark.triangle.fill")
                    .font(.caption)
                    .foregroundStyle(theme.danger)
                Text(message)
                    .font(.caption)
                    .foregroundStyle(theme.text)
                    .fixedSize(horizontal: false, vertical: true)
            }
            if let retry {
                Button {
                    retry()
                } label: {
                    Label("Retry", systemImage: "arrow.clockwise")
                }
                .buttonStyle(.bordered)
                .controlSize(.small)
                .disabled(loading)
            }
        }
        .padding(Space.md)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(theme.danger.opacity(0.12),
                    in: RoundedRectangle(cornerRadius: Radius.md))
    }

    // MARK: - Load

    private func reload() async {
        loading = true
        defer { loading = false }
        do {
            records = try await apiClient.api.evidence(forItem: itemUid)
            error = nil
        } catch {
            self.error = error.localizedDescription
            Haptics.error()
        }
        if verifiesChain && !records.isEmpty {
            await runVerify()
        }
    }

    /// The chain verdict is informative, not fatal: a verify failure leaves the
    /// records listed and downgrades the badge to "unverified" rather than
    /// blanking the section. The error is still surfaced, in the badge.
    private func runVerify() async {
        verifying = true
        defer { verifying = false }
        do {
            verify = try await apiClient.api.evidenceVerify()
            verifyError = nil
            if verify?.ok == false { Haptics.warning() }
        } catch {
            verify = nil
            verifyError = error.localizedDescription
        }
    }

    private func fetchRaw(_ r: EvidenceRecord) async {
        downloadingId = r.id
        downloadError = nil
        defer { downloadingId = nil }
        do {
            let data = try await apiClient.api.evidenceRaw(r.id)
            let name = evidenceFileName(id: r.id, contentType: r.content_type)
            let url = FileManager.default.temporaryDirectory
                .appendingPathComponent(name)
            try data.write(to: url, options: .atomic)
            download = EvidenceDownload(id: r.id, url: url, bytes: data.count,
                                        contentType: r.content_type,
                                        sha256: r.content_sha256)
            Haptics.success()
        } catch {
            downloadError = "Could not fetch the raw capture — \(error.localizedDescription) Operator access is required for this endpoint."
            Haptics.error()
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// MARK: - Raw download
// ─────────────────────────────────────────────────────────────────────────────

/// A capture written to a temporary file, ready to hand to the share sheet.
/// Modelled as a value so the sheet is driven by `.sheet(item:)` and cannot be
/// presented without a file behind it.
struct EvidenceDownload: Identifiable {
    let id: String
    let url: URL
    let bytes: Int
    let contentType: String?
    let sha256: String
}

/// Terminal for the raw bytes. It shows what the file IS and hands it to the
/// system share sheet; it never displays the content. Rendering a captured
/// page inline would execute a third party's markup inside an analyst tool.
private struct EvidenceDownloadSheet: View {
    let download: EvidenceDownload

    @Environment(\.theme) private var theme
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            VStack(alignment: .leading, spacing: Space.lg) {
                HStack(alignment: .top, spacing: Space.sm) {
                    Image(systemName: "exclamationmark.shield.fill")
                        .foregroundStyle(theme.warning)
                    Text("This is raw third-party content captured from the network. It is untrusted: open it only in a tool you would trust with a hostile file.")
                        .font(.callout)
                        .foregroundStyle(theme.text)
                        .fixedSize(horizontal: false, vertical: true)
                }
                .padding(Space.md)
                .background(theme.warning.opacity(0.12),
                            in: RoundedRectangle(cornerRadius: Radius.md))

                VStack(alignment: .leading, spacing: Space.xs) {
                    detail("File", download.url.lastPathComponent)
                    detail("Type", download.contentType ?? "unknown")
                    detail("Size", evidenceByteText(download.bytes))
                    detail("SHA-256", evidenceShortHash(download.sha256))
                }

                ShareLink(item: download.url) {
                    Label("Save or share file", systemImage: "square.and.arrow.up")
                }
                .buttonStyle(.borderedProminent)
                .compatLargeControl()

                Spacer(minLength: 0)
            }
            .padding(Space.lg)
            .frame(maxWidth: .infinity, alignment: .leading)
            .themedScreenBackground(theme)
            .navigationTitle("Raw capture")
            .compatInlineTitle()
            .toolbar {
                ToolbarItem(placement: .compatLeading) {
                    Button("Close") { dismiss() }
                }
            }
        }
        .compatSheetSizing(.compact)
    }

    private func detail(_ label: String, _ value: String) -> some View {
        HStack(alignment: .top, spacing: Space.sm) {
            Text(label)
                .font(.caption.bold())
                .foregroundStyle(theme.textMuted)
                .frame(width: 78, alignment: .leading)
            Text(value)
                .font(.caption.monospaced())
                .foregroundStyle(theme.text)
                .textSelection(.enabled)
            Spacer(minLength: 0)
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// MARK: - File-private helpers
// ─────────────────────────────────────────────────────────────────────────────

/// Head + tail of the digest. Enough to eyeball against another system, short
/// enough to sit on one line; the copy button always yields the full value.
private func evidenceShortHash(_ sha: String) -> String {
    guard sha.count > 24 else { return sha }
    return "\(sha.prefix(12))…\(sha.suffix(6))"
}

private func evidenceByteText(_ n: Int?) -> String {
    guard let n else { return "— bytes" }
    return ByteCountFormatter.string(fromByteCount: Int64(n), countStyle: .file)
}

private func evidenceStatusText(_ status: Int?) -> String {
    guard let status else { return "NO STATUS" }
    return "HTTP \(status)"
}

private func evidenceStatusTone(_ status: Int?) -> Pill.Tone {
    guard let status else { return .neutral }
    switch status {
    case 200..<300: return .success
    case 300..<400: return .info
    case 400..<500: return .warning
    default:        return .danger
    }
}

private func evidenceAbsoluteTime(_ iso: String) -> String {
    guard let d = evidenceDate(iso) else { return iso }
    return d.formatted(date: .abbreviated, time: .standard)
}

private func evidenceDate(_ s: String) -> Date? {
    let withFraction = ISO8601DateFormatter()
    withFraction.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
    if let d = withFraction.date(from: s) { return d }
    let plain = ISO8601DateFormatter()
    plain.formatOptions = [.withInternetDateTime]
    return plain.date(from: s)
}

/// Extension chosen so the file does NOT invite a renderer. Captured HTML
/// deliberately keeps `.bin`: naming it `.html` would encourage double-clicking
/// it straight into a browser, which is exactly the execution of untrusted
/// markup this whole path exists to avoid.
private func evidenceFileName(id: String, contentType: String?) -> String {
    let safeId = id.replacingOccurrences(of: "/", with: "_")
        .replacingOccurrences(of: ":", with: "_")
        .replacingOccurrences(of: "|", with: "_")
    let type = (contentType ?? "").lowercased()
    let ext: String
    if type.contains("json") {
        ext = "json"
    } else if type.contains("xml") || type.contains("rss") || type.contains("atom") {
        ext = "xml"
    } else if type.contains("csv") {
        ext = "csv"
    } else if type.hasPrefix("text/plain") {
        ext = "txt"
    } else {
        ext = "bin"
    }
    return "evidence-\(safeId).\(ext)"
}
