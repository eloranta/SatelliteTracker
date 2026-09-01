#pragma once

#include <memory>

#include <QFrame>

#include "../core/ObserverLocation.h"
#include "../core/Orbit/IOrbitPropagator.h"
#include "../core/Orbit/PassFinder.h"
#include "../data/Satellite.h"

class QLabel;
class QChartView;
class QChart;
class QLineSeries;
class QAreaSeries;
class QScatterSeries;
class QValueAxis;
class QDateTimeAxis;

namespace SatelliteTracker {

// One Tab 1 pass-grid card for one active satellite: an elevation-vs-time
// chart (updated ~every 30s when a fresh PassResult arrives) plus a live
// "now" marker and status chip (updated every 1s via tick()). Owns its own
// propagator so per-second updates don't need to go back through
// ActiveSatelliteTracker's off-thread recompute cycle.
//
// The area under the curve is red before AOS (not yet visible) and green
// from AOS onward; once LOS passes the card hides itself (still alive,
// still receiving updates -- just excluded from PassGridWidget's layout
// until its next pass arrives) and emits visibilityMaybeChanged() so the
// grid can reflow around it.
class PassCard : public QFrame {
    Q_OBJECT
public:
    explicit PassCard(const Satellite &satellite, QWidget *parent = nullptr);

    int noradId() const { return m_satellite.noradId; }

    // TLE/name may change after a catalog refresh; reloads the propagator
    // only if the TLE lines actually changed.
    void updateSatellite(const Satellite &satellite);
    void setObserverLocation(const ObserverLocation &location);
    void setPassResult(const PassResult &pass);
    void tick(const QDateTime &nowUtc);

signals:
    void visibilityMaybeChanged();

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void rebuildChartForPass();
    QString statusFor(const QDateTime &nowUtc, double elevationNowDeg, bool haveFix) const;

    Satellite m_satellite;
    ObserverLocation m_location;
    PassResult m_lastPass;
    std::unique_ptr<IOrbitPropagator> m_propagator;
    bool m_areaIsGreen = false;
    // LOS (or TCA, if LOS unknown) the time axis range was last set from;
    // used to tell "still the same pass, just refined" apart from "a new
    // pass" so the axis doesn't creep forward every ~30s recompute while a
    // satellite is CurrentlyInView (whose aosUtc is only "now", not the
    // true rise time -- see PassResult's aosUtc comment).
    QDateTime m_axisEndAnchor;

    QLabel *m_headerLabel = nullptr;
    QLabel *m_statusChip = nullptr;
    QLabel *m_summaryLabel = nullptr;

    QChartView *m_chartView = nullptr;
    QChart *m_chart = nullptr;
    QLineSeries *m_curveSeries = nullptr;
    QAreaSeries *m_areaSeries = nullptr;
    QScatterSeries *m_nowMarkerSeries = nullptr;
    QLineSeries *m_nowCursorSeries = nullptr; // vertical line at "now", shown only AOS..LOS
    QValueAxis *m_elevAxis = nullptr;
    QDateTimeAxis *m_timeAxis = nullptr;
};

} // namespace SatelliteTracker
