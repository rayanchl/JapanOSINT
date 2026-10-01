# Adding the WidgetKit + Share extension targets (Xcode, ~20 min)

**Status today:** `JapanOsintApp.xcodeproj` contains exactly **one**
`PBXNativeTarget` and one `PBXFileSystemSynchronizedRootGroup` (`path =
JapanOsintApp`). Consequently:

* `ios/JapanOsintWidgets/*.swift` (6 files) and
  `ios/JapanOsintShare/ShareViewController.swift` **never compile** — nothing
  references them.
* `CODE_SIGN_ENTITLEMENTS` appears nowhere in `project.pbxproj`, so all three
  `.entitlements` files are inert. `AppGroup.containerURL` therefore returns
  `nil` on every device, forever.
* Because of that, the widget snapshot, the share queue and `BackgroundRefresh`
  are all gated off at runtime (see `AppGroup.isConfigured`) so they don't burn
  background budget writing into a container that doesn't exist.
* `NSSupportsLiveActivities` has been **removed** from `Info.plist` — see step 7
  for when to put it back.

**Do not hand-edit `project.pbxproj` to fix this.** Adding two targets by hand
means inventing a dozen consistent 24-hex object IDs across
`PBXNativeTarget` / `PBXFileSystemSynchronizedRootGroup` /
`PBXCopyFilesBuildPhase` (Embed Foundation Extensions) / `PBXTargetDependency` /
`PBXContainerItemProxy` / `XCConfigurationList` / `XCBuildConfiguration` and
getting the `objectVersion = 77` schema exactly right. A single mistake yields a
project Xcode refuses to open. The GUI writes all of it correctly in a few
minutes.

Everything below is written against the current project settings:

| Setting | Value |
| --- | --- |
| App bundle id | `RCorp.JapanOsintApp` |
| Team | `YXQHVT52SE` |
| iOS deployment target | `26.1` |
| macOS deployment target | `26.0` |
| Swift version | `5.0` |
| App Group id | `group.com.rayanchl.japanosint` — **must** match `AppGroup.identifier` in `JapanOsintApp/Widgets/SharedSnapshot.swift` |

---

## Step 0 — register the App Group on the developer portal (do this first)

Ticking **App Groups** in Xcode before the group exists makes signing fail with
a provisioning error that reads like a certificate problem.

1. <https://developer.apple.com> → Certificates, Identifiers & Profiles →
   **Identifiers** → filter **App Groups** → **+**.
2. Description: `JapanOSINT shared container`.
   Identifier: `group.com.rayanchl.japanosint`.
3. Still under **Identifiers**, open the App ID `RCorp.JapanOsintApp` → enable
   **App Groups** → **Edit** → tick the new group → Save.

You'll repeat that last part for the two extension App IDs after Xcode creates
them (step 5).

---

## Step 1 — create the widget extension target

1. Xcode → **File ▸ New ▸ Target… ▸ iOS ▸ Widget Extension**.
2. Product Name: **`JapanOsintWidgets`** (exact — it must match the existing
   folder name so the files land in the right place).
3. **Untick "Include Configuration App Intent"**. The provider in
   `WidgetSnapshotReader.swift` is a plain `TimelineProvider`, not an
   `AppIntentTimelineProvider`; leaving it ticked generates a second
   `@main` entry point that clashes with `JapanOsintWidgetsBundle`.
4. **Tick "Include Live Activity"** — this adds `ActivityKit` linkage and the
   `SupportsLiveActivities` plumbing on the extension side.
5. Embed in Application: **JapanOsintApp**. Team: **YXQHVT52SE**.
6. When Xcode offers to activate the new scheme: **Cancel** (keep the app
   scheme active; you rarely want to run the widget scheme directly).

Xcode creates `JapanOsintWidgets/JapanOsintWidgets.swift`,
`…Bundle.swift`, `…Control.swift`, `…LiveActivity.swift`, `AppIntent.swift`
and an `Assets.xcassets` inside the existing folder.

**Delete every generated `.swift` file** (Move to Trash — the real
implementations are already there under the same folder), and delete the
generated `JapanOsintWidgetsBundle.swift` if its name collides with ours; ours
is the one containing `struct JapanOsintWidgetsBundle: WidgetBundle`. Keep the
generated `Info.plist` and `Assets.xcassets`.

---

## Step 2 — put the right files in the widget target

Select each file, open the **File inspector** (⌥⌘1) and set **Target
Membership**.

Widget target **only** (`JapanOsintWidgets` ticked, `JapanOsintApp` unticked):

| File |
| --- |
| `JapanOsintWidgets/JapanOsintWidgetsBundle.swift` |
| `JapanOsintWidgets/WidgetSnapshotReader.swift` |
| `JapanOsintWidgets/AlertInboxWidget.swift` |
| `JapanOsintWidgets/QuakeWidget.swift` |
| `JapanOsintWidgets/InboxAccessoryWidget.swift` |
| `JapanOsintWidgets/SearchLiveActivity.swift` |

> These must **never** be in the app target: `ios/JapanOsintApp/` is a
> synchronized root group, so anything moved under it compiles into the app —
> and an `@main struct …: WidgetBundle` inside an app target is a build error.
> That is why the folder sits one level up.

**Both** targets ticked (`JapanOsintApp` *and* `JapanOsintWidgets`) — these two
files live under the synchronized group, so the app membership is automatic and
you are only adding the widget one:

| File | Why |
| --- | --- |
| `JapanOsintApp/Widgets/SharedSnapshot.swift` | `AppGroup`, `WidgetSnapshot`, `WidgetAlert`, `WidgetQuake`, `ShareQueue` — the app writes, the widget reads |
| `JapanOsintApp/Widgets/SearchActivityAttributes.swift` | `SearchActivityAttributes` — the app calls `Activity.request`, the extension renders it |

Leave `JapanOsintApp/Widgets/WidgetSnapshotBuilder.swift` in the **app target
only**. It takes an `API` and does networking; the widget must not.

> **Note on the synchronized group.** Adding a second target membership to a
> file inside a `PBXFileSystemSynchronizedRootGroup` makes Xcode write a
> `PBXFileSystemSynchronizedBuildFileExceptionSet` entry. That's normal and
> correct — the project already has one for `Info.plist`.

---

## Step 3 — create the share extension target

1. **File ▸ New ▸ Target… ▸ iOS ▸ Share Extension**.
2. Product Name: **`JapanOsintShare`** (exact).
3. Embed in Application: **JapanOsintApp**. Team: **YXQHVT52SE**.
4. Delete the generated `ShareViewController.swift` **and** the generated
   `MainInterface.storyboard` — `JapanOsintShare/ShareViewController.swift`
   builds its own UI in code and does not use a storyboard.
5. In the generated `JapanOsintShare/Info.plist`, delete the
   `NSExtensionMainStoryboard` key and replace the whole `NSExtension` dict with
   the one in `JapanOsintShare/Info-activation-snippet.plist`. The template
   ships `TRUEPREDICATE`, which puts JapanOSINT in the share sheet for every
   file type on the system, including ones it will immediately reject.

Target membership:

| File | Targets |
| --- | --- |
| `JapanOsintShare/ShareViewController.swift` | `JapanOsintShare` only |
| `JapanOsintApp/Widgets/SharedSnapshot.swift` | add `JapanOsintShare` (so it is now app + widgets + share) — `ShareQueue.enqueue` lives there |

---

## Step 4 — wire the entitlements files

The three `.entitlements` files already exist with the correct contents. They
are dead only because no target points at them.

For each target: **target ▸ Build Settings ▸ search `CODE_SIGN_ENTITLEMENTS`**
and set it for **both Debug and Release**:

| Target | `CODE_SIGN_ENTITLEMENTS` |
| --- | --- |
| `JapanOsintApp` | `JapanOsintApp/JapanOsintApp.entitlements` |
| `JapanOsintWidgets` | `JapanOsintWidgets/JapanOsintWidgets.entitlements` |
| `JapanOsintShare` | `JapanOsintShare/JapanOsintShare.entitlements` |

(Paths are relative to `ios/`, i.e. the directory holding the `.xcodeproj`.)

Delete any `*.entitlements` file the Xcode templates generated for the two new
targets, so there is exactly one per target and it is the checked-in one.

---

## Step 5 — App Groups capability on all three targets

For **each** of `JapanOsintApp`, `JapanOsintWidgets`, `JapanOsintShare`:

**Signing & Capabilities ▸ + Capability ▸ App Groups**, then tick
`group.com.rayanchl.japanosint`.

If a group appears greyed out, go back to the portal (step 0) and enable App
Groups on that target's App ID — Xcode will have registered
`RCorp.JapanOsintApp.JapanOsintWidgets` and
`RCorp.JapanOsintApp.JapanOsintShare` when it created the targets.

Verify afterwards that each `.entitlements` file still contains exactly:

```xml
<key>com.apple.security.application-groups</key>
<array>
    <string>group.com.rayanchl.japanosint</string>
</array>
```

Xcode sometimes rewrites the file; if it added anything else, revert to the
checked-in version and keep the capability ticked.

---

## Step 6 — Background Modes on the app target

`JapanOsintApp ▸ Signing & Capabilities ▸ + Capability ▸ Background Modes`,
then tick **Background fetch** and **Background processing**.

`Info.plist` already declares the matching
`BGTaskSchedulerPermittedIdentifiers` (`com.rayanchl.japanosint.refresh`,
`com.rayanchl.japanosint.sync`) and `UIBackgroundModes`. Those identifiers must
stay byte-identical to the constants in
`JapanOsintApp/Background/BackgroundRefresh.swift`.

---

## Step 7 — re-enable Live Activities

Only after step 1 has the widget target compiling `SearchLiveActivity.swift`:

1. Add back to `JapanOsintApp/Info.plist`:

   ```xml
   <key>NSSupportsLiveActivities</key>
   <true/>
   ```

   It is required on the **app** target as well as the extension, or
   `Activity.request(...)` throws at runtime.

2. Confirm the same key is present in the widget extension's `Info.plist`.
3. Write the call site — nothing currently calls `Activity.request`, so the
   feature stays inert until something starts an activity when a search run
   begins (`Search/SearchStore.start`) and ends it on the terminal snapshot.
   Live Activities do **not** render in the Simulator; test on device.

---

## Step 8 — verify

1. **Build the app scheme.** It should compile exactly as before; the only
   behavioural change is that `AppGroup.containerURL` is now non-nil.
2. Run on device, sign in, background the app. The console line
   `[AppGroup] "group.com.rayanchl.japanosint" is not available…` must **not**
   appear — if it does, the capability is not actually applied to the running
   binary.
3. Long-press the Home Screen → **+** → search "JapanOSINT". The three widgets
   should be offered and render real numbers within a minute of the app writing
   a snapshot (the app writes on gate-ready, on scene activation, and from the
   background task).
4. Share a URL from Safari → JapanOSINT should appear. Tap it; the app should
   open on the Search tab with that URL as the query
   (`drainShareQueue` → `mapNav.showSearch`). Sharing a PDF should **not**
   offer JapanOSINT — that is the activation rule from step 3 working.

---

## Gotchas that cost the most time

* **`@main` collision.** Two `@main` symbols in one target is a hard error. The
  app has `JapanOsintApp.swift`; the widget bundle has
  `JapanOsintWidgetsBundle.swift`. Keep them in different targets.
* **`SWIFT_DEFAULT_ACTOR_ISOLATION = MainActor`** is set on the app target only.
  Set it on both new targets too, or the shared files (`SharedSnapshot.swift`,
  `SearchActivityAttributes.swift`) get different isolation in each target and
  can fail to compile in one while succeeding in the other.
* **Deployment targets.** Set `IPHONEOS_DEPLOYMENT_TARGET = 26.1` on both new
  targets; Xcode defaults new targets to the current SDK, which may be lower or
  higher than the app's and produces an "embedded binary targets a different
  iOS version" error at embed time.
* **`SUPPORTED_PLATFORMS`.** The app builds for
  `iphoneos iphonesimulator macosx xros xrsimulator`. Extensions created from
  the iOS templates default to iOS only. Leave them iOS-only — but that means
  the **macOS build must keep working with no extensions**, which it does today
  precisely because everything gates on `AppGroup.isConfigured`.
* **Don't move the extension folders under `ios/JapanOsintApp/`.** That
  directory is a synchronized root group; anything inside it is compiled into
  the app automatically, which is exactly the collision this layout avoids.
