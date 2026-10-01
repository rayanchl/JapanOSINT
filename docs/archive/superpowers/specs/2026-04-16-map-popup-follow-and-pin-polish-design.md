# Map Popup Follow + Pin Polish

Small UI polish for the map: popup tracks its pin across pan/zoom, popup grows a
downward ridge pointing at the pin, and layer icons are 20% larger and 20%
darker while the legend keeps its original bright colors.

## 1. Popup follows its map pin

**Problem.** `MapPage` stores the click's screen pixel (`{x, y}`) and feeds it
to `MapPopup` as an absolute CSS position. When the user pans or zooms after
clicking, the popup stays glued to the same screen pixel while the pin moves
underneath, so they desync.

**Change.**

- On feature click, `MapView` passes the feature's **lng/lat** to
  `onFeatureClick` (instead of `{x, y}`) — taken from
  `feature.geometry.coordinates` for point features.
- `MapView` also exposes its `maplibregl.Map` via a ref prop
  (`mapRef`) so `MapPage` can project the lng/lat on demand.
- `MapPage` keeps the popup's **lngLat** in state and uses a small hook
  `useMapProjection(mapRef, lngLat)` that:
  - subscribes to `map.on('move')` and `map.on('moveend')`,
  - recomputes `map.project([lng, lat])` on every frame via
    `requestAnimationFrame` throttling,
  - returns `{x, y}` in screen pixels, or `null` if `lngLat` is offscreen.
- `MapPopup` continues to render at the provided `position`; no API change
  beyond the prop already existing.

**Non-goals.** No drag-to-move, no viewport clamping, no hide-when-offscreen
logic beyond returning `null` position (popup simply stays at last known
position if projection fails).

## 2. Ridge on the popup pointing at the pin

Add a downward-pointing triangle to the bottom of the popup container, centered
horizontally, so the card visually "points" at the clicked pin.

- Implemented as a CSS pseudo-element (`::after`) on the popup root.
- Two-layer technique: outer triangle in the popup's border color, inner
  triangle (1px inset) in the popup's background, so the 1px border is
  preserved around the notch.
- Size: ~8px tall, ~14px wide. Color matches the existing `glass-panel`
  border + background — no layer tinting.
- Popup already uses `transform: translate(-50%, -110%)`, so the ridge sits
  directly above the pin.

## 3. Icons 20% bigger and 20% darker

- **Bigger.** `UNIFORM_ICON_SIZE` in `client/src/components/map/MapView.jsx`
  goes from `0.5` → `0.6`. Applies uniformly across every symbol layer.
- **Darker.** In `registerLayerIcons`, the color passed to `rasterizeIcon`
  is darkened by 20% (multiply each RGB channel by 0.8, preserving hue).
  Introduce a tiny helper `darkenHex(hex, factor)` co-located in MapView (or
  in `utils/iconRaster.js` if cleaner).
- **Legend unchanged.** `LayerPanel` continues to read `def.color` directly,
  so swatches stay bright. Only the rasterized pin tint is darkened.

## Files touched

- `client/src/components/map/MapView.jsx` — icon size, darker tint, emit
  `lngLat` on click, accept + populate a `mapRef` prop.
- `client/src/components/map/MapPage.jsx` — store `lngLat` instead of `xy`,
  use `useMapProjection` to derive screen position.
- `client/src/components/map/MapPopup.jsx` — add the ridge pseudo-element via
  a new CSS class (or inline via `index.css`).
- `client/src/hooks/useMapProjection.js` — new hook.
- `client/src/styles/index.css` — CSS for the popup ridge if not inlined.

## Testing

Manual: click a pin, pan + zoom, confirm popup follows. Confirm ridge points
at the pin. Confirm icons visibly larger and darker; confirm LayerPanel
swatches are unchanged.
