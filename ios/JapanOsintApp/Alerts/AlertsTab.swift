import SwiftUI

/// Console destination for alert rules. Mounted inside Console's
/// NavigationStack (RootView/ConsoleHub) so it doesn't own its own.
struct AlertsTab: View {
    @EnvironmentObject var settings: AppSettings
    @Environment(\.theme) private var theme

    @State private var rules: [AlertRule] = []
    @State private var loading = false
    @State private var error: String?
    @State private var editorRule: AlertRule?
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
        .background(theme.surface.ignoresSafeArea())
        .navigationTitle("Alerts")
        .toolbar {
            ToolbarItem(placement: .topBarTrailing) {
                Button { showCreate = true } label: { Image(systemName: "plus.circle.fill") }
                    .accessibilityLabel("New alert")
            }
            ToolbarItem(placement: .topBarTrailing) {
                Button { Task { await reload() } } label: { Image(systemName: "arrow.clockwise") }
                    .disabled(loading)
            }
        }
        .task { if rules.isEmpty { await reload() } }
        .refreshable { await reload() }
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
    }

    // MARK: - List

    private var list: some View {
        List {
            ForEach(rules) { rule in
                AlertRuleRow(
                    rule: rule,
                    onToggle: { Task { await toggle(rule) } },
                    onTap: { editorRule = rule },
                    onMute: { Task { await mute(rule, durationSec: 3600) } },
                    onUnmute: { Task { await unmute(rule) } },
                    onTest: { Task { await test(rule) } },
                    onDelete: { Task { await delete(rule) } }
                )
            }
        }
        .listStyle(.insetGrouped)
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
            let api = API(baseURL: settings.backendBaseURL)
            rules = try await api.alertsList()
            error = nil
        } catch let e {
            error = e.localizedDescription
        }
    }

    private func toggle(_ rule: AlertRule) async {
        var copy = rule
        copy.enabled.toggle()
        do {
            let updated = try await API(baseURL: settings.backendBaseURL).alertUpdate(copy)
            if let i = rules.firstIndex(where: { $0.id == updated.id }) { rules[i] = updated }
        } catch let e { error = e.localizedDescription }
    }

    private func mute(_ rule: AlertRule, durationSec: Int?) async {
        do {
            try await API(baseURL: settings.backendBaseURL).alertMute(rule.id, durationSec: durationSec)
            await reload()
        } catch let e { error = e.localizedDescription }
    }

    private func unmute(_ rule: AlertRule) async {
        do {
            try await API(baseURL: settings.backendBaseURL).alertUnmute(rule.id)
            await reload()
        } catch let e { error = e.localizedDescription }
    }

    private func test(_ rule: AlertRule) async {
        do {
            try await API(baseURL: settings.backendBaseURL).alertTest(rule.id)
            Haptics.success()
        } catch let e { error = e.localizedDescription }
    }

    private func delete(_ rule: AlertRule) async {
        do {
            try await API(baseURL: settings.backendBaseURL).alertDelete(rule.id)
            rules.removeAll { $0.id == rule.id }
        } catch let e { error = e.localizedDescription }
    }
}

/// One row per rule. Shows name, channel count + types, predicate summary,
/// mute state. Tap opens the editor; swipe / context menu for actions.
struct AlertRuleRow: View {
    let rule: AlertRule
    let onToggle: () -> Void
    let onTap: () -> Void
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
