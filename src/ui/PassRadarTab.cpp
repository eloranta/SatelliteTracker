#include "PassRadarTab.h"

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
    m_angularAxis->setLabelFormat(QStringLiteral("%d°"));
    m_angularAxis->setTickCount(9); // 0,45,...,360
    m_chart->addAxis(m_angularAxis, QPolarChart::PolarOrientationAngular);

    m_radialAxis = new QValueAxis();
    m_radialAxis->setRange(0.0, 90.0);
    // Want 0 deg (horizon) at the outer rim and 90 deg (zenith) at the
    // center. setReverse(true) turned out to invert QPolarChart's radial
    // axis the opposite way from a normal Cartesian "reverse" -- leaving it
    // at its default (false) is what actually puts min-at-rim/max-at-center.
    m_radialAxis->setTitleText(QStringLiteral("Elevation (°)"));
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
        m_headerLabel->setText(
            QStringLiteral("%1 — AOS %2 · TCA %3 (%4° az %5°) · LOS %6")
                .arg(name, aosText, pass.tcaUtc.toLocalTime().toString(QStringLiteral("HH:mm:ss")),
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
        points.append(QPointF(p.azimuthDeg, p.elevationDeg));
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
        m_nowMarkerSeries->replace({QPointF(look.azimuthDeg, look.elevationDeg)});
    } else {
        m_nowMarkerSeries->clear();
    }
}

} // namespace SatelliteTracker
