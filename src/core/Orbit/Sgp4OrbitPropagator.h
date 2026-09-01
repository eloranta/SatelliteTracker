#pragma once

#include <memory>

#include "IOrbitPropagator.h"

namespace libsgp4 {
class SGP4;
}

namespace SatelliteTracker {

// SGP4/SDP4 propagator backed by the vendored dnwrnr/sgp4 library
// (third_party/sgp4). libsgp4 types never appear outside this header/.cpp.
class Sgp4OrbitPropagator : public IOrbitPropagator {
public:
    Sgp4OrbitPropagator();
    ~Sgp4OrbitPropagator() override;

    bool loadTle(const QString &line1, const QString &line2, QString *errorOut = nullptr) override;

    LookAngle computeLookAngle(const QDateTime &utcInstant,
                                double observerLatDeg, double observerLonDeg,
                                double observerAltMeters) const override;

private:
    std::unique_ptr<libsgp4::SGP4> m_sgp4;
};

} // namespace SatelliteTracker
