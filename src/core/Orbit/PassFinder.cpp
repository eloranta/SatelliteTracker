#include "PassFinder.h"

namespace SatelliteTracker {

namespace {

// Bound on how far back to search for a CurrentlyInView pass's true AOS.
// Generous for any real LEO/MEO amateur pass (minutes to tens of minutes);
// bounded so a satellite that's continuously above the horizon (e.g. a
// near-geostationary bird) doesn't turn into an unbounded backward scan.
constexpr int kBackwardAosSearchHours = 6;

double elevationDegAt(const IOrbitPropagator &propagator, const QDateTime &t,
                       double lat, double lon, double alt)
{
    const LookAngle look = propagator.computeLookAngle(t, lat, lon, alt);
    // Treat a failed propagation as "far below the horizon" so it never
    // reads as a rise/set crossing.
    return look.valid ? look.elevationDeg : -90.0;
}

// Binary-searches for the instant elevation crosses zero between `lo` and
// `hi`. Assumes elevation(lo) and elevation(hi) already have opposite signs
// appropriate for the crossing direction being searched.
QDateTime bisectZeroCrossing(const IOrbitPropagator &propagator, double lat, double lon, double alt,
                              QDateTime lo, QDateTime hi, bool rising)
{
    for (int iter = 0; iter < 24; ++iter) {
        const QDateTime mid = lo.addMSecs(lo.msecsTo(hi) / 2);
        const double elevAtMid = elevationDegAt(propagator, mid, lat, lon, alt);
        const bool midIsBelow = elevAtMid <= 0.0;
        if (rising) {
            if (midIsBelow) lo = mid; else hi = mid;
        } else {
            if (midIsBelow) hi = mid; else lo = mid;
        }
    }
    return lo.addMSecs(lo.msecsTo(hi) / 2);
}

// Ternary-searches for the elevation maximum in a neighborhood of
// `approxPeak`, assuming elevation is unimodal (rises then falls) across
// the window — true for a single pass's TCA away from the AOS/LOS edges.
QDateTime refineTca(const IOrbitPropagator &propagator, double lat, double lon, double alt,
                     const QDateTime &approxPeak, qint64 halfWindowMs)
{
    QDateTime lo = approxPeak.addMSecs(-halfWindowMs);
    QDateTime hi = approxPeak.addMSecs(halfWindowMs);
    for (int iter = 0; iter < 30; ++iter) {
        const qint64 span = lo.msecsTo(hi);
        const QDateTime m1 = lo.addMSecs(span / 3);
        const QDateTime m2 = lo.addMSecs(2 * span / 3);
        if (elevationDegAt(propagator, m1, lat, lon, alt) < elevationDegAt(propagator, m2, lat, lon, alt)) {
            lo = m1;
        } else {
            hi = m2;
        }
    }
    return lo.addMSecs(lo.msecsTo(hi) / 2);
}

} // namespace

PassResult findNextPass(const IOrbitPropagator &propagator,
                         const QDateTime &fromUtc,
                         double observerLatDeg, double observerLonDeg, double observerAltMeters,
                         int lookaheadHours,
                         int coarseStepSeconds)
{
    PassResult result;

    const QDateTime windowEnd = fromUtc.addSecs(qint64(lookaheadHours) * 3600);
    const double lat = observerLatDeg;
    const double lon = observerLonDeg;
    const double alt = observerAltMeters;

    QDateTime prevT = fromUtc;
    double prevElev = elevationDegAt(propagator, prevT, lat, lon, alt);

    QDateTime aosUtc;
    bool haveAos = false;

    if (prevElev > 0.0) {
        result.state = PassState::CurrentlyInView;
        haveAos = true;

        // SGP4 propagates to past instants just as validly as future ones,
        // so the true AOS is findable by searching backward from fromUtc --
        // no need to settle for "now" as a stand-in, which would otherwise
        // clip the chart to only the not-yet-elapsed remainder of the pass.
        aosUtc = fromUtc;
        const QDateTime backLimit = fromUtc.addSecs(-qint64(kBackwardAosSearchHours) * 3600);
        QDateTime laterT = fromUtc;
        for (QDateTime t = fromUtc.addSecs(-coarseStepSeconds); t >= backLimit; t = t.addSecs(-coarseStepSeconds)) {
            const double elev = elevationDegAt(propagator, t, lat, lon, alt);
            if (elev <= 0.0) {
                aosUtc = bisectZeroCrossing(propagator, lat, lon, alt, t, laterT, /*rising=*/true);
                break;
            }
            laterT = t;
        }
    }

    // Scan forward for AOS (if not already in view) and then LOS.
    for (QDateTime t = fromUtc.addSecs(coarseStepSeconds); t <= windowEnd; t = t.addSecs(coarseStepSeconds)) {
        const double elev = elevationDegAt(propagator, t, lat, lon, alt);

        if (!haveAos) {
            if (prevElev <= 0.0 && elev > 0.0) {
                aosUtc = bisectZeroCrossing(propagator, lat, lon, alt, prevT, t, /*rising=*/true);
                haveAos = true;
                result.state = PassState::UpcomingPass;
            }
        } else {
            if (prevElev > 0.0 && elev <= 0.0) {
                result.losUtc = bisectZeroCrossing(propagator, lat, lon, alt, prevT, t, /*rising=*/false);
                break;
            }
        }

        prevT = t;
        prevElev = elev;
    }

    if (!haveAos) {
        return result; // NoPassInWindow
    }

    result.aosUtc = aosUtc;

    // Find the coarse sample with the highest elevation between AOS and
    // (LOS if found, else the end of the scanned window) to seed TCA
    // refinement.
    const QDateTime tcaScanEnd = result.losUtc.isValid() ? result.losUtc : prevT;
    QDateTime bestT = aosUtc;
    double bestElev = elevationDegAt(propagator, aosUtc, lat, lon, alt);
    for (QDateTime t = aosUtc; t <= tcaScanEnd; t = t.addSecs(coarseStepSeconds)) {
        const double elev = elevationDegAt(propagator, t, lat, lon, alt);
        if (elev > bestElev) {
            bestElev = elev;
            bestT = t;
        }
    }

    result.tcaUtc = refineTca(propagator, lat, lon, alt, bestT, qint64(coarseStepSeconds) * 1000);
    const LookAngle tcaLook = propagator.computeLookAngle(result.tcaUtc, lat, lon, alt);
    result.maxElevationDeg = tcaLook.valid ? tcaLook.elevationDeg : bestElev;
    result.maxElevAzimuthDeg = tcaLook.valid ? tcaLook.azimuthDeg : 0.0;

    // Sample the curve for charting. Adaptive step so a long or unbounded
    // (e.g. losUtc not found within the window) span still caps out around
    // ~180 points rather than growing unbounded.
    const QDateTime curveEnd = result.losUtc.isValid() ? result.losUtc : tcaScanEnd;
    const qint64 spanSeconds = aosUtc.secsTo(curveEnd);
    const int curveStepSeconds = qBound(5, int(spanSeconds / 180), coarseStepSeconds);
    for (QDateTime t = aosUtc; t <= curveEnd; t = t.addSecs(curveStepSeconds)) {
        result.curve.append({t, elevationDegAt(propagator, t, lat, lon, alt)});
    }

    return result;
}

QVector<PassResult> findUpcomingPasses(const IOrbitPropagator &propagator,
                                        const QDateTime &fromUtc,
                                        double observerLatDeg, double observerLonDeg, double observerAltMeters,
                                        int count,
                                        int lookaheadHoursPerCall,
                                        int coarseStepSeconds)
{
    QVector<PassResult> results;
    QDateTime cursor = fromUtc;

    while (results.size() < count) {
        const PassResult pass = findNextPass(propagator, cursor, observerLatDeg, observerLonDeg,
                                              observerAltMeters, lookaheadHoursPerCall, coarseStepSeconds);
        if (pass.state == PassState::NoPassInWindow) {
            break;
        }
        results.append(pass);

        const QDateTime end = pass.losUtc.isValid() ? pass.losUtc : pass.tcaUtc;
        cursor = end.addSecs(60);
    }

    return results;
}

QVector<PassResult> findPassesInWindow(const IOrbitPropagator &propagator,
                                        const QDateTime &fromUtc, const QDateTime &toUtc,
                                        double observerLatDeg, double observerLonDeg, double observerAltMeters,
                                        int coarseStepSeconds)
{
    QVector<PassResult> results;
    QDateTime cursor = fromUtc;

    while (cursor < toUtc) {
        // Shrinks each iteration as the window is consumed, so a satellite
        // with several passes in the window doesn't keep re-scanning far
        // past toUtc looking for one more.
        const int hoursLeft = int((cursor.secsTo(toUtc) + 3599) / 3600);
        const PassResult pass = findNextPass(propagator, cursor, observerLatDeg, observerLonDeg,
                                              observerAltMeters, qMax(1, hoursLeft), coarseStepSeconds);
        if (pass.state == PassState::NoPassInWindow || pass.aosUtc >= toUtc) {
            break;
        }
        results.append(pass);

        const QDateTime end = pass.losUtc.isValid() ? pass.losUtc : pass.tcaUtc;
        cursor = end.addSecs(60);
    }

    return results;
}

} // namespace SatelliteTracker
