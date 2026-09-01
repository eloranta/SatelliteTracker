#pragma once

#include <QString>

namespace SatelliteTracker::Maidenhead {

// Center of the resulting grid square (4-char) or subsquare (6-char).
struct GeoCoordinate {
    double latitudeDeg = 0.0;
    double longitudeDeg = 0.0;
};

// Validates a Maidenhead grid locator: 4 or 6 characters, field A-R, square
// 0-9, optional subsquare A-X (case-insensitive throughout).
bool isValidLocator(const QString &locator, QString *errorOut = nullptr);

// Converts a valid locator to the lat/lon at the center of its grid square
// (or subsquare, for 6-char locators). Returns false (with *errorOut set) if
// the locator fails isValidLocator().
bool locatorToLatLon(const QString &locator, GeoCoordinate *outCoord, QString *errorOut = nullptr);

} // namespace SatelliteTracker::Maidenhead
