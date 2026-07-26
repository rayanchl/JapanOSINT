import SwiftUI

// ─────────────────────────────────────────────────────────────────────────────
// Roadmap 15 — analyst notes on any `(ref_type, ref_id)` pair.
//
// List + compose + edit-own + delete, against `/api/annotations`. Works for an
// intel item, an entity, a breach record, a camera — the server does not care
// what the pair means, and neither does this view.
//
// Three behaviours are load-bearing:
//
//   • Notes are stored RAW. What the analyst typed is what is sent; markdown
//     is a rendering concern, applied only on display via
//     `AttributedString(markdown:)`. Nothing is normalised on the way in.
//
//   • A deleted note renders as a visible TOMBSTONE, never disappears. The
//     server soft-deletes and returns the tombstone for exactly this reason:
//     analyst notes are an audit trail, and a note that silently vanished
//     would make the trail unreadable.
//
//   • Only the author may edit or delete (the server answers 403 otherwise).
//     That is surfaced as a DISABLED affordance plus a one-line explanation,
//     so the user learns the rule instead of discovering it as a failed
//     request. When the caller does not know who the current user is, the
//     controls stay enabled and any 403 surfaces in the error banner — an
//     unknown identity is not evidence that editing is forbidden.
//
// Self-contained: takes the ref pair (and optionally the viewer's id), fetches
// its own data, presents its own editor sheet, and navigates nowhere.
// ─────────────────────────────────────────────────────────────────────────────

struct AnnotationsSection: View {
    let refType: String
    let refId: String
    /// Scopes the notes to a case when the host is a case workspace.
    var caseId: String? = nil
    /// `auth.me?.user?.id`. `nil` means "not known" — see the header note.
    var currentUserId: String? = nil
    var title: String = "Notes"

    @EnvironmentObject private var apiClient: APIClient
    @Environment(\.theme) private var theme

    @State private var notes: [Annotation] = []
    @State private var loading = false
    @State private var error: String?
    @State private var draft = ""
    @State private var composing = false
    @State private var saving = false
    @State private var busyId: String?
    @State private var editing: Annotation?

    var body: some View {
        VStack(alignment: .leading, spacing: Space.md) {
            header

            if let error {
                errorBanner(error)
            }

            if composing {
                composer
            }

            if loading && notes.isEmpty {
                ProgressView()
                    .frame(maxWidth: .infinity, alignment: .center)
                    .padding(.vertical, Space.lg)
            } else if notes.isEmpty && !composing {
                emptyState
            } else {
                ForEach(notes) { note in
                    noteCard(note)
                }
            }
        }
        .task(id: refKey) { await reload() }
        .sheet(item: $editing) { note in
            AnnotationEditorSheet(original: note) { newBody in
                Task { await commitEdit(note, body: newBody) }
            }
        }
    }

    private var refKey: String { "\(refType)|\(refId)|\(caseId ?? "")" }

    // MARK: - Header

    private var header: some View {
        HStack(spacing: Space.sm) {
            Image(systemName: "note.text")
                .font(.subheadline)
                .foregroundStyle(theme.accentAlt)
            Text(title)
                .font(.headline)
                .foregroundStyle(theme.text)
            if !notes.isEmpty {
                Text("\(notes.count)")
                    .font(.caption.monospacedDigit())
                    .foregroundStyle(theme.textMuted)
            }
            Spacer(minLength: 0)
            if loading && !notes.isEmpty {
                ProgressView().controlSize(.mini)
            }
            Button {
                composing = true
            } label: {
                Label("Add note", systemImage: "square.and.pencil")
                    .font(.caption)
            }
            .buttonStyle(.bordered)
            .controlSize(.small)
            .disabled(composing)
        }
    }

    // MARK: - Compose

    private var composer: some View {
        VStack(alignment: .leading, spacing: Space.sm) {
            TextEditor(text: $draft)
                .font(.callout)
                .frame(minHeight: 96)
                .scrollContentBackground(.hidden)
                .padding(Space.sm)
                .background(theme.surfaceElevated,
                            in: RoundedRectangle(cornerRadius: Radius.md))
                .overlay(
                    RoundedRectangle(cornerRadius: Radius.md)
                        .strokeBorder(theme.accent.opacity(0.35),
                                      lineWidth: Stroke.hairline)
                )

            HStack(spacing: Space.sm) {
                Text("Markdown. Stored exactly as typed.")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
                Spacer(minLength: 0)
                Button("Cancel") {
                    composing = false
                    draft = ""
                }
                .buttonStyle(.bordered)
                .controlSize(.small)
                .disabled(saving)
                Button {
                    Task { await create() }
                } label: {
                    if saving {
                        ProgressView().controlSize(.mini)
                    } else {
                        Text("Save note")
                    }
                }
                .buttonStyle(.borderedProminent)
                .controlSize(.small)
                .disabled(saving || draft.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
            }
        }
    }

    // MARK: - Rows

    @ViewBuilder
    private func noteCard(_ a: Annotation) -> some View {
        VStack(alignment: .leading, spacing: Space.xs) {
            HStack(spacing: Space.sm) {
                if a.isDeleted {
                    Pill(text: "DELETED", tone: .neutral, icon: "trash", maxWidth: 110)
                }
                Text(authorLabel(a))
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
                    .lineLimit(1)
                Text(relativeTime(a.updated_at ?? a.created_at))
                    .font(.caption2.monospacedDigit())
                    .foregroundStyle(theme.textMuted)
                if a.updated_at != nil, a.updated_at != a.created_at, !a.isDeleted {
                    Text("(edited)")
                        .font(.system(size: 9))
                        .foregroundStyle(theme.textMuted)
                }
                Spacer(minLength: 0)
                if !a.isDeleted {
                    rowActions(a)
                }
            }

            if a.isDeleted {
                // Tombstone. The body is intentionally NOT shown — the record
                // is kept so the trail is complete, not so the text survives.
                Text("Note deleted \(relativeTime(a.deleted_at)). The record is kept: notes are an audit trail, so a deletion is shown rather than hidden.")
                    .font(.caption)
                    .italic()
                    .foregroundStyle(theme.textMuted)
                    .fixedSize(horizontal: false, vertical: true)
            } else {
                Text(annotationMarkdown(a.body_md))
                    .font(.callout)
                    .foregroundStyle(theme.text)
                    .textSelection(.enabled)
                    .fixedSize(horizontal: false, vertical: true)
                if !canEdit(a) {
                    Text("Only the author can edit or delete this note.")
                        .font(.system(size: 10))
                        .foregroundStyle(theme.textMuted)
                }
            }
        }
        .padding(Space.md)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(theme.surfaceElevated,
                    in: RoundedRectangle(cornerRadius: Radius.md))
        .opacity(a.isDeleted ? 0.72 : 1)
    }

    @ViewBuilder
    private func rowActions(_ a: Annotation) -> some View {
        if busyId == a.id {
            ProgressView().controlSize(.mini)
        } else {
            let editable = canEdit(a)
            Button {
                editing = a
            } label: {
                Image(systemName: "pencil").font(.caption)
            }
            .buttonStyle(.plain)
            .foregroundStyle(editable ? theme.accent : theme.textMuted)
            .disabled(!editable)
            .accessibilityLabel(editable ? "Edit note" : "Edit note (author only)")

            Button {
                Task { await deleteNote(a) }
            } label: {
                Image(systemName: "trash").font(.caption)
            }
            .buttonStyle(.plain)
            .foregroundStyle(editable ? theme.danger : theme.textMuted)
            .disabled(!editable)
            .accessibilityLabel(editable ? "Delete note" : "Delete note (author only)")
        }
    }

    // MARK: - Empty / error

    private var emptyState: some View {
        ContentUnavailableView {
            Label {
                Text("No notes yet").foregroundStyle(theme.text)
            } icon: {
                Image(systemName: "note.text").foregroundStyle(theme.textMuted)
            }
        } description: {
            Text("Notes you add here stay attached to this record and are kept — including after deletion — as an audit trail.")
                .foregroundStyle(theme.textMuted)
        }
    }

    private func errorBanner(_ message: String) -> some View {
        HStack(alignment: .top, spacing: Space.sm) {
            Image(systemName: "exclamationmark.triangle.fill")
                .font(.caption)
                .foregroundStyle(theme.danger)
            Text(message)
                .font(.caption)
                .foregroundStyle(theme.text)
                .fixedSize(horizontal: false, vertical: true)
            Spacer(minLength: 0)
            Button {
                error = nil
            } label: {
                Image(systemName: "xmark").font(.caption2)
            }
            .buttonStyle(.plain)
            .foregroundStyle(theme.textMuted)
            .accessibilityLabel("Dismiss error")
        }
        .padding(Space.md)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(theme.danger.opacity(0.12),
                    in: RoundedRectangle(cornerRadius: Radius.md))
    }

    // MARK: - Rules

    /// A deleted note is nobody's to edit. Otherwise the author matches, or we
    /// do not know enough to forbid it — see the header note on unknown ids.
    private func canEdit(_ a: Annotation) -> Bool {
        if a.isDeleted { return false }
        guard let me = currentUserId, !me.isEmpty,
              let author = a.author_id, !author.isEmpty else { return true }
        return me == author
    }

    private func authorLabel(_ a: Annotation) -> String {
        guard let author = a.author_id, !author.isEmpty else { return "unknown author" }
        if let me = currentUserId, me == author { return "you" }
        return author.count > 12 ? String(author.prefix(12)) + "…" : author
    }

    // MARK: - Network

    private func reload() async {
        loading = true
        defer { loading = false }
        do {
            notes = try await apiClient.api.annotations(refType: refType,
                                                        refId: refId,
                                                        caseId: caseId)
            error = nil
        } catch {
            self.error = "Could not load notes — \(error.localizedDescription)"
            Haptics.error()
        }
    }

    private func create() async {
        guard !draft.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else { return }
        saving = true
        defer { saving = false }
        do {
            // `draft` is sent verbatim: the trim above is a validity check, not
            // a transform. Leading indentation is markdown.
            let created = try await apiClient.api.annotationCreate(refType: refType,
                                                                   refId: refId,
                                                                   bodyMd: draft,
                                                                   caseId: caseId)
            notes.insert(created, at: 0)
            draft = ""
            composing = false
            error = nil
            Haptics.success()
        } catch {
            self.error = "Could not save the note — \(error.localizedDescription)"
            Haptics.error()
        }
    }

    private func commitEdit(_ a: Annotation, body: String) async {
        busyId = a.id
        defer { busyId = nil }
        do {
            let updated = try await apiClient.api.annotationUpdate(a.id, bodyMd: body)
            replace(updated)
            error = nil
            Haptics.success()
        } catch {
            self.error = "Could not save the edit — \(error.localizedDescription)"
            Haptics.error()
        }
    }

    /// The server soft-deletes and returns the tombstone, which replaces the
    /// row in place. Nothing is removed from the list.
    private func deleteNote(_ a: Annotation) async {
        busyId = a.id
        defer { busyId = nil }
        do {
            let tombstone = try await apiClient.api.annotationDelete(a.id)
            replace(tombstone)
            error = nil
            Haptics.warning()
        } catch {
            self.error = "Could not delete the note — \(error.localizedDescription)"
            Haptics.error()
        }
    }

    private func replace(_ updated: Annotation) {
        if let i = notes.firstIndex(where: { $0.id == updated.id }) {
            notes[i] = updated
        } else {
            notes.insert(updated, at: 0)
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// MARK: - Editor sheet
// ─────────────────────────────────────────────────────────────────────────────

/// Deliberately dumb: it edits text and hands the result back. The network
/// call and its error live in the section, so a failed save surfaces in the
/// section's banner instead of trapping the user in a sheet.
private struct AnnotationEditorSheet: View {
    let original: Annotation
    let onCommit: (String) -> Void

    @Environment(\.theme) private var theme
    @Environment(\.dismiss) private var dismiss
    @State private var text: String

    init(original: Annotation, onCommit: @escaping (String) -> Void) {
        self.original = original
        self.onCommit = onCommit
        _text = State(initialValue: original.body_md)
    }

    var body: some View {
        NavigationStack {
            VStack(alignment: .leading, spacing: Space.md) {
                TextEditor(text: $text)
                    .font(.callout)
                    .frame(minHeight: 180)
                    .scrollContentBackground(.hidden)
                    .padding(Space.sm)
                    .background(theme.surfaceElevated,
                                in: RoundedRectangle(cornerRadius: Radius.md))
                Text("Markdown. Stored raw and rendered only on display.")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
                Spacer(minLength: 0)
            }
            .padding(Space.lg)
            .frame(maxWidth: .infinity, alignment: .leading)
            .themedScreenBackground(theme)
            .navigationTitle("Edit note")
            .compatInlineTitle()
            .toolbar {
                ToolbarItem(placement: .compatLeading) {
                    Button("Cancel") { dismiss() }
                }
                ToolbarItem(placement: .compatPrimary) {
                    Button("Save") {
                        onCommit(text)
                        dismiss()
                    }
                    .disabled(text.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
                }
            }
        }
        .compatSheetSizing(.medium)
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// MARK: - File-private helpers
// ─────────────────────────────────────────────────────────────────────────────

/// Inline-only markdown: bold/italic/code/links render, but block syntax is
/// preserved as written rather than collapsed, so a note's line breaks survive.
/// A note that fails to parse falls back to its literal text — never to blank.
private func annotationMarkdown(_ md: String) -> AttributedString {
    let options = AttributedString.MarkdownParsingOptions(
        interpretedSyntax: .inlineOnlyPreservingWhitespace)
    if let parsed = try? AttributedString(markdown: md, options: options) {
        return parsed
    }
    return AttributedString(md)
}
