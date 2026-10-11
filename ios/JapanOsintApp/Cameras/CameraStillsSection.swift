import SwiftUI
#if canImport(UIKit)
import UIKit
#elseif canImport(AppKit)
import AppKit
#endif

/// Captured stills timeline for one camera record (native/core/camera_stills.c),
/// shown inside `CameraPopup`.
///
/// - Collapsed until the user expands it, so opening a camera costs no request.
/// - The list route lists every view of the record newest-first; each frame
///   names its `view` ("views/<preset>") and `scene_changed` is tri-state —
///   nil means nobody could tell, which is NOT "unchanged".
/// - Tapping a frame fetches its raw bytes from the OPERATOR-GATED route. A
///   401/403/410 is shown as the reason (not as a broken image), and bytes are
///   only drawn when they carry a real image signature: they come from an
///   arbitrary third-party camera.
struct CameraStillsSection: View {
    /// An intel uid ("511-alberta-cameras|AB--1") or a bare camera_uid.
    let cameraId: String

    @EnvironmentObject private var apiClient: APIClient
    @Environment(\.theme) private var theme

    @State private var expanded = false
    @State private var stills: [CameraStill] = []
    @State private var meta: CameraStillsMeta?
    @State private var cursor: String?
    @State private var loading = false
    @State private var loadError: String?
    @State private var selected: CameraStill?

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Button {
                expanded.toggle()
            } label: {
                HStack {
                    PopupSectionHeader("Stills timeline", icon: "photo.stack")
                    Image(systemName: expanded ? "chevron.down" : "chevron.right")
                        .font(.caption)
                        .foregroundStyle(theme.textMuted)
                }
            }
            .buttonStyle(.plain)
            .accessibilityLabel(expanded ? "Collapse stills timeline" : "Expand stills timeline")

            if expanded {
                content
            }
        }
        .task(id: expanded) {
            if expanded && meta == nil && !loading { await load(cursor: nil) }
        }
        .onChange(of: cameraId) { _, _ in
            stills = []; meta = nil; cursor = nil; selected = nil; loadError = nil
        }
    }

    @ViewBuilder
    private var content: some View {
        if let loadError {
            Text("Could not load stills — \(loadError)")
                .font(.caption)
                .foregroundStyle(theme.danger)
        }
        if let meta { summary(meta) }
        if let selected {
            CameraStillFrameView(still: selected) { self.selected = nil }
        }
        if !loading && meta != nil && stills.isEmpty {
            Text("No stills captured for this camera.")
                .font(.caption)
                .foregroundStyle(theme.textMuted)
        }
        VStack(spacing: 1) {
            ForEach(stills) { s in
                Button { selected = s } label: { row(s) }
                    .buttonStyle(.plain)
            }
        }
        .clipShape(RoundedRectangle(cornerRadius: 10))
        if loading {
            ProgressView().frame(maxWidth: .infinity)
        } else if let cursor {
            Button("Load older (\(stills.count) of \(meta?.total ?? stills.count))") {
                Task { await load(cursor: cursor) }
            }
            .font(.caption)
        }
    }

    private func summary(_ m: CameraStillsMeta) -> some View {
        let targets = m.targets ?? []
        let failing = targets.filter { ($0.last_error ?? "").isEmpty == false }
        return VStack(alignment: .leading, spacing: 2) {
            Text("\(m.total ?? 0) frames · \(m.changed ?? 0) scene changes · \(m.unknown ?? 0) undetermined")
                .font(.caption)
                .foregroundStyle(theme.text)
            Text(captureLine(m, targets: targets))
                .font(.caption2)
                .foregroundStyle(theme.textMuted)
            if !failing.isEmpty {
                Text("Failing: " + failing.prefix(3).map { "\($0.view ?? "main") (\($0.last_error ?? "?"))" }
                        .joined(separator: ", ")
                     + (failing.count > 3 ? " and \(failing.count - 3) more" : ""))
                    .font(.caption2)
                    .foregroundStyle(theme.warning)
            }
        }
    }

    private func captureLine(_ m: CameraStillsMeta, targets: [CameraStillTarget]) -> String {
        var s = (m.capture_stills ?? false) ? "Capture ON" : "Capture OFF — an operator enables it per camera"
        if targets.count > 1 { s += " · \(targets.count) views" }
        if let total = m.targets_total, total > targets.count {
            s += " (of \(total); \(total - targets.count) beyond the per-record limit are not captured)"
        }
        if let o = m.oldest, let n = m.newest { s += " · \(o) → \(n) UTC" }
        return s
    }

    private func row(_ s: CameraStill) -> some View {
        HStack(spacing: 6) {
            Text(s.captured_at)
                .font(.caption.monospaced())
                .foregroundStyle(theme.text)
            if let v = s.view {
                Text(v).font(.caption2).foregroundStyle(theme.textMuted).lineLimit(1)
            }
            Spacer()
            sceneBadge(s.scene_changed)
            if s.blob_present == false {
                Image(systemName: "externaldrive.badge.xmark")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
                    .accessibilityLabel("Bytes not preserved")
            }
        }
        .padding(8)
        .background(selected?.id == s.id ? theme.surface : theme.surfaceElevated)
    }

    @ViewBuilder
    private func sceneBadge(_ v: Bool?) -> some View {
        switch v {
        case .some(true):  Text("changed").font(.caption2.bold()).foregroundStyle(theme.warning)
        case .some(false): Text("same").font(.caption2).foregroundStyle(theme.textMuted)
        case .none:        Text("unknown").font(.caption2).foregroundStyle(theme.textMuted)
        }
    }

    private func load(cursor: String?) async {
        loading = true
        defer { loading = false }
        do {
            let env = try await apiClient.api.cameraStills(cameraId, limit: 24, cursor: cursor)
            stills = cursor == nil ? env.data : stills + env.data
            meta = env.meta ?? meta
            self.cursor = env.page?.cursor
            loadError = nil
        } catch {
            loadError = error.localizedDescription
        }
    }
}

/// One frame, fetched from the operator-gated raw route.
private struct CameraStillFrameView: View {
    let still: CameraStill
    let onClose: () -> Void

    @EnvironmentObject private var apiClient: APIClient
    @Environment(\.theme) private var theme

    @State private var image: Image?
    @State private var failure: String?

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Rectangle()
                .fill(theme.surfaceElevated)
                .aspectRatio(16.0 / 9.0, contentMode: .fit)
                .frame(maxWidth: .infinity)
                .overlay {
                    if let image {
                        image.resizable().scaledToFit()
                    } else if let failure {
                        Text(failure)
                            .font(.caption2)
                            .foregroundStyle(theme.textMuted)
                            .multilineTextAlignment(.center)
                            .padding(8)
                    } else {
                        ProgressView()
                    }
                }
                .clipShape(RoundedRectangle(cornerRadius: 10))
            HStack {
                Text("\(still.captured_at) UTC" + (still.view.map { " · \($0)" } ?? ""))
                    .font(.caption2.monospaced())
                    .foregroundStyle(theme.textMuted)
                Spacer()
                Button("Close", action: onClose).font(.caption2)
            }
            if let ocr = still.ocr_text, !ocr.isEmpty {
                Text("OCR: \(ocr)").font(.caption2).foregroundStyle(theme.textMuted)
            }
        }
        .task(id: still.id) { await fetch() }
    }

    private func fetch() async {
        image = nil
        failure = nil
        do {
            let data = try await apiClient.api.cameraStillRaw(still.id)
            guard Self.isImage(data), let img = Self.decode(data) else {
                failure = "The stored frame is not a recognisable image; it is not displayed."
                return
            }
            image = img
        } catch let APIError.http(code, body) {
            failure = Self.message(code: code, body: body)
        } catch {
            failure = "Frame unavailable — \(error.localizedDescription)"
        }
    }

    /// JPEG / PNG / GIF / WebP signatures only.
    static func isImage(_ d: Data) -> Bool {
        let b = [UInt8](d.prefix(12))
        guard b.count >= 4 else { return false }
        if b[0] == 0xFF && b[1] == 0xD8 && b[2] == 0xFF { return true }
        if b[0] == 0x89 && b[1] == 0x50 && b[2] == 0x4E && b[3] == 0x47 { return true }
        if b[0] == 0x47 && b[1] == 0x49 && b[2] == 0x46 { return true }
        if b.count >= 12 && b[0] == 0x52 && b[1] == 0x49 && b[2] == 0x46 && b[3] == 0x46
            && b[8] == 0x57 && b[9] == 0x45 && b[10] == 0x42 && b[11] == 0x50 { return true }
        return false
    }

    static func decode(_ d: Data) -> Image? {
        #if canImport(UIKit)
        return UIImage(data: d).map { Image(uiImage: $0) }
        #elseif canImport(AppKit)
        return NSImage(data: d).map { Image(nsImage: $0) }
        #else
        return nil
        #endif
    }

    /// The server's `{"error": …}` code, turned into the sentence an analyst
    /// needs: the raw route is operator-only and frames may be unpreserved.
    static func message(code: Int, body: String) -> String {
        struct E: Decodable { let error: String? }
        let reason = (try? JSONDecoder().decode(E.self, from: Data(body.utf8)))?.error ?? ""
        switch code {
        case 401: return "Sign in to view raw frames."
        case 403:
            return reason == "Platform operator access not configured"
                ? "Raw frames are operator-only, and no platform operator is configured on this server."
                : "Raw frames are restricted to platform operators (403)."
        case 410:
            return reason == "blob_evicted"
                ? "The frame record exists but its bytes were evicted by the evidence budget (410)."
                : "The frame record exists but its bytes were not preserved — capture_evidence is off for source 'camera-stills' (410)."
        case 404: return "Frame not found (404)."
        default:  return reason.isEmpty ? "Frame unavailable (HTTP \(code))." : "Frame unavailable: \(reason) (HTTP \(code))."
        }
    }
}
