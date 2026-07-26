import SwiftUI

// ─────────────────────────────────────────────────────────────────────────────
// Roadmap item 7 — the ONE export surface.
//
// `GET /api/export/:kind?format=…&<the caller's own filters>` is a single
// streaming exporter that consumes the SAME filter vocabulary as the list
// endpoint it mirrors. So this sheet does not invent an export filter model:
// the caller hands over the exact `[URLQueryItem]` it is already using, and we
// forward them verbatim (minus paging params, which an export ignores).
//
// Three server behaviours are surfaced rather than hidden:
//
//  1. PLAN ROW CAP. The server fetches `LIMIT cap+1` so truncation is *known*,
//     never guessed, and reports it in every format — a trailing
//     `# truncated=true; …` comment row in CSV, `meta.export.truncated` in
//     JSON, `properties.truncated` in GeoJSON. `inspect()` reads that back and
//     the result section says the file is incomplete. A silently short export
//     is the failure mode this whole block exists to prevent.
//  2. `kind == "breach"` REQUIRES `?source=`. Breach records are reachable
//     per-source only; a bulk export must not become the way around that. We
//     say so and collect the source here instead of letting the request 400.
//  3. GEOJSON IS INTEL-ONLY. Only the intel projection carries lat/lon/geometry
//     columns; every other kind would stream back a well-formed and completely
//     empty FeatureCollection. The picker hides the option and explains why.
//
// No progress fraction is available: `API.exportRows` returns one `Data` after
// the chunked response completes, so the sheet shows an indeterminate,
// explicitly-labelled wait rather than a fake percentage.
// ─────────────────────────────────────────────────────────────────────────────

/// The three formats `/api/export/:kind` accepts.
enum ExportFormat: String, CaseIterable, Identifiable {
    case csv, geojson, json

    var id: String { rawValue }

    var label: String {
        switch self {
        case .csv:     return "CSV"
        case .geojson: return "GeoJSON"
        case .json:    return "JSON"
        }
    }

    var fileExtension: String { rawValue }

    var blurb: String {
        switch self {
        case .csv:     return "One row per record, UTF-8 with a BOM so Excel opens Japanese text correctly."
        case .geojson: return "A FeatureCollection. Records with no coordinates are dropped."
        case .json:    return "The full record objects, with an export trailer describing the run."
        }
    }
}

/// Turns a thrown `APIError` into the sentence the server actually wrote.
///
/// Every roadmap endpoint fails as `{"error":"…"}` — sometimes a snake_case
/// code, sometimes a full explanation (saved-search → alert writes a whole
/// paragraph). `APIError.http` carries that body verbatim, so showing
/// `error.localizedDescription` would put `HTTP 400: {"error":"…"}` in front of
/// the user. This unwraps it, and upgrades the terse codes to something a
/// person can act on.
enum ServerError {
    static func message(_ error: Error) -> String {
        guard case let APIError.http(code, body) = error else {
            return error.localizedDescription
        }
        if let detail = detail(from: body), !detail.isEmpty {
            return expand(detail) ?? detail
        }
        if body.isEmpty { return "HTTP \(code)" }
        return "HTTP \(code): \(body)"
    }

    /// The `error` string out of a `{"error":"…"}` body, if that's what it is.
    static func detail(from body: String) -> String? {
        guard let data = body.data(using: .utf8),
              let raw = try? JSONSerialization.jsonObject(with: data),
              let obj = raw as? [String: Any],
              let msg = obj["error"] as? String else { return nil }
        return msg
    }

    /// Known snake_case codes → a sentence. Unknown codes fall through and are
    /// shown as the server wrote them, which is better than swallowing them.
    private static func expand(_ code: String) -> String? {
        switch code {
        case "source_required":
            return "A breach export must name one breach source. Breach records are only ever readable per-source."
        case "unknown_kind":
            return "The server does not export that kind of record."
        case "unknown_format":
            return "The server does not produce that format."
        case "kind_unavailable":
            return "This server build cannot export that kind yet."
        case "forbidden":
            return "You do not have permission for this. Bulk breach export needs the analyst role or higher."
        case "not_found":
            return "That record no longer exists."
        case "invalid_kind":
            return "That kind is not one the server accepts."
        case "invalid_value":
            return "The server could not read that as a valid identifier."
        case "value_required":
            return "An identifier is required."
        case "label_too_long":
            return "That label is too long (200 characters maximum)."
        case "invalid_permalink":
            return "That link is not a valid permalink, or it was minted by a newer version of the app."
        case "state_too_large":
            return "This view has too much state to fit in a link."
        case "server_error":
            return "The server hit an internal error. Try again."
        case "tenant_required":
            return "No workspace is selected."
        default:
            return nil
        }
    }
}

/// Reusable export sheet. Present it from any list surface with that surface's
/// live filters; it owns format choice, the request, the temp file and the
/// share sheet.
///
/// ```swift
/// .sheet(isPresented: $showExport) {
///     ExportSheet(kind: "intel", filters: currentFilters,
///                 contextLabel: "Intel · \(sourceName)")
/// }
/// ```
struct ExportSheet: View {
    /// `intel` | `entities` | `breach` | `case` | `alert_events`.
    let kind: String
    /// The caller's current filters, forwarded verbatim. Paging params are
    /// dropped here — the plan row cap is the only limit an export honours.
    var filters: [URLQueryItem] = []
    /// Optional human description of what is being exported ("Intel · NHK").
    var contextLabel: String? = nil

    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme
    @Environment(\.dismiss) private var dismiss

    @State private var format: ExportFormat = .csv
    @State private var breachSource: String = ""
    @State private var running = false
    @State private var error: String?
    @State private var outcome: Outcome?

    /// What one completed export produced.
    struct Outcome {
        let url: URL
        let bytes: Int
        let rows: Int?
        let rowCap: Int?
        let plan: String?
        let truncated: Bool
    }

    // MARK: - Body

    var body: some View {
        NavigationStack {
            Form {
                summarySection
                formatSection
                if kind == "breach" { breachSourceSection }
                filtersSection
                actionSection
                if let outcome { resultSection(outcome) }
                if let error { errorSection(error) }
            }
            .compatGroupedForm()
            .themedScreenBackground(theme)
            .navigationTitle("Export")
            .compatInlineTitle()
            .toolbar {
                ToolbarItem(placement: .compatLeading) {
                    Button("Close") { dismiss() }
                }
            }
            .onAppear {
                if breachSource.isEmpty, let s = callerSource { breachSource = s }
                if !availableFormats.contains(format) {
                    format = availableFormats.first ?? .json
                }
            }
        }
        .compatSheetSizing(.form)
    }

    // MARK: - Sections

    private var summarySection: some View {
        Section {
            LabeledContent("Records") {
                Text(kindLabel)
                    .font(.caption.monospaced())
                    .foregroundStyle(theme.textMuted)
            }
            if let contextLabel, !contextLabel.isEmpty {
                LabeledContent("Scope") {
                    Text(contextLabel)
                        .font(.caption)
                        .foregroundStyle(theme.textMuted)
                        .multilineTextAlignment(.trailing)
                }
            }
        } header: {
            Text("What")
        } footer: {
            Text("Exports run against the same filters as the screen you came from, so what you get is what you were looking at.")
                .font(.caption2)
        }
    }

    private var formatSection: some View {
        Section {
            Picker("Format", selection: $format) {
                ForEach(availableFormats) { f in
                    Text(f.label).tag(f)
                }
            }
            .pickerStyle(.segmented)
            Text(format.blurb)
                .font(.caption2)
                .foregroundStyle(theme.textMuted)
        } header: {
            Text("Format")
        } footer: {
            if kind != "intel" {
                Text("GeoJSON is only offered for intel. No other record type carries coordinates, so a GeoJSON export of \(kindLabel) would be an empty FeatureCollection.")
                    .font(.caption2)
            }
        }
    }

    private var breachSourceSection: some View {
        Section {
            TextField("Breach source ID", text: $breachSource)
                .font(.system(.body, design: .monospaced))
                .compatNoAutocap()
                .autocorrectionDisabled()
            if effectiveSource.isEmpty {
                Label("Required — the export cannot run without it.",
                      systemImage: "exclamationmark.triangle.fill")
                    .font(.caption2)
                    .foregroundStyle(theme.warning)
            }
        } header: {
            Text("Breach source")
        } footer: {
            Text("Breach records are readable one source at a time and never as an open feed, so a bulk export has to name the source too. The export carries the redacted field set only — never the stored SHA-1 of an identifier, and never a secret.")
                .font(.caption2)
        }
    }

    @ViewBuilder
    private var filtersSection: some View {
        Section {
            if forwardedFilters.isEmpty {
                Text("No filters — this exports everything you are allowed to see, up to your plan's row cap.")
                    .font(.caption)
                    .foregroundStyle(theme.textMuted)
            } else {
                ForEach(forwardedFilters, id: \.self) { item in
                    HStack(spacing: Space.sm) {
                        Text(item.name)
                            .font(.caption.monospaced())
                            .foregroundStyle(theme.textMuted)
                        Spacer(minLength: Space.sm)
                        Text(item.value ?? "—")
                            .font(.caption.monospaced())
                            .foregroundStyle(theme.text)
                            .lineLimit(1)
                            .truncationMode(.middle)
                    }
                }
            }
        } header: {
            Text("Filters sent")
        }
    }

    private var actionSection: some View {
        Section {
            Button {
                Task { await runExport() }
            } label: {
                HStack(spacing: Space.sm) {
                    if running { ProgressView().controlSize(.small) }
                    Text(running ? "Exporting…" : "Export")
                        .font(.body.weight(.semibold))
                    Spacer()
                }
            }
            .disabled(!canExport)
            .compatLargeControl()

            if running {
                Text("Streaming rows from the server. Large exports take a while and there is no percentage to show — the server sends one continuous stream and the total is not known until it ends.")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
            }
        }
    }

    private func resultSection(_ o: Outcome) -> some View {
        Section {
            if o.truncated {
                VStack(alignment: .leading, spacing: Space.xs) {
                    Label("This export is incomplete", systemImage: "exclamationmark.triangle.fill")
                        .font(.subheadline.weight(.semibold))
                        .foregroundStyle(theme.warning)
                    Text(truncationExplanation(o))
                        .font(.caption)
                        .foregroundStyle(theme.text)
                }
                .padding(.vertical, Space.xs)
            } else {
                Label("Complete", systemImage: "checkmark.seal.fill")
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(theme.success)
            }

            LabeledContent("File") {
                Text(o.url.lastPathComponent)
                    .font(.caption.monospaced())
                    .foregroundStyle(theme.textMuted)
                    .lineLimit(1)
                    .truncationMode(.middle)
            }
            LabeledContent("Size") {
                Text(ByteCountFormatter.string(fromByteCount: Int64(o.bytes),
                                               countStyle: .file))
                    .font(.caption.monospacedDigit())
                    .foregroundStyle(theme.textMuted)
            }
            if let rows = o.rows {
                LabeledContent("Rows") {
                    Text("\(rows)")
                        .font(.caption.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                }
            }
            if let cap = o.rowCap {
                LabeledContent("Plan row cap") {
                    Text("\(cap)")
                        .font(.caption.monospacedDigit())
                        .foregroundStyle(o.truncated ? theme.warning : theme.textMuted)
                }
            }

            ShareLink(item: o.url) {
                Label("Share file", systemImage: "square.and.arrow.up")
            }
            .compatLargeControl()
        } header: {
            Text("Result")
        } footer: {
            if o.truncated {
                Text("Narrow the filters — a date range or a single source — and export again to get the rest.")
                    .font(.caption2)
            } else if o.rows == 0 {
                Text("The export ran successfully and matched no rows. The file is a valid, empty \(format.label) document.")
                    .font(.caption2)
            }
        }
    }

    private func errorSection(_ message: String) -> some View {
        Section {
            VStack(alignment: .leading, spacing: Space.xs) {
                Label("Export failed", systemImage: "xmark.octagon.fill")
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(theme.danger)
                Text(message)
                    .font(.caption)
                    .foregroundStyle(theme.text)
                    .textSelection(.enabled)
            }
            .padding(.vertical, Space.xs)
            Button {
                Task { await runExport() }
            } label: {
                Label("Try again", systemImage: "arrow.clockwise")
            }
            .disabled(!canExport)
        }
    }

    // MARK: - Derived state

    private var kindLabel: String {
        switch kind {
        case "intel":         return "Intel items"
        case "entities":      return "Entities"
        case "breach":        return "Breach records"
        case "case":          return "Case"
        case "alert_events":  return "Alert events"
        default:              return kind
        }
    }

    private var availableFormats: [ExportFormat] {
        kind == "intel" ? ExportFormat.allCases : [.csv, .json]
    }

    private var callerSource: String? {
        filters.first(where: { $0.name == "source" })?.value
    }

    private var effectiveSource: String {
        let typed = breachSource.trimmingCharacters(in: .whitespaces)
        if !typed.isEmpty { return typed }
        return (callerSource ?? "").trimmingCharacters(in: .whitespaces)
    }

    /// Everything we forward, with paging + format stripped (the server ignores
    /// `limit` for exports and we set `format` ourselves).
    private var forwardedFilters: [URLQueryItem] {
        var q = filters.filter {
            $0.name != "format" && $0.name != "limit" && $0.name != "cursor"
        }
        if kind == "breach" {
            q.removeAll { $0.name == "source" }
            let s = effectiveSource
            if !s.isEmpty { q.append(URLQueryItem(name: "source", value: s)) }
        }
        return q
    }

    private var canExport: Bool {
        if running { return false }
        if kind == "breach" && effectiveSource.isEmpty { return false }
        return true
    }

    private func truncationExplanation(_ o: Outcome) -> String {
        let planPart = o.plan.map { "Your \($0) plan" } ?? "Your plan"
        if let cap = o.rowCap {
            return "\(planPart) caps an export at \(cap) rows and the query matched more than that. Rows beyond the cap are NOT in this file."
        }
        return "\(planPart) caps how many rows one export may contain and the query matched more. Rows beyond the cap are NOT in this file."
    }

    // MARK: - Work

    private func runExport() async {
        running = true
        defer { running = false }
        error = nil
        outcome = nil
        do {
            let data = try await apiClient.api.exportRows(
                kind: kind, format: format.rawValue, query: forwardedFilters)
            let info = Self.inspect(data, format: format)
            let url = try Self.writeTempFile(data, kind: kind,
                                             ext: format.fileExtension)
            outcome = Outcome(url: url, bytes: data.count, rows: info.rows,
                              rowCap: info.rowCap, plan: info.plan,
                              truncated: info.truncated)
            if info.truncated { Haptics.warning() } else { Haptics.success() }
        } catch {
            self.error = ServerError.message(error)
            Haptics.error()
        }
    }

    // MARK: - Temp file

    private static func writeTempFile(_ data: Data, kind: String,
                                      ext: String) throws -> URL {
        let safeKind = kind.filter {
            $0.isASCII && ($0.isLetter || $0.isNumber || $0 == "_" || $0 == "-")
        }
        let name = "\(safeKind.isEmpty ? "export" : safeKind)-\(stamp(Date())).\(ext)"
        let dir = FileManager.default.temporaryDirectory
            .appendingPathComponent("exports", isDirectory: true)
        try FileManager.default.createDirectory(at: dir,
                                                withIntermediateDirectories: true)
        let url = dir.appendingPathComponent(name)
        try data.write(to: url, options: .atomic)
        return url
    }

    /// `20260726T113000Z` — matches the server's own Content-Disposition stamp.
    private static func stamp(_ date: Date) -> String {
        let f = DateFormatter()
        f.locale = Locale(identifier: "en_US_POSIX")
        f.timeZone = TimeZone(identifier: "UTC")
        f.dateFormat = "yyyyMMdd'T'HHmmss'Z'"
        return f.string(from: date)
    }

    // MARK: - Truncation / trailer parsing

    /// The export trailer lives at the END of the payload in all three formats,
    /// so we look at the tail rather than parsing a potentially huge document.
    /// `String(decoding:as:)` never fails — a byte boundary chopped mid-rune
    /// becomes U+FFFD, which is harmless for substring matching.
    private static func inspect(_ data: Data, format: ExportFormat)
        -> (rows: Int?, rowCap: Int?, plan: String?, truncated: Bool) {
        let tailBytes = data.suffix(min(data.count, 8192))
        let tail = String(decoding: tailBytes, as: UTF8.self)
        switch format {
        case .csv:
            return (nil,
                    intAfter("row_cap=", in: tail),
                    quotedAfter("plan=\"", in: tail),
                    tail.contains("# truncated=true"))
        case .json, .geojson:
            return (jsonInt("rows", in: tail),
                    jsonInt("row_cap", in: tail),
                    jsonString("plan", in: tail),
                    tail.contains("\"truncated\":true"))
        }
    }

    private static func jsonInt(_ key: String, in s: String) -> Int? {
        intAfter("\"\(key)\":", in: s)
    }

    private static func jsonString(_ key: String, in s: String) -> String? {
        quotedAfter("\"\(key)\":\"", in: s)
    }

    private static func intAfter(_ needle: String, in s: String) -> Int? {
        guard let r = s.range(of: needle) else { return nil }
        var digits = ""
        for ch in s[r.upperBound...] {
            if ch == " " && digits.isEmpty { continue }
            if ch.isASCII && ch.isNumber { digits.append(ch) } else { break }
        }
        return Int(digits)
    }

    private static func quotedAfter(_ needle: String, in s: String) -> String? {
        guard let r = s.range(of: needle) else { return nil }
        let rest = s[r.upperBound...]
        guard let end = rest.firstIndex(of: "\"") else { return nil }
        return String(rest[rest.startIndex..<end])
    }
}
