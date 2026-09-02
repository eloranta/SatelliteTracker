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

// One Tab 1 pass-grid card for one specific pass of one satellite: an
// elevation-vs-time chart (set once via setPassResult()) plus a live "now"
// marker and status chip (updated every 1s via tick()). Owns its own
// propagator so per-second updates don't need to go back through
// PassGridWidget's off-thread recompute cycle. PassGridWidget rebuilds its
// whole card set each ~30s cycle rather than reusing cards, so a given
// PassCard instance only ever receives one setPassResult() call.
//
// The area under the curve is red before AOS (not yet risen) and green from
// AOS onward; once its own LOS passes the card hides itself (still alive,
// but excluded from PassGridWidget's layout -- the next 30s rebuild
// replaces it with fresh cards for the satellite's next passes) and emits
// visibilityMaybeChanged() so the grid can reflow around it.
class PassCard : public QFrame {
    Q_OBJECT
public:
    explicit PassCard(const Satellite &satellite, QWidget *parent = nullptr);

    int noradId() const { return m_satellite.noradId; }

    // For PassGridWidget's chronological ordering: the current pass's AOS
    // (or, if there's no upcoming pass yet, an arbitrarily-far-future value
    // so such cards sort last).
    QDateTime aosSortKey() const;

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
    void updateHeaderLabel();
    QString statusFor(const QDateTime &nowUtc, double elevationNowDeg, bool haveFix) const;

    Satellite m_satellite;
    ObserverLocation m_location;
    PassResult m_lastPass;
    std::unique_ptr<IOrbitPropagator> m_propagator;
    bool m_areaIsGreen = false;

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
