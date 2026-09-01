#pragma once

#include <QDateTime>
#include <QVector>

#include "IOrbitPropagator.h"

namespace SatelliteTracker {

enum class PassState { NoPassInWindow, CurrentlyInView, UpcomingPass };

struct ElevationPoint {
    QDateTime utc;
    double elevationDeg = 0.0;
};

struct PassResult {
    PassState state = PassState::NoPassInWindow;
    // aosUtc: for UpcomingPass, the true rise instant; for CurrentlyInView,
    // the start of the search window (the actual rise time is in the past
    // and unknown).
    QDateTime aosUtc;
    QDateTime tcaUtc;
    QDateTime losUtc;
    double maxElevationDeg = 0.0;
    double maxElevAzimuthDeg = 0.0;
    // Elevation-vs-time samples from aosUtc through losUtc (or the scanned
    // window's end, if losUtc wasn't found), for charting. Empty when state
    // == NoPassInWindow.
    QVector<ElevationPoint> curve;
};

// Finds the next pass (or the currently-in-progress one) for one satellite
// over a fixed observer, scanning forward from `fromUtc` across
// `lookaheadHours`. Coarse-samples elevation every `coarseStepSeconds` and
// bisects/refines around zero-crossings and the elevation peak.
PassResult findNextPass(const IOrbitPropagator &propagator,
                         const QDateTime &fromUtc,
                         double observerLatDeg, double observerLonDeg, double observerAltMeters,
                         int lookaheadHours = 24,
                         int coarseStepSeconds = 30);

// Finds up to `count` consecutive upcoming passes, each found via
// findNextPass starting just after the previous one's end. Stops early if a
// NoPassInWindow result is hit before `count` is reached.
QVector<PassResult> findUpcomingPasses(const IOrbitPropagator &propagator,
                                        const QDateTime &fromUtc,
                                        double observerLatDeg, double observerLonDeg, double observerAltMeters,
                                        int count = 5,
                                        int lookaheadHoursPerCall = 72,
                                        int coarseStepSeconds = 30);

} // namespace SatelliteTracker
