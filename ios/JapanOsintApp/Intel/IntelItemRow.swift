import SwiftUI

/// Level-2 row: data-type icon + title + relative time + tag chips.
///
/// The leading icon and tint come from the shared `LayerRegistry` mapping,
/// the same one that powers source rows and map markers — so an OpenPhish
/// row, a Shodan row, and the corresponding map layer all read with the same
/// symbol/colour.
struct IntelItemRow: View {
    let item: IntelItem
    @EnvironmentObject var registry: LayerRegistry
    @Environment(\.theme) private var theme

    var body: some View {
        if item.isCollectorNotice {
            noticeRow
        } else {
            findingRow
        }
    }

    /// A collector's disclosure about itself — a truncated walk or a missing
    /// credential — styled as a notice, not a finding: neutral icon, no
    /// source-coloured badge, and a label saying what kind of notice it is.
    /// See `IntelItem.isCollectorNotice` for why this must never fall through
    /// to the finding layout.
    private var noticeRow: some View {
        HStack(alignment: .top, spacing: Space.md) {
            Image(systemName: item.isStatusNotice ? "key.slash" : "text.badge.minus")
                .font(.callout)
                .foregroundStyle(theme.textMuted)
                .frame(width: 26, height: 26)
                .background(theme.textMuted.opacity(0.10),
                            in: RoundedRectangle(cornerRadius: Radius.sm))
            VStack(alignment: .leading, spacing: 4) {
                HStack(spacing: Space.xs) {
                    Pill(text: item.isStatusNotice ? "needs credential" : "truncated",
                         tone: .info, maxWidth: 140)
                    Text(item.source_id)
                        .font(Typography.monoSmall)
                        .foregroundStyle(theme.textMuted)
                        .lineLimit(1)
                }
                Text(item.title ?? item.uid)
                    .font(.caption)
                    .foregroundStyle(theme.textMuted)
                    .lineLimit(2)
            }
        }
        .padding(.vertical, 2)
    }

    private var findingRow: some View {
        HStack(alignment: .top, spacing: Space.md) {
            iconBadge
            VStack(alignment: .leading, spacing: 4) {
                JapaneseAware(
                    text: item.title ?? item.uid,
                    font: .subheadline.weight(.medium),
                    foregroundStyle: AnyShapeStyle(theme.text)
                )
                HStack(spacing: Space.sm - 2) {
                    Text(relativeTime(item.published_at ?? item.fetched_at))
                        .font(Typography.monoSmall)
                        .foregroundStyle(theme.textMuted)
                    if let tags = item.tags, !tags.isEmpty {
                        ForEach(tags.prefix(3), id: \.self) { t in
                            Pill(text: t, tone: .info, maxWidth: 140)
                        }
                    }
                    Spacer(minLength: 0)
                }
                if let summary = item.summary, !summary.isEmpty {
                    Text(summary)
                        .font(.caption)
                        .foregroundStyle(theme.textMuted)
                        .lineLimit(2)
                }
            }
        }
        .padding(.vertical, 2)
    }

    private var iconBadge: some View {
        let color = registry.color(for: item.source_id)
        return Image(systemName: registry.symbol(for: item.source_id))
            .font(.callout)
            .foregroundStyle(color)
            .frame(width: 26, height: 26)
            .background(color.opacity(0.14), in: RoundedRectangle(cornerRadius: Radius.sm))
    }
}
