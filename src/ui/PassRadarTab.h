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
class QCategoryAxis;

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
// convention -- no rotation needed. Labeled ticks every 30 degrees, plus
// unlabeled minor ticks every 10 degrees (QValueAxis::setMinorTickCount,
// inherited since QCategoryAxis is a QValueAxis). A QCategoryAxis, not a
// plain QValueAxis, for the same reason as the radial axis below: labels
// come from literal category text, not QValueAxis::setLabelFormat()'s
// '%d'-style substitution, which mangles the degree sign (see radial axis
// comment) -- this keeps both axes' degree signs on the one code path that
// actually renders it correctly.
//
// Radial axis: elevation, wanted with 0 deg (horizon) at the outer rim and
// 90 deg (zenith) at the center, the usual "radar" convention. QAbstractAxis
// ::setReverse() does NOT achieve this -- verified by reading Qt Charts'
// own source (AbstractDomain::toPolarR in xypolardomain.cpp): the radial
// value-to-radius mapping always puts axis-min at the center and axis-max
// at the rim, with no reverse handling at all, so `reverse` is silently a
// no-op for a polar radial axis. Instead, m_radialAxis is a QCategoryAxis
// plotted over a "distance from zenith" domain (radius = 90 - elevationDeg),
// with one category per 15-degree ring (90/75/.../0 at the center/.../rim)
// -- each category boundary draws its own concentric gridline circle, which
// is how the 15-degree rings are drawn at all, not via minor ticks (a
// QCategoryAxis on a polar radial axis renders one circle per category,
// unlike QValueAxis's tick-based circles). The inversion is done in the
// data/labels rather than relying on an axis flag.
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
    QCategoryAxis *m_angularAxis = nullptr;
    QCategoryAxis *m_radialAxis = nullptr;
    QLineSeries *m_trackSeries = nullptr;
    QScatterSeries *m_nowMarkerSeries = nullptr;
};

} // namespace SatelliteTracker
