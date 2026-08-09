import SwiftUI

/// Create / edit form for an alert rule. Sheet-presented.
///
/// Fields:
///   - Name + enabled toggle (toggle hidden on Create until first save)
///   - Predicate: FTS query, comma-separated source-ids, comma-separated
///     tags (any-of)
///   - Channels: dynamic list of (type, target) rows. Webhook also takes
///     a 16-char-minimum signing secret.
///   - Throttle: dedup window seconds + storm cap per hour
///
/// Validation mirrors the server's `validateRule` — better to surface
/// errors before the round-trip.
struct AlertEditor: View {
    let onSave: (AlertRule) -> Void

    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme
    @Environment(\.dismiss) private var dismiss

    @State private var name: String
    @State private var enabled: Bool
    @State private var mode: String         // "fts" | "llm"
    @State private var nlQuery: String
    @State private var q: String
    @State private var sourcesCSV: String
    @State private var tagsCSV: String
    @State private var channels: [AlertChannel]
    @State private var dedupSec: Int
    @State private var stormCap: Int

    @State private var saving = false
    @State private var suggesting = false
    @State private var error: String?

    private let ruleId: String
    private let isCreate: Bool

    /// What the server substitutes for a stored webhook secret on reads
    /// (`core/alertsapi.c`, `one_rule`). Four U+2022 bullets — 12 UTF-8 bytes,
    /// which is *below* the server's 16-byte minimum, so echoing it back on a
    /// PATCH is an unconditional 400 and renaming a webhook rule was
    /// impossible.
    ///
    /// Dropping the `secret` key alone does NOT fix that: `validate_rule`
    /// requires a real ≥16-char secret on every webhook channel in whatever
    /// `channels` array the PATCH ends up merging, so an absent secret is
    /// rejected exactly like a short one. The merge is top-level, which means
    /// the only way to keep a stored secret is to omit `channels` entirely —
    /// see `API.alertUpdate(_:omittingChannels:)` and `channelsWereEdited`.
    private static let maskedSecret = "••••"

    /// The channel list as it arrived from the server, so `save()` can tell
    /// "the user left the channels alone" (safe to omit from the PATCH and let
    /// the stored secrets stand) from "the user edited them" (the whole array
    /// is replaced server-side, so every webhook in it needs a real secret).
    private let loadedChannels: [AlertChannel]

    init(rule: AlertRule, onSave: @escaping (AlertRule) -> Void) {
        self.onSave = onSave
        self.ruleId = rule.id
        self.isCreate = rule.id.isEmpty
        self.loadedChannels = rule.channels
        _name = State(initialValue: rule.name)
        _enabled = State(initialValue: rule.enabled)
        _mode = State(initialValue: rule.predicate.mode ?? "fts")
        _nlQuery = State(initialValue: rule.predicate.nl_query ?? "")
        _q = State(initialValue: rule.predicate.q ?? "")
        _sourcesCSV = State(initialValue: (rule.predicate.source_ids ?? []).joined(separator: ", "))
        _tagsCSV = State(initialValue: (rule.predicate.tags_any ?? []).joined(separator: ", "))
        _channels = State(initialValue: rule.channels)
        _dedupSec = State(initialValue: rule.dedup_window_sec)
        _stormCap = State(initialValue: rule.storm_cap_per_hour)
    }

    var body: some View {
        NavigationStack {
            Form {
                Section("Rule") {
                    TextField("Name", text: $name)
                    if !isCreate {
                        Toggle("Enabled", isOn: $enabled)
                    }
                }

                Section {
                    Picker("Query type", selection: $mode) {
                        Text("FTS").tag("fts")
                        Text("LLM pipeline").tag("llm")
                    }
                    .pickerStyle(.segmented)

                    if mode == "llm" {
                        TextField("Natural-language query (e.g. phishing targeting JP banks)",
                                  text: $nlQuery)
                            .compatNoAutocap()
                        Button {
                            Task { await suggestFTS() }
                        } label: {
                            Label(suggesting ? "Suggesting…" : "Suggest FTS query",
                                  systemImage: "sparkles")
                        }
                        .disabled(suggesting ||
                                  nlQuery.trimmingCharacters(in: .whitespaces).isEmpty)
                    }

                    TextField("FTS query (e.g. phishing AND tld:.jp)", text: $q)
                        .compatNoAutocap()
                        .autocorrectionDisabled()
                        .font(.system(.body, design: .monospaced))
                        .fontDesign(.monospaced)
                    TextField("Source IDs (comma-separated)", text: $sourcesCSV)
                        .compatNoAutocap()
                        .autocorrectionDisabled()
                        .font(.system(.body, design: .monospaced))
                        .fontDesign(.monospaced)
                    TextField("Tags any-of (comma-separated)", text: $tagsCSV)
                        .compatNoAutocap()
                        .autocorrectionDisabled()
                } header: {
                    Text("Match when…")
                } footer: {
                    Text(mode == "llm"
                         ? "LLM mode runs the agentic search pipeline on your natural-language query (ingesting new intel as usual) and fires on items matching the FTS query below. Tap “Suggest FTS query” to have the model draft it, then edit freely."
                         : "All filled fields combine with AND. Leave everything blank to match every new item (use a tight throttle if you do).")
                        .font(.caption2)
                }

                Section {
                    ForEach(Array(channels.enumerated()), id: \.offset) { idx, _ in
                        channelEditor(at: idx)
                    }
                    .onDelete { channels.remove(atOffsets: $0) }

                    Menu {
                        Button { channels.append(AlertChannel(type: .email, target: "", secret: nil)) }
                        label: { Label("Email", systemImage: "envelope") }
                        Button { channels.append(AlertChannel(type: .webhook, target: "", secret: "")) }
                        label: { Label("Webhook", systemImage: "link") }
                    } label: {
                        Label("Add channel", systemImage: "plus.circle")
                    }
                } header: {
                    Text("Deliver to")
                } footer: {
                    Text(isCreate
                         ? "Webhook receivers can verify each call's HMAC-SHA256 signature using the secret you paste below. Min 16 characters."
                         : "Webhook receivers can verify each call's HMAC-SHA256 signature. Min 16 characters. Leave the channels untouched to keep the stored secret; if you change any channel you must re-enter the secret, because the server replaces the whole channel list.")
                        .font(.caption2)
                }

                Section("Throttle") {
                    Stepper(value: $dedupSec, in: 0...86400, step: 300) {
                        HStack {
                            Text("Dedup window")
                            Spacer()
                            Text(formatSeconds(dedupSec))
                                .font(.caption.monospacedDigit())
                                .foregroundStyle(theme.textMuted)
                        }
                    }
                    Stepper(value: $stormCap, in: 1...10000, step: 10) {
                        HStack {
                            Text("Max fires / hour")
                            Spacer()
                            Text("\(stormCap)")
                                .font(.caption.monospacedDigit())
                                .foregroundStyle(theme.textMuted)
                        }
                    }
                }

                // Roadmap 10 — live backtest: "would have matched N items in
                // the last 7 days". Only once the draft has real criteria, so an
                // empty new rule doesn't kick off a full-corpus scan.
                if hasPreviewableCriteria {
                    RulePreviewSection(predicate: previewPredicate)
                }

                if let error {
                    Section {
                        Text(error).font(.caption).foregroundStyle(theme.danger)
                    }
                }
            }
            .navigationTitle(isCreate ? "New alert" : "Edit alert")
            .compatGroupedForm()
            .themedScreenBackground(theme)
            .compatInlineTitle()
            .toolbar {
                ToolbarItem(placement: .compatLeading) {
                    Button("Cancel") { dismiss() }
                }
                ToolbarItem(placement: .compatPrimary) {
                    Button(isCreate ? "Create" : "Save") {
                        Task { await save() }
                    }
                    .disabled(saving)
                }
            }
        }
        .compatSheetSizing(.large)
    }

    // MARK: - Channel row editor

    @ViewBuilder
    private func channelEditor(at idx: Int) -> some View {
        let ch = channels[idx]
        VStack(alignment: .leading, spacing: Space.sm - 2) {
            HStack(spacing: Space.sm) {
                Image(systemName: ch.type == .email ? "envelope.fill" : "link")
                    .foregroundStyle(theme.accent)
                Text(ch.type.label.uppercased())
                    .font(.caption2.bold())
                    .tracking(0.6)
                    .foregroundStyle(theme.accent)
                Spacer()
            }
            TextField(
                "",
                text: Binding(
                    get: { channels[idx].target },
                    set: { channels[idx].target = $0 }
                ),
                prompt: Text(ch.type == .email ? "email@example.com"
                                               : "https://example.com/hook")
                    .foregroundColor(theme.accent)
            )
            .compatNoAutocap()
            .autocorrectionDisabled()
            .compatKeyboard(email: ch.type == .email)
            .font(.system(.body, design: .monospaced))
            .fontDesign(.monospaced)

            if ch.type == .webhook {
                SecureField(
                    "",
                    text: Binding(
                        get: { channels[idx].secret ?? "" },
                        set: { channels[idx].secret = $0 }
                    ),
                    prompt: Text("Signing secret (≥16 chars)")
                        .foregroundColor(theme.accent)
                )
                .compatNoAutocap()
                .autocorrectionDisabled()
                .font(.system(.body, design: .monospaced))
                .fontDesign(.monospaced)
            }
        }
        .padding(.vertical, 2)
    }

    // MARK: - Backtest preview (roadmap 10)

    /// The draft predicate as a plain dict, the same shape `alertPreview` and
    /// the saved rule use. Mirrors the assembly in `save()` so the preview
    /// matches exactly what would be persisted.
    private var previewPredicate: [String: Any] {
        var p: [String: Any] = ["mode": mode]
        let trimQ = q.trimmingCharacters(in: .whitespaces)
        if !trimQ.isEmpty { p["q"] = trimQ }
        let srcs = splitCSV(sourcesCSV)
        if !srcs.isEmpty { p["source_ids"] = srcs }
        let tags = splitCSV(tagsCSV)
        if !tags.isEmpty { p["tags_any"] = tags }
        if mode == "llm" {
            let nl = nlQuery.trimmingCharacters(in: .whitespaces)
            if !nl.isEmpty { p["nl_query"] = nl }
        }
        return p
    }

    /// True once the draft has at least one real matching term — avoids firing a
    /// full-corpus backtest for an empty new rule.
    private var hasPreviewableCriteria: Bool {
        !q.trimmingCharacters(in: .whitespaces).isEmpty
            || !splitCSV(sourcesCSV).isEmpty
            || !splitCSV(tagsCSV).isEmpty
            || (mode == "llm" && !nlQuery.trimmingCharacters(in: .whitespaces).isEmpty)
    }

    // MARK: - Save

    private func save() async {
        saving = true
        defer { saving = false }

        let trimmedName = name.trimmingCharacters(in: .whitespaces)
        if trimmedName.isEmpty { error = "Name is required"; return }
        if channels.isEmpty { error = "Add at least one channel"; return }
        // "Untouched" = still carrying the server's mask (or blank). Only an
        // *edit* can leave that state behind, so on create it is always a
        // validation failure.
        let channelsWereEdited = isCreate || channels != loadedChannels
        var anyUntouchedWebhook = false
        for (i, ch) in channels.enumerated() {
            if ch.target.trimmingCharacters(in: .whitespaces).isEmpty {
                error = "Channel \(i + 1) is missing a target"; return
            }
            if ch.type == .webhook {
                let secret = (ch.secret ?? "").trimmingCharacters(in: .whitespaces)
                let untouched = !isCreate
                    && (secret == Self.maskedSecret || secret.isEmpty)
                if untouched {
                    anyUntouchedWebhook = true
                    // The whole array is replaced when `channels` is sent, and
                    // the server cannot merge a stored secret into it. Say so
                    // here rather than shipping a request that can only 400.
                    if channelsWereEdited {
                        error = "Re-enter the webhook secret for channel \(i + 1) — "
                              + "editing the channel list replaces the stored secret."
                        return
                    }
                } else if secret.count < 16 {
                    error = "Webhook secret must be at least 16 characters"; return
                }
            }
        }

        // Nothing about the channels changed and at least one webhook is still
        // on its mask ⇒ omit `channels` from the PATCH so the stored secrets
        // survive. Anything else sends the array as authored.
        let omitChannels = !isCreate && !channelsWereEdited && anyUntouchedWebhook

        var predicate = AlertPredicate()
        predicate.mode = mode
        let trimQ = q.trimmingCharacters(in: .whitespaces)
        if !trimQ.isEmpty { predicate.q = trimQ }
        let srcs = splitCSV(sourcesCSV)
        if !srcs.isEmpty { predicate.source_ids = srcs }
        let tags = splitCSV(tagsCSV)
        if !tags.isEmpty { predicate.tags_any = tags }
        if mode == "llm" {
            let nl = nlQuery.trimmingCharacters(in: .whitespaces)
            if !nl.isEmpty { predicate.nl_query = nl }
        }

        let rule = AlertRule(
            id: ruleId, name: trimmedName, enabled: enabled,
            predicate: predicate,
            channels: channels,
            dedup_window_sec: dedupSec,
            storm_cap_per_hour: stormCap,
            muted_until: nil,
            created_at: nil, updated_at: nil
        )

        do {
            let api = apiClient.api
            let saved = isCreate
                ? try await api.alertCreate(rule)
                : try await api.alertUpdate(rule, omittingChannels: omitChannels)
            onSave(saved)
            Haptics.success()
            dismiss()
        } catch let e {
            error = e.localizedDescription
            Haptics.error()
        }
    }

    /// Ask the backend's LLM suggester to draft an FTS query from the
    /// natural-language prompt; the user can then accept or edit it. This is
    /// the "choose the FTS search done by the LLM" step.
    private func suggestFTS() async {
        let nl = nlQuery.trimmingCharacters(in: .whitespaces)
        guard !nl.isEmpty else { return }
        suggesting = true
        defer { suggesting = false }
        do {
            let suggestions = try await apiClient.api.searchSuggest(nl)
            if let first = suggestions.first(where: { !$0.trimmingCharacters(in: .whitespaces).isEmpty }) {
                q = first
                error = nil
                Haptics.success()
            } else {
                error = "No FTS suggestion returned"
            }
        } catch let e {
            error = e.localizedDescription
            Haptics.error()
        }
    }

    private func splitCSV(_ s: String) -> [String] {
        s.split(separator: ",")
            .map { $0.trimmingCharacters(in: .whitespaces) }
            .filter { !$0.isEmpty }
    }

    private func formatSeconds(_ s: Int) -> String {
        if s == 0 { return "off" }
        if s < 60 { return "\(s)s" }
        if s < 3600 { return "\(s / 60)m" }
        return "\(s / 3600)h"
    }
}
