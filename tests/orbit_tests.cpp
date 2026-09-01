// Plain assert-and-exit-code test executable (no test framework dependency).
// Run via `ctest` or directly as orbit_tests.exe.

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <QDateTime>
#include <QTimeZone>

#include "../src/core/Maidenhead.h"
#include "../src/core/Orbit/PassFinder.h"
#include "../src/core/Orbit/Sgp4OrbitPropagator.h"

using namespace SatelliteTracker;

namespace {

int g_failures = 0;

void check(bool condition, const char *description)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        ++g_failures;
    } else {
        std::printf("ok: %s\n", description);
    }
}

void checkNear(double actual, double expected, double tolerance, const char *description)
{
    check(std::fabs(actual - expected) <= tolerance, description);
}

void testMaidenheadFormulaBoundaries()
{
    // Deterministic formula-boundary checks (no external reference needed).
    Maidenhead::GeoCoordinate aa00;
    check(Maidenhead::locatorToLatLon(QStringLiteral("AA00"), &aa00), "AA00 is a valid locator");
    checkNear(aa00.latitudeDeg, -89.5, 1e-9, "AA00 center latitude is -89.5");
    checkNear(aa00.longitudeDeg, -179.0, 1e-9, "AA00 center longitude is -179.0");

    Maidenhead::GeoCoordinate rr99;
    check(Maidenhead::locatorToLatLon(QStringLiteral("RR99"), &rr99), "RR99 is a valid locator");
    checkNear(rr99.latitudeDeg, 89.5, 1e-9, "RR99 center latitude is 89.5");
    checkNear(rr99.longitudeDeg, 179.0, 1e-9, "RR99 center longitude is 179.0");
}

void testMaidenheadRealWorldSanity()
{
    // FN31 covers the Hartford/Newington, CT area (a commonly-cited ham
    // radio reference grid square) -- checked against the square's known
    // bounds rather than a memorized decimal to avoid a flaky test.
    Maidenhead::GeoCoordinate fn31;
    check(Maidenhead::locatorToLatLon(QStringLiteral("FN31"), &fn31), "FN31 is a valid locator");
    check(fn31.latitudeDeg > 40.0 && fn31.latitudeDeg < 43.0, "FN31 latitude is in the northeastern US band");
    check(fn31.longitudeDeg > -75.0 && fn31.longitudeDeg < -71.0, "FN31 longitude is in the northeastern US band");

    // 6-char subsquare should land inside the same 4-char square.
    Maidenhead::GeoCoordinate fn31pr;
    check(Maidenhead::locatorToLatLon(QStringLiteral("FN31pr"), &fn31pr), "FN31pr is a valid locator");
    check(fn31pr.latitudeDeg >= 41.0 && fn31pr.latitudeDeg < 42.0, "FN31pr latitude falls inside FN31");
    check(fn31pr.longitudeDeg >= -74.0 && fn31pr.longitudeDeg < -72.0, "FN31pr longitude falls inside FN31");
}

void testMaidenheadValidation()
{
    QString error;
    check(!Maidenhead::isValidLocator(QStringLiteral("AA0"), &error), "3-char locator is rejected");
    check(!Maidenhead::isValidLocator(QStringLiteral("SA12"), &error), "field letter outside A-R is rejected");
    check(!Maidenhead::isValidLocator(QStringLiteral("AA00yz"), &error), "subsquare letter outside A-X is rejected");
    check(Maidenhead::isValidLocator(QStringLiteral("kp20")), "lowercase locator is accepted");
}

void testPassFinderInvariants()
{
    // Verification test vector for satellite 00005, taken verbatim from the
    // vendored library's own SGP4-VER.TLE test data (third_party/sgp4).
    // Near-earth (non deep-space) orbit, epoch 2000-06-27.
    const QString line1 = QStringLiteral(
        "1 00005U 58002B   00179.78495062  .00000023  00000-0  28098-4 0  4753");
    const QString line2 = QStringLiteral(
        "2 00005  34.2682 348.7242 1859667 331.7664  19.3264 10.82419157413667");

    Sgp4OrbitPropagator propagator;
    QString loadError;
    check(propagator.loadTle(line1, line2, &loadError), "pinned TLE loads into the propagator");

    // A few hours after epoch, well within valid propagation range.
    const QDateTime fromUtc(QDate(2000, 6, 28), QTime(0, 0, 0), QTimeZone::UTC);
    // 34.3 deg inclination orbit guarantees passes over this observer,
    // within its latitude band, within 48h.
    const double observerLat = 30.0;
    const double observerLon = -75.0;
    const double observerAlt = 0.0;

    const PassResult pass = findNextPass(propagator, fromUtc, observerLat, observerLon, observerAlt,
                                          /*lookaheadHours=*/48, /*coarseStepSeconds=*/30);

    check(pass.state != PassState::NoPassInWindow, "a pass is found within 48h for a mid-latitude observer");
    if (pass.state == PassState::NoPassInWindow) {
        return; // remaining checks are meaningless without a pass
    }

    check(pass.aosUtc.isValid(), "AOS time is valid");
    check(pass.tcaUtc.isValid(), "TCA time is valid");

    if (pass.state == PassState::UpcomingPass) {
        check(pass.losUtc.isValid(), "LOS time is valid for an upcoming pass");
        check(pass.aosUtc < pass.tcaUtc, "AOS precedes TCA");
        check(pass.tcaUtc < pass.losUtc, "TCA precedes LOS");

        const double elevAtAos = propagator.computeLookAngle(pass.aosUtc, observerLat, observerLon, observerAlt).elevationDeg;
        const double elevAtLos = propagator.computeLookAngle(pass.losUtc, observerLat, observerLon, observerAlt).elevationDeg;
        checkNear(elevAtAos, 0.0, 1.0, "elevation is ~0 deg at AOS");
        checkNear(elevAtLos, 0.0, 1.0, "elevation is ~0 deg at LOS");
    }

    const double elevAtTca = propagator.computeLookAngle(pass.tcaUtc, observerLat, observerLon, observerAlt).elevationDeg;
    const double elevBeforeTca = propagator.computeLookAngle(pass.tcaUtc.addSecs(-60), observerLat, observerLon, observerAlt).elevationDeg;
    const double elevAfterTca = propagator.computeLookAngle(pass.tcaUtc.addSecs(60), observerLat, observerLon, observerAlt).elevationDeg;
    check(elevAtTca >= elevBeforeTca && elevAtTca >= elevAfterTca, "TCA is a local elevation maximum");
    checkNear(pass.maxElevationDeg, elevAtTca, 1e-6, "maxElevationDeg matches the elevation computed at tcaUtc");
}

void testCurrentlyInViewFindsTrueAos()
{
    // Regression test: a CurrentlyInView PassResult must report the true
    // rise instant (found by searching backward from "now"), not "now"
    // itself -- otherwise a chart built from it clips the already-elapsed
    // part of the pass.
    const QString line1 = QStringLiteral(
        "1 00005U 58002B   00179.78495062  .00000023  00000-0  28098-4 0  4753");
    const QString line2 = QStringLiteral(
        "2 00005  34.2682 348.7242 1859667 331.7664  19.3264 10.82419157413667");

    Sgp4OrbitPropagator propagator;
    check(propagator.loadTle(line1, line2), "pinned TLE loads for the CurrentlyInView test");

    const QDateTime fromUtc(QDate(2000, 6, 28), QTime(0, 0, 0), QTimeZone::UTC);
    const double observerLat = 30.0;
    const double observerLon = -75.0;
    const double observerAlt = 0.0;

    const PassResult upcoming = findNextPass(propagator, fromUtc, observerLat, observerLon, observerAlt,
                                              /*lookaheadHours=*/48, /*coarseStepSeconds=*/30);
    check(upcoming.state == PassState::UpcomingPass, "first pass from the pinned epoch is upcoming, not in-view");
    if (upcoming.state != PassState::UpcomingPass) return;

    // TCA is by definition inside the pass, so starting a fresh search
    // there should find the *same* pass as already in progress.
    const PassResult inView = findNextPass(propagator, upcoming.tcaUtc, observerLat, observerLon, observerAlt,
                                            /*lookaheadHours=*/48, /*coarseStepSeconds=*/30);
    check(inView.state == PassState::CurrentlyInView, "searching from TCA reports CurrentlyInView");

    check(qAbs(upcoming.aosUtc.msecsTo(inView.aosUtc)) < 2000,
          "CurrentlyInView's aosUtc matches the pass's true AOS, not the search instant");
    check(qAbs(upcoming.losUtc.msecsTo(inView.losUtc)) < 2000,
          "CurrentlyInView's losUtc still matches the same pass's true LOS");

    check(!inView.curve.isEmpty(), "CurrentlyInView still produces a curve");
    if (!inView.curve.isEmpty()) {
        check(qAbs(inView.aosUtc.msecsTo(inView.curve.first().utc)) < 2000,
              "CurrentlyInView's curve starts at the true AOS, not at the search instant (TCA)");
    }
}

} // namespace

int main()
{
    testMaidenheadFormulaBoundaries();
    testMaidenheadRealWorldSanity();
    testMaidenheadValidation();
    testPassFinderInvariants();
    testCurrentlyInViewFindsTrueAos();

    if (g_failures > 0) {
        std::fprintf(stderr, "\n%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("\nall checks passed\n");
    return 0;
}
