# SatelliteTracker — Technical Specification (v1)

**Project path:** `C:/zown/SatelliteTracker`
**Platform:** Windows 10/11 (x64)
**Stack:** C++17/20, Qt 6 Widgets, CMake, vcpkg
**Class of app:** Mission-control style multi-satellite pass predictor with alerts and logging

---

## 1. Overview

SatelliteTracker downloads orbital elements (TLEs) for a user-selected set of satellites, propagates their orbits, and shows upcoming passes over a fixed observer location. The user builds an "active" watchlist from a searchable catalog table; each active satellite gets a live elevation chart showing its next pass. The app raises alerts (AOS/LOS/high-elevation passes) and logs both application events and predicted/actual pass data.

### 1.1 Goals (v1)
- Fetch and cache TLEs live from Celestrak and/or Space-Track.
- Tab 2: full catalog table with checkboxes to mark satellites "active."
- Tab 1: grid of elevation-vs-time charts, one per active satellite, for its next pass.
- Alerting on AOS / LOS / configurable elevation threshold.
- Persistent logging of application events and predicted passes.

### 1.2 Out of scope (v1)
- 3D globe visualization (flat elevation charts only).
- Ground track / world map view.
- Antenna rotator or radio control integration.
- Multi-observer / multi-station support (single observer location for v1).

---

## 2. Application Structure — Two Primary Tabs

### Tab 1 — Pass Grid ("Live Dashboard")
- A `QScrollArea` containing a `QGridLayout` of **pass cards**, one per satellite marked active in Tab 2.
- Each pass card contains:
  - Satellite name + NORAD ID header
  - A `QChart` (Qt Charts) line plot: elevation (°) on Y, time on X, spanning from now through the end of the next pass (or "no pass in next N hours" placeholder state)
  - AOS / TCA (time of closest approach / max elevation) / LOS markers on the chart, each annotated with time and (for TCA) max elevation + azimuth
  - A live "now" marker on the curve when the satellite is currently above the horizon, updating every tick (default 1s)
  - Status chip: `Idle` / `Rising` / `In View` / `Setting` / `No upcoming pass`
- Grid column count configurable in Settings (default: auto-fit based on window width).
- Cards re-flow automatically as satellites are checked/unchecked in Tab 2.
- Double-clicking a card opens a detail dialog with a longer-range table of the satellite's next N passes.

**As built (M3):** AOS/TCA/LOS/max-elevation "markers ... annotated with
time" are rendered as a compact text line under the chart (e.g. `AOS 03:14 ·
TCA 03:19 (62° az 210°) · LOS 03:24`) rather than as in-chart annotations —
same information, simpler and more reliably legible than custom
`QGraphicsItem` label placement. The status chip mapping: `No upcoming pass`
= `PassResult.state == NoPassInWindow`; `Idle` = below horizon, AOS >5 min
away; `Rising` = below horizon, AOS within 5 min; `In View` = above horizon,
at/before TCA (ascending); `Setting` = above horizon, after TCA (descending).
Grid column count is auto-fit only (viewport width ÷ a fixed card min-width)
— the "configurable in Settings" part is deferred to M7, since only the
minimal M2 Observer Location dialog exists so far, not the full Settings
dialog. The double-click detail dialog shows the next 5 passes (not
configurable yet), computed via `PassFinder::findUpcomingPasses`.

### Tab 2 — Satellite Catalog
- `QTableView` backed by a `QAbstractTableModel`, one row per satellite from the loaded TLE set.
- Columns:

| Column | Description |
|---|---|
| ☑ Active | Checkbox — controls whether the satellite appears in Tab 1 |
| Name | Satellite common name |
| NORAD ID | Catalog number |
| Int'l Designator | COSPAR ID |
| Source | `Celestrak` / `Space-Track` / `Local file` |
| Epoch | TLE epoch timestamp (UTC) |
| Inclination (°) | From TLE |
| Period (min) | Derived from mean motion |
| Apogee / Perigee (km) | Derived |
| Next AOS | Computed next pass start for the configured observer, refreshed periodically |

- Toolbar above the table: search/filter box (name or NORAD ID), source/group selector (e.g., Celestrak groups: `active`, `stations`, `weather`, `amateur`, `starlink`, or a Space-Track custom query), **Refresh** button, last-updated timestamp, row count.
- Filter box does incremental filtering via a `QSortFilterProxyModel`.
- Checkbox state persists across TLE refreshes (matched by NORAD ID) and across app restarts.

**As built (M2):** the "Name" column above is the *full* Celestrak name (e.g.
"OSCAR 7 (AO-7)") and exists in the model but is hidden by default; a **Short
Name** column (the familiar designator pulled from the parenthetical, e.g.
"AO-7"; special-cased to "ISS" rather than "ZARYA") takes its place as the
visible name column. Search still matches against the full name too. Next AOS
is computed only for satellites checked active — not the whole catalog, which
can run to thousands of rows for groups like `active` — recomputed every 30s
by `ActiveSatelliteTracker` off the UI thread; unchecked rows show "—".

---

## 3. Alerts

### 3.1 Alert types
- **AOS** — satellite rises above the horizon (elevation crosses 0° ascending).
- **LOS** — satellite sets (elevation crosses 0° descending).
- **High-elevation pass** — max elevation for an upcoming pass exceeds a configurable threshold (e.g., "notify me about passes above 45°").
- **Fetch failure** — TLE download failed (data may be going stale).

### 3.2 Delivery
- Windows system tray notification (`QSystemTrayIcon::showMessage`) — primary channel.
- Optional audible beep (checkbox in Settings).
- In-app **Alerts panel** (dockable `QDockWidget`) — running list of fired alerts with timestamp, satellite, type, and details; supports "acknowledge" and "clear."

### 3.3 Configuration
- Global default rule set, with optional per-satellite overrides (right-click a row in Tab 2 → "Alert settings for this satellite").
- Threshold values, enable/disable per alert type, and quiet hours (e.g., no notifications 23:00–07:00) all configurable in Settings.

---

## 4. Logging

Two distinct logs, both backed by a local SQLite database (`%LOCALAPPDATA%/SatelliteTracker/data.db`):

1. **Application event log** — startup/shutdown, TLE fetch attempts (success/failure/row count), errors/exceptions, settings changes. Viewable in a "Log" dock/tab with level filter (Info/Warning/Error) and export to CSV.
2. **Pass log** — every predicted pass for every satellite that was active at prediction time: satellite, AOS/TCA/LOS timestamps, max elevation, max elevation azimuth, duration. Written when a pass is computed; a separate "actual" flag can later be set if live tracking confirms the satellite was observed in view for the full window (best-effort, based on the app running continuously).

Both logs are queryable/filterable in-app and exportable to CSV. Log retention configurable (default: 90 days, auto-pruned).

---

## 5. Data Sources

### 5.1 Celestrak (no auth required)
```
GET https://celestrak.org/NORAD/elements/gp.php?GROUP=<group>&FORMAT=tle
```
Groups relevant to a general watchlist: `active`, `stations`, `visual`, `weather`, `starlink`, `last-30-days`. Configurable in Settings; multiple groups can be merged (deduplicated by NORAD ID).

### 5.2 Space-Track (auth required)
- Login via `POST https://www.space-track.org/ajaxauth/login` (username/password stored via Windows Credential Manager, not in plaintext config).
- Session cookie reused for subsequent GP queries, e.g.:
```
GET https://www.space-track.org/basicspacedata/query/class/gp/NORAD_CAT_ID/<id>/orderby/EPOCH desc/limit/1/format/tle
```
- Respect Space-Track's rate limits (documented: ~30 requests/min, 300/hr) — all requests funneled through a single rate-limited queue.

### 5.3 Local cache & offline behavior
- Last successfully fetched TLE set cached to SQLite; on startup, app loads from cache immediately, then triggers a background refresh.
- If refresh fails, app continues on cached data and raises a "fetch failure" alert; catalog table shows a "stale" badge with the cache age once it exceeds a configurable threshold (default 24h).
- Refresh interval configurable (default: every 4 hours), plus manual "Refresh" button.

---

## 6. Data Model

```
Satellite
├─ norad_id        INTEGER PK
├─ name             TEXT
├─ intl_designator  TEXT
├─ tle_line1        TEXT
├─ tle_line2        TEXT
├─ epoch            DATETIME
├─ source           TEXT   ('celestrak' | 'space-track' | 'local')
├─ is_active        BOOLEAN
└─ last_updated     DATETIME

Pass
├─ id               INTEGER PK
├─ norad_id         INTEGER FK -> Satellite
├─ aos_time         DATETIME
├─ tca_time         DATETIME
├─ los_time         DATETIME
├─ max_elevation    REAL
├─ max_elev_azimuth REAL
├─ duration_sec     INTEGER
└─ confirmed_actual BOOLEAN

AlertRule
├─ id               INTEGER PK
├─ scope            TEXT   ('global' | norad_id)
├─ alert_type        TEXT   ('AOS' | 'LOS' | 'HIGH_ELEV' | 'FETCH_FAIL')
├─ threshold_value   REAL   (nullable, for HIGH_ELEV)
├─ enabled           BOOLEAN
└─ quiet_hours_range TEXT   (nullable)

AppLogEntry
├─ id        INTEGER PK
├─ timestamp DATETIME
├─ level     TEXT ('INFO'|'WARN'|'ERROR')
└─ message   TEXT

Settings (key/value via QSettings, mirrored to DB for portability)
├─ observer_lat, observer_lon, observer_alt_m
├─ tle_sources[], refresh_interval_min
├─ spacetrack_username (credential ref only, not the secret)
├─ grid_columns, chart_lookahead_hours
└─ alert defaults, log retention days
```

**As built (M2):** `is_active` is now live (Tab 2 checkbox, persisted via
`SatelliteRepository::setSatelliteActive`). The `Settings` block's observer
fields are implemented differently than planned above: the user enters a
**Maidenhead grid locator** (e.g. `KP20`) plus an altitude in meters, not raw
lat/lon directly — lat/lon are derived from the locator on every load, so the
locator stays the single source of truth. Stored as `Observer/GridLocator`
and `Observer/AltitudeMeters` via `QSettings` only (no DB mirror yet — not
needed until something other than this one dialog needs to read it). `Pass`,
`AlertRule`, and `AppLogEntry` remain unimplemented, on schedule for M4/M5.

---

## 7. Architecture

### 7.1 Module breakdown

Target layout (per the phased plan below, module contents grow into this as later
milestones land — as of M1, none of `Orbit/`, `AlertEngine/`, `workers/`, the docks, or
`SettingsDialog` existed yet; see "Current layout (M2 additions)" below for what M2 added):
```
SatelliteTracker/
├─ CMakeLists.txt
├─ src/
│  ├─ main.cpp
│  ├─ core/
│  │  ├─ Orbit/            # SGP4 propagation wrapper, pass-finding algorithm (M2+)
│  │  ├─ TleFetch/         # Celestrak + Space-Track clients, rate limiter
│  │  └─ AlertEngine/      # rule evaluation, notification dispatch (M4+)
│  ├─ data/
│  │  ├─ Database/         # SQLite schema + migrations (QtSql)
│  │  ├─ SatelliteModel.*  # QAbstractTableModel for Tab 2
│  │  └─ Repository/       # DAO layer between core and DB
│  ├─ ui/
│  │  ├─ MainWindow.*
│  │  ├─ PassGridWidget.*  # Tab 1 container + card layout (M3+)
│  │  ├─ PassCard.*        # individual chart card (M3+)
│  │  ├─ CatalogTableView.*# Tab 2
│  │  ├─ AlertsDock.*      # M4+
│  │  ├─ LogDock.*         # M5+
│  │  └─ SettingsDialog.*  # M7+
│  └─ workers/
│     ├─ TleFetchWorker.*  # runs on QThreadPool, emits results via signal (M2+)
│     └─ PropagationWorker.* # M2+
├─ resources/
│  └─ icons, .qrc
├─ tests/
│  └─ (Qt Test / Catch2 unit tests for orbit math, TLE parsing, pass finder)
└─ third_party/ (or resolved via vcpkg)
```

**Current layout (M1, as built):** flatter than the target above — `core/` holds
`TleParser.*` and `CelestrakClient.*` directly (no `TleFetch/` subfolder yet, no
Space-Track client, no rate limiter); `data/` holds `Satellite.h`, `Database.*`,
`SatelliteRepository.*`, and `SatelliteModel.*` directly (no `Database/` or
`Repository/` subfolders); `ui/` holds only `MainWindow.*`. See `README.md` for the
build-verified file list.

**Current layout (M2 additions):** `core/Orbit/` now exists as planned —
`IOrbitPropagator.h` (interface), `Sgp4OrbitPropagator.*` (wraps the vendored
library), `PassFinder.*` (AOS/TCA/LOS via coarse sampling + bisection). No
separate `workers/PropagationWorker.*` was built, though — active-satellite
recomputation lives in `core/ActiveSatelliteTracker.*`, which deliberately
reuses the `QtConcurrent::run` + `QFutureWatcher` pattern `MainWindow`
already used for TLE parsing, rather than introducing a second worker
architecture. Also new: `core/Maidenhead.*` (grid locator ↔ lat/lon),
`core/AppSettings.*` + `core/ObserverLocation.h` (the first `QSettings`
usage), `ui/ObserverLocationDialog.*` (a minimal preview of the eventual M7
`SettingsDialog`, scoped to just the observer location), and
`third_party/sgp4/` — a vendored copy of `dnwrnr/sgp4` (Apache-2.0, pinned
commit), built as its own static library target, since no SGP4 package
exists for the MinGW/MSYS2 toolchain this project builds with (see §7.2/§9).
`tests/orbit_tests.cpp` is no longer aspirational either — a small
assert-and-exit-code executable wired via `enable_testing()`/`add_test()`,
validating the Maidenhead formula and pass-finder invariants against real
reference data (no new test-framework dependency).

**Current layout (M3 additions):** `ui/PassCard.*`, `ui/PassGridWidget.*`,
and `ui/PassDetailDialog.*` now exist as planned. One addition not in the
original target layout: `core/SatelliteNaming.*`, a small shared utility
(short-designator extraction + the FM/Linear mode lookup) pulled out of
`SatelliteModel.cpp` once `PassCard`'s header needed the same logic, rather
than duplicating it. **Known gap:** `tests/orbit_tests.cpp` wasn't extended
to cover the M3-added `PassResult::curve` sampling or
`findUpcomingPasses()` — both are exercised indirectly (curve data flows
through `PassCard`'s chart, verified by a manual smoke test) but have no
dedicated assertions yet.

### 7.2 Key libraries
- **Qt 6 Widgets + Qt Charts** — UI and elevation plots.
- **QtSql (SQLite driver)** — local persistence.
- **QNetworkAccessManager** — HTTPS TLE downloads (Celestrak/Space-Track).
- **SGP4 propagation** — vendored as source (`third_party/sgp4/`, from `dnwrnr/sgp4`, Apache-2.0, pinned commit), not pulled via vcpkg, since the active toolchain is MinGW/MSYS2 (see §9) and no SGP4 package exists for it; wrapped behind an internal `IOrbitPropagator` interface (`Sgp4OrbitPropagator`) so the library can be swapped without touching call sites.
- **Windows Credential Manager (via `wincred.h` or a thin Qt wrapper)** — secure storage for Space-Track credentials.

### 7.3 Threading model
- UI thread stays free of network I/O and propagation math.
- TLE fetches run on a dedicated worker (`QThreadPool` task); results parsed off-thread and merged into the DB, then the model is refreshed on the UI thread via a queued signal.
- Pass prediction for all active satellites runs on a periodic timer (default every 30s) on a worker thread; each `PassCard` is updated via signal/slot with the freshly computed pass window.
- Alert evaluation happens right after each propagation pass, also off the UI thread; only notification dispatch touches the UI/tray (queued to main thread).

**As built (M3):** the 30s recompute is `ActiveSatelliteTracker`'s existing
`QtConcurrent::run` + `QFutureWatcher` job (added in M2), whose
`passesUpdated` signal now also reaches `PassGridWidget`/`PassCard` — no
second worker was introduced. The chart's live "now" marker and status chip
tick every 1s on the UI thread directly (each `PassCard` owns its own
`Sgp4OrbitPropagator`, reloaded only when its TLE changes): a single
propagation per active card per second is cheap enough (well within the §8
NFR's 50-satellite target) that hopping to a worker thread for it wasn't
worth the complexity.

---

## 8. Non-Functional Requirements

- **Performance:** support at least 50 simultaneously active satellites in the pass grid without UI stutter (propagation batched and throttled; charts redraw only their changed portion, not full rebuild, when possible).
- **Resilience:** app must remain usable fully offline once TLEs are cached; network failures degrade gracefully (stale badge + alert, not a crash).
- **Time handling:** all internal computation and storage in UTC; UI displays configurable local time or UTC.
- **Config storage:** `QSettings` (registry-backed on Windows) for lightweight preferences; SQLite for satellite/pass/log data.
- **Secrets:** Space-Track password never stored in `QSettings` or the SQLite DB — Windows Credential Manager only.
- **Packaging:** `windeployqt`-based deployment; installer via Inno Setup or NSIS; app data under `%LOCALAPPDATA%/SatelliteTracker/`.

---

## 9. Build & Tooling

- **Build system:** CMake ≥ 3.25 (currently CMake 3.30 in use), C++20, Ninja generator.
- **Toolchain (current, M1):** MinGW-w64 via MSYS2 (`MSYS2 MINGW64` shell), using the
  prebuilt `mingw-w64-x86_64-qt6-base` package (Widgets, Network, Sql w/ SQLite driver,
  Concurrent) — chosen to avoid building Qt6 from source under vcpkg. See `README.md` for
  the exact `pacman`/`cmake` steps.
- **Toolchain (alternative):** MSVC (Visual Studio 2022 toolset) + vcpkg manifest mode was
  considered for Qt6, sqlite3, and the SGP4 library, but never set up in the tree — no
  `vcpkg.json` exists here. A stray top-level `build/` directory from an earlier, abandoned
  attempt at this (pointing at an MSVC `cl.exe`, with a broken `vcpkg-manifest-install.log`)
  is left over but unused; the real build lives in `build/Desktop_Qt_6_9_0_MinGW_64_bit-Debug/`
  (Qt Creator's MinGW/Ninja kit). MinGW/MSYS2 is the only active build path through M3; the
  two toolchains aren't mixed in one build tree. SGP4 is vendored as source instead (§7.2).
  Qt Charts (M3) is a separate MSYS2 package, `mingw-w64-x86_64-qt6-charts` — not bundled
  with `qt6-base` — see `README.md` for the updated prerequisites.
- **Packaging note:** `windeployqt` is used post-build to gather MinGW Qt DLLs/plugins
  next to the `.exe` for standalone runs outside the MSYS2 shell.
- **CI (optional but recommended):** GitHub Actions Windows runner — configure, build, run unit tests (orbit math + TLE parsing are the highest-value things to test), and produce a packaged artifact.

---

## 10. Phased Delivery Plan

| Milestone | Deliverable | Status |
|---|---|---|
| M1 | TLE fetch (Celestrak first) + parsing + SQLite cache; Tab 2 catalog table (no checkboxes yet) | **Done** — see `README.md` |
| M2 | SGP4 propagation + next-pass finder for a fixed observer; Tab 2 checkboxes wired to an "active satellites" list | **Done** — vendored SGP4 (§7.2), Maidenhead-locator observer settings (§6), Next AOS scoped to active satellites only (§2) |
| M3 | Tab 1 pass grid with live elevation charts for active satellites | **Done** — Qt Charts linked, status-chip mapping + simplified annotations (§2), no dedicated tests for the new curve/multi-pass code yet (§7.1) |
| M4 | Alert engine (AOS/LOS/threshold) + tray notifications + Alerts dock | Not started |
| M5 | Pass log + app event log + Log dock + CSV export | Not started |
| M6 | Space-Track integration (credential storage, auth, rate-limited client) | Not started |
| M7 | Settings dialog (observer location, sources, refresh interval, alert config, retention); packaging/installer | Not started |

---

## 11. Open Questions / Assumptions to Confirm

- ~~Observer location: assumed fixed and set once in Settings — confirm no need for "current GPS location" auto-detection.~~ **Resolved in M2:** no GPS auto-detection; fixed, set once via `ObserverLocationDialog` (Settings → Observer Location…). Entered as a Maidenhead grid locator rather than raw lat/lon (ham-radio-friendly and less error-prone to type than decimal coordinates), plus an altitude in meters.
- Space-Track query scope: assumed on-demand per-satellite queries against the GP class; confirm if a full bulk catalog download (like Celestrak) is preferred instead to reduce request count.
- ~~Chart lookahead: assumed each Tab 1 card shows only the *next* pass; confirm whether a secondary "next 3 passes" mini-list per card is wanted in v1 or can wait.~~ **Resolved in M3:** the card itself still shows only the next pass, as assumed; the "several upcoming passes" need is instead met by the double-click detail dialog (next 5 passes), keeping the card uncluttered.
- No map/globe in v1 — confirm this is acceptable, since it's a common expectation for "satellite tracker" apps and could be a fast-follow (v1.1) using the existing propagation core.
