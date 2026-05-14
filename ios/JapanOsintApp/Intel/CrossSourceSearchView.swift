import SwiftUI

/// Cross-source FTS results — used at the top of `IntelTab` when the search
/// bar has text. Returns flat list of items, grouped visually by source via
/// the source pill on each row.
///
/// Bilingual search: when `bilingual.translated` is set, the request carries
/// both queries as `q` and `qAlt`. The server merges results and tags
/// translation-only matches with `via_translation: true`. The header chip
/// shows the user what we're also searching for.
struct CrossSourceSearchView: View {
    let bilingual: BilingualQuery

    @EnvironmentObject var settings: AppSettings
    @Environment(\.theme) private var theme

    @State private var items: [IntelItem] = []
    @State private var loading = false
    @State private var error: String?
    @State private var lastKey: String?

    /// Composite of (original, translated) so the search task re-fires when
    /// either side changes — e.g. when the translation resolves a beat after
    /// the user typed.
    private var taskKey: String { "\(bilingual.original)|\(bilingual.translated ?? "")" }

    var body: some View {
        VStack(spacing: 0) {
            if bilingual.hasTranslation {
                translationBanner
            }
            content
        }
        .task(id: taskKey) { await search() }
    }

    @ViewBuilder
    private var content: some View {
        if loading && items.isEmpty {
            ProgressView("Searching…")
                .frame(maxWidth: .infinity, maxHeight: .infinity)
        } else if items.isEmpty {
            OfflineStateView(
                kind: .empty,
                title: "No matches for \"\(bilingual.original)\"",
                message: error,
                systemImage: "magnifyingglass"
            )
        } else {
            List {
                Section {
                    ForEach(items) { item in
                        NavigationLink(value: item) {
                            VStack(alignment: .leading, spacing: 4) {
                                HStack(spacing: 6) {
                                    Text(item.source_id)
                                        .font(.caption2.monospaced())
                                        .foregroundStyle(theme.accent)
                                    if item.via_translation == true {
                                        BilingualBadge(style: .compact)
                                    }
                                    Spacer(minLength: 0)
                                }
                                IntelItemRow(item: item)
                            }
                        }
                    }
                } header: {
                    resultsCountHeader
                }
            }
            .listStyle(.insetGrouped)
            .navigationDestination(for: IntelItem.self) { item in
                IntelDetail(uid: item.uid, fallbackTitle: item.title ?? item.uid)
            }
        }
    }

    /// "N results for "<query>"" — uses monospacedDigit so the count doesn't
    /// shift as more results stream in.
    private var resultsCountHeader: some View {
        HStack(spacing: 6) {
            Text("\(items.count) result\(items.count == 1 ? "" : "s")")
                .font(.caption.bold().monospacedDigit())
                .foregroundStyle(theme.text)
            Text("for")
                .font(.caption)
                .foregroundStyle(theme.textMuted)
            Text("\"\(bilingual.original)\"")
                .font(.caption.monospaced())
                .foregroundStyle(theme.accent)
                .lineLimit(1)
                .truncationMode(.middle)
            Spacer(minLength: 0)
        }
        .textCase(nil)
    }

    /// "Also searching: <translated> [Apple Translated]" — surfaces the
    /// translated query to the user so they understand why extra results
    /// appeared.
    private var translationBanner: some View {
        HStack(spacing: 6) {
            Text("Also searching:")
                .font(.caption2)
                .foregroundStyle(theme.textMuted)
            Text(bilingual.translated ?? "")
                .font(.system(.caption, design: .monospaced))
                .foregroundStyle(theme.text)
                .lineLimit(1)
            BilingualBadge(style: .full)
            Spacer(minLength: 0)
        }
        .padding(.horizontal, 14)
        .padding(.vertical, 6)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(theme.surfaceElevated.opacity(0.6))
    }

    private func search() async {
        guard taskKey != lastKey else { return }
        lastKey = taskKey
        let trimmed = bilingual.original.trimmingCharacters(in: .whitespaces)
        if trimmed.isEmpty {
            items = []
            return
        }
        loading = true
        defer { loading = false }
        try? await Task.sleep(for: .milliseconds(250))    // debounce
        guard taskKey == lastKey else { return }
        do {
            let env = try await API(baseURL: settings.backendBaseURL).intelItems(
                q: trimmed,
                qAlt: bilingual.translated,
                limit: 100
            )
            items = env.data
            error = nil
        } catch let err {
            error = err.localizedDescription
        }
    }
}
