import SwiftUI

// ─────────────────────────────────────────────────────────────────────────────
// Rule backtest / preview (roadmap 10).
//
// A self-contained `Section` meant to be dropped straight into `AlertEditor`'s
// `Form`, directly under the "Match when…" section:
//
//     RulePreviewSection(predicate: currentPredicateDictionary)
//
// Rule authoring is otherwise blind — you write an FTS expression, save, and
// find out days later whether it matches nothing or everything. This runs the
// SAME matcher the live ingest path uses (POST /api/alerts/preview) over the
// last 7 days and reports what would have fired. It never writes alert_events.
//
// Debounced ~600 ms on the predicate's canonical JSON, via `.task(id:)`: when
// the id changes SwiftUI cancels the in-flight task and starts a new one, so
// the sleep at the top of `runPreview()` IS the debounce — a keystroke that
// arrives before the sleep finishes cancels the request that was about to be
// made, and no request is ever left racing a newer one.
//
// Two server answers get first-class treatment because both are easy to
// misread as "no matches":
//   • `meta.truncated`  — the scan hit its row cap, so the real number is
//                         higher. Rendered as "at least N", never as N.
//   • `meta.skipped == "llm_mode_unsupported"` — an LLM-mode rule is not
//                         evaluable inline. It cannot be previewed AND it does
//                         not fire; saying "0 matches" there would be a lie.
// ─────────────────────────────────────────────────────────────────────────────

struct RulePreviewSection: View {
    /// The predicate exactly as it would be POSTed with the rule. Pass a plain
    /// dictionary (the same shape `API.alertPreview(predicate:)` takes) so the
    /// editor can preview an unsaved draft.
    let predicate: [String: Any]
    /// Optional window start (ISO-8601). `nil` → the server's default 7 days,
    /// which it also clamps to a maximum window.
    var since: String? = nil
    /// Debounce before the request fires. Exposed for tests / tuning.
    var debounceMilliseconds: Int = 600

    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme

    @State private var result: AlertPreviewResult?
    @State private var running = false
    @State private var error: String?
    @State private var expanded = false

    var body: some View {
        Section {
            headline
            if let m = result?.matches, !m.isEmpty {
                DisclosureGroup(isExpanded: $expanded) {
                    ForEach(m) { match in
                        sampleRow(match)
                    }
                } label: {
                    Label("Sample matches (\(m.count))", systemImage: "list.bullet")
                        .font(.caption)
                        .foregroundStyle(theme.accent)
                }
            }
            if let error {
                HStack(alignment: .top, spacing: Space.sm) {
                    Image(systemName: "exclamationmark.triangle.fill")
                        .foregroundStyle(theme.danger)
                    VStack(alignment: .leading, spacing: 2) {
                        Text("Preview failed")
                            .font(.caption.weight(.semibold))
                            .foregroundStyle(theme.danger)
                        Text(error)
                            .font(.caption2)
                            .foregroundStyle(theme.textMuted)
                            .fixedSize(horizontal: false, vertical: true)
                    }
                    Spacer(minLength: 0)
                    Button("Retry") { Task { await runPreview(debounced: false) } }
                        .font(.caption.weight(.semibold))
                        .buttonStyle(.plain)
                        .foregroundStyle(theme.accent)
                }
            }
        } header: {
            HStack(spacing: Space.sm) {
                Text("Backtest")
                Spacer(minLength: 0)
                if running {
                    ProgressView().controlSize(.mini)
                }
            }
        } footer: {
            Text("Runs the live matcher over recent history. Nothing is delivered and no firing history is written — this is a dry run.")
                .font(.caption2)
        }
        // The debounce lives inside `runPreview`; changing the predicate
        // cancels the pending sleep along with the task.
        .task(id: signature) {
            await runPreview(debounced: true)
        }
    }

    // MARK: - Headline

    @ViewBuilder
    private var headline: some View {
        if let skipped = result?.meta?.skipped, !skipped.isEmpty {
            skippedNotice(skipped)
        } else if let n = matchCount {
            VStack(alignment: .leading, spacing: 3) {
                Text(verbatim: countSentence(n))
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(n == 0 ? theme.warning : theme.success)
                    .monospacedDigit()
                    .fixedSize(horizontal: false, vertical: true)
                if let scanned = result?.meta?.scanned {
                    Text("Scanned \(scanned) item\(scanned == 1 ? "" : "s")\(truncated ? " — the scan cap was reached, so the true total is higher." : ".")")
                        .font(.caption2)
                        .foregroundStyle(theme.textMuted)
                        .monospacedDigit()
                        .fixedSize(horizontal: false, vertical: true)
                }
                if n == 0 {
                    Text("Nothing in the window matched. Loosen the query, or check the source IDs and tags are spelled the way the collector emits them.")
                        .font(.caption2)
                        .foregroundStyle(theme.textMuted)
                        .fixedSize(horizontal: false, vertical: true)
                }
            }
        } else if running {
            Text("Backtesting…")
                .font(.subheadline)
                .foregroundStyle(theme.textMuted)
        } else if result != nil {
            // The request succeeded but carried no count. Saying "0" here would
            // be inventing an answer the server did not give.
            Text("The server answered, but did not report a match count.")
                .font(.caption)
                .foregroundStyle(theme.warning)
                .fixedSize(horizontal: false, vertical: true)
        } else if error == nil {
            Text("Edit the query above to see what it would have matched.")
                .font(.caption)
                .foregroundStyle(theme.textMuted)
        }
    }

    /// The server refused to evaluate this predicate. The only value it emits
    /// today is `llm_mode_unsupported`; anything else is surfaced verbatim
    /// rather than silently treated as a zero.
    @ViewBuilder
    private func skippedNotice(_ skipped: String) -> some View {
        HStack(alignment: .top, spacing: Space.sm) {
            Image(systemName: "sparkles")
                .foregroundStyle(theme.warning)
            VStack(alignment: .leading, spacing: 3) {
                if skipped == "llm_mode_unsupported" {
                    Text("LLM-mode rules can't be previewed — and they don't fire.")
                        .font(.subheadline.weight(.semibold))
                        .foregroundStyle(theme.warning)
                        .fixedSize(horizontal: false, vertical: true)
                    Text("A natural-language query is not something the ingest matcher can evaluate, so this rule will never match an incoming item on its own. Use “Suggest FTS query” above to turn it into an FTS expression — that expression is what actually fires, and it is what this backtest measures.")
                        .font(.caption2)
                        .foregroundStyle(theme.textMuted)
                        .fixedSize(horizontal: false, vertical: true)
                } else {
                    Text("Preview skipped: \(skipped)")
                        .font(.subheadline.weight(.semibold))
                        .foregroundStyle(theme.warning)
                        .fixedSize(horizontal: false, vertical: true)
                    Text("The server declined to evaluate this predicate, so no match count is available. This is not the same as “no matches”.")
                        .font(.caption2)
                        .foregroundStyle(theme.textMuted)
                        .fixedSize(horizontal: false, vertical: true)
                }
            }
            Spacer(minLength: 0)
        }
    }

    private func sampleRow(_ match: AlertPreviewMatch) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(match.title ?? match.uid)
                .font(.caption.weight(.medium))
                .foregroundStyle(theme.text)
                .lineLimit(2)
            HStack(spacing: Space.xs) {
                if let src = match.source_id, !src.isEmpty {
                    Text(src)
                        .font(.caption2.monospaced())
                        .foregroundStyle(theme.textMuted)
                        .lineLimit(1)
                }
                Text(relativeTime(match.published_at))
                    .font(.caption2.monospacedDigit())
                    .foregroundStyle(theme.textMuted)
                Spacer(minLength: 0)
            }
        }
        .padding(.vertical, 1)
    }

    // MARK: - Wording

    private var truncated: Bool { result?.meta?.truncated ?? false }

    private var matchCount: Int? {
        if let c = result?.count { return c }
        // Falls back to the sample array only when the server sent one — a
        // sample list is a lower bound, never the total, so `truncated` still
        // governs how the number is phrased.
        if let m = result?.matches { return m.count }
        return nil
    }

    private func countSentence(_ n: Int) -> String {
        let noun = n == 1 ? "item" : "items"
        let qty  = truncated ? "at least \(n) \(noun)" : "\(n) \(noun)"
        return "Would have matched \(qty) \(windowPhrase)"
    }

    private var windowPhrase: String {
        if since == nil { return "in the last 7 days" }
        if let s = result?.meta?.since, !s.isEmpty {
            return "since \(relativeTime(s))"
        }
        return "in the preview window"
    }

    // MARK: - Request

    /// Canonical JSON for the predicate. `.sortedKeys` makes the string stable
    /// across dictionary orderings, so re-rendering with an equivalent
    /// dictionary does not re-fire the request.
    private var signature: String {
        guard JSONSerialization.isValidJSONObject(predicate),
              let data = try? JSONSerialization.data(withJSONObject: predicate,
                                                     options: [.sortedKeys]),
              let s = String(data: data, encoding: .utf8) else {
            return "__invalid__"
        }
        return s + "|" + (since ?? "")
    }

    private func runPreview(debounced: Bool) async {
        if debounced {
            try? await Task.sleep(for: .milliseconds(debounceMilliseconds))
            if Task.isCancelled { return }
        }
        guard JSONSerialization.isValidJSONObject(predicate) else {
            error = "This predicate can't be encoded as JSON."
            result = nil
            return
        }
        running = true
        defer { running = false }
        do {
            let r = try await apiClient.api.alertPreview(predicate: predicate,
                                                         since: since)
            if Task.isCancelled { return }
            result = r
            error = nil
        } catch let e {
            if Task.isCancelled { return }
            result = nil
            error = e.localizedDescription
        }
    }
}
