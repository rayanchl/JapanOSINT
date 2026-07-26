import SwiftUI
import CoreLocation

// ─────────────────────────────────────────────────────────────────────────────
// Roadmap 27 — media attached to an intel item: thumbnail, dimensions, EXIF
// summary, and the valuable part, EXIF GPS.
//
// An image that carries a GPS tag puts an item on the map that had no
// coordinate of its own. That row is therefore the headline of this section,
// not a footnote in a properties table, and it gets its own tinted row with a
// "Show on map" action. This view does not know where the map is, so it calls
// `onShowOnMap`; the host supplies the destination.
//
// `ocr_text` is a MACHINE READING of pixels. It is labelled as such and never
// presented as asserted text — an OCR pass that reads 「東」 as 「柬」 changes
// the meaning of a report, and the reader has to know the difference between
// "the image says" and "a program thinks the image says".
//
// The list is a parameter rather than a fetch: `GET /api/intel/items/:uid/
// media` (core/media.h) has no client method in `API+Roadmap.swift` yet, and
// this component does not add networking of its own. The host passes the
// assets plus optional `loading` / `error` so the states still render here.
// ─────────────────────────────────────────────────────────────────────────────

struct MediaSection: View {
    let assets: [MediaAsset]
    var title: String = "Media"
    /// Driven by the host's fetch, so this view renders every state without
    /// owning a request.
    var loading: Bool = false
    var error: String? = nil
    var onRetry: (() -> Void)? = nil
    /// Called with the EXIF coordinate and the asset it came from.
    var onShowOnMap: ((CLLocationCoordinate2D, MediaAsset) -> Void)? = nil

    @Environment(\.theme) private var theme
    @State private var expandedOCR: Set<String> = []

    var body: some View {
        VStack(alignment: .leading, spacing: Space.md) {
            header

            if loading && assets.isEmpty {
                ProgressView()
                    .frame(maxWidth: .infinity, alignment: .center)
                    .padding(.vertical, Space.lg)
            } else if let error {
                errorBanner(error)
            } else if assets.isEmpty {
                emptyState
            } else {
                ForEach(assets) { asset in
                    assetCard(asset)
                }
            }
        }
    }

    // MARK: - Header

    private var header: some View {
        HStack(spacing: Space.sm) {
            Image(systemName: "photo.on.rectangle.angled")
                .font(.subheadline)
                .foregroundStyle(theme.accentAlt)
            Text(title)
                .font(.headline)
                .foregroundStyle(theme.text)
            if !assets.isEmpty {
                Text("\(assets.count)")
                    .font(.caption.monospacedDigit())
                    .foregroundStyle(theme.textMuted)
            }
            Spacer(minLength: 0)
            if geolocatedCount > 0 {
                Pill(text: "\(geolocatedCount) GEOTAGGED", tone: .accent,
                     icon: "mappin.and.ellipse", maxWidth: 160)
            }
            if loading && !assets.isEmpty {
                ProgressView().controlSize(.mini)
            }
        }
    }

    private var geolocatedCount: Int {
        assets.reduce(into: 0) { total, asset in
            if mediaCoordinate(asset) != nil { total += 1 }
        }
    }

    // MARK: - Asset card

    private func assetCard(_ a: MediaAsset) -> some View {
        VStack(alignment: .leading, spacing: Space.sm) {
            HStack(alignment: .top, spacing: Space.md) {
                thumbnail(a)
                VStack(alignment: .leading, spacing: Space.xs) {
                    Text(mediaFileName(a.url))
                        .font(.caption.monospaced())
                        .foregroundStyle(theme.text)
                        .lineLimit(1)
                        .truncationMode(.middle)

                    HStack(spacing: Space.sm) {
                        Text(mediaDimensions(a))
                            .font(.caption2.monospacedDigit())
                            .foregroundStyle(theme.textMuted)
                        Text(mediaByteText(a.bytes))
                            .font(.caption2.monospacedDigit())
                            .foregroundStyle(theme.textMuted)
                        if let ct = a.content_type, !ct.isEmpty {
                            Text(ct)
                                .font(.caption2.monospaced())
                                .foregroundStyle(theme.textMuted)
                                .lineLimit(1)
                        }
                        Spacer(minLength: 0)
                    }

                    exifSummary(a)
                }
            }

            if let coord = mediaCoordinate(a) {
                exifLocationRow(a, coord: coord)
            }

            ocrBlock(a)
        }
        .padding(Space.md)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(theme.surfaceElevated,
                    in: RoundedRectangle(cornerRadius: Radius.md))
    }

    private func thumbnail(_ a: MediaAsset) -> some View {
        let side: CGFloat = 92
        return AsyncImage(url: URL(string: a.url)) { phase in
            switch phase {
            case .success(let image):
                image
                    .resizable()
                    .scaledToFill()
            case .failure:
                ZStack {
                    theme.surface
                    Image(systemName: "photo.badge.exclamationmark")
                        .font(.title3)
                        .foregroundStyle(theme.textMuted)
                }
            case .empty:
                ZStack {
                    theme.surface
                    ProgressView().controlSize(.small)
                }
            @unknown default:
                theme.surface
            }
        }
        .frame(width: side, height: side)
        .clipped()
        .clipShape(RoundedRectangle(cornerRadius: Radius.sm))
        .overlay(
            RoundedRectangle(cornerRadius: Radius.sm)
                .strokeBorder(theme.textMuted.opacity(0.25),
                              lineWidth: Stroke.hairline)
        )
        .accessibilityLabel("Attached image")
    }

    // MARK: - EXIF

    @ViewBuilder
    private func exifSummary(_ a: MediaAsset) -> some View {
        let camera = mediaCameraLine(a)
        let captured = mediaCaptureTime(a)
        if camera != nil || captured != nil {
            VStack(alignment: .leading, spacing: Space.xs) {
                if let camera {
                    HStack(spacing: Space.xs) {
                        Image(systemName: "camera")
                            .font(.system(size: 9))
                            .foregroundStyle(theme.textMuted)
                        Text(camera)
                            .font(.caption2)
                            .foregroundStyle(theme.text)
                            .lineLimit(1)
                    }
                }
                if let captured {
                    HStack(spacing: Space.xs) {
                        Image(systemName: "clock")
                            .font(.system(size: 9))
                            .foregroundStyle(theme.textMuted)
                        Text(captured)
                            .font(.caption2.monospacedDigit())
                            .foregroundStyle(theme.text)
                            .lineLimit(1)
                    }
                }
            }
        } else if a.exif == nil || a.exif?.isEmpty == true {
            Text("No EXIF metadata")
                .font(.system(size: 10))
                .foregroundStyle(theme.textMuted)
        }
    }

    private func exifLocationRow(_ a: MediaAsset,
                                 coord: CLLocationCoordinate2D) -> some View {
        HStack(spacing: Space.sm) {
            Image(systemName: "mappin.and.ellipse")
                .font(.subheadline)
                .foregroundStyle(theme.accentAlt)
            VStack(alignment: .leading, spacing: Space.xs) {
                Text("EXIF location")
                    .font(.caption.weight(.semibold))
                    .foregroundStyle(theme.text)
                Text(String(format: "%.5f, %.5f", coord.latitude, coord.longitude))
                    .font(.caption2.monospaced())
                    .foregroundStyle(theme.textMuted)
                    .textSelection(.enabled)
            }
            Spacer(minLength: 0)
            if let show = onShowOnMap {
                Button {
                    Haptics.selection()
                    show(coord, a)
                } label: {
                    Label("Show on map", systemImage: "map")
                        .font(.caption2)
                }
                .buttonStyle(.bordered)
                .controlSize(.small)
            }
        }
        .padding(Space.sm)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(theme.accentAlt.opacity(0.12),
                    in: RoundedRectangle(cornerRadius: Radius.sm))
    }

    // MARK: - OCR

    @ViewBuilder
    private func ocrBlock(_ a: MediaAsset) -> some View {
        if let raw = a.ocr_text,
           !raw.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
            let isExpanded = expandedOCR.contains(a.id)
            VStack(alignment: .leading, spacing: Space.xs) {
                HStack(spacing: Space.sm) {
                    Pill(text: "OCR · MACHINE-READ", tone: .warning,
                         icon: "text.viewfinder", maxWidth: 190)
                    Spacer(minLength: 0)
                    Button {
                        if isExpanded {
                            expandedOCR.remove(a.id)
                        } else {
                            expandedOCR.insert(a.id)
                        }
                    } label: {
                        Text(isExpanded ? "Show less" : "Show all")
                            .font(.caption2)
                    }
                    .buttonStyle(.plain)
                    .foregroundStyle(theme.accent)
                }
                Text(raw)
                    .font(.caption2.monospaced())
                    .foregroundStyle(theme.text)
                    .lineLimit(isExpanded ? nil : 3)
                    .textSelection(.enabled)
                    .fixedSize(horizontal: false, vertical: true)
                Text("Text extracted from the image by OCR. It is a program's reading of pixels, not asserted content — check it against the image before quoting it.")
                    .font(.system(size: 10))
                    .foregroundStyle(theme.textMuted)
                    .fixedSize(horizontal: false, vertical: true)
            }
            .padding(Space.sm)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(theme.warning.opacity(0.10),
                        in: RoundedRectangle(cornerRadius: Radius.sm))
        }
    }

    // MARK: - Empty / error

    private var emptyState: some View {
        ContentUnavailableView {
            Label {
                Text("No media").foregroundStyle(theme.text)
            } icon: {
                Image(systemName: "photo.on.rectangle").foregroundStyle(theme.textMuted)
            }
        } description: {
            Text("No images were attached to this item, or the media pipeline has not analyzed it yet.")
                .foregroundStyle(theme.textMuted)
        }
    }

    private func errorBanner(_ message: String) -> some View {
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
            if let onRetry {
                Button {
                    onRetry()
                } label: {
                    Label("Retry", systemImage: "arrow.clockwise")
                }
                .buttonStyle(.bordered)
                .controlSize(.small)
            }
        }
        .padding(Space.md)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(theme.danger.opacity(0.12),
                    in: RoundedRectangle(cornerRadius: Radius.md))
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// MARK: - File-private EXIF readers
//
// The server encodes EXIF as nested groups (core/media.c `media_exif_to_json`):
//
//     {"gps":{"lat":…,"lon":…,"alt_m":…},
//      "camera":{"make":…,"model":…,"software":…,"lens":…},
//      "time":{"original":…,"digitized":…,"modified":…},
//      "image":{"width":…,"height":…,"orientation":…}}
//
// `AnyCodable` decodes a nested object to `[String: Any]`, so these readers
// walk group-then-key. Flat spellings are accepted too, in case a different
// analyzer ever writes the column.
// ─────────────────────────────────────────────────────────────────────────────

/// GPS from EXIF. Prefers `MediaAsset.exifCoordinate`, then reads the nested
/// `gps.lat` / `gps.lon` the C encoder actually emits — the DTO's accessor
/// looks for FLAT `gps_lat` / `gps_lon` keys, which this server never writes,
/// so relying on it alone would silently hide every geotagged image.
/// Out-of-range and null-island values are rejected so a bad tag never plants
/// a pin in the Atlantic.
private func mediaCoordinate(_ a: MediaAsset) -> CLLocationCoordinate2D? {
    if let c = a.exifCoordinate,
       mediaValidCoordinate(c.latitude, c.longitude) {
        return c
    }
    guard let exif = a.exif else { return nil }
    var lat = mediaNumber(exif["gps_lat"]?.value)
    var lon = mediaNumber(exif["gps_lon"]?.value)
    if lat == nil || lon == nil, let gps = exif["gps"]?.value as? [String: Any] {
        lat = lat ?? mediaNumber(gps["lat"])
        lon = lon ?? mediaNumber(gps["lon"])
    }
    guard let lat, let lon, mediaValidCoordinate(lat, lon) else { return nil }
    return CLLocationCoordinate2D(latitude: lat, longitude: lon)
}

private func mediaValidCoordinate(_ lat: Double, _ lon: Double) -> Bool {
    guard lat.isFinite, lon.isFinite else { return false }
    guard (-90...90).contains(lat), (-180...180).contains(lon) else { return false }
    return lat != 0 || lon != 0
}

/// `AnyCodable` yields `Int` or `Double` for JSON numbers; a string is accepted
/// too so a re-encoded payload still reads.
private func mediaNumber(_ v: Any?) -> Double? {
    switch v {
    case let d as Double: return d
    case let i as Int:    return Double(i)
    case let n as NSNumber: return n.doubleValue
    case let s as String: return Double(s)
    default:              return nil
    }
}

private func mediaExifString(_ a: MediaAsset, group: String, keys: [String]) -> String? {
    guard let exif = a.exif else { return nil }
    if let g = exif[group]?.value as? [String: Any] {
        for k in keys {
            if let s = g[k] as? String,
               !s.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
                return s
            }
        }
    }
    // Flat fallback, e.g. "camera_make".
    for k in keys {
        if let s = exif["\(group)_\(k)"]?.value as? String,
           !s.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
            return s
        }
    }
    return nil
}

/// "Canon · EOS R6" — make and model joined, either alone when only one is
/// present, nil when EXIF carried no camera identity at all.
private func mediaCameraLine(_ a: MediaAsset) -> String? {
    let make = mediaExifString(a, group: "camera", keys: ["make"])
    let model = mediaExifString(a, group: "camera", keys: ["model"])
    switch (make, model) {
    case let (m?, n?):
        // Many bodies repeat the make inside the model ("NIKON CORPORATION" /
        // "NIKON D850"); don't print it twice.
        return n.localizedCaseInsensitiveContains(m) ? n : "\(m) · \(n)"
    case let (m?, nil): return m
    case let (nil, n?): return n
    default: return nil
    }
}

/// EXIF capture time. The tag format is "YYYY:MM:DD HH:MM:SS" — colons in the
/// date, which no ISO parser accepts — so it is normalised before display and
/// passed through untouched when it is some other shape.
private func mediaCaptureTime(_ a: MediaAsset) -> String? {
    guard let raw = mediaExifString(a, group: "time",
                                    keys: ["original", "digitized", "modified"])
    else { return nil }
    return "Captured \(mediaNormalizedExifDate(raw))"
}

private func mediaNormalizedExifDate(_ raw: String) -> String {
    let parts = raw.split(separator: " ", maxSplits: 1, omittingEmptySubsequences: false)
    guard parts.count == 2 else { return raw }
    let date = parts[0].replacingOccurrences(of: ":", with: "-")
    return "\(date) \(parts[1])"
}

private func mediaDimensions(_ a: MediaAsset) -> String {
    if let w = a.width, let h = a.height, w > 0, h > 0 {
        return "\(w) × \(h)"
    }
    // Fall back to the EXIF image group when the columns were never filled.
    if let exif = a.exif, let img = exif["image"]?.value as? [String: Any],
       let w = mediaNumber(img["width"]), let h = mediaNumber(img["height"]),
       w > 0, h > 0 {
        return "\(Int(w)) × \(Int(h))"
    }
    return "dimensions unknown"
}

private func mediaByteText(_ n: Int?) -> String {
    guard let n else { return "size unknown" }
    return ByteCountFormatter.string(fromByteCount: Int64(n), countStyle: .file)
}

/// Last path component of the asset URL, for a row label that is readable
/// without printing a 200-character CDN URL.
private func mediaFileName(_ urlString: String) -> String {
    if let url = URL(string: urlString) {
        let name = url.lastPathComponent
        if !name.isEmpty, name != "/" { return name }
        if let host = url.host { return host }
    }
    return urlString
}
