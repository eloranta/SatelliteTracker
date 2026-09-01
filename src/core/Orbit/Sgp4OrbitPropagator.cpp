#include "Sgp4OrbitPropagator.h"

#include <exception>

#include "CoordTopocentric.h"
#include "DateTime.h"
#include "Eci.h"
#include "Observer.h"
#include "SGP4.h"
#include "Tle.h"
#include "Util.h"

namespace SatelliteTracker {

Sgp4OrbitPropagator::Sgp4OrbitPropagator() = default;
Sgp4OrbitPropagator::~Sgp4OrbitPropagator() = default;

bool Sgp4OrbitPropagator::loadTle(const QString &line1, const QString &line2, QString *errorOut)
{
    try {
        const libsgp4::Tle tle(line1.toStdString(), line2.toStdString());
        m_sgp4 = std::make_unique<libsgp4::SGP4>(tle);
        return true;
    } catch (const std::exception &ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        m_sgp4.reset();
        return false;
    }
}

LookAngle Sgp4OrbitPropagator::computeLookAngle(const QDateTime &utcInstant,
                                                 double observerLatDeg, double observerLonDeg,
                                                 double observerAltMeters) const
{
    LookAngle result;
    if (!m_sgp4) {
        return result;
    }

    try {
        const QDateTime utc = utcInstant.toUTC();
        const QDate date = utc.date();
        const QTime time = utc.time();
        const libsgp4::DateTime when(date.year(), date.month(), date.day(),
                                      time.hour(), time.minute(), time.second(),
                                      time.msec() * 1000);

        const libsgp4::Eci eci = m_sgp4->FindPosition(when);
        libsgp4::Observer observer(observerLatDeg, observerLonDeg, observerAltMeters / 1000.0);
        const libsgp4::CoordTopocentric topo = observer.GetLookAngle(eci);

        result.azimuthDeg = libsgp4::Util::RadiansToDegrees(topo.azimuth);
        result.elevationDeg = libsgp4::Util::RadiansToDegrees(topo.elevation);
        result.rangeKm = topo.range;
        result.valid = true;
    } catch (const std::exception &) {
        result.valid = false;
    }

    return result;
}

} // namespace SatelliteTracker
