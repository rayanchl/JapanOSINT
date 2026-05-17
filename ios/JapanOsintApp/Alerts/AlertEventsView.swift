import SwiftUI

/// Firing history for one alert rule (GET /api/alerts/:id/events).
///
/// Each row is one evaluation: the matched intel item (title joined
/// server-side), when it matched, which channels delivered, and — for
/// suppressed rows — why (dedup window or storm cap). Tapping a row with a
/// live item pushes the full intel detail.
///
/// Presented as a sheet from `AlertsTab`; owns its own NavigationStack so
/// the detail push works on phone and iPad alike.
struct AlertEventsView: View {
    let rule: AlertRule

    @EnvironmentObject private var apiClient: APIClient
    @Environment(\.theme) private var theme
    @Environment(\.dismiss) private var dismiss

    @State private var events: [AlertEvent] = []
    @State private var loading = false
    @State private var error: String?

    var body: some View {
        NavigationStack {
            Group {
                if loading && events.isEmpty {
                    ProgressView().frame(maxWidth: .infinity, maxHeight: .infinity)
                } else if events.isEmpty {
                    emptyState
                } else {
                    list
                }
            }
            .background(theme.surface.ignoresSafeArea())
            .navigationTitle("History · \(rule.name)")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .topBarLeading) {
                    Button("Done") { dismiss() }
                }
                ToolbarItem(placement: .topBarTrailing) {
                    Button { Task { await reload() } } label: {
                        Image(systemName: "arrow.clockwise")
                    }.disabled(loading)
                }
            }
            .navigationDestination(for: String.self) { uid in
                IntelDetail(uid: uid, fallbackTitle: "Matched item")
            }
        }
        .task { if events.isEmpty { await reload() } }
    }

    private var list: some View {
        List {
            ForEach(events) { ev in
                row(ev)
            }
        }
        .listStyle(.plain)
        .scrollContentBackground(.hidden)
        .refreshable { await reload() }
    }

    @ViewBuilder
    private func row(_ ev: AlertEvent) -> some View {
        let hasItem = ev.item_title != nil || !ev.item_uid.isEmpty
        let content = VStack(alignment: .leading, spacing: 4) {
            HStack(spacing: 6) {
                Image(systemName: ev.suppressed == 1 ? "bell.slash.fill" : "bell.fill")
                    .font(.caption2)
                    .foregroundStyle(ev.suppressed == 1 ? theme.warning : theme.success)
                Text(ev.item_title ?? ev.item_uid)
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(theme.text)
                    .lineLimit(2)
            }
            HStack(spacing: 6) {
                Text(relativeTime(ev.matched_at))
                    .font(.caption2.monospacedDigit())
                    .foregroundStyle(theme.textMuted)
                if let src = ev.item_source_id {
                    Text("· \(src)").font(.caption2.monospaced()).foregroundStyle(theme.textMuted)
                }
            }
            HStack(spacing: 5) {
                if ev.suppressed == 1 {
                    Pill(text: "SUPPRESSED", tone: .warning, icon: "nosign", maxWidth: 130)
                    if let r = ev.reason, !r.isEmpty {
                        Text(r).font(.caption2.monospaced()).foregroundStyle(theme.textMuted)
                    }
                } else if ev.delivered_channels.isEmpty {
                    Pill(text: "NO CHANNELS", tone: .info, icon: "minus.circle", maxWidth: 130)
                } else {
                    ForEach(ev.delivered_channels, id: \.self) { c in
                        Pill(text: c.uppercased(),
                             tone: c == "email" ? .info : .accent,
                             icon: c == "email" ? "envelope" : "link",
                             maxWidth: 110)
                    }
                }
                if let r = ev.reason, !r.isEmpty, ev.suppressed != 1 {
                    Text(r).font(.caption2).foregroundStyle(theme.danger).lineLimit(1)
                }
                Spacer(minLength: 0)
            }
        }
        .padding(.vertical, 4)

        if hasItem, !ev.item_uid.isEmpty {
            NavigationLink(value: ev.item_uid) { content }
                .listRowBackground(theme.surface)
        } else {
            content.listRowBackground(theme.surface)
        }
    }

    private var emptyState: some View {
        VStack(spacing: Space.md) {
            Image(systemName: "clock.badge.questionmark")
                .font(.largeTitle).foregroundStyle(theme.textMuted)
            Text("No firings yet")
                .font(.headline).foregroundStyle(theme.text)
            Text("This rule hasn't matched any new intel items. Use the Test action on the rule to send a synthetic event.")
                .font(.caption).foregroundStyle(theme.textMuted)
                .multilineTextAlignment(.center)
                .padding(.horizontal, Space.xl)
            if let error {
                Text(error).font(.caption2).foregroundStyle(theme.danger)
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }

    private func reload() async {
        loading = true
        defer { loading = false }
        do {
            events = try await apiClient.api.alertEvents(rule.id, limit: 200)
            error = nil
        } catch {
            self.error = error.localizedDescription
        }
    }
}
