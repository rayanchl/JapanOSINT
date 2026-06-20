import SwiftUI

/// Unified filter affordance shared by every list tab that pairs a search
/// field with structured filters (Saved, Cameras, Intel, Sources, Layers,
/// API Keys, per-source items). Keeping the trigger + sheet + chip rows in
/// one place means the icon, the "active" fill state, the "Filters" sheet
/// chrome, and the "Reset filters" footer are pixel-identical everywhere —
/// the search field stays the tab's own, only the filter UI is unified.

/// Standard toolbar trigger. Filled icon whenever any filter is active so
/// the user can tell at a glance the list is narrowed.
struct FilterToolbarButton: View {
    let isActive: Bool
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Image(systemName: isActive
                  ? "line.3.horizontal.decrease.circle.fill"
                  : "line.3.horizontal.decrease.circle")
        }
        .accessibilityLabel(isActive ? "Filters (active)" : "Filters")
    }
}

/// Reusable filter-sheet scaffold: a `Form` in a `NavigationStack` with the
/// shared "Filters" title, a Done button, medium/large detents, and an
/// auto-appended destructive "Reset filters" section shown whenever
/// `isActive` is true. Callers supply only the filter-specific sections.
struct FilterSheet<Content: View>: View {
    @Environment(\.theme) private var theme
    let isActive: Bool
    let onReset: () -> Void
    let onDone: () -> Void
    @ViewBuilder var content: Content

    var body: some View {
        NavigationStack {
            Form {
                content
                if isActive {
                    Section {
                        Button("Reset filters", role: .destructive, action: onReset)
                    }
                }
            }
            .compatGroupedForm()
            .themedScreenBackground(theme)
            .navigationTitle("Filters")
            .compatInlineTitle()
            .toolbar {
                ToolbarItem(placement: .compatPrimary) {
                    Button("Done", action: onDone)
                }
            }
        }
        .compatSheetSizing(.medium)
    }
}

/// A multi-select chip list section: tappable rows with an optional leading
/// symbol, an optional trailing count, and a checkmark when selected. Matches
/// the layer / channel / role rows the older hand-rolled sheets used.
struct FilterMultiSelectSection<Item: Hashable>: View {
    let title: String
    let items: [Item]
    var emptyText: String = "Nothing to filter yet"
    let label: (Item) -> String
    var symbol: ((Item) -> (name: String, color: Color))? = nil
    var count: ((Item) -> Int)? = nil
    let isSelected: (Item) -> Bool
    let toggle: (Item) -> Void

    @Environment(\.theme) private var theme

    var body: some View {
        Section(title) {
            if items.isEmpty {
                Text(emptyText).foregroundStyle(theme.textMuted)
            } else {
                ForEach(items, id: \.self) { item in
                    Button { toggle(item) } label: {
                        HStack(spacing: 8) {
                            if let s = symbol?(item) {
                                Image(systemName: s.name)
                                    .foregroundStyle(s.color)
                                    .frame(width: 22)
                            }
                            Text(label(item))
                                .foregroundStyle(theme.text)
                            Spacer()
                            if let c = count?(item) {
                                Text("\(c)")
                                    .font(.caption2.monospaced())
                                    .foregroundStyle(theme.textMuted)
                            }
                            if isSelected(item) {
                                Image(systemName: "checkmark")
                                    .foregroundStyle(theme.accent)
                            }
                        }
                    }
                }
            }
        }
    }
}

/// A single-choice segmented section — the "All / Has X / No X" style toggle
/// every existing sheet hand-rolled (image presence, feed availability, key
/// status). `Choice` must be `CaseIterable & Identifiable` with a `label`.
protocol FilterChoice: CaseIterable, Identifiable, Hashable {
    var label: String { get }
}

struct FilterSegmentedSection<Choice: FilterChoice>: View where Choice.AllCases: RandomAccessCollection {
    let title: String
    @Binding var selection: Choice

    var body: some View {
        Section(title) {
            Picker(title, selection: $selection) {
                ForEach(Array(Choice.allCases)) { c in
                    Text(c.label).tag(c)
                }
            }
            .pickerStyle(.segmented)
        }
    }
}
