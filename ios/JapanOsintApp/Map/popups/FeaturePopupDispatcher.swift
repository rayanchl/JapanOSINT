import SwiftUI

/// Routes a `GeoFeature` to the appropriate popup view based on its layer id.
/// Shared by MapTab and SavedTab so the routing rules stay in one place.
/// `showsMiniMap` injects an embedded MapKit preview into the Coordinates
/// section — useful when the popup is opened from a non-map context (e.g.
/// the Saved tab list/grid) where the user has no spatial reference.
@ViewBuilder
func featurePopup(for feat: GeoFeature, showsMiniMap: Bool = false) -> some View {
    let id = feat.layerId.lowercased()
    if id == "cameras" || id == "camera-discovery" || id.contains("camera") || id.contains("webcam") {
        CameraPopup(feature: feat, showsMiniMap: showsMiniMap)
    } else if id.contains("station") || id.contains("subway") || id.contains("train") {
        StationPopup(feature: feat, showsMiniMap: showsMiniMap)
    } else if id.contains("plane") || id.contains("flight") || id.contains("ship") || id.contains("ais") || id.contains("vehicle") {
        VehiclePopup(feature: feat, showsMiniMap: showsMiniMap)
    } else {
        FeaturePopup(feature: feat, showsMiniMap: showsMiniMap)
    }
}
