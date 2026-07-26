import SwiftUI

// ─────────────────────────────────────────────────────────────────────────────
// Cross-rule notification inbox (roadmap 11).
//
// `AlertEventsView` answers "what has THIS rule done"; this screen answers
// "what happened while I was away", across every rule, newest first, with a
// read/unread state that survives on the server.
//
// Mounting: this view does NOT own a `NavigationStack` — mount it inside one
// (the same way `AlertsTab` is mounted inside ConsoleHub's stack). It registers
// a `navigationDestination` for `AlertInboxTarget` rather than for `String`, so
// it cannot collide with a host stack that already routes plain `String`s.
//
// The model is injected, not created here, because the tab badge has to read
// the same `unreadCount` the list is showing.
//
// Breach-monitor hits (roadmap 24) arrive with a synthetic `breach:` uid; the
// server resolves their title/source/link through the breach adapter, so this
// view just renders what came back and degrades to the uid when a field is
// null. `IntelDetail` already knows how to open a `breach:` uid.
// ─────────────────────────────────────────────────────────────────────────────

/// Push target for a matched item. A dedicated type (rather than `String`) so
/// registering the destination can never hijack another screen's String route.
struct AlertInboxTarget: Hashable {
    let uid: String
    let title: String
}

struct AlertInboxView: View {
    @ObservedObject var model: AlertInboxModel

    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme

    /// Mute presets offered from the row context menu. Kept short and concrete:
    /// each one ends by itself, which is what makes muting obviously temporary.
    struct MutePreset: Identifiable {
        let label: String
        let seconds: Int
        var id: Int { seconds }
    }

    private static let mutePresets: [MutePreset] = [
        MutePreset(label: "1 hour",   seconds: 3600),
        MutePreset(label: "8 hours",  seconds: 28800),
        MutePreset(label: "24 hours", seconds: 86400)
    ]

    var body: some View {
        Group {
            if model.loading && model.events.isEmpty {
                ProgressView().frame(maxWidth: .infinity, maxHeight: .infinity)
            } else if model.events.isEmpty {
                emptyState
            } else {
                list
            }
        }
        .safeAreaInset(edge: .top, spacing: 0) { header }
        .themedScreenBackground(theme)
        .navigationTitle("Inbox")
        .toolbar {
            ToolbarItem(placement: .compatPrimary) {
                Button {
                    Task { await model.markAllRead(api: apiClient.api) }
                } label: {
                    Image(systemName: "envelope.open")
                }
                .disabled(model.unreadCount == 0)
                .accessibilityLabel("Mark all read")
            }
            ToolbarItem(placement: .compatPrimary) {
                Button {
                    Task { await model.reload(api: apiClient.api) }
                } label: {
                    Image(systemName: "arrow.clockwise")
                }
                .disabled(model.loading)
                .accessibilityLabel("Refresh inbox")
            }
        }
        .task {
            if model.events.isEmpty { await model.reload(api: apiClient.api) }
        }
        .onChange(of: model.filter) { _, _ in
            Task { await model.reload(api: apiClient.api) }
        }
        .navigationDestination(for: AlertInboxTarget.self) { target in
            IntelDetail(uid: target.uid, fallbackTitle: target.title)
        }
        .alert("Inbox error",
               isPresented: Binding(get: { model.error != nil },
                                    set: { if !$0 { model.error = nil } })) {
            Button("OK", role: .cancel) { model.error = nil }
        } message: {
            Text(model.error ?? "")
        }
    }

    // MARK: - Header (filter + unread summary + mute undo)

    private var header: some View {
        VStack(spacing: Space.sm) {
            HStack(spacing: Space.md) {
                Picker("Filter", selection: $model.filter) {
                    ForEach(AlertInboxModel.Filter.allCases) { f in
                        Text(f.label).tag(f)
                    }
                }
                .pickerStyle(.segmented)
                .frame(maxWidth: 220)

                Spacer(minLength: 0)

                if model.unreadCount > 0 {
                    Pill(text: "\(model.unreadCount) UNREAD",
                         tone: .accent, size: .md,
                         icon: "circlebadge.fill", solid: true)
                        .monospacedDigit()
                } else {
                    Pill(text: "ALL READ", tone: .success, icon: "checkmark")
                }
            }

            if let mute = model.lastMute { muteBanner(mute) }
        }
        .padding(.horizontal, Space.lg)
        .padding(.vertical, Space.sm)
        .background(theme.surface)
    }

    /// Reversible by construction: the banner names the rule, states the window,
    /// says the rule still exists, and offers the undo inline.
    private func muteBanner(_ mute: AlertInboxModel.MutedRule) -> some View {
        HStack(alignment: .top, spacing: Space.sm) {
            Image(systemName: "bell.slash.fill")
                .font(.caption)
                .foregroundStyle(theme.warning)
            VStack(alignment: .leading, spacing: 2) {
                Text("Muted “\(mute.ruleName)” for \(mute.durationLabel)")
                    .font(.caption.weight(.semibold))
                    .foregroundStyle(theme.text)
                Text("The rule still exists and still evaluates — it just won't notify you until the mute expires.")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
                    .fixedSize(horizontal: false, vertical: true)
            }
            Spacer(minLength: 0)
            VStack(spacing: Space.xs) {
                Button("Unmute") {
                    Task { await model.unmute(ruleId: mute.ruleId, api: apiClient.api) }
                }
                .font(.caption.weight(.semibold))
                .buttonStyle(.plain)
                .foregroundStyle(theme.accent)
                Button {
                    model.dismissMuteBanner()
                } label: {
                    Image(systemName: "xmark.circle.fill")
                        .foregroundStyle(theme.textMuted)
                }
                .buttonStyle(.plain)
                .accessibilityLabel("Dismiss")
            }
        }
        .padding(Space.sm)
        .background(theme.warning.opacity(0.12),
                    in: RoundedRectangle(cornerRadius: Radius.md, style: .continuous))
    }

    // MARK: - List

    private var list: some View {
        List {
            ForEach(model.events) { ev in
                row(ev)
            }
        }
        .listStyle(.plain)
        .scrollContentBackground(.hidden)
        .refreshable { await model.reload(api: apiClient.api) }
    }

    @ViewBuilder
    private func row(_ ev: AlertInboxEvent) -> some View {
        NavigationLink(value: AlertInboxTarget(uid: ev.item_uid,
                                               title: displayTitle(ev))) {
            AlertInboxRow(event: ev,
                          title: displayTitle(ev),
                          muted: model.isMuted(ev.rule_id))
        }
        // Opening a notification is the normal way to consume it. The tap is
        // *simultaneous* with the link so navigation is never blocked; the
        // swipe action below is the deliberate path when you don't want to open.
        .simultaneousGesture(TapGesture().onEnded {
            Task { await model.markRead(ev, api: apiClient.api) }
        })
        .listRowBackground(ev.unread ? theme.accent.opacity(0.10) : theme.surface)
        .swipeActions(edge: .leading, allowsFullSwipe: true) {
            if ev.unread {
                Button {
                    Task { await model.markRead(ev, api: apiClient.api) }
                } label: {
                    Label("Read", systemImage: "envelope.open")
                }
                .tint(theme.accent)
            }
        }
        .swipeActions(edge: .trailing, allowsFullSwipe: false) {
            if model.isMuted(ev.rule_id) {
                Button {
                    Task { await model.unmute(ruleId: ev.rule_id, api: apiClient.api) }
                } label: {
                    Label("Unmute rule", systemImage: "bell")
                }
                .tint(theme.success)
            } else {
                Button {
                    Task {
                        await model.mute(ruleId: ev.rule_id,
                                         ruleName: ev.rule_name ?? "this rule",
                                         durationSec: 3600,
                                         durationLabel: "1 hour",
                                         api: apiClient.api)
                    }
                } label: {
                    Label("Mute 1h", systemImage: "bell.slash")
                }
                .tint(theme.warning)
            }
        }
        .contextMenu {
            if ev.unread {
                Button {
                    Task { await model.markRead(ev, api: apiClient.api) }
                } label: {
                    Label("Mark read", systemImage: "envelope.open")
                }
            }
            if model.isMuted(ev.rule_id) {
                Button {
                    Task { await model.unmute(ruleId: ev.rule_id, api: apiClient.api) }
                } label: {
                    Label("Unmute rule", systemImage: "bell")
                }
            } else {
                Menu {
                    ForEach(Self.mutePresets) { preset in
                        Button(preset.label) {
                            Task {
                                await model.mute(ruleId: ev.rule_id,
                                                 ruleName: ev.rule_name ?? "this rule",
                                                 durationSec: preset.seconds,
                                                 durationLabel: preset.label,
                                                 api: apiClient.api)
                            }
                        }
                    }
                } label: {
                    Label("Mute this rule for…", systemImage: "bell.slash")
                }
            }
            if let link = ev.item_link, !link.isEmpty {
                Button {
                    Clipboard.copy(link)
                } label: {
                    Label("Copy link", systemImage: "doc.on.doc")
                }
            }
            Button {
                Clipboard.copy(ev.item_uid)
            } label: {
                Label("Copy item id", systemImage: "number")
            }
        }
    }

    // MARK: - Empty state

    private var emptyState: some View {
        ContentUnavailableView {
            Label(model.filter == .unread ? "Nothing unread" : "No notifications",
                  systemImage: model.filter == .unread ? "checkmark.circle" : "tray")
                .foregroundStyle(theme.text)
        } description: {
            Text(model.filter == .unread
                 ? "Every alert you've received has been read. Switch to All to see the history."
                 : "When one of your alert rules matches a new item, it lands here — across every rule, newest first.")
                .foregroundStyle(theme.textMuted)
        } actions: {
            if model.filter == .unread {
                Button("Show all") { model.filter = .all }
                    .buttonStyle(.borderedProminent)
            }
        }
    }

    // MARK: - Helpers

    /// Server-resolved title when there is one. A breach-monitor hit whose
    /// preview could not be resolved gets a plain description rather than a raw
    /// `breach:email:…` uid, which tells the user nothing.
    private func displayTitle(_ ev: AlertInboxEvent) -> String {
        if let t = ev.item_title, !t.isEmpty { return t }
        if ev.item_uid.hasPrefix("breach:") { return "Breach exposure match" }
        return ev.item_uid.isEmpty ? "Matched item" : ev.item_uid
    }
}

// MARK: - Row

/// One inbox row. The unread/read distinction is carried by three independent
/// signals — a filled leading dot, the title's weight, and the row tint — so it
/// survives colour-blindness, greyscale and a glance.
struct AlertInboxRow: View {
    let event: AlertInboxEvent
    let title: String
    let muted: Bool

    @Environment(\.theme) private var theme

    var body: some View {
        HStack(alignment: .top, spacing: Space.sm) {
            unreadDot
            VStack(alignment: .leading, spacing: 3) {
                Text(title)
                    .font(event.unread ? .subheadline.weight(.semibold)
                                       : .subheadline)
                    .foregroundStyle(event.unread ? theme.text : theme.textMuted)
                    .lineLimit(2)

                HStack(spacing: Space.xs) {
                    Pill(text: (event.rule_name ?? "Deleted rule").uppercased(),
                         tone: .accent, icon: "bell.fill", maxWidth: 150)
                    Text(relativeTime(event.matched_at))
                        .font(.caption2.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                    if let src = event.item_source_id, !src.isEmpty {
                        Text("· \(src)")
                            .font(.caption2.monospaced())
                            .foregroundStyle(theme.textMuted)
                            .lineLimit(1)
                    }
                    Spacer(minLength: 0)
                }

                HStack(spacing: Space.xs) {
                    if isBreach {
                        Pill(text: "BREACH", tone: .danger,
                             icon: "lock.trianglebadge.exclamationmark", maxWidth: 110)
                    }
                    if event.delivered_channels.isEmpty {
                        // Matches AlertEventsView: no channel fired is a real
                        // outcome (rule has no channels, or delivery is still
                        // queued) — not the same thing as a successful send.
                        Pill(text: "NO CHANNELS", tone: .info,
                             icon: "minus.circle", maxWidth: 130)
                    } else {
                        ForEach(event.delivered_channels, id: \.self) { c in
                            Pill(text: c.uppercased(),
                                 tone: c == "email" ? .info : .accent,
                                 icon: c == "email" ? "envelope" : "link",
                                 maxWidth: 110)
                        }
                    }
                    if muted {
                        Pill(text: "RULE MUTED", tone: .warning,
                             icon: "bell.slash.fill", maxWidth: 130)
                    }
                    Spacer(minLength: 0)
                }
            }
        }
        .padding(.vertical, 4)
        .accessibilityElement(children: .combine)
        .accessibilityLabel(event.unread ? "Unread. \(title)" : title)
    }

    private var isBreach: Bool { event.item_uid.hasPrefix("breach:") }

    @ViewBuilder
    private var unreadDot: some View {
        // Always occupies the same space so read and unread rows stay aligned.
        Circle()
            .fill(event.unread ? theme.accent : Color.clear)
            .frame(width: 8, height: 8)
            .padding(.top, 6)
            .accessibilityHidden(true)
    }
}
