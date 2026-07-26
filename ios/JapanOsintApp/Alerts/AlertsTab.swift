import SwiftUI

/// Console destination for alert rules. Mounted inside Console's
/// NavigationStack (RootView/ConsoleHub) so it doesn't own its own.
struct AlertsTab: View {
    @EnvironmentObject var settings: AppSettings
    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme

    @State private var rules: [AlertRule] = []
    @State private var loading = false
    @State private var error: String?
    /// Successful test-fire outcome. Separate from `error` so a delivered
    /// test reads as confirmation, not as a failure in the danger colour.
    @State private var testResult: String?
    @State private var editorRule: AlertRule?
    @State private var eventsRule: AlertRule?
    @State private var showCreate: Bool = false

    var body: some View {
        Group {
            if loading && rules.isEmpty {
                ProgressView().frame(maxWidth: .infinity, maxHeight: .infinity)
            } else if rules.isEmpty {
                emptyState
            } else {
                list
            }
        }
        .themedScreenBackground(theme)
        .navigationTitle("Alerts")
        .toolbar {
            ToolbarItem(placement: .compatPrimary) {
                Button { showCreate = true } label: { Image(systemName: "plus.circle.fill") }
                    .accessibilityLabel("New alert")
            }
            ToolbarItem(placement: .compatPrimary) {
                Button { Task { await reload() } } label: { Image(systemName: "arrow.clockwise") }
                    .disabled(loading)
            }
        }
        .task { if rules.isEmpty { await reload() } }
        .refreshable { await reload() }
        // Test-fire now performs real delivery, so its outcome needs a modal:
        // the inline `error` line only renders in the empty state, and a
        // rule you just tested is by definition not the empty state.
        .alert("Test delivery",
               isPresented: Binding(get: { testResult != nil },
                                    set: { if !$0 { testResult = nil } })) {
            Button("OK", role: .cancel) { testResult = nil }
        } message: {
            Text(testResult ?? "")
        }
        .alert("Test failed",
               isPresented: Binding(get: { error != nil && !rules.isEmpty },
                                    set: { if !$0 { error = nil } })) {
            Button("OK", role: .cancel) { error = nil }
        } message: {
            Text(error ?? "")
        }
        .sheet(isPresented: $showCreate) {
            AlertEditor(rule: AlertRule.blank, onSave: { saved in
                rules.insert(saved, at: 0)
            })
        }
        .sheet(item: $editorRule) { rule in
            AlertEditor(rule: rule, onSave: { saved in
                if let i = rules.firstIndex(where: { $0.id == saved.id }) {
                    rules[i] = saved
                }
            })
        }
        .sheet(item: $eventsRule) { rule in
            AlertEventsView(rule: rule)
        }
    }

    // MARK: - List

    private var list: some View {
        List {
            ForEach(rules) { rule in
                AlertRuleRow(
                    rule: rule,
                    onToggle: { Task { await toggle(rule) } },
                    onTap: { editorRule = rule },
                    onHistory: { eventsRule = rule },
                    onMute: { Task { await mute(rule, durationSec: 3600) } },
                    onUnmute: { Task { await unmute(rule) } },
                    onTest: { Task { await test(rule) } },
                    onDelete: { Task { await delete(rule) } }
                )
            }
        }
        .compatInsetGroupedListStyle()
        .scrollContentBackground(.hidden)
    }

    private var emptyState: some View {
        VStack(spacing: Space.md) {
            Image(systemName: "bell.badge")
                .font(.largeTitle)
                .foregroundStyle(theme.textMuted)
            Text("No alert rules yet")
                .font(.headline)
                .foregroundStyle(theme.text)
            Text("Get pinged when a new intel item matches your filter — phishing IOC, CVE near a CIDR, anything FTS-searchable.")
                .font(.caption)
                .foregroundStyle(theme.textMuted)
                .multilineTextAlignment(.center)
                .padding(.horizontal, Space.xl)
            Button { showCreate = true } label: {
                Label("Create your first rule", systemImage: "plus")
            }
            .buttonStyle(.borderedProminent)
            if let error {
                Text(error).font(.caption2).foregroundStyle(theme.danger)
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }

    // MARK: - Actions

    private func reload() async {
        loading = true
        defer { loading = false }
        do {
            rules = try await apiClient.api.alertsList()
            error = nil
        } catch let e {
            error = e.localizedDescription
        }
    }

    private func toggle(_ rule: AlertRule) async {
        guard let idx = rules.firstIndex(where: { $0.id == rule.id }) else { return }
        let original = rules[idx]
        // Optimistic: flip the switch immediately so the UI feels instant.
        var copy = rule
        copy.enabled.toggle()
        rules[idx] = copy
        Haptics.selection()
        do {
            let updated = try await apiClient.api.alertUpdate(copy)
            if let i = rules.firstIndex(where: { $0.id == updated.id }) { rules[i] = updated }
        } catch let e {
            // Roll back to the server-truth state.
            if let i = rules.firstIndex(where: { $0.id == original.id }) { rules[i] = original }
            error = e.localizedDescription
            Haptics.error()
        }
    }

    private func mute(_ rule: AlertRule, durationSec: Int?) async {
        guard let idx = rules.firstIndex(where: { $0.id == rule.id }) else { return }
        let original = rules[idx]
        // Optimistic: flip the row to muted right away; reload replaces this
        // placeholder with the server's authoritative muted_until.
        rules[idx].muted_until = ISO8601DateFormatter().string(
            from: Date().addingTimeInterval(Double(durationSec ?? 3600)))
        Haptics.selection()
        do {
            try await apiClient.api.alertMute(rule.id, durationSec: durationSec)
            await reload()
        } catch let e {
            if let i = rules.firstIndex(where: { $0.id == original.id }) { rules[i] = original }
            error = e.localizedDescription
            Haptics.error()
        }
    }

    private func unmute(_ rule: AlertRule) async {
        guard let idx = rules.firstIndex(where: { $0.id == rule.id }) else { return }
        let original = rules[idx]
        rules[idx].muted_until = nil
        Haptics.selection()
        do {
            try await apiClient.api.alertUnmute(rule.id)
            await reload()
        } catch let e {
            if let i = rules.firstIndex(where: { $0.id == original.id }) { rules[i] = original }
            error = e.localizedDescription
            Haptics.error()
        }
    }

    private func test(_ rule: AlertRule) async {
        do {
            let result = try await apiClient.api.alertTest(rule.id)
            // A 200 no longer means "delivered" — the request succeeded, but
            // an individual channel can still have been refused. Report the
            // channel outcome, since that is the only reason to press Test.
            if result.ok {
                testResult = result.summary
                Haptics.success()
            } else {
                error = result.summary
                Haptics.error()
            }
        } catch let e {
            error = e.localizedDescription
            Haptics.error()
        }
    }

    private func delete(_ rule: AlertRule) async {
        // Optimistic: drop the row immediately; restore it if the server
        // rejects the delete.
        let snapshot = rules
        rules.removeAll { $0.id == rule.id }
        Haptics.warning()
        do {
            try await apiClient.api.alertDelete(rule.id)
        } catch let e {
            rules = snapshot
            error = e.localizedDescription
            Haptics.error()
        }
    }
}

/// One row per rule. Shows name, channel count + types, predicate summary,
/// mute state. Tap opens the editor; swipe / context menu for actions.
struct AlertRuleRow: View {
    let rule: AlertRule
    let onToggle: () -> Void
    let onTap: () -> Void
    let onHistory: () -> Void
    let onMute: () -> Void
    let onUnmute: () -> Void
    let onTest: () -> Void
    let onDelete: () -> Void

    @Environment(\.theme) private var theme

    var body: some View {
        Button(action: onTap) {
            HStack(alignment: .top, spacing: Space.md) {
                Image(systemName: isMuted ? "bell.slash.fill" : (rule.enabled ? "bell.fill" : "bell.slash"))
                    .font(.body)
                    .foregroundStyle(iconColor)
                    .frame(width: 26, height: 26)
                    .background(iconColor.opacity(0.14), in: RoundedRectangle(cornerRadius: Radius.sm))
                VStack(alignment: .leading, spacing: 3) {
                    Text(rule.name)
                        .font(.subheadline.weight(.semibold))
                        .foregroundStyle(theme.text)
                        .lineLimit(1)
                    if !predicateSummary.isEmpty {
                        Text(predicateSummary)
                            .font(.caption2.monospaced())
                            .foregroundStyle(theme.textMuted)
                            .lineLimit(2)
                    }
                    HStack(spacing: Space.sm - 2) {
                        ForEach(rule.channels) { ch in
                            Pill(text: ch.type.rawValue.uppercased(),
                                 tone: ch.type == .email ? .info : .accent,
                                 icon: ch.type == .email ? "envelope" : "link",
                                 maxWidth: 110)
                        }
                        Spacer(minLength: 0)
                    }
                }
                Spacer(minLength: 0)
                Toggle("", isOn: Binding(get: { rule.enabled }, set: { _ in onToggle() }))
                    .labelsHidden()
            }
            .padding(.vertical, 2)
        }
        .buttonStyle(.plain)
        .swipeActions(edge: .leading, allowsFullSwipe: false) {
            Button(action: onHistory) { Label("History", systemImage: "clock.arrow.circlepath") }
                .tint(theme.accent)
            Button(action: onTest) { Label("Test", systemImage: "play.fill") }.tint(theme.accentAlt)
            if isMuted {
                Button(action: onUnmute) { Label("Unmute", systemImage: "bell") }.tint(theme.success)
            } else {
                Button(action: onMute) { Label("Mute 1h", systemImage: "bell.slash") }.tint(theme.warning)
            }
        }
        .swipeActions(edge: .trailing) {
            Button(role: .destructive, action: onDelete) {
                Label("Delete", systemImage: "trash")
            }
        }
        .contextMenu {
            Button { onHistory() } label: { Label("Firing history", systemImage: "clock.arrow.circlepath") }
            Button { onTest() } label: { Label("Test fire", systemImage: "play.fill") }
            if isMuted {
                Button { onUnmute() } label: { Label("Unmute", systemImage: "bell") }
            } else {
                Button { onMute() } label: { Label("Mute 1h", systemImage: "bell.slash") }
            }
            Button(role: .destructive) { onDelete() } label: { Label("Delete", systemImage: "trash") }
        }
    }

    private var isMuted: Bool {
        guard let until = rule.muted_until else { return false }
        return until > "now()" || !until.isEmpty   // server-truth; local string compare ok for "exists"
    }

    private var iconColor: Color {
        if !rule.enabled { return theme.textMuted }
        if isMuted        { return theme.warning }
        return theme.success
    }

    private var predicateSummary: String {
        var parts: [String] = []
        if let q = rule.predicate.q, !q.isEmpty { parts.append("q=\"\(q)\"") }
        if let s = rule.predicate.source_ids, !s.isEmpty { parts.append("src:\(s.prefix(2).joined(separator: ","))") }
        if let t = rule.predicate.tags_any, !t.isEmpty { parts.append("tag:\(t.prefix(2).joined(separator: ","))") }
        if rule.predicate.bbox?.count == 4 { parts.append("bbox") }
        return parts.joined(separator: " · ")
    }
}

extension AlertRule {
    /// Blank rule used by the create sheet. Server fills in id on POST.
    static var blank: AlertRule {
        AlertRule(
            id: "", name: "", enabled: true,
            predicate: AlertPredicate(),
            channels: [],
            dedup_window_sec: 3600,
            storm_cap_per_hour: 100,
            muted_until: nil,
            created_at: nil, updated_at: nil
        )
    }
}
