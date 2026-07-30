import Foundation

/// The inputs `CameraFeedView` needs, pulled out of a free-form property bag.
///
/// Camera records reach the app through two unrelated doors: as a `GeoFeature`
/// on the map (`CameraPopup`) and as an `IntelItem` in the Intel tab
/// (`IntelDetail`) — the collectors write the same properties to both. This
/// type is the one place that knows which keys carry the feed, so the Intel
/// path can't silently drift into rendering a camera as a table of strings.
struct CameraFeedFields {
    let directSnapshotURL: String?
    let pageURL: String?
    let youtubeID: String?
    let hlsURL: String?
    let discoveryChannel: String?
    let cameraUID: String?
    /// Original aggregator page URL — set by the server when the camera URL was
    /// upgraded (e.g. scs.com.ua → YouTube channel-live), so `CameraFeedView`
    /// can fall back to a snapshot of it when the upgraded iframe fails.
    let originalPageURL: String?

    // Ordered candidate keys — collectors are not consistent about which one
    // they emit, so first non-empty string wins.
    private static let directKeys = ["thumbnail_url", "snapshot_url", "image", "thumbnail", "photo"]
    private static let pageKeys   = ["url", "source_url", "page_url", "embed_url", "link"]

    init(properties: [String: AnyCodable]) {
        directSnapshotURL = Self.firstString(properties, Self.directKeys)
        pageURL           = Self.firstString(properties, Self.pageKeys)
        youtubeID         = Self.string(properties, "youtube_id")
        hlsURL            = Self.string(properties, "hls_url")
        cameraUID         = Self.string(properties, "camera_uid")
        originalPageURL   = Self.string(properties, "original_page_url")

        // `discovery_channels` is serialized as either a JSON array or a bare
        // string depending on the collector; `discovery_channel` is the
        // singular form some emit instead.
        if let arr = properties["discovery_channels"]?.value as? [Any],
           let first = arr.first as? String, !first.isEmpty {
            discoveryChannel = first
        } else if let s = Self.string(properties, "discovery_channels") {
            discoveryChannel = s
        } else {
            discoveryChannel = Self.string(properties, "discovery_channel")
        }
    }

    /// Keys this type reads. A properties table can subtract these so the same
    /// values aren't listed underneath the feed that already renders them.
    static var consumedKeys: Set<String> {
        Set(directKeys + pageKeys + [
            "original_page_url", "youtube_id", "hls_url", "camera_uid",
            "discovery_channels", "discovery_channel",
        ])
    }

    /// Whether this property bag describes a camera at all.
    ///
    /// Deliberately broader than "has a resolvable feed": a camera whose feed
    /// resolves to `.linkOnly` is still a camera, and the caller should show it
    /// the camera surface (with its page link) rather than a generic detail
    /// view. `surveillance_type` is the key the `public-cameras` collector
    /// uses; the rest come from the discovery pipeline.
    static func describesCamera(_ properties: [String: AnyCodable]) -> Bool {
        if properties["camera_uid"] != nil || properties["camera_id"] != nil { return true }
        if properties["camera_type"] != nil { return true }
        if let t = string(properties, "surveillance_type")?.lowercased(), t == "camera" { return true }
        return false
    }

    // MARK: - Helpers

    private static func string(_ props: [String: AnyCodable], _ key: String) -> String? {
        guard let s = props[key]?.value as? String, !s.isEmpty else { return nil }
        return s
    }

    private static func firstString(_ props: [String: AnyCodable], _ keys: [String]) -> String? {
        for key in keys {
            if let s = string(props, key) { return s }
        }
        return nil
    }
}
