import SwiftUI

/// Mounted inside Console's NavigationStack. "Show on map" buttons inside
/// the panel publish the coordinate through `MapNavigation`, which switches
/// to the Map tab and lets it consume `pendingFlyTo`.
struct CameraDiscoveryTab: View {
    @EnvironmentObject var nav: MapNavigation

    var body: some View {
        CameraDiscoveryView { coord, feat in
            nav.showOnMap(coord, feature: feat)
        }
    }
}
