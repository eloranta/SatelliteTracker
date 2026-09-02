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
    // The true rise instant, for both UpcomingPass and CurrentlyInView --
    // SGP4 propagates to past instants as validly as future ones, so
    // findNextPass() searches backward from fromUtc when already in view
    // rather than settling for "now" as a stand-in. Only falls back to
    // fromUtc itself if no rise is found within a bounded backward search
    // (e.g. a continuously-visible near-geostationary satellite).
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
