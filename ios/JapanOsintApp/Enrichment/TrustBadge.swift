import SwiftUI

// ─────────────────────────────────────────────────────────────────────────────
// Roadmap 30 — source reliability, rendered as a chip you can interrogate.
//
// The whole point of this badge is that it EXPLAINS its number instead of
// asserting it, so the compact chip is a tappable affordance onto a breakdown
// of the four weighted components the server folds together
// (core/source_trust.h):
//
//     reliability = 0.40 · success_rate_30d
//                 + 0.30 · freshness
//                 + 0.20 · (1 − anomaly_rate_30d)
//                 + 0.10 · volume_stability
//
// Two states are load-bearing and are NOT grades:
//
//   • `rated == false` — the source has no fetch history in the scoring
//     window. "We have no evidence" and "we have evidence it is bad" are
//     different claims; conflating them by drawing an F would make every
//     other badge in the app untrustworthy. It renders as a neutral "—".
//
//   • `quarantined == true` — an explicit operator/maintenance verdict, a
//     hard zero that must not be averaged away by a good 30-day record. It
//     renders as its own chip and the breakdown says why.
//
// Self-contained: takes a `SourceTrust` value, keeps no shared state, and
// performs no navigation. Drop it anywhere a source is named.
// ─────────────────────────────────────────────────────────────────────────────

struct TrustBadge: View {

    /// `.compact` is the list-row form: a chip that opens the breakdown in a
    /// popover. `.expanded` renders the chip with the breakdown card inline —
    /// for a source detail pane where there is room to just show the maths.
    enum Presentation { case compact, expanded }

    let trust: SourceTrust
    /// Named in the breakdown header so a popover detached from its row still
    /// says what it is about.
    var sourceName: String? = nil
    var presentation: Presentation = .compact

    @Environment(\.theme) private var theme
    @State private var detailShown = false

    var body: some View {
        switch presentation {
        case .compact:
            Button {
                Haptics.selection()
                detailShown = true
            } label: {
                chip
            }
            .buttonStyle(.plain)
            .accessibilityLabel(accessibilityText)
            .accessibilityHint("Shows how this score is calculated")
            .popover(isPresented: $detailShown) {
                TrustBreakdownCard(trust: trust, sourceName: sourceName)
                    .environment(\.theme, theme)
                    .trustPopoverAdaptation()
            }
        case .expanded:
            VStack(alignment: .leading, spacing: Space.sm) {
                chip.accessibilityLabel(accessibilityText)
                TrustBreakdownCard(trust: trust, sourceName: sourceName)
            }
        }
    }

    // MARK: - Chip

    @ViewBuilder
    private var chip: some View {
        switch TrustGrading.kind(of: trust) {
        case .quarantined:
            Pill(text: "QUARANTINED", tone: .danger,
                 icon: "exclamationmark.octagon.fill", maxWidth: 160)
        case .unrated:
            // Neutral, explicitly not a grade. The em dash is the whole point.
            Pill(text: "—", tone: .neutral, icon: "minus.circle", maxWidth: 80)
        case .graded:
            Pill(text: gradeChipText, tone: TrustGrading.tone(for: trust),
                 icon: "checkmark.seal", maxWidth: 150)
        }
    }

    private var gradeChipText: String {
        let letter = TrustGrading.letter(for: trust) ?? "?"
        if let r = trust.reliability {
            return "\(letter) · \(TrustGrading.percent(r))"
        }
        return letter
    }

    private var accessibilityText: String {
        switch TrustGrading.kind(of: trust) {
        case .quarantined:
            return "Source quarantined. Reliability scores zero."
        case .unrated:
            return "Unrated. No fetch history in the scoring window — this is not a low score."
        case .graded:
            let letter = TrustGrading.letter(for: trust) ?? "?"
            if let r = trust.reliability {
                return "Reliability grade \(letter), \(TrustGrading.percent(r))"
            }
            return "Reliability grade \(letter)"
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// MARK: - Breakdown card
// ─────────────────────────────────────────────────────────────────────────────

/// The expanded explanation. Internal (not private) so a host screen can embed
/// it directly — e.g. a source detail pane — without going through the popover.
struct TrustBreakdownCard: View {
    let trust: SourceTrust
    var sourceName: String? = nil

    @Environment(\.theme) private var theme

    var body: some View {
        VStack(alignment: .leading, spacing: Space.md) {
            header

            switch TrustGrading.kind(of: trust) {
            case .quarantined:
                note("Quarantined by an operator or by maintenance. Quarantine is an explicit verdict, so the score is a hard zero regardless of a good 30-day record.",
                     tone: theme.danger, icon: "exclamationmark.octagon.fill")
                if trust.rated {
                    Text("History before quarantine")
                        .font(.caption2.weight(.semibold))
                        .foregroundStyle(theme.textMuted)
                    components
                }
            case .unrated:
                note("Unrated — no fetch runs recorded in the scoring window. This is an absence of evidence, not a low score, and it is deliberately not rendered as a grade.",
                     tone: theme.textMuted, icon: "minus.circle")
            case .graded:
                components
            }

            Divider().overlay(theme.textMuted.opacity(0.3))
            Text("Weights are fixed: 0.40 success · 0.30 freshness · 0.20 anomaly-free · 0.10 volume stability. Window is the last 30 days of fetch history.")
                .font(.caption2)
                .foregroundStyle(theme.textMuted)
                .fixedSize(horizontal: false, vertical: true)
        }
        .padding(Space.lg)
        .frame(minWidth: 300, idealWidth: 330, maxWidth: 380, alignment: .leading)
        .background(theme.surfaceElevated)
    }

    private var header: some View {
        VStack(alignment: .leading, spacing: Space.xs) {
            Text(sourceName ?? "Source reliability")
                .font(.subheadline.weight(.semibold))
                .foregroundStyle(theme.text)
                .lineLimit(2)
            if let r = trust.reliability, trust.rated {
                HStack(spacing: Space.sm) {
                    Text(TrustGrading.percent(r))
                        .font(.title3.weight(.bold))
                        .foregroundStyle(TrustGrading.color(for: trust, theme: theme))
                        .monospacedDigit()
                    if let letter = TrustGrading.letter(for: trust) {
                        Text("grade \(letter)")
                            .font(.caption)
                            .foregroundStyle(theme.textMuted)
                    }
                    Spacer(minLength: 0)
                }
            }
        }
    }

    private var components: some View {
        VStack(alignment: .leading, spacing: Space.sm) {
            metric("Success rate", value: trust.successRate,
                   weight: "×0.40", higherIsBetter: true)
            metric("Freshness", value: trust.freshness,
                   weight: "×0.30", higherIsBetter: true)
            metric("Anomaly rate", value: trust.anomalyRate,
                   weight: "×0.20", higherIsBetter: false)
            metric("Volume stability", value: trust.volumeStability,
                   weight: "×0.10", higherIsBetter: true)

            HStack(spacing: Space.md) {
                counter("Runs (30d)", trust.runs30d)
                counter("Anomalies (30d)", trust.anomalies30d)
                Spacer(minLength: 0)
            }
            .padding(.top, Space.xs)
        }
    }

    /// One weighted component: name, its share of the score, the value, and a
    /// bar. `higherIsBetter == false` inverts the tone so a high anomaly rate
    /// reads red rather than green.
    private func metric(_ label: String, value: Double?,
                        weight: String, higherIsBetter: Bool) -> some View {
        VStack(alignment: .leading, spacing: Space.xs) {
            HStack(spacing: Space.xs) {
                Text(label)
                    .font(.caption)
                    .foregroundStyle(theme.text)
                Text(weight)
                    .font(.caption2.monospacedDigit())
                    .foregroundStyle(theme.textMuted)
                Spacer(minLength: Space.sm)
                Text(value.map { TrustGrading.percent($0) } ?? "—")
                    .font(.caption.monospacedDigit())
                    .foregroundStyle(value == nil ? theme.textMuted : theme.text)
            }
            bar(value, higherIsBetter: higherIsBetter)
            if !higherIsBetter, value != nil {
                Text("lower is better")
                    .font(.system(size: 9))
                    .foregroundStyle(theme.textMuted)
            }
        }
    }

    @ViewBuilder
    private func bar(_ value: Double?, higherIsBetter: Bool) -> some View {
        let width: CGFloat = 240
        let raw = value ?? 0
        let clamped = CGFloat(min(max(raw, 0), 1))
        // Quality of the component, 0…1, with anomaly rate flipped so the
        // colour ramp always means the same thing.
        let quality = higherIsBetter ? raw : (1 - raw)
        let tint: Color = value == nil
            ? theme.textMuted
            : (quality >= 0.75 ? theme.success : (quality >= 0.5 ? theme.warning : theme.danger))
        ZStack(alignment: .leading) {
            Capsule().fill(tint.opacity(0.18))
                .frame(width: width, height: 5)
            if value != nil {
                Capsule().fill(tint)
                    .frame(width: max(2, width * clamped), height: 5)
            }
        }
        .frame(maxWidth: width, alignment: .leading)
    }

    private func counter(_ label: String, _ n: Int?) -> some View {
        VStack(alignment: .leading, spacing: Space.xs) {
            Text(n.map { String($0) } ?? "—")
                .font(.caption.monospacedDigit().weight(.semibold))
                .foregroundStyle(theme.text)
            Text(label)
                .font(.system(size: 9))
                .foregroundStyle(theme.textMuted)
        }
    }

    private func note(_ text: String, tone: Color, icon: String) -> some View {
        HStack(alignment: .top, spacing: Space.sm) {
            Image(systemName: icon)
                .font(.caption)
                .foregroundStyle(tone)
            Text(text)
                .font(.caption)
                .foregroundStyle(theme.text)
                .fixedSize(horizontal: false, vertical: true)
        }
        .padding(Space.sm)
        .background(tone.opacity(0.12), in: RoundedRectangle(cornerRadius: Radius.sm))
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// MARK: - Grading helpers
// ─────────────────────────────────────────────────────────────────────────────

/// Shared classification so the chip, the accessibility label and the card can
/// never disagree about what the same `SourceTrust` means.
enum TrustGrading {
    enum Kind { case unrated, quarantined, graded }

    static func kind(of t: SourceTrust) -> Kind {
        if t.quarantined == true { return .quarantined }
        if !t.rated { return .unrated }
        // Rated but the server sent neither a grade nor a score: still not
        // evidence of badness, so it stays neutral rather than becoming an F.
        return letter(for: t) == nil ? .unrated : .graded
    }

    /// Server-sent grade wins. The derivation is only a fallback for a payload
    /// that carried a score without a letter — it never invents a grade for an
    /// unrated source.
    static func letter(for t: SourceTrust) -> String? {
        if let g = t.grade?.trimmingCharacters(in: .whitespacesAndNewlines), !g.isEmpty {
            return g.uppercased()
        }
        guard let r = t.reliability, r.isFinite else { return nil }
        if r >= 0.90 { return "A" }
        if r >= 0.75 { return "B" }
        if r >= 0.60 { return "C" }
        if r >= 0.40 { return "D" }
        return "F"
    }

    static func tone(for t: SourceTrust) -> Pill.Tone {
        if t.quarantined == true { return .danger }
        guard t.rated, let letter = letter(for: t) else { return .neutral }
        switch letter.prefix(1) {
        case "A", "B": return .success
        case "C":      return .warning
        default:       return .danger
        }
    }

    static func color(for t: SourceTrust, theme: ThemePalette) -> Color {
        switch tone(for: t) {
        case .success: return theme.success
        case .warning: return theme.warning
        case .danger:  return theme.danger
        case .accent:  return theme.accent
        case .info:    return theme.accentAlt
        case .neutral: return theme.textMuted
        }
    }

    static func percent(_ v: Double) -> String {
        guard v.isFinite else { return "—" }
        return String(format: "%.0f%%", min(max(v, 0), 1) * 100)
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// MARK: - Platform shim
// ─────────────────────────────────────────────────────────────────────────────

private extension View {
    /// Keeps the breakdown a popover on iPhone instead of adapting to a
    /// full-height sheet, which would be absurd for a 300pt card. No-op on
    /// macOS, where popovers are popovers.
    @ViewBuilder
    func trustPopoverAdaptation() -> some View {
        #if os(iOS)
        presentationCompactAdaptation(.popover)
        #else
        self
        #endif
    }
}
