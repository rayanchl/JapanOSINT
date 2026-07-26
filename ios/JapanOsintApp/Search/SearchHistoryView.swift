import SwiftUI
import Foundation

// ─────────────────────────────────────────────────────────────────────────────
// Roadmap item 38 — recent searches.
//
// THE PRIVACY LINE IS THE FEATURE, NOT A DISCLAIMER.
// `/api/search-history` carries `tenant_id AND user_id` on the read AND on the
// clear, and there is no privileged variant that drops the user predicate. What
// an analyst is investigating is not something a workspace owner should be able
// to read, and the clear is deliberately NOT audited — recording "user X erased
// their search history" in a table the owner reads would reinstate exactly the
// visibility the per-user scoping exists to prevent.
//
// That is a promise the product makes, so the UI states it in plain words where
// the user can see it, instead of leaving them to guess.
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
    /// How many rows to ask for. The server clamps to 200.
    var limit: Int = 100

    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme

    @State private var entries: [SearchHistoryEntry] = []
    @State private var loading = true
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
            Text("Deletes every entry below. Only your own history is affected, and nothing is recorded about the deletion. Saved searches are kept.")
        }
    }

    // MARK: - List

    private var list: some View {
        List {
            Section {
                privacyBanner
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
            } header: {
                Text("RECENT")
                    .font(Typography.display(10, weight: .semibold))
                    .tracking(1.2)
                    .foregroundStyle(theme.textMuted)
            } footer: {
                Text(onRerun == nil
                     ? "The server keeps only your most recent entries and drops older ones automatically."
                     : "Tap an entry to run it again. The server keeps only your most recent entries and drops older ones automatically.")
                    .font(.caption2)
            }
        }
        .listStyle(.plain)
        .scrollContentBackground(.hidden)
    }

    private var privacyBanner: some View {
        HStack(alignment: .top, spacing: Space.md) {
            Image(systemName: "lock.fill")
                .font(.subheadline)
                .foregroundStyle(theme.accent)
                .frame(width: 24)
            VStack(alignment: .leading, spacing: 2) {
                Text("Private to you")
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(theme.text)
                Text("Search history is scoped to your account. Workspace owners and admins cannot read what you have been investigating, and clearing it is not logged.")
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
                    Spacer(minLength: 0)
                }
            }
            if showsChevron {
                Image(systemName: "arrow.counterclockwise")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
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
            }
        } description: {
            Text("Searches you run are recorded here, privately — only your account can read this list.")
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

    private func reload() async {
        loading = true
        defer { loading = false }
        do {
            entries = try await apiClient.api.searchHistory(limit: limit)
            error = nil
        } catch {
            self.error = ServerError.message(error)
            Haptics.error()
        }
    }

    private func clear() async {
        clearing = true
        defer { clearing = false }
        do {
            try await apiClient.api.searchHistoryClear()
            entries = []
            error = nil
            Haptics.success()
        } catch {
            self.error = ServerError.message(error)
            Haptics.error()
        }
    }
}
