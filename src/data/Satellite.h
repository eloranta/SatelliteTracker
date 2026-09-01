#pragma once

#include <QString>
#include <QDateTime>

namespace SatelliteTracker {

// Plain data record for one satellite: raw TLE fields plus values
// derived from them at parse time (inclination, period, apogee/perigee).
struct Satellite {
    int noradId = 0;
    QString name;
    QString intlDesignator;
    QString tleLine1;
    QString tleLine2;
    QDateTime epochUtc;
    QString source;          // "celestrak" | "space-track" | "local"
    bool isActive = false;   // reserved for M2 (Tab 1 watchlist); not exposed in Tab 2 UI yet
    QDateTime lastUpdatedUtc;

    // Derived orbital quantities (computed by TleParser)
    double inclinationDeg = 0.0;
    double eccentricity = 0.0;
    double meanMotionRevPerDay = 0.0;
    double periodMinutes = 0.0;
    double apogeeKm = 0.0;
    double perigeeKm = 0.0;
};

} // namespace SatelliteTracker
