#include "AppSettings.h"

#include <QSettings>

#include "Maidenhead.h"

namespace SatelliteTracker::AppSettings {

namespace {
constexpr auto kGridLocatorKey = "Observer/GridLocator";
constexpr auto kAltitudeMetersKey = "Observer/AltitudeMeters";
}

ObserverLocation loadObserverLocation()
{
    ObserverLocation loc;

    QSettings settings;
    const QString locator = settings.value(QLatin1String(kGridLocatorKey)).toString();
    if (locator.isEmpty()) {
        return loc; // isConfigured stays false
    }

    Maidenhead::GeoCoordinate coord;
    if (!Maidenhead::locatorToLatLon(locator, &coord)) {
        return loc; // stored locator is stale/invalid; treat as unconfigured
    }

    loc.gridLocator = locator;
    loc.latitudeDeg = coord.latitudeDeg;
    loc.longitudeDeg = coord.longitudeDeg;
    loc.altitudeMeters = settings.value(QLatin1String(kAltitudeMetersKey), 0.0).toDouble();
    loc.isConfigured = true;
    return loc;
}

bool saveObserverLocation(const ObserverLocation &loc, QString *errorOut)
{
    if (!Maidenhead::isValidLocator(loc.gridLocator, errorOut)) {
        return false;
    }

    QSettings settings;
    settings.setValue(QLatin1String(kGridLocatorKey), loc.gridLocator);
    settings.setValue(QLatin1String(kAltitudeMetersKey), loc.altitudeMeters);
    return true;
}

} // namespace SatelliteTracker::AppSettings
