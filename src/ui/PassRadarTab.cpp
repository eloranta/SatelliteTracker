#include "PassRadarTab.h"

#include <cmath>
#include <utility>

#include <QCategoryAxis>
#include <QChartView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineSeries>
#include <QPen>
#include <QPolarChart>
#include <QScatterSeries>
#include <QVBoxLayout>
#include <QtMath>

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

const int kArrowCount = 3;
const double kArrowWingLength = 6.0;   // in radius units (0-90 domain)
const double kArrowWingAngleDeg = 25.0;

// (azimuthDeg, plotted radius) -> cartesian, consistent with Qt Charts' own
// polar convention (0 deg at top, increasing clockwise). Used only to
// compute the arrow chevrons' local tangent direction -- never touches the
// actual plotted axes/series.
QPointF toCartesian(const QPointF &polar)
{
    const double rad = qDegreesToRadians(polar.x());
    return QPointF(polar.y() * std::sin(rad), polar.y() * std::cos(rad));
}

QPointF toPolar(const QPointF &cartesian)
{
    double azimuthDeg = qRadiansToDegrees(std::atan2(cartesian.x(), cartesian.y()));
    if (azimuthDeg < 0.0)
        azimuthDeg += 360.0;
    return QPointF(azimuthDeg, std::hypot(cartesian.x(), cartesian.y()));
}

QPointF rotated(const QPointF &v, double rad)
{
    return QPointF(v.x() * std::cos(rad) - v.y() * std::sin(rad),
                   v.x() * std::sin(rad) + v.y() * std::cos(rad));
}
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

    m_angularAxis = new QCategoryAxis();
    // QValueAxis::setLabelFormat() round-trips the format string through
    // toLatin1() then QString::asprintf() (see Qt Charts'
    // ChartAxisElement::createValueLabels()/formatLabel()) -- U+00B0 alone
    // isn't valid UTF-8, so a '%d<degree>' format silently loses the degree
    // sign. QCategoryAxis labels are used as literal text with no such
    // round-trip (same as m_radialAxis below), so use it here too, even
    // though azimuth has no non-linear axis-orientation need of its own.
    m_angularAxis->setRange(0.0, 360.0);
    m_angularAxis->setLabelsPosition(QCategoryAxis::AxisLabelsPositionOnValue);
    m_angularAxis->setMinorTickCount(2);  // 2 unlabeled ticks between majors -> every 10 deg
    m_angularAxis->setStartValue(0.0);
    // Cardinal points (0/90/180/270) are labeled N/E/S/W instead of their
    // degree number -- the compass-bearing convention this axis already
    // follows (0 deg at top, clockwise), spelled out for the reader.
    m_angularAxis->append(QStringLiteral("N"), 0.0);
    m_angularAxis->append(QStringLiteral("30") + kDegreeSign, 30.0);
    m_angularAxis->append(QStringLiteral("60") + kDegreeSign, 60.0);
    m_angularAxis->append(QStringLiteral("E"), 90.0);
    m_angularAxis->append(QStringLiteral("120") + kDegreeSign, 120.0);
    m_angularAxis->append(QStringLiteral("150") + kDegreeSign, 150.0);
    m_angularAxis->append(QStringLiteral("S"), 180.0);
    m_angularAxis->append(QStringLiteral("210") + kDegreeSign, 210.0);
    m_angularAxis->append(QStringLiteral("240") + kDegreeSign, 240.0);
    m_angularAxis->append(QStringLiteral("W"), 270.0);
    m_angularAxis->append(QStringLiteral("300") + kDegreeSign, 300.0);
    m_angularAxis->append(QStringLiteral("330") + kDegreeSign, 330.0);
    // No separate 360 entry: QCategoryAxis::append() keys categories by
    // label text and silently drops a second "N" (it would otherwise be
    // identical to the 0-deg entry above -- 0 and 360 are the same point on
    // a full circle, so nothing is lost by not drawing a redundant spoke).
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

    m_arrowSeries.reserve(kArrowCount);
    for (int i = 0; i < kArrowCount; ++i) {
        auto *arrow = new QLineSeries();
        arrow->setPen(QPen(m_trackSeries->color(), 2));
        m_chart->addSeries(arrow);
        arrow->attachAxis(m_angularAxis);
        arrow->attachAxis(m_radialAxis);
        m_arrowSeries.append(arrow);
    }

    m_nowMarkerSeries = new QScatterSeries();
    m_nowMarkerSeries->setMarkerSize(12.0);
    m_chart->addSeries(m_nowMarkerSeries);
    m_nowMarkerSeries->attachAxis(m_angularAxis);
    m_nowMarkerSeries->attachAxis(m_radialAxis);

    m_chartView = new QChartView(m_chart, this);
    m_chartView->setRenderHint(QPainter::Antialiasing);

    m_azElLabel = new QLabel(this);
    QFont azElFont = m_azElLabel->font();
    azElFont.setPointSize(azElFont.pointSize() + 2);
    m_azElLabel->setFont(azElFont);
    m_azElLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_azElLabel->setTextFormat(Qt::PlainText);
    m_azElLabel->setText(QStringLiteral("Az —\nEl —"));

    auto *chartRow = new QHBoxLayout();
    chartRow->addWidget(m_azElLabel, 0);
    chartRow->addWidget(m_chartView, 1);
    layout->addLayout(chartRow, 1);

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
        m_azElLabel->setText(QStringLiteral("Az —\nEl —"));
        for (QLineSeries *arrow : std::as_const(m_arrowSeries))
            arrow->clear();
        return;
    }

    QList<QPointF> points;
    points.reserve(m_lastPass.curve.size());
    for (const ElevationPoint &p : m_lastPass.curve) {
        points.append(QPointF(p.azimuthDeg, elevationToRadius(p.elevationDeg)));
    }
    m_trackSeries->replace(points);
    rebuildArrows(points);
}

void PassRadarTab::rebuildArrows(const QList<QPointF> &trackPoints)
{
    const int n = trackPoints.size();
    if (n < 2) {
        for (QLineSeries *arrow : std::as_const(m_arrowSeries))
            arrow->clear();
        return;
    }

    for (int a = 0; a < m_arrowSeries.size(); ++a) {
        // Anchors evenly spaced along the track, in travel order.
        const double fraction = double(a + 1) / double(m_arrowSeries.size() + 1);
        const int i = qBound(0, int(fraction * (n - 1)), n - 2);

        const QPointF p0 = toCartesian(trackPoints[i]);
        const QPointF p1 = toCartesian(trackPoints[i + 1]);
        QPointF dir = p1 - p0;
        const double dirLen = std::hypot(dir.x(), dir.y());
        if (dirLen < 1e-9) {
            m_arrowSeries[a]->clear();
            continue;
        }
        dir /= dirLen;

        const QPointF backDir(-dir.x(), -dir.y());
        const double angleRad = qDegreesToRadians(kArrowWingAngleDeg);
        const QPointF leftWing = p0 + rotated(backDir, angleRad) * kArrowWingLength;
        const QPointF rightWing = p0 + rotated(backDir, -angleRad) * kArrowWingLength;

        m_arrowSeries[a]->replace({toPolar(leftWing), toPolar(p0), toPolar(rightWing)});
    }
}

void PassRadarTab::tick(const QDateTime &nowUtc)
{
    if (!m_propagator || !m_location.isConfigured
        || m_lastPass.state == PassState::NoPassInWindow
        || nowUtc < m_lastPass.aosUtc
        || (m_lastPass.losUtc.isValid() && nowUtc > m_lastPass.losUtc)) {
        m_nowMarkerSeries->clear();
        m_azElLabel->setText(QStringLiteral("Az —\nEl —"));
        return;
    }

    const LookAngle look = m_propagator->computeLookAngle(
        nowUtc, m_location.latitudeDeg, m_location.longitudeDeg, m_location.altitudeMeters);
    if (look.valid && look.elevationDeg > 0.0) {
        m_nowMarkerSeries->replace({QPointF(look.azimuthDeg, elevationToRadius(look.elevationDeg))});
        m_azElLabel->setText(
            QStringLiteral("Az %1").arg(QString::number(look.azimuthDeg, 'f', 1)) + kDegreeSign
            + QStringLiteral("\nEl %1").arg(QString::number(look.elevationDeg, 'f', 1)) + kDegreeSign);
    } else {
        m_nowMarkerSeries->clear();
        m_azElLabel->setText(QStringLiteral("Az —\nEl —"));
    }
}

} // namespace SatelliteTracker
