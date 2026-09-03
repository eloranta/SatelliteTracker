#include "PassRadarTab.h"

#include <QCategoryAxis>
#include <QChartView>
#include <QLabel>
#include <QLineSeries>
#include <QPolarChart>
#include <QScatterSeries>
#include <QValueAxis>
#include <QVBoxLayout>

#include "../core/Orbit/Sgp4OrbitPropagator.h"
#include "../core/SatelliteNaming.h"

namespace SatelliteTracker {

namespace {
// Radial plot value: "distance from zenith", so elevation=90 (zenith) plots
// at radius 0 (center) and elevation=0 (horizon) plots at radius 90 (rim).
// See PassRadarTab.h for why this is done in data space rather than via
// QAbstractAxis::setReverse().
double elevationToRadius(double elevationDeg)
{
    return 90.0 - elevationDeg;
}

// Built from its numeric code point rather than embedded as a literal '°'
// character, so its meaning can't depend on the compiler's assumed source
// encoding.
const QChar kDegreeSign(176);
} // namespace

PassRadarTab::PassRadarTab(const Satellite &satellite, QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);

    m_headerLabel = new QLabel(this);
    QFont headerFont = m_headerLabel->font();
    headerFont.setBold(true);
    m_headerLabel->setFont(headerFont);
    m_headerLabel->setAlignment(Qt::AlignCenter);
    m_headerLabel->setWordWrap(true);
    layout->addWidget(m_headerLabel);

    m_chart = new QPolarChart();
    m_chart->legend()->hide();

    m_angularAxis = new QValueAxis();
    m_angularAxis->setRange(0.0, 360.0);
    m_angularAxis->setLabelFormat(QStringLiteral("%d") + kDegreeSign);
    m_angularAxis->setTickCount(13);      // labeled ticks every 30 deg (0,30,...,360)
    m_angularAxis->setMinorTickCount(2);  // 2 unlabeled ticks between majors -> every 10 deg
    m_chart->addAxis(m_angularAxis, QPolarChart::PolarOrientationAngular);

    m_radialAxis = new QCategoryAxis();
    // append()/setStartValue() alone only define label boundaries -- the
    // underlying QValueAxis min/max the polar domain actually maps radius
    // from default to (0, 0), which would divide-by-zero every point.
    m_radialAxis->setRange(0.0, 90.0);
    m_radialAxis->setLabelsPosition(QCategoryAxis::AxisLabelsPositionOnValue);
    m_radialAxis->setStartValue(0.0);
    // One category per 15-degree elevation ring; each boundary draws its own
    // concentric gridline circle (QCategoryAxis on a polar radial axis draws
    // one circle per category, unlike QValueAxis's ticks/minor ticks).
    m_radialAxis->append(QStringLiteral("90") + kDegreeSign, 0.0);
    m_radialAxis->append(QStringLiteral("75") + kDegreeSign, 15.0);
    m_radialAxis->append(QStringLiteral("60") + kDegreeSign, 30.0);
    m_radialAxis->append(QStringLiteral("45") + kDegreeSign, 45.0);
    m_radialAxis->append(QStringLiteral("30") + kDegreeSign, 60.0);
    m_radialAxis->append(QStringLiteral("15") + kDegreeSign, 75.0);
    m_radialAxis->append(QStringLiteral("0") + kDegreeSign, 90.0);
    m_radialAxis->setTitleText(QStringLiteral("Elevation (") + kDegreeSign + QStringLiteral(")"));
    m_chart->addAxis(m_radialAxis, QPolarChart::PolarOrientationRadial);

    m_trackSeries = new QLineSeries();
    m_chart->addSeries(m_trackSeries);
    m_trackSeries->attachAxis(m_angularAxis);
    m_trackSeries->attachAxis(m_radialAxis);

    m_nowMarkerSeries = new QScatterSeries();
    m_nowMarkerSeries->setMarkerSize(12.0);
    m_chart->addSeries(m_nowMarkerSeries);
    m_nowMarkerSeries->attachAxis(m_angularAxis);
    m_nowMarkerSeries->attachAxis(m_radialAxis);

    m_chartView = new QChartView(m_chart, this);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    layout->addWidget(m_chartView, 1);

    setPassResult(satellite, PassResult());
}

void PassRadarTab::setObserverLocation(const ObserverLocation &location)
{
    m_location = location;
}

void PassRadarTab::setPassResult(const Satellite &satellite, const PassResult &pass)
{
    const bool tleChanged = satellite.tleLine1 != m_satellite.tleLine1
                          || satellite.tleLine2 != m_satellite.tleLine2;
    m_satellite = satellite;
    m_lastPass = pass;

    if (tleChanged || !m_propagator) {
        auto propagator = std::make_unique<Sgp4OrbitPropagator>();
        propagator->loadTle(satellite.tleLine1, satellite.tleLine2);
        m_propagator = std::move(propagator);
    }

    const QString name = SatelliteNaming::shortName(satellite.name);
    if (pass.state == PassState::NoPassInWindow) {
        m_headerLabel->setText(name + QStringLiteral(" — no pass in the next 24h"));
    } else {
        const QString aosText = pass.state == PassState::CurrentlyInView
            ? QStringLiteral("in view")
            : pass.aosUtc.toLocalTime().toString(QStringLiteral("HH:mm:ss"));
        const QString losText = pass.losUtc.isValid()
            ? pass.losUtc.toLocalTime().toString(QStringLiteral("HH:mm:ss"))
            : QStringLiteral("—");
        const QString headerTemplate = QStringLiteral("%1 — AOS %2 · TCA %3 (%4")
            + kDegreeSign + QStringLiteral(" az %5") + kDegreeSign + QStringLiteral(") · LOS %6");
        m_headerLabel->setText(
            headerTemplate.arg(name, aosText, pass.tcaUtc.toLocalTime().toString(QStringLiteral("HH:mm:ss")),
                     QString::number(pass.maxElevationDeg, 'f', 0),
                     QString::number(pass.maxElevAzimuthDeg, 'f', 0), losText));
    }

    rebuildPlot();
}

void PassRadarTab::rebuildPlot()
{
    if (m_lastPass.state == PassState::NoPassInWindow || m_lastPass.curve.isEmpty()) {
        m_trackSeries->clear();
        m_nowMarkerSeries->clear();
        return;
    }

    QList<QPointF> points;
    points.reserve(m_lastPass.curve.size());
    for (const ElevationPoint &p : m_lastPass.curve) {
        points.append(QPointF(p.azimuthDeg, elevationToRadius(p.elevationDeg)));
    }
    m_trackSeries->replace(points);
}

void PassRadarTab::tick(const QDateTime &nowUtc)
{
    if (!m_propagator || !m_location.isConfigured
        || m_lastPass.state == PassState::NoPassInWindow
        || nowUtc < m_lastPass.aosUtc
        || (m_lastPass.losUtc.isValid() && nowUtc > m_lastPass.losUtc)) {
        m_nowMarkerSeries->clear();
        return;
    }

    const LookAngle look = m_propagator->computeLookAngle(
        nowUtc, m_location.latitudeDeg, m_location.longitudeDeg, m_location.altitudeMeters);
    if (look.valid && look.elevationDeg > 0.0) {
        m_nowMarkerSeries->replace({QPointF(look.azimuthDeg, elevationToRadius(look.elevationDeg))});
    } else {
        m_nowMarkerSeries->clear();
    }
}

} // namespace SatelliteTracker
