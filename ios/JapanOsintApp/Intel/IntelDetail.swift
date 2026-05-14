import SwiftUI

/// Level-3 detail: full body + properties table for a single intel item.
/// Lazy-loads via `/api/intel/items/:uid` so the row stays light.
struct IntelDetail: View {
    let uid: String
    let fallbackTitle: String

    @EnvironmentObject var settings: AppSettings
    @Environment(\.theme) private var theme

    @State private var item: IntelItem?
    @State private var loading = true
    @State private var error: String?

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 14) {
                header
                if loading {
                    ProgressView()
                } else if let error {
                    Text(error).font(.caption).foregroundStyle(theme.warning)
                } else if let item {
                    metaRow(item)
                    if let body = item.body, !body.isEmpty {
                        JapaneseAware(
                            text: body,
                            font: .body,
                            foregroundStyle: AnyShapeStyle(theme.text)
                        )
                    } else if let summary = item.summary, !summary.isEmpty {
                        JapaneseAware(
                            text: summary,
                            font: .body,
                            foregroundStyle: AnyShapeStyle(theme.text)
                        )
                    }
                    if let urlStr = item.link, let url = URL(string: urlStr) {
                        Link(destination: url) {
                            Label("Open source", systemImage: "safari")
                        }
                        .buttonStyle(.borderedProminent)
                    }
                    if let props = item.properties, !props.isEmpty {
                        Text("Properties").font(.headline).foregroundStyle(theme.text)
                        propertiesGrid(props)
                    }
                }
            }
            .padding()
        }
        .background(theme.surface.ignoresSafeArea())
        .navigationTitle(item?.title ?? fallbackTitle)
        .navigationBarTitleDisplayMode(.inline)
        .task { await load() }
    }

    private var header: some View {
        TranslatableHeader(text: item?.title ?? fallbackTitle) { EmptyView() }
    }

    private func metaRow(_ it: IntelItem) -> some View {
        HStack(spacing: 8) {
            Text(relativeTime(it.published_at ?? it.fetched_at))
                .font(.caption.monospacedDigit())
                .foregroundStyle(theme.textMuted)
            Text("·").font(.caption).foregroundStyle(theme.textMuted)
            Text(it.source_id)
                .font(.caption.monospaced())
                .foregroundStyle(theme.textMuted)
            if let author = it.author {
                Text("·").font(.caption).foregroundStyle(theme.textMuted)
                Text(author)
                    .font(.caption)
                    .foregroundStyle(theme.textMuted)
                    .lineLimit(1)
            }
            Spacer(minLength: 0)
        }
    }

    private func propertiesGrid(_ properties: [String: AnyCodable]) -> some View {
        VStack(spacing: 1) {
            ForEach(properties.keys.sorted(), id: \.self) { key in
                HStack(alignment: .top, spacing: 8) {
                    Text(key)
                        .font(.caption.bold())
                        .foregroundStyle(theme.textMuted)
                        .frame(width: 110, alignment: .leading)
                    JapaneseAware(
                        text: stringify(properties[key]?.value),
                        font: .caption,
                        foregroundStyle: AnyShapeStyle(theme.text)
                    )
                }
                .padding(8)
                .background(theme.surfaceElevated)
            }
        }
        .clipShape(RoundedRectangle(cornerRadius: 10))
    }

    private func stringify(_ value: Any?) -> String {
        switch value {
        case nil:                 return "—"
        case let v as String:     return v
        case let v as NSNumber:   return v.stringValue
        case let v as Bool:       return v ? "true" : "false"
        case let v as [Any]:      return v.map { String(describing: $0) }.joined(separator: ", ")
        case let v as [String: Any]: return "{\(v.count) keys}"
        default:                  return String(describing: value!)
        }
    }

    private func load() async {
        loading = true
        defer { loading = false }
        do {
            item = try await API(baseURL: settings.backendBaseURL).intelItem(uid: uid)
            error = nil
        } catch let err {
            error = err.localizedDescription
        }
    }
}
