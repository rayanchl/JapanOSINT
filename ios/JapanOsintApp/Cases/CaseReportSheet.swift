import SwiftUI

// ─────────────────────────────────────────────────────────────────────────────
// Cases (roadmap items 14 + 16) — export a case as a document.
//
// POST /api/cases/:id/report is a STREAMED, non-JSON response, which is why
// `API.caseReport` hands back raw `Data` instead of a decoded type. That Data is
// written to a temp file and handed to `ShareLink`, because a share sheet wants
// a file with a name and an extension, not a string in memory.
//
// Two things the server contract pins down and this sheet honours (reportapi.h):
//  · Section ORDER is fixed. The request's ordering is ignored on purpose — a
//    report with the citations before the findings is not a report — so the
//    toggles below only choose membership, never sequence.
//  · Sending an EMPTY section list means "all six", not "none". So when every
//    section is on we send nil (the same thing, one less field), and when none
//    is on we refuse to send at all rather than silently produce a full report
//    the user just told us they didn't want.
//
// There is no PDF format, and that is deliberate rather than missing: the server
// links no PDF engine. Export HTML and print it if you need one.
// ─────────────────────────────────────────────────────────────────────────────

struct CaseReportSheet: View {
    let caseId: String
    let caseName: String

    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme
    @Environment(\.dismiss) private var dismiss

    /// The document order the server emits. Never reordered.
    static let sectionOrder: [String] = ["header", "summary", "findings",
                                         "entities", "citations", "audit"]

    @State private var format: String = "md"
    @State private var enabled: Set<String> = Set(CaseReportSheet.sectionOrder)
    @State private var generating = false
    @State private var error: String?
    @State private var fileURL: URL?
    @State private var byteCount: Int = 0

    private var selected: [String] {
        CaseReportSheet.sectionOrder.filter { enabled.contains($0) }
    }

    var body: some View {
        NavigationStack {
            Form {
                formatSection
                sectionsSection
                outputSection
                if let error {
                    Section {
                        Text(error)
                            .font(.caption)
                            .foregroundStyle(theme.danger)
                    }
                }
            }
            .compatGroupedForm()
            .themedScreenBackground(theme)
            .navigationTitle("Export report")
            .compatInlineTitle()
            .toolbar {
                ToolbarItem(placement: .compatLeading) {
                    Button("Close") { dismiss() }
                        .disabled(generating)
                }
            }
        }
        .compatSheetSizing(.form)
    }

    // MARK: - Format

    private var formatSection: some View {
        Section {
            Picker("Format", selection: $format) {
                Text("Markdown").tag("md")
                Text("HTML").tag("html")
            }
            .pickerStyle(.segmented)
            .onChange(of: format) { _, _ in invalidate() }
        } header: {
            Text("Format")
        } footer: {
            Text(format == "html"
                 ? "A standalone, self-contained HTML file. Analyst-authored markdown inside notes and the summary is shown verbatim and inert — never rendered as live markup."
                 : "Markdown is the primary format. There is no PDF: the server links no PDF engine on purpose. Export HTML and print it if you need one.")
        }
    }

    // MARK: - Sections

    private var sectionsSection: some View {
        Section {
            ForEach(CaseReportSheet.sectionOrder, id: \.self) { name in
                Toggle(isOn: binding(for: name)) {
                    VStack(alignment: .leading, spacing: 1) {
                        Text(Self.sectionTitle(name))
                            .font(.subheadline)
                            .foregroundStyle(theme.text)
                        Text(Self.sectionBlurb(name))
                            .font(.caption2)
                            .foregroundStyle(theme.textMuted)
                    }
                }
            }
        } header: {
            Text("Sections")
        } footer: {
            Text(enabled.isEmpty
                 ? "Pick at least one section. An empty selection means “everything” to the server, so we won't send one — it would hand you the opposite of what you asked for."
                 : "The document always follows the order above; the server ignores any reordering. The title and classification banner are emitted regardless of what is selected.")
                .font(.caption2)
        }
    }

    private func binding(for name: String) -> Binding<Bool> {
        Binding(
            get: { enabled.contains(name) },
            set: { on in
                if on { enabled.insert(name) } else { enabled.remove(name) }
                invalidate()
            })
    }

    // MARK: - Output

    @ViewBuilder
    private var outputSection: some View {
        Section {
            if generating {
                HStack(spacing: Space.md) {
                    ProgressView()
                    VStack(alignment: .leading, spacing: 1) {
                        Text("Building the report…")
                            .font(.subheadline)
                            .foregroundStyle(theme.text)
                        Text("Large cases take a few seconds — the server walks findings, notes, entities and the audit trail.")
                            .font(.caption2)
                            .foregroundStyle(theme.textMuted)
                    }
                }
                .padding(.vertical, Space.xs)
            } else {
                Button {
                    Task { await generate() }
                } label: {
                    Label(fileURL == nil ? "Generate report" : "Regenerate",
                          systemImage: "doc.richtext")
                }
                .disabled(enabled.isEmpty)
            }

            if let fileURL {
                ShareLink(item: fileURL) {
                    Label("Share \(fileURL.lastPathComponent)",
                          systemImage: "square.and.arrow.up")
                }
                HStack(spacing: Space.xs) {
                    Pill(text: format.uppercased(), tone: .accent, maxWidth: 80)
                    Text("\(byteCount) bytes")
                        .font(.caption2.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                    Text("· \(selected.count) of \(CaseReportSheet.sectionOrder.count) sections")
                        .font(.caption2)
                        .foregroundStyle(theme.textMuted)
                    Spacer(minLength: 0)
                }
            }
        } header: {
            Text("Document")
        } footer: {
            Text("Findings render the snapshot frozen at pin time, never a live re-read — that is what makes the document evidence of what you saw rather than of what the source says today.")
                .font(.caption2)
        }
    }

    // MARK: - Actions

    /// Any option change throws the previous document away. Sharing a file that
    /// no longer matches the toggles on screen is how a wrong report gets sent.
    private func invalidate() {
        fileURL = nil
        byteCount = 0
    }

    private func generate() async {
        guard !enabled.isEmpty else { return }
        generating = true
        defer { generating = false }
        fileURL = nil
        do {
            let sections: [String]? =
                selected.count == CaseReportSheet.sectionOrder.count ? nil : selected
            let data = try await apiClient.api.caseReport(caseId,
                                                          format: format,
                                                          sections: sections)
            let ext = (format == "html") ? "html" : "md"
            let name = "case-\(Self.slug(caseName))-\(Self.stamp()).\(ext)"
            let url = FileManager.default.temporaryDirectory
                .appendingPathComponent(name)
            try data.write(to: url, options: .atomic)
            fileURL = url
            byteCount = data.count
            error = nil
            Haptics.success()
        } catch let e {
            error = e.localizedDescription
            Haptics.error()
        }
    }

    // MARK: - Copy

    private static func sectionTitle(_ name: String) -> String {
        switch name {
        case "header":    return "Header"
        case "summary":   return "Executive summary"
        case "findings":  return "Findings"
        case "entities":  return "Entities"
        case "citations": return "Citations"
        case "audit":     return "Audit excerpt"
        default:          return name
        }
    }

    private static func sectionBlurb(_ name: String) -> String {
        switch name {
        case "header":    return "Name, status, priority, author, generated-at, workspace"
        case "summary":   return "The case summary field, verbatim"
        case "findings":  return "Pinned items in pin order, with their notes"
        case "entities":  return "Entities referenced by the pinned items"
        case "citations": return "Source, link and capture time (plus content hash if evidence exists)"
        case "audit":     return "Audit events whose target is this case"
        default:          return name
        }
    }

    /// Filename slug: lowercase alphanumerics and hyphens only, so nothing from
    /// a case name can reach the filesystem or a Content-Disposition header.
    private static func slug(_ raw: String) -> String {
        var out = ""
        var lastDash = false
        for ch in raw.lowercased() {
            if ch.isLetter || ch.isNumber, ch.isASCII {
                out.append(ch)
                lastDash = false
            } else if !lastDash {
                out.append("-")
                lastDash = true
            }
            if out.count >= 40 { break }
        }
        let trimmed = out.trimmingCharacters(in: CharacterSet(charactersIn: "-"))
        return trimmed.isEmpty ? "case" : trimmed
    }

    private static func stamp() -> String {
        let f = DateFormatter()
        f.locale = Locale(identifier: "en_US_POSIX")
        f.timeZone = TimeZone(identifier: "UTC")
        f.dateFormat = "yyyyMMdd-HHmmss"
        return f.string(from: Date())
    }
}
