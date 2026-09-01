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
class QScatterSeries;
class QValueAxis;
class QDateTimeAxis;

namespace SatelliteTracker {

// One Tab 1 pass-grid card for one active satellite: an elevation-vs-time
// chart (updated ~every 30s when a fresh PassResult arrives) plus a live
// "now" marker and status chip (updated every 1s via tick()). Owns its own
// propagator so per-second updates don't need to go back through
// ActiveSatelliteTracker's off-thread recompute cycle.
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

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void rebuildChartForPass();
    QString statusFor(const QDateTime &nowUtc, double elevationNowDeg, bool haveFix) const;

    Satellite m_satellite;
    ObserverLocation m_location;
    PassResult m_lastPass;
    std::unique_ptr<IOrbitPropagator> m_propagator;

    QLabel *m_headerLabel = nullptr;
    QLabel *m_statusChip = nullptr;
    QLabel *m_summaryLabel = nullptr;

    QChartView *m_chartView = nullptr;
    QChart *m_chart = nullptr;
    QLineSeries *m_curveSeries = nullptr;
    QScatterSeries *m_nowMarkerSeries = nullptr;
    QValueAxis *m_elevAxis = nullptr;
    QDateTimeAxis *m_timeAxis = nullptr;
};

} // namespace SatelliteTracker
