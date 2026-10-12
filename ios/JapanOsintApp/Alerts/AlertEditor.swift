import SwiftUI

/// Create / edit form for an alert rule. Sheet-presented.
///
/// Fields:
///   - Name + enabled toggle (toggle hidden on Create until first save)
///   - Predicate: keywords (FTS) or a description in words (a semantic rule,
///     matched by embedding similarity), plus comma-separated source-ids and
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
    /// Semantic rules: the cosine an item must reach (server default 0.82).
    @State private var threshold: Double
    @State private var q: String
    @State private var sourcesCSV: String
    @State private var tagsCSV: String
    @State private var channels: [AlertChannel]
    @State private var dedupSec: Int
    @State private var stormCap: Int

    @State private var saving = false
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

    /// The predicate exactly as it arrived. `save()` starts from THIS and
    /// overwrites only the five terms the form owns.
    ///
    /// The server replaces the predicate wholesale — `core/alertsapi.c` does
    /// `cJSON_Duplicate(bp,1)` whenever the PATCH body carries one — so building
    /// a fresh `AlertPredicate()` here silently deleted every term this editor
    /// has no control for: `bbox`, `polygon`, `circle`, `aoi_id`, `tags_all`,
    /// `entity_ids`, `entity_types`, `record_types`. Renaming a geofenced rule
    /// un-geofenced it, and nothing said so. Keeping the loaded value also lets
    /// a caller PREFILL a spatial term (see `AlertRule.blank(geofencedBy:)`) and
    /// have it survive Create.
    private let loadedPredicate: AlertPredicate

    /// The server's word on a saved semantic rule: whether it can fire yet.
    private let loadedSemantic: AlertSemanticStatus?

    /// The server's default similarity threshold (core/alert_sem.h).
    private static let defaultThreshold = 0.82

    init(rule: AlertRule, onSave: @escaping (AlertRule) -> Void) {
        self.onSave = onSave
        self.ruleId = rule.id
        self.isCreate = rule.id.isEmpty
        self.loadedChannels = rule.channels
        self.loadedPredicate = rule.predicate
        self.loadedSemantic = rule.semantic
        _name = State(initialValue: rule.name)
        _enabled = State(initialValue: rule.enabled)
        _mode = State(initialValue: rule.predicate.mode ?? "fts")
        _nlQuery = State(initialValue: rule.predicate.nl_query ?? "")
        _threshold = State(initialValue: rule.predicate.threshold ?? Self.defaultThreshold)
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
                    Picker("Match by", selection: $mode) {
                        Text("Keywords").tag("fts")
                        Text("Describe it in words").tag("llm")
                    }
                    .pickerStyle(.segmented)

                    if mode == "llm" {
                        TextField("What should fire this alert? (e.g. phishing aimed at Japanese banks)",
                                  text: $nlQuery)
                            .compatNoAutocap()
                        VStack(alignment: .leading, spacing: 2) {
                            HStack {
                                Text("Similarity needed")
                                Spacer()
                                Text(String(format: "%.2f", threshold))
                                    .font(.caption.monospacedDigit())
                                    .foregroundStyle(theme.textMuted)
                            }
                            Slider(value: $threshold, in: 0.50...0.99, step: 0.01)
                                .accessibilityLabel("Similarity needed")
                                .accessibilityValue(String(format: "%.2f", threshold))
                        }
                        if let sem = loadedSemantic, sem.evaluable == false {
                            Label(sem.reason ?? "This rule cannot fire yet.",
                                  systemImage: "exclamationmark.triangle")
                                .font(.caption)
                                .foregroundStyle(theme.warning)
                                .fixedSize(horizontal: false, vertical: true)
                        }
                    }

                    TextField(mode == "llm" ? "Also require keywords (optional)"
                                            : "FTS query (e.g. phishing AND tld:.jp)", text: $q)
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
                         ? "Fires on a new item that means what you described — its embedding is at least the similarity above to your description — and that also matches every other field filled in here. Items are checked after the server embeds them (minutes after ingest), and items outside the embedding window are never checked. Higher similarity fires less often."
                         : "All filled fields combine with AND. Leave everything blank to match every new item (use a tight throttle if you do).")
                        .font(.caption2)
                }

                if !preservedTerms.isEmpty {
                    Section {
                        ForEach(preservedTerms, id: \.self) { term in
                            Label(term, systemImage: "lock")
                                .font(.caption)
                                .foregroundStyle(theme.textMuted)
                        }
                    } header: {
                        Text("Also matching")
                    } footer: {
                        Text("This rule carries terms this form has no control for. They are kept exactly as they are when you save — they are listed here so \"save\" never looks like it dropped them.")
                            .font(.caption2)
                    }
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
                    .accessibilityHidden(true)   // the channel type follows
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

    // MARK: - Preserved (uneditable) predicate terms

    /// Human names for every loaded predicate term the form cannot edit. Shown
    /// read-only so the analyst can see the rule is more than what is on screen.
    private var preservedTerms: [String] {
        let p = loadedPredicate
        var out: [String] = []
        if let b = p.bbox, b.count == 4 {
            out.append(String(format: "Inside bbox %.3f, %.3f → %.3f, %.3f",
                              b[0], b[1], b[2], b[3]))
        }
        if let ring = p.polygon, !ring.isEmpty {
            out.append("Inside a \(ring.count)-point polygon")
        }
        if let c = p.circle {
            out.append(String(format: "Within %@ of lat %.3f, lon %.3f",
                              DrawnAOI.formatRadius(c.radius_m), c.lat, c.lon))
        }
        if let id = p.aoi_id, !id.isEmpty { out.append("Inside saved area \(id)") }
        if let t = p.tags_all, !t.isEmpty {
            out.append("All of these tags: \(t.joined(separator: ", "))")
        }
        if let e = p.entity_ids, !e.isEmpty {
            out.append("\(e.count) watched entit\(e.count == 1 ? "y" : "ies")")
        }
        if let t = p.entity_types, !t.isEmpty {
            out.append("Entity types: \(t.joined(separator: ", "))")
        }
        // `record_types` is deliberately included even though the C matcher
        // ignores it (see Models.swift): it round-trips, and hiding a stored key
        // would make "save" look lossy the next time someone diffs the rule.
        if let t = p.record_types, !t.isEmpty {
            out.append("Record types (not enforced by the server): \(t.joined(separator: ", "))")
        }
        return out
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
            p["threshold"] = roundedThreshold
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

        // Start from what was loaded, not from a blank — see `loadedPredicate`.
        // Every field below is assigned unconditionally (nil when the control is
        // empty) so clearing a box in the form still clears the term; every
        // field NOT named here is carried through untouched.
        var predicate = loadedPredicate
        predicate.mode = mode
        let trimQ = q.trimmingCharacters(in: .whitespaces)
        predicate.q = trimQ.isEmpty ? nil : trimQ
        let srcs = splitCSV(sourcesCSV)
        predicate.source_ids = srcs.isEmpty ? nil : srcs
        let tags = splitCSV(tagsCSV)
        predicate.tags_any = tags.isEmpty ? nil : tags
        let nl = nlQuery.trimmingCharacters(in: .whitespaces)
        // `nl_query` only means anything in llm mode, but it is preserved rather
        // than dropped when the user switches back to fts — switching modes is
        // not a request to throw the prompt away.
        predicate.nl_query = nl.isEmpty ? nil : nl
        // The threshold belongs to the description: set in "words" mode, kept
        // as loaded otherwise (switching modes is not a request to forget it).
        if mode == "llm" { predicate.threshold = roundedThreshold }

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

    /// The slider's value as sent: two decimals, so a saved rule reads back
    /// as the number the analyst saw.
    private var roundedThreshold: Double {
        (threshold * 100).rounded() / 100
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
