#pragma once

#include <QDateTime>
#include <QString>

namespace SatelliteTracker {

// Topocentric look angle from an observer to a satellite at one instant.
struct LookAngle {
    double azimuthDeg = 0.0;
    double elevationDeg = 0.0;
    double rangeKm = 0.0;
    bool valid = false; // false if propagation failed (e.g. bad/decayed TLE)
};

// Wraps a concrete SGP4/SDP4 implementation so the rest of the app never
// touches vendored library types directly (see Sgp4OrbitPropagator).
class IOrbitPropagator {
public:
    virtual ~IOrbitPropagator() = default;

    virtual bool loadTle(const QString &line1, const QString &line2, QString *errorOut = nullptr) = 0;

    virtual LookAngle computeLookAngle(const QDateTime &utcInstant,
                                        double observerLatDeg, double observerLonDeg,
                                        double observerAltMeters) const = 0;
};

} // namespace SatelliteTracker
