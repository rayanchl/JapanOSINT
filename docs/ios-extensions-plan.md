# iOS extension targets — setup plan (roadmap items 35 & 36)

> **STATUS (2026-07-28).** The app-side code that needs **no** new target is now
> written and wired into the app target:
> - `Intents/` — `IntentRouter`, `SearchIntent`, `ExposureIntent`,
>   `JapanOsintShortcuts`, `SpotlightIndexer` (created this pass; the earlier
>   claim that these were committed was inaccurate — they did not exist).
>   `AddToCaseIntent` is intentionally **deferred**: a correct version needs a
>   case-context surface (a picker-on-drain path) the app doesn't have yet, and
>   a placeholder Siri action with no item context would be worse than none.
> - `Background/BackgroundRefresh.swift` — `BGTaskScheduler` register/schedule,
>   registered from `JapanOsintApp.init`.
> - `Widgets/WidgetSnapshotBuilder.swift` — the missing app-side writer that
>   makes the widgets non-empty; called on gate-ready, scene-activation, and the
>   BGTask. (`SharedSnapshot.write` previously had **zero callers**.)
> - `JapanOsintApp.swift` wiring: `onOpenURL` (share-queue drain + permalink
>   token → Search), `onContinueUserActivity(CSSearchableItemActionType)`
>   (Spotlight deep-link), `onReceive(IntentRouter.$pending)`, scene-phase drain.
> - `Info.plist`: `NSSupportsLiveActivities`, `BGTaskSchedulerPermittedIdentifiers`,
>   `UIBackgroundModes`.
> - Entitlements files created for all three targets (App Group), but **not**
>   wired into build settings — see Step 1.
>
> **What still requires Xcode** (unchanged from below): create the two extension
> targets, tick App Group / Background Modes capabilities, set
> `CODE_SIGN_ENTITLEMENTS`, register the App Group on the developer portal. The
> entitlements files are left unreferenced on purpose: pointing a target at an
> App Group that isn't provisioned fails code-signing, so that wiring must
> happen in the same Xcode pass that registers the group.

Everything here is written and committed. What is **not** done is the part
that requires Xcode: creating two extension targets and ticking membership
boxes. This document is the exact sequence.

> **Read this first.** `ios/JapanOsintApp.xcodeproj` uses a
> `PBXFileSystemSynchronizedRootGroup` over `ios/JapanOsintApp/`, which means
> **every `.swift` file under that folder is automatically compiled into the
> main app target.** A widget's `@main struct …: Widget` compiled into an app
> target is a build error. That is why the extension sources live in
> `ios/JapanOsintWidgets/` and `ios/JapanOsintShare/` — *outside* the synced
> group — and why you must add those folders to their own targets by hand.

---

## What actually needs a new target

| Roadmap | Feature | New target? |
|---|---|---|
| 35 | Home-screen + Lock Screen widgets | **Yes** — Widget Extension |
| 35 | Live Activity (running OSINT search) | No — same Widget Extension |
| 36 | Share extension (URL/image/text → app) | **Yes** — Share Extension |
| 36 | App Intents / Siri / Shortcuts | **No** — main app target |
| 36 | Spotlight (CoreSpotlight) indexing | **No** — main app target |
| 37 | `BGTaskScheduler` background refresh | **No** — main app target |

Half of item 36 and all of 37 need no Xcode surgery at all. Do those first if
you want value before touching the project file.

---

## Step 1 — App Group (do this before anything else)

Both extensions and the app must share a container: the widget cannot make
network calls on your behalf with the user's bearer token, so the app writes a
small snapshot file and the widget reads it.

1. Apple Developer portal → Identifiers → App Groups → register
   **`group.com.rayanchl.japanosint`**
   (If you prefer a different id, change `AppGroup.identifier` in
   `ios/JapanOsintApp/Widgets/SharedSnapshot.swift` — it is defined in exactly
   one place.)
2. In Xcode, for **each** of the three targets (app, widget, share):
   Signing & Capabilities → **+ Capability** → **App Groups** → tick the group.
3. Confirm each target gained an `.entitlements` file containing
   `com.apple.security.application-groups`.

**If the group is missing or mistyped, `containerURL(forSecurityApplicationGroupIdentifier:)`
returns nil and every widget silently shows placeholder data.** That failure
mode looks exactly like "the widget doesn't work", so verify this first.

---

## Step 2 — Widget Extension target (item 35)

1. File → New → Target → **Widget Extension**.
   - Product name: `JapanOsintWidgets`
   - **Tick "Include Live Activity"** (adds `NSSupportsLiveActivities` to the
     extension's Info.plist).
   - Do **not** tick "Include Configuration Intent" — these widgets are static.
2. Xcode generates a `JapanOsintWidgets/` folder with boilerplate. **Delete the
   generated `.swift` files**, then drag in the committed folder
   `ios/JapanOsintWidgets/` (Create groups, target = JapanOsintWidgets only).
3. Tick **both** app and widget membership (File Inspector → Target
   Membership) on exactly these two files — they are the only shared ones:
   - `ios/JapanOsintApp/Widgets/SharedSnapshot.swift`
   - `ios/JapanOsintApp/Widgets/SearchActivityAttributes.swift`

   Everything else is one side or the other. The Live Activity's *view*
   (`SearchLiveActivity.swift`) stays extension-only; only the attributes are
   shared, because the app requests and updates the activity while the
   extension renders it.
4. Main app's Info.plist: add **`NSSupportsLiveActivities` = YES**.
   (The extension's plist gets it from the template; the *app* needs it too or
   `Activity.request(...)` throws at runtime.)
5. Deployment target for the extension: **iOS 26.1**, matching the app.

### Files committed for this target
```
ios/JapanOsintWidgets/
  JapanOsintWidgetsBundle.swift   @main WidgetBundle — the entry point
  AlertInboxWidget.swift          small/medium: unread count + latest matches
  QuakeWidget.swift               small/medium: latest quake / active warnings
  InboxAccessoryWidget.swift      Lock Screen accessory: unread badge
  SearchLiveActivity.swift        Live Activity for a running OSINT search
  WidgetSnapshotReader.swift      reads the App Group JSON, never the network
```

### How the widget gets data
It does **not** call the API. `WidgetSnapshotReader` reads
`widget-snapshot.json` from the App Group container. The app writes that file
(`SharedSnapshot.write(...)`) on three occasions: foreground refresh, inbox
poll, and the BGTask in item 37. After writing, the app calls
`WidgetCenter.shared.reloadAllTimelines()`.

This is deliberate. A widget extension has no access to the Keychain-backed
bearer token and gets killed for slow work — a widget that tries to
authenticate and fetch will mostly render "—".

**Consequence to accept:** widget freshness is bounded by how often the app
runs, not by a timer. With item 37's background refresh that is ~15 min
best-effort; without it, only when the user opens the app. Build 37 first or
the widgets will look stale and it will not be the widget's fault.

---

## Step 3 — Share Extension target (item 36)

1. File → New → Target → **Share Extension**. Product name: `JapanOsintShare`.
2. Delete the generated files; drag in `ios/JapanOsintShare/`.
3. Add the App Group capability (Step 1).
4. Replace the generated `Info.plist`'s `NSExtensionActivationRule` with the
   one in `ios/JapanOsintShare/Info-activation-snippet.plist` — it accepts
   1 URL, 1 image, or 1 text item and rejects everything else, so the app does
   not appear in the share sheet for content it cannot use.

The extension does not call the API either. It writes the shared payload into
the App Group as a queued item and opens the app via
`japanosint://share?id=<uuid>` — the app drains the queue on launch. Doing the
work in the app means one auth path, one error surface, and no duplicated
networking in a process that gets ~15 s to live.

---

## Step 4 — App Intents + Spotlight (no new target)

Already committed under `ios/JapanOsintApp/Intents/`:
```
JapanOsintShortcuts.swift    AppShortcutsProvider — Siri phrases
SearchIntent.swift           "Search JapanOSINT for …"
ExposureIntent.swift         "Check exposure for …"
AddToCaseIntent.swift        "Add this to a case"
SpotlightIndexer.swift       CoreSpotlight indexing for cases + saved items
```
These compile into the app target automatically. Two things to wire once:

1. `JapanOsintApp.swift` — call `SpotlightIndexer.shared.reindexIfNeeded()`
   after auth resolves.
2. Handle `CSSearchableItemActionType` in `onContinueUserActivity` to deep-link
   a Spotlight hit. The URL router from item 38's permalinks is the same
   mechanism; reuse `PermalinkResolver`.

---

## Step 5 — Background refresh (item 37, no new target)

1. Signing & Capabilities → **+ Capability → Background Modes** → tick
   **Background fetch** and **Background processing**.
2. Info.plist → `BGTaskSchedulerPermittedIdentifiers` (array):
   - `com.rayanchl.japanosint.refresh`
   - `com.rayanchl.japanosint.sync`
3. Register both in `JapanOsintApp.init()` — see
   `ios/JapanOsintApp/Background/BackgroundRefresh.swift`.

The refresh task delta-syncs using the keyset cursor already in
`intelapi.h`, then writes the widget snapshot. That single call is what makes
item 35 look alive.

---

## Verification checklist

- [ ] App Group id identical in all three targets *and* in `SharedSnapshot.swift`
- [ ] `SharedSnapshot.swift` has membership in **both** app and widget targets
- [ ] `NSSupportsLiveActivities` in the **app's** Info.plist, not just the extension's
- [ ] Widget shows real data after opening the app once (proves the container path)
- [ ] Share sheet shows JapanOSINT for a URL, and **not** for a PDF
- [ ] "Hey Siri, search JapanOSINT for …" resolves
- [ ] `BGTaskSchedulerPermittedIdentifiers` matches the registered ids exactly —
      a mismatch throws at registration, which is easy to miss in a launch log

## Known limits, stated up front

- **Live Activity needs a real device.** The simulator does not render them.
- **Widget timelines are advisory.** iOS decides refresh budget; a widget that
  "should" update every 15 minutes may not. Do not treat it as a monitor.
- **The share extension has no auth UI.** If the user is signed out, the queued
  item still lands and the app surfaces the sign-in when it drains the queue.
- **Spotlight indexing is per-device and not encrypted by us.** Index case
  names and saved-item titles only — do not index annotation bodies or breach
  identifiers, which is why `SpotlightIndexer` has an explicit allowlist rather
  than reflecting over the model.
