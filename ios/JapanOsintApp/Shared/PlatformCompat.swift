import SwiftUI
#if canImport(UIKit)
import UIKit
#elseif canImport(AppKit)
import AppKit
#endif

/// Cross-platform shims so the shared SwiftUI codebase compiles and behaves on
/// both iOS and native macOS. Each helper keeps the iOS behaviour intact and
/// degrades to the closest sensible Mac equivalent (or a no-op where the
/// concept doesn't exist on macOS, e.g. keyboard type / autocapitalization).
///
/// The call sites stay platform-agnostic — they use `.compat*` modifiers
/// instead of UIKit-only APIs like `navigationBarTitleDisplayMode` or
/// `keyboardType`, both of which are unavailable when SDKROOT resolves to
/// macosx.

// MARK: - Toolbar placements

extension ToolbarItemPlacement {
    /// Trailing action area. `.topBarTrailing` is iOS-only; `.primaryAction`
    /// is the cross-platform equivalent and lands in the same visual slot.
    static var compatPrimary: ToolbarItemPlacement {
        #if os(iOS)
        .topBarTrailing
        #else
        .primaryAction
        #endif
    }

    /// Leading action area. Most leading items in this app are Cancel/Close/
    /// Back affordances, for which `.cancellationAction` is the natural Mac
    /// placement (and honours ⎋).
    static var compatLeading: ToolbarItemPlacement {
        #if os(iOS)
        .topBarLeading
        #else
        .cancellationAction
        #endif
    }
}

// MARK: - Search field placement

extension SearchFieldPlacement {
    /// Always-visible search drawer under the nav bar on iOS; `.automatic` on
    /// macOS (the search field lives in the toolbar — `.navigationBarDrawer`
    /// is iOS-only).
    static var compatDrawer: SearchFieldPlacement {
        #if os(iOS)
        .navigationBarDrawer(displayMode: .always)
        #else
        .automatic
        #endif
    }
}

// MARK: - Navigation chrome & text input

extension View {
    /// `navigationBarTitleDisplayMode(.inline)` on iOS; no-op on macOS where
    /// there is no large/inline title distinction.
    @ViewBuilder
    func compatInlineTitle(_ inline: Bool = true) -> some View {
        #if os(iOS)
        navigationBarTitleDisplayMode(inline ? .inline : .automatic)
        #else
        self
        #endif
    }

    /// `navigationBarHidden(_:)` on iOS; `.toolbar(.hidden)` on macOS.
    @ViewBuilder
    func compatNavBarHidden(_ hidden: Bool = true) -> some View {
        #if os(iOS)
        navigationBarHidden(hidden)
        #else
        toolbar(hidden ? .hidden : .automatic, for: .windowToolbar)
        #endif
    }

    /// `.compatNoAutocap()` on iOS; no-op on macOS (hardware
    /// keyboards don't auto-capitalize).
    @ViewBuilder
    func compatNoAutocap() -> some View {
        #if os(iOS)
        textInputAutocapitalization(.never)
        #else
        self
        #endif
    }

    /// URL keyboard on iOS; no-op on macOS.
    @ViewBuilder
    func compatKeyboardURL() -> some View {
        #if os(iOS)
        keyboardType(.URL)
        #else
        self
        #endif
    }

    /// Email keyboard on iOS; no-op on macOS. `email == false` falls back to
    /// the URL keyboard to mirror the iOS call sites that branch on field type.
    @ViewBuilder
    func compatKeyboard(email: Bool) -> some View {
        #if os(iOS)
        keyboardType(email ? .emailAddress : .URL)
        #else
        self
        #endif
    }

    /// Hides the bottom tab bar on iOS; no-op on macOS, which has no tab bar
    /// (the `.tabBar` toolbar placement is iOS-only).
    @ViewBuilder
    func compatHideTabBar() -> some View {
        #if os(iOS)
        toolbar(.hidden, for: .tabBar)
        #else
        self
        #endif
    }

    /// `.insetGrouped` list style on iOS; `.inset` on macOS (the closest Mac
    /// equivalent — `.insetGrouped` is iOS-only).
    @ViewBuilder
    func compatInsetGroupedListStyle() -> some View {
        #if os(iOS)
        listStyle(.insetGrouped)
        #else
        listStyle(.inset)
        #endif
    }

    /// Paged `TabView` on iOS; the default style on macOS, which has no paging
    /// container (the onboarding carousel renders as a plain switchable view).
    @ViewBuilder
    func compatPageTabViewStyle() -> some View {
        #if os(iOS)
        tabViewStyle(.page(indexDisplayMode: .always))
        #else
        self
        #endif
    }

    // MARK: Sheet & content sizing (macOS-native layout)

    /// Sizes a presented sheet's content. iOS uses height detents (drag-to-
    /// resize bottom sheets); macOS has no detents — sheets are fixed centered
    /// panels — so we pin an explicit width/height instead. Apply to the sheet
    /// *content* (same place the iOS `presentationDetents` lived).
    @ViewBuilder
    func compatSheetSizing(_ kind: SheetSize) -> some View {
        #if os(iOS)
        presentationDetents(kind.detents)
        #else
        frame(
            minWidth: kind.size.width, idealWidth: kind.size.width,
            minHeight: kind.size.height, idealHeight: kind.size.height
        )
        #endif
    }

    /// Caps content width and centers it on macOS so forms/detail panes don't
    /// stretch across a wide window (the iOS layout fills the narrow screen, so
    /// this is a no-op there). Leaves vertical layout untouched.
    @ViewBuilder
    func compatReadableWidth(_ maxWidth: CGFloat = 720) -> some View {
        #if os(macOS)
        frame(maxWidth: maxWidth)
            .frame(maxWidth: .infinity)   // center the capped block in the pane
        #else
        self
        #endif
    }

    /// Pointer-hover highlight for custom `.buttonStyle(.plain)` rows that live
    /// outside a `List` (List rows hover for free). No-op on iOS (no pointer).
    @ViewBuilder
    func compatHoverHighlight(cornerRadius: CGFloat = 8) -> some View {
        #if os(macOS)
        modifier(HoverHighlight(cornerRadius: cornerRadius))
        #else
        self
        #endif
    }

    /// `.formStyle(.grouped)` on macOS — renders `Form` as a centered, width-
    /// constrained, inset-grouped panel (the System Settings look) instead of a
    /// full-width stretch. No-op on iOS, where `Form` is already grouped.
    @ViewBuilder
    func compatGroupedForm() -> some View {
        #if os(macOS)
        formStyle(.grouped)
        #else
        self
        #endif
    }

    /// `.controlSize(.large)` on macOS for primary action buttons (gives them a
    /// proper Mac click target); no-op on iOS where buttons are already large.
    @ViewBuilder
    func compatLargeControl() -> some View {
        #if os(macOS)
        controlSize(.large)
        #else
        self
        #endif
    }

    /// Hides every row/section separator on macOS, where `List`/`Form` draw
    /// hairline rules between rows that the iOS grouped look doesn't. No-op on
    /// iOS (the grouped insets already read cleanly there).
    @ViewBuilder
    func compatNoSeparators() -> some View {
        #if os(macOS)
        listRowSeparator(.hidden)
            .listSectionSeparator(.hidden)
        #else
        self
        #endif
    }

    /// Slims a linear `ProgressView` on macOS, where the default bar reads
    /// chunky next to our compact cards. `.mini` is the thinnest AppKit track;
    /// no-op on iOS, where the bar is already a hairline.
    @ViewBuilder
    func compatThinProgress() -> some View {
        #if os(macOS)
        controlSize(.mini)
        #else
        self
        #endif
    }
}

/// Logical sheet sizes mapped to iOS detents and macOS fixed panel dimensions.
enum SheetSize {
    case compact   // short utility sheets (filters, pickers)
    case medium    // standard detail / form sheets
    case large     // tall content (full editors, long detail)
    case form      // settings-style forms

    #if os(iOS)
    var detents: Set<PresentationDetent> {
        switch self {
        case .compact: return [.medium]
        case .medium:  return [.medium, .large]
        case .large:   return [.large]
        case .form:    return [.large]
        }
    }
    #endif

    var size: CGSize {
        switch self {
        case .compact: return CGSize(width: 460, height: 420)
        case .medium:  return CGSize(width: 560, height: 560)
        case .large:   return CGSize(width: 640, height: 720)
        case .form:    return CGSize(width: 600, height: 640)
        }
    }
}

#if os(macOS)
/// Subtle fill that appears under the pointer, matching AppKit's row-hover feel.
private struct HoverHighlight: ViewModifier {
    let cornerRadius: CGFloat
    @State private var hovering = false
    func body(content: Content) -> some View {
        content
            .background(
                RoundedRectangle(cornerRadius: cornerRadius)
                    .fill(Color.primary.opacity(hovering ? 0.08 : 0))
            )
            .onHover { hovering = $0 }
            .animation(.easeOut(duration: 0.12), value: hovering)
    }
}
#endif

// MARK: - Clipboard

/// Platform-neutral clipboard write. Mirrors `UIPasteboard.general.string = …`.
enum Clipboard {
    static func copy(_ string: String) {
        #if canImport(UIKit)
        UIPasteboard.general.string = string
        #elseif canImport(AppKit)
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(string, forType: .string)
        #endif
    }
}

// MARK: - System colors

/// Cross-platform system colors used by the `.system` theme palette. iOS uses
/// the UIColor semantic colors; macOS maps each to its AppKit counterpart so
/// the neutral theme tracks the OS appearance on both.
extension Color {
    static var compatSurface: Color {
        #if canImport(UIKit)
        Color(uiColor: .systemBackground)
        #elseif canImport(AppKit)
        Color(nsColor: .windowBackgroundColor)
        #else
        Color(.sRGB, white: 0, opacity: 0)
        #endif
    }

    static var compatSurfaceElevated: Color {
        #if canImport(UIKit)
        Color(uiColor: .secondarySystemBackground)
        #elseif canImport(AppKit)
        // NOT underPageBackgroundColor — that's the dark "behind a page" color
        // and stays near-black even in Aqua (light) mode, making every elevated
        // card look stuck in dark mode. controlBackgroundColor is the content/
        // card surface that actually tracks the OS appearance (white-ish in
        // light, dark in dark).
        Color(nsColor: .controlBackgroundColor)
        #else
        Color(.sRGB, white: 0.1, opacity: 1)
        #endif
    }

    static var compatLabel: Color {
        #if canImport(UIKit)
        Color(uiColor: .label)
        #elseif canImport(AppKit)
        Color(nsColor: .labelColor)
        #else
        .primary
        #endif
    }

    static var compatSecondaryLabel: Color {
        #if canImport(UIKit)
        Color(uiColor: .secondaryLabel)
        #elseif canImport(AppKit)
        Color(nsColor: .secondaryLabelColor)
        #else
        .secondary
        #endif
    }
}
