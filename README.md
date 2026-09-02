# SatelliteTracker — M1 + M2 + M3 Build (MinGW-w64 / MSYS2)

Implements Milestones 1–3 from `SatelliteTracker.md`:
- Live TLE fetch from Celestrak (`GROUP`/`FORMAT=tle` endpoint), async, no auth
- TLE parsing into `Satellite` records, with derived inclination/period/apogee/perigee
- SQLite cache (upsert-by-NORAD-ID) under `%LOCALAPPDATA%/SatelliteTracker/data.db`, tagged
  per Celestrak group so the catalog view can be scoped to one group at a time
- Tab 2 "Satellite Catalog": searchable/sortable table, group selector, Refresh button,
  last-updated + row-count status, auto-refresh (24h) with a 5-minute retry on failure
- Tab 2 **Active** checkbox column, wired to a persisted watchlist (`is_active`), plus a
  **Next AOS** column computed for checked satellites only, refreshed every 30s
- SGP4/SDP4 propagation (vendored `third_party/sgp4`) + a next-pass finder (AOS/TCA/LOS)
  for a fixed observer, entered as a Maidenhead grid locator via **Settings → Observer
  Location…**
- A **Name** column showing the familiar short designator (e.g. `AO-7`, `ISS`) instead of
  the full Celestrak name; the full name is still in the model and searchable, just hidden.
  A **Mode** column tags known satellites FM/Linear (a small static lookup, blank if unknown).
  Catalog table sorts by Name by default (click any header to resort)
- Tab 1 "Pass Grid": always tries to fill 12 cards. Every active satellite contributes its
  next 12 passes (including one already in progress) to a shared pool; the pool is sorted
  chronologically by AOS and only the soonest 12 across the whole watchlist get cards — a
  satellite with several near-term passes can occupy multiple slots, so it's not a fixed
  one-card-per-satellite grid. Each card: an elevation-vs-time chart (Qt
  Charts) whose area fill is red before AOS and green from AOS onward, a dashed vertical
  "now" cursor between AOS and LOS, a "now" marker + status chip
  (`Idle`/`Rising`/`In View`/`Setting`/`No upcoming pass`) updated every 1s, a header showing
  the satellite name + AOS in local time (`Today HH:mm:ss`), and an AOS/TCA/LOS/max-elevation
  summary line. A card hides itself once its own LOS passes; the next ~30s recompute cycle
  replaces the whole set. Double-click a card for that satellite's next-5-passes table

Not yet implemented (later milestones per the spec): alerts (M4), pass/app logging (M5),
Space-Track auth (M6), full Settings dialog + installer (M7 — M2 ships only a minimal
Observer Location dialog, a preview of it).

## Prerequisites (MSYS2 MinGW64)

This project targets the MinGW-w64 toolchain via [MSYS2](https://www.msys2.org/), which is
the simplest way to get a prebuilt Qt6 that matches a MinGW compiler (no separate Qt
installer, no vcpkg Qt build from source).

1. Install MSYS2 (default location `C:\msys64`).
2. Open the **"MSYS2 MINGW64"** shell (not the plain MSYS2 shell — toolchain and Qt
   packages differ per subsystem) and install:

```bash
pacman -Syu                 # update once, may ask you to restart the shell and re-run
pacman -S --needed \
  mingw-w64-x86_64-toolchain \
  mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-ninja \
  mingw-w64-x86_64-qt6-base \
  mingw-w64-x86_64-qt6-charts
```

`mingw-w64-x86_64-qt6-base` includes Widgets, Network, Sql (with the SQLite driver
bundled), and Concurrent — everything M1+M2 need. **`mingw-w64-x86_64-qt6-charts` is a
separate package** (confirmed via packages.msys2.org — Qt Charts isn't bundled into
`qt6-base`), needed for M3's Tab 1 elevation charts. No extra package is needed for SGP4 —
it's vendored as source under `third_party/sgp4/` (see `SatelliteTracker.md` §7.2/§9) and
builds automatically as part of the CMake tree.

## Build

From the **MSYS2 MINGW64** shell, `cd` to the project folder (MSYS2 mounts Windows drives
under `/c/...`):

```bash
cd /c/ZOWN/SatelliteTracker
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Or, using the included preset:

```bash
cmake --preset mingw64
cmake --build --preset mingw64
```

CMake will find Qt6 automatically because the MINGW64 shell's `PATH`/`CMAKE_PREFIX_PATH`
already points at `/mingw64` where the packages installed. If it can't find Qt6, pass it
explicitly:

```bash
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/msys64/mingw64
```

### Tests

`tests/orbit_tests.cpp` validates the Maidenhead grid-locator conversion, the pass finder's
AOS/TCA/LOS invariants against real reference data (a real SGP4 verification TLE and a
well-known ham radio grid square), and the `CurrentlyInView` backward-AOS-search fix (a
search starting from a pass's TCA correctly recovers the same true AOS/LOS as the original
upcoming-pass search) — no test-framework dependency, just asserts and an exit code, wired
into CTest. Note: `findUpcomingPasses` and the chart's curve sampling aren't covered by
dedicated assertions yet, only exercised indirectly through the UI.

```bash
cmake --build build --target orbit_tests
ctest --test-dir build --output-on-failure
```

## Run

Either from the MINGW64 shell (`./build/SatelliteTracker.exe`) or by double-clicking the
`.exe` from plain Windows Explorer — the latter needs the MinGW Qt DLLs on `PATH` or
deployed alongside the executable:

```bash
# From the MINGW64 shell, deploy Qt DLLs + plugins next to the exe for standalone use:
windeployqt build/SatelliteTracker.exe
```

On first run the app creates its local cache at
`%LOCALAPPDATA%\SatelliteTracker\data.db`, loads it (empty), and immediately triggers a
background fetch of the "Amateur Radio" Celestrak group (the default in the group
dropdown). Switch groups via the dropdown and hit **Refresh** to pull a different set;
fetched satellites upsert into the same cache tagged by group, so switching groups is
additive, not destructive, and the catalog view is scoped to whichever group is selected.

To see **Next AOS** populate (Tab 2) and cards appear in **Tab 1**, set your station's
location once via **Settings → Observer Location…** (a Maidenhead grid locator, e.g.
`KP20`, plus an optional altitude), then check a satellite's **Active** box in the catalog
— Tab 1 recomputes within one 30s tick. Whether it actually gets a card depends on where its
next few passes fall relative to everyone else's: with few satellites active, one with
several near-term passes can fill most of the grid on its own; with many active, the grid
just shows whichever 12 passes (across everyone's next 12 each) are soonest.

## Note on toolchains

MSVC + vcpkg was considered as an alternative build path but never actually set up — there
is no `vcpkg.json` in this tree. A leftover, broken top-level `build/` directory from an
abandoned attempt at this exists but is unused; ignore it. MinGW/MSYS2 (below) is the only
supported toolchain, and SGP4 is vendored as source rather than resolved via a package
manager (see `SatelliteTracker.md` §9 for details).

## Project layout

```
SatelliteTracker/
├─ CMakeLists.txt
├─ third_party/
│  └─ sgp4/          # vendored dnwrnr/sgp4 (Apache-2.0), built as a static lib target
├─ tests/
│  └─ orbit_tests.cpp # Maidenhead + pass-finder checks, wired into CTest
├─ src/
│  ├─ main.cpp
│  ├─ data/          # Satellite struct, SQLite Database + Repository, SatelliteModel (Tab 2)
│  ├─ core/           # TleParser, CelestrakClient, Maidenhead, SatelliteNaming, AppSettings,
│  │                  # ObserverLocation, ActiveSatelliteTracker, and Orbit/ (IOrbitPropagator,
│  │                  # Sgp4OrbitPropagator, PassFinder)
│  └─ ui/              # MainWindow (tabs, catalog table, toolbar/menu wiring),
│                       # ObserverLocationDialog, PassGridWidget, PassCard, PassDetailDialog
```
