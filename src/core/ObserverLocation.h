#pragma once

#include <QString>

namespace SatelliteTracker {

// A fixed observer position for pass prediction. Derived from a Maidenhead
// grid locator (see Maidenhead.h) plus a user-entered altitude.
struct ObserverLocation {
    QString gridLocator;
    double latitudeDeg = 0.0;
    double longitudeDeg = 0.0;
    double altitudeMeters = 0.0;
    bool isConfigured = false;
};

} // namespace SatelliteTracker
