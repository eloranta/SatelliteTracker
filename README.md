# SatelliteTracker — M1 Build (MinGW-w64 / MSYS2)

Implements Milestone 1 from `SatelliteTracker.md`:
- Live TLE fetch from Celestrak (`GROUP`/`FORMAT=tle` endpoint), async, no auth
- TLE parsing into `Satellite` records, with derived inclination/period/apogee/perigee
- SQLite cache (upsert-by-NORAD-ID) under `%LOCALAPPDATA%/SatelliteTracker/data.db`
- Tab 2 "Satellite Catalog": searchable/sortable table, group selector, Refresh button,
  last-updated + row-count status
- Tab 1 is a placeholder — the pass grid ships in M3

Not yet implemented (later milestones per the spec): Tab 2 checkboxes / Tab 1 watchlist (M2),
elevation charts (M3), alerts (M4), pass/app logging (M5), Space-Track auth (M6), Settings
dialog + installer (M7).

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
  mingw-w64-x86_64-qt6-base
```

`mingw-w64-x86_64-qt6-base` includes Widgets, Network, Sql (with the SQLite driver
bundled), and Concurrent — everything M1 needs.

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
background fetch of the "Active satellites" Celestrak group. Switch groups via the
dropdown and hit **Refresh** to pull a different set; fetched satellites upsert into the
same cache, so switching groups is additive, not destructive.

## Alternative: vcpkg (MSVC)

`vcpkg.json` is still included if you'd rather build with MSVC + vcpkg instead of
MinGW/MSYS2 — see git history or ask for those instructions again. The two toolchains
aren't mixed in one build tree; pick one.

## Project layout

```
SatelliteTracker/
├─ CMakeLists.txt
├─ vcpkg.json        # only relevant to the MSVC/vcpkg alternative above
├─ src/
│  ├─ main.cpp
│  ├─ data/         # Satellite struct, SQLite Database + Repository, SatelliteModel (Tab 2)
│  ├─ core/          # TleParser (TLE -> Satellite), CelestrakClient (async fetch)
│  └─ ui/             # MainWindow (tabs, catalog table, toolbar wiring)
```
