import SwiftUI

// ─────────────────────────────────────────────────────────────────────────────
// Roadmap 25 — near-duplicate clusters, framed as CORROBORATION.
//
// When the intel list is fetched with `?collapse=1`, near-identical reports
// fold into one row and the survivor carries a `ClusterInfo`. The instinct is
// to treat the folded members as clutter that was tidied away. They are the
// opposite: "twenty independent sources reported this" is frequently the most
// valuable fact on the row, and this badge exists to say it out loud and let
// the analyst open the list.
//
// Server contract (core/simhash.h), which this view mirrors exactly:
//   • `cluster_size`         — TOTAL members corpus-wide, including this item
//   • `cluster_source_count` — distinct source_ids across the whole cluster
//   • `duplicates`           — the OTHER members, capped at 50
//   • `duplicates_truncated` — true when that cap cut the list
//
// So the expandable list shows "showing 50 of N other reports" whenever the
// cap bit is set: a partial list that pretended to be whole would understate
// corroboration, which is the one error this feature must not make.
//
// Self-contained: takes a `ClusterInfo`, navigates nowhere, and reports taps
// through `onOpenDuplicate`.
// ─────────────────────────────────────────────────────────────────────────────

struct ClusterBadge: View {
    let cluster: ClusterInfo
    /// Called when a duplicate is tapped. The host owns the destination
    /// (typically `IntelDetail(uid:fallbackTitle:)`). Without it the list is
    /// informational and rows are not tappable.
    var onOpenDuplicate: ((ClusterDuplicate) -> Void)? = nil

    @Environment(\.theme) private var theme
    @State private var expanded = false

    var body: some View {
        if hasCluster {
            VStack(alignment: .leading, spacing: Space.sm) {
                summaryButton
                if expanded {
                    duplicateList
                }
            }
        }
    }

    // MARK: - Derived facts

    private var duplicates: [ClusterDuplicate] { cluster.duplicates ?? [] }

    /// Nothing to say when the item was never clustered. A row with no cluster
    /// id and no duplicates simply renders nothing rather than a "1 source"
    /// chip on every uncollapsed list in the app.
    private var hasCluster: Bool {
        (cluster.cluster_id?.isEmpty == false) || !duplicates.isEmpty
    }

    /// Total members including this one. Falls back to the enumerated list + 1
    /// (`duplicates` excludes the item itself) when the server omitted the
    /// count, so the number is never smaller than what we can already see.
    private var totalReports: Int {
        max(cluster.cluster_size ?? 0, duplicates.count + 1)
    }

    /// Distinct sources across the cluster. This — not the member count — is
    /// what makes a story corroborated: ten copies from one aggregator are one
    /// source, and the server already computes the distinction.
    private var sourceCount: Int {
        if let n = cluster.cluster_source_count, n > 0 { return n }
        let distinct = Set(duplicates.map(\.source_id))
        return max(distinct.count, 1)
    }

    private var otherReports: Int { max(totalReports - 1, 0) }

    private var isCorroborated: Bool { sourceCount > 1 }

    private var summaryText: String {
        if isCorroborated {
            return "Reported by \(sourceCount) sources"
        }
        return "Single source"
    }

    // MARK: - Summary

    private var summaryButton: some View {
        Button {
            Haptics.selection()
            withAnimation(.easeInOut(duration: 0.15)) { expanded.toggle() }
        } label: {
            HStack(spacing: Space.sm) {
                Image(systemName: isCorroborated
                      ? "person.2.badge.key"
                      : "person.crop.circle.badge.questionmark")
                    .font(.caption)
                    .foregroundStyle(isCorroborated ? theme.success : theme.textMuted)
                VStack(alignment: .leading, spacing: Space.xs) {
                    Text(summaryText)
                        .font(.caption.weight(.semibold).monospacedDigit())
                        .foregroundStyle(isCorroborated ? theme.text : theme.textMuted)
                    Text(subtitleText)
                        .font(.caption2.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                        .lineLimit(2)
                        .multilineTextAlignment(.leading)
                }
                Spacer(minLength: 0)
                if !duplicates.isEmpty {
                    Image(systemName: expanded ? "chevron.up" : "chevron.down")
                        .font(.caption2)
                        .foregroundStyle(theme.textMuted)
                }
            }
            .padding(Space.sm)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(
                (isCorroborated ? theme.success : theme.textMuted).opacity(0.12),
                in: RoundedRectangle(cornerRadius: Radius.md)
            )
            .contentShape(RoundedRectangle(cornerRadius: Radius.md))
        }
        .buttonStyle(.plain)
        .disabled(duplicates.isEmpty)
        .accessibilityLabel(accessibilityText)
    }

    private var subtitleText: String {
        if !isCorroborated {
            return "No other source in the corpus reported this."
        }
        let reportWord = otherReports == 1 ? "other report" : "other reports"
        if cluster.duplicates_truncated == true {
            return "\(totalReports) near-identical reports · showing \(duplicates.count) of \(otherReports) \(reportWord)"
        }
        return "\(totalReports) near-identical reports · \(otherReports) \(reportWord) folded in"
    }

    private var accessibilityText: String {
        isCorroborated
            ? "Corroborated: reported by \(sourceCount) sources, \(totalReports) near-identical reports."
            : "Single source: no other source reported this."
    }

    // MARK: - Duplicates

    private var duplicateList: some View {
        VStack(alignment: .leading, spacing: Space.xs) {
            if cluster.duplicates_truncated == true {
                Text("Showing \(duplicates.count) of \(otherReports) other reports — the enumeration is capped, the count above is not.")
                    .font(.caption2.monospacedDigit())
                    .foregroundStyle(theme.warning)
                    .fixedSize(horizontal: false, vertical: true)
            }
            ForEach(duplicates) { d in
                duplicateRow(d)
            }
            if let id = cluster.cluster_id, !id.isEmpty {
                Text("cluster \(id)")
                    .font(.caption2.monospaced())
                    .foregroundStyle(theme.textMuted)
                    .lineLimit(1)
                    .truncationMode(.middle)
                    .padding(.top, Space.xs)
            }
        }
        .padding(.leading, Space.sm)
    }

    @ViewBuilder
    private func duplicateRow(_ d: ClusterDuplicate) -> some View {
        let content = HStack(spacing: Space.sm) {
            Image(systemName: "doc.on.doc")
                .font(.caption2)
                .foregroundStyle(theme.textMuted)
            Text(d.source_id ?? "unknown source")
                .font(.caption2.monospaced())
                .foregroundStyle(theme.text)
                .lineLimit(1)
            Text(d.uid ?? "")
                .font(.caption2.monospaced())
                .foregroundStyle(theme.textMuted)
                .lineLimit(1)
                .truncationMode(.middle)
            Spacer(minLength: 0)
            if onOpenDuplicate != nil {
                Image(systemName: "chevron.right")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
            }
        }
        .padding(.vertical, Space.xs)
        .padding(.horizontal, Space.sm)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(theme.surfaceElevated,
                    in: RoundedRectangle(cornerRadius: Radius.sm))
        .contentShape(RoundedRectangle(cornerRadius: Radius.sm))

        if let open = onOpenDuplicate {
            Button {
                Haptics.selection()
                open(d)
            } label: {
                content
            }
            .buttonStyle(.plain)
            .compatHoverHighlight(cornerRadius: Radius.sm)
        } else {
            content
        }
    }
}
