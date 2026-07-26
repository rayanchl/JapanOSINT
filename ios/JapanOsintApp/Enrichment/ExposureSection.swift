import SwiftUI

// ─────────────────────────────────────────────────────────────────────────────
// Roadmap 23 — breach exposure for one entity.
//
// Fetches `GET /api/entities/:type/:id/breaches` via `API.entityBreaches`,
// which returns both the per-breach rows and an aggregate `exposure` block.
// The section answers three questions in order: how exposed is this entity
// (summary line), what KIND of data leaked (aggregated data-class chips), and
// which breaches were they (the list).
//
// NO SECRET IS EVER RENDERED HERE. The endpoint does not return one, and this
// view deliberately says so in its footer rather than leaving a gap a reader
// might interpret as "a password is behind a tap". The operator-gated reveal
// path lives on the breach record itself (IntelDetail), not on exposure.
//
// Deep link: each row carries `item_uid` — the uid of the breach record in the
// intel corpus. This view does not know where that should go, so it calls
// `onOpenBreachItem`; the host supplies the destination.
// ─────────────────────────────────────────────────────────────────────────────

struct ExposureSection: View {
    /// Entity type as the API expects it (e.g. "email", "domain", "username").
    let entityType: String
    /// The resolved `entity_id`, not a display value.
    let entityId: String
    var title: String = "Breach exposure"
    var limit: Int = 50
    /// Called with `EntityBreach.item_uid`. Rows without a uid are rendered
    /// non-tappable rather than pushing nowhere.
    var onOpenBreachItem: ((String) -> Void)? = nil

    @EnvironmentObject private var apiClient: APIClient
    @Environment(\.theme) private var theme

    @State private var breaches: [EntityBreach] = []
    @State private var exposure: EntityExposure?
    @State private var loading = false
    @State private var error: String?

    var body: some View {
        VStack(alignment: .leading, spacing: Space.md) {
            header

            if loading && breaches.isEmpty {
                ProgressView()
                    .frame(maxWidth: .infinity, alignment: .center)
                    .padding(.vertical, Space.lg)
            } else if let error {
                errorState(error)
            } else if breaches.isEmpty && (exposure?.breach_count ?? 0) == 0 {
                emptyState
            } else {
                summaryLine
                if !aggregatedClasses.isEmpty { classChips }
                breachList
                if truncatedNote != nil { truncationRow }
            }

            footer
        }
        // Keyed on the entity so pushing a second entity onto the stack refetches.
        .task(id: entityKey) { await reload() }
    }

    private var entityKey: String { "\(entityType)|\(entityId)" }

    // MARK: - Header / footer

    private var header: some View {
        HStack(spacing: Space.sm) {
            Image(systemName: "lock.trianglebadge.exclamationmark")
                .font(.subheadline)
                .foregroundStyle(breaches.isEmpty ? theme.textMuted : theme.danger)
            Text(title)
                .font(.headline)
                .foregroundStyle(theme.text)
            Spacer(minLength: 0)
            if loading && !breaches.isEmpty {
                ProgressView().controlSize(.mini)
            }
            Button {
                Task { await reload() }
            } label: {
                Image(systemName: "arrow.clockwise")
                    .font(.caption)
            }
            .buttonStyle(.plain)
            .foregroundStyle(theme.accent)
            .disabled(loading)
            .accessibilityLabel("Reload breach exposure")
        }
    }

    private var footer: some View {
        Text("Exposure is derived from breach corpus metadata. No passwords or other secrets are returned by this endpoint.")
            .font(.caption2)
            .foregroundStyle(theme.textMuted)
            .fixedSize(horizontal: false, vertical: true)
    }

    // MARK: - Summary

    private var summaryLine: some View {
        Text(summaryText)
            .font(.subheadline)
            .foregroundStyle(theme.text)
            .fixedSize(horizontal: false, vertical: true)
    }

    private var summaryText: String {
        let n = exposure?.breach_count ?? breaches.count
        let noun = n == 1 ? "breach" : "breaches"
        if let span = yearSpan {
            return "Seen in \(n) \(noun), \(span)."
        }
        return "Seen in \(n) \(noun)."
    }

    /// "2019–2024", or a single year when first and last coincide. Falls back
    /// to the row dates when the aggregate omitted them.
    private var yearSpan: String? {
        let first = exposureYear(exposure?.first_breach_date)
            ?? breaches.compactMap { exposureYear($0.breach_date) }.min()
        let last = exposureYear(exposure?.last_breach_date)
            ?? breaches.compactMap { exposureYear($0.breach_date) }.max()
        guard let first, let last else { return nil }
        return first == last ? "\(first)" : "\(first)–\(last)"
    }

    /// Aggregated across the whole exposure when the server supplied it, else
    /// unioned from the rows we have — never silently narrower than the data.
    private var aggregatedClasses: [String] {
        if let dc = exposure?.data_classes, !dc.isEmpty { return dc }
        var seen = Set<String>()
        var out: [String] = []
        for b in breaches {
            for c in b.data_classes where !seen.contains(c) {
                seen.insert(c)
                out.append(c)
            }
        }
        return out.sorted()
    }

    private var classChips: some View {
        VStack(alignment: .leading, spacing: Space.xs) {
            Text("Data exposed")
                .font(.caption2.weight(.semibold))
                .foregroundStyle(theme.textMuted)
            FlowLayout(spacing: Space.xs) {
                ForEach(aggregatedClasses, id: \.self) { c in
                    Pill(text: c, tone: exposureClassTone(c), maxWidth: 220)
                }
            }
        }
    }

    // MARK: - List

    private var breachList: some View {
        VStack(spacing: Space.sm) {
            ForEach(breaches) { b in
                row(b)
            }
        }
    }

    @ViewBuilder
    private func row(_ b: EntityBreach) -> some View {
        let card = rowCard(b)
        if let uid = b.item_uid, !uid.isEmpty, let open = onOpenBreachItem {
            Button {
                Haptics.selection()
                open(uid)
            } label: {
                card
            }
            .buttonStyle(.plain)
            .compatHoverHighlight(cornerRadius: Radius.md)
            .accessibilityHint("Opens the breach record")
        } else {
            card
        }
    }

    private func rowCard(_ b: EntityBreach) -> some View {
        VStack(alignment: .leading, spacing: Space.xs) {
            HStack(alignment: .firstTextBaseline, spacing: Space.sm) {
                Text(b.title ?? b.name ?? b.breach_id)
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(theme.text)
                    .lineLimit(2)
                Spacer(minLength: 0)
                if b.verified == true {
                    Pill(text: "VERIFIED", tone: .success,
                         icon: "checkmark.seal", maxWidth: 110)
                } else {
                    // Not "fake" — simply not corroborated by the corpus.
                    Pill(text: "UNVERIFIED", tone: .neutral,
                         icon: "questionmark.circle", maxWidth: 120)
                }
                if b.sensitive == true {
                    Pill(text: "SENSITIVE", tone: .danger,
                         icon: "eye.slash", maxWidth: 110)
                }
            }

            HStack(spacing: Space.sm) {
                if let d = b.domain, !d.isEmpty {
                    Text(d)
                        .font(.caption2.monospaced())
                        .foregroundStyle(theme.textMuted)
                        .lineLimit(1)
                }
                if let date = b.breach_date, !date.isEmpty {
                    Text(date)
                        .font(.caption2.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                }
                if let n = b.pwn_count {
                    Text("\(n.formatted()) accounts")
                        .font(.caption2.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                }
                Spacer(minLength: 0)
                if b.item_uid == nil, onOpenBreachItem != nil {
                    Text("no linked record")
                        .font(.system(size: 9))
                        .foregroundStyle(theme.textMuted)
                } else if onOpenBreachItem != nil {
                    Image(systemName: "chevron.right")
                        .font(.caption2)
                        .foregroundStyle(theme.textMuted)
                }
            }

            if !b.data_classes.isEmpty {
                FlowLayout(spacing: Space.xs) {
                    ForEach(exposureVisibleClasses(b.data_classes), id: \.self) { c in
                        Pill(text: c, tone: exposureClassTone(c), maxWidth: 200)
                    }
                    if b.data_classes.count > exposureClassPreviewCount {
                        Pill(text: "+\(b.data_classes.count - exposureClassPreviewCount)",
                             tone: .neutral, maxWidth: 60)
                    }
                }
            }
        }
        .padding(Space.md)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: Radius.md))
        .contentShape(RoundedRectangle(cornerRadius: Radius.md))
    }

    /// The list is capped by `limit`; when the aggregate knows about more
    /// breaches than we listed, say so rather than implying the list is whole.
    private var truncatedNote: String? {
        guard let total = exposure?.breach_count, total > breaches.count else { return nil }
        return "Showing \(breaches.count) of \(total) breaches."
    }

    @ViewBuilder
    private var truncationRow: some View {
        if let note = truncatedNote {
            Text(note)
                .font(.caption2.monospacedDigit())
                .foregroundStyle(theme.warning)
        }
    }

    // MARK: - Empty / error

    private var emptyState: some View {
        ContentUnavailableView {
            Label {
                Text("No known exposure").foregroundStyle(theme.text)
            } icon: {
                Image(systemName: "lock.shield").foregroundStyle(theme.textMuted)
            }
        } description: {
            Text("No breach in the local corpus references this entity. That is an absence of evidence, not proof that it was never exposed.")
                .foregroundStyle(theme.textMuted)
        }
    }

    private func errorState(_ message: String) -> some View {
        VStack(alignment: .leading, spacing: Space.sm) {
            HStack(alignment: .top, spacing: Space.sm) {
                Image(systemName: "exclamationmark.triangle.fill")
                    .font(.caption)
                    .foregroundStyle(theme.danger)
                Text(message)
                    .font(.caption)
                    .foregroundStyle(theme.text)
                    .fixedSize(horizontal: false, vertical: true)
            }
            Button {
                Task { await reload() }
            } label: {
                Label("Retry", systemImage: "arrow.clockwise")
            }
            .buttonStyle(.bordered)
            .controlSize(.small)
            .disabled(loading)
        }
        .padding(Space.md)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(theme.danger.opacity(0.12),
                    in: RoundedRectangle(cornerRadius: Radius.md))
    }

    // MARK: - Load

    private func reload() async {
        loading = true
        defer { loading = false }
        do {
            let env = try await apiClient.api.entityBreaches(type: entityType,
                                                             id: entityId,
                                                             limit: limit)
            breaches = env.data
            exposure = env.exposure
            error = nil
        } catch {
            self.error = error.localizedDescription
            Haptics.error()
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// MARK: - File-private helpers
// ─────────────────────────────────────────────────────────────────────────────

/// How many data-class chips a single row shows before collapsing to "+N".
private let exposureClassPreviewCount = 4

private func exposureVisibleClasses(_ all: [String]) -> [String] {
    Array(all.prefix(exposureClassPreviewCount))
}

/// Year from an ISO-ish date. Breach dates arrive as "2019-05-12" or a full
/// timestamp; both start with the year, so the prefix is enough and cannot
/// mis-parse a timezone.
private func exposureYear(_ s: String?) -> Int? {
    guard let s, s.count >= 4 else { return nil }
    return Int(s.prefix(4))
}

/// Credential-bearing classes are the ones that change what an analyst does
/// next, so they read hot. Everything else stays neutral — colouring every
/// chip would make none of them mean anything.
private func exposureClassTone(_ raw: String) -> Pill.Tone {
    let c = raw.lowercased()
    if c.contains("password") || c.contains("credential")
        || c.contains("hash") || c.contains("security question") {
        return .danger
    }
    if c.contains("email") || c.contains("phone") || c.contains("address")
        || c.contains("name") || c.contains("date of birth") || c.contains("government") {
        return .warning
    }
    return .neutral
}
