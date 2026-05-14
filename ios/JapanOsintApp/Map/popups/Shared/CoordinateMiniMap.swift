import SwiftUI
import MapKit

/// Small embedded MapKit preview centered on a coordinate, with a single
/// pin. Used in popup "Coordinates" sections when the popup might be opened
/// from a non-map context (Saved tab list / grid) — gives spatial context
/// without forcing the user to switch tabs.
struct CoordinateMiniMap: View {
    let coordinate: CLLocationCoordinate2D
    var height: CGFloat = 160
    var spanDegrees: Double = 0.02

    @State private var cameraPosition: MapCameraPosition

    init(coordinate: CLLocationCoordinate2D, height: CGFloat = 160, spanDegrees: Double = 0.02) {
        self.coordinate = coordinate
        self.height = height
        self.spanDegrees = spanDegrees
        self._cameraPosition = State(initialValue: .region(
            MKCoordinateRegion(
                center: coordinate,
                span: MKCoordinateSpan(latitudeDelta: spanDegrees, longitudeDelta: spanDegrees)
            )
        ))
    }

    var body: some View {
        Map(position: $cameraPosition, interactionModes: [.pan, .zoom]) {
            Annotation("", coordinate: coordinate) {
                Image(systemName: "mappin.circle.fill")
                    .font(.title2)
                    .foregroundStyle(.red)
                    .background(Circle().fill(.white).frame(width: 22, height: 22))
            }
        }
        .mapStyle(.standard(elevation: .realistic, pointsOfInterest: .excludingAll))
        .frame(height: height)
        .clipShape(RoundedRectangle(cornerRadius: 12, style: .continuous))
        .overlay(
            RoundedRectangle(cornerRadius: 12, style: .continuous)
                .strokeBorder(Color.primary.opacity(0.08), lineWidth: 0.5)
        )
    }
}
