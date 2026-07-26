import SwiftUI

// ─────────────────────────────────────────────────────────────────────────────
// Cases (roadmap item 14) — the "Add to case" sheet.
//
// Reachable from anywhere: hand it a `refType` / `refId` / optional `label` and
// it pins that reference into whichever case the analyst taps. This is the most
// used surface in the feature, so it is built to be fast and forgiving:
//
//  · It paints instantly from `CaseStore.cases` (already in memory) and then
//    replaces that seed with its OWN unfiltered fetch. It deliberately does not
//    call `store.setFilter(nil)`, which would yank the Cases tab's status
//    segment out from under the user just because they pinned something.
//  · The active case sorts first, open cases before closed/archived.
//  · "New case…" creates and pins in one action — the common case of "this
//    doesn't belong anywhere yet" shouldn't mean cancel, switch tabs, create,
//    come back.
//  · A failed pin keeps the sheet open with the reason on screen. Only a
//    confirmed pin gets the success haptic and the dismiss.
//
// The snapshot is synthesized server-side (casesapi.h): we send only the
// reference, and the server freezes a display copy at pin time.
// ─────────────────────────────────────────────────────────────────────────────

struct CasePickerSheet: View {
    /// One of `CaseRefType.all` — anything else is a 400 from the server.
    let refType: String
    let refId: String
    /// Optional analyst-supplied caption. Also the fallback the Findings list
    /// renders when the server could not resolve a snapshot.
    var label: String? = nil
    /// Fires after a confirmed pin, before dismissal.
    var onPinned: ((CaseSummary) -> Void)? = nil

    @EnvironmentObject var apiClient: APIClient
    @EnvironmentObject var store: CaseStore
    @Environment(\.theme) private var theme
    @Environment(\.dismiss) private var dismiss

    @State private var rows: [CaseSummary] = []
    @State private var loading = false
    @State private var query = ""
    @State private var error: String?
    @State private var pinningId: String?
    @State private var showNewField = false
    @State private var newName = ""
    @State private var creating = false
    @FocusState private var newFieldFocused: Bool

    private var trimmedNewName: String {
        newName.trimmingCharacters(in: .whitespacesAndNewlines)
    }

    var body: some View {
        NavigationStack {
            List {
                referenceSection
                createSection
                if let error {
                    Section {
                        Text(error)
                            .font(.caption)
                            .foregroundStyle(theme.danger)
                    }
                    .listRowBackground(theme.surface)
                }
                casesSection
            }
            .compatInsetGroupedListStyle()
            .themedScreenBackground(theme)
            .navigationTitle("Add to case")
            .compatInlineTitle()
            .searchable(text: $query, placement: .compatDrawer, prompt: "Search cases")
            .toolbar {
                ToolbarItem(placement: .compatLeading) {
                    Button("Cancel") { dismiss() }
                }
                ToolbarItem(placement: .compatPrimary) {
                    Button { Task { await reload() } } label: {
                        Image(systemName: "arrow.clockwise")
                    }
                    .disabled(loading)
                }
            }
            .task { await seedAndReload() }
        }
        .compatSheetSizing(.large)
    }

    // MARK: - Sections

    private var referenceSection: some View {
        Section {
            VStack(alignment: .leading, spacing: 3) {
                HStack(spacing: Space.sm) {
                    Image(systemName: CaseRefType.icon(refType))
                        .font(.caption)
                        .foregroundStyle(theme.accent)
                    Text(label ?? refId)
                        .font(.subheadline.weight(.semibold))
                        .foregroundStyle(theme.text)
                        .lineLimit(2)
                }
                Text("\(refType) · \(refId)")
                    .font(.caption2.monospaced())
                    .foregroundStyle(theme.textMuted)
                    .lineLimit(1)
                if !CaseRefType.all.contains(refType) {
                    Text("\"\(refType)\" is not one of the reference types this backend accepts (\(CaseRefType.all.joined(separator: ", "))). Pinning it will be refused.")
                        .font(.caption2)
                        .foregroundStyle(theme.danger)
                }
            }
            .padding(.vertical, Space.xs)
        } header: {
            header("Pinning")
        }
        .listRowBackground(theme.surface)
    }

    private var createSection: some View {
        Section {
            if showNewField {
                HStack(spacing: Space.sm) {
                    TextField("New case name", text: $newName)
                        .font(.subheadline)
                        .focused($newFieldFocused)
                        .submitLabel(.done)
                        .onSubmit { Task { await createAndPin() } }
                    Button {
                        Task { await createAndPin() }
                    } label: {
                        if creating {
                            ProgressView().controlSize(.small)
                        } else {
                            Text("Create")
                        }
                    }
                    .buttonStyle(.borderedProminent)
                    .disabled(creating || trimmedNewName.isEmpty)
                }
                .onAppear { newFieldFocused = true }
            } else {
                Button {
                    showNewField = true
                    Haptics.selection()
                } label: {
                    Label("New case…", systemImage: "plus.circle.fill")
                        .foregroundStyle(theme.accent)
                }
            }
        } header: {
            header("Create")
        } footer: {
            Text("Creates the case and pins this reference into it in one step.")
                .font(.caption2)
        }
        .listRowBackground(theme.surface)
    }

    @ViewBuilder
    private var casesSection: some View {
        if !visible.isEmpty {
            Section {
                ForEach(visible) { summary in
                    caseButton(summary)
                }
            } header: {
                header(loading ? "Cases · refreshing" : "Cases · \(visible.count)")
            }
            .listRowBackground(theme.surface)
        } else if loading {
            Section {
                HStack {
                    Spacer(minLength: 0)
                    ProgressView()
                    Spacer(minLength: 0)
                }
                .padding(.vertical, Space.md)
            }
            .listRowBackground(Color.clear)
        } else {
            Section {
                Text(query.trimmingCharacters(in: .whitespaces).isEmpty
                     ? "No cases in this workspace yet — create one above."
                     : "No case matches \"\(query)\".")
                    .font(.caption)
                    .foregroundStyle(theme.textMuted)
                    .padding(.vertical, Space.xs)
            }
            .listRowBackground(theme.surface)
        }
    }

    private func caseButton(_ summary: CaseSummary) -> some View {
        Button {
            Task { await pin(summary) }
        } label: {
            HStack(alignment: .top, spacing: Space.md) {
                Image(systemName: CaseStatus.icon(summary.status))
                    .font(.body)
                    .foregroundStyle(summary.isOpen ? theme.success : theme.textMuted)
                    .frame(width: 24, height: 24)

                VStack(alignment: .leading, spacing: 3) {
                    Text(summary.name)
                        .font(.subheadline.weight(.semibold))
                        .foregroundStyle(theme.text)
                        .lineLimit(2)
                    HStack(spacing: Space.sm - 2) {
                        if store.activeCaseId == summary.id {
                            Pill(text: "ACTIVE", tone: .accent, icon: "target", maxWidth: 90)
                        }
                        if !summary.isOpen {
                            Pill(text: CaseStatus.label(summary.status).uppercased(),
                                 tone: CaseStatus.tone(summary.status),
                                 maxWidth: 110)
                        }
                        if summary.priority > 0 {
                            Pill(text: CasePriority.label(summary.priority).uppercased(),
                                 tone: CasePriority.tone(summary.priority),
                                 maxWidth: 110)
                        }
                        Spacer(minLength: 0)
                    }
                    Text("updated \(caseRelativeTime(summary.updated_at))")
                        .font(.caption2.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                }

                Spacer(minLength: 0)

                if pinningId == summary.id {
                    ProgressView().controlSize(.small)
                } else {
                    Image(systemName: "pin")
                        .font(.caption)
                        .foregroundStyle(theme.textMuted)
                }
            }
            .padding(.vertical, 2)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(pinningId != nil || creating)
    }

    private func header(_ text: String) -> some View {
        Text(text.uppercased())
            .font(Typography.display(10, weight: .semibold))
            .tracking(1.2)
            .foregroundStyle(theme.textMuted)
    }

    // MARK: - Ordering

    /// Active case first, then open cases, then most-recently-updated. Search
    /// matches name, summary and id.
    private var visible: [CaseSummary] {
        let q = query.trimmingCharacters(in: .whitespaces).lowercased()
        let filtered = q.isEmpty ? rows : rows.filter { c in
            if c.name.lowercased().contains(q) { return true }
            if let s = c.summary, s.lowercased().contains(q) { return true }
            return c.id.lowercased().contains(q)
        }
        let active = store.activeCaseId
        return filtered.sorted { a, b in
            let aActive = (a.id == active)
            let bActive = (b.id == active)
            if aActive != bActive { return aActive }
            if a.isOpen != b.isOpen { return a.isOpen }
            return a.updated_at > b.updated_at
        }
    }

    // MARK: - Loading

    private func seedAndReload() async {
        if rows.isEmpty { rows = store.cases }   // instant paint
        await reload()
    }

    /// Deliberately unfiltered (`status: nil`): you must be able to pin into a
    /// closed case, and the Cases tab's status segment is none of this sheet's
    /// business.
    private func reload() async {
        loading = true
        defer { loading = false }
        do {
            let env = try await apiClient.api.cases(status: nil, limit: 200, cursor: nil)
            rows = env.data.sorted { $0.updated_at > $1.updated_at }
            error = nil
        } catch let e {
            error = e.localizedDescription
        }
    }

    // MARK: - Actions

    private func pin(_ summary: CaseSummary) async {
        pinningId = summary.id
        defer { pinningId = nil }
        do {
            try await store.pin(caseId: summary.id,
                                refType: refType,
                                refId: refId,
                                label: label)
            error = nil
            Haptics.success()
            onPinned?(summary)
            dismiss()
        } catch let e {
            // Stay put: dismissing on failure would look exactly like success.
            error = e.localizedDescription
            Haptics.error()
        }
    }

    private func createAndPin() async {
        let name = trimmedNewName
        guard !name.isEmpty, !creating else { return }
        creating = true
        defer { creating = false }
        do {
            let detail = try await store.create(name: name, summary: nil, priority: 0)
            let summary = CaseStore.summary(from: detail)
            rows.insert(summary, at: 0)
            try await store.pin(caseId: detail.id,
                                refType: refType,
                                refId: refId,
                                label: label)
            error = nil
            newName = ""
            showNewField = false
            Haptics.success()
            onPinned?(summary)
            dismiss()
        } catch let e {
            // The case may well have been created before the pin failed — say
            // so rather than letting the user create a duplicate on retry.
            error = "\(e.localizedDescription)\n\nIf the case was created, it is in the list below — tap it to pin."
            Haptics.error()
        }
    }
}
