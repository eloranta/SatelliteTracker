#pragma once

#include <memory>

#include <QWidget>

#include "../core/ObserverLocation.h"
#include "../core/Orbit/IOrbitPropagator.h"
#include "../core/Orbit/PassFinder.h"
#include "../data/Satellite.h"

class QLabel;
class QChartView;
class QPolarChart;
class QLineSeries;
class QScatterSeries;
class QValueAxis;

namespace SatelliteTracker {

// A Tab-widget page for one satellite, opened (or focused) by double-
// clicking one of its PassCards: a polar azimuth/elevation "radar" plot of
// a specific pass, the kind of sky-path view used for aiming a directional
// antenna. Long-lived and re-updatable (setPassResult() may be called again
// later, e.g. if the user double-clicks a different upcoming pass for the
// same satellite) -- unlike PassCard, which is single-shot.
//
// Angular axis: azimuth, 0-360 degrees, 0 at the top increasing clockwise
// (Qt Charts' polar-chart default), which already matches compass bearing
// convention -- no rotation needed. Radial axis: elevation, 0-90 at its
// default (non-reversed) orientation, which for QPolarChart puts the
// minimum (0 deg, horizon) at the outer rim and the maximum (90 deg,
// zenith) at the center -- matching the usual "radar" convention.
class PassRadarTab : public QWidget {
    Q_OBJECT
public:
    explicit PassRadarTab(const Satellite &satellite, QWidget *parent = nullptr);

    int noradId() const { return m_satellite.noradId; }

    void setObserverLocation(const ObserverLocation &location);
    void setPassResult(const Satellite &satellite, const PassResult &pass);
    void tick(const QDateTime &nowUtc);

private:
    void rebuildPlot();

    Satellite m_satellite;
    ObserverLocation m_location;
    PassResult m_lastPass;
    std::unique_ptr<IOrbitPropagator> m_propagator;

    QLabel *m_headerLabel = nullptr;
    QPolarChart *m_chart = nullptr;
    QChartView *m_chartView = nullptr;
    QValueAxis *m_angularAxis = nullptr;
    QValueAxis *m_radialAxis = nullptr;
    QLineSeries *m_trackSeries = nullptr;
    QScatterSeries *m_nowMarkerSeries = nullptr;
};

} // namespace SatelliteTracker
