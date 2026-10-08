import SwiftUI
import Foundation

// ─────────────────────────────────────────────────────────────────────────────
// Roadmap item 38 — recent searches.
//
// THE WORKSPACE LINE IS STATED, NOT LEFT TO GUESS.
// Decided 2026-10-05: everything a workspace authors is visible to all of its
// members. `/api/search-history` is read under `tenant_id` only — every
// member's entries, each carrying its author (`user_id`, `mine`) — and cleared
// under `tenant_id AND user_id`: clearing removes the caller's own entries and
// nobody else's. The clear is still deliberately NOT audited.
//
// This screen used to promise "Private to you — workspace owners and admins
// cannot read what you have been investigating". That stopped being true, and
// a privacy promise the server does not keep is worse than none, so the UI now
// says plainly who sees the list.
//
// The list is offset-paged with a measured total (`page.total`); the footer
// shows "showing N of M" and loads the next page.
//
// Re-running is a host concern: history rows carry `kind` + `params`, and the
// endpoint that executes them differs per kind, so this view hands the entry
// back through `onRerun` rather than pretending to run anything itself.
// ─────────────────────────────────────────────────────────────────────────────

/// Recent searches. Push it (it does not own a `NavigationStack`) or wrap it in
/// one when presenting as a sheet.
struct SearchHistoryView: View {
    /// Tapping a row calls this. When `nil`, rows are read-only.
    var onRerun: ((SearchHistoryEntry) -> Void)? = nil
    /// Rows per page. The server clamps to 1…200.
    var limit: Int = 50

    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme

    @State private var entries: [SearchHistoryEntry] = []
    @State private var page: OffsetPage?
    @State private var names: [String: String] = [:]
    @State private var loading = true
    @State private var loadingMore = false
    @State private var moreError: String?
    @State private var error: String?
    @State private var clearing = false
    @State private var showClear = false

    var body: some View {
        Group {
            if loading && entries.isEmpty {
                ProgressView().frame(maxWidth: .infinity, maxHeight: .infinity)
            } else if entries.isEmpty, let error {
                OfflineStateView(kind: .error,
                                 title: "Couldn't load history",
                                 message: error,
                                 retry: { Task { await reload() } })
            } else if entries.isEmpty {
                emptyState
            } else {
                list
            }
        }
        .themedScreenBackground(theme)
        .navigationTitle("Recent searches")
        .toolbar {
            ToolbarItem(placement: .compatPrimary) {
                Button { Task { await reload() } } label: {
                    Image(systemName: "arrow.clockwise")
                }
                .disabled(loading || clearing)
                .accessibilityLabel("Reload")
            }
            ToolbarItem(placement: .compatPrimary) {
                Button(role: .destructive) { showClear = true } label: {
                    Image(systemName: "trash")
                }
                .disabled(entries.isEmpty || clearing)
                .accessibilityLabel("Clear history")
            }
        }
        .task { if entries.isEmpty { await reload() } }
        .refreshable { await reload() }
        .confirmationDialog("Clear your search history?",
                            isPresented: $showClear,
                            titleVisibility: .visible) {
            Button("Clear history", role: .destructive) {
                Task { await clear() }
            }
            Button("Cancel", role: .cancel) {}
        } message: {
            Text("Deletes every entry you ran. Your teammates' entries are kept, nothing is recorded about the deletion, and saved searches are kept.")
        }
    }

    // MARK: - List

    private var list: some View {
        List {
            Section {
                workspaceBanner
            }
            .listRowBackground(theme.surface)

            if let error, !entries.isEmpty {
                Section {
                    Label(error, systemImage: "exclamationmark.triangle.fill")
                        .font(.caption)
                        .foregroundStyle(theme.danger)
                }
                .listRowBackground(theme.surface)
            }

            Section {
                ForEach(entries) { entry in
                    entryRow(entry)
                }
                PageBoundFooter(shown: entries.count, page: page, noun: "entries",
                                loadingMore: loadingMore, error: moreError,
                                loadMore: { await loadMore() })
                    .listRowBackground(theme.surface)
            } header: {
                Text("RECENT")
                    .font(Typography.display(10, weight: .semibold))
                    .tracking(1.2)
                    .foregroundStyle(theme.textMuted)
            } footer: {
                Text(onRerun == nil
                     ? "The server keeps each member's most recent entries and drops older ones automatically."
                     : "Tap an entry to run it again. The server keeps each member's most recent entries and drops older ones automatically.")
                    .font(.caption2)
            }
        }
        .listStyle(.plain)
        .scrollContentBackground(.hidden)
    }

    private var workspaceBanner: some View {
        HStack(alignment: .top, spacing: Space.md) {
            Image(systemName: "person.2.fill")
                .font(.subheadline)
                .foregroundStyle(theme.accent)
                .frame(width: 24)
                .accessibilityHidden(true)   // "Shared with your workspace" follows
            VStack(alignment: .leading, spacing: 2) {
                Text("Shared with your workspace")
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(theme.text)
                Text("Every member of this workspace sees every member's searches here, each with who ran it. Clearing removes only your own entries, and is not logged.")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
            }
        }
        .padding(.vertical, Space.xs)
    }

    @ViewBuilder
    private func entryRow(_ entry: SearchHistoryEntry) -> some View {
        if let onRerun {
            Button {
                Haptics.selection()
                onRerun(entry)
            } label: {
                entryContent(entry, showsChevron: true)
            }
            .buttonStyle(.plain)
            .listRowBackground(theme.surface)
        } else {
            entryContent(entry, showsChevron: false)
                .listRowBackground(theme.surface)
        }
    }

    private func entryContent(_ entry: SearchHistoryEntry,
                              showsChevron: Bool) -> some View {
        HStack(spacing: Space.sm) {
            VStack(alignment: .leading, spacing: Space.xs) {
                HStack(spacing: Space.sm) {
                    Pill(text: SearchKindStyle.label(entry.kind),
                         tone: SearchKindStyle.tone(entry.kind), size: .md)
                    Text(SearchParamsSummary.text(entry.params) ?? "(no query terms)")
                        .font(.subheadline)
                        .foregroundStyle(theme.text)
                        .lineLimit(2)
                }
                HStack(spacing: Space.sm) {
                    Text(relativeTime(entry.ts))
                        .font(.caption2.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                    Text("·").font(.caption2).foregroundStyle(theme.textMuted)
                    // A null result_count means the run was recorded without a
                    // count, NOT that it found nothing.
                    Text(entry.result_count == nil
                         ? "count not recorded"
                         : "\(entry.result_count ?? 0) result\((entry.result_count ?? 0) == 1 ? "" : "s")")
                        .font(.caption2.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                    Text("·").font(.caption2).foregroundStyle(theme.textMuted)
                    Text(AuthorLabel.text(userId: entry.user_id, mine: entry.mine, names: names))
                        .font(.caption2)
                        .foregroundStyle(theme.textMuted)
                        .lineLimit(1)
                    Spacer(minLength: 0)
                }
            }
            if showsChevron {
                Image(systemName: "arrow.counterclockwise")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
                    .accessibilityLabel("Run this search again")
            }
        }
        .padding(.vertical, Space.xs)
        .contentShape(Rectangle())
    }

    private var emptyState: some View {
        ContentUnavailableView {
            Label {
                Text("No recent searches").foregroundStyle(theme.text)
            } icon: {
                Image(systemName: "clock.arrow.circlepath")
                    .foregroundStyle(theme.textMuted)
                    .accessibilityHidden(true)   // "No recent searches" is the label
            }
        } description: {
            Text("Every search anyone in this workspace runs is recorded here, and every member can read this list.")
                .foregroundStyle(theme.textMuted)
        } actions: {
            Button {
                Task { await reload() }
            } label: {
                Label("Reload", systemImage: "arrow.clockwise")
            }
            .buttonStyle(.bordered)
            .tint(theme.accent)
        }
    }

    // MARK: - Work

    /// Re-read from the top, as many rows as are on screen (up to the server's
    /// 200), so a refresh or a mutation does not collapse a list the user has
    /// paged through back to one page.
    private func reload() async {
        loading = true
        defer { loading = false }
        do {
            let size = PageBound.reloadSize(shown: entries.count, pageSize: limit)
            let r = try await apiClient.api.searchHistory(limit: size, offset: 0)
            entries = r.rows
            page = r.page
            error = nil
            moreError = nil
        } catch {
            self.error = ServerError.message(error)
            Haptics.error()
        }
        // Author names are a nicety: without the roster a row still says
        // "you" or "a teammate", so a failure here is not an error banner.
        if let roster = try? await apiClient.api.membersList() {
            names = AuthorLabel.names(roster)
        }
    }

    private func loadMore() async {
        guard page?.has_more == true, !loadingMore else { return }
        loadingMore = true
        defer { loadingMore = false }
        do {
            let next = page?.nextOffset(fallback: entries.count) ?? entries.count
            let r = try await apiClient.api.searchHistory(limit: limit, offset: next)
            entries = PageBound.append(entries, r.rows)
            page = r.page
            moreError = nil
        } catch {
            moreError = ServerError.message(error)
        }
    }

    private func clear() async {
        clearing = true
        defer { clearing = false }
        do {
            try await apiClient.api.searchHistoryClear()
            error = nil
            Haptics.success()
            // Only the caller's own entries are gone; teammates' stay listed.
            entries = []
            await reload()
        } catch {
            self.error = ServerError.message(error)
            Haptics.error()
        }
    }
}
