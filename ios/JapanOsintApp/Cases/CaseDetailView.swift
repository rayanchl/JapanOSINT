import SwiftUI

// ─────────────────────────────────────────────────────────────────────────────
// Cases (roadmap item 14) — the case workspace.
//
// Four panes over one case: Findings (the pinned evidence set), Notes
// (annotations on those findings), Activity (the server-written history plus
// analyst comments) and Members (the roster).
//
// THE RULE THAT IS NOT COSMETIC: a finding renders its SNAPSHOT, never a live
// re-read. `case_items.snapshot_json` is the display copy frozen at pin time,
// and casesapi.h is explicit about why — the corpora underneath a case get
// re-fetched, re-ingested and merged, so a "helpfully refreshed" finding is a
// forged record. When `snapshot.resolved == false` (the referenced row could
// not be read at pin time) the row says so and falls back to the honest
// `ref_type · ref_id`, because a blank row is indistinguishable from a pin that
// was never made.
//
// Notes are fetched per pinned finding. That is not a stylistic choice:
// `API.annotations(refType:refId:caseId:limit:)` requires a ref pair, so there
// is no single "every note on this case" call to make. It also means a note
// always has something to be about — which is why the composer picks a finding.
// ─────────────────────────────────────────────────────────────────────────────

struct CaseDetailView: View {
    let caseId: String
    /// Shown in the header until `caseDetail` lands, so a push never lands on
    /// a screen titled "Case".
    var initialName: String? = nil

    @EnvironmentObject var apiClient: APIClient
    @EnvironmentObject var store: CaseStore
    @Environment(\.theme) private var theme
    @Environment(\.dismiss) private var dismiss

    enum Pane: String, CaseIterable, Identifiable {
        case findings, notes, activity, members
        var id: String { rawValue }
        var label: String {
            switch self {
            case .findings: return "Findings"
            case .notes:    return "Notes"
            case .activity: return "Activity"
            case .members:  return "Members"
            }
        }
    }

    /// How many findings we will fan out note lookups for. One request per
    /// pinned item is the only shape the annotations client offers; the cap
    /// keeps a 400-item case from firing 400 requests on pane switch.
    private static let noteFanoutCap = 60

    @State private var pane: Pane = .findings

    @State private var detail: CaseDetail?
    @State private var items: [CaseItemRef] = []
    @State private var itemsCursor: String?
    @State private var notes: [Annotation] = []
    @State private var activity: [CaseActivityEntry] = []
    @State private var activityCursor: String?
    @State private var members: [CaseMemberRef] = []

    @State private var loading = false
    @State private var loadingMoreItems = false
    @State private var loadingMoreActivity = false
    @State private var notesLoading = false
    @State private var membersLoading = false

    @State private var error: String?
    /// Success confirmations, in the success colour — kept apart from `error`
    /// so "this is now your active case" doesn't read as a failure.
    @State private var notice: String?

    @State private var showEdit = false
    @State private var showReport = false
    @State private var showDelete = false
    @State private var deleted = false

    @State private var commentText = ""
    @State private var sendingComment = false

    @State private var noteTargetId = ""
    @State private var noteText = ""
    @State private var savingNote = false

    // MARK: - Body

    var body: some View {
        List {
            headerSection
            paneSection
            paneContent
        }
        .compatInsetGroupedListStyle()
        .themedScreenBackground(theme)
        .navigationTitle(detail?.name ?? initialName ?? "Case")
        .compatInlineTitle()
        .refreshable { await loadAll() }
        .toolbar {
            ToolbarItem(placement: .compatPrimary) {
                Menu {
                    Button { showReport = true } label: {
                        Label("Export report…", systemImage: "doc.richtext")
                    }
                    Button { showEdit = true } label: {
                        Label("Edit case…", systemImage: "pencil")
                    }
                    Button {
                        store.activeCaseId = caseId
                        notice = "\"Add to case\" now defaults to this case."
                        error = nil
                        Haptics.selection()
                    } label: {
                        Label("Make active case", systemImage: "target")
                    }
                    Divider()
                    Button(role: .destructive) { showDelete = true } label: {
                        Label("Delete case…", systemImage: "trash")
                    }
                } label: {
                    Image(systemName: "ellipsis.circle")
                }
                .disabled(detail == nil)
            }
            ToolbarItem(placement: .compatPrimary) {
                Button { Task { await loadAll() } } label: {
                    Image(systemName: "arrow.clockwise")
                }
                .disabled(loading)
            }
        }
        .task { if detail == nil { await loadAll() } }
        .onChange(of: pane) { _, newPane in
            if newPane == .members { Task { await loadMembers() } }
        }
        .onChange(of: deleted) { _, isDeleted in
            if isDeleted { dismiss() }
        }
        .sheet(isPresented: $showEdit) {
            if let d = detail {
                CaseEditSheet(detail: d) { updated in
                    detail = updated
                }
            }
        }
        .sheet(isPresented: $showReport) {
            CaseReportSheet(caseId: caseId,
                            caseName: detail?.name ?? initialName ?? caseId)
        }
        .sheet(isPresented: $showDelete) {
            CaseDeleteSheet(
                caseName: detail?.name ?? initialName ?? caseId,
                findingCount: totalPinned,
                onConfirm: {
                    let ok = await store.delete(caseId)
                    if ok { deleted = true }
                    return ok
                })
        }
    }

    private var totalPinned: Int {
        if let counts = detail?.item_counts {
            return counts.values.reduce(0, +)
        }
        return items.count
    }

    // MARK: - Header

    private var headerSection: some View {
        Section {
            VStack(alignment: .leading, spacing: Space.sm) {
                Text(detail?.name ?? initialName ?? caseId)
                    .font(.headline)
                    .foregroundStyle(theme.text)

                if let s = detail?.summary, !s.isEmpty {
                    Text(s)
                        .font(.caption)
                        .foregroundStyle(theme.textMuted)
                        .textSelection(.enabled)
                }

                HStack(spacing: Space.sm - 2) {
                    if let d = detail {
                        Pill(text: CaseStatus.label(d.status).uppercased(),
                             tone: CaseStatus.tone(d.status),
                             icon: CaseStatus.icon(d.status),
                             maxWidth: 130)
                        Pill(text: "P\(d.priority) · \(CasePriority.label(d.priority).uppercased())",
                             tone: CasePriority.tone(d.priority),
                             icon: "exclamationmark.triangle.fill",
                             maxWidth: 170)
                    } else if loading {
                        ProgressView().controlSize(.small)
                    }
                    if store.activeCaseId == caseId {
                        Pill(text: "ACTIVE", tone: .accent, icon: "target", maxWidth: 90)
                    }
                    Spacer(minLength: 0)
                    Button { showEdit = true } label: {
                        Label("Edit", systemImage: "pencil")
                            .font(.caption)
                    }
                    .buttonStyle(.bordered)
                    .disabled(detail == nil)
                }

                HStack(spacing: Space.xs) {
                    Text("created \(caseRelativeTime(detail?.created_at))")
                    Text("·")
                    Text("updated \(caseRelativeTime(detail?.updated_at))")
                    Spacer(minLength: 0)
                }
                .font(.caption2.monospacedDigit())
                .foregroundStyle(theme.textMuted)

                if let closed = detail?.closed_at, !closed.isEmpty {
                    Text("closed \(caseAbsoluteTime(closed))")
                        .font(.caption2.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                }

                countChips

                if let error {
                    Text(error)
                        .font(.caption2)
                        .foregroundStyle(theme.danger)
                }
                if let notice {
                    Text(notice)
                        .font(.caption2)
                        .foregroundStyle(theme.success)
                }
            }
            .padding(.vertical, Space.xs)
        }
        .listRowBackground(theme.surface)
    }

    /// Per-ref_type counts from the detail envelope. The server always sends
    /// all six keys (zeros included), so only the non-zero ones are drawn.
    @ViewBuilder
    private var countChips: some View {
        let counts = detail?.item_counts ?? [:]
        let present = CaseRefType.all.filter { (counts[$0] ?? 0) > 0 }
        if !present.isEmpty {
            HStack(spacing: Space.xs) {
                ForEach(present, id: \.self) { t in
                    Pill(text: "\(counts[t] ?? 0) \(CaseRefType.short(t))",
                         tone: .neutral,
                         icon: CaseRefType.icon(t),
                         maxWidth: 150)
                }
                Spacer(minLength: 0)
            }
        }
    }

    private var paneSection: some View {
        Section {
            Picker("Pane", selection: $pane) {
                ForEach(Pane.allCases) { p in
                    Text(p.label).tag(p)
                }
            }
            .pickerStyle(.segmented)
            .labelsHidden()
        }
        .listRowBackground(Color.clear)
    }

    @ViewBuilder
    private var paneContent: some View {
        switch pane {
        case .findings: findingsSections
        case .notes:    notesSections
        case .activity: activitySections
        case .members:  membersSections
        }
    }

    // MARK: - Findings

    /// Grouped as a named type rather than a labelled tuple: Swift has no key
    /// paths into tuple elements, so `ForEach(_:id:)` cannot address one.
    private var groupedItems: [CaseFindingGroup] {
        var buckets: [String: [CaseItemRef]] = [:]
        for item in items {
            buckets[item.ref_type, default: []].append(item)
        }
        let keys = buckets.keys.sorted { a, b in
            let ia = CaseRefType.sortIndex(a)
            let ib = CaseRefType.sortIndex(b)
            return ia == ib ? a < b : ia < ib
        }
        return keys.map { CaseFindingGroup(id: $0, rows: buckets[$0] ?? []) }
    }

    @ViewBuilder
    private var findingsSections: some View {
        if items.isEmpty {
            Section {
                if loading {
                    centeredProgress
                } else {
                    emptyRow(icon: "pin.slash",
                             title: "No findings pinned",
                             message: "Pin an intel item, entity, breach record, camera or map feature into this case from its own screen. Every pin stores a snapshot of what you saw at that moment — reports render the snapshot and never re-read the live row.")
                }
            }
            .listRowBackground(theme.surface)
        } else {
            ForEach(groupedItems) { group in
                Section {
                    ForEach(group.rows) { item in
                        findingRow(item)
                    }
                } header: {
                    sectionHeader("\(CaseRefType.label(group.id)) · \(group.rows.count)")
                }
            }
            if itemsCursor != nil {
                Section {
                    loadMoreRow(loadingMoreItems, label: "Load more findings") {
                        await loadMoreItems()
                    }
                }
                .listRowBackground(Color.clear)
            }
        }
    }

    @ViewBuilder
    private func findingRow(_ item: CaseItemRef) -> some View {
        // `resolved == nil` is treated exactly like false: an envelope that
        // never said it resolved is not a resolved envelope.
        let resolved = item.snapshot?.resolved ?? false
        let title = item.snapshot?.displayTitle ?? item.label ?? item.ref_id
        let captured = item.snapshot?.captured_at
        let stamps = captured == nil
            ? "pinned \(caseRelativeTime(item.added_at))"
            : "pinned \(caseRelativeTime(item.added_at)) · snapshot \(caseRelativeTime(captured))"

        let content = VStack(alignment: .leading, spacing: 3) {
            HStack(spacing: 6) {
                Image(systemName: CaseRefType.icon(item.ref_type))
                    .font(.caption2)
                    .foregroundStyle(resolved ? theme.accent : theme.warning)
                Text(title)
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(theme.text)
                    .lineLimit(2)
            }
            // Always shown, resolved or not: the reference IS the finding.
            Text("\(item.ref_type) · \(item.ref_id)")
                .font(.caption2.monospaced())
                .foregroundStyle(theme.textMuted)
                .lineLimit(1)
            HStack(spacing: Space.xs) {
                if !resolved {
                    Pill(text: "UNRESOLVED SNAPSHOT",
                         tone: .warning,
                         icon: "questionmark.circle",
                         maxWidth: 200)
                }
                if let origin = item.snapshot?.origin, !origin.isEmpty {
                    Pill(text: origin.uppercased(),
                         tone: .neutral,
                         icon: "camera.viewfinder",
                         maxWidth: 110)
                }
                Spacer(minLength: 0)
            }
            Text(stamps)
                .font(.caption2.monospacedDigit())
                .foregroundStyle(theme.textMuted)
            if !resolved {
                Text("The referenced row couldn't be read when this was pinned, so there is no display copy. The pin is kept as a bare reference — it is still evidence that this was cited.")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
            }
        }
        .padding(.vertical, 2)

        Group {
            // Only intel items have a detail screen in this build. The push
            // shows the LIVE item; the snapshot above is what was pinned, and
            // the two can legitimately differ.
            if item.ref_type == "intel_item" {
                NavigationLink {
                    IntelDetail(uid: item.ref_id, fallbackTitle: title)
                } label: {
                    content
                }
            } else {
                content
            }
        }
        .listRowBackground(theme.surface)
        .swipeActions(edge: .trailing) {
            Button(role: .destructive) {
                Task { await unpin(item) }
            } label: {
                Label("Unpin", systemImage: "pin.slash")
            }
        }
        .contextMenu {
            Button {
                Clipboard.copy(item.ref_id)
                Haptics.tap()
            } label: {
                Label("Copy reference id", systemImage: "doc.on.doc")
            }
            Button {
                noteTargetId = item.id
                pane = .notes
                Haptics.selection()
            } label: {
                Label("Add a note", systemImage: "note.text.badge.plus")
            }
            Button(role: .destructive) {
                Task { await unpin(item) }
            } label: {
                Label("Unpin", systemImage: "pin.slash")
            }
        }
    }

    // MARK: - Notes

    @ViewBuilder
    private var notesSections: some View {
        Section {
            noteComposer
        } header: {
            sectionHeader("New note")
        }
        .listRowBackground(theme.surface)

        if notesLoading && notes.isEmpty {
            Section { centeredProgress }
                .listRowBackground(Color.clear)
        } else if notes.isEmpty {
            Section {
                emptyRow(icon: "note.text",
                         title: items.isEmpty ? "Nothing to annotate yet" : "No notes yet",
                         message: items.isEmpty
                            ? "A note always attaches to a finding, so it can never be a note about nothing. Pin something into this case and the composer above will let you annotate it."
                            : "Notes are part of the audit trail: only their author can edit one, deleting one leaves a tombstone, and nothing is ever erased.")
            }
            .listRowBackground(theme.surface)
        } else {
            Section {
                ForEach(notes) { note in
                    noteRow(note)
                }
            } header: {
                sectionHeader("Notes · \(notes.count)")
            } footer: {
                Text("Tombstones for deleted notes are only returned to analyst-and-up accounts, and only when the server is asked for them. This build's annotations client doesn't request them, so notes deleted before this screen loaded won't appear.")
                    .font(.caption2)
            }
        }
    }

    @ViewBuilder
    private var noteComposer: some View {
        if items.isEmpty {
            Text("Pin a finding first — a note is attached to something.")
                .font(.caption)
                .foregroundStyle(theme.textMuted)
        } else {
            VStack(alignment: .leading, spacing: Space.sm) {
                Picker("Attach to", selection: $noteTargetId) {
                    ForEach(items) { item in
                        Text(shortItemLabel(item)).tag(item.id)
                    }
                }
                .pickerStyle(.menu)

                TextField("Note — markdown, stored raw", text: $noteText, axis: .vertical)
                    .lineLimit(3...8)
                    .font(.subheadline)

                HStack(spacing: Space.sm) {
                    Spacer(minLength: 0)
                    Button {
                        Task { await saveNote() }
                    } label: {
                        if savingNote {
                            ProgressView().controlSize(.small)
                        } else {
                            Label("Add note", systemImage: "plus")
                        }
                    }
                    .buttonStyle(.borderedProminent)
                    .disabled(savingNote
                              || noteTargetId.isEmpty
                              || noteText.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
                }
            }
            .padding(.vertical, Space.xs)
        }
    }

    @ViewBuilder
    private func noteRow(_ note: Annotation) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack(spacing: 6) {
                Image(systemName: note.isDeleted ? "trash.slash" : "note.text")
                    .font(.caption2)
                    .foregroundStyle(note.isDeleted ? theme.textMuted : theme.accent)
                Text(noteTargetLabel(note))
                    .font(.caption2.monospaced())
                    .foregroundStyle(theme.textMuted)
                    .lineLimit(1)
                Spacer(minLength: 0)
            }

            if note.isDeleted {
                // Tombstone, never a hidden row: the note being gone is itself
                // part of the record.
                Text("note deleted")
                    .font(.subheadline.italic())
                    .foregroundStyle(theme.textMuted)
                Text("The row survives on purpose — an audit trail you can quietly edit is not an audit trail.")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
            } else {
                // Rendered as plain text, not parsed markdown: annotationsapi.h
                // stores body_md raw precisely because analysts paste payloads
                // and raw markup AS EVIDENCE.
                Text(note.body_md)
                    .font(.subheadline)
                    .foregroundStyle(theme.text)
                    .textSelection(.enabled)
            }

            HStack(spacing: Space.xs) {
                Text(note.author_id ?? "unknown author")
                    .font(.caption2.monospaced())
                    .foregroundStyle(theme.textMuted)
                    .lineLimit(1)
                Text("·")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
                Text(caseRelativeTime(note.isDeleted
                                      ? note.deleted_at
                                      : (note.updated_at ?? note.created_at)))
                    .font(.caption2.monospacedDigit())
                    .foregroundStyle(theme.textMuted)
                if note.updated_at != nil && !note.isDeleted {
                    Pill(text: "EDITED", tone: .neutral, maxWidth: 80)
                }
                Spacer(minLength: 0)
            }
        }
        .padding(.vertical, 2)
        .listRowBackground(theme.surface)
    }

    private func shortItemLabel(_ item: CaseItemRef) -> String {
        let resolved = item.snapshot?.resolved ?? false
        let title = item.snapshot?.displayTitle ?? item.label
        if let title, !title.isEmpty, resolved || item.label != nil {
            return title
        }
        return "\(item.ref_type) · \(item.ref_id)"
    }

    private func noteTargetLabel(_ note: Annotation) -> String {
        if let match = items.first(where: {
            $0.ref_type == note.ref_type && $0.ref_id == note.ref_id
        }) {
            return shortItemLabel(match)
        }
        return "\(note.ref_type) · \(note.ref_id)"
    }

    // MARK: - Activity

    @ViewBuilder
    private var activitySections: some View {
        Section {
            VStack(alignment: .leading, spacing: Space.sm) {
                TextField("Add a comment", text: $commentText, axis: .vertical)
                    .lineLimit(2...6)
                    .font(.subheadline)
                HStack {
                    Spacer(minLength: 0)
                    Button {
                        Task { await sendComment() }
                    } label: {
                        if sendingComment {
                            ProgressView().controlSize(.small)
                        } else {
                            Label("Comment", systemImage: "bubble.left")
                        }
                    }
                    .buttonStyle(.borderedProminent)
                    .disabled(sendingComment
                              || commentText.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
                }
            }
            .padding(.vertical, Space.xs)
        } header: {
            sectionHeader("Comment")
        }
        .listRowBackground(theme.surface)

        if activity.isEmpty {
            Section {
                if loading {
                    centeredProgress
                } else {
                    emptyRow(icon: "clock.badge.questionmark",
                             title: "No history yet",
                             message: "Creating, editing, pinning and roster changes are written here by the server — not by the client — so this feed is a record of what happened, not of what someone chose to type.")
                }
            }
            .listRowBackground(theme.surface)
        } else {
            Section {
                ForEach(activity) { entry in
                    activityRow(entry)
                }
            } header: {
                sectionHeader("Activity · newest first")
            }
            if activityCursor != nil {
                Section {
                    loadMoreRow(loadingMoreActivity, label: "Load older activity") {
                        await loadMoreActivity()
                    }
                }
                .listRowBackground(Color.clear)
            }
        }
    }

    @ViewBuilder
    private func activityRow(_ entry: CaseActivityEntry) -> some View {
        HStack(alignment: .top, spacing: Space.md) {
            Image(systemName: CaseActivityKind.icon(entry.kind))
                .font(.caption)
                .foregroundStyle(CaseActivityKind.isSystem(entry.kind)
                                 ? theme.textMuted : theme.accent)
                .frame(width: 22, height: 22)

            VStack(alignment: .leading, spacing: 3) {
                HStack(spacing: Space.xs) {
                    Pill(text: CaseActivityKind.label(entry.kind).uppercased(),
                         tone: CaseActivityKind.tone(entry.kind),
                         maxWidth: 170)
                    Spacer(minLength: 0)
                    Text(caseRelativeTime(entry.ts))
                        .font(.caption2.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                }
                if let entryBody = entry.body, !entryBody.isEmpty {
                    Text(entryBody)
                        .font(.subheadline)
                        .foregroundStyle(theme.text)
                        .textSelection(.enabled)
                }
                HStack(spacing: Space.xs) {
                    Text(entry.actor_id ?? "system")
                        .font(.caption2.monospaced())
                        .foregroundStyle(theme.textMuted)
                        .lineLimit(1)
                    if let target = entry.target_ref, !target.isEmpty {
                        Text("→ \(target)")
                            .font(.caption2.monospaced())
                            .foregroundStyle(theme.textMuted)
                            .lineLimit(1)
                    }
                    Spacer(minLength: 0)
                }
                if let mentions = entry.mentions, !mentions.isEmpty {
                    HStack(spacing: Space.xs) {
                        ForEach(mentions, id: \.self) { m in
                            Pill(text: "@\(m)", tone: .info, maxWidth: 150)
                        }
                        Spacer(minLength: 0)
                    }
                }
            }
        }
        .padding(.vertical, 2)
        .listRowBackground(theme.surface)
    }

    // MARK: - Members

    @ViewBuilder
    private var membersSections: some View {
        Section {
            if membersLoading && members.isEmpty {
                centeredProgress
            } else if members.isEmpty {
                emptyRow(icon: "person.2",
                         title: "No case members",
                         message: "Nobody is explicitly on this case. Any analyst, admin or owner in the workspace can still read and write it — a roster is how you narrow write access and name a lead.")
            } else {
                ForEach(members) { member in
                    HStack(spacing: Space.md) {
                        Image(systemName: member.role == "lead"
                              ? "person.crop.circle.badge.checkmark"
                              : "person.crop.circle")
                            .font(.body)
                            .foregroundStyle(member.role == "lead" ? theme.accent : theme.textMuted)
                        VStack(alignment: .leading, spacing: 1) {
                            Text(member.email ?? member.user_id)
                                .font(.subheadline)
                                .foregroundStyle(theme.text)
                                .lineLimit(1)
                            Text(member.user_id)
                                .font(.caption2.monospaced())
                                .foregroundStyle(theme.textMuted)
                                .lineLimit(1)
                        }
                        Spacer(minLength: 0)
                        Pill(text: member.role.uppercased(),
                             tone: memberTone(member.role),
                             maxWidth: 130)
                    }
                    .padding(.vertical, 2)
                }
            }
        } header: {
            sectionHeader("Roster")
        } footer: {
            Text("Read-only here. The server sets a roster with PUT /api/cases/:id/members (case lead, or a workspace owner/admin) and this build's API client has no call for it — so this screen shows the roster rather than pretending to edit it.")
                .font(.caption2)
        }
        .listRowBackground(theme.surface)
    }

    private func memberTone(_ role: String) -> Pill.Tone {
        switch role {
        case "lead":        return .accent
        case "contributor": return .info
        default:            return .neutral
        }
    }

    // MARK: - Shared row helpers

    private var centeredProgress: some View {
        HStack {
            Spacer(minLength: 0)
            ProgressView()
            Spacer(minLength: 0)
        }
        .padding(.vertical, Space.md)
    }

    private func sectionHeader(_ text: String) -> some View {
        Text(text.uppercased())
            .font(Typography.display(10, weight: .semibold))
            .tracking(1.2)
            .foregroundStyle(theme.textMuted)
    }

    private func emptyRow(icon: String, title: String, message: String) -> some View {
        VStack(alignment: .leading, spacing: Space.xs) {
            HStack(spacing: Space.sm) {
                Image(systemName: icon)
                    .font(.body)
                    .foregroundStyle(theme.textMuted)
                Text(title)
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(theme.text)
            }
            Text(message)
                .font(.caption)
                .foregroundStyle(theme.textMuted)
        }
        .padding(.vertical, Space.xs)
    }

    private func loadMoreRow(_ busy: Bool, label: String,
                             action: @escaping () async -> Void) -> some View {
        HStack(spacing: Space.sm) {
            Spacer(minLength: 0)
            if busy {
                ProgressView()
            } else {
                Image(systemName: "arrow.down.circle")
                    .font(.caption)
                    .foregroundStyle(theme.textMuted)
            }
            Text(busy ? "Loading…" : label)
                .font(.caption)
                .foregroundStyle(theme.textMuted)
            Spacer(minLength: 0)
        }
        .padding(.vertical, Space.sm)
        .onAppear { Task { await action() } }
    }

    // MARK: - Loading

    private func loadAll() async {
        loading = true
        defer { loading = false }
        do {
            let d = try await apiClient.api.caseDetail(caseId)
            detail = d
            if let roster = d.members { members = roster }
            if let counts = d.item_counts {
                store.noteItemCount(counts.values.reduce(0, +), for: caseId)
            }

            let itemsEnv = try await apiClient.api.caseItems(caseId, limit: 100)
            items = itemsEnv.data
            itemsCursor = itemsEnv.page?.cursor
            if noteTargetId.isEmpty || !items.contains(where: { $0.id == noteTargetId }) {
                noteTargetId = items.first?.id ?? ""
            }

            let actEnv = try await apiClient.api.caseActivity(caseId, limit: 100)
            activity = actEnv.data
            activityCursor = actEnv.page?.cursor

            error = nil
        } catch let e {
            error = e.localizedDescription
        }
        await loadNotes()
    }

    private func loadMoreItems() async {
        guard let cursor = itemsCursor, !loadingMoreItems else { return }
        loadingMoreItems = true
        defer { loadingMoreItems = false }
        do {
            let env = try await apiClient.api.caseItems(caseId, limit: 100, cursor: cursor)
            var seen = Set(items.map(\.id))
            for row in env.data where !seen.contains(row.id) {
                seen.insert(row.id)
                items.append(row)
            }
            itemsCursor = env.page?.cursor
            error = nil
        } catch let e {
            error = e.localizedDescription
        }
    }

    private func loadMoreActivity() async {
        guard let cursor = activityCursor, !loadingMoreActivity else { return }
        loadingMoreActivity = true
        defer { loadingMoreActivity = false }
        do {
            let env = try await apiClient.api.caseActivity(caseId, limit: 100, cursor: cursor)
            var seen = Set(activity.map(\.id))
            for row in env.data where !seen.contains(row.id) {
                seen.insert(row.id)
                activity.append(row)
            }
            activityCursor = env.page?.cursor
            error = nil
        } catch let e {
            error = e.localizedDescription
        }
    }

    /// One request per pinned finding — see the file header. Failures are
    /// counted and reported rather than swallowed, because a Notes pane that is
    /// quietly missing half the notes is worse than one that says so.
    private func loadNotes() async {
        guard !items.isEmpty else {
            notes = []
            return
        }
        notesLoading = true
        defer { notesLoading = false }

        let targets = Array(items.prefix(Self.noteFanoutCap))
        var collected: [Annotation] = []
        var failures = 0
        for item in targets {
            do {
                let rows = try await apiClient.api.annotations(refType: item.ref_type,
                                                               refId: item.ref_id,
                                                               caseId: caseId,
                                                               limit: 100)
                collected.append(contentsOf: rows)
            } catch {
                failures += 1
            }
        }

        var seen = Set<String>()
        notes = collected
            .filter { seen.insert($0.id).inserted }
            .sorted { $0.created_at > $1.created_at }

        if failures > 0 {
            error = "\(failures) of \(targets.count) note lookups failed — this list is incomplete."
        }
        if items.count > targets.count {
            notice = "Showing notes for the first \(targets.count) of \(items.count) findings."
        }
    }

    private func loadMembers() async {
        membersLoading = true
        defer { membersLoading = false }
        do {
            members = try await apiClient.api.caseMembers(caseId)
            error = nil
        } catch let e {
            error = e.localizedDescription
        }
    }

    // MARK: - Mutations

    /// Optimistic unpin with rollback, same shape as `AlertsTab.delete`.
    private func unpin(_ item: CaseItemRef) async {
        let snapshot = items
        items.removeAll { $0.id == item.id }
        Haptics.warning()
        do {
            try await apiClient.api.caseUnpin(caseId,
                                              refType: item.ref_type,
                                              refId: item.ref_id)
            error = nil
            notes.removeAll { $0.ref_type == item.ref_type && $0.ref_id == item.ref_id }
            if noteTargetId == item.id { noteTargetId = items.first?.id ?? "" }
            await refreshCounts()
        } catch let e {
            items = snapshot
            error = e.localizedDescription
            Haptics.error()
        }
    }

    private func saveNote() async {
        guard let target = items.first(where: { $0.id == noteTargetId }) else {
            error = "Pick which finding this note is about."
            Haptics.error()
            return
        }
        savingNote = true
        defer { savingNote = false }
        do {
            let created = try await apiClient.api.annotationCreate(
                refType: target.ref_type,
                refId: target.ref_id,
                bodyMd: noteText.trimmingCharacters(in: .whitespacesAndNewlines),
                caseId: caseId)
            notes.insert(created, at: 0)
            noteText = ""
            error = nil
            notice = nil
            Haptics.success()
        } catch let e {
            error = e.localizedDescription
            Haptics.error()
        }
    }

    private func sendComment() async {
        let text = commentText.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !text.isEmpty else { return }
        sendingComment = true
        defer { sendingComment = false }
        do {
            try await apiClient.api.caseComment(caseId, body: text)
            commentText = ""
            error = nil
            Haptics.success()
            // POST /activity returns the created row, but the client discards
            // it, so re-read the feed to get the server's id and timestamp
            // rather than inventing a local placeholder.
            let env = try await apiClient.api.caseActivity(caseId, limit: 100)
            activity = env.data
            activityCursor = env.page?.cursor
        } catch let e {
            error = e.localizedDescription
            Haptics.error()
        }
    }

    /// Re-read the header counts after a pin/unpin. The unpin itself already
    /// reported its own outcome, so a failure here only leaves a stale count.
    private func refreshCounts() async {
        guard let d = try? await apiClient.api.caseDetail(caseId) else { return }
        detail = d
        if let counts = d.item_counts {
            store.noteItemCount(counts.values.reduce(0, +), for: caseId)
        }
    }
}

// MARK: - Findings grouping

/// One Findings section: a `ref_type` and the pins of that type. `id` IS the
/// ref_type.
private struct CaseFindingGroup: Identifiable {
    let id: String
    let rows: [CaseItemRef]
}

// MARK: - Edit sheet

/// Inline edit of the four PATCHable fields. Sends all four rather than a diff:
/// the endpoint is a partial update, and a four-key body is one fewer place for
/// a "which fields changed" comparison to get it wrong.
private struct CaseEditSheet: View {
    let detail: CaseDetail
    let onSaved: (CaseDetail) -> Void

    @EnvironmentObject var store: CaseStore
    @Environment(\.theme) private var theme
    @Environment(\.dismiss) private var dismiss

    @State private var name: String
    @State private var summary: String
    @State private var status: String
    @State private var priority: Int
    @State private var saving = false
    @State private var error: String?

    init(detail: CaseDetail, onSaved: @escaping (CaseDetail) -> Void) {
        self.detail = detail
        self.onSaved = onSaved
        _name = State(initialValue: detail.name)
        _summary = State(initialValue: detail.summary ?? "")
        _status = State(initialValue: detail.status)
        _priority = State(initialValue: detail.priority)
    }

    private var trimmedName: String {
        name.trimmingCharacters(in: .whitespacesAndNewlines)
    }

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    TextField("Case name", text: $name)
                        .font(.subheadline)
                } header: {
                    Text("Name")
                }

                Section {
                    TextField("Summary", text: $summary, axis: .vertical)
                        .lineLimit(3...10)
                        .font(.subheadline)
                } header: {
                    Text("Summary")
                } footer: {
                    Text("Rendered as the executive summary of the exported report.")
                }

                Section {
                    Picker("Status", selection: $status) {
                        ForEach(CaseStatus.all, id: \.self) { s in
                            Text(CaseStatus.label(s)).tag(s)
                        }
                    }
                    Picker("Priority", selection: $priority) {
                        ForEach(CasePriority.range, id: \.self) { p in
                            Text("\(p) · \(CasePriority.label(p))").tag(p)
                        }
                    }
                } header: {
                    Text("Triage")
                } footer: {
                    Text("Closing a case stamps closed_at server-side; reopening clears it.")
                }

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
            .navigationTitle("Edit case")
            .compatInlineTitle()
            .toolbar {
                ToolbarItem(placement: .compatLeading) {
                    Button("Cancel") { dismiss() }
                }
                ToolbarItem(placement: .compatPrimary) {
                    Button {
                        Task { await save() }
                    } label: {
                        if saving {
                            ProgressView().controlSize(.small)
                        } else {
                            Text("Save")
                        }
                    }
                    .disabled(saving || trimmedName.isEmpty)
                }
            }
        }
        .compatSheetSizing(.form)
    }

    private func save() async {
        saving = true
        defer { saving = false }
        let fields: [String: Any] = [
            "name": trimmedName,
            "summary": summary.trimmingCharacters(in: .whitespacesAndNewlines),
            "status": status,
            "priority": priority
        ]
        do {
            let updated = try await store.update(detail.id, fields: fields)
            error = nil
            Haptics.success()
            onSaved(updated)
            dismiss()
        } catch let e {
            error = e.localizedDescription
            Haptics.error()
        }
    }
}

// MARK: - Delete sheet

/// Destructive, irreversible, and therefore typed-confirmation only.
///
/// A sheet rather than a `.alert` with a text field: an alert's buttons don't
/// reliably honour `.disabled`, and a confirmation you can tap through without
/// typing is not a confirmation.
private struct CaseDeleteSheet: View {
    let caseName: String
    let findingCount: Int
    /// Returns true when the server confirmed the delete.
    let onConfirm: () async -> Bool

    @Environment(\.theme) private var theme
    @Environment(\.dismiss) private var dismiss

    @State private var typed = ""
    @State private var working = false
    @State private var error: String?

    private var confirmed: Bool {
        let t = typed.trimmingCharacters(in: .whitespacesAndNewlines)
        if t.isEmpty { return false }
        if t.caseInsensitiveCompare(caseName) == .orderedSame { return true }
        return t == "DELETE"
    }

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    VStack(alignment: .leading, spacing: Space.sm) {
                        HStack(spacing: Space.sm) {
                            Image(systemName: "exclamationmark.triangle.fill")
                                .foregroundStyle(theme.danger)
                            Text("This cannot be undone")
                                .font(.subheadline.weight(.semibold))
                                .foregroundStyle(theme.text)
                        }
                        Text("Deleting “\(caseName)” permanently removes:")
                            .font(.caption)
                            .foregroundStyle(theme.textMuted)
                        VStack(alignment: .leading, spacing: 2) {
                            bullet("\(findingCount) pinned finding\(findingCount == 1 ? "" : "s") and their snapshots")
                            bullet("every note attached to this case")
                            bullet("the entire activity history, including comments")
                            bullet("the case roster")
                        }
                        Text("The intel items, entities and breach records themselves are not deleted — only this case's record of citing them.")
                            .font(.caption2)
                            .foregroundStyle(theme.textMuted)
                    }
                    .padding(.vertical, Space.xs)
                }

                Section {
                    TextField("Type the case name", text: $typed)
                        .font(.subheadline.monospaced())
                        .compatNoAutocap()
                        .autocorrectionDisabled()
                } header: {
                    Text("Confirm")
                } footer: {
                    Text("Type “\(caseName)” — or the word DELETE — to enable the button.")
                }

                if let error {
                    Section {
                        Text(error)
                            .font(.caption)
                            .foregroundStyle(theme.danger)
                    }
                }

                Section {
                    Button(role: .destructive) {
                        Task { await run() }
                    } label: {
                        HStack {
                            if working { ProgressView().controlSize(.small) }
                            Text(working ? "Deleting…" : "Delete this case")
                            Spacer(minLength: 0)
                        }
                    }
                    .disabled(!confirmed || working)
                }
            }
            .compatGroupedForm()
            .themedScreenBackground(theme)
            .navigationTitle("Delete case")
            .compatInlineTitle()
            .toolbar {
                ToolbarItem(placement: .compatLeading) {
                    Button("Cancel") { dismiss() }
                        .disabled(working)
                }
            }
        }
        .compatSheetSizing(.form)
    }

    private func bullet(_ text: String) -> some View {
        HStack(alignment: .top, spacing: Space.xs) {
            Text("•")
                .font(.caption)
                .foregroundStyle(theme.danger)
            Text(text)
                .font(.caption)
                .foregroundStyle(theme.text)
        }
    }

    private func run() async {
        working = true
        defer { working = false }
        let ok = await onConfirm()
        if ok {
            dismiss()
        } else {
            // `CaseStore.delete` already published the reason; repeat it here
            // so the user sees it without the sheet closing under them.
            error = "The server refused the delete. Only a workspace owner/admin or a case lead may delete a case."
        }
    }
}
