#pragma once

#include <QString>

#include "ObserverLocation.h"

namespace SatelliteTracker::AppSettings {

// Loads the saved observer location. Lat/lon are re-derived from the stored
// grid locator every time (not stored directly), so the locator is always
// the single source of truth. Returns a default-constructed
// ObserverLocation (isConfigured == false) if none has been saved yet, or
// the stored locator is no longer valid.
ObserverLocation loadObserverLocation();

// Validates `loc.gridLocator` and, if valid, saves the locator + altitude.
// Returns false (with *errorOut set) if the locator is invalid.
bool saveObserverLocation(const ObserverLocation &loc, QString *errorOut = nullptr);

} // namespace SatelliteTracker::AppSettings
