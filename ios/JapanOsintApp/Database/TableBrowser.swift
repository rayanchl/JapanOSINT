import SwiftUI

struct TableBrowser: View {
    @EnvironmentObject var settings: AppSettings
    @Environment(\.theme) private var theme

    @State private var tables: [DBTable] = []
    @State private var selected: String?
    @State private var page: DBPage?
    @State private var search: String = ""
    @State private var orderBy: String?
    @State private var orderAsc = true
    @State private var offset = 0
    @State private var loading = false
    @State private var errorMessage: String?
    @State private var expandedRow: Int?

    private var pageSize: Int { max(1, settings.dbTablePageSize) }

    var body: some View {
        VStack(spacing: 0) {
            tableSelector
            Divider().opacity(0.5)
            content
        }
        .searchable(text: $search,
                    placement: .navigationBarDrawer(displayMode: .always),
                    prompt: "Search rows…")
        .onSubmit(of: .search) { offset = 0; Task { await loadRows() } }
        .onChange(of: search) { _, newValue in
            if newValue.isEmpty {
                offset = 0
                Task { await loadRows() }
            }
        }
        .task { if tables.isEmpty { await loadTables() } }
        .refreshable { await loadRows() }
    }

    // MARK: - Table selector

    private var tableSelector: some View {
        HStack(spacing: 8) {
            Image(systemName: "tablecells")
                .foregroundStyle(theme.accent)
            Menu {
                Picker("Table", selection: Binding(
                    get: { selected ?? tables.first?.name ?? "" },
                    set: { newValue in
                        selected = newValue
                        offset = 0
                        orderBy = nil
                        expandedRow = nil
                        Task { await loadRows() }
                    })
                ) {
                    ForEach(tables) {
                        Text("\($0.name) (\($0.row_count))")
                            .monospacedDigit()
                            .tag($0.name)
                    }
                }
            } label: {
                HStack(spacing: 4) {
                    Text(selected ?? "Pick a table")
                        .font(.subheadline.bold())
                        .foregroundStyle(theme.text)
                    Image(systemName: "chevron.up.chevron.down")
                        .font(.caption2)
                        .foregroundStyle(theme.textMuted)
                }
            }
            Spacer()
            if let p = page {
                Text("\(p.total) rows")
                    .font(.caption2.monospacedDigit())
                    .foregroundStyle(theme.textMuted)
            }
            if loading { ProgressView().scaleEffect(0.7) }
        }
        .padding(.horizontal)
        .padding(.vertical, 8)
    }

    // MARK: - Content

    @ViewBuilder
    private var content: some View {
        if let err = errorMessage {
            errorBanner(err)
        }
        if let p = page {
            ScrollView {
                LazyVStack(spacing: 6) {
                    columnHeader(columns: p.columns)
                    ForEach(Array(p.rows.enumerated()), id: \.offset) { idx, row in
                        rowCard(idx: idx, columns: p.columns, row: row)
                    }
                }
                .padding(.horizontal, 12)
                .padding(.vertical, 8)
                footer(p: p)
                    .padding(.horizontal)
                    .padding(.bottom, 12)
            }
        } else if tables.isEmpty && !loading {
            OfflineStateView(retry: { Task { await loadTables() } })
        } else {
            Spacer()
        }
    }

    // MARK: - Header / cards

    private func columnHeader(columns: [DBColumn]) -> some View {
        HStack(spacing: 6) {
            ForEach(Array(columns.prefix(3).enumerated()), id: \.element.name) { _, c in
                Button {
                    if orderBy == c.name { orderAsc.toggle() }
                    else { orderBy = c.name; orderAsc = true }
                    Task { await loadRows() }
                } label: {
                    HStack(spacing: 3) {
                        Text(c.name).font(.caption2.bold())
                        if orderBy == c.name {
                            Image(systemName: orderAsc ? "arrow.up" : "arrow.down")
                                .font(.caption2)
                        }
                    }
                    .foregroundStyle(orderBy == c.name ? theme.accent : theme.textMuted)
                }
                .buttonStyle(.plain)
            }
            Spacer()
        }
        .padding(.horizontal, 4)
    }

    private func rowCard(idx: Int, columns: [DBColumn], row: [String: AnyCodable]) -> some View {
        let isOpen = expandedRow == idx
        let primary = primaryValue(columns: columns, row: row)
        let secondaries = secondaryValues(columns: columns, row: row)

        return VStack(alignment: .leading, spacing: 8) {
            Button {
                withAnimation(.easeInOut(duration: 0.18)) {
                    expandedRow = isOpen ? nil : idx
                }
            } label: {
                HStack(alignment: .center, spacing: 10) {
                    Text("\(offset + idx + 1)")
                        .font(.caption2.monospacedDigit().bold())
                        .foregroundStyle(theme.textMuted)
                        .frame(width: 28, alignment: .leading)
                    VStack(alignment: .leading, spacing: 4) {
                        Text(primary)
                            .font(.subheadline.weight(.medium))
                            .foregroundStyle(theme.text)
                            .lineLimit(1)
                        if !secondaries.isEmpty {
                            VStack(alignment: .leading, spacing: 4) {
                                ForEach(secondaries.prefix(3), id: \.0) { (k, v) in
                                    chip(label: k, value: v)
                                }
                            }
                        }
                    }
                    Spacer()
                    Image(systemName: isOpen ? "chevron.up" : "chevron.down")
                        .font(.caption2)
                        .foregroundStyle(theme.textMuted)
                }
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)

            if isOpen {
                detailGrid(row: row)
            }
        }
        .padding(12)
        .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: 12))
    }

    private func chip(label: String, value: String) -> some View {
        HStack(spacing: 3) {
            Text(label).font(.caption2).foregroundStyle(theme.textMuted)
            Text(value).font(.caption2.monospacedDigit()).foregroundStyle(theme.text)
                .lineLimit(1)
        }
        .padding(.horizontal, 6).padding(.vertical, 2)
        .background(theme.surface, in: Capsule())
    }

    private func detailGrid(row: [String: AnyCodable]) -> some View {
        LazyVGrid(columns: [GridItem(.flexible(), alignment: .topLeading)],
                  alignment: .leading, spacing: 0) {
            ForEach(row.keys.sorted(), id: \.self) { k in
                HStack(alignment: .top, spacing: 8) {
                    Text(k)
                        .font(.caption.bold())
                        .foregroundStyle(theme.textMuted)
                        .frame(width: 110, alignment: .leading)
                    Text(stringify(row[k]?.value))
                        .font(.caption)
                        .foregroundStyle(theme.text)
                        .textSelection(.enabled)
                        .frame(maxWidth: .infinity, alignment: .leading)
                }
                .padding(.vertical, 4)
            }
        }
    }

    private func primaryValue(columns: [DBColumn], row: [String: AnyCodable]) -> String {
        for k in ["name", "title", "id", "label"] {
            if let v = row[k]?.value, let s = stringifyOptional(v) { return s }
        }
        if let first = columns.first, let v = row[first.name]?.value,
           let s = stringifyOptional(v) { return s }
        return "—"
    }

    private func secondaryValues(columns: [DBColumn], row: [String: AnyCodable]) -> [(String, String)] {
        let skip: Set<String> = ["name", "title", "id", "label"]
        var out: [(String, String)] = []
        for c in columns where !skip.contains(c.name) {
            if let s = stringifyOptional(row[c.name]?.value) {
                out.append((c.name, s))
                if out.count >= 4 { break }
            }
        }
        return out
    }

    private func errorBanner(_ msg: String) -> some View {
        HStack(spacing: 8) {
            Image(systemName: "exclamationmark.triangle.fill")
                .foregroundStyle(theme.danger)
            Text(msg).font(.caption).foregroundStyle(theme.text).lineLimit(3)
            Spacer()
        }
        .padding(10)
        .background(theme.danger.opacity(0.12), in: RoundedRectangle(cornerRadius: 10))
        .padding(.horizontal)
        .padding(.top, 8)
    }

    private func footer(p: DBPage) -> some View {
        HStack(spacing: 12) {
            Button {
                offset = max(0, offset - pageSize)
                expandedRow = nil
                Task { await loadRows() }
            } label: {
                Label("Prev", systemImage: "chevron.left")
                    .labelStyle(.titleAndIcon)
            }
            .buttonStyle(.bordered)
            .controlSize(.small)
            .disabled(offset == 0)

            Spacer()
            Text("\(p.offset + 1)–\(min(p.offset + p.rows.count, p.total)) of \(p.total)")
                .font(.caption2.monospacedDigit())
                .foregroundStyle(theme.textMuted)
            Spacer()

            Button {
                offset += pageSize
                expandedRow = nil
                Task { await loadRows() }
            } label: {
                Label("Next", systemImage: "chevron.right")
                    .labelStyle(.titleAndIcon)
            }
            .buttonStyle(.bordered)
            .controlSize(.small)
            .disabled(offset + pageSize >= p.total)
        }
    }

    // MARK: - Helpers

    private func stringify(_ v: Any?) -> String {
        stringifyOptional(v) ?? "—"
    }

    private func stringifyOptional(_ v: Any?) -> String? {
        switch v {
        case nil: return nil
        case let s as String: return s.isEmpty ? nil : s
        case let n as Int: return String(n)
        case let n as Double: return String(n)
        case let b as Bool: return b ? "true" : "false"
        default: return String(describing: v!)
        }
    }

    // MARK: - Loading

    private func loadTables() async {
        let api = API(baseURL: settings.backendBaseURL)
        loading = true
        defer { loading = false }
        do {
            tables = try await api.dbTables()
            if selected == nil { selected = tables.first?.name }
            if selected != nil { await loadRows() }
        } catch {
            errorMessage = error.localizedDescription
        }
    }

    private func loadRows() async {
        guard let table = selected else { return }
        let api = API(baseURL: settings.backendBaseURL)
        loading = true
        defer { loading = false }
        do {
            page = try await api.dbRows(
                table: table,
                query: search,
                orderBy: orderBy,
                orderDir: orderAsc ? "ASC" : "DESC",
                limit: pageSize,
                offset: offset
            )
            errorMessage = nil
        } catch {
            errorMessage = error.localizedDescription
        }
    }
}
